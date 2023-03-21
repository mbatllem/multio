#include <fstream>
#include <unistd.h>
#include <limits>
#include <iomanip>
#include <cstring>
#include <iostream>
#include <string.h>

#include "eckit/filesystem/TmpFile.h"
#include "eckit/testing/Test.h"

#include "multio/api/multio_c.h"
#include "multio/api/multio_c_cpp_utils.h"
#include "multio/message/Metadata.h"

#include "eckit/exception/Exceptions.h"
#include "eckit/io/FileHandle.h"
#include "eckit/log/Log.h"
#include "eckit/mpi/Comm.h"
#include "eckit/option/CmdArgs.h"
#include "eckit/option/SimpleOption.h"

#include "multio/api/multio_c_cpp_utils.h"
#include "multio/tools/MultioTool.h"
#include "multio/util/ConfigurationPath.h"

//#include "TestDataContent.h"
//#include "testHelpers.h"

using multio::util::configuration_file_name;
using multio::util::configuration_path_name;


class MultioReplayAdamCApi final : public multio::MultioTool {
public:
    MultioReplayAdamCApi(int argc, char** argv);

private:
    void usage(const std::string& tool) const override {
        eckit::Log::info() << std::endl << "Usage: " << tool << " [options]" << std::endl;
    }

    void init(const eckit::option::CmdArgs& args) override;

    void finish(const eckit::option::CmdArgs& args) override;

    void execute(const eckit::option::CmdArgs& args) override;

    
    std::string transportType_ = "";
    std::string pathtodata_="";

    size_t clientCount_ = 1;
    std::string replayField_ = "";
    int step_ = 1;
    

    bool singlePrecision_;
};

MultioReplayAdamCApi::MultioReplayAdamCApi(int argc, char** argv) :
    multio::MultioTool(argc, argv), singlePrecision_(false) {
    options_.push_back(new eckit::option::SimpleOption<std::string>("transport", "Type of transport layer"));
    options_.push_back(new eckit::option::SimpleOption<std::string>("path", "Path to data"));
    options_.push_back(new eckit::option::SimpleOption<long>("nbclients", "Number of clients"));
    options_.push_back(new eckit::option::SimpleOption<long>("field", "Name of field to replay"));
    options_.push_back(new eckit::option::SimpleOption<long>("step", "Time counter for the field to replay"));

    std::cout << "Tool Created" << std::endl;
    std::cout << "Command line arguments:" << std::endl;
    for(int i = 0; i < argc; i++){
        std::cout << argv[i] << std::endl;
    }
    std::cout << std::endl;
    return;
}

void MultioReplayAdamCApi::init(const eckit::option::CmdArgs& args) {
    std::cout << "INIT" << std::endl;
    args.get("transport", transportType_);
    args.get("path", pathtodata_);
    args.get("step", step_);

    std::cout << "Transport: " << transportType_ << std::endl;
    std::cout << "Path to data: " << pathtodata_ << std::endl;
    std::cout << "Step: " << step_ << std::endl;
}

void MultioReplayAdamCApi::finish(const eckit::option::CmdArgs&) {
    std::cout << "FINISH" << std::endl;
}

void MultioReplayAdamCApi::execute(const eckit::option::CmdArgs&) {
    std::cout << "EXECUTE" << std::endl;
}

namespace multio {
namespace test{

CASE("Initial Test for version") {
    const char *version = nullptr;
    int rc;
    rc = multio_version(&version);
    if (rc != MULTIO_SUCCESS){
        std::cout << "Error getting Version" << std::endl;
    }
    else{
        std::cout << "Version " << version << std::endl;
    }
    EXPECT(rc==MULTIO_SUCCESS);
}

CASE("Test loading config") {
    multio_handle_t* multio_handle = nullptr;
    multio_configurationcontext_t* multio_cc = nullptr;
    int rc;

    rc = multio_new_configurationcontext(&multio_cc);
    const char *conf_path = "/Users/maaw/multio/tests/multio/";
    std::cout << conf_path << std::endl;

    rc = multio_conf_set_path(multio_cc, conf_path);
    if(rc==MULTIO_SUCCESS){
        std::cout << "Config Path set" << std::endl;
    }
    else{
        std::cout << "Config Path NOT set" << std::endl;
    }

    auto configPath = configuration_path_name();
    auto configFile = configuration_file_name();
    std::cout << "Configuration Path: " << configPath.asString().c_str() << std::endl;
    std::cout << "Configuration File: " << configFile.asString().c_str() << std::endl;
    rc = multio_new_configurationcontext_from_filename(&multio_cc, configFile.asString().c_str());
    if(rc==MULTIO_SUCCESS){
        std::cout << "Config created from Filename" << std::endl;
    }
    else{
        std::cout << "Config NOT created from filename" << std::endl;
    }
    multio_new_handle(&multio_handle, multio_cc);

    rc = multio_start_server(multio_cc);
    if(rc==MULTIO_SUCCESS){
        std::cout << "Server started" << std::endl;
    }
    else{
        std::cout << "Server NOT started" << std::endl;
    }

    rc = multio_delete_configurationcontext(multio_cc);
    if(rc==MULTIO_SUCCESS){
        std::cout << "Config deleted" << std::endl;
    }
    else{
        std::cout << "Config NOT deleted" << std::endl;
    }
    
    EXPECT(rc==MULTIO_SUCCESS);
}

CASE("Test Multio Initialisation") {
    int rc;
    rc = multio_initialise();
    if (rc != MULTIO_SUCCESS) {
        std::cout << "Multio NOT Initialised" << std::endl;
    }
    else{ 
        std::cout << "Multio Initialised" << std::endl;
    }
    EXPECT(rc==MULTIO_SUCCESS);
}

CASE("Test creating metadata"){
    int rc;
    multio_metadata_t* md = nullptr;
    rc = multio_new_metadata(&md);
    
    const char* key = "test";
    int value=1;

    multio_metadata_set_int(md, key, value);

    long long_value = 1;
    
    multio_metadata_set_long(md, key, long_value);
    
    long long ll_value = 1;

    multio_metadata_set_longlong(md, key, ll_value);

    const char * s_value = "test_val";

    multio_metadata_set_string(md, key, s_value);

    multio_handle_t* multio_handle = nullptr;
    multio_configurationcontext_t* multio_cc = nullptr;
 
    //multio_new_handle(&multio_handle, multio_cc);    
    
    rc = multio_delete_metadata(md);
    EXPECT(rc==MULTIO_SUCCESS);

}

CASE("Read from grib file"){
    const char* path = "/Users/maaw/multio/tests/multio/test.grib";
    auto field = eckit::PathName{path};

    std::cout << field << std::endl;

    eckit::FileHandle infile{field.fullName()};
    size_t bytes = infile.openForRead();

    eckit::Buffer buffer(bytes);
    infile.read(buffer.data(), bytes);

    infile.close();
   
    auto sz = static_cast<int>(buffer.size()) / sizeof(double);
    std::cout << "Size of Buffer: " << sz << std::endl;
    
    EXPECT(1==1);

}

CASE("Write to field"){

    multio_handle_t* multio_handle = nullptr;
    multio_configurationcontext_t* multio_cc = nullptr;
    int rc;

    rc = multio_new_configurationcontext(&multio_cc);
    const char *conf_path = "/Users/maaw/multio/tests/multio/";
    std::cout << conf_path << std::endl;

    rc = multio_conf_set_path(multio_cc, conf_path);
    if(rc==MULTIO_SUCCESS){
        std::cout << "Config Path set" << std::endl;
    }
    else{
        std::cout << "Config Path NOT set" << std::endl;
    }
    multio_new_handle(&multio_handle, multio_cc);
    EXPECT(rc==MULTIO_SUCCESS);

    const char* path = "/Users/maaw/multio/tests/multio/test.grib";
    auto field = eckit::PathName{path};

    std::cout << field << std::endl;

    eckit::FileHandle infile{field.fullName()};
    size_t bytes = infile.openForRead();

    eckit::Buffer buffer(bytes);
    infile.read(buffer.data(), bytes);

    infile.close();
   
    auto sz = static_cast<int>(buffer.size()) / sizeof(double);
    std::cout << "Size of Buffer: " << sz << std::endl;

    multio_metadata_t* md = nullptr;
    multio_new_metadata(&md);

    multio_metadata_set_string(md, "category", "test_data");
    multio_metadata_set_int(md, "globalSize", sz);
    multio_metadata_set_int(md, "level", 1);
    multio_metadata_set_int(md, "step", 1);

    multio_metadata_set_double(md, "missingValue", 0.0);
    multio_metadata_set_bool(md, "bitmapPresent", false);
    multio_metadata_set_int(md, "bitsPerValue", 16);

    multio_metadata_set_bool(md, "toAllServers", false);

    // Overwrite these fields in the existing metadata object
    multio_metadata_set_string(md, "name", "test");

    rc = multio_write_field(multio_handle, md, reinterpret_cast<const double*>(buffer.data()), sz);
    
    EXPECT(rc==MULTIO_SUCCESS);

}


}
}

int main(int argc, char** argv){
    std::cout << "Start Test!" << std::endl;
    MultioReplayAdamCApi tool(argc, argv);
    tool.start();
    return eckit::testing::run_tests(argc, argv);
}
