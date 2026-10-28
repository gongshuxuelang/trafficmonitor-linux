#include "ui/settings_dialog.h"
#include "ui_settings_dialog.h"

namespace tm_linux {

namespace {

/**
 * @brief 返回显示项的中文标签。
 * @param id 显示项 ID。
 * @return 标签文本；未知 ID 原样返回。
 */
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

/**
 * @brief 枚举 /sys/class/net 下除 lo 以外的网络接口。
 * @return 排序后的接口名列表。
 */
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
    , ui_(std::make_unique<Ui::SettingsDialog>())
{
    ui_->setupUi(this);

    ui_->refresh_interval->setValue(initial_.refresh_interval_ms);
    ui_->opacity->setValue(initial_.opacity);
    ui_->always_on_top->setChecked(initial_.always_on_top);
    ui_->locked->setChecked(initial_.locked);
    ui_->click_through->setChecked(initial_.click_through);

    // uic 不会把 QComboBox 项的 userData 写进生成的代码，这里补上“自动”的取值。
    ui_->network_interface->setItemData(0, QStringLiteral("auto"));
    for (const QString& interface : networkInterfaces()) {
        ui_->network_interface->addItem(interface, interface);
    }
    int network_index =
        ui_->network_interface->findData(QString::fromStdString(initial_.network_interface));
    if (network_index < 0) {
        ui_->network_interface->addItem(QString::fromStdString(initial_.network_interface),
                                        QString::fromStdString(initial_.network_interface));
        network_index = ui_->network_interface->count() - 1;
    }
    ui_->network_interface->setCurrentIndex(network_index);

    ui_->disk_mount_point->setText(QString::fromStdString(initial_.disk_mount_point));
    ui_->unit_mode->setCurrentIndex(static_cast<int>(initial_.unit_mode));
    ui_->unit_base->setCurrentIndex(initial_.unit_base == UnitBase::Bit ? 1 : 0);
    ui_->gpu_enabled->setChecked(initial_.gpu_enabled);

    std::set<std::string> seen;
    for (const auto& item : initial_.display_items) {
        seen.insert(item.id);
        auto* list_item = new QListWidgetItem(itemLabel(item.id), ui_->display_items);
        list_item->setFlags(list_item->flags() | Qt::ItemIsUserCheckable);
        list_item->setData(Qt::UserRole, QString::fromStdString(item.id));
        list_item->setCheckState(item.visible ? Qt::Checked : Qt::Unchecked);
    }
    for (const auto& id : plugin_item_ids) {
        if (!seen.insert(id).second) {
            continue;
        }
        auto* list_item = new QListWidgetItem(itemLabel(id), ui_->display_items);
        list_item->setFlags(list_item->flags() | Qt::ItemIsUserCheckable);
        list_item->setData(Qt::UserRole, QString::fromStdString(id));
        list_item->setCheckState(Qt::Checked);
    }

    ui_->main_layout->setStretch(2, 1);

    connect(ui_->button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui_->button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

SettingsDialog::~SettingsDialog() = default;

AppSettings SettingsDialog::settings() const
{
    AppSettings result = initial_;

    result.refresh_interval_ms = ui_->refresh_interval->value();
    result.opacity = ui_->opacity->value();
    result.always_on_top = ui_->always_on_top->isChecked();
    result.locked = ui_->locked->isChecked();
    result.click_through = ui_->click_through->isChecked();
    result.network_interface = ui_->network_interface->currentData().toString().toStdString();
    result.disk_mount_point = ui_->disk_mount_point->text().toStdString();
    switch (ui_->unit_mode->currentIndex()) {
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
    result.unit_base = ui_->unit_base->currentIndex() == 1 ? UnitBase::Bit : UnitBase::Byte;
    result.gpu_enabled = ui_->gpu_enabled->isChecked();

    std::vector<DisplayItemSetting> items;
    for (int index = 0; index < ui_->display_items->count(); ++index) {
        const QListWidgetItem* item = ui_->display_items->item(index);
        const std::string id = item->data(Qt::UserRole).toString().toStdString();
        if (!id.empty()) {
            items.push_back(DisplayItemSetting{id, item->checkState() == Qt::Checked});
        }
    }
    if (!items.empty()) {
        result.display_items = std::move(items);
    }

    return result;
}

} // namespace tm_linux
