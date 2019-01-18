
#include "Sink.h"

#include <iostream>

#include "eckit/parser/JSONDataBlob.h"

#include "atlas/array.h"
#include "atlas/field/Field.h"
#include "atlas/util/Metadata.h"

#include "multio/server/Message.h"
#include "multio/server/SerialisationHelpers.h"

namespace multio {
namespace server {

Sink::Sink(DataSink* ds, const std::string& nm) : Action{nm}, dataSink_{ds} {}
Sink::Sink(std::unique_ptr<DataSink>&& ds, const std::string& nm) :
    Action{nm},
    dataSink_{std::move(ds)} {}

bool Sink::doExecute(std::shared_ptr<Message> msg) const {

    configure(fetch_metadata(*msg));

    eckit::DataBlobPtr blob(eckit::DataBlobFactory::build("test", msg->data(), msg->size()));

    dataSink_->write(blob);

    return true;
}

void Sink::configure(const atlas::util::Metadata& metadata) const {
    eckit::LocalConfiguration config;
    config.set("path", eckit::PathName{metadata.get<std::string>("name") +
                                       "::" + std::to_string(metadata.get<int>("levels")) +
                                       "::" + std::to_string(metadata.get<int>("steps"))});
    dataSink_.reset(DataSinkFactory::instance().build("file", config));
}

}  // namespace server
}  // namespace multio
