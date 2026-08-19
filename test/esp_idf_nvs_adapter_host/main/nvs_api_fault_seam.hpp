#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "esp_err.h"

namespace issue90_host {

class NvsApiFaultSeam final {
   public:
    static void reset();
    static void failOpen(esp_err_t error);
    static void failSizeQuery(esp_err_t error);
    static void failRead(
        esp_err_t error,
        std::optional<std::size_t> reportedLength = std::nullopt);
    static void failSet(esp_err_t error, bool afterRealMutation = false);
    static void failCommit(esp_err_t error);
    static void raceAfterSizeQuery(std::string replacement);
    [[nodiscard]] static std::size_t openCalls();
};

}  // namespace issue90_host
