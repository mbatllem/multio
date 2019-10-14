/*
 * (C) Copyright 1996- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */

#include "TcpTransport.h"

#include <algorithm>
#include <iostream>

#include "eckit/config/LocalConfiguration.h"
#include "eckit/log/Plural.h"
#include "eckit/maths/Functions.h"
#include "eckit/runtime/Main.h"
#include "eckit/serialisation/MemoryStream.h"

namespace multio {
namespace server {

namespace {
Message decodeMessage(eckit::Stream& stream) {
    unsigned t;
    stream >> t;

    std::string src_grp;
    stream >> src_grp;
    size_t src_id;
    stream >> src_id;

    std::string dest_grp;
    stream >> dest_grp;
    size_t dest_id;
    stream >> dest_id;

    Message msg{Message::Header{static_cast<Message::Tag>(t), TcpPeer{src_grp, src_id},
                                TcpPeer{dest_grp, dest_id}}};
    msg.decode(stream);

    return msg;
}
}  // namespace


struct Connection {
    eckit::Select& select_;
    eckit::TCPSocket socket_;

    Connection(eckit::Select& select, eckit::TCPSocket& socket) : select_{select}, socket_{socket} {
        select_.add(socket_);
    }

    ~Connection() {
        select_.remove(socket_);
        socket_.close();
    }

    bool ready() { return select_.set(socket_); }
};

TcpTransport::TcpTransport(const eckit::Configuration& config) :
    Transport(config),
    local_{"localhost", config.getUnsigned("local_port")} {
    auto serverConfigs = config.getSubConfigurations("servers");

    for (auto cfg : serverConfigs) {
        auto host = cfg.getString("host");
        auto ports = cfg.getUnsignedVector("ports");

        if (amIServer(host, ports)) {
            server_.reset(new eckit::TCPServer{static_cast<int>(local_.port())});
            select_.add(*server_);
        }
        else {
            // TODO: assert that (local_.host(), local_.port()) is in the list of clients
            for (const auto port : ports) {
                try {
                    eckit::TCPClient client;
                    outgoing_.emplace(TcpPeer{host, port},
                                      new eckit::TCPSocket{client.connect(host, port, 5, 10)});
                }
                catch (eckit::TooManyRetries& e) {
                    eckit::Log::error() << "Failed to establish connection to host: " << host
                                        << ", port: " << port << std::endl;
                }
            }
        }
    }
}


Message TcpTransport::nextMessage(eckit::TCPSocket& socket) const {
    size_t size;
    socket.read(&size, sizeof(size));

    eckit::Buffer buffer{size};
    socket.read(buffer, static_cast<long>(size));

    eckit::MemoryStream stream{buffer};

    return decodeMessage(stream);
}

Message TcpTransport::receive() {

    waitForEvent();

    for (auto it = begin(incoming_); it != end(incoming_); ++it) {
        if (not (*it)->ready()) {
            continue;
        }

        auto msg = nextMessage((*it)->socket_);

        if(msg.tag() == Message::Tag::Close) {
            incoming_.erase(it);
        } else {
            std::swap(*it, incoming_[incoming_.size() - 1]);
        }

        return msg;
    }

    throw eckit::SeriousBug("No message received");
}

void TcpTransport::send(const Message& msg) {
    const auto& socket = outgoing_.at(msg.destination());

    // Add 4K for header/footer etc. Should be plenty
    eckit::Buffer buffer{eckit::round(msg.size(), 8) + 4096};

    eckit::MemoryStream stream{buffer};

    msg.encode(stream);

    auto size = stream.bytesWritten();
    socket->write(&size, sizeof(size));

    socket->write(buffer, static_cast<int>(size));
}

Peer TcpTransport::localPeer() const {
    return local_;
}

void TcpTransport::print(std::ostream& os) const {
    os << "TcpTransport()";
}

bool TcpTransport::acceptConnection() {
    if (not select_.set(*server_)) {
        return false;
    }

    eckit::TCPSocket socket{server_->accept()};
    incoming_.emplace_back(new Connection{select_, socket});

    return true;
}

void TcpTransport::waitForEvent() {
    do {
        while (not select_.ready(5)) {
            eckit::Log::info() << "Waiting... There are "
                               << eckit::Plural(incoming_.size(), "connection") << " still active"
                               << std::endl;
        }
    } while (acceptConnection());
}

bool TcpTransport::amIServer(const std::string& host, std::vector<size_t> ports) {
    return ((host == "localhost") || (host == local_.host())) &&
           (find(begin(ports), end(ports), local_.port()) != end(ports));
}

static TransportBuilder<TcpTransport> TcpTransportBuilder("tcp");

}  // namespace server
}  // namespace multio
