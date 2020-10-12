#include "mgw-source/filters/common/mgw/check_response_utils.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "envoy/config/core/v3/base.pb.h"
#include "mgw-api/services/response/v3/attribute_context.pb.h"
#include "mgw-api/services/response/v3/mgw_res.pb.h"
#include "envoy/ssl/connection.h"

#include "common/buffer/buffer_impl.h"
#include "common/common/assert.h"
#include "common/common/empty_string.h"
#include "common/common/enum_to_int.h"
#include "common/grpc/async_client_impl.h"
#include "common/http/codes.h"
#include "common/http/headers.h"
#include "common/http/utility.h"
#include "common/network/utility.h"
#include "common/protobuf/protobuf.h"

#include "absl/strings/str_cat.h"

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace MGW {

void CheckResponseUtils::setAttrContextPeer(envoy::service::mgw_res::v3::AttributeContext::Peer& peer,
                                           const Network::Connection& connection,
                                           const std::string& service, const bool local,
                                           bool include_certificate) {

  // Set the address
  auto addr = peer.mutable_address();
  if (local) {
    Envoy::Network::Utility::addressToProtobufAddress(*connection.localAddress(), *addr);
  } else {
    Envoy::Network::Utility::addressToProtobufAddress(*connection.remoteAddress(), *addr);
  }

  // Set the principal. Preferably the URI SAN, DNS SAN or Subject in that order from the peer's
  // cert. Include the X.509 certificate of the source peer, if configured to do so.
  auto ssl = connection.ssl();
  if (ssl != nullptr) {
    if (local) {
      const auto uri_sans = ssl->uriSanLocalCertificate();
      if (uri_sans.empty()) {
        const auto dns_sans = ssl->dnsSansLocalCertificate();
        if (dns_sans.empty()) {
          peer.set_principal(ssl->subjectLocalCertificate());
        } else {
          peer.set_principal(dns_sans[0]);
        }
      } else {
        peer.set_principal(uri_sans[0]);
      }
    } else {
      const auto uri_sans = ssl->uriSanPeerCertificate();
      if (uri_sans.empty()) {
        const auto dns_sans = ssl->dnsSansPeerCertificate();
        if (dns_sans.empty()) {
          peer.set_principal(ssl->subjectPeerCertificate());
        } else {
          peer.set_principal(dns_sans[0]);
        }
      } else {
        peer.set_principal(uri_sans[0]);
      }
      if (include_certificate) {
        peer.set_certificate(ssl->urlEncodedPemEncodedPeerCertificate());
      }
    }
  }

  if (!service.empty()) {
    peer.set_service(service);
  }
}

std::string CheckResponseUtils::getHeaderStr(const Envoy::Http::HeaderEntry* entry) {
  if (entry) {
    // TODO(jmarantz): plumb absl::string_view further here; there's no need
    // to allocate a temp string in the local uses.
    return std::string(entry->value().getStringView());
  }
  return EMPTY_STRING;
}

void CheckResponseUtils::setRequestTime(envoy::service::mgw_res::v3::AttributeContext::Request& req,
                                       const StreamInfo::StreamInfo& stream_info) {
  // Set the timestamp when the proxy receives the first byte of the request.
  req.mutable_time()->MergeFrom(Protobuf::util::TimeUtil::NanosecondsToTimestamp(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          stream_info.startTime().time_since_epoch())
          .count()));
}

void CheckResponseUtils::setHttpResponse(
    envoy::service::mgw_res::v3::AttributeContext::HttpRequest& httpreq, uint64_t stream_id,
    const StreamInfo::StreamInfo& stream_info, const Buffer::Instance* encoding_buffer,
    const Envoy::Http::ResponseHeaderMap& headers, uint64_t max_response_bytes) {
  httpreq.set_id(std::to_string(stream_id));
  httpreq.set_status(getHeaderStr(headers.Status()));
  httpreq.set_content_type(getHeaderStr(headers.ContentType()));
  httpreq.set_size(stream_info.bytesReceived());

  if (stream_info.protocol()) {
    httpreq.set_protocol(Envoy::Http::Utility::getProtocolString(stream_info.protocol().value()));
  }

  // Fill in the headers.
  auto mutable_headers = httpreq.mutable_headers();
  headers.iterate(
      [](const Envoy::Http::HeaderEntry& e, void* ctx) {
        // Skip any client EnvoyAuthPartialBody header, which could interfere with internal use.
        if (e.key().getStringView() != Http::Headers::get().EnvoyAuthPartialBody.get()) {
          auto* mutable_headers = static_cast<Envoy::Protobuf::Map<std::string, std::string>*>(ctx);
          (*mutable_headers)[std::string(e.key().getStringView())] =
              std::string(e.value().getStringView());
        }
        return Envoy::Http::HeaderMap::Iterate::Continue;
      },
      mutable_headers);

  // Set request body.
  if (max_response_bytes > 0 && encoding_buffer != nullptr) {
    const uint64_t length = std::min(encoding_buffer->length(), max_response_bytes);
    std::string data(length, 0);
    encoding_buffer->copyOut(0, length, &data[0]);
    httpreq.set_body(std::move(data));

    // Add in a header to detect when a partial body is used.
    (*mutable_headers)[Http::Headers::get().EnvoyAuthPartialBody.get()] =
        length != encoding_buffer->length() ? "true" : "false";
  }
}

void CheckResponseUtils::setAttrContextResponse(
    envoy::service::mgw_res::v3::AttributeContext::Request& req, const uint64_t stream_id,
    const StreamInfo::StreamInfo& stream_info, const Buffer::Instance* encoding_buffer,
    const Envoy::Http::ResponseHeaderMap& headers, uint64_t max_request_bytes) {
  setRequestTime(req, stream_info);
  setHttpResponse(*req.mutable_http(), stream_id, stream_info, encoding_buffer, headers,
                 max_request_bytes);
}

void CheckResponseUtils::createHttpCheck(
    const Envoy::Http::StreamEncoderFilterCallbacks* callbacks,
    const Envoy::Http::ResponseHeaderMap& headers,
    Protobuf::Map<std::string, std::string>&& context_extensions,
    envoy::config::core::v3::Metadata&& metadata_context,
    envoy::service::mgw_res::v3::CheckRequest& request, uint64_t max_response_bytes,
    bool include_peer_certificate) {

  auto attrs = request.mutable_attributes();
  // TODO(amalimatharaarachchi)
  // const std::string service = getHeaderStr(headers.EnvoyDownstreamServiceCluster());
  const std::string service = "";

  // *cb->connection(), callbacks->streamInfo() and callbacks->encodingBuffer() are not qualified as
  // const.
  auto* cb = const_cast<Envoy::Http::StreamEncoderFilterCallbacks*>(callbacks);
  setAttrContextPeer(*attrs->mutable_source(), *cb->connection(), service, false,
                     include_peer_certificate);
  setAttrContextPeer(*attrs->mutable_destination(), *cb->connection(), "", true,
                     include_peer_certificate);
  setAttrContextResponse(*attrs->mutable_request(), cb->streamId(), cb->streamInfo(),
                        cb->encodingBuffer(), headers, max_response_bytes);

  // Fill in the context extensions and metadata context.
  (*attrs->mutable_context_extensions()) = std::move(context_extensions);
  (*attrs->mutable_metadata_context()) = std::move(metadata_context);
}

void CheckResponseUtils::createTcpCheck(const Network::ReadFilterCallbacks* callbacks,
                                       envoy::service::mgw_res::v3::CheckRequest& request,
                                       bool include_peer_certificate) {

  auto attrs = request.mutable_attributes();

  auto* cb = const_cast<Network::ReadFilterCallbacks*>(callbacks);
  setAttrContextPeer(*attrs->mutable_source(), cb->connection(), "", false,
                     include_peer_certificate);
  setAttrContextPeer(*attrs->mutable_destination(), cb->connection(), "", true,
                     include_peer_certificate);
}

} // namespace MGW
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy
