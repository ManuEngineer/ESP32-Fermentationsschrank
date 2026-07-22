#pragma once

#include "network_status.hpp"

namespace device_platform_test_support {

// Deterministisch steuerbarer Mock des Netzwerkverbindungsstatus fuer native
// Tests (z. B. WLAN-Ausfall bei weiterlaufendem sicheren Prozess).
class MockNetworkStatus final : public device_platform::INetworkStatus {
   public:
    [[nodiscard]] bool isConnected() const override;

    void setConnected(bool connected);

   private:
    bool connected_{false};
};

}  // namespace device_platform_test_support
