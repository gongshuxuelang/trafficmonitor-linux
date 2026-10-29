#ifndef TRAFFICMONITOR_LINUX_SRC_UI_SETTINGS_DIALOG_H_
#define TRAFFICMONITOR_LINUX_SRC_UI_SETTINGS_DIALOG_H_

#include "pch.h"

#include "common/DTO/app_settings.h"

namespace Ui {
class SettingsDialog;
}

namespace tm_linux {

/**
 * @brief 设置对话框，编辑刷新间隔、外观、网络接口与显示项。
 */
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief 使用当前设置和插件条目构建对话框。
     * @param settings 当前设置。
     * @param plugin_item_ids 插件提供的显示项 ID 列表。
     * @param parent 父窗口。
     */
    explicit SettingsDialog(const AppSettings& settings,
                            const std::vector<std::string>& plugin_item_ids,
                            QWidget* parent = nullptr);

    /**
     * @brief 释放界面对象。
     */
    ~SettingsDialog() override;

    /**
     * @brief 读取控件内容生成新的设置。
     * @return 修改后的设置；未修改的字段沿用原值。
     */
    AppSettings settings() const;

private:
    AppSettings initial_;
    std::unique_ptr<Ui::SettingsDialog> ui_;
};

} // namespace tm_linux

#endif  // TRAFFICMONITOR_LINUX_SRC_UI_SETTINGS_DIALOG_H_
