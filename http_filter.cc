#include <string>

#include "http_filter.h"
// #include "greeter_client.cc"

namespace Envoy {
namespace Http {

HttpSampleGRPCFilterConfig::HttpSampleGRPCFilterConfig(const helloworld::MyGRPC& proto_config)
    : key_(proto_config.key()), val_(proto_config.val()) {}

HttpSampleGRPCFilter::HttpSampleGRPCFilter(HttpSampleGRPCFilterConfigSharedPtr config)
    : config_(config) {}

HttpSampleGRPCFilter::~HttpSampleGRPCFilter() {}

void HttpSampleGRPCFilter::onDestroy() {}

const LowerCaseString HttpSampleGRPCFilter::headerKey() const {
  return LowerCaseString(config_->key());
}

const std::string HttpSampleGRPCFilter::headerValue() const {
  return config_->val();
}

//decoder path
FilterHeadersStatus HttpSampleGRPCFilter::decodeHeaders(RequestHeaderMap& headers, bool) {
  // add a header
  LowerCaseString incoming_header = LowerCaseString("incoming-header"); 
  ENVOY_LOG(info, "[woohoo] inside decodeHeaders");
  headers.addCopy(incoming_header, "welcome");
  auto* h = headers.get(LowerCaseString("my-header"));
  if (h != nullptr) {
    headerVal = h->value().getStringView();
  }
  return FilterHeadersStatus::Continue;
  }

FilterDataStatus HttpSampleGRPCFilter::decodeData(Buffer::Instance&, bool) {
  return FilterDataStatus::Continue;
}

FilterTrailersStatus HttpSampleGRPCFilter::decodeTrailers(RequestTrailerMap&) {
  return FilterTrailersStatus::Continue;
}

void HttpSampleGRPCFilter::setDecoderFilterCallbacks(StreamDecoderFilterCallbacks& callbacks) {
  decoder_callbacks_ = &callbacks;
}

//encoder path
FilterHeadersStatus HttpSampleGRPCFilter::encodeHeaders(ResponseHeaderMap& headers, bool) {
  // add a header
  LowerCaseString outgoing_header = LowerCaseString("outgoing-header");
  ENVOY_LOG(info, "[woohoo] inside encodeHeaders");
  headers.addCopy(outgoing_header, headerVal);
  return FilterHeadersStatus::Continue;
}

FilterHeadersStatus HttpSampleGRPCFilter::encode100ContinueHeaders(ResponseHeaderMap& ) {
  return FilterHeadersStatus::Continue;
}

FilterDataStatus HttpSampleGRPCFilter::encodeData(Buffer::Instance& data, bool ) {
  ENVOY_LOG(info, "[woohoo] print print");
  ENVOY_LOG(info, data.toString());
  return FilterDataStatus::Continue;
}
FilterTrailersStatus HttpSampleGRPCFilter::encodeTrailers(ResponseTrailerMap& ) {
  return FilterTrailersStatus::Continue;
}
FilterMetadataStatus HttpSampleGRPCFilter::encodeMetadata(MetadataMap& ) {
  return FilterMetadataStatus::Continue;
}
void HttpSampleGRPCFilter::setEncoderFilterCallbacks(StreamEncoderFilterCallbacks& callbacks) {
  encoder_callbacks_ = &callbacks;
}

} // namespace Http
} // namespace Envoy
