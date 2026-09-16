/*
 * Issue #89 isolated native capability probe.  This is not production code.
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
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

static const char *TAG = "issue89_native";

static void fail_closed_nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed (0x%x); no erase performed; probe stopped", err);
        abort();
    }
}

static bool make_volatile_softap_config(wifi_config_t *config)
{
    uint8_t random_bytes[6] = {0};
    esp_fill_random(random_bytes, sizeof(random_bytes));
    int ssid_written = snprintf((char *)config->ap.ssid, sizeof(config->ap.ssid),
                                "R1NAT-%02X%02X%02X",
                                random_bytes[3], random_bytes[4], random_bytes[5]);
    int password_written = snprintf((char *)config->ap.password, sizeof(config->ap.password),
                                     "%02X%02X%02X%02X%02X%02X%02X%02X",
                                     random_bytes[0], random_bytes[1], random_bytes[2],
                                     random_bytes[3], random_bytes[4], random_bytes[5],
                                     (unsigned)esp_random() & 0xFFU,
                                     ((unsigned)esp_random() >> 8U) & 0xFFU);
    config->ap.authmode = WIFI_AUTH_WPA2_PSK;
    config->ap.max_connection = 2;
    config->ap.pmf_cfg.required = true;
    return ssid_written > 0 && (size_t)ssid_written < sizeof(config->ap.ssid) &&
           password_written > 0 && (size_t)password_written < sizeof(config->ap.password);
}

static esp_err_t setup_page(httpd_req_t *request)
{
    static const char page[] =
        "<!doctype html><meta charset=utf-8>"
        "<title>Issue 89 isolated probe</title>"
        "<h1>Direct local setup transport</h1>"
        "<p>This page proves only the isolated ESP-IDF HTTP path.</p>"
        "<p>No credential is stored, applied, or logged by this probe.</p>";
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, page, HTTPD_RESP_USE_STRLEN);
}

static httpd_handle_t start_http_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 4;
    config.lru_purge_enable = true;
    httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(httpd_start(&server, &config));
    const httpd_uri_t setup_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = setup_page,
        .user_ctx = NULL,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &setup_uri));
    return server;
}

void app_main(void)
{
    fail_closed_nvs_init();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    (void)ap_netif;

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    wifi_config_t ap_config = {0};
    if (!make_volatile_softap_config(&ap_config)) {
        ESP_LOGE(TAG, "cannot generate volatile protected SoftAP credentials; probe stopped");
        return;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "native SoftAP started: service=%s, key=<redacted>", ap_config.ap.ssid);

    (void)start_http_server();
    ESP_LOGI(TAG, "direct-IP HTTP page is available at the SoftAP address");
    ESP_LOGI(TAG, "DNS/captive portal, scan, reconnect, and credential commit remain unimplemented in this pre-owner probe");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
