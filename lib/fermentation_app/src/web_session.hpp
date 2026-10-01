#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

#include "http_server_lifecycle.hpp"
#include "secure_random_source.hpp"
#include "device_ui_session.hpp"

namespace fermentation {

inline constexpr std::size_t kMaximumWebSessions = 4U;
inline constexpr std::uint64_t kWebSessionIdleLimitMs = 30ULL * 60ULL * 1000ULL;
inline constexpr std::uint64_t kWebSessionAbsoluteLimitMs =
    12ULL * 60ULL * 60ULL * 1000ULL;
inline constexpr std::size_t kMaximumCompletedMutationOutcomes = 8U;
inline constexpr std::size_t kMaximumMutationFingerprintBytes = 512U;
inline constexpr std::size_t kMaximumReplayOutcomeContentTypeBytes = 64U;
inline constexpr std::size_t kMaximumReplayOutcomeBodyBytes = 256U;
inline constexpr std::size_t kMaximumReplayOutcomeBytes =
    kMaximumReplayOutcomeContentTypeBytes + kMaximumReplayOutcomeBodyBytes;
// Raw replay payload only: four sessions, eight outcomes plus one in-flight
// fingerprint each. Fixed entry metadata is covered by the manager size check.
inline constexpr std::size_t kMaximumWebSessionReplayPayloadBytes =
    kMaximumWebSessions * (kMaximumCompletedMutationOutcomes + 1U) *
    (kMaximumMutationFingerprintBytes + kMaximumReplayOutcomeBytes);
inline constexpr std::size_t kMaximumWebSessionManagerBytes = 32ULL * 1024ULL;

struct WebSessionHandle {
    std::size_t slot{0U};
    std::uint64_t generation{0U};
    friend bool operator==(WebSessionHandle left, WebSessionHandle right) {
        return left.slot == right.slot && left.generation == right.generation;
    }
};

enum class WebSessionStatus : std::uint8_t {
    Created,
    Found,
    Missing,
    Expired,
    Capacity,
    RandomUnavailable,
    Invalid,
};

struct WebSessionResult {
    WebSessionStatus status{WebSessionStatus::Invalid};
    std::optional<WebSessionHandle> handle;
    std::string cookieValue;
    std::string csrfToken;
};

enum class MutationSequenceState : std::uint8_t {
    Available,
    InFlight,
    Exhausted,
};

struct MutationSequenceView {
    MutationSequenceState state{MutationSequenceState::Exhausted};
    std::uint64_t nextMutationSeq{0U};
    std::optional<std::uint64_t> inFlightMutationSeq;
};

struct WebMutationOutcome {
    std::uint16_t statusCode{500U};
    std::string contentType{"application/json; charset=utf-8"};
    std::string body;
};

struct WebSessionManagerTestAccess;

enum class MutationReservationStatus : std::uint8_t {
    Reserved,
    ReplayOutcome,
    InFlight,
    SequenceConflict,
    SequenceGap,
    ReplayExpired,
    SequenceReused,
    Exhausted,
    InvalidSession,
    InvalidFingerprint,
};

struct MutationReservation {
    MutationReservationStatus status{MutationReservationStatus::InvalidSession};
    std::optional<std::uint64_t> sequence;
    std::optional<WebMutationOutcome> outcome;
};

struct ServiceLeaseView {
    bool active{false};
    std::optional<std::uint64_t> remainingMillis;
};

[[nodiscard]] std::optional<std::uint64_t> parseMutationSequence(
    const std::string& value) noexcept;
[[nodiscard]] std::string mutationFingerprint(
    const device_platform::HttpRequest& request);

class WebSessionManager final {
   public:
    explicit WebSessionManager(device_platform::ISecureRandomSource& random,
                               device_platform::ServiceSessionPolicy
                                   servicePolicy = {5ULL * 60ULL * 1000ULL,
                                                    15ULL * 60ULL * 1000ULL})
        : random_(random), servicePolicy_(servicePolicy) {}

    [[nodiscard]] WebSessionResult create(std::uint64_t nowMs);
    [[nodiscard]] WebSessionResult find(const std::string& cookie,
                                        std::uint64_t nowMs);
    [[nodiscard]] bool validateCsrf(WebSessionHandle handle,
                                    const std::string& token,
                                    std::uint64_t nowMs);
    [[nodiscard]] std::optional<std::string> csrfToken(WebSessionHandle handle,
                                                       std::uint64_t nowMs);
    [[nodiscard]] std::optional<std::string> cookieValue(
        WebSessionHandle handle, std::uint64_t nowMs);
    [[nodiscard]] bool touch(WebSessionHandle handle, std::uint64_t nowMs);
    void revoke(WebSessionHandle handle);
    void revokeAll();
    [[nodiscard]] bool grantServiceLease(WebSessionHandle handle,
                                         std::uint64_t nowMs);
    [[nodiscard]] bool serviceLeaseActive(WebSessionHandle handle,
                                          std::uint64_t nowMs);
    [[nodiscard]] ServiceLeaseView serviceLeaseStatus(
        WebSessionHandle handle, std::uint64_t nowMs) const;
    void revokeServiceLease(WebSessionHandle handle);

    [[nodiscard]] MutationSequenceView mutationSequence(
        WebSessionHandle handle, std::uint64_t nowMs) const;
    [[nodiscard]] MutationReservation reserveMutation(
        WebSessionHandle handle, std::uint64_t nowMs, std::uint64_t sequence,
        const std::string& fingerprint);
    [[nodiscard]] bool completeMutation(WebSessionHandle handle,
                                        std::uint64_t nowMs,
                                        std::uint64_t sequence,
                                        const std::string& fingerprint,
                                        const WebMutationOutcome& outcome);

   private:
    struct CompletedMutation {
        std::uint64_t sequence{0U};
        std::array<char, kMaximumMutationFingerprintBytes> fingerprint{};
        std::uint16_t fingerprintLength{0U};
        std::uint16_t statusCode{500U};
        std::array<char, kMaximumReplayOutcomeContentTypeBytes> contentType{};
        std::uint16_t contentTypeLength{0U};
        std::array<char, kMaximumReplayOutcomeBodyBytes> body{};
        std::uint16_t bodyLength{0U};
    };
    struct Session {
        bool active{false};
        std::uint64_t generation{0U};
        std::array<std::uint8_t, 16U> id{};
        std::array<std::uint8_t, 16U> csrf{};
        std::uint64_t createdAtMs{0U};
        std::uint64_t lastActivityMs{0U};
        std::uint64_t highWater{0U};
        std::uint64_t replayFloor{0U};
        std::optional<CompletedMutation> inFlight;
        std::array<CompletedMutation, kMaximumCompletedMutationOutcomes>
            completed{};
        std::size_t completedCount{0U};
        device_platform::ServiceSessionLease serviceLease;
    };

    friend struct WebSessionManagerTestAccess;

    [[nodiscard]] Session* get(WebSessionHandle handle, std::uint64_t nowMs);
    [[nodiscard]] const Session* get(WebSessionHandle handle,
                                     std::uint64_t nowMs) const;
    [[nodiscard]] static std::string hex(const std::uint8_t* bytes,
                                         std::size_t length);
    [[nodiscard]] static bool decodeCookie(const std::string& cookie,
                                           std::array<std::uint8_t, 16U>& id);
    [[nodiscard]] static bool equalId(const Session& session,
                                      const std::array<std::uint8_t, 16U>& id);
    static void clearMutation(CompletedMutation& mutation) noexcept;
    static void clearMutationState(Session& session) noexcept;
    [[nodiscard]] static bool fingerprintMatches(
        const CompletedMutation& mutation,
        const std::string& fingerprint) noexcept;
    [[nodiscard]] static bool storeOutcome(
        const WebMutationOutcome& outcome,
        CompletedMutation& mutation) noexcept;
    [[nodiscard]] static WebMutationOutcome restoreOutcome(
        const CompletedMutation& mutation);

    device_platform::ISecureRandomSource& random_;
    device_platform::ServiceSessionPolicy servicePolicy_;
    mutable std::mutex mutex_;
    std::array<Session, kMaximumWebSessions> sessions_{};
};

}  // namespace fermentation
