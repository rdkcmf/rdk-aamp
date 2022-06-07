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
#include "AampUtils.h"

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

#define EXCEPTION_ERR_MSG_MAX_LEN 1024

/**
 * @enum ErrorCode
 * @brief JavaScript error codes
 */
enum ErrorCode
{
	AAMPJS_INVALID_ARGUMENT = -1,
	AAMPJS_MISSING_OBJECT = -2,
	AAMPJS_GENERIC_ERROR = -3
};

/**
 * @fn aamp_CStringToJSValue
 * @param[in] context JS execution context
 * @param[in] sz C string
 * @retval JSValue that is the converted JSString
 */
JSValueRef aamp_CStringToJSValue(JSContextRef context, const char* sz);

/**
 * @fn aamp_JSValueToCString
 * @param[in] context JS execution context
 * @param[in] value JSValue of JSString to be converted
 * @param[out] exception pointer to a JSValueRef in which to store an exception, if any
 * @retval converted C string
 */
char* aamp_JSValueToCString(JSContextRef context, JSValueRef value, JSValueRef* exception);

/**
 * @fn aamp_JSValueToJSONCString
 * @param[in] context JS execution context
 * @param[in] value JSValue of JSString to be converted
 * @param[out] exception pointer to a JSValueRef in which to store an exception, if any
 * @retval converted C string
 */
char* aamp_JSValueToJSONCString(JSContextRef context, JSValueRef value, JSValueRef* exception);

/**
 * @fn aamp_JSValueIsArray
 * @param[in] context JS exception context
 * @param[in] value JSValue to check if array or not
 * @retval true if JSValue is an array
 */
bool aamp_JSValueIsArray(JSContextRef context, JSValueRef value);

/**
 * @fn aamp_StringArrayToCStringArray
 * @param[in] context JS execution context
 * @param[in] arrayRef JSValue of an array of JSString
 * @retval converted array of C strings
 */
std::vector<std::string> aamp_StringArrayToCStringArray(JSContextRef context, JSValueRef arrayRef);

/**
 * @fn aamp_GetException
 * @param[in] context JS exception context
 * @param[in] error error/exception code
 * @param[in] additionalInfo additional error description
 * @retval JSValue object with exception details
 */
JSValueRef aamp_GetException(JSContextRef context, ErrorCode error, const char *additionalInfo);

/**
 * @fn aamp_getEventTypeFromName
 * @param[in] szName JS event name
 * @retval AAMPEventType of corresponding AAMP event
 */
AAMPEventType aamp_getEventTypeFromName(const char* szName);

/**
 * @fn aamp_dispatchEventToJS
 * @param[in] context JS execution context
 * @param[in] callback function to which event has to be dispatched as an arg
 * @param[in] event the JS event to be dispatched
 */
void aamp_dispatchEventToJS(JSContextRef context, JSObjectRef callback, JSObjectRef event);

/**
 * @fn aampPlayer_getEventTypeFromName
 * @param[in] szName JS event name
 * @retval AAMPEventType of corresponding AAMP event
 */
AAMPEventType aampPlayer_getEventTypeFromName(const char* szName);

/**
 * @fn aampPlayer_getNameFromEventType
 * @param[in] type AAMP event type
 * @retval JS event name corresponding to AAMP event
 */
const char* aampPlayer_getNameFromEventType(AAMPEventType type);

/**
 * @fn aamp_CreateTimedMetadataJSObject
 * @param[in] context JS execution context
 * @param[in] timeMS time in milliseconds, mostly metadata position in playlist
 * @param[in] szName name of the metadata tag
 * @param[in] szContent metadata associated with the tag
 * @param[in] id adbreak/reservation ID if its a adbreak metadata
 * @param[in] durationMS duration of ad break if its a adbreak metadata
 * @retval JSObject of TimedMetadata generated
 */
JSObjectRef aamp_CreateTimedMetadataJSObject(JSContextRef context, long long timeMS, const char* szName, const char* szContent, const char* id, double durationMS);

#endif /* __AAMP_JSUTILS_H__ */
