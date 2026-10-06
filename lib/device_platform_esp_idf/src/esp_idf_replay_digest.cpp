#include "esp_idf_replay_digest.hpp"

#include <algorithm>

#include "psa/crypto.h"

namespace device_platform_esp_idf {
namespace {

bool update(psa_hash_operation_t& operation, std::string_view bytes) {
    if (bytes.empty()) return true;
    return psa_hash_update(&operation,
                           reinterpret_cast<const std::uint8_t*>(bytes.data()),
                           bytes.size()) == PSA_SUCCESS;
}

bool updateNul(psa_hash_operation_t& operation) {
    constexpr std::uint8_t nul = 0U;
    return psa_hash_update(&operation, &nul, 1U) == PSA_SUCCESS;
}

}  // namespace

bool EspIdfSha256ReplayDigest::digest(
    const device_platform::ReplayDigestInput& input,
    device_platform::ReplayDigest& out) {
    out.fill(0U);
    if (psa_crypto_init() != PSA_SUCCESS) return false;

    psa_hash_operation_t operation = PSA_HASH_OPERATION_INIT;
    psa_status_t status = psa_hash_setup(&operation, PSA_ALG_SHA_256);
    if (status == PSA_SUCCESS && !update(operation, input.method))
        status = PSA_ERROR_GENERIC_ERROR;
    if (status == PSA_SUCCESS && !updateNul(operation))
        status = PSA_ERROR_GENERIC_ERROR;
    if (status == PSA_SUCCESS && !update(operation, input.path))
        status = PSA_ERROR_GENERIC_ERROR;
    if (status == PSA_SUCCESS && !updateNul(operation))
        status = PSA_ERROR_GENERIC_ERROR;
    if (status == PSA_SUCCESS && !update(operation, input.bodyLength))
        status = PSA_ERROR_GENERIC_ERROR;
    if (status == PSA_SUCCESS && !updateNul(operation))
        status = PSA_ERROR_GENERIC_ERROR;
    if (status == PSA_SUCCESS && !update(operation, input.body))
        status = PSA_ERROR_GENERIC_ERROR;

    std::size_t outputLength = 0U;
    if (status == PSA_SUCCESS) {
        status =
            psa_hash_finish(&operation, out.data(), out.size(), &outputLength);
    }
    const auto abortStatus = psa_hash_abort(&operation);
    if (status != PSA_SUCCESS || abortStatus != PSA_SUCCESS ||
        outputLength != out.size()) {
        std::fill(out.begin(), out.end(), 0U);
        return false;
    }
    return true;
}

}  // namespace device_platform_esp_idf
