#include "mock_network_status.hpp"

namespace device_platform_test_support {

bool MockNetworkStatus::isConnected() const { return connected_; }

void MockNetworkStatus::setConnected(bool connected) { connected_ = connected; }

}  // namespace device_platform_test_support
