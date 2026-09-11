/*
 * 视频渲染系统 - 硬件适配层
 * 连接 esp_video_render 组件与 NV3007 LCD
 */

#include "freertos/FreeRTOS.h"
#include "esp_gmf_video_dec.h"
#include "esp_gmf_video_enc.h"
#include "esp_gmf_video_overlay.h"
#include "esp_gmf_video_scale.h"
#include "esp_gmf_video_crop.h"
#include "esp_gmf_pool.h"
#include "esp_video_dec_default.h"
#include "esp_gmf_video_color_convert.h"
#include "esp_video_render.h"
#include "esp_video_render_backend.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "settings.h"
#include "video_render.h"
#include "hardware_init.h"

#define TAG "RENDER_SYS"

static esp_video_render_handle_t s_video_render = NULL;
static esp_gmf_pool_handle_t s_render_pool = NULL;

/**
 * @brief 创建默认的 GMF 池
 */
static int create_default_pool(esp_gmf_pool_handle_t *pool)
{
    *pool = NULL;
    esp_gmf_element_handle_t el = NULL;
    do {
        BREAK_ON_FAIL(esp_gmf_pool_init(pool));

        /* 注册视频解码器 */
        esp_gmf_video_dec_init(NULL, &el);
        BREAK_ON_FAIL(esp_gmf_pool_register_element(*pool, el, NULL));

        el = NULL;
        esp_gmf_video_overlay_init(NULL, &el);
        BREAK_ON_FAIL(esp_gmf_pool_register_element(*pool, el, NULL));

        /* ESP32-S3 使用软件缩放和裁剪 */
        el = NULL;
        esp_imgfx_scale_cfg_t scale_cfg = {
            .filter_type = ESP_IMGFX_SCALE_FILTER_TYPE_BILINEAR
        };
        esp_gmf_video_scale_init(&scale_cfg, &el);
        BREAK_ON_FAIL(esp_gmf_pool_register_element(*pool, el, NULL));

        el = NULL;
        esp_imgfx_crop_cfg_t crop_cfg = {};
        esp_gmf_video_crop_init(&crop_cfg, &el);
        BREAK_ON_FAIL(esp_gmf_pool_register_element(*pool, el, NULL));

        el = NULL;
        esp_imgfx_color_convert_cfg_t color_convert_cfg = {
            .color_space_std = ESP_IMGFX_COLOR_SPACE_STD_BT601
        };
        esp_gmf_video_color_convert_init(&color_convert_cfg, &el);
        BREAK_ON_FAIL(esp_gmf_pool_register_element(*pool, el, NULL));

        return 0;
    } while (0);

    if (el) {
        esp_gmf_element_deinit(el);
    }
    if (*pool) {
        esp_gmf_pool_deinit(*pool);
        *pool = NULL;
    }
    return -1;
}

/**
 * @brief 创建视频渲染实例
 * 
 * @param fps 帧率
 * @return int 0=成功, -1=失败
 */
int create_video_render(uint8_t fps)
{
    int ret = 0;

    /* 获取 LCD 句柄 */
    esp_lcd_panel_handle_t panel = lcd_get_panel();
    esp_lcd_panel_io_handle_t io = lcd_get_io();
    if (panel == NULL || io == NULL) {
        ESP_LOGE(TAG, "LCD not initialized");
        return -1;
    }

    fps = fps > 0 ? fps : 10;

    do {
        /* 注册默认解码器 */
        esp_video_dec_register_default();

        /* 创建 GMF 池 */
        ret = create_default_pool(&s_render_pool);
        BREAK_ON_FAIL(ret);

        /* 创建视频渲染实例 */
        esp_video_render_cfg_t render_cfg = {
            .pool = s_render_pool,
            .fps = fps,
        };
        ret = esp_video_render_create(&render_cfg, &s_video_render);
        BREAK_ON_FAIL(ret);

        /* 配置 LCD 后端 */
        esp_video_render_lcd_cfg_t lcd_cfg = {
            .width = LCD_H_RES,
            .height = LCD_V_RES,
            .fb_num = 1,
            .lcd_handle = panel,
            .io_handle = io,
            .lcd_type = ESP_VIDEO_RENDER_LCD_TYPE_DVP,  /* SPI 类型 */
            .out_format = ESP_VIDEO_RENDER_FORMAT_RGB565_BE,  /* 大端字节序 */
        };

        esp_video_render_backend_cfg_t backend_cfg = {
            .ops = esp_video_render_get_lcd_backend(),
            .cfg = &lcd_cfg,
            .cfg_size = sizeof(lcd_cfg),
        };
        ret = esp_video_render_set_display(s_video_render, &backend_cfg);
        BREAK_ON_FAIL(ret);

        ESP_LOGI(TAG, "Video render created (fps=%d, %dx%d)", fps, LCD_H_RES, LCD_V_RES);
        return 0;
    } while (0);

    ESP_LOGE(TAG, "Failed to create video render");
    destroy_video_render();
    return ret;
}

/**
 * @brief 获取视频渲染句柄
 */
esp_video_render_handle_t get_video_render(void)
{
    return s_video_render;
}

/**
 * @brief 销毁视频渲染实例
 */
void destroy_video_render(void)
{
    if (s_video_render) {
        esp_video_render_destroy(s_video_render);
        s_video_render = NULL;
    }
    if (s_render_pool) {
        esp_gmf_pool_deinit(s_render_pool);
        s_render_pool = NULL;
    }
    esp_video_dec_unregister_default();
    ESP_LOGI(TAG, "Video render destroyed");
}