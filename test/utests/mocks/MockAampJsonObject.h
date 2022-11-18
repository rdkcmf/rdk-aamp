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

#pragma once

#include <gmock/gmock.h>
#include "AampJsonObject.h"

class MockAampJsonObject : public AampJsonObject
{
public:
    MOCK_METHOD(bool, add, (const std::string& name, const std::string& value, const ENCODING encoding));
    MOCK_METHOD(bool, add, (const std::string& name, const char *value, const ENCODING encoding));
    MOCK_METHOD(bool, add, (const std::string& name, const std::vector<std::string>& values));
    MOCK_METHOD(bool, add, (const std::string& name, const std::vector<uint8_t>& values, const ENCODING encoding));
    MOCK_METHOD(bool, add, (const std::string& name, AampJsonObject& value));
    MOCK_METHOD(bool, add, (const std::string& name, std::vector<AampJsonObject*>& values));
    MOCK_METHOD(bool, add, (const std::string& name, cJSON *value));
    MOCK_METHOD(bool, add, (const std::string& name, bool value));
    MOCK_METHOD(bool, add, (const std::string& name, int value));
    MOCK_METHOD(bool, add, (const std::string& name, long value));
    MOCK_METHOD(bool, add, (const std::string& name, double value));

    MOCK_METHOD(bool, set, (AampJsonObject *parent, cJSON *object));

    MOCK_METHOD(bool, get, (const std::string& name, AampJsonObject &value));
    MOCK_METHOD(bool, get, (const std::string& name, std::string& value));
    MOCK_METHOD(bool, get, (const std::string& name, int& value));
    MOCK_METHOD(bool, get, (const std::string& name, std::vector<std::string>& values));
    MOCK_METHOD(bool, get, (const std::string& name, std::vector<uint8_t>& values, const ENCODING encoding));
};

extern std::shared_ptr<MockAampJsonObject> g_mockAampJsonObject;
