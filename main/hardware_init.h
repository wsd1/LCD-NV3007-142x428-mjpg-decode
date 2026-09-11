/*
 * 硬件初始化模块头文件
 */

#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief 初始化 NV3007 LCD
 * 
 * @return esp_err_t 
 */
esp_err_t lcd_init(void);

/**
 * @brief 获取 LCD 面板句柄
 */
esp_lcd_panel_handle_t lcd_get_panel(void);

/**
 * @brief 获取 LCD IO 句柄
 */
esp_lcd_panel_io_handle_t lcd_get_io(void);

/**
 * @brief 初始化 SD卡
 * 
 * @return esp_err_t 
 */
esp_err_t sd_card_init(void);

/**
 * @brief 卸载 SD卡
 */
void sd_card_deinit(void);

/**
 * @brief 检查 SD卡上的文件是否存在
 */
bool sd_card_file_exists(const char *path);

#ifdef __cplusplus
}
#endif