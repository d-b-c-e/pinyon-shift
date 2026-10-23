#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "telemetry/telemetry_out.h"

// PLATFORM
// Deliberately no platform socket headers or linkage: these names are inert fakes.
using SOCKET = int;
using u_long = unsigned long;
using socklen_t = unsigned;
#ifdef _WIN32
constexpr int INVALID_SOCKET = -1;
#endif
constexpr int AF_UNSPEC = 0, SOCK_DGRAM = 2, IPPROTO_UDP = 17;
constexpr int FIONBIO = 1, F_GETFL = 2, F_SETFL = 3, O_NONBLOCK = 4;
struct WSADATA {};
#define MAKEWORD(a, b) ((a) | ((b) << 8))
struct sockaddr { int endpoint = 0; };
using sockaddr_storage = sockaddr;
struct addrinfo {
  int ai_family = 0, ai_socktype = 0, ai_protocol = 0;
  socklen_t ai_addrlen = sizeof(sockaddr);
  sockaddr* ai_addr = nullptr;
};
bool telemetry_udp = false, telemetry_udp_moving_only = false;
std::string telemetry_udp_target = "first:5300", telemetry_udp_format = "fm7";
#define REXCVAR_GET(name) name
#define REXLOG_WARN(...) ((void)0)
#define REXLOG_INFO(...) ((void)0)
namespace pinyon_shift::diagnostics {
void RecordEvent(const char*, std::initializer_list<std::pair<std::string, std::string>>) {}
}

std::atomic<int> checks{0};
void Check(bool ok, const char* message) {
  ++checks;
  // _Exit: a resolver thread may still wait inside a fake, so no static destruction on failure.
  if (!ok) { std::cerr << "FAIL: " << message << std::endl; std::_Exit(1); }
}
// The fakes run on the resolver thread as well as the caller's: counters are atomic, shared maps sit under `fake`.
std::atomic<int> starts{0}, cleanups{0}, resolves{0}, frees{0}, sockets{0}, closes{0}, setup_calls{0}, failure{0};
std::mutex fake;
std::condition_variable gate_wake;
bool gate_closed = false;  // a lookup waits here while true, like a slow DNS answer
int in_lookup = 0;
std::vector<std::string> hosts;
bool drop_send = false;
std::map<int, bool> nonblocking;
std::vector<std::vector<uint8_t>> packets;
std::vector<int> endpoints;
bool NoSockets() { std::lock_guard l(fake); return nonblocking.empty(); }
void Gate(bool closed) {
  { std::lock_guard l(fake); gate_closed = closed; }
  gate_wake.notify_all();
}
int InLookup() { std::lock_guard l(fake); return in_lookup; }
bool Until(const std::function<bool()>& ready) {
  for (int i = 0; i < 5000; ++i) {
    if (ready()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return ready();
}
int WSAStartup(int, WSADATA*) {  // counts successful starts only: a failed start needs no cleanup
  if (failure == 1) return -1;
  ++starts;
  return 0;
}
int WSACleanup() { ++cleanups; return 0; }
int getaddrinfo(const char* host, const char*, const addrinfo*, addrinfo** found) {
  {
    std::unique_lock l(fake);
    ++in_lookup;
    hosts.emplace_back(host);
    gate_wake.wait(l, [] { return !gate_closed; });
    --in_lookup;
  }
  const int n = ++resolves;
  if (failure == 2) return -1;
  auto* info = new addrinfo;
  info->ai_addr = new sockaddr;
  info->ai_addr->endpoint = n;
  *found = info;
  return 0;
}
void freeaddrinfo(addrinfo* info) { ++frees; delete info->ai_addr; delete info; }
int socket(int, int, int) {
  const int s = ++sockets;
  if (failure == 3) return -1;
  std::lock_guard l(fake);
  nonblocking[s] = false;
  return s;
}
int closesocket(int s) { ++closes; std::lock_guard l(fake); nonblocking.erase(s); return 0; }
int close(int s) { return closesocket(s); }
int ioctlsocket(int s, int command, u_long* value) {
  ++setup_calls;
  Check(command == FIONBIO && *value == 1, "Windows requests nonblocking socket");
  if (failure == 4) return -1;
  std::lock_guard l(fake);
  nonblocking[s] = true;
  return 0;
}
int fcntl(int s, int command, int value) {
  ++setup_calls;
  if (command == F_GETFL) return failure == 5 ? -1 : 8;
  Check(command == F_SETFL && value == (8 | O_NONBLOCK), "POSIX preserves existing flags");
  if (failure == 4) return -1;
  std::lock_guard l(fake);
  nonblocking[s] = true;
  return 0;
}
int sendto(int s, const char* bytes, int size, int, const sockaddr* address, socklen_t) {
  { std::lock_guard l(fake); Check(nonblocking.at(s), "every send uses nonblocking socket"); }
  packets.emplace_back(bytes, bytes + size);
  endpoints.push_back(address->endpoint);
  return drop_send ? -1 : size;
}

// PRODUCTION_SENDER

template <typename T> T Field(size_t packet, size_t offset) {
  T value;
  std::memcpy(&value, packets.at(packet).data() + offset, sizeof(value));
  return value;
}
bool Inactive(size_t packet) {
  const auto& bytes = packets.at(packet);
  for (size_t i = 0; i < bytes.size(); ++i)
    if ((i < 4 || i >= 8) && bytes[i] != 0) return false;
  return true;
}
// The resolver has answered the current request (or there is none).
bool Settle() {
  using namespace pinyon_shift::telemetry;
  return Until([] {
    std::lock_guard lock(g_mutex);
    return !g_resolver || g_target.empty() || g_resolver->done.load() == g_generation;
  });
}
bool Ready(std::future<void>& f) { return f.wait_for(std::chrono::seconds(2)) == std::future_status::ready; }

int main() {
  using namespace pinyon_shift::telemetry;
  CarSample sample;
  sample.driving = true;
  sample.rpm = 2345;
  sample.redline_rpm = 7000;
  sample.speed_mps = 12.5f;
  sample.gear = 3;
  Initialize(); Send(sample); Shutdown();
  Check(starts == 0 && resolves == 0 && sockets == 0 && packets.empty(), "disabled lifecycle is inert");

  // A lookup that stays pending: configuration, frames and close never wait for it, and nothing is published from it.
  telemetry_udp = true;
  telemetry_udp_target = "slow:5300";
  Gate(true);
  auto configured = std::async(std::launch::async, [] { Initialize(); });
  Check(Ready(configured), "configuration returns while the lookup is pending");
  Check(Until([] { return InLookup() == 1; }), "the lookup is in progress");
  Send(sample);
  Check(packets.empty() && NoSockets(), "no socket or packet while the lookup is pending");
  auto closed = std::async(std::launch::async, [] { Shutdown(); });
  Check(Ready(closed), "close does not wait for the lookup");
  Gate(false);
  Check(Until([] { return frees == 1 && closes == 1; }) && resolves == 1 && sockets == 1,
        "a late result after close is freed and its socket closed");
  Check(NoSockets() && packets.empty(), "the late result never sends or stays open");

  // A newer target, then 50 more edits, while the first lookup is pending: one lookup in flight and one pending.
  Gate(true);
  Initialize();
  Check(Until([] { return InLookup() == 1; }), "the second lookup is in progress");
  telemetry_udp_target = "fresh:5301";
  Configure();
  for (int i = 0; i < 50; ++i) { telemetry_udp_target = "edit" + std::to_string(i) + ":5302"; Configure(); }
  telemetry_udp_target = "fresh:5301";
  Configure();
  Gate(false);
  Check(Settle(), "the newest request is answered");
  Send(sample);
  Check(resolves == 3 && packets.size() == 1 && endpoints.back() == 3, "repeated edits resolve only the newest target");
  {
    std::lock_guard l(fake);
    Check(hosts.size() == 3 && hosts[1] == "slow" && hosts[2] == "fresh", "edits in between are never looked up");
  }
  Check(closes == 2, "the superseded lookup's socket is closed");

  // A finished result for an older request is never adopted, even when it is still waiting in the resolver.
  {
    Gate(true);
    telemetry_udp_target = "white:5303";
    Configure();  // parks the fresh endpoint; the resolver now waits inside this lookup
    Check(Until([] { return InLookup() == 1; }), "the third lookup is in progress");
    const auto before = packets.size();
    {
      std::lock_guard lock(g_mutex);
      std::lock_guard slot(g_resolver->mutex);
      g_resolver->result.socket = socket(0, 0, 0);
      { std::lock_guard l(fake); nonblocking[g_resolver->result.socket] = true; }
      g_resolver->result.address.endpoint = 999;
      g_resolver->error = nullptr;
      g_resolver->done = g_generation - 1;
    }
    Send(sample);
    Check(packets.size() == before, "a finished result for an older request is never adopted");
    {
      std::lock_guard lock(g_mutex);
      std::lock_guard slot(g_resolver->mutex);
      CloseSocket(g_resolver->result.socket);
      g_resolver->result.socket = kNoSocket;
    }
    Gate(false);
    Check(Settle(), "the third request is answered");
  }

  // Disable while a lookup is pending: the late result is closed, never adopted.
  Gate(true);
  telemetry_udp_target = "gone:5304";
  Configure();
  Check(Until([] { return InLookup() == 1; }), "the fourth lookup is in progress");
  telemetry_udp = false;
  Configure();
  const int closed_before = closes;
  Gate(false);
  Check(Until([&] { return closes == closed_before + 1; }) && NoSockets(), "a lookup finished after disable is closed");
  const auto parked = packets.size();
  telemetry_udp = true;
  Send(sample);
  Check(packets.size() == parked, "a disabled sender adopts nothing when re-enabled without a new request");
  Shutdown();
  Check(Until([] { return InLookup() == 0; }) && NoSockets(), "pending work drains with no open socket");
#ifdef _WIN32
  Check(Until([] { return cleanups == starts; }) && starts == 2, "each resolver's Winsock reference ends with it");
#endif

  // The ordinary lifecycle, now through the resolver: counts below are relative to this point.
  const int r0 = resolves, f0 = frees, s0 = sockets, c0 = closes;
  packets.clear(); endpoints.clear();
  telemetry_udp_target = "first:5300";
  telemetry_udp_format = "fm7";
  Initialize();
  Check(Settle() && resolves == r0 + 1 && frees == f0 + 1 && sockets == s0 + 1 && setup_calls > 0,
        "initialize prepares endpoint off the caller");
  Check(packets.empty(), "configuration does not emit driving sample");
  Send(sample);
  Check(resolves == r0 + 1 && packets.size() == 1 && packets[0].size() == 311, "frame never resolves; FM7 length");
  Check(Field<int>(0, 0) == 1 && Field<float>(0, 16) == 2345 && Field<float>(0, 244) == 12.5f && packets[0][307] == 3,
        "production encoder preserves requested fields");
  // Hold the real production lock on this thread; Send must finish on another.
  {
    std::lock_guard lock(g_mutex);
    auto sent = std::async(std::launch::async, [&] { Send(sample); });
    Check(sent.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "busy config drops frame without waiting");
    sent.get();
  }
  Check(packets.size() == 1, "contended frame emits nothing");
  drop_send = true;
  Send(sample);
  Check(packets.size() == 2 && resolves == r0 + 1, "would-block drops once without retry or resolve");
  drop_send = false;
  telemetry_udp_target = "second:55301";
  Configure();
  Check(packets.size() == 3 && Inactive(2) && endpoints[2] == r0 + 1 && closes == c0 + 1,
        "target change parks old endpoint");
  Check(Settle(), "the new target is answered");
  Send(sample);
  Check(endpoints.back() == r0 + 2 && packets.back().size() == 311, "new target receives next frame");
  telemetry_udp_format = "horizon";
  Configure();
  Check(Inactive(packets.size() - 1) && packets.back().size() == 311, "format change parks using old layout");
  Check(Settle(), "the format change is answered");
  Send(sample);
  Check(packets.back().size() == 324 && Field<float>(packets.size() - 1, 256) == 12.5f && packets.back()[319] == 3,
        "Horizon production layout reaches new socket");
  const auto before_disable = packets.size();
  telemetry_udp = false;
  Send(sample); Configure();
  Check(packets.size() == before_disable + 1 && Inactive(packets.size() - 1), "disable parks once even after frame gate");
  Configure(); Send(sample);
  Check(packets.size() == before_disable + 1 && NoSockets(), "disabled stays closed");
  telemetry_udp = true;
  Configure();
  Check(Settle(), "re-enable is answered");
  Send(sample);
  const auto before_close = packets.size();
  drop_send = true;
  Shutdown();
  Check(packets.size() == before_close + 1 && Inactive(packets.size() - 1) && NoSockets(),
        "normal shutdown attempts inactive once and closes despite error");
  const int resolved_at_close = resolves;
  Shutdown(); Configure(); Send(sample);
  Check(resolves == resolved_at_close && packets.size() == before_close + 1, "queued config and repeat shutdown cannot reopen");
#ifdef _WIN32
  Check(Until([] { return cleanups == starts; }) && starts == 3, "Winsock lifetime balanced across disable and shutdown");
#endif
  drop_send = false;
  // Each setup failure must leave Send inert; failed targets stay sticky until reconfigured.
  for (int mode = 1; mode <= 5; ++mode) {
#ifndef _WIN32
    if (mode == 1) continue;
#else
    if (mode == 5) continue;
#endif
    failure = mode;
    const auto before = packets.size();
    const int resolved = resolves, freed = frees, opened = sockets, closed_count = closes;
    Initialize();
    Check(Settle(), "a failing request is answered");
    Send(sample); Configure(); Send(sample);
    Check(packets.size() == before && NoSockets(), "setup failure never sends or retains socket");
    Check(resolves == resolved + (mode == 1 ? 0 : 1), "failed target is not retried each frame");
    if (mode >= 3) Check(frees == freed + 1 && sockets == opened + 1, "resolved address released after socket/setup failure");
    if (mode >= 4) Check(closes == closed_count + 1, "nonblocking failure closes owned handle");
    Shutdown();
  }
  failure = 0;
  Initialize();
  Check(Settle(), "the recovered request is answered");
  Send(sample); Shutdown();
  Check(NoSockets() && Inactive(packets.size() - 1), "new lifecycle recovers and closes");
  Check(Until([] { return InLookup() == 0; }), "no lookup left running");
#ifdef _WIN32
  Check(Until([] { return cleanups == starts; }), "every resolver ended its Winsock reference");
#endif
  std::cout << checks << " sender checks pass\n";
  std::cout.flush();
  std::_Exit(0);  // detached resolver threads may still be finishing; skip static destruction
}
