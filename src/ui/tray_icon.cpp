#include "ui/tray_icon.h"
#include "ui_tray_icon.h"

#include <QAction>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSystemTrayIcon>

namespace tm_linux {

namespace {

/**
 * @brief 生成主题图标不可用时的占位图标。
 * @return 绘制有 "TM" 字样的圆角图标。
 */
QIcon fallbackIcon()
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(30, 120, 200));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(QRectF(1, 1, 30, 30), 6, 6);
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(9);
    painter.setFont(font);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, QStringLiteral("TM"));
    return QIcon(pixmap);
}

} // namespace

TrayIcon::TrayIcon(QObject* parent)
    : QObject(parent)
    , ui_(std::make_unique<Ui::TrayMenu>())
{
    QIcon icon = QIcon::fromTheme(QStringLiteral("utilities-system-monitor"));
    if (icon.isNull()) {
        icon = fallbackIcon();
    }

    tray_ = new QSystemTrayIcon(icon, this);
    menu_ = new QMenu();
    ui_->setupUi(menu_);

    connect(ui_->action_show_hide, &QAction::triggered, this, &TrayIcon::showHideRequested);
    connect(ui_->action_settings, &QAction::triggered, this, &TrayIcon::settingsRequested);
    connect(ui_->action_quit, &QAction::triggered, this, &TrayIcon::quitRequested);

    tray_->setContextMenu(menu_);
    tray_->setToolTip(QStringLiteral("TrafficMonitor"));

    connect(tray_, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger
                    || reason == QSystemTrayIcon::DoubleClick) {
                    emit showHideRequested();
                }
            });
}

TrayIcon::~TrayIcon()
{
    if (tray_ != nullptr) {
        tray_->hide();
    }
    delete menu_;
    menu_ = nullptr;
}

void TrayIcon::show()
{
    if (tray_ == nullptr) {
        return;
    }
    tray_->show();
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning("TrafficMonitor: system tray is not available; enable the AppIndicator "
                 "extension with: gnome-extensions enable ubuntu-appindicators@ubuntu.com");
    }
}

void TrayIcon::hide()
{
    if (tray_ != nullptr) {
        tray_->hide();
    }
}

void TrayIcon::setTooltip(const QString& text)
{
    if (tray_ != nullptr) {
        tray_->setToolTip(text);
    }
}

bool TrayIcon::isSystemTrayAvailable() const
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

QMenu* TrayIcon::menu() const
{
    return menu_;
}

} // namespace tm_linux
