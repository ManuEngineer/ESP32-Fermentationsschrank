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
    // Retain the exact bounded request identity. A short non-cryptographic
    // digest could misclassify a changed payload as an identical retry.
    if (request.method.empty() || request.method.size() > 16U ||
        request.path.empty() || request.path.size() > 256U ||
        request.body.size() > 4096U) {
        return {};
    }
    std::string material;
    material.reserve(request.body.size() + request.method.size() +
                     request.path.size() + 32U);
    material.append(request.method);
    material.push_back('\0');
    material.append(request.path);
    material.push_back('\0');
    material.append(std::to_string(request.body.size()));
    material.push_back('\0');
    material.append(request.body);
    return material;
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

bool WebSessionManager::decodeCookie(const std::string& cookie,
                                     std::array<std::uint8_t, 16U>& id) {
    std::optional<std::string> value;
    std::size_t start = 0U;
    while (start <= cookie.size()) {
        const auto separator = cookie.find(';', start);
        const auto end =
            separator == std::string::npos ? cookie.size() : separator;
        auto first = cookie.find_first_not_of(" \t", start);
        if (first == std::string::npos || first >= end) {
            if (end != cookie.size()) return false;
        } else {
            auto last = cookie.find_last_not_of(" \t", end - 1U);
            if (last == std::string::npos || last < first) return false;
            const auto equals = cookie.find('=', first);
            if (equals == std::string::npos || equals >= end || equals == first)
                return false;
            const auto name = cookie.substr(first, equals - first);
            if (name == "FSSESSION") {
                if (value.has_value()) return false;
                const auto valueStart = equals + 1U;
                if (valueStart > last) return false;
                value = cookie.substr(valueStart, last - valueStart + 1U);
            }
        }
        if (separator == std::string::npos) break;
        start = separator + 1U;
    }
    if (!value.has_value() || value->size() != 32U) return false;
    for (std::size_t i = 0U; i < id.size(); ++i) {
        const auto nibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        const int high = nibble((*value)[i * 2U]);
        const int low = nibble((*value)[i * 2U + 1U]);
        if (high < 0 || low < 0) return false;
        id[i] = static_cast<std::uint8_t>((high << 4U) | low);
    }
    return true;
}

bool WebSessionManager::equalId(const Session& session,
                                const std::array<std::uint8_t, 16U>& id) {
    std::uint8_t difference = 0U;
    for (std::size_t i = 0U; i < id.size(); ++i)
        difference |= session.id[i] ^ id[i];
    return difference == 0U;
}

WebSessionManager::Session* WebSessionManager::get(WebSessionHandle handle,
                                                   std::uint64_t nowMs) {
    if (handle.slot >= sessions_.size()) return nullptr;
    auto& session = sessions_[handle.slot];
    if (!session.active || session.generation != handle.generation)
        return nullptr;
    if (nowMs < session.createdAtMs ||
        nowMs - session.createdAtMs >= kWebSessionAbsoluteLimitMs ||
        nowMs < session.lastActivityMs ||
        nowMs - session.lastActivityMs >= kWebSessionIdleLimitMs) {
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
    // Expiry is a lifecycle transition, not only a lookup concern. Reclaim
    // abandoned sessions before capacity is evaluated, but never evict a
    // live session merely to make room for a new login.
    for (auto& session : sessions_) {
        if (session.active &&
            (nowMs < session.createdAtMs ||
             nowMs - session.createdAtMs >= kWebSessionAbsoluteLimitMs ||
             nowMs < session.lastActivityMs ||
             nowMs - session.lastActivityMs >= kWebSessionIdleLimitMs)) {
            session.active = false;
        }
    }
    Session* target = nullptr;
    std::size_t slot = 0U;
    for (; slot < sessions_.size(); ++slot) {
        if (!sessions_[slot].active) {
            target = &sessions_[slot];
            break;
        }
    }
    if (target == nullptr)
        return {WebSessionStatus::Capacity, std::nullopt, {}, {}};
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
    return {WebSessionStatus::Created, handle, id,
            hex(target->csrf.data(), target->csrf.size())};
}

WebSessionResult WebSessionManager::find(const std::string& cookie,
                                         std::uint64_t nowMs) {
    std::array<std::uint8_t, 16U> id{};
    if (!decodeCookie(cookie, id))
        return {WebSessionStatus::Missing, std::nullopt, {}, {}};
    std::lock_guard<std::mutex> lock(mutex_);
    for (std::size_t slot = 0U; slot < sessions_.size(); ++slot) {
        auto& session = sessions_[slot];
        if (session.active && equalId(session, id)) {
            const WebSessionHandle handle{slot, session.generation};
            if (get(handle, nowMs) == nullptr)
                return {WebSessionStatus::Expired, std::nullopt, {}, {}};
            session.lastActivityMs = nowMs;
            return {WebSessionStatus::Found, handle,
                    hex(session.id.data(), 16U), hex(session.csrf.data(), 16U)};
        }
    }
    return {WebSessionStatus::Missing, std::nullopt, {}, {}};
}

bool WebSessionManager::validateCsrf(WebSessionHandle handle,
                                     const std::string& token,
                                     std::uint64_t nowMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto* session = get(handle, nowMs);
    return session != nullptr &&
           constantEqual(token, hex(session->csrf.data(), 16U));
}

std::optional<std::string> WebSessionManager::csrfToken(WebSessionHandle handle,
                                                        std::uint64_t nowMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto* session = get(handle, nowMs);
    return session == nullptr
               ? std::nullopt
               : std::optional<std::string>{hex(session->csrf.data(), 16U)};
}

std::optional<std::string> WebSessionManager::cookieValue(
    WebSessionHandle handle, std::uint64_t nowMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto* session = get(handle, nowMs);
    return session == nullptr
               ? std::nullopt
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
    if (handle.slot < sessions_.size() &&
        sessions_[handle.slot].generation == handle.generation)
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
    session->serviceLease =
        device_platform::ServiceSessionLease(servicePolicy_, nowMs);
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

ServiceLeaseView WebSessionManager::serviceLeaseStatus(
    WebSessionHandle handle, std::uint64_t nowMs) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto* session = get(handle, nowMs);
    if (session == nullptr) return {};
    return {session->serviceLease.activeAt(nowMs), std::nullopt};
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
        return {MutationSequenceState::InFlight, 0U,
                session->inFlight->sequence};
    if (session->highWater == std::numeric_limits<std::uint64_t>::max())
        return {MutationSequenceState::Exhausted, 0U, std::nullopt};
    return {MutationSequenceState::Available, session->highWater + 1U,
            std::nullopt};
}

MutationReservation WebSessionManager::reserveMutation(
    WebSessionHandle handle, std::uint64_t nowMs, std::uint64_t sequence,
    const std::string& fingerprint) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto* session = get(handle, nowMs);
    if (session == nullptr)
        return {MutationReservationStatus::InvalidSession, std::nullopt,
                std::nullopt};
    if (fingerprint.empty())
        return {MutationReservationStatus::InvalidFingerprint, std::nullopt,
                std::nullopt};
    if (session->highWater == std::numeric_limits<std::uint64_t>::max())
        return {MutationReservationStatus::Exhausted, std::nullopt,
                std::nullopt};
    if (session->inFlight.has_value()) {
        if (sequence == session->inFlight->sequence) {
            return {constantEqual(fingerprint, session->inFlight->fingerprint)
                        ? MutationReservationStatus::InFlight
                        : MutationReservationStatus::SequenceReused,
                    sequence, std::nullopt};
        }
        return {MutationReservationStatus::SequenceConflict, std::nullopt,
                std::nullopt};
    }
    if (sequence <= session->replayFloor)
        return {MutationReservationStatus::ReplayExpired, sequence,
                std::nullopt};
    for (const auto& completed : session->completed) {
        if (completed.sequence == sequence) {
            if (!constantEqual(completed.fingerprint, fingerprint))
                return {MutationReservationStatus::SequenceReused, sequence,
                        std::nullopt};
            return {MutationReservationStatus::ReplayOutcome, sequence,
                    completed.outcome};
        }
    }
    if (sequence > session->highWater + 1U)
        return {MutationReservationStatus::SequenceGap, sequence, std::nullopt};
    if (sequence <= session->highWater)
        return {MutationReservationStatus::ReplayExpired, sequence,
                std::nullopt};
    session->inFlight = CompletedMutation{sequence, fingerprint, {}};
    return {MutationReservationStatus::Reserved, sequence, std::nullopt};
}

bool WebSessionManager::completeMutation(WebSessionHandle handle,
                                         std::uint64_t nowMs,
                                         std::uint64_t sequence,
                                         const std::string& fingerprint,
                                         const WebMutationOutcome& outcome) {
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
