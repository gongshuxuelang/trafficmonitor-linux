#ifndef TRAFFICMONITOR_LINUX_SRC_UI_FLOATING_WINDOW_H_
#define TRAFFICMONITOR_LINUX_SRC_UI_FLOATING_WINDOW_H_

#include "pch.h"

#include "common/DTO/app_settings.h"
#include "common/DTO/metrics.h"
#include "core/plugin/plugin_host.h"

namespace Ui {
class FloatingWindow;
}

namespace tm_linux {

/**
 * @brief 无边框置顶悬浮窗，绘制网络、CPU、内存等指标行。
 */
class FloatingWindow : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief 创建悬浮窗，并使用默认设置初始化。
     * @param parent 父窗口。
     */
    explicit FloatingWindow(QWidget* parent = nullptr);

    /**
     * @brief 释放界面对象。
     */
    ~FloatingWindow() override;

    /**
     * @brief 应用设置（置顶、透明度、点击穿透、几何位置）并重建显示行。
     * @param settings 新设置。
     */
    void setSettings(const AppSettings& settings);

    /**
     * @brief 更新最新指标快照并重绘。
     * @param snapshot 指标快照。
     */
    void setSnapshot(const Snapshot& snapshot);

    /**
     * @brief 更新插件条目并重绘。
     * @param items 插件条目列表。
     */
    void setPluginItems(const std::vector<PluginItemView>& items);

    /**
     * @brief 获取当前设置。
     * @return 设置引用。
     */
    const AppSettings& settings() const;

    /**
     * @brief 获取最近一次指标快照。
     * @return 快照引用。
     */
    const Snapshot& snapshot() const;

    /**
     * @brief 获取当前渲染的文本行，主要用于测试断言。
     * @return 行文本列表。
     */
    const QStringList& renderedLines() const;

signals:
    /**
     * @brief 用户请求打开设置对话框。
     */
    void settingsRequested();

    /**
     * @brief 用户请求退出程序。
     */
    void quitRequested();

    /**
     * @brief 上下文菜单修改了窗口设置。
     */
    void settingsChanged();

    /**
     * @brief 拖动结束后窗口位置或尺寸发生变化。
     * @param geometry 新的几何区域。
     */
    void geometryChanged(const QRect& geometry);

protected:
    /**
     * @brief 绘制圆角半透明背景与指标文本。
     * @param event 绘制事件。
     */
    void paintEvent(QPaintEvent* event) override;

    /**
     * @brief 左键按下且窗口未锁定、未开启点击穿透时开始拖动。
     * @param event 鼠标事件。
     */
    void mousePressEvent(QMouseEvent* event) override;

    /**
     * @brief 拖动过程中移动窗口。
     * @param event 鼠标事件。
     */
    void mouseMoveEvent(QMouseEvent* event) override;

    /**
     * @brief 结束拖动并发出 geometryChanged。
     * @param event 鼠标事件。
     */
    void mouseReleaseEvent(QMouseEvent* event) override;

    /**
     * @brief 显示右键菜单并处理设置、锁定、穿透、置顶与退出操作。
     * @param event 上下文菜单事件。
     */
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    /**
     * @brief 根据设置与快照生成要显示的文本行。
     */
    void rebuildLines();

    AppSettings settings_;
    Snapshot snapshot_;
    std::vector<PluginItemView> plugin_items_;
    QStringList lines_;
    QPoint drag_offset_;
    bool dragging_{};
    std::unique_ptr<Ui::FloatingWindow> ui_;
};

} // namespace tm_linux

#endif  // TRAFFICMONITOR_LINUX_SRC_UI_FLOATING_WINDOW_H_
