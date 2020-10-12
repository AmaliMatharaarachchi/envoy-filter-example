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

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MGW {

/**
 * All stats for the mgw filter. @see stats_macros.h
 */

#define ALL_mgw_FILTER_STATS(COUNTER)                                                        \
  COUNTER(ok)                                                                                      \
  COUNTER(denied)                                                                                  \
  COUNTER(error)                                                                                   \
  COUNTER(failure_mode_allowed)

/**
 * Wrapper struct for mgw filter stats. @see stats_macros.h
 */
struct MGWFilterStats {
  ALL_mgw_FILTER_STATS(GENERATE_COUNTER_STRUCT)
};

/**
 * Configuration for the External mgw request filter.
 */
class FilterConfig {
public:
  FilterConfig(const envoy::extensions::filters::http::mgw::v3::Path& config,
               const LocalInfo::LocalInfo& local_info, Stats::Scope& scope,
               Runtime::Loader& runtime, Http::Context& http_context,
               const std::string& stats_prefix)
      : allow_partial_message_(config.with_request_body().allow_partial_message()),
        failure_mode_allow_(config.failure_mode_allow()),
        clear_route_cache_(config.clear_route_cache()),
        max_request_bytes_(config.with_request_body().max_request_bytes()),
        status_on_error_(toErrorCode(config.status_on_error().code())), local_info_(local_info),
        scope_(scope), runtime_(runtime), http_context_(http_context),
        filter_enabled_(config.has_filter_enabled()
                            ? absl::optional<Runtime::FractionalPercent>(
                                  Runtime::FractionalPercent(config.filter_enabled(), runtime_))
                            : absl::nullopt),
        pool_(scope_.symbolTable()),
        metadata_context_namespaces_(config.metadata_context_namespaces().begin(),
                                     config.metadata_context_namespaces().end()),
        include_peer_certificate_(config.include_peer_certificate()),
        stats_(generateStats(stats_prefix, scope)), mgw_ok_(pool_.add("mgw.ok")),
        mgw_denied_(pool_.add("mgw.denied")),
        mgw_error_(pool_.add("mgw.error")),
        mgw_failure_mode_allowed_(pool_.add("mgw.failure_mode_allowed")) {}

  bool allowPartialMessage() const { return allow_partial_message_; }

  bool withRequestBody() const { return max_request_bytes_ > 0; }

  bool failureModeAllow() const { return failure_mode_allow_; }

  bool clearRouteCache() const { return clear_route_cache_; }

  uint32_t maxRequestBytes() const { return max_request_bytes_; }

  const LocalInfo::LocalInfo& localInfo() const { return local_info_; }

  Http::Code statusOnError() const { return status_on_error_; }

  bool filterEnabled() { return filter_enabled_.has_value() ? filter_enabled_->enabled() : true; }

  Runtime::Loader& runtime() { return runtime_; }

  Stats::Scope& scope() { return scope_; }

  Http::Context& httpContext() { return http_context_; }

  const std::vector<std::string>& metadataContextNamespaces() {
    return metadata_context_namespaces_;
  }

  const MGWFilterStats& stats() const { return stats_; }

  void incCounter(Stats::Scope& scope, Stats::StatName name) {
    scope.counterFromStatName(name).inc();
  }

  bool includePeerCertificate() const { return include_peer_certificate_; }

private:
  static Http::Code toErrorCode(uint64_t status) {
    const auto code = static_cast<Http::Code>(status);
    if (code >= Http::Code::Continue && code <= Http::Code::NetworkAuthenticationRequired) {
      return code;
    }
    return Http::Code::Forbidden;
  }

  MGWFilterStats generateStats(const std::string& prefix, Stats::Scope& scope) {
    const std::string final_prefix = prefix + "mgw.";
    return {ALL_mgw_FILTER_STATS(POOL_COUNTER_PREFIX(scope, final_prefix))};
  }

  const bool allow_partial_message_;
  const bool failure_mode_allow_;
  const bool clear_route_cache_;
  const uint32_t max_request_bytes_;
  const Http::Code status_on_error_;
  const LocalInfo::LocalInfo& local_info_;
  Stats::Scope& scope_;
  Runtime::Loader& runtime_;
  Http::Context& http_context_;

  const absl::optional<Runtime::FractionalPercent> filter_enabled_;

  // TODO(nezdolik): stop using pool as part of deprecating cluster scope stats.
  Stats::StatNamePool pool_;

  const std::vector<std::string> metadata_context_namespaces_;

  const bool include_peer_certificate_;

  // The stats for the filter.
  MGWFilterStats stats_;

public:
  // TODO(nezdolik): deprecate cluster scope stats counters in favor of filter scope stats
  // (MGWFilterStats stats_).
  const Stats::StatName mgw_ok_;
  const Stats::StatName mgw_denied_;
  const Stats::StatName mgw_error_;
  const Stats::StatName mgw_failure_mode_allowed_;
};

} // namespace MGW
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy