/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/* NV3007 LCD 分辨率 */
#define VIDEO_WIDTH     142
#define VIDEO_HEIGHT    428
#define LCD_H_RES       VIDEO_WIDTH
#define LCD_V_RES       VIDEO_HEIGHT

/* MJPEG 帧缓冲区大小 (根据实际MJPG文件调整) */
#define MAX_FRAME_SIZE  (32 * 1024)

/* 播放时间 (毫秒)，0 表示无限循环 */
#define PLAY_TIME       0

/* SD卡上的视频文件路径 */
#define MJPEG_FILE_PATH "/sdcard/video.mjpeg"

/* 备用文件路径（用于测试） */
#define LEFT_FILE       MJPEG_FILE_PATH
#define RIGHT_FILE      "/sdcard/video2.mjpeg"

/* LCD 引脚定义 */
#define LCD_PIN_SCK     7
#define LCD_PIN_MOSI    8
#define LCD_PIN_RST     9
#define LCD_PIN_DC      10
#define LCD_PIN_CS      11
#define LCD_PIN_BL      12
#define LCD_SPI_HOST    SPI2_HOST

/* SD卡引脚定义 */
#define SD_SCK_GPIO     16
#define SD_MOSI_GPIO    17
#define SD_MISO_GPIO    18
#define SD_CS_GPIO      15
#define SD_SPI_HOST     SPI3_HOST

/* 每次刷屏的行数 */
#define CHUNK_LINES     50

#ifdef __cplusplus
}
#endif