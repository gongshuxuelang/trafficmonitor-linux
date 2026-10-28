#include "core/plugin/tm_plugin_api.h"

namespace {

TMStringView testValue(void*)
{
    static const char value[] = "42";
    return TMStringView{value, 2};
}

const TMPluginItemV1 kItem{
    sizeof(TMPluginItemV1),
    TM_PLUGIN_API_VERSION,
    "test.item",
    "Test Item",
    "T:",
    "42",
    0,
    nullptr,
    testValue,
    nullptr,
    nullptr,
    nullptr,
};

TMResult testInitialize(void*, const TMPluginHostV1*)
{
    return TM_OK;
}

void testShutdown(void*)
{
}

void testUpdate(void*, const TMMetricSnapshot*)
{
}

uint32_t testItemCount(void*)
{
    return 1;
}

const TMPluginItemV1* testItem(void*, uint32_t index)
{
    return index == 0 ? &kItem : nullptr;
}

TMStringView testTooltip(void*)
{
    static const char text[] = "test tooltip";
    return TMStringView{text, sizeof(text) - 1};
}

const TMPluginV1 kPlugin{
    sizeof(TMPluginV1),
    TM_PLUGIN_API_VERSION,
    "test.plugin",
    "Test Plugin",
    "1.0.0",
    "tester",
    "Plugin used by the host tests",
    "",
    "",
    nullptr,
    testInitialize,
    testShutdown,
    testUpdate,
    testItemCount,
    testItem,
    nullptr,
    nullptr,
    testTooltip,
    nullptr,
};

} // namespace

extern "C" TM_PLUGIN_EXPORT const TMPluginV1* tm_plugin_get_instance(uint32_t)
{
    return &kPlugin;
}
