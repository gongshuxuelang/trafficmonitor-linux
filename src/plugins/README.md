# 插件源码

本目录存放 TrafficMonitor for Linux 的进程内插件源码。插件编译为动态库（`.so`），由主程序通过 `dlopen` 加载，并按 `src/include/tm_plugin_api.h` 定义的稳定 C ABI 通信。

## 目录

- `gpu/`：NVIDIA GPU 插件，通过 NVML 提供 GPU 利用率、显存和温度。NVML 不可用时插件仍会加载，显示值降级为 `--`。

## 约定

- 每个插件一个子目录，并在根目录 `CMakeLists.txt` 中用 `add_library(<name> MODULE ...)` 注册。
- 插件必须导出 `tm_plugin_get_instance(uint32_t host_api_version)`，入口定义见 `src/include/tm_plugin_api.h`。
- 构建产物：开发时输出到 `build/plugins/`，安装时放到 `/usr/lib/trafficmonitor-linux/plugins/`。
- 运行时可用 `TRAFFICMONITOR_PLUGIN_DIR` 覆盖插件目录。
- 仅用于测试的插件在 `src/tests/plugins/`，不随程序安装。
