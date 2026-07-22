#ifndef FAT_FS_H_
#define FAT_FS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#define FATFS_BASE_PATH  "/storage"
#define FATFS_PART_LABEL "storage"

/* ---- Initialization ---- */
esp_err_t FatFS_init(void);
esp_err_t FatFS_deinit(void);
bool      FatFS_is_mounted(void);

/* ---- File operations (thread-safe) ---- */
esp_err_t FatFS_read_file(const char *path, void *buf, size_t buf_size, size_t *bytes_read);
esp_err_t FatFS_write_file(const char *path, const void *data, size_t len, bool append);
esp_err_t FatFS_delete_file(const char *path);
bool      FatFS_file_exists(const char *path);
esp_err_t FatFS_file_size_get(const char *path, size_t *size);

/* ---- Info ---- */
esp_err_t FatFS_info(uint64_t *total_bytes, uint64_t *free_bytes);

#ifdef __cplusplus
}
#endif

#endif /* FAT_FS_H_ */