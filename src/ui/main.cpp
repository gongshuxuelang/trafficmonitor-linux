#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <unistd.h>

#include "tm/app_settings.h"
#include "tm/formatting.h"
#include "tm/logger.h"
#include "tm/plugin_host.h"
#include "tm/sampler.h"
#include "tm/storage.h"
#include "tm/system_metrics.h"
#include "ui/floating_window.h"
#include "ui/settings_dialog.h"
#include "ui/tray_icon.h"

using namespace tm_linux;

namespace {

constexpr const char* kAppName = "trafficmonitor-linux";
constexpr const char* kVersion = "0.1.0";

QString configDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/") + QLatin1String(kAppName);
}

QString dataDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/") + QLatin1String(kAppName);
}

QString pluginDirectory()
{
    const QString override_dir = qEnvironmentVariable("TRAFFICMONITOR_PLUGIN_DIR");
    if (!override_dir.isEmpty()) {
        return override_dir;
    }
    return QCoreApplication::applicationDirPath() + QStringLiteral("/plugins");
}

QString todayLocal()
{
    return QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
}

qint64 alignedTimestamp()
{
    return (QDateTime::currentSecsSinceEpoch() / 60) * 60;
}

QString tooltipText(const Snapshot& snapshot)
{
    const QString up = snapshot.network.has_value()
        ? QString::fromStdString(formatBytesPerSecond(snapshot.network->tx_bytes_per_sec,
                                                      UnitMode::Auto, UnitBase::Byte))
        : QStringLiteral("--");
    const QString down = snapshot.network.has_value()
        ? QString::fromStdString(formatBytesPerSecond(snapshot.network->rx_bytes_per_sec,
                                                      UnitMode::Auto, UnitBase::Byte))
        : QStringLiteral("--");
    const QString cpu = QString::fromStdString(formatPercent(snapshot.cpu.usage_percent));
    const QString memory = snapshot.memory.has_value()
        ? QString::fromStdString(formatPercent(snapshot.memory->used_percent))
        : QStringLiteral("--");
    return QStringLiteral("↑ %1  ↓ %2\nCPU %3  MEM %4").arg(up, down, cpu, memory);
}

int runDiagnostics()
{
    QTextStream out(stdout);
    out << "TrafficMonitor for Linux " << kVersion << "\n";
    out << "Qt: " << qVersion() << "\n";
    out << "Session: " << qEnvironmentVariable("XDG_SESSION_TYPE") << "\n";
    out << "Desktop: " << qEnvironmentVariable("XDG_CURRENT_DESKTOP") << "\n";
    out << "Config dir: " << configDirectory() << "\n";
    out << "Data dir: " << dataDirectory() << "\n";
    out << "Plugin dir: " << pluginDirectory() << "\n";

    SystemMetricsCollector collector;
    collector.sample();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    const Snapshot snapshot = collector.sample();

    out << "Network: "
        << (snapshot.network.has_value() ? QString::fromStdString(snapshot.network->interface)
                                         : QStringLiteral("unavailable"))
        << "\n";
    if (snapshot.network.has_value()) {
        out << "  up: "
            << QString::fromStdString(formatBytesPerSecond(snapshot.network->tx_bytes_per_sec,
                                                           UnitMode::Auto, UnitBase::Byte))
            << "  down: "
            << QString::fromStdString(formatBytesPerSecond(snapshot.network->rx_bytes_per_sec,
                                                           UnitMode::Auto, UnitBase::Byte))
            << "\n";
    }
    out << "CPU cores: " << snapshot.cpu.core_count << "\n";
    out << "CPU usage: " << QString::fromStdString(formatPercent(snapshot.cpu.usage_percent))
        << "\n";
    if (snapshot.memory.has_value()) {
        out << "Memory: " << QString::fromStdString(formatPercent(snapshot.memory->used_percent))
            << "  "
            << QString::fromStdString(formatMemory(snapshot.memory->used_bytes,
                                                   snapshot.memory->total_bytes))
            << "\n";
    } else {
        out << "Memory: unavailable\n";
    }
    if (snapshot.disk.has_value()) {
        out << "Disk: io "
            << (snapshot.disk->io_available
                    ? QString::fromStdString(formatPercent(snapshot.disk->io_util_percent))
                    : QStringLiteral("--"))
            << "  used "
            << QString::fromStdString(formatPercent(snapshot.disk->used_percent)) << "\n";
    } else {
        out << "Disk: unavailable\n";
    }
    out << "CPU temperature: "
        << (snapshot.temperature.cpu_c.has_value()
                ? QString::number(*snapshot.temperature.cpu_c, 'f', 1)
                : QStringLiteral("--"))
        << "\n";
    out << "CPU frequency: " << QString::number(snapshot.cpu.frequency_mhz, 'f', 1) << " MHz\n";

    PluginHost plugins(pluginDirectory().toStdString());
    plugins.loadAll();
    out << "Plugins scanned: " << plugins.plugins().size() << "\n";
    plugins.refreshItemValues();
    for (const auto& plugin : plugins.plugins()) {
        out << "  " << QString::fromStdString(plugin.pluginId) << " "
            << (plugin.loaded ? "loaded" : "failed");
        if (!plugin.loaded) {
            out << " (" << QString::fromStdString(plugin.error) << ")";
        }
        out << "\n";
    }
    for (const auto& item : plugins.allItems()) {
        out << "    " << QString::fromStdString(item.itemId) << " = "
            << QString::fromStdString(item.value) << "\n";
    }
    out.flush();
    return 0;
}

int runQuit()
{
    QLocalSocket socket;
    socket.connectToServer(QStringLiteral("trafficmonitor-linux-%1").arg(::getuid()));
    if (!socket.waitForConnected(300)) {
        QTextStream(stderr) << "TrafficMonitor is not running\n";
        return 1;
    }
    socket.write("quit");
    socket.waitForBytesWritten(300);
    socket.disconnectFromServer();
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    QStringList arguments;
    arguments.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        arguments.push_back(QString::fromLocal8Bit(argv[i]));
    }

    QCoreApplication::setOrganizationName(QLatin1String(kAppName));
    QCoreApplication::setApplicationName(QLatin1String(kAppName));
    QCoreApplication::setApplicationVersion(QLatin1String(kVersion));

    if (arguments.contains(QStringLiteral("--version"))) {
        QCoreApplication app(argc, argv);
        QTextStream(stdout) << "TrafficMonitor for Linux " << kVersion << "\n";
        return 0;
    }
    if (arguments.contains(QStringLiteral("--diagnose"))) {
        QCoreApplication app(argc, argv);
        return runDiagnostics();
    }
    if (arguments.contains(QStringLiteral("--quit"))) {
        QCoreApplication app(argc, argv);
        return runQuit();
    }

    QApplication app(argc, argv);
    app.setApplicationDisplayName(QStringLiteral("TrafficMonitor"));
    app.setDesktopFileName(QStringLiteral("trafficmonitor-linux"));
    app.setQuitOnLastWindowClosed(false);

    const bool safe_mode = arguments.contains(QStringLiteral("--safe-mode"));
    const bool smoke_test = arguments.contains(QStringLiteral("--smoke-test"));
    const bool open_settings = arguments.contains(QStringLiteral("--settings"));

    const QString config_dir = configDirectory();
    const QString data_dir = dataDirectory();
    if (!QDir().mkpath(config_dir) || !QDir().mkpath(data_dir)) {
        QTextStream(stderr) << "Cannot create TrafficMonitor directories\n";
        return 1;
    }

    QString state_dir =
        QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation);
    if (state_dir.isEmpty()) {
        state_dir = QDir::homePath() + QStringLiteral("/.local/state");
    }
    state_dir += QStringLiteral("/trafficmonitor-linux");
    QDir().mkpath(state_dir);
    Logger logger((state_dir + QStringLiteral("/app.log")).toStdString());
    qInfo() << "TrafficMonitor" << kVersion << "starting";
    qInfo() << "Config directory:" << config_dir;
    qInfo() << "Data directory:" << data_dir;

    const QString database_path = data_dir + QStringLiteral("/trafficmonitor.db");
    auto openDatabase = [](const QString& path) -> std::unique_ptr<Database> {
        auto database = std::make_unique<Database>(path.toStdString());
        QString error;
        if (!database->open(&error) || !database->initializeSchema(&error)) {
            qWarning() << "Cannot open traffic database" << path << error;
            return nullptr;
        }
        return database;
    };

    auto database = openDatabase(database_path);
    if (!database && QFile::exists(database_path)) {
        const QString corrupt_path = database_path + QStringLiteral(".corrupt-")
            + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
        if (QFile::rename(database_path, corrupt_path)) {
            qWarning() << "Moved corrupt database to" << corrupt_path;
        }
        database = openDatabase(database_path);
    }
    if (!database) {
        qCritical() << "Cannot initialize traffic database";
        return 1;
    }

    SettingsRepository settings_repository(database->handle());
    SettingsModel settings_model(settings_repository);
    AppSettings settings = safe_mode ? AppSettings::defaults() : settings_model.load();

    HistoryRepository history(database->handle());
    std::uint64_t last_history_up = 0;
    std::uint64_t last_history_down = 0;
    if (const auto totals = history.dailyTotals(todayLocal(), QStringLiteral("__total__"))) {
        last_history_up = totals->up_bytes;
        last_history_down = totals->down_bytes;
    }

    SystemMetricsCollector collector;
    collector.setNetworkInterface(settings.network_interface);
    collector.setDiskMountPoint(settings.disk_mount_point);
    collector.setTodayTotals(last_history_up, last_history_down);

    PluginHost plugin_host(pluginDirectory().toStdString());
    plugin_host.setConfigDir((config_dir + QStringLiteral("/plugins")).toStdString());
    if (!safe_mode) {
        plugin_host.loadAll();
        for (const auto& plugin : plugin_host.plugins()) {
            if (plugin.loaded) {
                qInfo() << "Plugin loaded:" << QString::fromStdString(plugin.pluginId)
                        << QString::fromStdString(plugin.version);
            } else {
                qWarning() << "Plugin failed:" << QString::fromStdString(plugin.path.string())
                           << QString::fromStdString(plugin.error);
            }
        }
    }

    Sampler sampler(collector);
    sampler.setSampleCallback([&plugin_host](const Snapshot& snapshot) {
        plugin_host.update(snapshot);
    });
    sampler.start(settings.refresh_interval_ms);

    if (!smoke_test) {
        const QString server_name = QStringLiteral("trafficmonitor-linux-%1").arg(::getuid());
        QLocalSocket probe;
        probe.connectToServer(server_name);
        if (probe.waitForConnected(300)) {
            probe.write("show-settings");
            probe.waitForBytesWritten(300);
            sampler.stop();
            return 0;
        }
    }

    FloatingWindow window;
    window.setSettings(settings);

    auto updatePluginItems = [&]() {
        auto items = plugin_host.allItems();
        if (!settings.gpu_enabled) {
            items.erase(std::remove_if(items.begin(), items.end(), [](const PluginItemView& item) {
                            return item.itemId.starts_with("trafficmonitor.gpu.");
                        }),
                        items.end());
        }
        window.setPluginItems(items);
    };
    updatePluginItems();
    window.show();

    TrayIcon tray;
    tray.show();

    const auto tray_warning = settings_repository.getValue(
        QStringLiteral("general.tray_warning_shown"));
    if (!smoke_test && !tray.isSystemTrayAvailable()
        && qEnvironmentVariableIsEmpty("TRAFFICMONITOR_NO_TRAY_PROMPT")
        && !(tray_warning.has_value() && tray_warning->toBool())) {
        settings_repository.setValue(QStringLiteral("general.tray_warning_shown"), true);
        QTimer::singleShot(0, &window, [&window]() {
            QMessageBox::information(
                &window, QStringLiteral("托盘不可用"),
                QStringLiteral("当前 GNOME 会话没有启用 AppIndicator 扩展。\n"
                               "请运行：gnome-extensions enable ubuntu-appindicators@ubuntu.com\n"
                               "或使用 --settings 参数打开设置。"));
        });
    }

    auto writeHistory = [&]() {
        const auto latest = sampler.latest();
        if (!latest || !latest->network.has_value()) {
            return;
        }

        const std::uint64_t up = latest->today_up_bytes;
        const std::uint64_t down = latest->today_down_bytes;
        const std::uint64_t up_delta = up >= last_history_up ? up - last_history_up : 0;
        const std::uint64_t down_delta =
            down >= last_history_down ? down - last_history_down : 0;
        if (up_delta == 0 && down_delta == 0) {
            return;
        }

        const qint64 timestamp = alignedTimestamp();
        const QString day = todayLocal();
        const QString interface =
            QString::fromStdString(latest->network->interface);
        history.appendSample(timestamp, day, interface, up_delta, down_delta);
        history.appendSample(timestamp, day, QStringLiteral("__total__"), up_delta, down_delta);
        last_history_up = up;
        last_history_down = down;
    };

    auto applySettings = [&](AppSettings new_settings) {
        settings = std::move(new_settings);
        settings_model.save(settings);
        sampler.stop();
        collector.setNetworkInterface(settings.network_interface);
        collector.setDiskMountPoint(settings.disk_mount_point);
        sampler.setInterval(settings.refresh_interval_ms);
        sampler.start(settings.refresh_interval_ms);
        window.setSettings(settings);
        updatePluginItems();
        window.show();
    };

    auto showSettings = [&]() {
        std::vector<std::string> plugin_item_ids;
        for (const auto& item : plugin_host.allItems()) {
            plugin_item_ids.push_back(item.itemId);
        }
        SettingsDialog dialog(settings, plugin_item_ids, &window);
        if (dialog.exec() == QDialog::Accepted) {
            applySettings(dialog.settings());
        }
    };

    QObject::connect(&window, &FloatingWindow::settingsRequested, &window, showSettings);
    QObject::connect(&window, &FloatingWindow::quitRequested, &app, &QCoreApplication::quit);
    QObject::connect(&window, &FloatingWindow::settingsChanged, &window, [&]() {
        settings = window.settings();
        settings_model.save(settings);
    });
    QObject::connect(&window, &FloatingWindow::geometryChanged, &window, [&](const QRect& rect) {
        settings.geometry = WindowGeometry{rect.x(), rect.y(), rect.width(), rect.height()};
        settings_model.save(settings);
    });

    QObject::connect(&tray, &TrayIcon::showHideRequested, &window,
                     [&window]() { window.setVisible(!window.isVisible()); });
    QObject::connect(&tray, &TrayIcon::settingsRequested, &window, showSettings);
    QObject::connect(&tray, &TrayIcon::quitRequested, &app, &QCoreApplication::quit);

    QTimer ui_timer;
    std::uint64_t last_sequence = 0;
    QObject::connect(&ui_timer, &QTimer::timeout, &app, [&]() {
        const auto latest = sampler.latest();
        if (!latest || latest->sample_sequence == last_sequence) {
            return;
        }
        last_sequence = latest->sample_sequence;
        plugin_host.refreshItemValues();
        updatePluginItems();
        window.setSnapshot(*latest);
        tray.setTooltip(tooltipText(*latest));
    });
    ui_timer.start(200);

    QTimer history_timer;
    QObject::connect(&history_timer, &QTimer::timeout, &app, writeHistory);
    history_timer.start(60000);

    QLocalServer* single_instance_server = nullptr;
    if (!smoke_test) {
        const QString server_name = QStringLiteral("trafficmonitor-linux-%1").arg(::getuid());
        QLocalServer::removeServer(server_name);
        auto* server = new QLocalServer(&app);
        if (server->listen(server_name)) {
            single_instance_server = server;
            QObject::connect(server, &QLocalServer::newConnection, &app, [&, server]() {
                while (server->hasPendingConnections()) {
                    QLocalSocket* socket = server->nextPendingConnection();
                    QObject::connect(socket, &QLocalSocket::readyRead, &app,
                                     [&, socket]() {
                                         const QByteArray message = socket->readAll();
                                         if (message.contains("show-settings")) {
                                             QTimer::singleShot(0, &window, showSettings);
                                         } else if (message.contains("quit")) {
                                             QCoreApplication::quit();
                                         }
                                         socket->disconnectFromServer();
                                     });
                }
            });
        }
    }

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [&]() {
        writeHistory();
        sampler.stop();
        settings_model.save(settings);
        tray.hide();
        if (single_instance_server != nullptr) {
            single_instance_server->close();
        }
    });

    if (open_settings) {
        QTimer::singleShot(0, &window, showSettings);
    }

    if (smoke_test) {
        QTimer::singleShot(1000, &app, [&]() {
            QTextStream(stdout) << "SMOKE_OK\n";
            app.quit();
        });
    }

    const int result = app.exec();
    return result;
}
