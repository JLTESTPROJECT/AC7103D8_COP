#ifndef __JLDTP_H__
#define __JLDTP_H__

#include "generic/typedef.h"

// 类型定义
typedef void *jldtp_handle_t;
typedef void (*jldtp_data_callback_t)(void *user_data, u8 *data, u16 length);

// 错误码定义
typedef enum {
    JLDTP_SUCCESS = 1,
    JLDTP_ERROR_INVALID_PARAM = -1,
    JLDTP_ERROR_MEMORY = -2,
    JLDTP_ERROR_CRC = -3,
    JLDTP_ERROR_FRAME_TOO_LONG = -4,
    JLDTP_ERROR_DEVICE = -5,
    JLDTP_ERROR_BUSY = -6,
} jldtp_result_t;

// 传输设备接口
struct jldtp_transport {
    u16 mtu_size;
    void *(*open)(jldtp_handle_t handle);
    void (*close)(void *handle);
    int (*read)(void *handle, u8 *data, int length);
    int (*write)(void *handle, u8 *data, int length);
};

// 配置结构
typedef struct {
    u16 max_tx_pending;
    const char *task_name;
} jldtp_config_t;

// API 函数声明

/**
 * @brief 创建JLDTP实例
 * @return JLDTP句柄，失败返回NULL
 */
jldtp_handle_t jldtp_create(void);

/**
 * @brief 使用配置创建JLDTP实例
 * @param config 配置参数
 * @return JLDTP句柄，失败返回NULL
 */
jldtp_handle_t jldtp_create_with_config(const jldtp_config_t *config);

/**
 * @brief 销毁JLDTP实例
 * @param handle JLDTP句柄
 */
void jldtp_destroy(jldtp_handle_t handle);

/**
 * @brief 设置任务名称
 * @param handle JLDTP句柄
 * @param task_name 任务名称
 */
void jldtp_set_task_name(jldtp_handle_t handle, const char *task_name);

int jldtp_get_mtu_size(jldtp_handle_t handle);

/**
 * @brief 注册数据接收回调
 * @param handle JLDTP句柄
 * @param user_data 用户数据
 * @param callback 数据回调函数
 */
void jldtp_register_data_callback(jldtp_handle_t handle, void *user_data,
                                  jldtp_data_callback_t callback);

/**
 * @brief 注册传输设备
 * @param handle JLDTP句柄
 * @param transport 传输设备接口
 * @return 执行结果
 */
jldtp_result_t jldtp_register_transport(jldtp_handle_t handle, const struct jldtp_transport *transport);

/**
 * @brief 分配发送缓冲区
 * @param handle JLDTP句柄
 * @param data_length 数据长度
 * @return 缓冲区指针，失败返回NULL
 */
u8 *jldtp_alloc_tx_buffer(jldtp_handle_t handle, u16 data_length);

jldtp_result_t jldtp_commit_tx_buffer(jldtp_handle_t handle, u8 *buffer,
                                      u16 data_len, u8 priority);
/**
 * @brief 发送数据
 * @param handle JLDTP句柄
 * @param buffer 缓冲区指针
 * @param data_len 数据长度
 * @param priority 优先级
 * @return 执行结果
 */
jldtp_result_t jldtp_send_data(jldtp_handle_t handle, u8 *buffer,
                               u16 data_len, u8 priority);

/**
 * @brief 处理接收数据
 * @param handle JLDTP句柄
 * @param data 接收数据
 * @param data_len 数据长度
 */
void jldtp_process_rx_data(jldtp_handle_t handle, u8 *data, int data_len);

/**
 * @brief 通知接收数据就绪
 * @param handle JLDTP句柄
 */
void jldtp_notify_rx_ready(jldtp_handle_t handle);

/**
 * @brief 通知发送完成
 * @param handle JLDTP句柄
 * @param completed_count 完成数量
 */
void jldtp_notify_tx_complete(jldtp_handle_t handle, int completed_count);

#endif // __JLDTP_H__

