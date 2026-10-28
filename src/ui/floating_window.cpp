#include "ui/floating_window.h"
#include "ui_floating_window.h"

namespace tm_linux {

namespace {

/**
 * @brief 将 std::string 转换为 QString。
 * @param value 输入字符串。
 * @return 转换后的文本。
 */
QString toQString(const std::string& value)
{
    return QString::fromStdString(value);
}

} // namespace

FloatingWindow::FloatingWindow(QWidget* parent)
    : QWidget(parent)
    , ui_(std::make_unique<Ui::FloatingWindow>())
{
    ui_->setupUi(this);

    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    settings_ = AppSettings::defaults();
    resize(settings_.geometry.width, settings_.geometry.height);
}

FloatingWindow::~FloatingWindow() = default;

void FloatingWindow::setSettings(const AppSettings& settings)
{
    settings_ = settings;

    if (windowFlags().testFlag(Qt::WindowStaysOnTopHint) != settings_.always_on_top) {
        setWindowFlag(Qt::WindowStaysOnTopHint, settings_.always_on_top);
    }
    setAttribute(Qt::WA_TransparentForMouseEvents, settings_.click_through);
    setWindowOpacity(std::clamp(settings_.opacity, 20, 100) / 100.0);

    if (settings_.geometry.width > 0 && settings_.geometry.height > 0) {
        setGeometry(settings_.geometry.x, settings_.geometry.y, settings_.geometry.width,
                    settings_.geometry.height);
    }

    rebuildLines();
    update();
}

void FloatingWindow::setSnapshot(const Snapshot& snapshot)
{
    snapshot_ = snapshot;
    rebuildLines();
    update();
}

void FloatingWindow::setPluginItems(const std::vector<PluginItemView>& items)
{
    plugin_items_ = items;
    rebuildLines();
    update();
}

const AppSettings& FloatingWindow::settings() const
{
    return settings_;
}

const Snapshot& FloatingWindow::snapshot() const
{
    return snapshot_;
}

const QStringList& FloatingWindow::renderedLines() const
{
    return lines_;
}

void FloatingWindow::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF background = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    path.addRoundedRect(background, 8.0, 8.0);
    painter.fillPath(path, QColor(18, 18, 18, 210));
    painter.setPen(QPen(QColor(90, 90, 90, 160), 1.0));
    painter.drawPath(path);

    if (lines_.isEmpty()) {
        return;
    }

    QFont font = painter.font();
    font.setPointSize(9);
    painter.setFont(font);

    const QFontMetrics metrics(font);
    const int padding = 8;
    const int line_height = metrics.height() + 2;
    const int columns = lines_.size() > 5 ? 2 : 1;
    const int rows = (lines_.size() + columns - 1) / columns;
    const int column_width = (width() - padding * (columns + 1)) / columns;

    painter.setPen(QColor(235, 235, 235));
    for (int index = 0; index < lines_.size(); ++index) {
        const int column = index / rows;
        const int row = index % rows;
        const QRect text_rect(padding + column * (column_width + padding),
                              padding + row * line_height, column_width, line_height);
        painter.drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter, lines_[index]);
    }
}

void FloatingWindow::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !settings_.locked && !settings_.click_through) {
        dragging_ = true;
        drag_offset_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void FloatingWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (dragging_) {
        move(event->globalPosition().toPoint() - drag_offset_);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void FloatingWindow::mouseReleaseEvent(QMouseEvent* event)
{
    if (dragging_) {
        dragging_ = false;
        emit geometryChanged(geometry());
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void FloatingWindow::contextMenuEvent(QContextMenuEvent* event)
{
    ui_->action_locked->setChecked(settings_.locked);
    ui_->action_click_through->setChecked(settings_.click_through);
    ui_->action_always_on_top->setChecked(settings_.always_on_top);

    QAction* chosen = ui_->context_menu->exec(event->globalPos());
    if (chosen == ui_->action_settings) {
        emit settingsRequested();
        return;
    }
    if (chosen == ui_->action_locked) {
        settings_.locked = ui_->action_locked->isChecked();
        setAttribute(Qt::WA_TransparentForMouseEvents, settings_.click_through);
        emit settingsChanged();
        return;
    }
    if (chosen == ui_->action_click_through) {
        settings_.click_through = ui_->action_click_through->isChecked();
        setAttribute(Qt::WA_TransparentForMouseEvents, settings_.click_through);
        emit settingsChanged();
        return;
    }
    if (chosen == ui_->action_always_on_top) {
        settings_.always_on_top = ui_->action_always_on_top->isChecked();
        setWindowFlag(Qt::WindowStaysOnTopHint, settings_.always_on_top);
        show();
        emit settingsChanged();
        return;
    }
    if (chosen == ui_->action_quit) {
        emit quitRequested();
    }
}

void FloatingWindow::rebuildLines()
{
    lines_.clear();

    auto addLine = [this](const QString& label, const QString& value) {
        lines_.append(label.isEmpty() ? value : label + QLatin1Char(' ') + value);
    };

    for (const auto& item : settings_.display_items) {
        if (!item.visible) {
            continue;
        }

        const std::string& id = item.id;
        if (id == "net.up") {
            const QString value = snapshot_.network.has_value()
                ? toQString(formatBytesPerSecond(snapshot_.network->tx_bytes_per_sec,
                                                 settings_.unit_mode, settings_.unit_base))
                : QStringLiteral("--");
            addLine(QStringLiteral("↑"), value);
        } else if (id == "net.down") {
            const QString value = snapshot_.network.has_value()
                ? toQString(formatBytesPerSecond(snapshot_.network->rx_bytes_per_sec,
                                                 settings_.unit_mode, settings_.unit_base))
                : QStringLiteral("--");
            addLine(QStringLiteral("↓"), value);
        } else if (id == "cpu.usage") {
            addLine(QStringLiteral("CPU"), toQString(formatPercent(snapshot_.cpu.usage_percent)));
        } else if (id == "mem.usage") {
            const QString value = snapshot_.memory.has_value()
                ? toQString(formatPercent(snapshot_.memory->used_percent))
                : QStringLiteral("--");
            addLine(QStringLiteral("MEM"), value);
        } else if (id == "disk.io") {
            const QString value = snapshot_.disk.has_value() && snapshot_.disk->io_available
                ? toQString(formatPercent(snapshot_.disk->io_util_percent))
                : QStringLiteral("--");
            addLine(QStringLiteral("DISK"), value);
        } else if (id == "cpu.temp") {
            addLine(QStringLiteral("TEMP"),
                    toQString(formatTemperature(snapshot_.temperature.cpu_c)));
        } else if (id == "cpu.freq") {
            const QString value = snapshot_.cpu.frequency_mhz > 0.0
                ? toQString(formatFrequencyMHz(snapshot_.cpu.frequency_mhz))
                : QStringLiteral("--");
            addLine(QStringLiteral("FREQ"), value);
        } else if (id == "today.up") {
            addLine(QStringLiteral("今日↑"), toQString(formatBytes(snapshot_.today_up_bytes)));
        } else if (id == "today.down") {
            addLine(QStringLiteral("今日↓"), toQString(formatBytes(snapshot_.today_down_bytes)));
        } else {
            const auto plugin = std::find_if(plugin_items_.begin(), plugin_items_.end(),
                                             [&id](const PluginItemView& item) {
                                                 return item.itemId == id;
                                             });
            if (plugin == plugin_items_.end()) {
                continue;
            }
            addLine(toQString(plugin->labelText), toQString(plugin->value));
        }
    }
}

} // namespace tm_linux
