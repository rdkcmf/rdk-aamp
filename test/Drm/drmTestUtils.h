#include <vector>
#include <cjson/cJSON.h>

#include "CppUTest/TestHarness.h"

// Useful macros for checking JSON
#define CHECK_JSON_STR_VALUE(o,p,e){cJSON *jsonV = cJSON_GetObjectItem(o, p); \
									CHECK_TEXT(jsonV != NULL, "Missing '" p "' property in JSON data"); \
									CHECK_EQUAL_TEXT(true, cJSON_IsString(jsonV), "Property '" p "' value is not a string"); \
									STRCMP_EQUAL(e, cJSON_GetStringValue(jsonV));}

class TestUtilJsonWrapper {
public:
	TestUtilJsonWrapper(const std::string& jsonStr);
	TestUtilJsonWrapper(const char *jsonStr, size_t size) : TestUtilJsonWrapper(std::string(jsonStr, size)) {};
	TestUtilJsonWrapper(const std::vector<uint8_t> jsonData) : TestUtilJsonWrapper(std::string((const char*)jsonData.data(), jsonData.size())) {};
	~TestUtilJsonWrapper();
	cJSON* getJsonObj() { return mJsonObj; };
private:
	cJSON *mJsonObj;
};

SimpleString StringFrom(const std::vector<std::string>& vec);
SimpleString StringFrom(const std::vector<std::uint8_t>& v);
