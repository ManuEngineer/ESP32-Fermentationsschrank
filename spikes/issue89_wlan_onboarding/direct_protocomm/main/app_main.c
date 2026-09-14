/*
 * Issue #89 isolated direct protocomm capability probe.  This is not
 * production firmware and intentionally does not implement onboarding.
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "protocomm.h"
#include "protocomm_httpd.h"
#include "protocomm_security0.h"

static const char *TAG = "issue89_direct_pc";

static bool make_volatile_softap_config(wifi_config_t *config)
{
    uint8_t random_bytes[6] = {0};
    esp_fill_random(random_bytes, sizeof(random_bytes));
    int ssid_written = snprintf((char *)config->ap.ssid, sizeof(config->ap.ssid),
                                "R1PC-%02X%02X%02X",
                                random_bytes[3], random_bytes[4], random_bytes[5]);
    int password_written = snprintf((char *)config->ap.password,
                                     sizeof(config->ap.password),
                                     "%02X%02X%02X%02X%02X%02X%02X%02X",
                                     random_bytes[0], random_bytes[1], random_bytes[2],
                                     random_bytes[3], random_bytes[4], random_bytes[5],
                                     (unsigned)esp_random() & 0xFFU,
                                     ((unsigned)esp_random() >> 8U) & 0xFFU);
    config->ap.authmode = WIFI_AUTH_WPA2_PSK;
    config->ap.max_connection = 2;
    config->ap.pmf_cfg.required = true;
    return ssid_written > 0 && (size_t)ssid_written < sizeof(config->ap.ssid) &&
           password_written > 0 &&
           (size_t)password_written < sizeof(config->ap.password);
}

static esp_err_t probe_endpoint_handler(uint32_t session_id,
                                        const uint8_t *inbuf,
                                        ssize_t inlen,
                                        uint8_t **outbuf,
                                        ssize_t *outlen,
                                        void *priv_data)
{
    (void)session_id;
    (void)inbuf;
    (void)inlen;

    const char *endpoint_name = (const char *)priv_data;
    const char response_format[] =
        "{\"endpoint\":\"%s\",\"status\":\"handler-boundary-only\"}";
    int response_len = snprintf(NULL, 0, response_format, endpoint_name);
    if (response_len < 0) {
        return ESP_FAIL;
    }

    *outbuf = malloc((size_t)response_len + 1U);
    if (*outbuf == NULL) {
        return ESP_ERR_NO_MEM;
    }
    (void)snprintf((char *)*outbuf, (size_t)response_len + 1U,
                   response_format, endpoint_name);
    *outlen = response_len;
    return ESP_OK;
}

static void fail_closed_nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed (0x%x); no erase performed; probe stopped", err);
        abort();
    }
}

void app_main(void)
{
    fail_closed_nvs_init();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    wifi_config_t ap_config = {0};
    if (!make_volatile_softap_config(&ap_config)) {
        ESP_LOGE(TAG, "cannot generate volatile protected SoftAP credentials; probe stopped");
        return;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "direct protocomm SoftAP started: service=%s, key=<redacted>",
             ap_config.ap.ssid);

    protocomm_t *pc = protocomm_new();
    if (pc == NULL) {
        ESP_LOGE(TAG, "protocomm allocation failed; probe stopped");
        return;
    }
    protocomm_httpd_config_t http_config = {
        .ext_handle_provided = false,
        .data.config = PROTOCOMM_HTTPD_DEFAULT_CONFIG(),
    };
    ESP_ERROR_CHECK(protocomm_httpd_start(pc, &http_config));
    ESP_ERROR_CHECK(protocomm_set_security(pc, "r1-session",
                                            &protocomm_security0, NULL));
    ESP_ERROR_CHECK(protocomm_set_version(pc, "r1-version",
                                          "issue89-direct-protocomm-probe-1"));
    ESP_ERROR_CHECK(protocomm_add_endpoint(pc, "r1-set",
                                            probe_endpoint_handler, "r1-set"));
    ESP_ERROR_CHECK(protocomm_add_endpoint(pc, "r1-test",
                                            probe_endpoint_handler, "r1-test"));
    ESP_ERROR_CHECK(protocomm_add_endpoint(pc, "r1-commit",
                                            probe_endpoint_handler, "r1-commit"));

    ESP_LOGI(TAG, "public protocomm HTTP endpoints bound: r1-session, r1-version, r1-set, r1-test, r1-commit");
    ESP_LOGI(TAG, "set/test/commit handlers do not parse, apply, persist, or select credentials");
    ESP_LOGI(TAG, "no browser UI, DNS responder, reconnect manager, or candidate selection is included");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
