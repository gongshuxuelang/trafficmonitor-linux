#pragma once

#include <QFile>
#include <QLoggingCategory>
#include <QMutex>
#include <QString>

#include <filesystem>
#include <utility>

namespace tm_linux {

class Logger {
public:
    explicit Logger(std::filesystem::path path);
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log(const QString& message);

private:
    static void messageHandler(QtMsgType type, const QMessageLogContext& context,
                               const QString& message);
    void rotateIfNeeded();

    std::filesystem::path path_;
    QFile file_;
    QMutex mutex_;
    QtMessageHandler previous_handler_{};
    static Logger* instance_;
};

} // namespace tm_linux
