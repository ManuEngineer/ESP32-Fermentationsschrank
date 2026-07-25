#include "crc32.hpp"

#include <array>

namespace device_platform {

namespace {

const std::array<uint32_t, 256>& crcTable() {
    static const std::array<uint32_t, 256> table = [] {
        std::array<uint32_t, 256> generated{};
        for (uint32_t index = 0U; index < 256U; ++index) {
            uint32_t crc = index;
            for (int bit = 0; bit < 8; ++bit) {
                crc = (crc & 1U) != 0U ? (0xEDB88320U ^ (crc >> 1U))
                                       : (crc >> 1U);
            }
            generated[index] = crc;
        }
        return generated;
    }();
    return table;
}

}  // namespace

bool Crc32IsoHdlc::update(const void* data, std::size_t length) {
    if (length == 0U) {
        return true;
    }
    if (data == nullptr) {
        return false;
    }
    const auto* bytes = static_cast<const uint8_t*>(data);
    const auto& table = crcTable();
    uint32_t crc = crc_;
    for (std::size_t index = 0U; index < length; ++index) {
        const auto tableIndex =
            static_cast<uint8_t>((crc ^ bytes[index]) & 0xFFU);
        crc = table[tableIndex] ^ (crc >> 8U);
    }
    crc_ = crc;
    return true;
}

bool Crc32IsoHdlc::update(const std::string& data) {
    return update(data.data(), data.size());
}

uint32_t Crc32IsoHdlc::finalize() const { return crc_ ^ 0xFFFFFFFFU; }

uint32_t computeCrc32IsoHdlc(const std::string& data) {
    Crc32IsoHdlc accumulator;
    // `std::string::data()` ist nie nullptr, dieser Aufruf kann daher nie
    // fehlschlagen.
    static_cast<void>(accumulator.update(data));
    return accumulator.finalize();
}

}  // namespace device_platform
