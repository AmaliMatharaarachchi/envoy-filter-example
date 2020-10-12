#include "mgw-source/filters/http/mgw/config.h"

#include <chrono>
#include <string>

#include "envoy/config/core/v3/grpc_service.pb.h"
#include "mgw-api/extensions/filters/http/mgw/v3//mgw.pb.h"
#include "mgw-api/extensions/filters/http/mgw/v3//mgw.pb.validate.h"
#include "envoy/registry/registry.h"

#include "common/protobuf/utility.h"

#include "mgw-source/filters/common/mgw/mgw_grpc_impl.h"
#include "mgw-source/filters/common/mgw/mgw_res_grpc_impl.h"
#include "mgw-source/filters/http/mgw/mgw.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MGW {

Http::FilterFactoryCb MGWFilterConfig::createFilterFactoryFromProtoTyped(
    const envoy::extensions::filters::http::mgw::v3::MGW& proto_config,
    const std::string& stats_prefix, Server::Configuration::FactoryContext& context) {
  const auto req_filter_config =
      std::make_shared<FilterConfig>(proto_config.request(), context.localInfo(), context.scope(),
                                     context.runtime(), context.httpContext(), stats_prefix);
  const auto res_filter_config =
      std::make_shared<FilterConfig>(proto_config.request(), context.localInfo(), context.scope(),
                                     context.runtime(), context.httpContext(), stats_prefix);
  Http::FilterFactoryCb callback;

  // TODO(amalimatharaarachchi) add http service for the response intercept call if only needed.
  // if (proto_config.has_http_service()) {
  //   // Raw HTTP client.
  //   const uint32_t timeout_ms =
  //   PROTOBUF_GET_MS_OR_DEFAULT(proto_config.http_service().server_uri(),
  //                                                          timeout, DefaultTimeout);
  //   const auto client_config =
  //       std::make_shared<Extensions::Filters::Common::MGW::ClientConfig>(
  //           proto_config, timeout_ms, proto_config.http_service().path_prefix());
  //   callback = [filter_config, client_config,
  //               &context](Http::FilterChainFactoryCallbacks& callbacks) {
  //     auto client = std::make_unique<Extensions::Filters::Common::MGW::RawHttpClientImpl>(
  //         context.clusterManager(), client_config, context.timeSource());
  //     callbacks.addStreamFilter(Http::StreamFilterSharedPtr{
  //         std::make_shared<Filter>(filter_config, std::move(client))});
  //   };
  // } else {
  // gRPC client.
  const uint32_t req_timeout_ms =
      PROTOBUF_GET_MS_OR_DEFAULT(proto_config.request().grpc_service(), timeout, DefaultTimeout);
  const uint32_t res_timeout_ms =
      PROTOBUF_GET_MS_OR_DEFAULT(proto_config.response().grpc_service(), timeout, DefaultTimeout);
  callback = [req_grpc_service = proto_config.request().grpc_service(),
              res_grpc_service = proto_config.response().grpc_service(), &context,
              req_filter_config, res_filter_config, req_timeout_ms,
              res_timeout_ms](Http::FilterChainFactoryCallbacks& callbacks) {
    const auto req_async_client_factory =
        context.clusterManager().grpcAsyncClientManager().factoryForGrpcService(
            req_grpc_service, context.scope(), true);
    const auto res_async_client_factory =
        context.clusterManager().grpcAsyncClientManager().factoryForGrpcService(
            res_grpc_service, context.scope(), true);
    auto req_client = std::make_unique<Filters::Common::MGW::GrpcClientImpl>(
        req_async_client_factory->create(), std::chrono::milliseconds(req_timeout_ms));
    auto res_client = std::make_unique<Filters::Common::MGW::GrpcResClientImpl>(
        res_async_client_factory->create(), std::chrono::milliseconds(res_timeout_ms));
    callbacks.addStreamFilter(Http::StreamFilterSharedPtr{std::make_shared<Filter>(
        req_filter_config, res_filter_config, std::move(req_client), std::move(res_client))});
  };

  return callback;
};

Router::RouteSpecificFilterConfigConstSharedPtr
MGWFilterConfig::createRouteSpecificFilterConfigTyped(
    const envoy::extensions::filters::http::mgw::v3::MGWPerRoute& proto_config,
    Server::Configuration::ServerFactoryContext&, ProtobufMessage::ValidationVisitor&) {
  return std::make_shared<FilterConfigPerRoute>(proto_config);
}

/**
 * Static registration for the mgw filter. @see RegisterFactory.
 */
REGISTER_FACTORY(MGWFilterConfig, Server::Configuration::NamedHttpFilterConfigFactory){"envoy.mgw"};

} // namespace MGW
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
