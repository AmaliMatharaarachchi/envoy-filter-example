#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "mgw-api/extensions/filters/http/mgw/v3/mgw.pb.h"
#include "envoy/http/filter.h"
#include "envoy/local_info/local_info.h"
#include "envoy/runtime/runtime.h"
#include "envoy/service/auth/v3/external_auth.pb.h"
#include "mgw-api/services/response/v3/mgw_res.pb.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"
#include "envoy/upstream/cluster_manager.h"

#include "common/common/assert.h"
#include "common/common/logger.h"
#include "common/common/matchers.h"
#include "common/http/codes.h"
#include "common/http/header_map_impl.h"
#include "common/runtime/runtime_protos.h"

#include "mgw-source/filters/common/mgw/mgw.h"
#include "mgw-source/filters/common/mgw/mgw_grpc_impl.h"
#include "mgw-source/filters/http/mgw/filter_config.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MGW {

using FilterConfigSharedPtr = std::shared_ptr<FilterConfig>;

/**
 * Per route settings for ExtAuth. Allows customizing the CheckRequest on a
 * virtualhost\route\weighted cluster level.
 */
class FilterConfigPerRoute : public Router::RouteSpecificFilterConfig {
public:
  using ContextExtensionsMap = Protobuf::Map<std::string, std::string>;

  FilterConfigPerRoute(
      const envoy::extensions::filters::http::mgw::v3::MGWPerRoute& config)
      : context_extensions_(config.has_check_settings()
                                ? config.check_settings().context_extensions()
                                : ContextExtensionsMap()),
        disabled_(config.disabled()) {}

  void merge(const FilterConfigPerRoute& other);

  /**
   * @return Context extensions to add to the CheckRequest.
   */
  const ContextExtensionsMap& contextExtensions() const { return context_extensions_; }
  // Allow moving the context extensions out of this object.
  ContextExtensionsMap&& takeContextExtensions() { return std::move(context_extensions_); }

  bool disabled() const { return disabled_; }

private:
  // We save the context extensions as a protobuf map instead of an std::map as this allows us to
  // move it to the CheckRequest, thus avoiding a copy that would incur by converting it.
  ContextExtensionsMap context_extensions_;
  bool disabled_;
};

/**
 * HTTP mgw filter. Depending on the route configuration, this filter calls the global
 * mgw service before allowing further filter iteration.
 */
class Filter : public Logger::Loggable<Logger::Id::filter>,
               public Http::StreamFilter,
               public Filters::Common::MGW::RequestCallbacks,
               public Filters::Common::MGW::ResponseCallbacks {
public:
  Filter(const FilterConfigSharedPtr& req_config, const FilterConfigSharedPtr& res_config,
         Filters::Common::MGW::ClientPtr&& client, Filters::Common::MGW::ResClientPtr&& res_client)
      : req_config_(req_config), client_(std::move(client)),
        req_stats_(req_config->stats()), res_config_(res_config),
                   res_client_(std::move(res_client)) {}

  // Http::StreamFilterBase
  void onDestroy() override;

  // Http::StreamDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers,
                                          bool end_stream) override;
  Http::FilterDataStatus decodeData(Buffer::Instance& data, bool end_stream) override;
  Http::FilterTrailersStatus decodeTrailers(Http::RequestTrailerMap& trailers) override;
  void setDecoderFilterCallbacks(Http::StreamDecoderFilterCallbacks& callbacks) override;

  // Http::StreamEncoderFilter
  Http::FilterHeadersStatus encode100ContinueHeaders(Http::ResponseHeaderMap&) override;
  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap&, bool) override;
  Http::FilterDataStatus encodeData(Buffer::Instance&, bool) override;
  Http::FilterTrailersStatus encodeTrailers(Http::ResponseTrailerMap&) override;
  Http::FilterMetadataStatus encodeMetadata(Http::MetadataMap&) override;
  void setEncoderFilterCallbacks(Http::StreamEncoderFilterCallbacks&) override;

  // MGW::RequestCallbacks
  void onComplete(Filters::Common::MGW::ResponsePtr&&) override;
  // MGW::ResponseCallbacks
  void onResponseComplete(Filters::Common::MGW::ResponsePtr&&) override;

private:
  void addResponseHeaders(Http::HeaderMap& header_map, const Http::HeaderVector& headers);
  // State of this filter's communication with the external decode/encode service.
  // The filter has either not started calling the external service, in the middle of calling
  // it or has completed.
  enum class State { NotStarted, Calling, Complete };
  bool buffer_data_{};
  bool skip_check_{false};

  ////// request path members
  void initiateCall(const Http::RequestHeaderMap& headers,
                    const Router::RouteConstSharedPtr& route);
  void continueDecoding();
  bool skipCheckForRoute(const Router::RouteConstSharedPtr& route) const;
  // FilterReturn is used to capture what the return code should be to the filter chain.
  // if this filter is either in the middle of calling the service or the result is denied then
  // the filter chain should stop. Otherwise the filter chain can continue to the next filter.
  enum class FilterReturn { ContinueDecoding, StopDecoding };
  FilterConfigSharedPtr req_config_;
  Filters::Common::MGW::ClientPtr client_;
  Http::StreamDecoderFilterCallbacks* callbacks_{};
  Http::RequestHeaderMap* request_headers_;
  State state_{State::NotStarted}; // state of request check service
  FilterReturn filter_return_{FilterReturn::ContinueDecoding};
  Upstream::ClusterInfoConstSharedPtr cluster_;
  // Used to identify if the callback to onComplete() is synchronous (on the stack) or asynchronous.
  bool initiating_call_{};
  envoy::service::auth::v3::CheckRequest check_request_{};
  bool isBufferFull() const;
  //TODO(amalimatharaarachchi) add stats to response path as well.
  // The stats for the filter.
  MGWFilterStats req_stats_;

  ////// response path members
  void initiateResponseInterceptCall(const Http::ResponseHeaderMap& headers,
                    const Router::RouteConstSharedPtr& route);
  void continueEncoding();
  bool skipCheckForResRoute(const Router::RouteConstSharedPtr& route) const;
  // FilterReturn is used to capture what the return code should be to the filter chain.
  // if this filter is either in the middle of calling the service or the result is denied then
  // the filter chain should stop. Otherwise the filter chain can continue to the next filter.
  enum class ResponseFilterReturn { ContinueEncoding, StopEncoding };
  FilterConfigSharedPtr res_config_;
  Filters::Common::MGW::ResClientPtr res_client_;
  Http::StreamEncoderFilterCallbacks* res_callbacks_{};
  Http::ResponseHeaderMap* response_headers_;
  State res_state_{State::NotStarted}; //state of response interceptor service
  ResponseFilterReturn response_filter_return_{ResponseFilterReturn::ContinueEncoding};
  //TODO(amalimatharaarachchi) upstream cluster is used for downstream
  Upstream::ClusterInfoConstSharedPtr downstream_cluster_;
  // Used to identify if the response callback to onComplete() is synchronous (on the stack) or asynchronous.
  bool initiating_responce_call_{};
  envoy::service::mgw_res::v3::CheckRequest res_intercept_request_{};
  bool isResBufferFull() const;
};

} // namespace MGW
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
