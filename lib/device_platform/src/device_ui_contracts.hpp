#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace device_platform {
namespace detail {

template <typename Tag>
class DeviceUiId {
   public:
    DeviceUiId() = default;
    explicit DeviceUiId(std::string value) : value_(std::move(value)) {}

    [[nodiscard]] const std::string& value() const noexcept { return value_; }
    [[nodiscard]] bool empty() const noexcept { return value_.empty(); }

    friend bool operator==(const DeviceUiId& left, const DeviceUiId& right) {
        return left.value_ == right.value_;
    }
    friend bool operator!=(const DeviceUiId& left, const DeviceUiId& right) {
        return !(left == right);
    }

   private:
    std::string value_;
};

struct BrandingIdTag {};
struct LocaleIdTag {};
struct ThemeIdTag {};
struct TimeZoneIdTag {};
struct TextNamespaceTag {};

}  // namespace detail

using BrandingId = detail::DeviceUiId<detail::BrandingIdTag>;
using LocaleId = detail::DeviceUiId<detail::LocaleIdTag>;
using ThemeId = detail::DeviceUiId<detail::ThemeIdTag>;
using TimeZoneId = detail::DeviceUiId<detail::TimeZoneIdTag>;
using TextNamespace = detail::DeviceUiId<detail::TextNamespaceTag>;

struct TextKey {
    TextNamespace nameSpace;
    std::string value;

    [[nodiscard]] bool valid() const noexcept {
        return !nameSpace.empty() && !value.empty();
    }
    [[nodiscard]] std::string visibleTechnicalKey() const;

    friend bool operator==(const TextKey& left, const TextKey& right) {
        return left.nameSpace == right.nameSpace && left.value == right.value;
    }
    friend bool operator!=(const TextKey& left, const TextKey& right) {
        return !(left == right);
    }
};

enum class UiSurface : std::uint8_t {
    LocalDisplay,
    WebInterface,
};

struct UiRequestId {
    std::uint64_t value{0U};

    [[nodiscard]] bool valid() const noexcept { return value != 0U; }
    friend bool operator==(UiRequestId left, UiRequestId right) {
        return left.value == right.value;
    }
};

struct UiRefreshRevision {
    std::uint64_t value{0U};

    friend bool operator==(UiRefreshRevision left, UiRefreshRevision right) {
        return left.value == right.value;
    }
};

enum class DeviceUiCommandOutcomeCategory : std::uint8_t {
    Accepted,
    Rejected,
    ConfirmationRequired,
    Busy,
    Unavailable,
};

[[nodiscard]] DeviceUiCommandOutcomeCategory safeOutcomeCategory(
    DeviceUiCommandOutcomeCategory category) noexcept;

// UTC is optional until the existing time source is trusted. The platform
// contract deliberately retains the configured canonical zone rather than
// inventing a local wall-clock value.
struct ClockViewInput {
    std::optional<std::int64_t> trustedUtc;
    TimeZoneId canonicalTimeZoneId;
};

struct DeviceUiBuildCatalog {
    BrandingId activeBranding;
    std::vector<BrandingId> includedBrandings;
    std::vector<LocaleId> includedLocales;
    std::vector<ThemeId> includedThemes;
    LocaleId englishFallback{LocaleId{"en"}};
    ThemeId defaultTheme;

    [[nodiscard]] bool contains(const BrandingId& branding) const;
    [[nodiscard]] bool contains(const LocaleId& locale) const;
    [[nodiscard]] bool contains(const ThemeId& theme) const;
    [[nodiscard]] bool valid() const;
};

}  // namespace device_platform
