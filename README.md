# TrafficMonitor for Linux

Linux 版 TrafficMonitor P0 原型。Qt Widgets + QPainter 悬浮窗，core 采集库，SQLite 配置与历史，C ABI 插件，NVIDIA GPU 插件。

## 当前 P0 能力

- 网络上传/下载速度（`/proc/net/dev`）
- CPU 使用率（`/proc/stat`）
- 内存使用率（`/proc/meminfo`）
- 磁盘 I/O 与容量（`/proc/diskstats`、`statvfs`）
- CPU/GPU/硬盘/主板温度（`/sys/class/hwmon`）
- CPU 频率（`cpufreq`，回退 `/proc/cpuinfo`）
- NVIDIA GPU 利用率、显存、温度（C ABI 插件，NVML）
- XWayland 无边框、透明、置顶悬浮窗
- 拖动、锁定位置、鼠标穿透、右键菜单
- SVG/PNG 托盘图标（GNOME 需要启用 AppIndicator 扩展）
- 简单设置对话框
- SQLite 配置持久化与今日流量历史
- 单实例运行、`--quit`、`--diagnose`、`--safe-mode`、`--smoke-test`
- 17 个自动化测试，覆盖采集解析、SQLite、插件 ABI、GPU 插件、UI 组件和启动烟测

P0 尚未包含：JSON 皮肤、GNOME 顶栏扩展、D-Bus、AMD/Intel GPU、风扇/功耗/电池、进程 Top N、容器。

P0 实测结果和未验证项见 `docs/p0-status.md`。

## 构建

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

依赖：Qt 6.2+、CMake 3.28+、Ninja、C++20。构建矩阵覆盖 Ubuntu 22.04/24.04/26.04、
Debian 12/13。Ubuntu 22.04 和 Debian 12 自带的 CMake 低于 3.28，
流水线会自动下载并校验 CMake 3.31.6。

构建默认由 Ninja 按 CPU 核数并行；系统装有 `ccache` 时配置阶段会自动启用编译缓存
（`-DTM_USE_CCACHE=OFF` 可关闭）。

## Debug 构建

使用 CMake presets 一键配置、构建和测试：

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

Debug 构建启用 `-g3 -O0 -Wall -Wextra`，定义 `TM_DEBUG`，并把所有日志
（含 `qDebug`/`qInfo`）带源码位置输出到 stderr。构建输出位于 `build/debug/`。

`run.sh` 会优先运行 `build/debug/trafficmonitor-linux`；也可以通过
`TRAFFICMONITOR_BUILD_DIR` 指定其他构建目录：

```bash
TRAFFICMONITOR_BUILD_DIR="$PWD/build/release" ./run.sh
```

Release 构建默认拆分调试符号：

```bash
cmake --preset release
cmake --build --preset release
```

构建后保留 `build/release/trafficmonitor-linux.debug` 和
`build/release/plugins/gpu_plugin.so.debug`，gdb 会在同目录自动加载它们。
不需要拆分时配置 `-DTM_SEPARATE_DEBUG_SYMBOLS=OFF`。

## 打包 deb

一次生成在线包和离线包：

```bash
./packaging/build-deb.sh
```

产物在 `build/packages/`：

- `ubuntu26-trafficmonitor-linux_0.1.0-1_amd64.deb`：在线包，Qt/glibc 依赖由 apt 从软件源安装。
- `ubuntu26-trafficmonitor-linux-offline_0.1.0-1_amd64.deb`：离线包，捆绑 Qt 运行库、平台/图片/SQLite 插件及其依赖，只依赖 `libc6`。两个包冲突，不能同时安装。

文件名开头的 `ubuntu26` 由 `/etc/os-release` 的 `ID` 和 `VERSION_ID` 主版本生成，
也可以用 `TM_OS_TAG` 覆盖，例如 `TM_OS_TAG=ubuntu24`。包内的 `Package` 字段保持
`trafficmonitor-linux`，这样跨系统升级不会变成两个不同的包。

安装：

```bash
sudo apt install ./build/packages/ubuntu26-trafficmonitor-linux_0.1.0-1_amd64.deb
# 或
sudo apt install ./build/packages/ubuntu26-trafficmonitor-linux-offline_0.1.0-1_amd64.deb
```

包内路径：

- `/usr/bin/trafficmonitor-linux`
- `/usr/bin/trafficmonitor-linux-wrapper`
- `/usr/lib/trafficmonitor-linux/plugins/gpu_plugin.so`
- `/usr/lib/trafficmonitor-linux/lib/`（仅离线包：Qt 运行库）
- `/usr/lib/trafficmonitor-linux/qt-plugins/`（仅离线包：Qt 插件）
- `/usr/share/applications/trafficmonitor-linux.desktop`

脚本先执行 `cmake --preset release` 和构建，再用 CPack 生成在线包，最后用
`cmake --install` 加 `dpkg-deb` 组装离线包。已有 Release 构建时加 `--skip-build`。
CI 可以用 `TM_DEB_RELEASE` 覆盖 Debian revision，例如
`TM_DEB_RELEASE=20261027.abc1234` 生成 `0.1.0-20261027.abc1234`。

只生成在线包也可以直接用 CPack：

```bash
cmake --preset release
cmake --build --preset release
cpack --preset deb
```

依赖默认由 `dpkg-shlibdeps` 自动计算，需要安装 `dpkg-dev`。没有时用
`-DTM_DEB_SHLIBDEPS=OFF` 配置，改用 CMake 中手写的依赖列表（与 `packaging/deb/control` 一致）。

离线包的限制：

- 不捆绑 glibc，`Depends` 按构建机实际需要的最高 GLIBC 版本生成，目标机器需要相同或更新的 glibc。
- 不捆绑 NVIDIA 驱动库 `libnvidia-ml.so.1`，缺失时 GPU 数值显示 `--`。
- 通过 `trafficmonitor-linux-wrapper` 启动时会自动设置 `LD_LIBRARY_PATH` 和 `QT_PLUGIN_PATH`，桌面图标和命令行都应使用该 wrapper。
- 包内含 Qt 动态库，分发时需遵守 LGPL 要求。

## AppImage、RPM 和 Arch 包

三个脚本都复用离线包的 Qt 捆绑逻辑（`packaging/bundle-runtime.sh`），已有 Release
构建时加 `--skip-build`。

### AppImage（通用）

```bash
./packaging/build-appimage.sh
```

生成 `build/packages/trafficmonitor-linux_<版本>_x86_64.AppImage`。在 Ubuntu 22.04
上构建时 glibc 下限是 2.35，可运行在 Ubuntu 22.04+、Debian 12+、Fedora 36+ 等系统。
系统没有 libfuse2 时可以加 `--appimage-extract-and-run`。AppImage 不分在线/离线，
Qt 运行库和插件都捆绑在里面。

### RPM（Fedora/RHEL/openSUSE）

```bash
./packaging/build-rpm.sh
```

生成在线包 `trafficmonitor-linux-<版本>-<release>.<arch>.rpm` 和离线包
`trafficmonitor-linux-offline-...rpm`，release 带发行版标识（`fc41`、`el9`、`suse15`）。
需要在有 `rpmbuild` 的系统上执行，CI 目前使用 `fedora:latest`；脚本会根据
`/etc/os-release` 生成 `fc`/`el`/`suse` 标识，也可以在对应的 RHEL/openSUSE
容器里运行。License 字段默认 `Unspecified`，可以用 `TM_RPM_LICENSE` 覆盖。

### Arch（Arch/Manjaro）

```bash
./packaging/build-arch.sh
```

生成 `trafficmonitor-linux-<版本>-1-x86_64.pkg.tar.zst` 和对应的 offline 包。
需要在 Arch 系统上执行（`base-devel`、`qt6-base`、`qt6-svg`），CI 使用
`archlinux:latest`。脚本在 root 环境会自动创建 `builder` 用户运行 `makepkg`。
License 字段默认 `custom`，可以用 `TM_ARCH_LICENSE` 覆盖。

Flatpak 和 Snap 暂未接入流水线：它们需要额外的运行时、沙箱权限（`/proc`、`/sys`、
托盘）和发布工具，当前 AppImage 已经覆盖大多数发行版。需要时再单独验证这套沙箱行为。

## 自动发布

`.github/workflows/daily-release.yml` 每天北京时间 05:00 检查默认分支是否有新提交：

- 没有新提交：跳过。
- 有新提交：并行构建 Ubuntu 22.04/24.04/26.04、Debian 12/13 的 `.deb`，另加一个
  通用 AppImage、Fedora RPM 和 Arch 包，然后创建一个包含全部附件的 GitHub Release。
- 也可以在 Actions 页面手动运行，勾选 `force` 可以忽略“无新提交”检查。

`*.deb` 的文件名格式为 `<系统>-trafficmonitor-linux_0.1.0-20261027.abc1234_amd64.deb`，
系统前缀有 `ubuntu22-`、`ubuntu24-`、`ubuntu26-`、`debian12-`、`debian13-`。
Release 标签为 `v0.1.0-20261027.abc1234`。`0.1.0` 取 `CMakeLists.txt` 的
`project(... VERSION ...)` 和最近 `v*` 标签前缀中较大的一个；`20261027` 是北京时间日期；
`abc1234` 是发布提交的短 SHA。同一天多次发布用 SHA 区分。需要里程碑版本时，改
`CMakeLists.txt` 的版本号或打 `v1.0.0` 标签都可以，后续每日包会自动使用更大的前缀。

Release 附件包括五个系统的在线/离线 `.deb`、`*.AppImage`、Fedora/RHEL 的 `*.rpm`、
Arch 的 `*.pkg.tar.zst` 和 `SHA256SUMS`。每个系统在自己的容器里构建，离线包的
glibc 下限对应那个系统。Ubuntu 22.04 和 Debian 12 的 CMake 低于 3.28，流水线会
自动下载 CMake 3.31.6。私有仓库的矩阵会按任务数消耗 Actions 分钟。工作流文件
需要提交到默认分支，定时任务才会生效。

## 运行

```bash
./run.sh
```

`run.sh` 会在 Wayland 会话中设置 `QT_QPA_PLATFORM=xcb`，并指向 `build/plugins`。也可以手动运行：

```bash
QT_QPA_PLATFORM=xcb \
TRAFFICMONITOR_PLUGIN_DIR="$PWD/build/plugins" \
./build/trafficmonitor-linux
```

GNOME 托盘默认不可见，因为 `ubuntu-appindicators` 扩展可能未启用。启用：

```bash
gnome-extensions enable ubuntu-appindicators@ubuntu.com
```

如果托盘仍不可用，程序会弹出一次说明；悬浮窗和设置仍可用。

## 诊断

```bash
./build/trafficmonitor-linux --diagnose
./build/trafficmonitor-linux --version
./build/trafficmonitor-linux --safe-mode
./build/trafficmonitor-linux --smoke-test
```

## 插件

插件目录：

- 开发：`build/plugins/`
- 安装：`/usr/lib/trafficmonitor-linux/plugins/`
- 环境变量覆盖：`TRAFFICMONITOR_PLUGIN_DIR`

NVIDIA 插件通过 `dlopen("libnvidia-ml.so.1")` 加载 NVML。NVML 不可用时插件仍会加载，显示值降级为 `--`，主程序继续运行。

插件 ABI 定义在 `src/core/plugin/tm_plugin_api.h`。插件必须导出：

```c
const TMPluginV1* tm_plugin_get_instance(uint32_t host_api_version);
```

## 目录

- `src/core/`：数据模型、采集、SQLite、插件宿主、采样线程（头文件与实现同目录）
- `src/core/plugin/tm_plugin_api.h`：稳定 C ABI
- `src/include/pch.h`：预编译头
- `src/ui/`：悬浮窗、托盘、设置对话框（Qt Designer `.ui` 表单）、主程序
- `src/plugins/gpu/`：NVIDIA GPU 插件
- `src/tests/unit/`：Qt Test 单元与集成测试
- `src/skins/`：自定义皮肤资源预留目录（P0 未实现，见 `src/skins/README.md`）
- `packaging/`：deb 桌面文件与 XWayland 启动 wrapper

## 已知限制

- P0 只验证 NVIDIA GPU；AMD/Intel 后端在 M1。
- GNOME 托盘依赖 AppIndicator 扩展。
- 纯 Wayland 下 Qt 窗口无法保证置顶/定位；P0 使用 XWayland。
- 进程内插件崩溃会带崩主程序。
- 历史写入在 UI 线程，P0 可接受，M1 迁移到独立数据库线程。
- 皮肤 JSON、GNOME 顶栏扩展和 D-Bus 尚未实现。
