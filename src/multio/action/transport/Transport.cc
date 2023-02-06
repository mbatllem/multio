/*
 * (C) Copyright 1996- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */

#include "Transport.h"

#include <algorithm>

#include "eckit/config/Resource.h"

#include "multio/transport/TransportRegistry.h"
#include "multio/util/ConfigurationPath.h"
#include "multio/util/ScopedTimer.h"
#include "multio/util/logfile_name.h"

namespace multio {
namespace action {

using message::Message;
using transport::TransportRegistry;

namespace {
size_t serverIdDenom(size_t clientCount, size_t serverCount) {
    return (serverCount == 0) ? 1 : (((clientCount - 1) / serverCount) + 1);
}

std::vector<std::string> getHashKeys(const eckit::Configuration& conf) {
    if (conf.has("hash-keys")) {
        return conf.getStringVector("hash-keys");
    }
    return std::vector<std::string>{"category", "name", "level"};
}
}  // namespace

Transport::Transport(const ConfigurationContext& confCtx) :
    Action{confCtx},
    transport_{TransportRegistry::instance().get(confCtx)},
    client_{transport_->localPeer()},
    serverPeers_{transport_->serverPeers()},
    serverCount_{serverPeers_.size()},
    serverId_{client_.id() / serverIdDenom(transport_->serverCount(), serverCount_)},
    usedServerCount_{eckit::Resource<size_t>("multioMpiPoolSize;$MULTIO_USED_SERVERS", 1)},
    hashKeys_{getHashKeys(confCtx.config())},
    counters_(serverPeers_.size()),
    distType_{distributionType()} {
}

void Transport::executeImpl(Message msg) {
    // eckit::Log::info() << "Execute transport action for message " << msg << std::endl;
    util::ScopedTiming timing{statistics_.localTimer_, statistics_.actionTiming_};

    auto md = msg.metadata();
    if (md.getBool("toAllServers")) {
        for (auto& server : serverPeers_) {
            auto md = msg.metadata();
            Message trMsg{Message::Header{msg.tag(), client_, *server, std::move(md)},
                          msg.payload()};

            transport_->send(trMsg);
        }
    }
    else {
        auto server = chooseServer(msg.metadata());

        Message trMsg{Message::Header{msg.tag(), client_, server, std::move(md)},
                      std::move(msg.payload())};

        transport_->bufferedSend(trMsg);
    }
}

void Transport::print(std::ostream& os) const {
    os << "Action[" << *transport_ << "]";
}

message::Peer Transport::chooseServer(const message::Metadata& metadata) {
    ASSERT_MSG(serverCount_ > 0, "No server to choose from");

    auto getMetadataValue = [&](const std::string& hashKey) {
        if (!metadata.has(hashKey)) {
            std::ostringstream os;
            os << "The hash key \"" << hashKey << "\" is not defined in the metadata object: " << metadata << std::endl;
            throw transport::TransportException(os.str(), Here());
        }
        return metadata.getString(hashKey);
    };

    auto constructHash = [&](){
        std::ostringstream os;

        for(const std::string& s: hashKeys_) {
            os << getMetadataValue(s);
        }
        return os.str();
    };

    switch (distType_) {
        case DistributionType::hashed_cyclic: {
            std::string hashString = constructHash();
            ASSERT(usedServerCount_ <= serverCount_);

            auto offset = std::hash<std::string>{}(hashString) % usedServerCount_;
            auto id = (serverId_ + offset) % serverCount_;

            ASSERT(id < serverPeers_.size());

            return *serverPeers_[id];
        }
        case DistributionType::hashed_to_single: {
            std::string hashString = constructHash();
            auto id = std::hash<std::string>{}(hashString) % serverCount_;

            ASSERT(id < serverPeers_.size());

            return *serverPeers_[id];
        }
        case DistributionType::even: {
            std::string hashString = constructHash();

            if (destinations_.find(hashString) != end(destinations_)) {
                return destinations_.at(hashString);
            }

            auto it = std::min_element(begin(counters_), end(counters_));
            auto id = static_cast<size_t>(std::distance(std::begin(counters_), it));

            ASSERT(id < serverPeers_.size());
            ASSERT(id < counters_.size());

            ++counters_[id];

            auto dest = *serverPeers_[id];
            destinations_[hashString] = *serverPeers_[id];

            return dest;
        }
        default:
            throw eckit::SeriousBug("Unhandled distribution type");
    }
}

Transport::DistributionType Transport::distributionType() {
    const std::map<std::string, enum DistributionType> str2dist = {
        {"hashed_cyclic", DistributionType::hashed_cyclic},
        {"hashed_to_single", DistributionType::hashed_to_single},
        {"even", DistributionType::even}};

    auto key = std::getenv("MULTIO_SERVER_DISTRIBUTION");
    return key ? str2dist.at(key) : DistributionType::hashed_to_single;
}

static ActionBuilder<Transport> TransportBuilder("transport");

}  // namespace action
}  // namespace multio
