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
* @file JavaScript util functions for AAMP
*/

#include "jsutils.h"

#include <stdlib.h>
#include <stdio.h>

typedef struct _EventTypeMap
{
	AAMPEventType eventType;
	const char* szName;
} EventTypeMap;

static EventTypeMap aamp_eventTypes[] =
{
	{ (AAMPEventType)0, "onEvent"},
	{ AAMP_EVENT_TUNED, "tuned"},
	{ AAMP_EVENT_TUNE_FAILED, "tuneFailed"},
	{ AAMP_EVENT_SPEED_CHANGED, "speedChanged"},
	{ AAMP_EVENT_EOS, "eos"},
	{ AAMP_EVENT_PLAYLIST_INDEXED, "playlistIndexed"},
	{ AAMP_EVENT_PROGRESS, "progress"},
	{ AAMP_EVENT_CC_HANDLE_RECEIVED, "decoderAvailable"},
	{ AAMP_EVENT_JS_EVENT, "jsEvent"},
	{ AAMP_EVENT_VIDEO_METADATA, "metadata"},
	{ AAMP_EVENT_ENTERING_LIVE, "enteringLive"},
	{ AAMP_EVENT_BITRATE_CHANGED, "bitrateChanged"},
	{ AAMP_EVENT_TIMED_METADATA, "timedMetadata"},
	{ AAMP_EVENT_STATUS_CHANGED, "statusChanged"},
	{ (AAMPEventType)0, "" }
};

JSValueRef aamp_CStringToJSValue(JSContextRef context, const char* sz)
{
	JSStringRef str = JSStringCreateWithUTF8CString(sz);
	JSValueRef value = JSValueMakeString(context, str);
	JSStringRelease(str);

	return value;
}

char* aamp_JSValueToCString(JSContextRef context, JSValueRef value, JSValueRef* exception)
{
	JSStringRef jsstr = JSValueToStringCopy(context, value, exception);
	size_t len = JSStringGetMaximumUTF8CStringSize(jsstr);
	char* src = new char[len];
	JSStringGetUTF8CString(jsstr, src, len);
	JSStringRelease(jsstr);
	return src;
}

bool aamp_JSValueIsArray(JSContextRef context, JSValueRef value)
{
	JSObjectRef global = JSContextGetGlobalObject(context);
	JSStringRef arrayProp = JSStringCreateWithUTF8CString("Array");
	JSValueRef arrayVal = JSObjectGetProperty(context, global, arrayProp, NULL);
	JSStringRelease(arrayProp);

	if (JSValueIsObject(context, arrayVal))
	{
		JSObjectRef arrayObj = JSValueToObject(context, arrayVal, NULL);
		if (JSObjectIsFunction(context, arrayObj) || JSObjectIsConstructor(context, arrayObj))
		{
			return JSValueIsInstanceOfConstructor(context, value, arrayObj, NULL);
		}
	}

	return false;
}

std::vector<std::string> aamp_StringArrayToCStringArray(JSContextRef context, JSValueRef arrayRef)
{
    std::vector<std::string> retval;
    JSValueRef exception = NULL;

    if(!arrayRef)
    {
	ERROR("[AAMP_JS] %s() Error: value is NULL.", __FUNCTION__);
        return retval;
    }
    if (!JSValueIsObject(context, arrayRef))
    {
	ERROR("[AAMP_JS] %s() Error: value is not an object.", __FUNCTION__);
        return retval;
    }
    if(!aamp_JSValueIsArray(context, arrayRef))
    {
	ERROR("[AAMP_JS] %s() Error: value is not an array.", __FUNCTION__);
        return retval;
    }
    JSObjectRef arrayObj = JSValueToObject(context, arrayRef, &exception);
    if(exception)
    {
	ERROR("[AAMP_JS] %s() Error: exception accesing array object.", __FUNCTION__);
        return retval;
    }

    JSStringRef lengthStrRef = JSStringCreateWithUTF8CString("length");
    JSValueRef lengthRef = JSObjectGetProperty(context, arrayObj, lengthStrRef, &exception);
    if(exception)
    {
	ERROR("[AAMP_JS] %s() Error: exception accesing array length.", __FUNCTION__);
        return retval;
    }
    int length = JSValueToNumber(context, lengthRef, &exception);
    if(exception)
    {
	ERROR("[AAMP_JS] %s() Error: exception array length in not a number.", __FUNCTION__);
        return retval;
    }

    retval.reserve(length);
    for(int i = 0; i < length; i++)
    {
        JSValueRef strRef = JSObjectGetPropertyAtIndex(context, arrayObj, i, &exception);
        if(exception)
            continue;

        char* str = aamp_JSValueToCString(context, strRef, NULL);
	LOG("[AAMP_JS] %s() array[%d] = '%s'.", __FUNCTION__, i, str);
        retval.push_back(str);
        delete [] str;
    }

    JSStringRelease(lengthStrRef);

    return retval;
}

JSValueRef aamp_GetException(JSContextRef context, ErrorCode error, const char *additionalInfo)
{
	const char *str = "Generic Error";
	JSValueRef retVal;

	switch(error)
	{
		case AAMPJS_INVALID_ARGUMENT:
		case AAMPJS_MISSING_OBJECT:
			str = "TypeError";
			break;
		default:
			str = "Generic Error";
			break;
	}

	char exceptionMsg[1024];
	memset(exceptionMsg, '\0', 1024);

	if(additionalInfo)
	{
		snprintf(exceptionMsg, 1023, "%s: %s", str, additionalInfo);
	}
	else
	{
		snprintf(exceptionMsg, 1023, "%s!!", str);
	}

	ERROR("[AAMP_JS] %s() Error=%s", __FUNCTION__, exceptionMsg);

	const JSValueRef arguments[] = { aamp_CStringToJSValue(context, exceptionMsg) };
	JSValueRef exception = NULL;
	retVal = JSObjectMakeError(context, 1, arguments, &exception);
	if (exception)
	{
		ERROR("[AAMP_JS] %s() Error: exception creating an error object", __FUNCTION__);
		return NULL;
	}

	return retVal;
}

AAMPEventType aamp_getEventTypeFromName(const char* szName)
{
	AAMPEventType eventType = AAMP_MAX_NUM_EVENTS;
	int numEvents = sizeof(aamp_eventTypes) / sizeof(aamp_eventTypes[0]);

	for (int i=0; i<numEvents; i++)
	{
		if (strcmp(aamp_eventTypes[i].szName, szName) == 0)
		{
			eventType = aamp_eventTypes[i].eventType;
			break;
		}
	}

	return eventType;
}

const char* aamp_getNameFromEventType(AAMPEventType type)
{
	if (type > 0 && type < AAMP_MAX_NUM_EVENTS)
	{
		return aamp_eventTypes[type].szName;
	}
	else
	{
		return NULL;
	}
}
