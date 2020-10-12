#pragma once

#include "mgw-api/extensions/filters/http/mgw/v3/mgw.pb.h"
#include "mgw-api/extensions/filters/http/mgw/v3/mgw.pb.validate.h"

#include "extensions/filters/http/common/factory_base.h"
#include "extensions/filters/http/well_known_names.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MGW {

/**
 * Config registration for the external authorization filter. @see NamedHttpFilterConfigFactory.
 */
class MGWFilterConfig
    : public Common::FactoryBase<envoy::extensions::filters::http::mgw::v3::MGW,
                                 envoy::extensions::filters::http::mgw::v3::MGWPerRoute> {
public:
  MGWFilterConfig() : FactoryBase("envoy.filters.http.mgw") {}

private:
  static constexpr uint64_t DefaultTimeout = 200;
  Http::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const envoy::extensions::filters::http::mgw::v3::MGW& proto_config,
      const std::string& stats_prefix, Server::Configuration::FactoryContext& context) override;

  // filter support for route specific
  Router::RouteSpecificFilterConfigConstSharedPtr createRouteSpecificFilterConfigTyped(
      const envoy::extensions::filters::http::mgw::v3::MGWPerRoute& proto_config,
      Server::Configuration::ServerFactoryContext& context,
      ProtobufMessage::ValidationVisitor& validator) override;
};

} // namespace MGW
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
