#pragma once

#include <QDialog>

#include <string>
#include <vector>

#include "tm/app_settings.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QSpinBox;

namespace tm_linux {

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(const AppSettings& settings,
                            const std::vector<std::string>& plugin_item_ids,
                            QWidget* parent = nullptr);

    AppSettings settings() const;

private:
    AppSettings initial_;
    std::vector<std::string> plugin_item_ids_;

    QSpinBox* refresh_interval_{};
    QSpinBox* opacity_{};
    QCheckBox* always_on_top_{};
    QCheckBox* locked_{};
    QCheckBox* click_through_{};
    QCheckBox* gpu_enabled_{};
    QComboBox* network_interface_{};
    QComboBox* unit_mode_{};
    QComboBox* unit_base_{};
    QLineEdit* disk_mount_point_{};
    QListWidget* display_items_{};
};

} // namespace tm_linux
