#ifndef TRAFFICMONITOR_LINUX_SRC_CORE_LOGGER_H_
#define TRAFFICMONITOR_LINUX_SRC_CORE_LOGGER_H_

#include "pch.h"

namespace tm_linux {

/**
 * @brief 将 Qt 日志写入文件并在超过大小上限时轮转的日志器。
 */
class Logger {
public:
    /**
     * @brief 创建日志器并安装 Qt 消息处理器。
     * @param path 日志文件路径，父目录不存在时会自动创建。
     */
    explicit Logger(std::filesystem::path path);

    /**
     * @brief 恢复先前的 Qt 消息处理器并关闭日志文件。
     */
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    /**
     * @brief 写入一条带时间戳的日志。
     * @param message 日志正文；文件不可写时回退输出到 stderr。
     */
    void log(const QString& message);

private:
    /**
     * @brief Qt 消息回调，统一格式化日志并输出到文件与 stderr。
     * @param type 消息级别。
     * @param context 日志上下文，Debug 构建下用于附加源码位置。
     * @param message 消息正文。
     */
    static void messageHandler(QtMsgType type, const QMessageLogContext& context,
                               const QString& message);

    /**
     * @brief 当日志文件超过 5 MiB 时按 .1/.2/.3 依次轮转。
     */
    void rotateIfNeeded();

    std::filesystem::path path_;
    QFile file_;
    QMutex mutex_;
    QtMessageHandler previous_handler_{};
    static Logger* instance_;
};

} // namespace tm_linux

#endif  // TRAFFICMONITOR_LINUX_SRC_CORE_LOGGER_H_
