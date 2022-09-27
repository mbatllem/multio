/*
 * (C) Copyright 1996- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */

#include "Aggregation.h"

#include <algorithm>

#include "eckit/config/Configuration.h"
#include "eckit/exception/Exceptions.h"

#include "multio/LibMultio.h"
#include "multio/domain/Mappings.h"
#include "multio/util/ScopedTimer.h"

namespace multio {
namespace action {

using message::Peer;

Aggregation::Aggregation(const ConfigurationContext& confCtx) : Action(confCtx) {}

void Aggregation::execute(Message msg) const {
    if ((msg.tag() == Message::Tag::Field) && handleField(msg)) {
        executeNext(createGlobalField(std::move(msg)));
    }

    if ((msg.tag() == Message::Tag::StepComplete) && handleFlush(msg)) {
        executeNext(std::move(msg));
    }
}

bool Aggregation::handleField(const Message& msg) const {
    util::ScopedTiming timing{statistics_.localTimer_, statistics_.actionTiming_};
    messages_[msg.fieldId()].push_back(msg);
    return allPartsArrived(msg);
}

bool Aggregation::handleFlush(const Message& msg) const {
    // Initialise if need be
    util::ScopedTiming timing{statistics_.localTimer_, statistics_.actionTiming_};
    if (flushes_.find(msg.fieldId()) == end(flushes_)) {
        flushes_[msg.fieldId()] = 0;
    }
    
    // To test whether all clients have flushed, get and count their contribution to the domain
    const auto& domain = domain::Mappings::instance().get(msg.domain()).at(msg.source());
    auto& acc_size = flushes_.at(msg.fieldId());
    acc_size += domain->local_size();
    ASSERT(domain->global_size() == msg.globalSize());
    ASSERT(acc_size <= msg.globalSize());
    return (acc_size == msg.globalSize());
}

bool Aggregation::allPartsArrived(const Message& msg) const {
    LOG_DEBUG_LIB(LibMultio) << " *** Number of messages for field " << msg.fieldId() << " are "
                             << messages_.at(msg.fieldId()).size() << std::endl;

    const auto& domainMap = domain::Mappings::instance().get(msg.domain());

    return domainMap.isComplete() && (messages_.at(msg.fieldId()).size() == domainMap.size());
}

Message Aggregation::createGlobalField(const Message& msg) const {
    util::ScopedTiming timing{statistics_.localTimer_, statistics_.actionTiming_};

    const auto& fid = msg.fieldId();

    auto levelCount = msg.metadata().getLong("levelCount", 1);

    auto md = msg.header().metadata();
    Message msgOut{Message::Header{msg.header().tag(), Peer{msg.source().group()},
                                   Peer{msg.destination()}, std::move(md)},
                   eckit::Buffer{msg.globalSize() * levelCount * sizeof(double)}};

    domain::Mappings::instance().checkDomainConsistency(messages_.at(fid));

    for (const auto& msg : messages_.at(fid)) {
        domain::Mappings::instance().get(msg.domain()).at(msg.source())->to_global(msg, msgOut);
    }

    messages_.erase(fid);

    return msgOut;
}

void Aggregation::print(std::ostream& os) const {
    os << "Aggregation(for " << messages_.size() << " fields = [";
    for (const auto& msg : messages_) {
        os << '\n' << "  --->  " << msg.first;
    }
    os << "])";
}


static ActionBuilder<Aggregation> AggregationBuilder("aggregation");

}  // namespace action
}  // namespace multio
