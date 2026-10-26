#pragma once

#include <QObject>
#include <QString>

class QMenu;
class QSystemTrayIcon;

namespace tm_linux {

class TrayIcon : public QObject {
    Q_OBJECT

public:
    explicit TrayIcon(QObject* parent = nullptr);
    ~TrayIcon() override;

    void show();
    void hide();
    void setTooltip(const QString& text);
    bool isSystemTrayAvailable() const;
    QMenu* menu() const;

signals:
    void showHideRequested();
    void settingsRequested();
    void quitRequested();

private:
    QSystemTrayIcon* tray_{};
    QMenu* menu_{};
};

} // namespace tm_linux
