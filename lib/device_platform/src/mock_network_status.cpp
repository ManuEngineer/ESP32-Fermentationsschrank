#include "mock_network_status.hpp"

namespace device_platform {

bool MockNetworkStatus::isConnected() const { return connected_; }

void MockNetworkStatus::setConnected(bool connected) { connected_ = connected; }

}  // namespace device_platform
