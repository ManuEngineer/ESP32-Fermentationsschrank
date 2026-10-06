#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace device_platform {

inline constexpr std::size_t kReplayDigestBytes = 32U;
using ReplayDigest = std::array<std::uint8_t, kReplayDigestBytes>;

struct ReplayDigestInput {
    std::string_view method;
    std::string_view path;
    std::string_view bodyLength;
    std::string_view body;
};

// The adapter hashes exactly:
// method || NUL || path || NUL || canonical decimal body length || NUL || body
// It is deliberately a one-purpose port, not a general cryptographic facade.
class IReplayDigest {
   public:
    IReplayDigest() = default;
    virtual ~IReplayDigest() = default;
    IReplayDigest(const IReplayDigest&) = delete;
    IReplayDigest& operator=(const IReplayDigest&) = delete;
    IReplayDigest(IReplayDigest&&) = delete;
    IReplayDigest& operator=(IReplayDigest&&) = delete;

    [[nodiscard]] virtual bool digest(const ReplayDigestInput& input,
                                      ReplayDigest& out) = 0;
};

}  // namespace device_platform
