#pragma once
void g1_host_log(const char*, const char*, ...)
    __attribute__((format(printf, 2, 3)));
#define ESP_LOGI(tag, ...) g1_host_log(tag, __VA_ARGS__)
#define ESP_LOGE(tag, ...) g1_host_log(tag, __VA_ARGS__)
