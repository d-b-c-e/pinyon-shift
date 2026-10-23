#include "telemetry/telemetry_out.h"

#include <chrono>
#include <cstring>
#include <mutex>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <rex/cvar.h>
#include <rex/logging.h>

#include "pinyon_shift_diagnostics.h"

REXCVAR_DEFINE_BOOL(telemetry_udp, false, "Telemetry",
                    "Send the player car's telemetry over UDP in Forza's Data Out layout "
                    "(telemetry_udp_format), for SimHub, dashboards and motion software")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);
REXCVAR_DEFINE_STRING(telemetry_udp_format, "fm7", "Telemetry",
                      "Packet layout: fm7 (Forza Motorsport 7 Dash, 311 bytes) or horizon "
                      "(Forza Horizon 4/5, 324 bytes); pick the matching game in the tool")
    .allowed({"fm7", "horizon"})
    .lifecycle(rex::cvar::Lifecycle::kHotReload);
REXCVAR_DEFINE_BOOL(telemetry_udp_moving_only, false, "Telemetry",
                    "Send telemetry only while the car moves; standing still (and the menus) "
                    "sends race off, which silences shakers")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);
REXCVAR_DEFINE_STRING(telemetry_udp_target, "127.0.0.1:5300", "Telemetry",
                      "Where telemetry_udp sends its packets, as host:port")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

namespace pinyon_shift::telemetry {

namespace {

#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket kNoSocket = INVALID_SOCKET;
void CloseSocket(Socket socket) { closesocket(socket); }
#else
using Socket = int;
constexpr Socket kNoSocket = -1;
void CloseSocket(Socket socket) { close(socket); }
#endif

std::mutex g_mutex;
Socket g_socket = kNoSocket;
sockaddr_storage g_address{};
socklen_t g_address_size = 0;
std::string g_target;  // the target g_address was resolved from
bool g_failed = false;  // the target could not be used; retried when it changes
bool g_horizon = false;
bool g_sent = false;
bool g_stopped = true;
#ifdef _WIN32
bool g_winsock_started = false;
#endif

// Opens the socket for `target` (host:port), once per target.
bool Open(const std::string& target) {
  if (target == g_target) return !g_failed;
  g_target = target;
  g_failed = true;
  if (g_socket != kNoSocket) {
    CloseSocket(g_socket);
    g_socket = kNoSocket;
  }
#ifdef _WIN32
  if (!g_winsock_started) {
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return false;
    g_winsock_started = true;
  }
#endif
  const size_t colon = target.rfind(':');
  if (colon == std::string::npos || colon == 0 || colon + 1 == target.size()) {
    REXLOG_WARN("Telemetry: telemetry_udp_target must be host:port, not '{}'", target);
    return false;
  }
  std::string host = target.substr(0, colon);
  if (host.size() > 2 && host.front() == '[' && host.back() == ']') {
    host = host.substr(1, host.size() - 2);
  }
  const std::string port = target.substr(colon + 1);
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;
  addrinfo* found = nullptr;
  if (getaddrinfo(host.c_str(), port.c_str(), &hints, &found) != 0 || !found) {
    REXLOG_WARN("Telemetry: cannot resolve '{}'", target);
    return false;
  }
  g_socket = socket(found->ai_family, found->ai_socktype, found->ai_protocol);
  if (g_socket != kNoSocket) {
#ifdef _WIN32
    u_long nonblocking = 1;
    const bool ready = ioctlsocket(g_socket, FIONBIO, &nonblocking) == 0;
#else
    const int flags = fcntl(g_socket, F_GETFL, 0);
    const bool ready = flags >= 0 && fcntl(g_socket, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
    if (!ready) {
      CloseSocket(g_socket);
      g_socket = kNoSocket;
    }
  }
  if (g_socket != kNoSocket) {
    std::memcpy(&g_address, found->ai_addr, found->ai_addrlen);
    g_address_size = socklen_t(found->ai_addrlen);
  }
  freeaddrinfo(found);
  if (g_socket == kNoSocket) {
    REXLOG_WARN("Telemetry: cannot open a UDP socket");
    return false;
  }
  g_failed = false;
  REXLOG_INFO("Telemetry: sending Forza Data Out packets ({}) to {}",
              g_horizon ? "horizon" : "fm7", target);
  diagnostics::RecordEvent("telemetry.udp.open", {{"target", target}});
  return true;
}

uint32_t TimestampMs() {
  using namespace std::chrono;
  static const auto start = steady_clock::now();
  return uint32_t(duration_cast<milliseconds>(steady_clock::now() - start).count());
}

void WriteLocked(const CarSample& sample) {
  const uint32_t timestamp = TimestampMs();
  const auto dash = g_horizon ? std::array<uint8_t, kDashPacketSize>{} : EncodeDash(sample, timestamp);
  const auto wide = g_horizon ? EncodeHorizon(sample, timestamp)
                              : std::array<uint8_t, kHorizonPacketSize>{};
  const char* data = reinterpret_cast<const char*>(g_horizon ? wide.data() : dash.data());
  const int size = int(g_horizon ? wide.size() : dash.size());
  // Nonblocking and best effort: errors/would-block drop this frame, never retry.
  if (sendto(g_socket, data, size, 0, reinterpret_cast<const sockaddr*>(&g_address),
             g_address_size) == size) {
    g_sent = true;
  }
}

void CloseLocked() {
  if (g_socket != kNoSocket) {
    if (g_sent) WriteLocked({});
    CloseSocket(g_socket);
    g_socket = kNoSocket;
  }
  g_sent = false;
  g_target.clear();
  g_failed = false;
}

}  // namespace

bool Wanted() { return REXCVAR_GET(telemetry_udp); }
bool MovingOnly() { return REXCVAR_GET(telemetry_udp_moving_only); }

void Configure() {
  const bool enabled = Wanted();
  const auto target = REXCVAR_GET(telemetry_udp_target);
  const bool horizon = REXCVAR_GET(telemetry_udp_format) == "horizon";
  std::lock_guard lock(g_mutex);
  if (g_stopped) return;
  if (!enabled || target != g_target || horizon != g_horizon) CloseLocked();
  g_horizon = horizon;
  // shortcut: DNS can stall host configuration; use async resolution if this becomes a UI issue.
  // Guest Send only tries the lock and drops frames while configuration owns it.
  if (enabled) Open(target);
}

void Initialize() {
  {
    std::lock_guard lock(g_mutex);
    g_stopped = false;
  }
  Configure();
}

void Shutdown() {
  std::lock_guard lock(g_mutex);
  g_stopped = true;
  CloseLocked();
#ifdef _WIN32
  if (g_winsock_started) {
    WSACleanup();
    g_winsock_started = false;
  }
#endif
}

void Send(const CarSample& sample) {
  if (!REXCVAR_GET(telemetry_udp)) return;
  std::unique_lock lock(g_mutex, std::try_to_lock);
  if (!lock || g_stopped || g_socket == kNoSocket) return;
  WriteLocked(sample);
}

}  // namespace pinyon_shift::telemetry
