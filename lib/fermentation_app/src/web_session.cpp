#include "web_session.hpp"

#include <algorithm>
#include <limits>

namespace fermentation {
namespace {

constexpr char kHex[] = "0123456789abcdef";

bool constantEqual(const std::string& left, const std::string& right) noexcept {
    if (left.size() != right.size()) return false;
    unsigned char difference = 0U;
    for (std::size_t i = 0U; i < left.size(); ++i)
        difference |= static_cast<unsigned char>(left[i] ^ right[i]);
    return difference == 0U;
}

std::uint64_t fnv1a(const std::string& value) noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char byte : value) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    return hash;
}

}  // namespace

std::optional<std::uint64_t> parseMutationSequence(
    const std::string& value) noexcept {
    if (value.empty() || value.size() > 20U) return std::nullopt;
    std::uint64_t result = 0U;
    for (const unsigned char byte : value) {
        if (byte < '0' || byte > '9') return std::nullopt;
        const auto digit = static_cast<std::uint64_t>(byte - '0');
        if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10U)
            return std::nullopt;
        result = result * 10U + digit;
    }
    return result == 0U ? std::nullopt : std::optional<std::uint64_t>{result};
}

std::string mutationFingerprint(const device_platform::HttpRequest& request) {
    // The replay table stores only a bounded digest, never the request body or
    // a credential. FNV-1a is sufficient here because authentication/CSRF and
    // the monotone sequence provide the security boundary; this value only
    // distinguishes retries from reuse within one live session.
    std::string material;
    material.reserve(std::min<std::size_t>(512U, request.body.size() +
                                                     request.method.size() +
                                                     request.path.size() + 32U));
    material.append(request.method);
    material.push_back('\0');
    material.append(request.path);
    material.push_back('\0');
    material.append(std::to_string(request.body.size()));
    material.push_back('\0');
    material.append(request.body);
    const auto hash = fnv1a(material);
    std::string result;
    result.reserve(16U);
    for (int shift = 60; shift >= 0; shift -= 4)
        result.push_back(kHex[(hash >> static_cast<unsigned>(shift)) & 0x0FU]);
    return result;
}

std::string WebSessionManager::hex(const std::uint8_t* bytes,
                                   std::size_t length) {
    std::string result;
    result.reserve(length * 2U);
    for (std::size_t i = 0U; i < length; ++i) {
        result.push_back(kHex[(bytes[i] >> 4U) & 0x0FU]);
        result.push_back(kHex[bytes[i] & 0x0FU]);
    }
    return result;
}

bool WebSessionManager::decodeCookie(
    const std::string& cookie, std::array<std::uint8_t, 16U>& id) {
    constexpr char prefix[] = "FSSESSION=";
    const auto start = cookie.find(prefix);
    if (start == std::string::npos || start + sizeof(prefix) - 1U + 32U >
                                         cookie.size())
        return false;
    const auto valueStart = start + sizeof(prefix) - 1U;
    const auto value = cookie.substr(valueStart, 32U);
    for (std::size_t i = 0U; i < id.size(); ++i) {
        const auto nibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        const int high = nibble(value[i * 2U]);
        const int low = nibble(value[i * 2U + 1U]);
        if (high < 0 || low < 0) return false;
        id[i] = static_cast<std::uint8_t>((high << 4U) | low);
    }
    return true;
}

bool WebSessionManager::equalId(
    const Session& session, const std::array<std::uint8_t, 16U>& id) {
    std::uint8_t difference = 0U;
    for (std::size_t i = 0U; i < id.size(); ++i) difference |= session.id[i] ^ id[i];
    return difference == 0U;
}

WebSessionManager::Session* WebSessionManager::get(WebSessionHandle handle,
                                                     std::uint64_t nowMs) {
    if (handle.slot >= sessions_.size()) return nullptr;
    auto& session = sessions_[handle.slot];
    if (!session.active || session.generation != handle.generation) return nullptr;
    if (nowMs < session.createdAtMs || nowMs - session.createdAtMs >=
                                        kWebSessionAbsoluteLimitMs ||
        nowMs < session.lastActivityMs || nowMs - session.lastActivityMs >=
                                            kWebSessionIdleLimitMs) {
        session.active = false;
        return nullptr;
    }
    return &session;
}

const WebSessionManager::Session* WebSessionManager::get(
    WebSessionHandle handle, std::uint64_t nowMs) const {
    return const_cast<WebSessionManager*>(this)->get(handle, nowMs);
}

WebSessionResult WebSessionManager::create(std::uint64_t nowMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    Session* target = nullptr;
    std::size_t slot = 0U;
    for (; slot < sessions_.size(); ++slot) {
        if (!sessions_[slot].active) {
            target = &sessions_[slot];
            break;
        }
    }
    if (target == nullptr) return {WebSessionStatus::Capacity, std::nullopt, {}, {}};
    if (!random_.fill(target->id.data(), target->id.size()) ||
        !random_.fill(target->csrf.data(), target->csrf.size())) {
        return {WebSessionStatus::RandomUnavailable, std::nullopt, {}, {}};
    }
    target->active = true;
    ++target->generation;
    if (target->generation == 0U) ++target->generation;
    target->createdAtMs = nowMs;
    target->lastActivityMs = nowMs;
    target->highWater = 0U;
    target->replayFloor = 0U;
    target->inFlight.reset();
    target->completed.clear();
    target->serviceLease = device_platform::ServiceSessionLease{};
    const WebSessionHandle handle{slot, target->generation};
    const auto id = hex(target->id.data(), target->id.size());
    return {WebSessionStatus::Created, handle, id, hex(target->csrf.data(), target->csrf.size())};
}

WebSessionResult WebSessionManager::find(const std::string& cookie,
                                         std::uint64_t nowMs) {
    std::array<std::uint8_t, 16U> id{};
    if (!decodeCookie(cookie, id)) return {WebSessionStatus::Missing, std::nullopt, {}, {}};
    std::lock_guard<std::mutex> lock(mutex_);
    for (std::size_t slot = 0U; slot < sessions_.size(); ++slot) {
        auto& session = sessions_[slot];
        if (session.active && equalId(session, id)) {
            const WebSessionHandle handle{slot, session.generation};
            if (get(handle, nowMs) == nullptr)
                return {WebSessionStatus::Expired, std::nullopt, {}, {}};
            session.lastActivityMs = nowMs;
            return {WebSessionStatus::Found, handle, hex(session.id.data(), 16U),
                    hex(session.csrf.data(), 16U)};
        }
    }
    return {WebSessionStatus::Missing, std::nullopt, {}, {}};
}

bool WebSessionManager::validateCsrf(WebSessionHandle handle,
                                     const std::string& token,
                                     std::uint64_t nowMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto* session = get(handle, nowMs);
    return session != nullptr && constantEqual(token, hex(session->csrf.data(), 16U));
}

std::optional<std::string> WebSessionManager::csrfToken(WebSessionHandle handle,
                                                        std::uint64_t nowMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto* session = get(handle, nowMs);
    return session == nullptr ? std::nullopt
                              : std::optional<std::string>{hex(session->csrf.data(), 16U)};
}

std::optional<std::string> WebSessionManager::cookieValue(WebSessionHandle handle,
                                                          std::uint64_t nowMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto* session = get(handle, nowMs);
    return session == nullptr ? std::nullopt
                              : std::optional<std::string>{hex(session->id.data(), 16U)};
}

bool WebSessionManager::touch(WebSessionHandle handle, std::uint64_t nowMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* session = get(handle, nowMs);
    if (session == nullptr) return false;
    session->lastActivityMs = nowMs;
    return true;
}

void WebSessionManager::revoke(WebSessionHandle handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle.slot < sessions_.size() && sessions_[handle.slot].generation == handle.generation)
        sessions_[handle.slot].active = false;
}

void WebSessionManager::revokeAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& session : sessions_) session.active = false;
}

bool WebSessionManager::grantServiceLease(WebSessionHandle handle,
                                          std::uint64_t nowMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* session = get(handle, nowMs);
    if (session == nullptr) return false;
    session->serviceLease = device_platform::ServiceSessionLease(
        {5ULL * 60ULL * 1000ULL, 15ULL * 60ULL * 1000ULL}, nowMs);
    return session->serviceLease.activeAt(nowMs);
}

bool WebSessionManager::serviceLeaseActive(WebSessionHandle handle,
                                            std::uint64_t nowMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* session = get(handle, nowMs);
    if (session == nullptr || !session->serviceLease.activeAt(nowMs)) {
        return false;
    }
    session->serviceLease.observe(
        device_platform::ServiceSessionEvent::RelevantUserActivity, nowMs);
    return session->serviceLease.activeAt(nowMs);
}

void WebSessionManager::revokeServiceLease(WebSessionHandle handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle.slot < sessions_.size() &&
        sessions_[handle.slot].generation == handle.generation) {
        sessions_[handle.slot].serviceLease =
            device_platform::ServiceSessionLease{};
    }
}

MutationSequenceView WebSessionManager::mutationSequence(
    WebSessionHandle handle, std::uint64_t nowMs) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto* session = get(handle, nowMs);
    if (session == nullptr) return {};
    if (session->inFlight.has_value())
        return {MutationSequenceState::InFlight, 0U, session->inFlight->sequence};
    if (session->highWater == std::numeric_limits<std::uint64_t>::max())
        return {MutationSequenceState::Exhausted, 0U, std::nullopt};
    return {MutationSequenceState::Available, session->highWater + 1U, std::nullopt};
}

MutationReservation WebSessionManager::reserveMutation(
    WebSessionHandle handle, std::uint64_t nowMs, std::uint64_t sequence,
    const std::string& fingerprint) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* session = get(handle, nowMs);
    if (session == nullptr) return {MutationReservationStatus::InvalidSession, std::nullopt, std::nullopt};
    if (session->highWater == std::numeric_limits<std::uint64_t>::max())
        return {MutationReservationStatus::Exhausted, std::nullopt, std::nullopt};
    if (session->inFlight.has_value()) {
        if (sequence == session->inFlight->sequence) {
            return {constantEqual(fingerprint, session->inFlight->fingerprint)
                        ? MutationReservationStatus::InFlight
                        : MutationReservationStatus::SequenceReused,
                    sequence, std::nullopt};
        }
        return {MutationReservationStatus::SequenceConflict, std::nullopt, std::nullopt};
    }
    if (sequence <= session->replayFloor)
        return {MutationReservationStatus::ReplayExpired, sequence, std::nullopt};
    for (const auto& completed : session->completed) {
        if (completed.sequence == sequence) {
            if (!constantEqual(completed.fingerprint, fingerprint))
                return {MutationReservationStatus::SequenceReused, sequence, std::nullopt};
            return {MutationReservationStatus::ReplayOutcome, sequence, completed.outcome};
        }
    }
    if (sequence > session->highWater + 1U)
        return {MutationReservationStatus::SequenceGap, sequence, std::nullopt};
    if (sequence <= session->highWater)
        return {MutationReservationStatus::ReplayExpired, sequence, std::nullopt};
    session->inFlight = CompletedMutation{sequence, fingerprint, {}};
    return {MutationReservationStatus::Reserved, sequence, std::nullopt};
}

bool WebSessionManager::completeMutation(
    WebSessionHandle handle, std::uint64_t nowMs, std::uint64_t sequence,
    const std::string& fingerprint, const WebMutationOutcome& outcome) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* session = get(handle, nowMs);
    if (session == nullptr || !session->inFlight.has_value() ||
        session->inFlight->sequence != sequence ||
        !constantEqual(session->inFlight->fingerprint, fingerprint))
        return false;
    session->highWater = sequence;
    session->inFlight->outcome = outcome;
    session->completed.push_back(std::move(*session->inFlight));
    session->inFlight.reset();
    while (session->completed.size() > 8U) {
        session->replayFloor = session->completed.front().sequence;
        session->completed.pop_front();
    }
    return true;
}

}  // namespace fermentation
