#include <QAction>
#include <QMenu>
#include <QSignalSpy>
#include <QTest>

#include "ui/tray_icon.h"

using namespace tm_linux;

class TrayIconTest : public QObject {
    Q_OBJECT

private slots:
    void exposesMenuActions()
    {
        TrayIcon tray;
        QVERIFY(tray.menu() != nullptr);
        QCOMPARE(tray.menu()->actions().size(), 4);
    }

    void emitsSignalsFromActions()
    {
        TrayIcon tray;
        QVERIFY(tray.menu() != nullptr);
        QSignalSpy show_hide_spy(&tray, &TrayIcon::showHideRequested);
        QSignalSpy settings_spy(&tray, &TrayIcon::settingsRequested);
        QSignalSpy quit_spy(&tray, &TrayIcon::quitRequested);

        tray.menu()->actions()[0]->trigger();
        tray.menu()->actions()[1]->trigger();
        tray.menu()->actions()[3]->trigger();

        QCOMPARE(show_hide_spy.count(), 1);
        QCOMPARE(settings_spy.count(), 1);
        QCOMPARE(quit_spy.count(), 1);
    }
};

QTEST_MAIN(TrayIconTest)

#include "test_tray_icon.moc"
