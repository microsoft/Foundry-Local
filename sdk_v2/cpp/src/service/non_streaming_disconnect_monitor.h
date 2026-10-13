// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#ifdef FOUNDRY_LOCAL_HAS_WEB_SERVICE

#include "inferencing/session/request.h"

#include <oatpp/network/tcp/Connection.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <mstcpip.h>
#else
#include <cerrno>
#include <poll.h>
#include <sys/socket.h>
#endif

namespace fl {

// No response is written until non-streaming inference finishes, so oatpp cannot notice a closed peer on its own.
class NonStreamingDisconnectMonitor {
 public:
  NonStreamingDisconnectMonitor(std::shared_ptr<oatpp::data::stream::IOStream> connection, Request& request) {
    auto tcp = std::dynamic_pointer_cast<oatpp::network::tcp::Connection>(std::move(connection));
    if (!tcp) {
      throw std::invalid_argument("Non-streaming disconnect monitoring requires a TCP connection");
    }

    thread_ = std::jthread([this, tcp = std::move(tcp), &request](std::stop_token stop) {
      while (!stop.stop_requested()) {
        if (PeerDisconnected(tcp->getHandle())) {
          if (!stop.stop_requested()) {
            request.CancelCurrentOrNext();
            disconnected_.store(true);
          }
          return;
        }

        std::unique_lock lock(mutex_);
        cv_.wait_for(lock, std::chrono::milliseconds(10), [&] { return stop.stop_requested(); });
      }
    });
  }

  NonStreamingDisconnectMonitor(const NonStreamingDisconnectMonitor&) = delete;
  NonStreamingDisconnectMonitor& operator=(const NonStreamingDisconnectMonitor&) = delete;

  ~NonStreamingDisconnectMonitor() { Stop(); }

  bool Stop() {
    if (thread_.joinable()) {
      thread_.request_stop();
      cv_.notify_one();
      thread_.join();
    }

    return disconnected_.load();
  }

 private:
  static bool PeerDisconnected(oatpp::v_io_handle handle) {
#ifdef _WIN32
    const auto socket = static_cast<SOCKET>(handle);
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(socket, &readable);
    timeval timeout{};
    const int ready = ::select(0, &readable, nullptr, nullptr, &timeout);
    if (ready == SOCKET_ERROR) {
      return ::WSAGetLastError() != WSAEINTR;
    }
    if (ready == 0) {
      return false;
    }

    char byte;
    const int received = ::recv(socket, &byte, 1, MSG_PEEK);
    if (received == 0) {
      return true;
    }
    if (received == SOCKET_ERROR) {
      return ::WSAGetLastError() != WSAEWOULDBLOCK && ::WSAGetLastError() != WSAEINTR;
    }

    // An unread pipelined byte masks EOF from recv(MSG_PEEK); TCP state still reports the peer's FIN.
    DWORD version = 0;
    DWORD bytes_returned = 0;
    TCP_INFO_v0 info{};
    if (::WSAIoctl(socket, SIO_TCP_INFO, &version, sizeof(version), &info, sizeof(info),
                   &bytes_returned, nullptr, nullptr) == SOCKET_ERROR) {
      return ::WSAGetLastError() != WSAEINTR;
    }
    return info.State != TCPSTATE_ESTABLISHED;
#else
    pollfd descriptor{static_cast<int>(handle), POLLIN, 0};
#ifdef POLLRDHUP
    descriptor.events |= POLLRDHUP;
#endif
    const int ready = ::poll(&descriptor, 1, 0);
    if (ready < 0) {
      return errno != EINTR;
    }
    if (ready == 0) {
      return false;
    }

    if (descriptor.revents & (POLLHUP | POLLERR | POLLNVAL
#ifdef POLLRDHUP
                              | POLLRDHUP
#endif
                              )) {
      return true;
    }
    if (!(descriptor.revents & POLLIN)) {
      return false;
    }

    char byte;
    const int received = ::recv(descriptor.fd, &byte, 1, MSG_PEEK | MSG_DONTWAIT);
    return received == 0 || (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR);
#endif
  }

  std::atomic_bool disconnected_{false};
  std::mutex mutex_;
  std::condition_variable cv_;
  std::jthread thread_;
};

}  // namespace fl

#endif  // FOUNDRY_LOCAL_HAS_WEB_SERVICE
