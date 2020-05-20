#include <string>

#include "envoy/registry/registry.h"
// #include "envoy/source/common/protobuf/utility.h"

#include "http_filter.pb.h"
#include "http_filter.pb.validate.h"
#include "http_filter.h"

// #include "envoy/source/extensions/filters/http/common/factory_base.h"
// extensions/filters/http/common/factory_base.h"
// #include "envoy/extensions/filters/http/well_known_names.h"

namespace Envoy {
namespace Server {
namespace Configuration {

class HttpSampleGRPCFilterConfig : public NamedHttpFilterConfigFactory {
public:
  Http::FilterFactoryCb createFilterFactoryFromProto(const Protobuf::Message& proto_config,
                                                     const std::string&,
                                                     FactoryContext& context) override {
    return createFilter(Envoy::MessageUtil::downcastAndValidate<const sample::MyGRPC&>(
                            proto_config, context.messageValidationVisitor()), context);
  }

  /**
   *  Return the Protobuf Message that represents your config incase you have config proto
   */
  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return ProtobufTypes::MessagePtr{new sample::MyGRPC()};
  }

  std::string name() const override { return "sample"; }

private:
  Http::FilterFactoryCb createFilter(const sample::MyGRPC& proto_config, FactoryContext&) {
    Http::HttpSampleGRPCFilterConfigSharedPtr config =
        std::make_shared<Http::HttpSampleGRPCFilterConfig>(
            Http::HttpSampleGRPCFilterConfig(proto_config));

    return [config](Http::FilterChainFactoryCallbacks& callbacks) -> void {
      auto filter = new Http::HttpSampleGRPCFilter(config);
      //decoder path
      // callbacks.addStreamDecoderFilter(Http::StreamDecoderFilterSharedPtr{filter});
      //encoder path
      // callbacks.addStreamEncoderFilter(Http::StreamEncoderFilterSharedPtr{filter});
      callbacks.addStreamFilter(Http::StreamFilterSharedPtr{filter});
    };
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
