/*
 * @Author: bjy
 * @Date: 2025-07-18 14:08:10
 * @LastEditors: bjy
 * @LastEditTime: 2025-07-18 14:42:50
 * @FilePath: /camera/include/infra/logging/logger.h
 * @Description:
 *
 * Copyright (c) 2025 by bjy, All Rights Reserved.
 */
#ifndef LOGGER_H
#define LOGGER_H

// #include <stdarg.h>

typedef enum
{
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} LogLevel;

void log_init(const char *filename, LogLevel level);
void log_set_level(LogLevel level);
void log_close();

void log_write(LogLevel level, const char *file, int line, const char *fmt, ...);

#define LOGD(fmt, ...) log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) log_write(LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define CHECK_RET(expr, msg)                        \
    do                                              \
    {                                               \
        int _ret = (expr);                          \
        if (_ret != 0)                              \
        {                                           \
            LOGE("%s failed! ret = %d", msg, _ret); \
            return _ret;                            \
        }                                           \
        else                                        \
        {                                           \
            LOGI("%s success!", msg);               \
        }                                           \
    } while (0)

#endif
