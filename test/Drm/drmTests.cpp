#include <cjson/cJSON.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/TestHarness_c.h"
#include "CppUTest/TestRegistry.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTestExt/MockSupportPlugin.h"

int main(int ac, char** av)
{
	MemoryLeakWarningPlugin::turnOffNewDeleteOverloads();
	MockSupportPlugin mockPlugin;
	TestRegistry::getCurrentRegistry()->installPlugin(&mockPlugin);

	cJSON_Hooks hooks;
	hooks.malloc_fn = cpputest_malloc;
	hooks.free_fn = cpputest_free;
	cJSON_InitHooks(&hooks);

	return CommandLineTestRunner::RunAllTests(ac, av);
}
