#include "tm/logger.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>

#include <cstdio>
#include <system_error>

namespace tm_linux {

namespace {

QString typeName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("DEBUG");
    case QtInfoMsg:
        return QStringLiteral("INFO");
    case QtWarningMsg:
        return QStringLiteral("WARN");
    case QtCriticalMsg:
        return QStringLiteral("ERROR");
    case QtFatalMsg:
        return QStringLiteral("FATAL");
    }
    return QStringLiteral("LOG");
}

} // namespace

Logger* Logger::instance_ = nullptr;

Logger::Logger(std::filesystem::path path)
    : path_(std::move(path))
{
    std::error_code error;
    if (!path_.parent_path().empty()) {
        std::filesystem::create_directories(path_.parent_path(), error);
    }

    file_.setFileName(QString::fromStdString(path_.string()));
    file_.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);

    instance_ = this;
    previous_handler_ = qInstallMessageHandler(&Logger::messageHandler);
}

Logger::~Logger()
{
    qInstallMessageHandler(previous_handler_);
    if (instance_ == this) {
        instance_ = nullptr;
    }
    file_.close();
}

void Logger::log(const QString& message)
{
    QMutexLocker locker(&mutex_);

    if (!file_.isOpen()) {
        file_.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
        if (!file_.isOpen()) {
            std::fprintf(stderr, "%s\n", message.toLocal8Bit().constData());
            return;
        }
    }

    rotateIfNeeded();

    const QString line = QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
        + QLatin1Char(' ') + message + QLatin1Char('\n');
    file_.write(line.toUtf8());
    file_.flush();
}

void Logger::messageHandler(QtMsgType type, const QMessageLogContext&, const QString& message)
{
    const QString formatted = QStringLiteral("[%1] %2").arg(typeName(type), message);
    if (instance_ != nullptr) {
        instance_->log(formatted);
    }
    if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
        std::fprintf(stderr, "%s\n", formatted.toLocal8Bit().constData());
    }
}

void Logger::rotateIfNeeded()
{
    constexpr qint64 kMaxSize = 5 * 1024 * 1024;
    if (file_.size() < kMaxSize) {
        return;
    }

    file_.close();
    const QString base = QString::fromStdString(path_.string());
    QFile::remove(base + QStringLiteral(".3"));
    QFile::rename(base + QStringLiteral(".2"), base + QStringLiteral(".3"));
    QFile::rename(base + QStringLiteral(".1"), base + QStringLiteral(".2"));
    QFile::rename(base, base + QStringLiteral(".1"));
    file_.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

} // namespace tm_linux
