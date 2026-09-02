#include "nvs_state_store.hpp"

extern "C" void app_main() {
    // Compile/link contract only. Owning-context values are deliberately not
    // supplied by this generic consumer fixture.
    const auto config = device_platform_esp_idf::NvsStateStoreConfig::create(
        "consumer_partition", "consumer_namespace");
    if (config.has_value()) {
        (void)config->partitionLabel();
        (void)config->namespaceName();
    }
}
