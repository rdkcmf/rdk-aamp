/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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

/**
 * @file jsutils.h
 * @brief JavaScript util functions for AAMP
 */


#ifndef __AAMP_JSUTILS_H__
#define __AAMP_JSUTILS_H__

#include "main_aamp.h"

#include <JavaScriptCore/JavaScript.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#ifndef _DEBUG
//#define _DEBUG
#endif

#ifdef _DEBUG
#define LOG(...)  printf(__VA_ARGS__);printf("\n");fflush(stdout);
#else
#define LOG(...)
#endif

#define INFO(...)  printf(__VA_ARGS__);printf("\n");fflush(stdout);
#define ERROR(...)  printf(__VA_ARGS__);printf("\n");fflush(stdout);

#ifdef TRACE
#define TRACELOG(...)  printf(__VA_ARGS__);printf("\n");fflush(stdout);
#else
#define TRACELOG(...)
#endif

/**
 * @enum ErrorCode
 * @brief
 */
enum ErrorCode
{
	AAMPJS_INVALID_ARGUMENT = -1,
	AAMPJS_MISSING_OBJECT = -2,
	AAMPJS_GENERIC_ERROR = -3
};


/**
 * @brief
 * @param context
 * @param sz
 * @retval
 */
JSValueRef aamp_CStringToJSValue(JSContextRef context, const char* sz);


/**
 * @brief
 * @param context
 * @param value
 * @param exception
 * @retval
 */
char* aamp_JSValueToCString(JSContextRef context, JSValueRef value, JSValueRef* exception);


/**
 * @brief
 * @param context
 * @param value
 * @retval
 */
bool aamp_JSValueIsArray(JSContextRef context, JSValueRef value);

/**
 * @brief
 * @param context
 * @param arrayRef
 * @retval
 */
std::vector<std::string> aamp_StringArrayToCStringArray(JSContextRef context, JSValueRef arrayRef);


/**
 * @brief
 * @param context
 * @param error
 * @param additionalInfo
 * @retval
 */
JSValueRef aamp_GetException(JSContextRef context, ErrorCode error, const char *additionalInfo);


/**
 * @brief
 * @param szName
 * @retval
 */
AAMPEventType aamp_getEventTypeFromName(const char* szName);


/**
 * @brief
 * @param type
 * @retval
 */
const char* aamp_getNameFromEventType(AAMPEventType type);

#endif /* __AAMP_JSUTILS_H__ */
