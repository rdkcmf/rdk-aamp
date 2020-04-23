#include <string>
#include <iostream>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <iterator>
#include <sstream>

#include "CppUTest/TestHarness.h"

#include "drmTestUtils.h"

TestUtilJsonWrapper::TestUtilJsonWrapper(const std::string& jsonStr)
{
	mJsonObj = cJSON_Parse(jsonStr.c_str());
}

TestUtilJsonWrapper::~TestUtilJsonWrapper()
{
	if (mJsonObj) cJSON_Delete(mJsonObj);
}

SimpleString StringFrom(const std::vector<std::string>& vec)
{
	std::ostringstream oss;

	if (!vec.empty())
	{
		std::copy(vec.begin(), vec.end() - 1, std::ostream_iterator<std::string>(oss, ","));
		oss << vec.back();
	}

	return SimpleString(oss.str().c_str());
}

SimpleString StringFrom(const std::vector<std::uint8_t>& v)
{
	std::ostringstream oss;

	for (auto x : v)
	{
		oss << std::hex << std::setw(2) << std::setfill('0') << (unsigned)x;
	}

	return SimpleString(oss.str().c_str());
}
