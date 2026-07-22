#include <stdio.h>
#include "esp_log.h"
#include "tinyusb.h"
#include "tinyusb_msc.h"
#include "wear_levelling.h"
#include "FatFS.h"

static const char *TAG = "USB_MSC";

static bool s_initialized = false;
static tinyusb_msc_storage_handle_t s_storage_handle = NULL;

esp_err_t usb_msc_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    wl_handle_t wl_handle = FatFS_get_wl_handle();
    if (wl_handle == WL_INVALID_HANDLE) {
        ESP_LOGE(TAG, "Invalid wl_handle, make sure FatFS is mounted first");
        return ESP_ERR_INVALID_STATE;
    }

    // ---- 1. Install TinyUSB device driver (once) ----
    static bool tusb_installed = false;
    if (!tusb_installed) {
        tinyusb_config_t tusb_cfg = {
            .port = TINYUSB_PORT_FULL_SPEED_0,
            .phy = {
                .skip_setup = false,
                .self_powered = false,
                .vbus_monitor_io = -1,
            },
            .task = {
                .size = 4096,
                .priority = 5,
                .xCoreID = -1,
            },
            .descriptor = {
                .device = NULL,
                .qualifier = NULL,
                .string = NULL,
                .string_count = 0,
                .full_speed_config = NULL,
                .high_speed_config = NULL,
            },
            .event_cb = NULL,
            .event_arg = NULL,
        };
        esp_err_t ret = tinyusb_driver_install(&tusb_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "tinyusb_driver_install failed: %s", esp_err_to_name(ret));
            return ret;
        }
        tusb_installed = true;
    }

    // ---- 2. Install MSC driver (once) ----
    static bool msc_driver_installed = false;
    if (!msc_driver_installed) {
        tinyusb_msc_driver_config_t msc_cfg = {
            .user_flags.val = 0,
            .callback = NULL,
            .callback_arg = NULL,
        };
        esp_err_t ret = tinyusb_msc_install_driver(&msc_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "tinyusb_msc_install_driver failed: %s", esp_err_to_name(ret));
            return ret;
        }
        msc_driver_installed = true;
    }

    // ---- 3. Create SPI-flash-backed storage ----
    tinyusb_msc_storage_config_t storage_cfg = {
        .medium.wl_handle = wl_handle,
        .fat_fs = {
            .base_path = "/storage",
            .do_not_format = false,
            .format_flags = 0,
            .config = {
                .max_files = 2,
                .format_if_mount_failed = true,
            },
        },
        .mount_point = TINYUSB_MSC_STORAGE_MOUNT_USB,
    };

    esp_err_t ret = tinyusb_msc_new_storage_spiflash(&storage_cfg, &s_storage_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create storage: %s", esp_err_to_name(ret));
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "USB MSC initialized and exposed to host");
    return ESP_OK;
}

esp_err_t usb_msc_deinit(void)
{
    if (!s_initialized) return ESP_OK;

    esp_err_t ret = tinyusb_msc_delete_storage(s_storage_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete storage: %s", esp_err_to_name(ret));
        return ret;
    }
    s_storage_handle = NULL;
    s_initialized = false;
    ESP_LOGI(TAG, "USB MSC deinitialized");
    return ESP_OK;
}

bool usb_msc_is_connected(void)
{
    if (!s_initialized || s_storage_handle == NULL) {
        return false;
    }
    tinyusb_msc_mount_point_t mp;
    if (tinyusb_msc_get_storage_mount_point(s_storage_handle, &mp) == ESP_OK) {
        return (mp == TINYUSB_MSC_STORAGE_MOUNT_USB);
    }
    return false;
}
