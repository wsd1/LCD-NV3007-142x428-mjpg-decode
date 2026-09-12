/*
 * NV3007 LCD MJPEG 播放器
 * 主程序 - 从 SD卡读取 MJPEG 文件并在 LCD 上播放
 * 
 * 硬件配置：
 *   LCD: NV3007 SPI (142x428), SPI2
 *   SD卡: SPI3
 */

#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "settings.h"
#include "hardware_init.h"
#include "video_render.h"

static const char *TAG = "MAIN";

/* 视频文件路径缓存区 */
static char mjpgDirs[2048] = {0};
static char *mjpgDirPtr = mjpgDirs;

/**
 * @brief 扫描 SD卡上的 MJPEG 文件并紧凑存储到 mjpgDirs 中
 */
static void scan_mjpeg_files(void)
{
    DIR *dir = opendir("/sdcard");
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open /sdcard");
        return;
    }

    /* 重置指针到缓冲区开头 */
    mjpgDirPtr = mjpgDirs;
    memset(mjpgDirs, 0, sizeof(mjpgDirs));

    ESP_LOGI(TAG, "Scanning SD card for MJPEG files...");
    struct dirent *entry;
    int mjpeg_count = 0;

    while ((entry = readdir(dir)) != NULL) {
        /* 检查是否是 MJPEG 文件 */
        char *ext = strrchr(entry->d_name, '.');
        if (ext && (strcasecmp(ext, ".mjpeg") == 0 || strcasecmp(ext, ".mjpg") == 0)) {
            
            /* 构造完整路径 */
            char full_path[256];
            int path_len = snprintf(full_path, sizeof(full_path), "/sdcard/%s", entry->d_name);
            
            if (path_len < 0 || path_len >= sizeof(full_path)) {
                ESP_LOGW(TAG, "Filename too long: %s", entry->d_name);
                continue;
            }

            /* 检查剩余空间是否足够 (路径长度 + '\0') */
            size_t required_size = path_len + 1;
            if ((mjpgDirs + sizeof(mjpgDirs)) - mjpgDirPtr >= required_size) {
                memcpy(mjpgDirPtr, full_path, required_size);
                mjpgDirPtr += required_size; // 移动指针到下一个存储位置
                mjpeg_count++;
                ESP_LOGI(TAG, "  Added: %s", full_path);
            } else {
                ESP_LOGW(TAG, "Buffer full, skipping remaining files");
                break; 
            }
        }
    }
    closedir(dir);
    ESP_LOGI(TAG, "Found and stored %d MJPEG files", mjpeg_count);
}

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "NV3007 LCD MJPEG Player");
    ESP_LOGI(TAG, "LCD: %dx%d, SD Card: SPI%d", LCD_H_RES, LCD_V_RES, SD_SPI_HOST);
    ESP_LOGI(TAG, "========================================");

    /* 1. 初始化 LCD */
    esp_err_t ret = lcd_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LCD initialization failed!");
        return;
    }
    ESP_LOGI(TAG, "LCD initialized");

    /* 2. 初始化 SD卡 */
    ret = sd_card_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD card initialization failed!");
        return;
    }
    ESP_LOGI(TAG, "SD card initialized");

    /* 3. 扫描 SD卡文件 */
    scan_mjpeg_files();

    /* 4. 创建视频渲染实例 */
    ret = create_video_render(30);  // 30 fps
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to create video render");
        sd_card_deinit();
        return;
    }

    /* 5. 播放视频 */
    ESP_LOGI(TAG, "Starting video playlist...");
    ESP_LOGI(TAG, "Press reset to stop");

    /* 无限循环播放整个列表 */
    while (1) {
        char *current_path = mjpgDirs;

        /* 遍历紧凑存储的字符串缓冲区 */
        while (current_path < (mjpgDirs + sizeof(mjpgDirs)) && *current_path != '\0') {
            ESP_LOGI(TAG, "Now playing: %s", current_path);
            ret = video_play_mjpeg(current_path, 20, false); // 单文件不循环
            if (ret != 0) {
                ESP_LOGE(TAG, "Playback failed for %s", current_path);
            }

            /* 跳过当前字符串及其尾部的 '\0'，寻找下一个有效路径 */
            current_path += strlen(current_path) + 1;
        }

        if (mjpgDirs[0] == '\0') {
            ESP_LOGE(TAG, "No videos to play in buffer!");
            break;
        }

        ESP_LOGI(TAG, "Playlist completed, restarting...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    /* 6. 清理 */
    destroy_video_render();
    sd_card_deinit();

    ESP_LOGI(TAG, "Player stopped");
}