/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "MockAampJsonObject.h"
#include "AampJsonObject.h"

std::shared_ptr<MockAampJsonObject> g_mockAampJsonObject;

AampJsonObject::AampJsonObject() : mParent(NULL), mJsonObj()
{
}

AampJsonObject::AampJsonObject(const std::string& jsonStr) : mParent(NULL), mJsonObj()
{
}

AampJsonObject::AampJsonObject(const char* jsonStr) : mParent(NULL), mJsonObj()
{
}

AampJsonObject::~AampJsonObject()
{
}

bool AampJsonObject::add(const std::string& name, const std::string& value, const ENCODING encoding)
{
	if (g_mockAampJsonObject != nullptr)
	{
		return g_mockAampJsonObject->add(name, value, encoding);
	}
	return false;
}

bool AampJsonObject::add(const std::string& name, const char *value, const ENCODING encoding)
{
	if (g_mockAampJsonObject != nullptr)
	{
		return g_mockAampJsonObject->add(name, value, encoding);
	}
	return false;
}

bool AampJsonObject::add(const std::string& name, const std::vector<std::string>& values)
{
	if (g_mockAampJsonObject != nullptr)
	{
		return g_mockAampJsonObject->add(name, values);
	}
	return false;
}

bool AampJsonObject::add(const std::string& name, const std::vector<uint8_t>& values, const ENCODING encoding)
{
	if (g_mockAampJsonObject != nullptr)
	{
		return g_mockAampJsonObject->add(name, values, encoding);
	}
	return false;
}

bool AampJsonObject::add(const std::string& name, AampJsonObject& value)
{
	if (g_mockAampJsonObject != nullptr)
	{
		return g_mockAampJsonObject->add(name, value);
	}
	return false;
}

bool AampJsonObject::add(const std::string& name, std::vector<AampJsonObject*>& values)
{
	if (g_mockAampJsonObject != nullptr)
	{
		return g_mockAampJsonObject->add(name, values);
	}
	return false;
}

bool AampJsonObject::add(const std::string& name, cJSON *value)
{
	if (g_mockAampJsonObject != nullptr)
	{
		return g_mockAampJsonObject->add(name, value);
	}
	return false;
}

bool AampJsonObject::add(const std::string& name, bool value)
{
	if (g_mockAampJsonObject != nullptr)
	{
		return g_mockAampJsonObject->add(name, value);
	}
	return false;
}

bool AampJsonObject::add(const std::string& name, int value)
{
	if (g_mockAampJsonObject != nullptr)
	{
		return g_mockAampJsonObject->add(name, value);
	}
	return false;
}

bool AampJsonObject::add(const std::string& name, double value)
{
	if (g_mockAampJsonObject != nullptr)
	{
		return g_mockAampJsonObject->add(name, value);
	}
	return false;
}

bool AampJsonObject::add(const std::string& name, long value)
{
	if (g_mockAampJsonObject != nullptr)
	{
		return g_mockAampJsonObject->add(name, value);
	}
	return false;
}

bool AampJsonObject::set(AampJsonObject *parent, cJSON *object)
{
	if (g_mockAampJsonObject != nullptr)
	{
		return g_mockAampJsonObject->set(parent, object);
	}
	return false;
}

bool AampJsonObject::get(const std::string& name, AampJsonObject &value)
{
	if (g_mockAampJsonObject != nullptr)
	{
		return g_mockAampJsonObject->get(name, value);
	}
	return false;
}

bool AampJsonObject::get(const std::string& name, std::string& value)
{
	if (g_mockAampJsonObject != nullptr)
	{
		return g_mockAampJsonObject->get(name, value);
	}
	return false;
}

bool AampJsonObject::get(const std::string& name, int& value)
{
	if (g_mockAampJsonObject != nullptr)
	{
		return g_mockAampJsonObject->get(name, value);
	}
	return false;
}

bool AampJsonObject::get(const std::string& name, std::vector<std::string>& values)
{
	if (g_mockAampJsonObject != nullptr)
	{
		return g_mockAampJsonObject->get(name, values);
	}
	return false;
}

bool AampJsonObject::get(const std::string& name, std::vector<uint8_t>& values, const ENCODING encoding)
{
	if (g_mockAampJsonObject != nullptr)
	{
		return g_mockAampJsonObject->get(name, values, encoding);
	}
	return false;
}

std::string AampJsonObject::print()
{
	return "";
}

std::string AampJsonObject::print_UnFormatted()
{
	return "";
}

void AampJsonObject::print(std::vector<uint8_t>& data)
{
}

bool AampJsonObject::isArray(const std::string& name)
{
	return false;
}

bool AampJsonObject::isString(const std::string& name)
{
	return false;
}

bool AampJsonObject::isNumber(const std::string& name)
{
	return false;
}

bool AampJsonObject::isObject(const std::string& name)
{
	return false;
}
