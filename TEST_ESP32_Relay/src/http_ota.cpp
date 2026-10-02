#include "http_ota.h"

#include <stdio.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "http_ui.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ota";
static constexpr size_t kMaxImage = 0x200000;
static volatile bool s_busy;

void HttpOta::confirm_running() {
    (void)esp_ota_mark_app_valid_cancel_rollback();
}

void HttpOta::print_status() {
    const esp_partition_t *run = esp_ota_get_running_partition();
    const esp_app_desc_t *desc = esp_app_get_description();
    printf("ota partition=%s ver=%s\n", run && run->label ? run->label : "?",
           desc && desc->version ? desc->version : "?");
}

esp_err_t HttpOta::get_status(httpd_req_t *req) {
    const esp_partition_t *run = esp_ota_get_running_partition();
    const esp_partition_t *next = esp_ota_get_next_update_partition(nullptr);
    const esp_app_desc_t *desc = esp_app_get_description();
    char buf[192];
    snprintf(buf, sizeof(buf),
             "{\"running\":\"%s\",\"next\":\"%s\",\"version\":\"%s\"}",
             run && run->label ? run->label : "?", next && next->label ? next->label : "?",
             desc && desc->version ? desc->version : "?");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}

esp_err_t HttpOta::post_update(httpd_req_t *req) {
    if (s_busy) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "busy");
        return ESP_FAIL;
    }
    const size_t total = req->content_len;
    if (total < 1024 || total > kMaxImage) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "size");
        return ESP_FAIL;
    }

    const esp_partition_t *part = esp_ota_get_next_update_partition(nullptr);
    if (!part) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no ota partition");
        return ESP_FAIL;
    }

    s_busy = true;
    ESP_LOGI(TAG, "begin %u bytes -> %s", static_cast<unsigned>(total), part->label);

    esp_ota_handle_t handle = 0;
    esp_err_t err = esp_ota_begin(part, total, &handle);
    if (err != ESP_OK) {
        s_busy = false;
        ESP_LOGE(TAG, "esp_ota_begin %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, esp_err_to_name(err));
        return ESP_FAIL;
    }

    char chunk[1024];
    size_t got = 0;
    while (got < total) {
        const size_t want = total - got;
        const int n = httpd_req_recv(req, chunk, want > sizeof(chunk) ? sizeof(chunk) : want);
        if (n <= 0) {
            (void)esp_ota_abort(handle);
            s_busy = false;
            ESP_LOGE(TAG, "recv failed at %u/%u", static_cast<unsigned>(got),
                     static_cast<unsigned>(total));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv");
            return ESP_FAIL;
        }
        err = esp_ota_write(handle, chunk, static_cast<size_t>(n));
        if (err != ESP_OK) {
            (void)esp_ota_abort(handle);
            s_busy = false;
            ESP_LOGE(TAG, "esp_ota_write %s", esp_err_to_name(err));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, esp_err_to_name(err));
            return ESP_FAIL;
        }
        got += static_cast<size_t>(n);
        if (req->user_ctx) {
            static_cast<HttpUi *>(req->user_ctx)->note_rx();
        }
    }

    err = esp_ota_end(handle);
    if (err != ESP_OK) {
        s_busy = false;
        ESP_LOGE(TAG, "esp_ota_end %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, esp_err_to_name(err));
        return ESP_FAIL;
    }
    err = esp_ota_set_boot_partition(part);
    if (err != ESP_OK) {
        s_busy = false;
        ESP_LOGE(TAG, "esp_ota_set_boot_partition %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, esp_err_to_name(err));
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "ok, reboot to %s", part->label);
    httpd_resp_sendstr(req, "OK, rebooting\n");
    vTaskDelay(pdMS_TO_TICKS(300));
    esp_restart();
    return ESP_OK;
}
