// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Minimal, dependency-free HTTP/1.1 client for talking to the Foundry Local
// embedded web service on localhost. It is intentionally tiny: a single blocking
// POST that returns the full response. The Foundry Local web service is reached
// over loopback, so we don't need TLS, redirects, proxies, or keep-alive — we
// send `Connection: close` and read the body until the server closes the socket.
//
// Header-only so every sample can include it without extra build wiring.

#pragma once

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace sample::http {

/// Parsed components of an "http://host:port/path" URL.
struct Url {
  std::string host;
  std::string port = "80";
  std::string path = "/";
};

/// Parse "http://127.0.0.1:5273/v1" into {host, port, path}. Only the http scheme is supported.
inline Url ParseUrl(const std::string& url) {
  std::string rest = url;

  if (const auto scheme = rest.find("://"); scheme != std::string::npos) {
    rest = rest.substr(scheme + 3);
  }

  Url out;
  std::string host_port = rest;
  if (const auto slash = rest.find('/'); slash != std::string::npos) {
    host_port = rest.substr(0, slash);
    out.path = rest.substr(slash);
  }

  out.host = host_port;
  if (const auto colon = host_port.rfind(':'); colon != std::string::npos) {
    out.host = host_port.substr(0, colon);
    out.port = host_port.substr(colon + 1);
  }

  return out;
}

/// Result of an HTTP request: numeric status code and the raw response body.
struct Response {
  int status = 0;
  std::string body;
};

namespace detail {

#ifdef _WIN32
using socket_t = SOCKET;
constexpr socket_t kInvalidSocket = INVALID_SOCKET;

inline void CloseSocket(socket_t s) { ::closesocket(s); }

/// Initialize Winsock once per process via a function-local static.
inline void EnsureWinsock() {
  static const bool ok = [] {
    WSADATA data;
    return ::WSAStartup(MAKEWORD(2, 2), &data) == 0;
  }();

  if (!ok) {
    throw std::runtime_error("WSAStartup failed");
  }
}
#else
using socket_t = int;
constexpr socket_t kInvalidSocket = -1;

inline void CloseSocket(socket_t s) { ::close(s); }
inline void EnsureWinsock() {}
#endif

/// RAII wrapper so the socket is always closed, even on exceptions.
class Socket {
 public:
  explicit Socket(socket_t fd) : fd_(fd) {}
  ~Socket() {
    if (fd_ != kInvalidSocket) {
      CloseSocket(fd_);
    }
  }

  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;

  socket_t get() const noexcept { return fd_; }

 private:
  socket_t fd_;
};

/// Open a TCP connection to host:port, returning a connected socket.
inline socket_t Connect(const std::string& host, const std::string& port) {
  EnsureWinsock();

  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo* result = nullptr;
  if (::getaddrinfo(host.c_str(), port.c_str(), &hints, &result) != 0 || result == nullptr) {
    throw std::runtime_error("Failed to resolve " + host + ":" + port);
  }

  socket_t fd = kInvalidSocket;
  for (addrinfo* ai = result; ai != nullptr; ai = ai->ai_next) {
    fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd == kInvalidSocket) {
      continue;
    }

    if (::connect(fd, ai->ai_addr, static_cast<int>(ai->ai_addrlen)) == 0) {
      break;
    }

    CloseSocket(fd);
    fd = kInvalidSocket;
  }

  ::freeaddrinfo(result);

  if (fd == kInvalidSocket) {
    throw std::runtime_error("Failed to connect to " + host + ":" + port);
  }

  return fd;
}

/// Send the entire buffer, looping until every byte is written.
inline void SendAll(socket_t fd, const std::string& data) {
  size_t sent = 0;
  while (sent < data.size()) {
    const auto n = ::send(fd, data.data() + sent, static_cast<int>(data.size() - sent), 0);
    if (n <= 0) {
      throw std::runtime_error("Socket send failed");
    }

    sent += static_cast<size_t>(n);
  }
}

/// Read the full response until the peer closes the connection.
inline std::string ReadAll(socket_t fd) {
  std::string out;
  char buffer[8192];
  while (true) {
    const auto n = ::recv(fd, buffer, sizeof(buffer), 0);
    if (n < 0) {
      throw std::runtime_error("Socket recv failed");
    }

    if (n == 0) {
      break;  // peer closed
    }

    out.append(buffer, static_cast<size_t>(n));
  }

  return out;
}

/// Split a raw HTTP response into status code + body, decoding chunked bodies.
inline Response ParseHttpResponse(const std::string& raw) {
  Response resp;

  const auto header_end = raw.find("\r\n\r\n");
  if (header_end == std::string::npos) {
    throw std::runtime_error("Malformed HTTP response (no header terminator)");
  }

  const std::string headers = raw.substr(0, header_end);
  std::string body = raw.substr(header_end + 4);

  // Status line: "HTTP/1.1 200 OK"
  if (const auto sp = headers.find(' '); sp != std::string::npos) {
    resp.status = std::atoi(headers.c_str() + sp + 1);
  }

  // Decode Transfer-Encoding: chunked if present (the web service uses it for some responses).
  std::string lower_headers = headers;
  for (char& c : lower_headers) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }

  if (lower_headers.find("transfer-encoding: chunked") != std::string::npos) {
    std::string decoded;
    size_t pos = 0;
    while (pos < body.size()) {
      const auto line_end = body.find("\r\n", pos);
      if (line_end == std::string::npos) {
        break;
      }

      const size_t chunk_size = std::strtoul(body.substr(pos, line_end - pos).c_str(), nullptr, 16);
      if (chunk_size == 0) {
        break;
      }

      const size_t data_start = line_end + 2;
      if (data_start + chunk_size > body.size()) {
        break;
      }

      decoded.append(body, data_start, chunk_size);
      pos = data_start + chunk_size + 2;  // skip chunk data + trailing CRLF
    }

    body = std::move(decoded);
  }

  resp.body = std::move(body);
  return resp;
}

}  // namespace detail

/// POST a body to host:port/path and return the full response.
/// `extra_headers` entries are sent verbatim (e.g. {"Accept: application/json"}).
inline Response Post(const std::string& host, const std::string& port, const std::string& path,
                     const std::string& body, const std::string& content_type = "application/json",
                     const std::vector<std::string>& extra_headers = {}) {
  detail::Socket sock(detail::Connect(host, port));

  std::string request;
  request += "POST " + path + " HTTP/1.1\r\n";
  request += "Host: " + host + ":" + port + "\r\n";
  request += "Content-Type: " + content_type + "\r\n";
  request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
  request += "Connection: close\r\n";
  for (const auto& header : extra_headers) {
    request += header + "\r\n";
  }

  request += "\r\n";
  request += body;

  detail::SendAll(sock.get(), request);

  return detail::ParseHttpResponse(detail::ReadAll(sock.get()));
}

}  // namespace sample::http
