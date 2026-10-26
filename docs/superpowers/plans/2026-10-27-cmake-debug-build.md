# CMake Debug 构建支持实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 trafficmonitor-linux 增加 Debug 构建配置、调试日志、Release 符号拆分和 CMake presets。

**Architecture:** 用 `$<CONFIG:Debug>` 生成器表达式把 `-g3 -O0 -Wall -Wextra` 和
`TM_DEBUG` 宏挂到 Debug 配置；`logger.cpp` 通过 `TM_DEBUG` 控制 stderr 详细日志；
Release 下可选用 `objcopy` 拆分符号；`CMakePresets.json` 统一 debug/release 构建
目录与命令。

**Tech Stack:** CMake 3.28+、Ninja、C++20、Qt 6.5+（当前环境 Qt 6.10）、POSIX shell。

## Global Constraints

- 最低版本：CMake 3.28、Qt 6.5、C++20，生成器固定 Ninja。
- 只在 `CONFIG == Debug` 时启用 Debug 选项；Release 不得继承 `-g3`、`-O0`、
  `-Wall`、`-Wextra` 或 `TM_DEBUG`。
- 不启用 `-Werror`。
- 不设置默认 `CMAKE_BUILD_TYPE`。
- 不引入 ASAN/UBSAN/TSAN，不新增运行时 `--debug` 参数。
- 不修改采集、存储、UI 业务逻辑；日志改动只限 `src/core/logger.cpp`。
- 当前工作目录**不是 git 仓库**，所有“提交”步骤替换为验证检查点，不执行
  `git init` 或 `git commit`。
- 所有命令从项目根目录 `/home/lcx/librarys/code/trafficmonitor-linux` 执行。

---

### Task 1: Debug 编译选项与 TM_DEBUG 宏

**Files:**
- Modify: `CMakeLists.txt:6-11`

**Interfaces:**
- Produces: Debug 配置下所有目标自动获得 `-g3 -O0 -Wall -Wextra` 和
  `-DTM_DEBUG`；Task 2 依赖 `TM_DEBUG` 宏。

- [ ] **Step 1: 在 `CMAKE_CXX_EXTENSIONS` 之后加入 Debug 选项**

在 `CMakeLists.txt` 第 6 行 `set(CMAKE_CXX_EXTENSIONS OFF)` 之后、
`set(CMAKE_AUTOMOC ON)` 之前插入：

```cmake
add_compile_options(
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Debug>:-O0>
    $<$<CONFIG:Debug>:-Wall>
    $<$<CONFIG:Debug>:-Wextra>
)
add_compile_definitions($<$<CONFIG:Debug>:TM_DEBUG>)
```

- [ ] **Step 2: 配置 Debug 构建**

Run:

```bash
cmake -S . -B build/debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

Expected: 配置成功，生成 `build/debug/build.ninja`。

- [ ] **Step 3: 验证 Debug 编译参数**

Run:

```bash
grep -q -- '-DTM_DEBUG' build/debug/compile_commands.json
grep -q -- '-g3' build/debug/compile_commands.json
grep -q -- '-O0' build/debug/compile_commands.json
grep -q -- '-Wall' build/debug/compile_commands.json
grep -q -- '-Wextra' build/debug/compile_commands.json
```

Expected: 五条命令全部返回 0，无输出。

- [ ] **Step 4: 配置 Release 构建并确认没有 Debug 参数**

Run:

```bash
cmake -S . -B build/release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
! grep -q -- '-DTM_DEBUG' build/release/compile_commands.json
! grep -q -- '-g3' build/release/compile_commands.json
```

Expected: 两条 `grep` 均返回非 0，`!` 使命令整体成功。

- [ ] **Step 5: 完整构建 Debug**

Run:

```bash
cmake --build build/debug -j
```

Expected: 构建成功，退出码 0。

- [ ] **Step 6: 验证检查点**

Run:

```bash
cmake --build build/debug -j && echo "TASK1 OK"
```

Expected: 输出 `TASK1 OK`。工作区不是 git 仓库，不提交。

---

### Task 2: 调试日志输出与 Release 过滤

**Files:**
- Modify: `src/core/logger.cpp:80-89`
- Test: `tests/unit/test_logger.cpp`

**Interfaces:**
- Consumes: Task 1 定义的 `TM_DEBUG` 宏。
- Produces: Debug 构建中 `messageHandler` 把全部级别日志带 `file:line` 写到
  stderr；非 Debug 构建丢弃 `QtDebugMsg`。`Logger` 公开接口不变。

- [ ] **Step 1: 在测试文件中加入两个用例**

在 `tests/unit/test_logger.cpp` 顶部加入：

```cpp
#include <QDebug>
```

在 `rotatesAtFiveMegabytes()` 之后、类结束的 `};` 之前加入：

```cpp
    void dropsDebugMessagesInRelease()
    {
#ifdef TM_DEBUG
        QSKIP("Debug builds keep qDebug messages");
#else
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("app.log"));

        {
            Logger logger(path.toStdString());
            qDebug("debug-only-message");
        }

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QString content = QString::fromUtf8(file.readAll());
        QVERIFY(!content.contains(QStringLiteral("debug-only-message")));
#endif
    }

    void keepsDebugMessagesInDebugBuild()
    {
#ifdef TM_DEBUG
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("app.log"));

        {
            Logger logger(path.toStdString());
            qDebug("debug-only-message");
        }

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QString content = QString::fromUtf8(file.readAll());
        QVERIFY(content.contains(QStringLiteral("debug-only-message")));
#else
        QSKIP("Release builds drop qDebug messages");
#endif
    }
```

- [ ] **Step 2: 在 Release 下运行新测试，确认失败**

Run:

```bash
cmake --build build/release --target test_logger -j
./build/release/test_logger dropsDebugMessagesInRelease
```

Expected: FAIL，失败信息为
`!content.contains(QStringLiteral("debug-only-message"))` 不成立（当前实现会把
`qDebug` 写入日志文件）。

- [ ] **Step 3: 修改 `Logger::messageHandler`**

把 `src/core/logger.cpp` 中的 `messageHandler` 整体替换为：

```cpp
void Logger::messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
#ifndef TM_DEBUG
    Q_UNUSED(context);
    if (type == QtDebugMsg) {
        return;
    }
#endif

    QString location;
#ifdef TM_DEBUG
    if (context.file != nullptr) {
        location = QStringLiteral(" %1:%2")
                       .arg(QString::fromUtf8(context.file))
                       .arg(context.line);
    }
#endif

    const QString formatted =
        QStringLiteral("[%1]%2 %3").arg(typeName(type), location, message);
    if (instance_ != nullptr) {
        instance_->log(formatted);
    }
#ifdef TM_DEBUG
    std::fprintf(stderr, "%s\n", formatted.toLocal8Bit().constData());
#else
    if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
        std::fprintf(stderr, "%s\n", formatted.toLocal8Bit().constData());
    }
#endif
}
```

- [ ] **Step 4: 重新构建并运行 Release 测试**

Run:

```bash
cmake --build build/release --target test_logger -j
./build/release/test_logger dropsDebugMessagesInRelease keepsDebugMessagesInDebugBuild
```

Expected: `dropsDebugMessagesInRelease` PASS；`keepsDebugMessagesInDebugBuild`
SKIP。

- [ ] **Step 5: 构建并运行 Debug 测试**

Run:

```bash
cmake --build build/debug --target test_logger -j
./build/debug/test_logger keepsDebugMessagesInDebugBuild dropsDebugMessagesInRelease
```

Expected: `keepsDebugMessagesInDebugBuild` PASS；
`dropsDebugMessagesInRelease` SKIP。

- [ ] **Step 6: 验证 Debug 下 stderr 带源码位置**

Run:

```bash
./build/debug/test_logger keepsDebugMessagesInDebugBuild 2>&1 \
  | grep -E '\[DEBUG\].*test_logger\.cpp:[0-9]+ debug-only-message'
```

Expected: 输出一行匹配内容，退出码 0。

- [ ] **Step 7: 验证 Release 下 stderr 不出现 INFO**

Run:

```bash
STATE_DIR=$(mktemp -d)
CONFIG_DIR=$(mktemp -d)
DATA_DIR=$(mktemp -d)
QT_QPA_PLATFORM=offscreen \
XDG_STATE_HOME="$STATE_DIR" \
XDG_CONFIG_HOME="$CONFIG_DIR" \
XDG_DATA_HOME="$DATA_DIR" \
TRAFFICMONITOR_PLUGIN_DIR="$PWD/build/release/plugins" \
./build/release/trafficmonitor-linux --smoke-test \
  2>"$STATE_DIR/stderr.log"
! grep -q '\[INFO\]' "$STATE_DIR/stderr.log"
```

Expected: 烟测退出码 0，且 stderr 中没有 `[INFO]`。

- [ ] **Step 8: 执行完整测试套件（Debug）**

Run:

```bash
ctest --test-dir build/debug --output-on-failure
```

Expected: 全部测试通过，退出码 0。

- [ ] **Step 9: 验证检查点**

Run:

```bash
ctest --test-dir build/debug --output-on-failure && echo "TASK2 OK"
```

Expected: 输出 `TASK2 OK`。不提交。

---

### Task 3: Release 调试符号拆分

**Files:**
- Modify: `CMakeLists.txt`（选项、辅助函数、目标调用）

**Interfaces:**
- Produces: CMake 选项 `TM_SEPARATE_DEBUG_SYMBOLS`（默认 `OFF`）。当它为 `ON`
  且构建类型为 Release 时，`trafficmonitor-linux` 与 `gpu_plugin` 构建后生成
  同名 `.debug` 文件并写入 `.gnu_debuglink`。

- [ ] **Step 1: 在 Task 1 的 Debug 选项之后加入符号拆分选项和函数**

在 `add_compile_definitions($<$<CONFIG:Debug>:TM_DEBUG>)` 之后插入：

```cmake
option(TM_SEPARATE_DEBUG_SYMBOLS "Split .debug files for Release binaries" OFF)
if(TM_SEPARATE_DEBUG_SYMBOLS)
    add_compile_options($<$<CONFIG:Release>:-g>)
endif()

function(tm_separate_debug_symbols target)
    if(NOT TM_SEPARATE_DEBUG_SYMBOLS)
        return()
    endif()
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        return()
    endif()
    if(NOT CMAKE_OBJCOPY)
        message(WARNING "CMAKE_OBJCOPY not found; skipping debug symbol split for ${target}")
        return()
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} --only-keep-debug
                $<TARGET_FILE:${target}> $<TARGET_FILE:${target}>.debug
        COMMAND ${CMAKE_OBJCOPY} --strip-debug $<TARGET_FILE:${target}>
        COMMAND ${CMAKE_OBJCOPY}
                --add-gnu-debuglink=$<TARGET_FILE:${target}>.debug
                $<TARGET_FILE:${target}>
        COMMENT "Splitting debug symbols for ${target}"
        VERBATIM
    )
endfunction()
```

- [ ] **Step 2: 对主程序和 GPU 插件启用符号拆分**

在 `CMakeLists.txt` 末尾 `install(...)` 之前插入：

```cmake
tm_separate_debug_symbols(trafficmonitor-linux)
tm_separate_debug_symbols(gpu_plugin)
```

- [ ] **Step 3: 用选项 ON 配置 Release 构建**

Run:

```bash
cmake -S . -B build/release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DTM_SEPARATE_DEBUG_SYMBOLS=ON
```

Expected: 配置成功，无 `CMAKE_OBJCOPY not found` 警告。

- [ ] **Step 4: 构建主程序和 GPU 插件**

Run:

```bash
cmake --build build/release --target trafficmonitor-linux gpu_plugin -j
```

Expected: 构建成功，日志中出现 `Splitting debug symbols for ...`。

- [ ] **Step 5: 验证符号文件与 debuglink**

Run:

```bash
test -f build/release/trafficmonitor-linux.debug
test -f build/release/plugins/gpu_plugin.so.debug
readelf -S build/release/trafficmonitor-linux | grep -q '\.gnu_debuglink'
! readelf -S build/release/trafficmonitor-linux | grep -q '\.debug_info'
readelf -S build/release/trafficmonitor-linux.debug | grep -q '\.debug_info'
readelf -S build/release/plugins/gpu_plugin.so.debug | grep -q '\.debug_info'
```

Expected: 所有命令退出码 0；主程序无 `.debug_info`，`.debug` 文件包含
`.debug_info`。

- [ ] **Step 6: 验证拆分后的程序仍可运行**

Run:

```bash
./build/release/trafficmonitor-linux --version
```

Expected: 输出 `TrafficMonitor for Linux 0.1.0`，退出码 0。

- [ ] **Step 7: 验证选项 OFF 时不拆分**

Run:

```bash
cmake -S . -B build/release-off -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DTM_SEPARATE_DEBUG_SYMBOLS=OFF
cmake --build build/release-off --target trafficmonitor-linux -j
test ! -f build/release-off/trafficmonitor-linux.debug
readelf -S build/release-off/trafficmonitor-linux | grep -q '\.debug_info'
```

Expected: 三条验证命令全部成功。

- [ ] **Step 8: 验证检查点**

Run:

```bash
test -f build/release/trafficmonitor-linux.debug && echo "TASK3 OK"
```

Expected: 输出 `TASK3 OK`。不提交。

---

### Task 4: CMakePresets.json

**Files:**
- Create: `CMakePresets.json`

**Interfaces:**
- Consumes: `TM_SEPARATE_DEBUG_SYMBOLS`（Task 3）。
- Produces: `debug`、`release` 两个 configure preset，以及同名 build/test
  preset；供 Task 5 的 `run.sh` 使用 `build/debug`。

- [ ] **Step 1: 创建 `CMakePresets.json`**

写入完整内容：

```json
{
  "version": 6,
  "cmakeMinimumRequired": {
    "major": 3,
    "minor": 28,
    "patch": 0
  },
  "configurePresets": [
    {
      "name": "debug",
      "displayName": "Debug",
      "description": "Debug build with -g3 -O0 and TM_DEBUG",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/debug",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
      }
    },
    {
      "name": "release",
      "displayName": "Release",
      "description": "Release build with separated debug symbols",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/release",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
        "TM_SEPARATE_DEBUG_SYMBOLS": "ON"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "debug",
      "configurePreset": "debug"
    },
    {
      "name": "release",
      "configurePreset": "release"
    }
  ],
  "testPresets": [
    {
      "name": "debug",
      "configurePreset": "debug",
      "output": {
        "outputOnFailure": true
      }
    },
    {
      "name": "release",
      "configurePreset": "release",
      "output": {
        "outputOnFailure": true
      }
    }
  ]
}
```

- [ ] **Step 2: 验证 preset 可被识别**

Run:

```bash
cmake --list-presets
cmake --list-presets=build
cmake --list-presets=test
```

Expected: 分别列出 `debug` 和 `release`。

- [ ] **Step 3: 用 preset 配置并构建 Debug**

Run:

```bash
cmake --preset debug
cmake --build --preset debug
```

Expected: 配置和构建都成功。

- [ ] **Step 4: 用 preset 跑 Debug 测试**

Run:

```bash
ctest --preset debug
```

Expected: 全部测试通过。

- [ ] **Step 5: 用 preset 配置、构建并测试 Release**

Run:

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
```

Expected: 构建和测试全部成功，且 `build/release/trafficmonitor-linux.debug`
存在。

- [ ] **Step 6: 验证检查点**

Run:

```bash
ctest --preset debug && ctest --preset release && echo "TASK4 OK"
```

Expected: 输出 `TASK4 OK`。不提交。

---

### Task 5: run.sh 构建目录选择与 README

**Files:**
- Modify: `run.sh`
- Modify: `README.md:26-35`

**Interfaces:**
- Consumes: Task 4 的 `build/debug`、`build/release` 目录布局。
- Produces: `run.sh` 支持 `TRAFFICMONITOR_BUILD_DIR` 覆盖，默认优先
  `build/debug`，否则回退 `build/`。

- [ ] **Step 1: 替换 `run.sh` 内容**

写入完整内容：

```sh
#!/bin/sh

set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

BUILD_DIR="${TRAFFICMONITOR_BUILD_DIR:-}"
if [ -z "$BUILD_DIR" ]; then
    if [ -x "$ROOT/build/debug/trafficmonitor-linux" ]; then
        BUILD_DIR="$ROOT/build/debug"
    else
        BUILD_DIR="$ROOT/build"
    fi
fi

export TRAFFICMONITOR_PLUGIN_DIR="${TRAFFICMONITOR_PLUGIN_DIR:-$BUILD_DIR/plugins}"
if [ "${XDG_SESSION_TYPE:-}" = "wayland" ] && [ -z "${QT_QPA_PLATFORM:-}" ]; then
    export QT_QPA_PLATFORM=xcb
fi

exec "$BUILD_DIR/trafficmonitor-linux" "$@"
```

- [ ] **Step 2: 验证 run.sh 优先选择 build/debug**

Run:

```bash
sh -x ./run.sh --version 2>&1 | grep 'exec .*build/debug/trafficmonitor-linux'
```

Expected: 输出包含 `exec .../build/debug/trafficmonitor-linux --version`。

- [ ] **Step 3: 验证 `TRAFFICMONITOR_BUILD_DIR` 覆盖生效**

Run:

```bash
TRAFFICMONITOR_BUILD_DIR="$PWD/build/release" ./run.sh --version
```

Expected: 输出 `TrafficMonitor for Linux 0.1.0`。

- [ ] **Step 4: 在 README 的“构建”一节后补充 Debug 说明**

在 `README.md` 的 `## 构建` 小节结束、`## 运行` 之前插入：

````markdown
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
````

- [ ] **Step 5: 验证 README 内容**

Run:

```bash
grep -q '## Debug 构建' README.md
grep -q 'cmake --preset debug' README.md
grep -q 'TRAFFICMONITOR_BUILD_DIR' README.md
```

Expected: 三条命令退出码均为 0。

- [ ] **Step 6: 最终回归**

Run:

```bash
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
cmake --preset release && cmake --build --preset release && ctest --preset release
```

Expected: 两个配置全部构建成功、全部测试通过。

- [ ] **Step 7: 验证检查点**

Run:

```bash
grep -q '## Debug 构建' README.md && echo "TASK5 OK"
```

Expected: 输出 `TASK5 OK`。不提交。

---

## 计划自查

- **Spec 覆盖：** Debug 编译选项（Task 1）、`TM_DEBUG`（Task 1）、调试日志
  （Task 2）、Release 符号拆分（Task 3）、CMakePresets（Task 4）、`run.sh`
  与 README（Task 5）、验证方式（各 Task 的验证步骤）均有对应任务。
- **占位符：** 无 TBD/TODO，所有代码和命令均为完整内容。
- **一致性：** `TM_DEBUG`、`TM_SEPARATE_DEBUG_SYMBOLS`、`build/debug`、
  `build/release`、`TRAFFICMONITOR_BUILD_DIR` 在各任务中命名一致。
