
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include "esp_vfs_fat.h"

#include "panel.h"
#include "FatFS.h"

static const char *TAG = "FatFS";

static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
static bool s_mounted = false;

static bool is_safe_path(const char *path)
{
    if (path == NULL || path[0] == '\0') return false;
    if (path[0] == '/')  return false;
    if (strstr(path, "..")) return false;
    return true;
}

#define CHECK_SAFE_PATH(path)                              \
    do {                                                   \
        if (!is_safe_path(path)) {                         \
            AMOLED_console_log(ERROR, false, TAG,          \
                "Unsafe path: %s", (path) ? (path) : "(null)"); \
            return ESP_ERR_INVALID_ARG;                    \
        }                                                  \
    } while (0)

esp_err_t FatFS_init(void)
{
    if (s_mounted) {
        AMOLED_console_log(WARN, false, TAG, "Already mounted");
        return ESP_OK;
    }

    const esp_vfs_fat_mount_config_t fat_mount_config = {
        .format_if_mount_failed = true,
        .max_files = 4,
        .allocation_unit_size = 0,
    };

    esp_err_t ret = esp_vfs_fat_spiflash_mount_rw_wl(FATFS_BASE_PATH, FATFS_PART_LABEL,
                                                       &fat_mount_config, &s_wl_handle);
    if (ret != ESP_OK) {
        AMOLED_console_log(ERROR, false, TAG, "Failed to mount (%s)", esp_err_to_name(ret));
        return ret;
    }

    s_mounted = true;
    AMOLED_console_log(INFORM, false, TAG, "Mounted at %s", FATFS_BASE_PATH);
    return ESP_OK;
}

esp_err_t FatFS_deinit(void)
{
    if (!s_mounted) {
        AMOLED_console_log(WARN, false, TAG, "Not mounted");
        return ESP_OK;
    }

    esp_err_t ret = esp_vfs_fat_spiflash_unmount_rw_wl(FATFS_BASE_PATH, s_wl_handle);
    if (ret != ESP_OK) {
        AMOLED_console_log(ERROR, false, TAG, "Failed to unmount (%s)", esp_err_to_name(ret));
        return ret;
    }

    s_wl_handle = WL_INVALID_HANDLE;
    s_mounted = false;
    AMOLED_console_log(INFORM, false, TAG, "Unmounted");
    return ESP_OK;
}

esp_err_t FatFS_read_file(const char *path, void *buf, size_t buf_size, size_t *bytes_read)
{
    if (!s_mounted) {
        AMOLED_console_log(ERROR, false, TAG, "Not mounted, Failed to open the file");
        return ESP_ERR_INVALID_STATE;
    }

    CHECK_SAFE_PATH(path);

    if (buf == NULL || buf_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    char full_path[256];
    int _n = snprintf(full_path, sizeof(full_path), "%s/%s", FATFS_BASE_PATH, path);
    if (_n < 0 || (size_t)_n >= sizeof(full_path)) {
        AMOLED_console_log(ERROR, false, TAG, "Path too long (need %d)", _n);
        return ESP_ERR_INVALID_ARG;
    }

    FILE *f = fopen(full_path, "rb");
    if (f == NULL) {
        AMOLED_console_log(ERROR, false, TAG, "Failed to read %s", full_path);
        return ESP_ERR_NOT_FOUND;
    }

    size_t n = fread(buf, 1, buf_size - 1, f);
    ((char *)buf)[n] = '\0';

    if (bytes_read) {
        *bytes_read = n;
    }

    fclose(f);
    return ESP_OK;
}

esp_err_t FatFS_write_file(const char *path, const void *data, size_t len, bool append)
{
    if (!s_mounted) {
        AMOLED_console_log(ERROR, false, TAG, "Not mounted, Failed to open the file");
        return ESP_ERR_INVALID_STATE;
    }

    CHECK_SAFE_PATH(path);

    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char full_path[256];
    int _n = snprintf(full_path, sizeof(full_path), "%s/%s", FATFS_BASE_PATH, path);
    if (_n < 0 || (size_t)_n >= sizeof(full_path)) {
        AMOLED_console_log(ERROR, false, TAG, "Path too long (need %d)", _n);
        return ESP_ERR_INVALID_ARG;
    }

    if (append) {
        FILE *f = fopen(full_path, "ab");
        if (f == NULL) {
            AMOLED_console_log(ERROR, false, TAG, "Failed to write %s", full_path);
            return ESP_FAIL;
        }

        size_t written = fwrite(data, 1, len, f);
        if (written != len) {
            fclose(f);
            AMOLED_console_log(ERROR, false, TAG, "Write incomplete (%zu/%zu)", written, len);
            return ESP_FAIL;
        }

        fsync(fileno(f));
        fclose(f);
        return ESP_OK;
    }

    char tmp_path[260];
    _n = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", full_path);
    if (_n < 0 || (size_t)_n >= sizeof(tmp_path)) {
        return ESP_ERR_INVALID_ARG;
    }

    FILE *f = fopen(tmp_path, "wb");
    if (f == NULL) {
        AMOLED_console_log(ERROR, false, TAG, "Failed to open %s", tmp_path);
        return ESP_FAIL;
    }

    size_t written = fwrite(data, 1, len, f);
    if (written != len) {
        fclose(f);
        unlink(tmp_path);
        AMOLED_console_log(ERROR, false, TAG, "Write incomplete (%zu/%zu)", written, len);
        return ESP_FAIL;
    }

    if (fsync(fileno(f)) != 0) {
        fclose(f);
        unlink(tmp_path);
        AMOLED_console_log(ERROR, false, TAG, "fsync failed for %s", tmp_path);
        return ESP_FAIL;
    }
    fclose(f);

    if (rename(tmp_path, full_path) != 0) {
        unlink(tmp_path);
        AMOLED_console_log(ERROR, false, TAG, "rename failed %s->%s", tmp_path, full_path);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t FatFS_delete_file(const char *path)
{
    if (!s_mounted) {
        AMOLED_console_log(ERROR, false, TAG, "Not mounted, Failed to delete the file");
        return ESP_ERR_INVALID_STATE;
    }

    CHECK_SAFE_PATH(path);

    char full_path[256];
    int _n = snprintf(full_path, sizeof(full_path), "%s/%s", FATFS_BASE_PATH, path);
    if (_n < 0 || (size_t)_n >= sizeof(full_path)) {
        AMOLED_console_log(ERROR, false, TAG, "Path too long (need %d)", _n);
        return ESP_ERR_INVALID_ARG;
    }

    if (unlink(full_path) != 0) {
        AMOLED_console_log(ERROR, false, TAG, "Failed to delete %s", full_path);
        return ESP_FAIL;
    }

    return ESP_OK;
}

bool FatFS_file_exists(const char *path)
{
    if (!s_mounted) {
        AMOLED_console_log(ERROR, false, TAG, "Not mounted");
        return false;
    }

    CHECK_SAFE_PATH(path);

    char full_path[256];
    int _n = snprintf(full_path, sizeof(full_path), "%s/%s", FATFS_BASE_PATH, path);
    if (_n < 0 || (size_t)_n >= sizeof(full_path)) {
        AMOLED_console_log(ERROR, false, TAG, "Path too long (need %d)", _n);
        return false;
    }

    struct stat st;
    return (stat(full_path, &st) == 0);
}

esp_err_t FatFS_file_size_get(const char *path, size_t *size)
{
    if (!s_mounted) {
        AMOLED_console_log(ERROR, false, TAG, "Not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    CHECK_SAFE_PATH(path);

    if (size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char full_path[256];
    int _n = snprintf(full_path, sizeof(full_path), "%s/%s", FATFS_BASE_PATH, path);
    if (_n < 0 || (size_t)_n >= sizeof(full_path)) {
        AMOLED_console_log(ERROR, false, TAG, "Path too long (need %d)", _n);
        return ESP_ERR_INVALID_ARG;
    }

    struct stat st;
    if (stat(full_path, &st) != 0) {
        AMOLED_console_log(ERROR, false, TAG, "stat failed for %s", full_path);
        return ESP_ERR_NOT_FOUND;
    }

    *size = st.st_size;
    return ESP_OK;
}

esp_err_t FatFS_info(uint64_t *total_bytes, uint64_t *free_bytes)
{
    if (!s_mounted) {
        AMOLED_console_log(ERROR, false, TAG, "Not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    uint64_t dummy;
    return esp_vfs_fat_info(FATFS_BASE_PATH,
                            total_bytes  ? total_bytes  : &dummy,
                            free_bytes   ? free_bytes   : &dummy);
}
