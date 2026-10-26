#include "ui/settings_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <set>
#include <utility>

namespace tm_linux {

namespace {

QString itemLabel(const std::string& id)
{
    if (id == "net.up") return QStringLiteral("上传速度");
    if (id == "net.down") return QStringLiteral("下载速度");
    if (id == "cpu.usage") return QStringLiteral("CPU 使用率");
    if (id == "mem.usage") return QStringLiteral("内存使用率");
    if (id == "disk.io") return QStringLiteral("磁盘 I/O");
    if (id == "cpu.temp") return QStringLiteral("CPU 温度");
    if (id == "cpu.freq") return QStringLiteral("CPU 频率");
    if (id == "today.up") return QStringLiteral("今日上传");
    if (id == "today.down") return QStringLiteral("今日下载");
    return QString::fromStdString(id);
}

QStringList networkInterfaces()
{
    QStringList result;
    const QStringList entries =
        QDir(QStringLiteral("/sys/class/net")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& entry : entries) {
        if (entry != QStringLiteral("lo")) {
            result.push_back(entry);
        }
    }
    result.sort();
    return result;
}

} // namespace

SettingsDialog::SettingsDialog(const AppSettings& settings,
                               const std::vector<std::string>& plugin_item_ids, QWidget* parent)
    : QDialog(parent)
    , initial_(settings)
    , plugin_item_ids_(plugin_item_ids)
{
    setWindowTitle(QStringLiteral("TrafficMonitor 设置"));
    resize(440, 560);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    refresh_interval_ = new QSpinBox(this);
    refresh_interval_->setObjectName(QStringLiteral("refresh_interval"));
    refresh_interval_->setRange(200, 5000);
    refresh_interval_->setSuffix(QStringLiteral(" ms"));
    refresh_interval_->setValue(initial_.refresh_interval_ms);
    form->addRow(QStringLiteral("刷新间隔"), refresh_interval_);

    opacity_ = new QSpinBox(this);
    opacity_->setObjectName(QStringLiteral("opacity"));
    opacity_->setRange(20, 100);
    opacity_->setSuffix(QStringLiteral(" %"));
    opacity_->setValue(initial_.opacity);
    form->addRow(QStringLiteral("不透明度"), opacity_);

    always_on_top_ = new QCheckBox(QStringLiteral("总是置顶"), this);
    always_on_top_->setObjectName(QStringLiteral("always_on_top"));
    always_on_top_->setChecked(initial_.always_on_top);
    form->addRow(QString(), always_on_top_);

    locked_ = new QCheckBox(QStringLiteral("锁定位置"), this);
    locked_->setObjectName(QStringLiteral("locked"));
    locked_->setChecked(initial_.locked);
    form->addRow(QString(), locked_);

    click_through_ = new QCheckBox(QStringLiteral("鼠标穿透"), this);
    click_through_->setObjectName(QStringLiteral("click_through"));
    click_through_->setChecked(initial_.click_through);
    form->addRow(QString(), click_through_);

    network_interface_ = new QComboBox(this);
    network_interface_->setObjectName(QStringLiteral("network_interface"));
    network_interface_->addItem(QStringLiteral("自动"), QStringLiteral("auto"));
    for (const QString& interface : networkInterfaces()) {
        network_interface_->addItem(interface, interface);
    }
    int network_index =
        network_interface_->findData(QString::fromStdString(initial_.network_interface));
    if (network_index < 0) {
        network_interface_->addItem(QString::fromStdString(initial_.network_interface),
                                    QString::fromStdString(initial_.network_interface));
        network_index = network_interface_->count() - 1;
    }
    network_interface_->setCurrentIndex(network_index);
    form->addRow(QStringLiteral("网络接口"), network_interface_);

    disk_mount_point_ = new QLineEdit(QString::fromStdString(initial_.disk_mount_point), this);
    disk_mount_point_->setObjectName(QStringLiteral("disk_mount_point"));
    form->addRow(QStringLiteral("磁盘挂载点"), disk_mount_point_);

    unit_mode_ = new QComboBox(this);
    unit_mode_->setObjectName(QStringLiteral("unit_mode"));
    unit_mode_->addItem(QStringLiteral("自动"), 0);
    unit_mode_->addItem(QStringLiteral("KB/s"), 1);
    unit_mode_->addItem(QStringLiteral("MB/s"), 2);
    unit_mode_->setCurrentIndex(static_cast<int>(initial_.unit_mode));
    form->addRow(QStringLiteral("网速单位"), unit_mode_);

    unit_base_ = new QComboBox(this);
    unit_base_->setObjectName(QStringLiteral("unit_base"));
    unit_base_->addItem(QStringLiteral("字节 (B)"), 0);
    unit_base_->addItem(QStringLiteral("比特 (b)"), 1);
    unit_base_->setCurrentIndex(initial_.unit_base == UnitBase::Bit ? 1 : 0);
    form->addRow(QStringLiteral("字节/比特"), unit_base_);

    gpu_enabled_ = new QCheckBox(QStringLiteral("启用 GPU 插件"), this);
    gpu_enabled_->setObjectName(QStringLiteral("gpu_enabled"));
    gpu_enabled_->setChecked(initial_.gpu_enabled);
    form->addRow(QString(), gpu_enabled_);

    layout->addLayout(form);

    auto* items_label = new QLabel(QStringLiteral("显示项目"), this);
    layout->addWidget(items_label);

    display_items_ = new QListWidget(this);
    display_items_->setObjectName(QStringLiteral("display_items"));
    std::set<std::string> seen;
    for (const auto& item : initial_.display_items) {
        seen.insert(item.id);
        auto* list_item = new QListWidgetItem(itemLabel(item.id), display_items_);
        list_item->setFlags(list_item->flags() | Qt::ItemIsUserCheckable);
        list_item->setData(Qt::UserRole, QString::fromStdString(item.id));
        list_item->setCheckState(item.visible ? Qt::Checked : Qt::Unchecked);
    }
    for (const auto& id : plugin_item_ids_) {
        if (!seen.insert(id).second) {
            continue;
        }
        auto* list_item = new QListWidgetItem(itemLabel(id), display_items_);
        list_item->setFlags(list_item->flags() | Qt::ItemIsUserCheckable);
        list_item->setData(Qt::UserRole, QString::fromStdString(id));
        list_item->setCheckState(Qt::Checked);
    }
    layout->addWidget(display_items_, 1);

    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

AppSettings SettingsDialog::settings() const
{
    AppSettings result = initial_;

    if (refresh_interval_ != nullptr) {
        result.refresh_interval_ms = refresh_interval_->value();
    }
    if (opacity_ != nullptr) {
        result.opacity = opacity_->value();
    }
    if (always_on_top_ != nullptr) {
        result.always_on_top = always_on_top_->isChecked();
    }
    if (locked_ != nullptr) {
        result.locked = locked_->isChecked();
    }
    if (click_through_ != nullptr) {
        result.click_through = click_through_->isChecked();
    }
    if (network_interface_ != nullptr) {
        result.network_interface = network_interface_->currentData().toString().toStdString();
    }
    if (disk_mount_point_ != nullptr) {
        result.disk_mount_point = disk_mount_point_->text().toStdString();
    }
    if (unit_mode_ != nullptr) {
        switch (unit_mode_->currentData().toInt()) {
        case 1:
            result.unit_mode = UnitMode::Kb;
            break;
        case 2:
            result.unit_mode = UnitMode::Mb;
            break;
        default:
            result.unit_mode = UnitMode::Auto;
            break;
        }
    }
    if (unit_base_ != nullptr) {
        result.unit_base =
            unit_base_->currentData().toInt() == 1 ? UnitBase::Bit : UnitBase::Byte;
    }
    if (gpu_enabled_ != nullptr) {
        result.gpu_enabled = gpu_enabled_->isChecked();
    }
    if (display_items_ != nullptr) {
        std::vector<DisplayItemSetting> items;
        for (int index = 0; index < display_items_->count(); ++index) {
            const QListWidgetItem* item = display_items_->item(index);
            const std::string id = item->data(Qt::UserRole).toString().toStdString();
            if (!id.empty()) {
                items.push_back(
                    DisplayItemSetting{id, item->checkState() == Qt::Checked});
            }
        }
        if (!items.empty()) {
            result.display_items = std::move(items);
        }
    }

    return result;
}

} // namespace tm_linux
