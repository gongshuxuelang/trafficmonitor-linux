#ifndef TRAFFICMONITOR_LINUX_SRC_UI_TRAY_ICON_H_
#define TRAFFICMONITOR_LINUX_SRC_UI_TRAY_ICON_H_

#include "pch.h"

class QMenu;
class QSystemTrayIcon;

namespace Ui {
class TrayMenu;
}

namespace tm_linux {

/**
 * @brief 系统托盘图标及其右键菜单。
 */
class TrayIcon : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 创建托盘图标、菜单并连接菜单动作。
     * @param parent 父对象。
     */
    explicit TrayIcon(QObject* parent = nullptr);

    /**
     * @brief 隐藏托盘图标并释放菜单。
     */
    ~TrayIcon() override;

    /**
     * @brief 显示托盘图标；托盘不可用时输出警告。
     */
    void show();

    /**
     * @brief 隐藏托盘图标。
     */
    void hide();

    /**
     * @brief 设置托盘提示文本。
     * @param text 提示文本。
     */
    void setTooltip(const QString& text);

    /**
     * @brief 查询当前桌面是否提供系统托盘。
     * @return 可用返回 true。
     */
    bool isSystemTrayAvailable() const;

    /**
     * @brief 获取托盘右键菜单。
     * @return 菜单指针。
     */
    QMenu* menu() const;

signals:
    /**
     * @brief 用户请求显示或隐藏悬浮窗。
     */
    void showHideRequested();

    /**
     * @brief 用户请求打开设置对话框。
     */
    void settingsRequested();

    /**
     * @brief 用户请求退出程序。
     */
    void quitRequested();

private:
    QSystemTrayIcon* tray_{};
    QMenu* menu_{};
    std::unique_ptr<Ui::TrayMenu> ui_;
};

} // namespace tm_linux

#endif  // TRAFFICMONITOR_LINUX_SRC_UI_TRAY_ICON_H_
