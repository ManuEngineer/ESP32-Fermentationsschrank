#include "esp_idf_secure_random_source.hpp"

#include "esp_random.h"

namespace device_platform_esp_idf {

bool EspIdfSecureRandomSource::fill(void* buffer, std::size_t length) {
    if (length == 0U) {
        return true;
    }
    if (buffer == nullptr) {
        return false;
    }
    esp_fill_random(buffer, length);
    return true;
}

}  // namespace device_platform_esp_idf
