# CMake Debug 构建支持设计

日期：2026-10-27
状态：已批准

## 目标

为 trafficmonitor-linux 增加一套明确的 Debug 构建配置，使开发者可以：

- 用一条命令完成 Debug 配置、构建和测试；
- 在 gdb 下获得完整调试符号；
- 在 Debug 构建中看到带源码位置的详细日志；
- 在 Release 发行构建中按需分离调试符号文件。

## 非目标

- 不引入 ASAN、UBSAN、TSAN 等 sanitizer。
- 不新增运行时 `--debug` 命令行参数或日志级别配置。
- 不修改采集、存储、UI 等业务逻辑。
- 不改变未指定 `CMAKE_BUILD_TYPE` 时的现有构建行为。

## 总体方案

以标准 `CMAKE_BUILD_TYPE=Debug` 为唯一驱动条件，通过 CMake 生成器表达式把
Debug 专属编译选项和 `TM_DEBUG` 宏挂到 Debug 配置上。Release 配置不继承任何
Debug 选项。新增 `CMakePresets.json` 作为标准入口，统一构建目录、生成器和测试
命令。

## CMakeLists.txt 改动

在全部目标定义之前加入：

```cmake
add_compile_options(
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Debug>:-O0>
    $<$<CONFIG:Debug>:-Wall>
    $<$<CONFIG:Debug>:-Wextra>
)
add_compile_definitions($<$<CONFIG:Debug>:TM_DEBUG>)
```

说明：

- 目录级设置覆盖 `tm_core`、UI 可执行文件、插件和全部测试目标。
- 启用 `-Wall -Wextra`，但不启用 `-Werror`，现有代码的警告不会阻塞构建。
- 不设置默认 `CMAKE_BUILD_TYPE`，保持现有 `build/` 工作流不变。

新增 Release 符号拆分选项：

```cmake
option(TM_SEPARATE_DEBUG_SYMBOLS "Split .debug files for Release binaries" OFF)
if(TM_SEPARATE_DEBUG_SYMBOLS)
    add_compile_options($<$<CONFIG:Release>:-g>)
endif()
```

当选项为 `ON` 时，为 Release 配置追加 `-g` 编译选项；标准 Release 默认不带
调试信息，不追加 `-g` 时拆分出的 `.debug` 文件将为空。

当选项为 `ON` 且 `CMAKE_BUILD_TYPE` 为 `Release` 时，对以下目标添加
POST_BUILD 步骤：

- `trafficmonitor-linux`
- `gpu_plugin`

每个目标依次执行：

1. `objcopy --only-keep-debug <binary> <binary>.debug`
2. `objcopy --strip-debug <binary>`
3. `objcopy --add-gnu-debuglink=<binary>.debug <binary>`

`objcopy` 使用 CMake 提供的 `CMAKE_OBJCOPY`。若该变量为空，输出
`message(WARNING ...)` 并跳过拆分，不中断构建。`.debug` 文件保留在构建目录，
不新增安装规则。可执行文件对应 `trafficmonitor-linux.debug`，共享库对应
`gpu_plugin.so.debug`（沿用目标文件的完整文件名）。

## 日志行为

只修改 `src/core/logger.cpp` 的 `messageHandler`：

- Debug（定义了 `TM_DEBUG`）：
  - 所有消息级别（`QtDebugMsg`、`QtInfoMsg`、`QtWarningMsg`、
    `QtCriticalMsg`、`QtFatalMsg`）同时写入 stderr。
  - stderr 行包含 `QMessageLogContext` 的 `file:line`。
  - 消息仍按现有行为写入 `app.log`。
- 非 Debug：
  - 丢弃 `QtDebugMsg`，不写入日志文件，也不输出到 stderr。
  - `QtWarningMsg` 及以上保持现有 stderr 行为。

`Logger` 的公开接口、日志文件格式和轮转逻辑不变。现有
`tests/unit/test_logger.cpp` 只断言 `qInfo` 写入文件，不受影响。

## CMakePresets.json

新增文件，最低版本 6（对应 CMake 3.25+，项目最低要求 3.28）。

### Configure presets

| preset | binaryDir | generator | 关键缓存变量 |
|--------|-----------|-----------|--------------|
| `debug` | `${sourceDir}/build/debug` | Ninja | `CMAKE_BUILD_TYPE=Debug`、`CMAKE_EXPORT_COMPILE_COMMANDS=ON` |
| `release` | `${sourceDir}/build/release` | Ninja | `CMAKE_BUILD_TYPE=Release`、`CMAKE_EXPORT_COMPILE_COMMANDS=ON`、`TM_SEPARATE_DEBUG_SYMBOLS=ON` |

### Build / Test presets

- `debug`、`release` build presets 分别关联同名 configure preset。
- `debug`、`release` test presets 分别关联同名 configure preset，并设置
  `output.outputOnFailure=true`。

使用方式：

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

## run.sh 改动

`run.sh` 按以下顺序选择构建目录：

1. 若 `${TRAFFICMONITOR_BUILD_DIR:-}` 非空，使用该目录。
2. 否则若 `build/debug/trafficmonitor-linux` 可执行，使用 `build/debug`。
3. 否则使用现有 `build/`。

随后把 `TRAFFICMONITOR_PLUGIN_DIR` 默认指向所选目录下的 `plugins/`，并执行该
目录下的 `trafficmonitor-linux`。Wayland 下设置 `QT_QPA_PLATFORM=xcb` 的现有
逻辑保持不变。

## README 改动

在“构建”一节补充：

- Debug 构建的 preset 命令；
- Debug 构建包含 `-g3 -O0`、`TM_DEBUG` 和详细 stderr 日志；
- Release 拆分符号的方法及 `.debug` 文件位置；
- `run.sh` 优先运行 `build/debug` 的行为。

## 验证方式

1. `cmake --preset debug && cmake --build --preset debug` 构建成功。
2. `ctest --preset debug --output-on-failure` 全部测试通过。
3. 运行 Debug 版程序，确认 `qDebug`/`qInfo` 出现在 stderr 且带源码位置。
4. `cmake --preset release && cmake --build --preset release` 构建成功。
5. 确认 `build/release/trafficmonitor-linux.debug` 和
   `build/release/plugins/gpu_plugin.so.debug` 存在，主程序仍可启动。
6. 确认 Release 版程序不输出 `QtDebugMsg`。

## 涉及文件

| 文件 | 改动 |
|------|------|
| `CMakeLists.txt` | Debug 编译选项、`TM_DEBUG`、`TM_SEPARATE_DEBUG_SYMBOLS` 与符号拆分 |
| `CMakePresets.json` | 新增 debug/release configure、build、test presets |
| `src/core/logger.cpp` | Debug stderr 输出与源码位置；非 Debug 过滤 DEBUG |
| `run.sh` | 选择构建目录并优先运行 `build/debug` |
| `README.md` | 补充 Debug 构建、符号拆分与运行说明 |
