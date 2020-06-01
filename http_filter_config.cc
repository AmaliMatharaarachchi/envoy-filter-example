#include <string>

#include "envoy/registry/registry.h"

#include "helloworld.grpc.pb.h"
#include "http_filter.h"
// #include "greeter_client.cc"
#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include "helloworld.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using helloworld::Greeter;
using helloworld::HelloReply;
using helloworld::HelloRequest;

class GreeterClient;
namespace Envoy {
namespace Server {
namespace Configuration {

class GreeterClient {
public:
  GreeterClient(std::shared_ptr<Channel> channel) : stub_(Greeter::NewStub(channel)) {}

  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  std::string SayHello(const std::string& user) {
    // Data we are sending to the server.
    HelloRequest request;
    request.set_name(user);

    // Container for the data we expect from the server.
    HelloReply reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->SayHello(&context, request, &reply);

    // Act upon its status.
    if (status.ok()) {
      return reply.message();
    } else {
      std::cout << status.error_code() << ": " << status.error_message() << std::endl;
      return "RPC failed";
    }
  }

  std::string SayHelloAgain(const std::string& user) {
    // Follows the same pattern as SayHello.
    HelloRequest request;
    request.set_name(user);
    HelloReply reply;
    ClientContext context;

    // Here we can use the stub's newly available method we just added.
    Status status = stub_->SayHelloAgain(&context, request, &reply);
    if (status.ok()) {
      return reply.message();
    } else {
      std::cout << status.error_code() << ": " << status.error_message() << std::endl;
      return "RPC failed";
    }
  }

private:
  std::unique_ptr<Greeter::Stub> stub_;
};

class HttpSampleGRPCFilterConfig : public NamedHttpFilterConfigFactory,
                                   protected Logger::Loggable<Envoy::Logger::Id::main> {
public:
  Http::FilterFactoryCb createFilterFactoryFromProto(const Protobuf::Message& proto_config,
                                                     const std::string&,
                                                     FactoryContext& context) override {
    const auto& typed_config = dynamic_cast<const helloworld::MyGRPC&>(proto_config);
    return createFilter(typed_config, context);
  }

  /**
   *  Return the Protobuf Message that represents your config incase you have config proto
   */
  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return ProtobufTypes::MessagePtr{new helloworld::MyGRPC()};
  }

  std::string name() const override { return "sample"; }

private:
  Http::FilterFactoryCb createFilter(const helloworld::MyGRPC& proto_config, FactoryContext&) {

    Http::HttpSampleGRPCFilterConfigSharedPtr config =
        std::make_shared<Http::HttpSampleGRPCFilterConfig>(
            Http::HttpSampleGRPCFilterConfig(proto_config));
    ENVOY_LOG(info, "[woohoo] inside create filter");
    listener(config->key(), config->val());
    return [config](Http::FilterChainFactoryCallbacks& callbacks) -> void {
      auto filter = new Http::HttpSampleGRPCFilter(config);
      callbacks.addStreamFilter(Http::StreamFilterSharedPtr{filter});
    };
  }

  bool listener(std::string address, std::string port) {
    std::string server_address = address + ":" + port;
    std::cout << "Client querying server address: " << server_address << std::endl;

    // Instantiate the client. It requires a channel, out of which the actual RPCs
    // are created. This channel models a connection to an endpoint (in this case,
    // localhost at port 50051). We indicate that the channel isn't authenticated
    // (use of InsecureChannelCredentials()).
    GreeterClient greeter(grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()));
    std::string user("world");

    std::string reply = greeter.SayHello(user);
    std::cout << "Greeter received: " << reply << std::endl;

    reply = greeter.SayHelloAgain(user);
    std::cout << "Greeter received: " << reply << std::endl;
    return true;
  }
};

/**
 * Static registration for this sample filter. @see RegisterFactory.
 */
static Registry::RegisterFactory<HttpSampleGRPCFilterConfig, NamedHttpFilterConfigFactory>
    register_;



} // namespace Configuration
} // namespace Server
} // namespace Envoy



