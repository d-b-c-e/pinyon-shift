#include <chrono>
#include <cstring>
#include <cstdlib>
#include <future>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
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

int checks = 0;
void Check(bool ok, const char* message) {
  ++checks;
  if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
int starts = 0, cleanups = 0, resolves = 0, frees = 0, sockets = 0, closes = 0;
int setup_calls = 0, failure = 0;
bool drop_send = false;
std::map<int, bool> nonblocking;
std::vector<std::vector<uint8_t>> packets;
std::vector<int> endpoints;
int WSAStartup(int, WSADATA*) { ++starts; return failure == 1 ? -1 : 0; }
int WSACleanup() { ++cleanups; return 0; }
int getaddrinfo(const char*, const char*, const addrinfo*, addrinfo** found) {
  ++resolves;
  if (failure == 2) return -1;
  static sockaddr address;
  static addrinfo info;
  address.endpoint = resolves;
  info.ai_addr = &address;
  *found = &info;
  return 0;
}
void freeaddrinfo(addrinfo*) { ++frees; }
int socket(int, int, int) {
  ++sockets;
  if (failure == 3) return -1;
  nonblocking[sockets] = false;
  return sockets;
}
int closesocket(int s) { ++closes; nonblocking.erase(s); return 0; }
int close(int s) { return closesocket(s); }
int ioctlsocket(int s, int command, u_long* value) {
  ++setup_calls;
  Check(command == FIONBIO && *value == 1, "Windows requests nonblocking socket");
  if (failure == 4) return -1;
  nonblocking[s] = true;
  return 0;
}
int fcntl(int s, int command, int value) {
  ++setup_calls;
  if (command == F_GETFL) return failure == 5 ? -1 : 8;
  Check(command == F_SETFL && value == (8 | O_NONBLOCK), "POSIX preserves existing flags");
  if (failure == 4) return -1;
  nonblocking[s] = true;
  return 0;
}
int sendto(int s, const char* bytes, int size, int, const sockaddr* address, socklen_t) {
  Check(nonblocking.at(s), "every send uses nonblocking socket");
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
  telemetry_udp = true;
  Initialize();
  Check(resolves == 1 && frees == 1 && sockets == 1 && setup_calls > 0, "initialize prepares endpoint");
  Check(packets.empty(), "configuration does not emit driving sample");
  Send(sample);
  Check(resolves == 1 && packets.size() == 1 && packets[0].size() == 311, "frame never resolves; FM7 length");
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
  Check(packets.size() == 2 && resolves == 1, "would-block drops once without retry or resolve");
  drop_send = false;
  telemetry_udp_target = "second:55301";
  Configure();
  Check(packets.size() == 3 && Inactive(2) && endpoints[2] == 1 && closes == 1,
        "target change parks old endpoint");
  Send(sample);
  Check(endpoints.back() == 2 && packets.back().size() == 311, "new target receives next frame");
  telemetry_udp_format = "horizon";
  Configure();
  Check(Inactive(packets.size() - 1) && packets.back().size() == 311, "format change parks using old layout");
  Send(sample);
  Check(packets.back().size() == 324 && Field<float>(packets.size() - 1, 256) == 12.5f && packets.back()[319] == 3,
        "Horizon production layout reaches new socket");
  const auto before_disable = packets.size();
  telemetry_udp = false;
  Send(sample); Configure();
  Check(packets.size() == before_disable + 1 && Inactive(packets.size() - 1), "disable parks once even after frame gate");
  Configure(); Send(sample);
  Check(packets.size() == before_disable + 1 && nonblocking.empty(), "disabled stays closed");
  telemetry_udp = true;
  Configure(); Send(sample);
  const auto before_close = packets.size();
  drop_send = true;
  Shutdown();
  Check(packets.size() == before_close + 1 && Inactive(packets.size() - 1) && nonblocking.empty(),
        "normal shutdown attempts inactive once and closes despite error");
  const int resolved_at_close = resolves;
  Shutdown(); Configure(); Send(sample);
  Check(resolves == resolved_at_close && packets.size() == before_close + 1, "queued config and repeat shutdown cannot reopen");
#ifdef _WIN32
  Check(starts == 1 && cleanups == 1, "Winsock lifetime balanced across disable and shutdown");
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
    const int resolved = resolves, freed = frees, opened = sockets, closed = closes;
    Initialize(); Send(sample); Configure();
    Check(packets.size() == before && nonblocking.empty(), "setup failure never sends or retains socket");
    Check(resolves == resolved + (mode == 1 ? 0 : 1), "failed target is not retried each frame");
    if (mode >= 3) Check(frees == freed + 1 && sockets == opened + 1, "resolved address released after socket/setup failure");
    if (mode >= 4) Check(closes == closed + 1, "nonblocking failure closes owned handle");
    Shutdown();
  }
  failure = 0;
  Initialize(); Send(sample); Shutdown();
  Check(nonblocking.empty() && Inactive(packets.size() - 1), "new lifecycle recovers and closes");
  std::cout << checks << " sender checks pass\n";
}
