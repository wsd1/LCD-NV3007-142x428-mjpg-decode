/*
 * 视频渲染模块头文件
 */

#pragma once

#include "esp_video_render.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* 错误检查宏 */
#define BREAK_ON_FAIL(sta)                                          \
    {                                                               \
        int _ret = sta;                                             \
        if (_ret) {                                                 \
            ESP_LOGE(TAG, "Fail at %s:%d ret %d", __func__, __LINE__, _ret); \
            break;                                                  \
        }                                                           \
    }

/**
 * @brief 创建视频渲染实例
 * 
 * @param fps 帧率
 * @return int 0=成功, -1=失败
 */
int create_video_render(uint8_t fps);

/**
 * @brief 获取视频渲染句柄
 */
esp_video_render_handle_t get_video_render(void);

/**
 * @brief 销毁视频渲染实例
 */
void destroy_video_render(void);

/**
 * @brief 播放单个 MJPEG 视频文件
 * 
 * @param mjpeg_path MJPEG 文件路径
 * @param fps 帧率
 * @param loop 是否循环播放
 * @return int 0=成功, -1=失败
 */
int video_play_mjpeg(const char *mjpeg_path, int fps, bool loop);

#ifdef __cplusplus
}
#endif