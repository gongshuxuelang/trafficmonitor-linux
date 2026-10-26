#pragma once

#include <QPoint>
#include <QStringList>
#include <QWidget>

#include <vector>

#include "tm/app_settings.h"
#include "tm/metrics.h"
#include "tm/plugin_host.h"

namespace tm_linux {

class FloatingWindow : public QWidget {
    Q_OBJECT

public:
    explicit FloatingWindow(QWidget* parent = nullptr);

    void setSettings(const AppSettings& settings);
    void setSnapshot(const Snapshot& snapshot);
    void setPluginItems(const std::vector<PluginItemView>& items);

    const AppSettings& settings() const;
    const Snapshot& snapshot() const;
    const QStringList& renderedLines() const;

signals:
    void settingsRequested();
    void quitRequested();
    void settingsChanged();
    void geometryChanged(const QRect& geometry);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void rebuildLines();

    AppSettings settings_;
    Snapshot snapshot_;
    std::vector<PluginItemView> plugin_items_;
    QStringList lines_;
    QPoint drag_offset_;
    bool dragging_{};
};

} // namespace tm_linux
