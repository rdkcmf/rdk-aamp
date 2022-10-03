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
*
* Fake implementations of APIs from cJSON which is:
* Copyright (c) 2009-2017 Dave Gamble and cJSON contributors
* Licensed under the MIT License
*/

#include "cjson/cJSON.h"

CJSON_PUBLIC(cJSON *) cJSON_Parse(const char *value)
{
    return nullptr;
}

CJSON_PUBLIC(void) cJSON_free(void *object)
{
}

CJSON_PUBLIC(cJSON *) cJSON_GetObjectItem(const cJSON * const object, const char * const string)
{
    return nullptr;
}

CJSON_PUBLIC(void) cJSON_Delete(cJSON *item)
{

}

CJSON_PUBLIC(int) cJSON_GetArraySize(const cJSON *array)
{
    return 0;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsNumber(const cJSON * const item)
{
    return cJSON_False;
}

CJSON_PUBLIC(const char *) cJSON_GetErrorPtr(void)
{
    return (const char *)"";
}

CJSON_PUBLIC(cJSON *) cJSON_CreateArray(void)
{
	return nullptr;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateObject(void)
{
	return nullptr;
}
CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToArray(cJSON *array, cJSON *item)
{
	return cJSON_False;
}

CJSON_PUBLIC(cJSON*) cJSON_AddBoolToObject(cJSON * const object, const char * const name, const cJSON_bool boolean)
{
	return nullptr;
}

CJSON_PUBLIC(cJSON*) cJSON_AddNumberToObject(cJSON * const object, const char * const name, const double number)
{
	return nullptr;
}

CJSON_PUBLIC(cJSON*) cJSON_AddStringToObject(cJSON * const object, const char * const name, const char * const string)
{
	return nullptr;
}

CJSON_PUBLIC(cJSON*) cJSON_AddObjectToObject(cJSON * const object, const char * const name)
{
	return nullptr;
}

CJSON_PUBLIC(cJSON*) cJSON_AddArrayToObject(cJSON * const object, const char * const name)
{
	return nullptr;
}

CJSON_PUBLIC(char *) cJSON_Print(const cJSON *item)
{
    return (char *)"";
}

CJSON_PUBLIC(char *) cJSON_PrintUnformatted(const cJSON *item)
{
    return (char *)"";
}

