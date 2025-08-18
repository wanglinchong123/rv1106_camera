#include "infra/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>

static FILE *log_file = NULL;
static LogLevel current_level = LOG_LEVEL_DEBUG;

static const char *level_strings[] = {
    "DEBUG", "INFO", "WARN", "ERROR"
};

/**
 * @brief 初始化日志系统
 * 
 * 打开指定的日志文件用于写入日志信息，并设置日志输出等级。
 * 注意：该函数会清空旧日志，每次运行都会覆盖之前的内容。
 * 
 * @param filename 日志文件名（如 "chat.log"）
 * @param level 最低日志等级（低于该等级的日志不会被记录）
 *              可选值：LOG_LEVEL_DEBUG / LOG_LEVEL_INFO / LOG_LEVEL_WARN / LOG_LEVEL_ERROR
 */
void log_init(const char *filename, LogLevel level) {
    log_file = fopen(filename, "w");
    if (!log_file) {
        // 不向终端输出
        perror("无法打开日志文件");
        exit(EXIT_FAILURE);
    }
    current_level = level;
    LOGI("日志初始化成功[%s]", level_strings[level]);
}

/**
 * @brief 设置当前日志等级
 * 
 * 修改运行时的日志等级。等级低于当前设置的日志不会被写入文件。
 * 可用于运行过程中动态调整日志输出量。
 * 
 * @param level 新的最低日志等级
 */
void log_set_level(LogLevel level) {
    current_level = level;
}

/**
 * @brief 关闭日志系统
 * 
 * 关闭日志文件，释放资源。程序退出前应调用此函数，确保日志完整写入。
 */
void log_close() {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}

// void log_write(LogLevel level, const char *file, int line, const char *fmt, ...) {
//     if (level < current_level || !log_file) return;

//     time_t now = time(NULL);
//     struct tm *t = localtime(&now);
//     char time_buf[20];
//     strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", t);

//     // 日志写入文件
//     fprintf(log_file, "[%s] [%s] (%s:%d) ", time_buf, level_strings[level], file, line);

//     va_list args;
//     va_start(args, fmt);
//     vfprintf(log_file, fmt, args);
//     va_end(args);

//     fprintf(log_file, "\n");
//     fflush(log_file);
// }
void log_write(LogLevel level, const char *file, int line, const char *fmt, ...) {
    if (level < current_level || !log_file) return;

    // 获取当前时间，精确到毫秒
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double timestamp = tv.tv_sec + tv.tv_usec / 1000000.0;

    // 打印带毫秒的时间戳
    fprintf(log_file, "[%.3f] [%s] (%s:%d) ", timestamp, level_strings[level], file, line);

    va_list args;
    va_start(args, fmt);
    vfprintf(log_file, fmt, args);
    va_end(args);

    fprintf(log_file, "\n");
    fflush(log_file);
}
