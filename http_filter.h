#pragma once

#include <string>

#include "envoy/server/filter_config.h"

#include "helloworld.grpc.pb.h"

namespace Envoy {
namespace Http {

class HttpSampleGRPCFilterConfig {
public:
  HttpSampleGRPCFilterConfig(const helloworld::MyGRPC& proto_config);

  const std::string& key() const { return key_; }
  const std::string& val() const { return val_; }

private:
  const std::string key_;
  const std::string val_;
};

typedef std::shared_ptr<HttpSampleGRPCFilterConfig> HttpSampleGRPCFilterConfigSharedPtr;

class HttpSampleGRPCFilter : public StreamFilter,
                                protected Logger::Loggable<Envoy::Logger::Id::main> {
public:
  HttpSampleGRPCFilter(HttpSampleGRPCFilterConfigSharedPtr);
  ~HttpSampleGRPCFilter();

  // Http::StreamFilterBase
  void onDestroy() override;

  // // Http::StreamEncoderFilter
  FilterHeadersStatus encode100ContinueHeaders(ResponseHeaderMap& ) override;
  FilterHeadersStatus encodeHeaders(ResponseHeaderMap& , bool) override;
  FilterDataStatus encodeData(Buffer::Instance& , bool) override;
  FilterTrailersStatus encodeTrailers(ResponseTrailerMap& ) override;
  FilterMetadataStatus encodeMetadata(MetadataMap& ) override;
  void setEncoderFilterCallbacks(StreamEncoderFilterCallbacks& ) override;

  // Http::StreamDecoderFilter
  FilterHeadersStatus decodeHeaders(RequestHeaderMap&, bool) override;
  FilterDataStatus decodeData(Buffer::Instance&, bool) override;
  FilterTrailersStatus decodeTrailers(RequestTrailerMap&) override;
  void setDecoderFilterCallbacks(StreamDecoderFilterCallbacks&) override;

private:
  const HttpSampleGRPCFilterConfigSharedPtr config_;
  StreamDecoderFilterCallbacks* decoder_callbacks_;
  StreamEncoderFilterCallbacks* encoder_callbacks_;

  const LowerCaseString headerKey() const;
  const std::string headerValue() const;
  absl::string_view headerVal;
};

} // namespace Http
} // namespace Envoy
