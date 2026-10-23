#include "telemetry/telemetry_out.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

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

struct Endpoint {
  Socket socket = kNoSocket;
  sockaddr_storage address{};
  socklen_t address_size = 0;
};

// The slow part of opening a target (host lookup, then the nonblocking socket) runs on one resolver thread per sender
// lifetime, so Configure (UI thread) and Shutdown (close) never wait for DNS. The thread shares only this state: it
// resolves the latest wanted generation, keeps at most one finished endpoint for Send to adopt, and closes whatever a
// newer generation, a disable or Shutdown made stale. Shutdown stops it without joining; a lookup still in progress
// then only frees what it made.
struct Resolver {
  std::mutex mutex;
  std::condition_variable wake;
  uint64_t wanted = 0;           // generation the sender wants; its target ("" = nothing)
  std::string target;
  std::atomic<uint64_t> done{0};  // generation `result` and `error` belong to
  Endpoint result;               // owned here until Send adopts it
  const char* error = nullptr;   // why `done` could not be opened, or null
  bool stopped = false;
};

std::mutex g_mutex;
Socket g_socket = kNoSocket;
sockaddr_storage g_address{};
socklen_t g_address_size = 0;
std::string g_target;  // the target requested for g_generation
bool g_horizon = false;
bool g_sent = false;
bool g_stopped = true;
uint64_t g_generation = 0;  // bumped by every request and every close
uint64_t g_adopted = 0;     // the last generation whose result Send took
std::shared_ptr<Resolver> g_resolver;

// Resolver thread: host lookup and socket setup with no lock held. Returns null or why `target` cannot be used.
const char* OpenEndpoint(const std::string& target, Endpoint& out) {
  const size_t colon = target.rfind(':');
  if (colon == std::string::npos || colon == 0 || colon + 1 == target.size()) {
    return "telemetry_udp_target must be host:port, not";
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
  if (getaddrinfo(host.c_str(), port.c_str(), &hints, &found) != 0 || !found) return "cannot resolve";
  out.socket = socket(found->ai_family, found->ai_socktype, found->ai_protocol);
  if (out.socket != kNoSocket) {
#ifdef _WIN32
    u_long nonblocking = 1;
    const bool ready = ioctlsocket(out.socket, FIONBIO, &nonblocking) == 0;
#else
    const int flags = fcntl(out.socket, F_GETFL, 0);
    const bool ready = flags >= 0 && fcntl(out.socket, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
    if (!ready) {
      CloseSocket(out.socket);
      out.socket = kNoSocket;
    }
  }
  if (out.socket != kNoSocket) {
    std::memcpy(&out.address, found->ai_addr, found->ai_addrlen);
    out.address_size = socklen_t(found->ai_addrlen);
  }
  freeaddrinfo(found);
  return out.socket == kNoSocket ? "cannot open a UDP socket for" : nullptr;
}

void ResolveLoop(std::shared_ptr<Resolver> r) {
#ifdef _WIN32
  WSADATA data;
  const bool winsock = WSAStartup(MAKEWORD(2, 2), &data) == 0;  // this thread's reference; its sockets need it
#endif
  std::unique_lock lock(r->mutex);
  uint64_t handled = 0;
  for (;;) {
    r->wake.wait(lock, [&] { return r->stopped || r->wanted != handled; });
    if (r->stopped) break;
    if (r->result.socket != kNoSocket && r->done != r->wanted) {  // finished but superseded before Send took it
      CloseSocket(r->result.socket);
      r->result.socket = kNoSocket;
    }
    const uint64_t generation = handled = r->wanted;
    const std::string target = r->target;
    if (target.empty()) continue;
    lock.unlock();
    Endpoint endpoint;
#ifdef _WIN32
    const char* error = winsock ? OpenEndpoint(target, endpoint) : "cannot start Winsock for";
#else
    const char* error = OpenEndpoint(target, endpoint);
#endif
    lock.lock();
    if (r->stopped || generation != r->wanted) {
      if (endpoint.socket != kNoSocket) CloseSocket(endpoint.socket);
      continue;
    }
    if (r->result.socket != kNoSocket) CloseSocket(r->result.socket);
    r->result = endpoint;
    r->error = error;
    r->done = generation;
  }
  if (r->result.socket != kNoSocket) {
    CloseSocket(r->result.socket);
    r->result.socket = kNoSocket;
  }
  lock.unlock();
#ifdef _WIN32
  if (winsock) WSACleanup();
#endif
}

// Under g_mutex: the resolver works on `target` next ("" cancels). Nothing waits for it.
void RequestLocked(const std::string& target) {
  g_target = target;
  const uint64_t generation = ++g_generation;
  if (!g_resolver) {
    if (target.empty()) return;
    g_resolver = std::make_shared<Resolver>();
    std::thread(ResolveLoop, g_resolver).detach();
  }
  {
    std::lock_guard lock(g_resolver->mutex);
    g_resolver->wanted = generation;
    g_resolver->target = target;
  }
  g_resolver->wake.notify_one();
}

// Under g_mutex, on the Send path: take the finished endpoint for the current request, once.
void AdoptLocked() {
  if (!g_resolver || g_adopted == g_generation || g_resolver->done.load() != g_generation) return;
  g_adopted = g_generation;
  std::lock_guard lock(g_resolver->mutex);
  if (g_resolver->done != g_generation) return;
  if (g_resolver->error) {
    REXLOG_WARN("Telemetry: {} '{}'", g_resolver->error, g_target);
    return;
  }
  g_socket = g_resolver->result.socket;
  g_address = g_resolver->result.address;
  g_address_size = g_resolver->result.address_size;
  g_resolver->result.socket = kNoSocket;
  REXLOG_INFO("Telemetry: sending Forza Data Out packets ({}) to {}",
              g_horizon ? "horizon" : "fm7", g_target);
  diagnostics::RecordEvent("telemetry.udp.open", {{"target", g_target}});
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

// The old endpoint gets its inactive sample before it closes; any request still pending is cancelled.
void CloseLocked() {
  if (g_socket != kNoSocket) {
    if (g_sent) WriteLocked({});
    CloseSocket(g_socket);
    g_socket = kNoSocket;
  }
  g_sent = false;
  if (!g_target.empty()) RequestLocked("");
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
  // Only posts the target: the resolver thread looks it up and Send adopts the socket. A failed target is not
  // retried until it changes. Guest Send only tries the lock and drops frames while configuration owns it.
  if (enabled && target != g_target) RequestLocked(target);
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
  if (g_resolver) {  // stop without joining: a lookup in progress finishes on its own and frees what it made
    {
      std::lock_guard stop(g_resolver->mutex);
      g_resolver->stopped = true;
    }
    g_resolver->wake.notify_one();
    g_resolver.reset();
  }
}

void Send(const CarSample& sample) {
  if (!REXCVAR_GET(telemetry_udp)) return;
  std::unique_lock lock(g_mutex, std::try_to_lock);
  if (!lock || g_stopped) return;
  if (g_socket == kNoSocket) AdoptLocked();
  if (g_socket == kNoSocket) return;
  WriteLocked(sample);
}

}  // namespace pinyon_shift::telemetry
