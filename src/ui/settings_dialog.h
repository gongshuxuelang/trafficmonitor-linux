#pragma once

#include <QDialog>

#include <memory>
#include <string>
#include <vector>

#include "tm/app_settings.h"

namespace Ui {
class SettingsDialog;
}

namespace tm_linux {

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(const AppSettings& settings,
                            const std::vector<std::string>& plugin_item_ids,
                            QWidget* parent = nullptr);
    ~SettingsDialog() override;

    AppSettings settings() const;

private:
    AppSettings initial_;
    std::unique_ptr<Ui::SettingsDialog> ui_;
};

} // namespace tm_linux
