/*
 * 硬件初始化模块
 * 负责初始化 NV3007 LCD 和 SD卡
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_nv3007.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "esp_log.h"
#include "esp_err.h"
#include "settings.h"

static const char *TAG = "HW_INIT";

/* 全局句柄 */
static esp_lcd_panel_handle_t s_lcd_panel = NULL;
static esp_lcd_panel_io_handle_t s_lcd_io = NULL;
static sdmmc_card_t *s_sd_card = NULL;
/* 
// SPI 传输完成信号量 
static SemaphoreHandle_t s_trans_done = NULL;


// LCD DMA 缓冲区 
static uint16_t s_lcd_buf[LCD_H_RES * CHUNK_LINES];

static bool IRAM_ATTR on_color_trans_done(esp_lcd_panel_io_handle_t io,
                                          esp_lcd_panel_io_event_data_t *edata,
                                          void *user_ctx)
{
    BaseType_t hp_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(s_trans_done, &hp_task_woken);
    return hp_task_woken == pdTRUE;
}
*/
/**
 * @brief 初始化 LCD 背光
 */
static void lcd_backlight_init(void)
{
    gpio_config_t bl_cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << LCD_PIN_BL,
    };
    gpio_config(&bl_cfg);
    gpio_set_level(LCD_PIN_BL, 0);  // 先关闭背光
}

/**
 * @brief 初始化 NV3007 LCD
 * 
 * @return esp_err_t 
 */
esp_err_t lcd_init(void)
{
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "Initializing NV3007 LCD...");

    /* 创建信号量
    s_trans_done = xSemaphoreCreateBinary();
    if (s_trans_done == NULL) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        return ESP_ERR_NO_MEM;
    } */

    /* 初始化背光 */
    lcd_backlight_init();

    /* 1. 初始化 SPI 总线 */
    spi_bus_config_t bus_cfg = NV3007_PANEL_BUS_SPI_CONFIG(
        LCD_PIN_SCK, LCD_PIN_MOSI, LCD_H_RES * CHUNK_LINES * 2);
    ret = spi_bus_initialize(LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 2. 创建 Panel IO */
    esp_lcd_panel_io_spi_config_t io_cfg = NV3007_PANEL_IO_SPI_CONFIG(
        LCD_PIN_CS, LCD_PIN_DC, NULL, NULL);
    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_cfg, &s_lcd_io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create panel IO: %s", esp_err_to_name(ret));
        spi_bus_free(LCD_SPI_HOST);
        return ret;
    }

    /* 3. 创建 NV3007 面板 */
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = NULL,
    };
    ret = esp_lcd_new_panel_nv3007(s_lcd_io, &panel_cfg, &s_lcd_panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create NV3007 panel: %s", esp_err_to_name(ret));
        esp_lcd_panel_io_del(s_lcd_io);
        spi_bus_free(LCD_SPI_HOST);
        return ret;
    }

    /* 4. 复位并初始化面板 */
    ret = esp_lcd_panel_reset(s_lcd_panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset panel: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ret = esp_lcd_panel_init(s_lcd_panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init panel: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    /* 5. 点亮背光 */
    gpio_set_level(LCD_PIN_BL, 1);

    ESP_LOGI(TAG, "LCD initialized successfully (%dx%d)", LCD_H_RES, LCD_V_RES);
    return ESP_OK;

cleanup:
    if (s_lcd_panel) {
        esp_lcd_panel_del(s_lcd_panel);
        s_lcd_panel = NULL;
    }
    if (s_lcd_io) {
        esp_lcd_panel_io_del(s_lcd_io);
        s_lcd_io = NULL;
    }
    spi_bus_free(LCD_SPI_HOST);
    return ret;
}

/**
 * @brief 获取 LCD 面板句柄
 */
esp_lcd_panel_handle_t lcd_get_panel(void)
{
    return s_lcd_panel;
}

/**
 * @brief 获取 LCD IO 句柄
 */
esp_lcd_panel_io_handle_t lcd_get_io(void)
{
    return s_lcd_io;
}

/**
 * @brief 初始化 SD卡
 * 
 * @return esp_err_t 
 */
esp_err_t sd_card_init(void)
{
    ESP_LOGI(TAG, "Initializing SD card...");

    /* 配置 SPI 总线 */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_MOSI_GPIO,
        .miso_io_num = SD_MISO_GPIO,
        .sclk_io_num = SD_SCK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    /* 初始化 SPI 总线 */
    esp_err_t ret = spi_bus_initialize(SD_SPI_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 配置 SD卡设备 */
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS_GPIO;
    slot_config.host_id = SD_SPI_HOST;

    /* 配置挂载选项 */
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    /* 配置 SD卡主机 */
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_SPI_HOST;
    host.flags = SDMMC_HOST_FLAG_SPI | SDMMC_HOST_FLAG_DEINIT_ARG;

    /* 挂载 SD卡 */
    ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &s_sd_card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem");
        } else {
            ESP_LOGE(TAG, "Failed to initialize card: %s", esp_err_to_name(ret));
        }
        spi_bus_free(SD_SPI_HOST);
        return ret;
    }

    ESP_LOGI(TAG, "SD card mounted successfully");
    sdmmc_card_print_info(stdout, s_sd_card);

    return ESP_OK;
}

/**
 * @brief 卸载 SD卡
 */
void sd_card_deinit(void)
{
    if (s_sd_card) {
        esp_vfs_fat_sdcard_unmount("/sdcard", s_sd_card);
        spi_bus_free(SD_SPI_HOST);
        s_sd_card = NULL;
        ESP_LOGI(TAG, "SD card unmounted");
    }
}

/**
 * @brief 检查 SD卡上的文件是否存在
 */
bool sd_card_file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}