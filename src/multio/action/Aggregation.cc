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

#include "multio/LibMultio.h"
#include "multio/domain/Mappings.h"
#include "multio/util/ScopedTimer.h"

namespace multio {
namespace action {

using message::Peer;

Aggregation::Aggregation(const ConfigurationContext& confCtx) : ChainedAction(confCtx) {}

void Aggregation::executeImpl(Message msg) const {

    eckit::Log::info() << " ***** Aggregating message " << msg << std::endl;

    if ((msg.tag() == Message::Tag::Field) && handleField(msg)) {
        executeNext(createGlobalField(std::move(msg)));
    }

    if ((msg.tag() == Message::Tag::StepComplete) && handleFlush(msg)) {
        executeNext(std::move(msg));
    }
}

bool Aggregation::handleField(const Message& msg) const {
    util::ScopedTiming timing{statistics_.localTimer_, statistics_.actionTiming_};
    if (not msgMap_.contains(msg.fieldId())) {
        msgMap_.addNew(msg);
    }
    // TODO: Perhaps call collect indices here and store it for a later call on check consistnecy
    domain::Mappings::instance().get(msg.domain()).at(msg.source())->to_global(msg, msgMap_.at(msg.fieldId()));
    msgMap_.bookProcessedPart(msg.fieldId(), msg.source());
    return allPartsArrived(msg);
}

bool Aggregation::handleFlush(const Message& msg) const {
    // Initialise if need be
    util::ScopedTiming timing{statistics_.localTimer_, statistics_.actionTiming_};

    auto flushCount = [&msg, this]() {
        if (flushes_.find(msg.fieldId()) == end(flushes_)) {
            flushes_[msg.fieldId()] = 0;
        }
        return ++flushes_.at(msg.fieldId());
    }();

    const auto& domainMap = domain::Mappings::instance().get(msg.domain());

    return domainMap.isComplete() && flushCount == domainMap.size();
}

bool Aggregation::allPartsArrived(const Message& msg) const {
    LOG_DEBUG_LIB(LibMultio) << " *** Number of messages for field " << msg.fieldId() << " are "
                             << msgMap_.partsCount(msg.fieldId()) << std::endl;

    const auto& domainMap = domain::Mappings::instance().get(msg.domain());

    return domainMap.isComplete() && (msgMap_.partsCount(msg.fieldId()) == domainMap.size());
}

Message Aggregation::createGlobalField(const Message& msg) const {
    util::ScopedTiming timing{statistics_.localTimer_, statistics_.actionTiming_};

    eckit::Log::info() << "Creating global field " << std::endl;

    const auto& fid = msg.fieldId();

    // TODO: checking domain consistency is skipped for now...
    //domain::Mappings::instance().checkDomainConsistency(messages_.at(fid));

    auto msgOut = std::move(msgMap_.at(fid));

    msgMap_.reset(fid);

    return msgOut;
}

void Aggregation::print(std::ostream& os) const {
    os << "Aggregation(for " << msgMap_.size() << " fields = [";
    for (const auto& mp : msgMap_) {
        auto const& domainMap = domain::Mappings::instance().get(mp.second.domain());
        os << '\n'
           << "  --->  " << mp.first << " ---> Aggregated " << msgMap_.partsCount(mp.first) << " parts of a total of "
           << (domainMap.isComplete() ? domainMap.size() : 0);
    }
    os << "])";
}


static ActionBuilder<Aggregation> AggregationBuilder("aggregation");

}  // namespace action
}  // namespace multio
