/**
 * @file tm_plugin_api.h
 * @brief 插件与宿主之间的稳定 C ABI 定义。
 */
#ifndef TM_PLUGIN_API_H
#define TM_PLUGIN_API_H

#include "pch.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TM_PLUGIN_API_MAJOR 1u
#define TM_PLUGIN_API_MINOR 0u
#define TM_PLUGIN_API_VERSION ((TM_PLUGIN_API_MAJOR << 16) | TM_PLUGIN_API_MINOR)

#if defined(_WIN32)
#define TM_PLUGIN_EXPORT __declspec(dllexport)
#else
#define TM_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

typedef enum TMResult {
    TM_OK = 0,
    TM_ERR_INVALID_ARGUMENT = 1,
    TM_ERR_UNSUPPORTED = 2,
    TM_ERR_IO = 3,
    TM_ERR_UNAVAILABLE = 4,
    TM_ERR_INTERNAL = 5
} TMResult;

typedef struct TMStringView {
    const char* data;
    size_t size;
} TMStringView;

typedef struct TMMetricSnapshot {
    uint32_t struct_size;
    uint32_t api_version;
    uint64_t timestamp_unix_ms;
    uint64_t up_bytes_per_sec;
    uint64_t down_bytes_per_sec;
    double cpu_usage_percent;
    double memory_usage_percent;
    uint64_t memory_used_bytes;
    uint64_t memory_total_bytes;
    double disk_io_util_percent;
    double disk_space_used_percent;
    double cpu_temp_c;
    double gpu_temp_c;
    double hdd_temp_c;
    double mainboard_temp_c;
    double cpu_freq_mhz;
    uint64_t today_up_bytes;
    uint64_t today_down_bytes;
    uint32_t valid_flags;
} TMMetricSnapshot;

typedef enum TMValidFlag {
    TM_VALID_UP = 1u << 0,
    TM_VALID_DOWN = 1u << 1,
    TM_VALID_CPU = 1u << 2,
    TM_VALID_MEMORY = 1u << 3,
    TM_VALID_DISK_IO = 1u << 4,
    TM_VALID_DISK_SPACE = 1u << 5,
    TM_VALID_CPU_TEMP = 1u << 6,
    TM_VALID_GPU_TEMP = 1u << 7,
    TM_VALID_HDD_TEMP = 1u << 8,
    TM_VALID_MAINBOARD_TEMP = 1u << 9,
    TM_VALID_CPU_FREQ = 1u << 10,
    TM_VALID_TODAY_UP = 1u << 11,
    TM_VALID_TODAY_DOWN = 1u << 12
} TMValidFlag;

typedef struct TMDrawContextV1 {
    uint32_t struct_size;
    uint32_t api_version;
    void* backend;
    /** @brief 在指定矩形内绘制文本。 */
    void (*draw_text)(void* backend, int32_t x, int32_t y, int32_t w, int32_t h,
                      const char* text_utf8, size_t text_size,
                      uint32_t argb, uint8_t align, uint8_t flags);
    /** @brief 填充圆角矩形。 */
    void (*fill_rect)(void* backend, int32_t x, int32_t y, int32_t w, int32_t h,
                      uint32_t argb, uint8_t radius);
    /** @brief 绘制直线。 */
    void (*draw_line)(void* backend, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                      uint32_t argb, uint8_t width);
    /** @brief 绘制图标。 */
    void (*draw_icon)(void* backend, const char* icon_path_utf8,
                      int32_t x, int32_t y, int32_t w, int32_t h);
    /** @brief 测量文本尺寸。 */
    void (*measure_text)(void* backend, const char* text_utf8, size_t text_size,
                         int32_t* out_w, int32_t* out_h);
} TMDrawContextV1;

typedef enum TMItemFlags {
    TM_ITEM_CUSTOM_DRAW = 1u << 0,
    TM_ITEM_DOUBLE_LINE = 1u << 1,
    TM_ITEM_USAGE_GRAPH = 1u << 2
} TMItemFlags;

typedef enum TMMouseEventType {
    TM_MOUSE_LEFT_CLICK = 0,
    TM_MOUSE_RIGHT_CLICK = 1,
    TM_MOUSE_DOUBLE_CLICK = 2,
    TM_MOUSE_WHEEL_UP = 3,
    TM_MOUSE_WHEEL_DOWN = 4
} TMMouseEventType;

typedef enum TMMouseEventFlag {
    TM_MOUSE_FLAG_FLOATING_WINDOW = 1u << 0,
    TM_MOUSE_FLAG_GNOME_TOPBAR = 1u << 1
} TMMouseEventFlag;

typedef struct TMPluginItemV1 {
    uint32_t struct_size;
    uint32_t api_version;
    const char* item_id;
    const char* display_name;
    const char* label_text;
    const char* sample_text;
    uint32_t flags;
    void* user_data;
    /** @brief 返回条目当前显示文本。 */
    TMStringView (*get_value_text)(void* user_data);
    /** @brief 自定义绘制条目内容。 */
    void (*draw)(void* user_data, const TMDrawContextV1* ctx,
                 int32_t x, int32_t y, int32_t w, int32_t h, uint32_t flags);
    /** @brief 处理鼠标事件，返回非零表示事件已处理。 */
    int32_t (*on_mouse_event)(void* user_data, uint32_t type,
                              int32_t x, int32_t y, uint32_t flags);
    /** @brief 返回历史曲线当前值。 */
    double (*get_usage_graph_value)(void* user_data);
} TMPluginItemV1;

typedef struct TMPluginCommandV1 {
    uint32_t struct_size;
    uint32_t api_version;
    const char* command_id;
    const char* display_name;
    const char* icon_path;
    uint32_t flags;
    void* user_data;
    /** @brief 返回命令当前是否被勾选。 */
    int32_t (*is_checked)(void* user_data);
    /** @brief 执行命令。 */
    void (*invoke)(void* user_data, void* parent_window);
} TMPluginCommandV1;

struct TMPluginV1;

typedef struct TMPluginHostV1 {
    uint32_t struct_size;
    uint32_t api_version;
    void* host;
    /** @brief 读取宿主监控项的数值。 */
    uint64_t (*get_monitor_value)(void* host, uint32_t item);
    /** @brief 获取插件配置目录。 */
    const char* (*get_config_dir)(void* host);
    /** @brief 获取目标屏幕的 DPI。 */
    uint32_t (*get_dpi)(void* host, uint32_t target);
    /** @brief 输出一条插件日志。 */
    void (*log)(void* host, uint32_t level, const char* message_utf8);
    /** @brief 请求宿主重绘。 */
    void (*request_repaint)(void* host);
} TMPluginHostV1;

typedef struct TMPluginV1 {
    uint32_t struct_size;
    uint32_t api_version;
    const char* plugin_id;
    const char* name;
    const char* version;
    const char* author;
    const char* description;
    const char* icon_path;
    const char* settings_schema_path;
    void* user_data;
    /** @brief 初始化插件并接收宿主 API。 */
    TMResult (*initialize)(void* user_data, const TMPluginHostV1* host);
    /** @brief 关闭插件并释放资源。 */
    void (*shutdown)(void* user_data);
    /** @brief 接收最新的指标快照。 */
    void (*update)(void* user_data, const TMMetricSnapshot* snapshot);
    /** @brief 返回插件条目数量。 */
    uint32_t (*get_item_count)(void* user_data);
    /** @brief 按索引返回插件条目。 */
    const TMPluginItemV1* (*get_item)(void* user_data, uint32_t index);
    /** @brief 返回插件命令数量。 */
    uint32_t (*get_command_count)(void* user_data);
    /** @brief 按索引返回插件命令。 */
    const TMPluginCommandV1* (*get_command)(void* user_data, uint32_t index);
    /** @brief 返回插件提示文本。 */
    TMStringView (*get_tooltip)(void* user_data);
    /** @brief 通知插件配置已变更。 */
    void (*on_config_changed)(void* user_data, const char* config_json);
} TMPluginV1;

/**
 * @brief 插件入口：返回插件实例。
 * @param host_api_version 宿主支持的插件 API 版本。
 * @return 插件描述结构指针；宿主应校验 struct_size 与 api_version。
 */
TM_PLUGIN_EXPORT const TMPluginV1* tm_plugin_get_instance(uint32_t host_api_version);

#ifdef __cplusplus
}
#endif

#endif
