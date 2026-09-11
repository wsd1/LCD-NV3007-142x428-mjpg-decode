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

/**
 * @brief 扫描 SD卡上的 MJPEG 文件
 */
static void scan_mjpeg_files(void)
{
    DIR *dir = opendir("/sdcard");
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open /sdcard");
        return;
    }

    ESP_LOGI(TAG, "Files on SD card:");
    struct dirent *entry;
    int mjpeg_count = 0;
    while ((entry = readdir(dir)) != NULL) {
        ESP_LOGI(TAG, "  %s", entry->d_name);
        /* 检查是否是 MJPEG 文件 */
        char *ext = strrchr(entry->d_name, '.');
        if (ext && (strcasecmp(ext, ".mjpeg") == 0 || strcasecmp(ext, ".mjpg") == 0)) {
            mjpeg_count++;
        }
    }
    closedir(dir);
    ESP_LOGI(TAG, "Found %d MJPEG files", mjpeg_count);
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

    /* 4. 检查视频文件是否存在 */
    if (!sd_card_file_exists(MJPEG_FILE_PATH)) {
        ESP_LOGE(TAG, "Video file not found: %s", MJPEG_FILE_PATH);
        ESP_LOGI(TAG, "Please copy a MJPEG file to SD card as '%s'", MJPEG_FILE_PATH);
        sd_card_deinit();
        return;
    }

    /* 5. 创建视频渲染实例 */
    ret = create_video_render(30);  // 30 fps
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to create video render");
        sd_card_deinit();
        return;
    }

    /* 6. 播放视频 */
    ESP_LOGI(TAG, "Starting video playback...");
    ESP_LOGI(TAG, "Press reset to stop");

    /* 无限循环播放 */
    while (1) {
        ret = video_play_mjpeg(MJPEG_FILE_PATH, 20, true);  // 循环播放
        if (ret != 0) {
            ESP_LOGE(TAG, "Playback failed");
            break;
        }
        ESP_LOGI(TAG, "Playback completed, restarting...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    /* 7. 清理 */
    destroy_video_render();
    sd_card_deinit();

    ESP_LOGI(TAG, "Player stopped");
}