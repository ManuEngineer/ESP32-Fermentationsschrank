#include "esp_idf_authentication_kdf.hpp"

#include <algorithm>

#include "psa/crypto.h"

namespace device_platform_esp_idf {

bool EspIdfPbkdf2HmacSha256::derive(
    const std::string& secret, const fermentation::AuthVerifier& parameters,
    std::array<std::uint8_t, fermentation::kAuthenticationVerifierBytes>& out) {
    out.fill(0U);
    if (secret.empty() || secret.size() > 256U ||
        parameters.algorithmId !=
            fermentation::kAuthenticationPbkdf2Sha256Algorithm ||
        parameters.workFactor !=
            fermentation::kAuthenticationPbkdf2Sha256WorkFactor) {
        return false;
    }
    if (psa_crypto_init() != PSA_SUCCESS) return false;

    psa_key_derivation_operation_t operation =
        PSA_KEY_DERIVATION_OPERATION_INIT;
    const auto algorithm = PSA_ALG_PBKDF2_HMAC(PSA_ALG_SHA_256);
    psa_status_t status = psa_key_derivation_setup(&operation, algorithm);
    if (status == PSA_SUCCESS) {
        status = psa_key_derivation_input_integer(
            &operation, PSA_KEY_DERIVATION_INPUT_COST, parameters.workFactor);
    }
    if (status == PSA_SUCCESS) {
        status = psa_key_derivation_input_bytes(
            &operation, PSA_KEY_DERIVATION_INPUT_SALT, parameters.salt.data(),
            parameters.salt.size());
    }
    if (status == PSA_SUCCESS) {
        status = psa_key_derivation_input_bytes(
            &operation, PSA_KEY_DERIVATION_INPUT_PASSWORD,
            reinterpret_cast<const std::uint8_t*>(secret.data()),
            secret.size());
    }
    if (status == PSA_SUCCESS) {
        status =
            psa_key_derivation_output_bytes(&operation, out.data(), out.size());
    }
    const auto abortStatus = psa_key_derivation_abort(&operation);
    if (status != PSA_SUCCESS || abortStatus != PSA_SUCCESS) {
        std::fill(out.begin(), out.end(), 0U);
        return false;
    }
    return true;
}

}  // namespace device_platform_esp_idf
