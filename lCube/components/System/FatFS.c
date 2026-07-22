#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "panel.h"          // for AMOLED_console_log
#include "FatFS.h"

static const char *TAG = "FatFS";

static wl_handle_t fatfs_wl_handle = WL_INVALID_HANDLE;
static bool fatfs_mounted = false;
static SemaphoreHandle_t fatfs_mutex = NULL;

/* ---- helpers ---- */

static inline void fatfs_lock(void)
{
    if (fatfs_mutex) xSemaphoreTake(fatfs_mutex, portMAX_DELAY);
}

static inline void fatfs_unlock(void)
{
    if (fatfs_mutex) xSemaphoreGive(fatfs_mutex);
}

static bool is_safe_path(const char *path)
{
    if (path == NULL || path[0] == '\0') return false;
    if (path[0] == '/')  return false;          // absolute path not allowed
    if (strstr(path, "..")) return false;      // prevent directory traversal
    return true;
}

#define CHECK_SAFE_PATH(path) \
    do { \
        if (!is_safe_path(path)) { \
            AMOLED_console_log(ERROR, false, TAG, "Unsafe path: %s", (path) ? (path) : "(null)"); \
            return ESP_ERR_INVALID_ARG; \
        } \
    } while (0)

static bool fatfs_make_path(char *out, size_t out_size, const char *path)
{
    int n = snprintf(out, out_size, "%s/%s", FATFS_BASE_PATH, path);
    return (n >= 0 && (size_t)n < out_size);
}

/* ---- public API ---- */

esp_err_t FatFS_init(void)
{
    if (fatfs_mutex == NULL) {
        fatfs_mutex = xSemaphoreCreateMutex();
        if (fatfs_mutex == NULL) {
            AMOLED_console_log(ERROR, false, TAG, "Failed to create mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    fatfs_lock();
    if (fatfs_mounted) {
        AMOLED_console_log(WARN, false, TAG, "Already mounted");
        fatfs_unlock();
        return ESP_OK;
    }

    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 4,
        .allocation_unit_size = 0,
    };

    esp_err_t ret = esp_vfs_fat_spiflash_mount_rw_wl(FATFS_BASE_PATH, FATFS_PART_LABEL,
                                                       &mount_config, &fatfs_wl_handle);
    if (ret != ESP_OK) {
        AMOLED_console_log(ERROR, false, TAG, "Failed to mount (%s)", esp_err_to_name(ret));
        fatfs_unlock();
        return ret;
    }

    fatfs_mounted = true;
    AMOLED_console_log(INFORM, false, TAG, "Mounted at %s", FATFS_BASE_PATH);
    fatfs_unlock();
    return ESP_OK;
}

esp_err_t FatFS_deinit(void)
{
    fatfs_lock();
    if (!fatfs_mounted) {
        AMOLED_console_log(WARN, false, TAG, "Not mounted");
        fatfs_unlock();
        return ESP_OK;
    }

    esp_err_t ret = esp_vfs_fat_spiflash_unmount_rw_wl(FATFS_BASE_PATH, fatfs_wl_handle);
    if (ret != ESP_OK) {
        AMOLED_console_log(ERROR, false, TAG, "Failed to unmount (%s)", esp_err_to_name(ret));
        fatfs_unlock();
        return ret;
    }

    fatfs_wl_handle = WL_INVALID_HANDLE;
    fatfs_mounted = false;
    AMOLED_console_log(INFORM, false, TAG, "Unmounted");
    fatfs_unlock();
    return ESP_OK;
}

bool FatFS_is_mounted(void)
{
    fatfs_lock();
    bool m = fatfs_mounted;
    fatfs_unlock();
    return m;
}

wl_handle_t FatFS_get_wl_handle(void)
{
    fatfs_lock();
    wl_handle_t h = fatfs_wl_handle;
    fatfs_unlock();
    return h;
}

void FatFS_acquire(void)
{
    if (fatfs_mutex) xSemaphoreTake(fatfs_mutex, portMAX_DELAY);
}

void FatFS_release(void)
{
    if (fatfs_mutex) xSemaphoreGive(fatfs_mutex);
}

/* ---- file operations ---- */

esp_err_t FatFS_read_file(const char *path, void *buf, size_t buf_size, size_t *bytes_read)
{
    fatfs_lock();
    if (!fatfs_mounted) {
        fatfs_unlock();
        return ESP_ERR_INVALID_STATE;
    }

    CHECK_SAFE_PATH(path);

    if (buf == NULL || buf_size == 0) {
        fatfs_unlock();
        return ESP_ERR_INVALID_ARG;
    }

    char full_path[256];
    if (!fatfs_make_path(full_path, sizeof(full_path), path)) {
        AMOLED_console_log(ERROR, false, TAG, "Path too long");
        fatfs_unlock();
        return ESP_ERR_INVALID_ARG;
    }

    FILE *f = fopen(full_path, "rb");
    if (f == NULL) {
        AMOLED_console_log(ERROR, false, TAG, "Failed to open %s", full_path);
        fatfs_unlock();
        return ESP_ERR_NOT_FOUND;
    }

    size_t n = fread(buf, 1, buf_size, f);      // 移除多余的 null 终止
    if (bytes_read) *bytes_read = n;

    fclose(f);
    fatfs_unlock();
    return ESP_OK;
}

esp_err_t FatFS_write_file(const char *path, const void *data, size_t len, bool append)
{
    fatfs_lock();
    if (!fatfs_mounted) {
        fatfs_unlock();
        return ESP_ERR_INVALID_STATE;
    }

    CHECK_SAFE_PATH(path);

    if (data == NULL) {
        fatfs_unlock();
        return ESP_ERR_INVALID_ARG;
    }

    char full_path[256];
    if (!fatfs_make_path(full_path, sizeof(full_path), path)) {
        AMOLED_console_log(ERROR, false, TAG, "Path too long");
        fatfs_unlock();
        return ESP_ERR_INVALID_ARG;
    }

    if (append) {
        FILE *f = fopen(full_path, "ab");
        if (f == NULL) {
            AMOLED_console_log(ERROR, false, TAG, "Failed to open %s for append", full_path);
            fatfs_unlock();
            return ESP_FAIL;
        }
        size_t written = fwrite(data, 1, len, f);
        if (written != len) {
            fclose(f);
            AMOLED_console_log(ERROR, false, TAG, "Write incomplete (%zu/%zu)", written, len);
            fatfs_unlock();
            return ESP_FAIL;
        }
        fsync(fileno(f));
        fclose(f);
        fatfs_unlock();
        return ESP_OK;
    }

    /* Atomic write using temporary file */
    char tmp_path[260];
    int n = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", full_path);
    if (n < 0 || (size_t)n >= sizeof(tmp_path)) {
        fatfs_unlock();
        return ESP_ERR_INVALID_ARG;
    }

    FILE *f = fopen(tmp_path, "wb");
    if (f == NULL) {
        AMOLED_console_log(ERROR, false, TAG, "Failed to create %s", tmp_path);
        fatfs_unlock();
        return ESP_FAIL;
    }

    size_t written = fwrite(data, 1, len, f);
    if (written != len) {
        fclose(f);
        unlink(tmp_path);
        AMOLED_console_log(ERROR, false, TAG, "Write incomplete (%zu/%zu)", written, len);
        fatfs_unlock();
        return ESP_FAIL;
    }

    if (fsync(fileno(f)) != 0) {
        fclose(f);
        unlink(tmp_path);
        AMOLED_console_log(ERROR, false, TAG, "fsync failed for %s", tmp_path);
        fatfs_unlock();
        return ESP_FAIL;
    }
    fclose(f);

    if (rename(tmp_path, full_path) != 0) {
        unlink(tmp_path);
        AMOLED_console_log(ERROR, false, TAG, "rename failed %s->%s", tmp_path, full_path);
        fatfs_unlock();
        return ESP_FAIL;
    }

    fatfs_unlock();
    return ESP_OK;
}

esp_err_t FatFS_delete_file(const char *path)
{
    fatfs_lock();
    if (!fatfs_mounted) {
        fatfs_unlock();
        return ESP_ERR_INVALID_STATE;
    }

    CHECK_SAFE_PATH(path);

    char full_path[256];
    if (!fatfs_make_path(full_path, sizeof(full_path), path)) {
        AMOLED_console_log(ERROR, false, TAG, "Path too long");
        fatfs_unlock();
        return ESP_ERR_INVALID_ARG;
    }

    if (unlink(full_path) != 0) {
        AMOLED_console_log(ERROR, false, TAG, "Failed to delete %s", full_path);
        fatfs_unlock();
        return ESP_FAIL;
    }

    fatfs_unlock();
    return ESP_OK;
}

bool FatFS_file_exists(const char *path)
{
    fatfs_lock();
    if (!fatfs_mounted) {
        fatfs_unlock();
        return false;
    }

    CHECK_SAFE_PATH(path);   // 注意：此宏内有 return，但返回类型为 bool 时需要处理

    char full_path[256];
    if (!fatfs_make_path(full_path, sizeof(full_path), path)) {
        AMOLED_console_log(ERROR, false, TAG, "Path too long");
        fatfs_unlock();
        return false;
    }

    struct stat st;
    bool exists = (stat(full_path, &st) == 0);
    fatfs_unlock();
    return exists;
}

esp_err_t FatFS_file_size_get(const char *path, size_t *size)
{
    fatfs_lock();
    if (!fatfs_mounted) {
        fatfs_unlock();
        return ESP_ERR_INVALID_STATE;
    }

    CHECK_SAFE_PATH(path);

    if (size == NULL) {
        fatfs_unlock();
        return ESP_ERR_INVALID_ARG;
    }

    char full_path[256];
    if (!fatfs_make_path(full_path, sizeof(full_path), path)) {
        AMOLED_console_log(ERROR, false, TAG, "Path too long");
        fatfs_unlock();
        return ESP_ERR_INVALID_ARG;
    }

    struct stat st;
    if (stat(full_path, &st) != 0) {
        AMOLED_console_log(ERROR, false, TAG, "stat failed for %s", full_path);
        fatfs_unlock();
        return ESP_ERR_NOT_FOUND;
    }

    *size = st.st_size;
    fatfs_unlock();
    return ESP_OK;
}

esp_err_t FatFS_info(uint64_t *total_bytes, uint64_t *free_bytes)
{
    fatfs_lock();
    if (!fatfs_mounted) {
        fatfs_unlock();
        return ESP_ERR_INVALID_STATE;
    }

    uint64_t dummy;
    esp_err_t ret = esp_vfs_fat_info(FATFS_BASE_PATH,
                                     total_bytes ? total_bytes : &dummy,
                                     free_bytes  ? free_bytes  : &dummy);
    fatfs_unlock();
    return ret;
}