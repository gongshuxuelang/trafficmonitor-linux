# P0 技术原型状态

日期：2026-10-26  
机器：Ubuntu 26.04, GNOME Shell 50.1, Wayland 会话, Qt 6.10.2, i7-12800H, 14.8 GiB RAM, NVIDIA RTX A1000

## 构建与测试

```text
Debug:   18/18 tests passed
Release: 18/18 tests passed
```

测试覆盖：

- `/proc/net/dev` 解析与速率、计数器回绕
- `/proc/stat` CPU 使用率
- `/proc/meminfo` 内存使用率
- `/proc/diskstats` I/O 利用率与 `statvfs` 容量
- hwmon 温度扫描与分类
- CPU 频率与 `/proc/cpuinfo` 回退
- SQLite settings/history/plugin_state、配置导入导出
- C ABI 插件加载、缺失工厂、版本/结构校验
- NVIDIA GPU 插件真实加载与 NVML 读数
- 格式化、设置模型、悬浮窗、设置对话框、托盘、采样线程、日志轮转
- 应用启动烟测

## 真实硬件读数

`--diagnose` 输出（2026-10-26）：

```text
Network: wlp0s20f3
  up: 84 B/s  down: 589 B/s
CPU cores: 20
CPU usage: 7%
Memory: 60%  8.8 / 14.8 GiB
Disk: io 2%  used 38%
CPU temperature: 56.0
CPU frequency: 490.0 MHz
Plugins scanned: 1
  trafficmonitor.gpu loaded
    trafficmonitor.gpu.usage = 5%
    trafficmonitor.gpu.memory = 285 / 4096 MiB
    trafficmonitor.gpu.temp = 55°C
```

同一时刻 `top` 第二采样 CPU 使用率约 6.9%，`free` 计算的内存使用率约 58.9%。差值在 P0 可接受范围内，内存读数会随时序波动。

## 窗口验证

在 XWayland 下启动后，`xprop` 显示：

```text
_NET_WM_STATE(ATOM) = _NET_WM_STATE_SKIP_PAGER, _NET_WM_STATE_SKIP_TASKBAR,
                      _NET_WM_STATE_ABOVE, _NET_WM_STATE_DEMANDS_ATTENTION
WM_CLASS(STRING) = "trafficmonitor-linux", "trafficmonitor-linux"
_NET_WM_WINDOW_TYPE(ATOM) = _NET_WM_WINDOW_TYPE_UTILITY,
                            _KDE_NET_WM_WINDOW_TYPE_OVERRIDE,
                            _NET_WM_WINDOW_TYPE_NORMAL
```

窗口为 32 位透明视觉，HiDPI 2x 下实际尺寸 600x280（逻辑 300x140）。

## 性能

Release 构建，XWayland，`--safe-mode`，30 秒稳态采样：

```text
CPU: 0.567%
RSS: 75,128 KB
启动到窗口出现: 184 ms / 209 ms（两次有效测量）
```

P0 门槛：CPU <1%，RSS <80MB，启动 <1s。三项均满足。

## 数据库与回退

- 配置、历史流量、插件状态写入 SQLite，WAL 模式。
- 历史按 60 秒桶写入，同时写选中接口和 `__total__`。
- 损坏数据库启动测试：程序将原文件重命名为 `.corrupt-YYYYMMDD-HHMMSS`，重新创建数据库并继续启动。
- 设置导出/导入 JSON 回归测试通过。

## 安装布局

`cmake --install` 安装到临时前缀后验证：

```text
bin/trafficmonitor-linux
bin/trafficmonitor-linux-wrapper
lib/trafficmonitor-linux/plugins/gpu_plugin.so
share/applications/trafficmonitor-linux.desktop
```

已安装二进制 `--smoke-test` 通过。

## 未完成或未验证

- GNOME 托盘：`ubuntu-appindicators` 扩展已安装但被禁用，运行环境无法验证托盘图标。程序会检测并提示：
  `gnome-extensions enable ubuntu-appindicators@ubuntu.com`
- 网络速率与外部工具（`nload`/`iftop`）的逐样本对比未做；解析和差值计算已有单元测试。
- 24 小时连续运行未做。
- 皮肤 JSON、GNOME 顶栏扩展、D-Bus、AMD/Intel GPU、进程 Top N、风扇/功耗/电池、容器属于 M1/M2/M3。
- 插件为进程内加载，崩溃会带崩主程序。
