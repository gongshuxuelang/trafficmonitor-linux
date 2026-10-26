#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QSpinBox>
#include <QTest>

#include "ui/settings_dialog.h"

using namespace tm_linux;

class SettingsDialogTest : public QObject {
    Q_OBJECT

private slots:
    void roundTripsInitialSettings()
    {
        AppSettings settings = AppSettings::defaults();
        settings.refresh_interval_ms = 1500;
        settings.opacity = 75;
        settings.always_on_top = false;
        settings.network_interface = "auto";
        settings.disk_mount_point = "/home";
        settings.unit_mode = UnitMode::Kb;
        settings.unit_base = UnitBase::Bit;

        SettingsDialog dialog(settings, {"trafficmonitor.gpu.usage"});
        const AppSettings result = dialog.settings();

        QCOMPARE(result.refresh_interval_ms, 1500);
        QCOMPARE(result.opacity, 75);
        QCOMPARE(result.always_on_top, false);
        QCOMPARE(result.disk_mount_point, std::string("/home"));
        QCOMPARE(result.unit_mode, UnitMode::Kb);
        QCOMPARE(result.unit_base, UnitBase::Bit);
        QVERIFY(!result.display_items.empty());
    }

    void readsEditedWidgets()
    {
        AppSettings settings = AppSettings::defaults();
        SettingsDialog dialog(settings, {});

        auto* refresh = dialog.findChild<QSpinBox*>(QStringLiteral("refresh_interval"));
        auto* opacity = dialog.findChild<QSpinBox*>(QStringLiteral("opacity"));
        auto* mount = dialog.findChild<QLineEdit*>(QStringLiteral("disk_mount_point"));
        auto* unit_mode = dialog.findChild<QComboBox*>(QStringLiteral("unit_mode"));
        auto* items = dialog.findChild<QListWidget*>(QStringLiteral("display_items"));
        QVERIFY(refresh);
        QVERIFY(opacity);
        QVERIFY(mount);
        QVERIFY(unit_mode);
        QVERIFY(items);

        refresh->setValue(2500);
        opacity->setValue(60);
        mount->setText(QStringLiteral("/var"));
        unit_mode->setCurrentIndex(2); // MB/s
        QVERIFY(items->count() > 0);
        items->item(0)->setCheckState(Qt::Unchecked);

        const AppSettings result = dialog.settings();
        QCOMPARE(result.refresh_interval_ms, 2500);
        QCOMPARE(result.opacity, 60);
        QCOMPARE(result.disk_mount_point, std::string("/var"));
        QCOMPARE(result.unit_mode, UnitMode::Mb);
        QCOMPARE(result.display_items.front().visible, false);
    }
};

QTEST_MAIN(SettingsDialogTest)

#include "test_settings_dialog.moc"
