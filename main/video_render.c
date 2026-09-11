/*
 * 视频渲染核心逻辑
 * 处理 MJPEG 文件读取和渲染
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_gmf_oal_mem.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "settings.h"
#include "video_render.h"

#define TAG "VIDEO_RENDER"

/* 播放器状态结构 */
typedef struct
{
    FILE *fp;
    bool eos;
    int last_size;
    long file_size;
    esp_video_render_frame_t frame;
    esp_video_render_stream_handle_t stream;
    esp_video_render_handle_t video_render;
} player_t;

/**
 * @brief 查找 MJPEG 帧结束标记
 */
static int get_frame_end(uint8_t *data, int size, bool eof)
{
    for (int i = 0; i < size - 1; i++) {
        if (data[i] == 0xFF && data[i + 1] == 0xD9) {
            if (i + 3 < size) {
                if (data[i + 2] == 0xFF && data[i + 3] == 0xD8) {
                    return i + 2;
                }
            }
            if (eof && i == size - 2) {
                return i + 2;
            }
        }
    }
    return -1;
}

/**
 * @brief 读取一帧 MJPEG 数据
 * 
 * @return 0=成功, 1=文件结束, -1=错误
 */
static int player_read_mjpeg_frame(player_t *player)
{
    esp_video_render_frame_t *frame = &player->frame;

    /* 分配帧缓冲区 */
    if (frame->data == NULL) {
        frame->data = esp_gmf_oal_malloc_align(64, MAX_FRAME_SIZE);
        if (frame->data == NULL) {
            ESP_LOGE(TAG, "Failed to allocate frame buffer");
            return -1;
        }
    }

    frame->format = ESP_VIDEO_RENDER_FORMAT_MJPEG;
    int filled = 0;

    /* 处理剩余数据 */
    if (player->last_size) {
        memmove(frame->data, frame->data + frame->size, player->last_size);
        filled = player->last_size;
        player->last_size = 0;
    } else if (player->eos) {
        return 1;  // 文件结束
    }

    /* 读取数据直到找到完整帧 */
    int frame_end = -1;
    do {
        frame_end = get_frame_end(frame->data, filled, player->eos);
        if (frame_end > 0) {
            break;
        }

        int left = MAX_FRAME_SIZE - filled;
        if (left <= 0) {
            ESP_LOGE(TAG, "Frame too large");
            return -1;
        }

        int rd = (int)fread(frame->data + filled, 1, left, player->fp);
        if (rd < 0) {
            return -1;
        }
        filled += rd;
        player->eos = feof(player->fp);

        if (rd) {
            frame_end = get_frame_end(frame->data, filled, player->eos);
        }
    } while (0);

    if (frame_end <= 0) {
        ESP_LOGE(TAG, "No frame found in %d bytes", filled);
        return -1;
    }

    frame->size = frame_end;
    player->last_size = filled - frame->size;
    return 0;
}

/**
 * @brief 重置播放器到文件开头
 */
static void player_reset(player_t *player)
{
    fseek(player->fp, 0, SEEK_SET);
    player->last_size = 0;
    player->eos = false;
}

/**
 * @brief 清理播放器资源
 */
static void cleanup_player(player_t *player)
{
    if (player->stream) {
        esp_video_render_stream_close(player->stream);
        player->stream = NULL;
    }
    if (player->fp) {
        fclose(player->fp);
        player->fp = NULL;
    }
    if (player->frame.data) {
        esp_gmf_oal_free(player->frame.data);
        player->frame.data = NULL;
    }
}

/**
 * @brief 播放单个 MJPEG 视频文件
 * 
 * @param mjpeg_path MJPEG 文件路径
 * @param fps 帧率
 * @param loop 是否循环播放
 * @return int 0=成功, -1=失败
 */
int video_play_mjpeg(const char *mjpeg_path, int fps, bool loop)
{
    player_t player = {0};
    int ret = 0;

    ESP_LOGI(TAG, "Playing: %s (fps=%d, loop=%d)", mjpeg_path, fps, loop);

    /* 打开文件 */
    player.fp = fopen(mjpeg_path, "rb");
    if (player.fp == NULL) {
        ESP_LOGE(TAG, "Failed to open file: %s", mjpeg_path);
        return -1;
    }

    /* 获取文件大小 */
    fseek(player.fp, 0, SEEK_END);
    player.file_size = ftell(player.fp);
    fseek(player.fp, 0, SEEK_SET);
    ESP_LOGI(TAG, "File size: %ld bytes", player.file_size);

    /* 获取视频渲染句柄 */
    player.video_render = get_video_render();
    if (player.video_render == NULL) {
        ESP_LOGE(TAG, "Video render not initialized");
        fclose(player.fp);
        return -1;
    }

    /* 打开视频流 */
    esp_video_render_stream_info_t stream_info = {
        .info = {
            .format = ESP_VIDEO_RENDER_FORMAT_MJPEG,
            .width = VIDEO_WIDTH,
            .height = VIDEO_HEIGHT,
            .fps = fps,
        },
        .cached = true,
    };
    ret = esp_video_render_stream_open(player.video_render, &stream_info, &player.stream);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to open stream: %d", ret);
        cleanup_player(&player);
        return -1;
    }

    /* 设置显示区域（全屏） */
    esp_video_render_rect_t video_rect = {
        .x = 0,
        .y = 0,
        .width = LCD_H_RES,
        .height = LCD_V_RES,
    };
    ret = esp_video_render_stream_set_disp_rect(player.stream, &video_rect);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to set display rect: %d", ret);
        cleanup_player(&player);
        return -1;
    }

    /* 开始异步渲染 */
    ret = esp_video_render_stream_render_async(player.stream);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to start async render: %d", ret);
        cleanup_player(&player);
        return -1;
    }

    /* 播放循环 */
    uint32_t frame_count = 0;
    uint32_t start_time = esp_timer_get_time() / 1000;

    while (1) {
        /* 读取一帧 */
        ret = player_read_mjpeg_frame(&player);
        if (ret < 0) {
            ESP_LOGE(TAG, "Failed to read frame");
            break;
        }

        /* 文件结束 */
        if (ret == 1) {
            if (loop) {
                player_reset(&player);
                ESP_LOGI(TAG, "Looping...");
                continue;
            } else {
                break;
            }
        }

        /* 写入帧到渲染器 */
        ret = esp_video_render_stream_write(player.stream, &player.frame);
        if (ret != ESP_VIDEO_RENDER_ERR_OK) {
            ESP_LOGW(TAG, "Failed to write frame: %d", ret);
            if (ret == ESP_VIDEO_RENDER_ERR_INVALID_ARG ||
                ret == ESP_VIDEO_RENDER_ERR_INVALID_STATE) {
                break;
            }
        }

        frame_count++;

        /* 每100帧打印一次状态 */
        if (frame_count % 100 == 0) {
            uint32_t elapsed = (esp_timer_get_time() / 1000) - start_time;
            float actual_fps = (float)frame_count * 1000.0f / elapsed;
            ESP_LOGI(TAG, "Played %lu frames, %.1f fps", frame_count, actual_fps);
        }
    }

    /* 播放完成 */
    uint32_t total_time = (esp_timer_get_time() / 1000) - start_time;
    ESP_LOGI(TAG, "Playback finished: %lu frames in %lu ms", frame_count, total_time);

    cleanup_player(&player);
    return 0;
}