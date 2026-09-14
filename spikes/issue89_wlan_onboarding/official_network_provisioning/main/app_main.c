/*
 * Issue #89 isolated candidate probe.  This is not production firmware.
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "network_provisioning/manager.h"
#include "network_provisioning/scheme_softap.h"

static const char *TAG = "issue89_official";

static void provisioning_events(void *user_data,
                                network_prov_cb_event_t event,
                                void *event_data)
{
    (void)user_data;
    (void)event_data;
    switch (event) {
    case NETWORK_PROV_START:
        ESP_LOGI(TAG, "official SoftAP/protocomm manager started; browser contract still unproven");
        break;
    case NETWORK_PROV_WIFI_CRED_RECV:
        ESP_LOGI(TAG, "candidate credentials received (values redacted)");
        break;
    case NETWORK_PROV_WIFI_CRED_FAIL:
        ESP_LOGW(TAG, "candidate connection failed (credentials redacted)");
        break;
    case NETWORK_PROV_WIFI_CRED_SUCCESS:
        ESP_LOGI(TAG, "candidate connection succeeded; no project commit performed");
        break;
    case NETWORK_PROV_END:
        ESP_LOGI(TAG, "official manager ended");
        break;
    default:
        break;
    }
}

static bool make_volatile_softap_credentials(char *name,
                                             size_t name_size,
                                             char *key,
                                             size_t key_size)
{
    uint8_t random_bytes[6] = {0};
    esp_fill_random(random_bytes, sizeof(random_bytes));
    int name_written = snprintf(name, name_size, "R1SPK-%02X%02X%02X",
                                random_bytes[3], random_bytes[4], random_bytes[5]);
    int key_written = snprintf(key, key_size, "%02X%02X%02X%02X%02X%02X%02X%02X",
                               random_bytes[0], random_bytes[1], random_bytes[2],
                               random_bytes[3], random_bytes[4], random_bytes[5],
                               (unsigned)esp_random() & 0xFFU,
                               ((unsigned)esp_random() >> 8U) & 0xFFU);
    return name_written > 0 && (size_t)name_written < name_size && key_written > 0 &&
           (size_t)key_written < key_size;
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed (0x%x); no erase performed; probe stopped", err);
        return;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));
    network_prov_mgr_config_t manager_config = {
        .scheme = network_prov_scheme_softap,
        .scheme_event_handler = NETWORK_PROV_EVENT_HANDLER_NONE,
        .app_event_handler = {
            .event_cb = provisioning_events,
            .user_data = NULL,
        },
        .network_prov_wifi_conn_cfg = {
            .wifi_conn_attempts = 1,
        },
    };
    ESP_ERROR_CHECK(network_prov_mgr_init(manager_config));

    bool already_provisioned = false;
    ESP_ERROR_CHECK(network_prov_mgr_is_wifi_provisioned(&already_provisioned));
    if (already_provisioned) {
        ESP_LOGI(TAG, "native Wi-Fi state is already provisioned; no reset or replacement performed");
        ESP_LOGI(TAG, "probe idle; production credential ownership remains an Owner decision");
        return;
    }

    char service_name[16] = {0};
    char service_key[17] = {0};
    if (!make_volatile_softap_credentials(service_name, sizeof(service_name),
                                           service_key, sizeof(service_key))) {
        ESP_LOGE(TAG, "cannot generate volatile protected SoftAP credentials; probe stopped");
        return;
    }

    ESP_LOGI(TAG, "starting official SoftAP/protocomm candidate: service=%s, key=<redacted>", service_name);
    ESP_LOGI(TAG, "standard component starts its provisioning transport here; no browser UI or project commit is added");
    ESP_ERROR_CHECK(network_prov_mgr_start_provisioning(NETWORK_PROV_SECURITY_0,
                                                         NULL,
                                                         service_name,
                                                         service_key));

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
