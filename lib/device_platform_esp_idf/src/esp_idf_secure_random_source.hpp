#pragma once

#include <cstddef>

#include "secure_random_source.hpp"

namespace device_platform_esp_idf {

class EspIdfSecureRandomSource final
    : public device_platform::ISecureRandomSource {
   public:
    [[nodiscard]] bool fill(void* buffer, std::size_t length) override;
};

}  // namespace device_platform_esp_idf
