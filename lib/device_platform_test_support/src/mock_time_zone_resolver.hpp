#pragma once

#include <set>
#include <string>

#include "time_zone_resolver.hpp"

namespace device_platform_test_support {

// Deterministisch steuerbarer Zeitzonen-Resolver fuer native Tests: eine
// feste Menge "bekannter" IDs loest erfolgreich auf, alle anderen liefern
// `Unknown`; ein injizierbarer Fehler simuliert einen technischen
// Vorbereitungsfehler unabhaengig von der Bekanntheit der ID.
class MockTimeZoneResolver final : public device_platform::ITimeZoneResolver {
   public:
    [[nodiscard]] device_platform::TimeZoneResolutionStatus prepare(
        const std::string& ianaId) override;

    void addKnownZone(const std::string& ianaId);
    void injectFailure(bool shouldFail);

   private:
    std::set<std::string> knownZones_;
    bool shouldFail_{false};
};

}  // namespace device_platform_test_support
