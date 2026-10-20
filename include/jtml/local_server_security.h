#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <initializer_list>
#include <mutex>
#include <string>
#include <thread>

namespace JTMLInterpreter {

// Track recursive ownership so blocking lifecycle operations can reject a
// caller holding the lock instead of deadlocking a worker's broadcast.
class LocalServerExecutionMutex {
    std::recursive_mutex mutex;
    // Count all framework execution locks on this thread, including locks for
    // other headless interpreters: their callbacks can depend on this worker.
    inline static thread_local size_t heldDepth = 0;
public:
    void lock() { mutex.lock(); ++heldDepth; }
    bool try_lock() { if (!mutex.try_lock()) return false; ++heldDepth; return true; }
    void unlock() { --heldDepth; mutex.unlock(); }
    static bool heldByCurrentThread() { return heldDepth != 0; }
};

// Live interpreters, CLI HTTP handlers and WebSocket callbacks share this lock.
inline LocalServerExecutionMutex& localServerExecutionMutex() {
    static LocalServerExecutionMutex mutex;
    return mutex;
}

inline std::string localServerLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

inline std::string localServerAuthority(const std::string& host, uint16_t port) {
    return host + (port == 80 ? std::string() : ":" + std::to_string(port));
}

inline bool localServerRequestAllowed(const std::string& hostHeader,
                                      const std::string& originHeader,
                                      bool hasOrigin,
                                      const std::string& fetchSite,
                                      uint16_t listenerPort,
                                      uint16_t httpPort) {
    const auto host = localServerLower(hostHeader);
    const auto origin = localServerLower(originHeader);
    if (!fetchSite.empty() && fetchSite != "same-origin" && fetchSite != "none" &&
        !(hasOrigin && listenerPort != httpPort && (fetchSite == "same-site" || fetchSite == "cross-site"))) return false;
    for (const auto* name : {"localhost", "127.0.0.1", "[::1]"}) {
        const auto authority = localServerAuthority(name, listenerPort);
        if (host != authority && !(listenerPort == 80 && host == std::string(name) + ":80")) continue;
        if (!hasOrigin) return true; // Native clients remain usable without browser headers.
        // srcdoc Studio previews can use localhost for the socket while the
        // embedding HTTP page uses 127.0.0.1. Pin all aliases to this HTTP port.
        for (const auto* originHost : {"localhost", "127.0.0.1", "[::1]"}) {
            if (listenerPort == httpPort && std::string(originHost) != name) continue;
            const auto expected = "http://" + localServerAuthority(originHost, httpPort);
            if (origin == expected || (httpPort == 80 && origin == expected + ":80")) return true;
        }
        return false;
    }
    return false;
}

} // namespace JTMLInterpreter
