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
 * @file jsbindings.cpp
 * @brief JavaScript bindings for AAMP
 */


#include <JavaScriptCore/JavaScript.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "jsutils.h"
#include "main_aamp.h"
#include "priv_aamp.h"

#define GLOBAL_AAMP_NATIVEBINDING_VERSION "2.3"

static class PlayerInstanceAAMP* _allocated_aamp = NULL;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

extern "C"
{

	/**
	 * @brief
	 * @param 
	 * @retval
	 */
	JS_EXPORT JSGlobalContextRef JSContextGetGlobalContext(JSContextRef);


	/**
	 * @brief
	 * @param context
	 * @retval
	 */
	JSObjectRef AAMP_JS_AddEventTypeClass(JSGlobalContextRef context);

	/**
	 * @brief
	 * @param context
	 * @param timeMS
	 * @param szName
	 * @param szContent
	 * @retval
	 */
	JSObjectRef AAMP_JS_CreateTimedMetadata(JSContextRef context, double timeMS, const char* szName, const char* szContent);
}

/**
 * @struct AAMP_JS
 * @brief
 */
struct AAMP_JS
{
	JSGlobalContextRef _ctx;
	class PlayerInstanceAAMP* _aamp;
	class AAMP_JSListener* _listeners;

	JSObjectRef _eventType;
	JSObjectRef _subscribedTags;
};


/**
 * @brief
 * @param context
 * @param constructor
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSObjectRef AAMP_class_constructor(JSContextRef context, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	*exception = aamp_GetException(context, AAMPJS_GENERIC_ERROR, "Cannot create an object of AAMP");
	return NULL;
}


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param exception
 * @retval
 */
static JSValueRef AAMP_getProperty_closedCaptionEnabled(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if (pAAMP == NULL)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.closedCaptionEnabled on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param value
 * @param exception
 * @retval
 */
static bool AAMP_setProperty_closedCaptionEnabled(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if (pAAMP == NULL)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.closedCaptionEnabled on instances of AAMP");
		return false;
	}
	return true;
}


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param exception
 * @retval
 */
static JSValueRef AAMP_getProperty_initialBufferTime(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if (pAAMP == NULL)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.initialBufferTime on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param value
 * @param exception
 * @retval
 */
static bool AAMP_setProperty_initialBufferTime(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if (pAAMP == NULL)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.initialBufferTime on instances of AAMP");
		return false;
	}
	return true;
}


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param exception
 * @retval
 */
static JSValueRef AAMP_getProperty_trickPlayEnabled(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if (pAAMP == NULL)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.trickPlayEnabled on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param value
 * @param exception
 * @retval
 */
static bool AAMP_setProperty_trickPlayEnabled(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if (pAAMP == NULL)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.trickPlayEnabled on instances of AAMP");
		return false;
	}
	return true;
}


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param exception
 * @retval
 */
static JSValueRef AAMP_getProperty_EventType(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if (pAAMP == NULL)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.EventType on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
	return pAAMP->_eventType;
}


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param exception
 * @retval
 */
static JSValueRef AAMP_getProperty_MediaType(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (pAAMP == NULL)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.mediaType on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (pAAMP->_aamp->IsLive())
	{
		return aamp_CStringToJSValue(context, "live");
	}
	else
	{
		return aamp_CStringToJSValue(context, "vod");
	}
}


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param exception
 * @retval
 */
static JSValueRef AAMP_getProperty_Version(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	TRACELOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (pAAMP == NULL)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.version on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	return aamp_CStringToJSValue(context, GLOBAL_AAMP_NATIVEBINDING_VERSION);
}


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param exception
 * @retval
 */
static JSValueRef AAMP_getProperty_AudioLanguage(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	TRACELOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (pAAMP == NULL)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.audioLanguage on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	const char* language = pAAMP->_aamp->GetCurrentAudioLanguage();
	return aamp_CStringToJSValue(context, language);
}


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param exception
 * @retval
 */
static JSValueRef AAMP_getProperty_timedMetadata(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (pAAMP == NULL)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.timedMetadata on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	PrivateInstanceAAMP* privAAMP = (pAAMP->_aamp != NULL) ? pAAMP->_aamp->aamp : NULL;
	if (privAAMP == NULL)
	{
		ERROR("[AAMP_JS] %s() privAAMP not initialized", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "AAMP.timedMetadata - initialization error");
		return JSValueMakeUndefined(context);
	}

	int32_t length = privAAMP->timedMetadata.size();

	JSValueRef* array = new JSValueRef[length];
	for (int32_t i = 0; i < length; i++)
	{
		TimedMetadata item = privAAMP->timedMetadata.at(i);
		JSObjectRef ref = AAMP_JS_CreateTimedMetadata(context, item._timeMS, item._name.c_str(), item._content.c_str());
		array[i] = ref;
	}

	JSValueRef prop = JSObjectMakeArray(context, length, array, NULL);
	delete [] array;

	return prop;
}


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param value
 * @param exception
 * @retval
 */
static bool AAMP_setProperty_stallErrorCode(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (pAAMP == NULL)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.stallErrorCode on instances of AAMP");
		return false;
	}

	pAAMP->_aamp->SetStallErrorCode(JSValueToNumber(context, value, exception));
	return true;
}


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param value
 * @param exception
 * @retval
 */
static bool AAMP_setProperty_stallTimeout(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (pAAMP == NULL)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.stallTimeout on instances of AAMP");
		return false;
	}

	pAAMP->_aamp->SetStallTimeout(JSValueToNumber(context, value, exception));
	return true;
}

static bool AAMP_setProperty_reportInterval(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (pAAMP == NULL)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.reportInterval on instances of AAMP");
		return false;
	}

	pAAMP->_aamp->SetReportInterval(JSValueToNumber(context, value, exception));
	return true;
}

static const JSStaticValue AAMP_static_values[] =
{
	{"closedCaptionEnabled", AAMP_getProperty_closedCaptionEnabled, AAMP_setProperty_closedCaptionEnabled, kJSPropertyAttributeDontDelete },
	{"initialBufferTime", AAMP_getProperty_initialBufferTime, AAMP_setProperty_initialBufferTime, kJSPropertyAttributeDontDelete },
	{"trickPlayEnabled", AAMP_getProperty_trickPlayEnabled, AAMP_setProperty_trickPlayEnabled, kJSPropertyAttributeDontDelete },
	{"EventType", AAMP_getProperty_EventType, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{"mediaType", AAMP_getProperty_MediaType, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{"version", AAMP_getProperty_Version, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{"audioLanguage", AAMP_getProperty_AudioLanguage, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{"timedMetadata", AAMP_getProperty_timedMetadata, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{"stallErrorCode", NULL, AAMP_setProperty_stallErrorCode, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{"stallTimeout", NULL, AAMP_setProperty_stallTimeout, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{"reportInterval", NULL, AAMP_setProperty_reportInterval, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{NULL, NULL, NULL, 0}
};


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param exception
 * @retval
 */
static JSValueRef Event_getProperty_type(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	const AAMPEvent* pEvent = (const AAMPEvent*)JSObjectGetPrivate(thisObject); 
	if (pEvent == NULL)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call Event.type on instances of AAMPEvent");
		return JSValueMakeUndefined(context);
	}
	return JSValueMakeNumber(context, pEvent->type);
}

static const JSStaticValue Event_staticprops[] =
{
	{ "type", Event_getProperty_type, NULL, kJSPropertyAttributeReadOnly },
	{ NULL, NULL, NULL, 0 }
};

static const JSStaticFunction Event_staticfunctions[] =
{
	{ NULL, NULL, 0 }
};


/**
 * @brief
 * @param ctx
 * @param thisObject
 */
static void Event_init(JSContextRef ctx, JSObjectRef thisObject)
{
	//LOG("[AAMP_JS] %s()", __FUNCTION__);
}


/**
 * @brief
 * @param thisObject
 */
static void Event_finalize(JSObjectRef thisObject)
{
	//noisy - large (>400) burst of logging seen during garbage collection
	//LOG("[AAMP_JS] %s()", __FUNCTION__);

	const AAMPEvent* pEvent = (const AAMPEvent*)JSObjectGetPrivate(thisObject); 
	JSObjectSetPrivate(thisObject, NULL);
}


/**
 * @brief
 * @retval
 */
static JSClassRef Event_class_ref();


/**
 * @brief
 * @param ctx
 * @param constructor
 * @param argumentCount
 * @param arguments[]
 * @param execption
 * @retval
 */
static JSObjectRef Event_constructor(JSContextRef ctx, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* execption)
{
	//LOG("[AAMP_JS] %s()", __FUNCTION__);
	return JSObjectMake(ctx, Event_class_ref(), NULL);
}

static const JSClassDefinition Event_object_def =
{
	0,
	kJSClassAttributeNone,
	"__Event__AAMP",
	NULL,
	Event_staticprops,
	Event_staticfunctions,
	Event_init,
	Event_finalize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	Event_constructor,
	NULL,
	NULL
};


/**
 * @brief
 * @retval
 */
static JSClassRef Event_class_ref() {
	static JSClassRef _classRef = NULL;
	if (!_classRef) {
		_classRef = JSClassCreate(&Event_object_def);
	}
	return _classRef;
}

/**
 * @class AAMP_JSListener
 * @brief
 */
class AAMP_JSListener : public AAMPEventListener
{
public:

	/**
	 * @brief
	 * @param aamp
	 * @param type
	 * @param jsCallback
	 */
	static void AddEventListener(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback);

	/**
	 * @brief
	 * @param aamp
	 * @param type
	 * @param jsCallback
	 */
	static void RemoveEventListener(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback);

	AAMP_JSListener(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback)
		: _aamp(aamp)
		, _type(type)
		, _jsCallback(jsCallback)
		, _pNext(NULL)
	{
		LOG("[AAMP_JS] %s() ctx=%p, type=%d, jsCallback=%p", __FUNCTION__, _aamp->_ctx, _type, _jsCallback);
		JSValueProtect(_aamp->_ctx, _jsCallback);
	}


	/**
	 * @brief
	 * @retval
	 */
	virtual ~AAMP_JSListener()
	{
		LOG("[AAMP_JS] %s() ctx=%p, type=%d, jsCallback=%p", __FUNCTION__, _aamp->_ctx, _type, _jsCallback);
		JSValueUnprotect(_aamp->_ctx, _jsCallback);
	}


	/**
	 * @brief
	 * @param e
	 */
	void Event(const AAMPEvent& e)
	{
		if(e.type != AAMP_EVENT_PROGRESS)//log all events except progress which spams
			LOG("[AAMP_JS] %s() ctx=%p, type=%d, jsCallback=%p", __FUNCTION__, _aamp->_ctx, e.type, _jsCallback);

		JSObjectRef eventObj = JSObjectMake(_aamp->_ctx, Event_class_ref(), NULL);
		JSObjectSetPrivate(eventObj, (void*)&e);
		setEventProperties(e, _aamp->_ctx, eventObj);
		JSValueProtect(_aamp->_ctx, eventObj);
		JSValueRef args[1] = { eventObj };
		JSObjectCallAsFunction(_aamp->_ctx, _jsCallback, NULL, 1, args, NULL);
		JSValueUnprotect(_aamp->_ctx, eventObj);
	}


	/**
	 * @brief
	 * @param e
	 * @param context
	 * @param eventObj
	 */
	virtual void setEventProperties(const AAMPEvent& e, JSContextRef context, JSObjectRef eventObj)
	{
	}

public:
	AAMP_JS*			_aamp;
	AAMPEventType		_type;
	JSObjectRef			_jsCallback;
	AAMP_JSListener*	_pNext;
};

/**
 * @class AAMP_JSListener_Progress
 * @brief
 */
class AAMP_JSListener_Progress : public AAMP_JSListener
{
public:

	/**
	 * @brief AAMP_JSListener_Progress Constructor
	 * @param aamp
	 * @param type
	 * @param jsCallback
	 */
	AAMP_JSListener_Progress(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief
	 * @param e
	 * @param context
	 * @param eventObj
	 */
	void setEventProperties(const AAMPEvent& e, JSContextRef context, JSObjectRef eventObj)
	{
		JSStringRef name;

		name = JSStringCreateWithUTF8CString("durationMiliseconds");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, e.data.progress.durationMiliseconds), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("positionMiliseconds");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, e.data.progress.positionMiliseconds), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("playbackSpeed");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, e.data.progress.playbackSpeed), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("startMiliseconds");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, e.data.progress.startMiliseconds), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("endMiliseconds");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, e.data.progress.endMiliseconds), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);
	}
};

/**
 * @class AAMP_JSListener_BitRateChanged
 * @brief
 */
class AAMP_JSListener_BitRateChanged : public AAMP_JSListener
{
public:

	/**
	 * @brief AAMP_JSListener_BitRateChanged Constructor
	 * @param aamp
	 * @param type
	 * @param jsCallback
	 */
	AAMP_JSListener_BitRateChanged(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief
	 * @param e
	 * @param context
	 * @param eventObj
	 */
	void setEventProperties(const AAMPEvent& e, JSContextRef context, JSObjectRef eventObj)
	{
		JSStringRef name;
		name = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, e.data.bitrateChanged.time), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("bitRate");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, e.data.bitrateChanged.bitrate), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("description");
		JSObjectSetProperty(context, eventObj, name, aamp_CStringToJSValue(context,e.data.bitrateChanged.description ), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("width");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, e.data.bitrateChanged.width), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("height");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, e.data.bitrateChanged.height), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

	}
};

/**
 * @class AAMP_JSListener_SpeedChanged
 * @brief
 */
class AAMP_JSListener_SpeedChanged : public AAMP_JSListener
{
public:

        /**
         * @brief AAMP_JSListener_SpeedChanged Constructor
         * @param aamp
         * @param type
         * @param jsCallback
         */
	AAMP_JSListener_SpeedChanged(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief
	 * @param e
	 * @param context
	 * @param eventObj
	 */
	void setEventProperties(const AAMPEvent& e, JSContextRef context, JSObjectRef eventObj)
	{
		JSStringRef name;

		name = JSStringCreateWithUTF8CString("speed");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, e.data.speedChanged.rate), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("reason");
		JSObjectSetProperty(context, eventObj, name, aamp_CStringToJSValue(context, "unknown"), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);
	}
};

/**
 * @class AAMP_JSListener_TuneFailed
 * @brief
 */
class AAMP_JSListener_TuneFailed : public AAMP_JSListener
{
public:

        /**
         * @brief AAMP_JSListener_TuneFailed Constructor
         * @param aamp
         * @param type
         * @param jsCallback
         */
	AAMP_JSListener_TuneFailed(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief
	 * @param e
	 * @param context
	 * @param eventObj
	 */
	void setEventProperties(const AAMPEvent& e, JSContextRef context, JSObjectRef eventObj)
	{
		JSStringRef name;
		/*Keeping playerRecoveryEnabled  as false until we get more info on this*/
                name = JSStringCreateWithUTF8CString("recoveryEnabled");
                JSObjectSetProperty(context, eventObj, name, JSValueMakeBoolean(context, false), kJSPropertyAttributeReadOnly, NULL);
                JSStringRelease(name);
		int code = e.data.mediaError.code;
		const char* description = e.data.mediaError.description;

                name = JSStringCreateWithUTF8CString("code");
                JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, code), kJSPropertyAttributeReadOnly, NULL);
                JSStringRelease(name);

                name = JSStringCreateWithUTF8CString("description");
                JSObjectSetProperty(context, eventObj, name, aamp_CStringToJSValue(context, description), kJSPropertyAttributeReadOnly, NULL);
                JSStringRelease(name);
	}
};

/**
 * @class AAMP_JSListener_CCHandleReceived
 * @brief
 */
class AAMP_JSListener_CCHandleReceived : public AAMP_JSListener
{
public:
	AAMP_JSListener_CCHandleReceived(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief
	 * @param e
	 * @param context
	 * @param eventObj
	 */
	void setEventProperties(const AAMPEvent& e, JSContextRef context, JSObjectRef eventObj)
	{
		JSStringRef name;

		name = JSStringCreateWithUTF8CString("decoderHandle");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, e.data.ccHandle.handle), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);
	}
};

/**
 * @class AAMP_JSListener_VideoMetadata
 * @brief
 */
class AAMP_JSListener_VideoMetadata : public AAMP_JSListener
{
public:

        /**
         * @brief AAMP_JSListener_VideoMetadata Constructor
         * @param aamp
         * @param type
         * @param jsCallback
         */
	AAMP_JSListener_VideoMetadata(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief
	 * @param e
	 * @param context
	 * @param eventObj
	 */
	void setEventProperties(const AAMPEvent& e, JSContextRef context, JSObjectRef eventObj)
	{
		JSStringRef name;
		name = JSStringCreateWithUTF8CString("durationMiliseconds");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, e.data.metadata.durationMiliseconds), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		JSValueRef* array = new JSValueRef[e.data.metadata.languageCount];
		for (int32_t i = 0; i < e.data.metadata.languageCount; i++)
		{
			JSValueRef lang = aamp_CStringToJSValue(context, e.data.metadata.languages[i]);
			array[i] = lang;
			//JSValueRelease(lang);
		}
		JSValueRef prop = JSObjectMakeArray(context, e.data.metadata.languageCount, array, NULL);
		delete [] array;

		name = JSStringCreateWithUTF8CString("languages");
		JSObjectSetProperty(context, eventObj, name, prop, kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		array = new JSValueRef[e.data.metadata.bitrateCount];
		for (int32_t i = 0; i < e.data.metadata.bitrateCount; i++)
		{
			array[i] = JSValueMakeNumber(context, e.data.metadata.bitrates[i]);
		}
		prop = JSObjectMakeArray(context, e.data.metadata.bitrateCount, array, NULL);
		delete [] array;

		name = JSStringCreateWithUTF8CString("bitrates");
		JSObjectSetProperty(context, eventObj, name, prop, kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("width");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, e.data.metadata.width), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("height");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, e.data.metadata.height), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("hasDrm");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeBoolean(context, e.data.metadata.hasDrm), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);
	}
};

/**
 * @class AAMP_JSListener_TimedMetadata
 * @brief
 */
class AAMP_JSListener_TimedMetadata : public AAMP_JSListener
{
public:

        /**
         * @brief AAMP_JSListener_TimedMetadata Constructor
         * @param aamp
         * @param type
         * @param jsCallback
         */
	AAMP_JSListener_TimedMetadata(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief
	 * @param e
	 * @param context
	 * @param eventObj
	 */
	void setEventProperties(const AAMPEvent& e, JSContextRef context, JSObjectRef eventObj)
	{
		JSObjectRef timedMetadata = AAMP_JS_CreateTimedMetadata(context, e.data.timedMetadata.timeMilliseconds, e.data.timedMetadata.szName, e.data.timedMetadata.szContent);
		if (timedMetadata) {
			JSStringRef name = JSStringCreateWithUTF8CString("timedMetadata");
			JSObjectSetProperty(context, eventObj, name, timedMetadata, kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(name);
		}
	}
};

/**
 * @class AAMP_JSListener_StatusChanged
 * @brief
 */
class AAMP_JSListener_StatusChanged : public AAMP_JSListener
{
public:

        /**
         * @brief AAMP_JSListener_StatusChanged Constructor
         * @param aamp
         * @param type
         * @param jsCallback
         */
	AAMP_JSListener_StatusChanged(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief
	 * @param e
	 * @param context
	 * @param eventObj
	 */
	void setEventProperties(const AAMPEvent& e, JSContextRef context, JSObjectRef eventObj)
	{
		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("state");
		JSObjectSetProperty(context, eventObj, prop, JSValueMakeNumber(context, e.data.stateChanged.state), kJSPropertyAttributeReadOnly, NULL);
                JSStringRelease(prop);

	}
};


/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_addEventListener(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);

	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if (pAAMP == NULL)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.addEventListener on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount >= 2)
	{
		JSObjectRef callbackObj = JSValueToObject(context, arguments[1], NULL);

		if (callbackObj != NULL && JSObjectIsFunction(context, callbackObj))
		{
			char* type = aamp_JSValueToCString(context, arguments[0], NULL);
			AAMPEventType eventType = aamp_getEventTypeFromName(type);
			LOG("[AAMP_JS] %s() eventType='%s', %d", __FUNCTION__, type, eventType);

			if ((eventType >= 0) && (eventType < AAMP_MAX_NUM_EVENTS))
			{
				AAMP_JSListener::AddEventListener(pAAMP, eventType, callbackObj);
			}

			delete[] type;
		}
		else
		{
			ERROR("[AAMP_JS] %s() callbackObj=%p, JSObjectIsFunction(context, callbackObj)=%d", __FUNCTION__, callbackObj, JSObjectIsFunction(context, callbackObj));
			*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.addEventListener' - parameter 2 is not a function");
		}
	}
	else
	{
		ERROR("[AAMP_JS] %s() InvalidArgument: argumentCount=%d, expected: 2", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.addEventListener' - 2 arguments required.");
	}

	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param aamp
 * @param type
 * @param jsCallback
 */
void AAMP_JSListener::AddEventListener(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback)
{
	LOG("[AAMP_JS] %s(%p, %d, %p)", __FUNCTION__, aamp, type, jsCallback);

	AAMP_JSListener* pListener = 0;

	if(type == AAMP_EVENT_PROGRESS)
	{
		pListener = new AAMP_JSListener_Progress(aamp, type, jsCallback);
	}
	else if(type == AAMP_EVENT_SPEED_CHANGED)
	{
		pListener = new AAMP_JSListener_SpeedChanged(aamp, type, jsCallback);
	}
	else if(type == AAMP_EVENT_CC_HANDLE_RECEIVED)
	{
		pListener = new AAMP_JSListener_CCHandleReceived(aamp, type, jsCallback);
	}
	else if(type == AAMP_EVENT_VIDEO_METADATA)
	{
		pListener = new AAMP_JSListener_VideoMetadata(aamp, type, jsCallback);
	}
	else if(type == AAMP_EVENT_TUNE_FAILED)
	{
		pListener = new AAMP_JSListener_TuneFailed(aamp, type, jsCallback);
	}
	else if(type == AAMP_EVENT_BITRATE_CHANGED)
	{
		pListener = new AAMP_JSListener_BitRateChanged(aamp, type, jsCallback);
	}
	else if(type == AAMP_EVENT_TIMED_METADATA)
	{
		pListener = new AAMP_JSListener_TimedMetadata(aamp, type, jsCallback);
	}
	else if(type == AAMP_EVENT_STATUS_CHANGED)
	{
		pListener = new AAMP_JSListener_StatusChanged(aamp, type, jsCallback);
	}
	else
	{
		pListener = new AAMP_JSListener(aamp, type, jsCallback);
	}

	pListener->_pNext = aamp->_listeners;
	aamp->_listeners = pListener;
	aamp->_aamp->AddEventListener(type, pListener);
}


/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_removeEventListener(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);

	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if (pAAMP == NULL)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.removeEventListener on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount >= 2)
	{
		JSObjectRef callbackObj = JSValueToObject(context, arguments[1], NULL);

		if (callbackObj != NULL && JSObjectIsFunction(context, callbackObj))
		{
			char* type = aamp_JSValueToCString(context, arguments[0], NULL);
			AAMPEventType eventType = aamp_getEventTypeFromName(type);
			LOG("[AAMP_JS] %s() eventType='%s', %d", __FUNCTION__, type, eventType);

			if ((eventType >= 0) && (eventType < AAMP_MAX_NUM_EVENTS))
			{

				AAMP_JSListener::RemoveEventListener(pAAMP, eventType, callbackObj);
			}

			delete[] type;
		}
		else
		{
			ERROR("[AAMP_JS] %s() InvalidArgument: callbackObj=%p, JSObjectIsFunction(context, callbackObj)=%d", __FUNCTION__, callbackObj, JSObjectIsFunction(context, callbackObj));
			*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.removeEventListener' - parameter 2 is not a function");
		}
	}
	else
	{
		ERROR("[AAMP_JS] %s() InvalidArgument: argumentCount=%d, expected: 2", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.removeEventListener' - 2 arguments required.");
	}

	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param aamp
 * @param type
 * @param jsCallback
 */
void AAMP_JSListener::RemoveEventListener(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback)
{
	LOG("[AAMP_JS] %s(%p, %d, %p)", __FUNCTION__, aamp, type, jsCallback);

	AAMP_JSListener** ppListener = &aamp->_listeners;
	while (*ppListener != NULL)
	{
		AAMP_JSListener* pListener = *ppListener;
		if ((pListener->_type == type) && (pListener->_jsCallback == jsCallback))
		{
			*ppListener = pListener->_pNext;
			aamp->_aamp->RemoveEventListener(type, pListener);
			delete pListener;
			return;
		}
		ppListener = &pListener->_pNext;
	}
}


/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_setProperties(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_getProperties(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_tune(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if(!pAAMP)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.tune on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	char* contentType = NULL;
	switch(argumentCount)
	{
		case 2:
			{
				contentType = aamp_JSValueToCString(context, arguments[1], exception);
			}
		case 1:
			{
				char* url = aamp_JSValueToCString(context, arguments[0], exception);
				pAAMP->_aamp->Tune(url, contentType);
				delete [] url;
			}
			if(NULL != contentType)
			{
				delete [] contentType;
			}
			break;
		default:
			ERROR("[AAMP_JS] %s() InvalidArgument: argumentCount=%d, expected: 1 or 2", __FUNCTION__, argumentCount);
			*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.tune' - 1 argument required");
			break;
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_stop(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if(!pAAMP)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.stop on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
	pAAMP->_aamp->Stop();
	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_setRate(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if(!pAAMP)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setRate on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount < 1)
	{
		ERROR("[AAMP_JS] %s() InvalidArgument: argumentCount=%d, expected: 2", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setRate' - 2 arguments required");
	}
	else
	{
		int overshoot = 0;
		float rate = (float)JSValueToNumber(context, arguments[0], exception);
		// present JS doesnt support overshoot , check for arguement count and store.
		if(argumentCount > 1)
		{
			overshoot = (int)JSValueToNumber(context, arguments[1], exception);
		}
		LOG("[AAMP_JS] %s () rate=%f, overshoot=%d", __FUNCTION__, rate, overshoot);
		pAAMP->_aamp->SetRate(rate,overshoot);
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_seek(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if(!pAAMP)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.seek on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
		ERROR("[AAMP_JS] %s() InvalidArgument: argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.seek' - 1 argument required");
	}
	else
	{
		double position = JSValueToNumber(context, arguments[0], exception);
		LOG("[AAMP_JS] %s () position=%g", __FUNCTION__, position);
		pAAMP->_aamp->Seek(position);
	}
	return JSValueMakeUndefined(context);
}



/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_seekToLive(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if(!pAAMP)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.seekToLive on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
	pAAMP->_aamp->SeekToLive();
	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_setRect(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if(!pAAMP)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setRect on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 4)
	{
		ERROR("[AAMP_JS] %s() InvalidArgument: argumentCount=%d, expected: 4", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setRect' - 4 arguments required");
	}
	else
	{
		int x = JSValueToNumber(context, arguments[0], exception);
		int y = JSValueToNumber(context, arguments[1], exception);
		int w = JSValueToNumber(context, arguments[2], exception);
		int h = JSValueToNumber(context, arguments[3], exception);
		pAAMP->_aamp->SetVideoRectangle(x,y,w,h);
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_setVideoMute(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__); 
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setVideoMute on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
		ERROR("[AAMP_JS] %s() InvalidArgument: argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setVideoMute' - 1 argument required");
	}
	else
	{
		bool muted = JSValueToBoolean(context, arguments[0]);
		pAAMP->_aamp->SetVideoMute(muted);
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_setAudioVolume(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setAudioVolume on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
		ERROR("[AAMP_JS] %s() InvalidArgument: argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setAudioVolume' - 1 argument required");
	}
	else
	{
		unsigned int volume = JSValueToNumber(context, arguments[0], exception);
		pAAMP->_aamp->SetAudioVolume(volume);
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_setZoom(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if(!pAAMP)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setZoom on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
		ERROR("[AAMP_JS] %s() InvalidArgument: argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setZoom' - 1 argument required");
	}
	else
	{
		VideoZoomMode zoom;
		char* zoomStr = aamp_JSValueToCString(context, arguments[0], exception);
		LOG("[AAMP_JS] %s() zoomStr %s", __FUNCTION__, zoomStr);
		if (0 == strcmp(zoomStr, "none"))
		{
			zoom = VIDEO_ZOOM_NONE;
		}
		else
		{
			zoom = VIDEO_ZOOM_FULL;
		}
		pAAMP->_aamp->SetVideoZoom(zoom);
		delete[] zoomStr;
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_setLanguage(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if(!pAAMP)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setLanguage on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
		ERROR("[AAMP_JS] %s() InvalidArgument: argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setLanguage' - 1 argument required");
	}
	else
	{
		char* lang = aamp_JSValueToCString(context, arguments[0], exception);
		pAAMP->_aamp->SetLanguage(lang);
		delete [] lang;
	}
	return JSValueMakeUndefined(context);
}
 

/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_setSubscribeTags(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if(!pAAMP)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.subscribedTags on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
		ERROR("[AAMP_JS] %s() InvalidArgument: argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setSubscribeTags' - 1 argument required");
	}
	else if (!aamp_JSValueIsArray(context, arguments[0]))
	{
		ERROR("[AAMP_JS] %s() InvalidArgument: aamp_JSValueIsArray=%d", __FUNCTION__, aamp_JSValueIsArray(context, arguments[0]));
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setSubscribeTags' - parameter 1 is not an array");
	}
	else
	{
		if(pAAMP->_subscribedTags)
		{
			JSValueUnprotect(pAAMP->_ctx, pAAMP->_subscribedTags);
		}
		pAAMP->_subscribedTags = JSValueToObject(context, arguments[0], NULL);
		if(pAAMP->_subscribedTags)
		{
			JSValueProtect(pAAMP->_ctx, pAAMP->_subscribedTags);
			std::vector<std::string> subscribedTags = aamp_StringArrayToCStringArray(context, arguments[0]);
			pAAMP->_aamp->SetSubscribedTags(subscribedTags);
		}
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_addCustomHTTPHeader(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.addCustomHTTPHeader on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 2)
	{
		ERROR("[AAMP_JS] %s() InvalidArgument: argumentCount=%d, expected: 2", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.addCustomHTTPHeader' - 2 argument required");
	}
	else
	{
		char *name = aamp_JSValueToCString(context, arguments[0], exception);
		std::string headerName(name);
		std::vector<std::string> headerVal;

		delete[] name;

		if (aamp_JSValueIsArray(context, arguments[1]))
		{
			headerVal = aamp_StringArrayToCStringArray(context, arguments[1]);
		}
		else if (JSValueIsString(context, arguments[1]))
		{
			headerVal.reserve(1);
			char *value =  aamp_JSValueToCString(context, arguments[1], exception);
			headerVal.push_back(value);
			delete[] value;
		}

		// Don't support empty values now
		if (headerVal.size() == 0)
		{
			ERROR("[AAMP_JS] %s() InvalidArgument: Custom header's value is empty", __FUNCTION__, argumentCount);
			*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.addCustomHTTPHeader' - 2nd argument should be a string or array of strings");
			return JSValueMakeUndefined(context);
		}

		pAAMP->_aamp->AddCustomHTTPHeader(headerName, headerVal);
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_removeCustomHTTPHeader(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.removeCustomHTTPHeader on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
		ERROR("[AAMP_JS] %s() InvalidArgument: argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.removeCustomHTTPHeader' - 1 argument required");
	}
	else
	{
		char *name = aamp_JSValueToCString(context, arguments[0], exception);
		std::string headerName(name);
		pAAMP->_aamp->AddCustomHTTPHeader(headerName, std::vector<std::string>());
		delete[] name;

	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_setAds(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_getAudioTrackList(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_getAudioTrack(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_setAudioTrack(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_setClosedCaptionTrack(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	return JSValueMakeUndefined(context);
}

/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_setLicenseServerURL(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (!pAAMP)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setLicenseServerURL on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
		ERROR("[AAMP_JS] %s() InvalidArgument: argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setLicenseServerURL' - 1 argument required");
	}
	else
	{
		char *url = aamp_JSValueToCString(context, arguments[0], exception);
		pAAMP->_aamp->SetLicenseServerURL(url);
		delete [] url;
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_setAnonymousRequest(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (!pAAMP)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setAnonymousRequest on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
		ERROR("[AAMP_JS] %s() InvalidArgument: argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setAnonymousRequest' - 1 argument required");
	}
	else
	{
		bool isAnonymousReq = JSValueToBoolean(context, arguments[0]);
		pAAMP->_aamp->SetAnonymousRequest(isAnonymousReq);
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_setVODTrickplayFPS(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
        LOG("[AAMP_JS] %s()", __FUNCTION__);
        AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
        if (!pAAMP)
        {
                ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
                *exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setAnonymousRequest on instances of AAMP");
                return JSValueMakeUndefined(context);
        }

        if (argumentCount != 1)
        {
                ERROR("[AAMP_JS] %s() InvalidArgument: argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
                *exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setAnonymousRequest' - 1 argument required");
        }
        else
        {
                int vodTrickplayFPS = (int)JSValueToNumber(context, arguments[0], exception);
                pAAMP->_aamp->SetVODTrickplayFPS(vodTrickplayFPS);
        }
        return JSValueMakeUndefined(context);
}


/**
 * @brief
 * @param context
 * @param function
 * @param thisObject
 * @param argumentCount
 * @param arguments[]
 * @param exception
 * @retval
 */
static JSValueRef AAMP_setLinearTrickplayFPS(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
        LOG("[AAMP_JS] %s()", __FUNCTION__);
        AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
        if (!pAAMP)
        {
                ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
                *exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setAnonymousRequest on instances of AAMP");
                return JSValueMakeUndefined(context);
        }

        if (argumentCount != 1)
        {
                ERROR("[AAMP_JS] %s() InvalidArgument: argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
                *exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setAnonymousRequest' - 1 argument required");
        }
        else
        {
                int linearTrickplayFPS = (int)JSValueToNumber(context, arguments[0], exception);
                pAAMP->_aamp->SetLinearTrickplayFPS(linearTrickplayFPS);
        }
        return JSValueMakeUndefined(context);
}

static const JSStaticFunction AAMP_staticfunctions[] =
{
	{ "addEventListener", AAMP_addEventListener, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "removeEventListener", AAMP_removeEventListener, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setProperties", AAMP_setProperties, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "getProperties", AAMP_getProperties, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "tune", AAMP_tune, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "stop", AAMP_stop, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setRate", AAMP_setRate, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "seek", AAMP_seek, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "seekToLive", AAMP_seekToLive, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setRect", AAMP_setRect, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setZoom", AAMP_setZoom, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setLanguage", AAMP_setLanguage, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setSubscribeTags", AAMP_setSubscribeTags, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setAds", AAMP_setAds, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "subscribeTimedMetadata", AAMP_setSubscribeTags, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "getAudioTrackList", AAMP_getAudioTrackList, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "getAudioTrack", AAMP_getAudioTrack, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setAudioTrack", AAMP_setAudioTrack, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setClosedCaptionTrack", AAMP_setClosedCaptionTrack, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setVideoMute", AAMP_setVideoMute, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setAudioVolume", AAMP_setAudioVolume, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "addCustomHTTPHeader", AAMP_addCustomHTTPHeader, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "removeCustomHTTPHeader", AAMP_removeCustomHTTPHeader, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setLicenseServerURL", AAMP_setLicenseServerURL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setAnonymousRequest", AAMP_setAnonymousRequest, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },

	{ "setVODTrickplayFPS", AAMP_setVODTrickplayFPS, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },

	{ "setLinearTrickplayFPS", AAMP_setLinearTrickplayFPS, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ NULL, NULL, 0 }
};


/**
 * @brief
 * @param thisObject
 */
static void AAMP_finalize(JSObjectRef thisObject)
{
	LOG("[AAMP_JS] %s(): object=%p", __FUNCTION__, thisObject);

	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if (pAAMP == NULL)
	{
		ERROR("[AAMP_JS] %s() Error: JSObjectGetPrivate returned NULL!", __FUNCTION__);
		return;
	}
	JSObjectSetPrivate(thisObject, NULL);

	while (pAAMP->_listeners != NULL)
	{
		AAMPEventType eventType = pAAMP->_listeners->_type;
		JSObjectRef   jsCallback  = pAAMP->_listeners->_jsCallback;

		AAMP_JSListener::RemoveEventListener(pAAMP, eventType, jsCallback);
	}

	if (pAAMP->_ctx != NULL) {
		JSValueUnprotect(pAAMP->_ctx, pAAMP->_eventType);
		if(pAAMP->_subscribedTags) {
			JSValueUnprotect(pAAMP->_ctx, pAAMP->_subscribedTags);
		}
	}

	pthread_mutex_lock(&mutex);
	if (NULL != _allocated_aamp)
	{
		LOG("[AAMP_JS] %s:%d delete aamp %p\n", __FUNCTION__, __LINE__, _allocated_aamp);
		delete _allocated_aamp;
		_allocated_aamp = NULL;
	}
	pthread_mutex_unlock(&mutex);
	delete pAAMP;
}

static const JSClassDefinition AAMP_class_def =
{
	0,
	kJSClassAttributeNone,
	"__AAMP__class",
	NULL,
	AAMP_static_values,
	AAMP_staticfunctions,
	NULL,
	AAMP_finalize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	AAMP_class_constructor,
	NULL,
	NULL
};


/**
 * @brief
 * @retval
 */
static JSClassRef AAMP_class_ref() {
	static JSClassRef _classRef = NULL;
	if (!_classRef) {
		_classRef = JSClassCreate(&AAMP_class_def);
	}
	return _classRef;
}


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param exception
 * @retval
 */
static JSValueRef EventType_getproperty_AD_PLAYBACK_STARTED(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	return aamp_CStringToJSValue(context, "adPlaybackStarted");
}


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param exception
 * @retval
 */
static JSValueRef EventType_getproperty_AD_PLAYBACK_COMPLETED(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	return aamp_CStringToJSValue(context, "adPlaybackCompleted");
}


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param exception
 * @retval
 */
static JSValueRef EventType_getproperty_AD_PLAYBACK_INTERRUPTED(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	return aamp_CStringToJSValue(context, "adPlaybackInterrupted");
}


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param exception
 * @retval
 */
static JSValueRef EventType_getproperty_BUFFERING_BEGIN(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	return aamp_CStringToJSValue(context, "bufferingBegin");
}


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param exception
 * @retval
 */
static JSValueRef EventType_getproperty_BUFFERING_END(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	return aamp_CStringToJSValue(context, "bufferingEnd");
}


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param exception
 * @retval
 */
static JSValueRef EventType_getproperty_DECODER_AVAILABLE(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	return aamp_CStringToJSValue(context, "decoderAvailable");
}


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param exception
 * @retval
 */
static JSValueRef EventType_getproperty_DRM_METADATA_INFO_AVAILABLE(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	return aamp_CStringToJSValue(context, "drmMetadataInfoAvailable");
}


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param exception
 * @retval
 */
static JSValueRef EventType_getproperty_ENTERING_LIVE(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	return aamp_CStringToJSValue(context, "enteringLive");
}


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param exception
 * @retval
 */
static JSValueRef EventType_getproperty_MEDIA_OPENED(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	return aamp_CStringToJSValue(context, "mediaOpened");
}


/**
 * @brief
 * @param context
 * @param thisObject
 * @param propertyName
 * @param exception
 * @retval
 */
static JSValueRef EventType_getproperty_MEDIA_STOPPED(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
	return aamp_CStringToJSValue(context, "mediaStopped");
}

static const JSStaticValue EventType_staticprops[] =
{
	{ "AD_PLAYBACK_STARTED", EventType_getproperty_AD_PLAYBACK_STARTED, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "AD_PLAYBACK_COMPLETED", EventType_getproperty_AD_PLAYBACK_COMPLETED, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "AD_PLAYBACK_INTERRUPTED", EventType_getproperty_AD_PLAYBACK_INTERRUPTED, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "BUFFERING_BEGIN", EventType_getproperty_BUFFERING_BEGIN, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "BUFFERING_END", EventType_getproperty_BUFFERING_END, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "DECODER_AVAILABLE", EventType_getproperty_DECODER_AVAILABLE, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "DRM_METADATA_INFO_AVAILABLE", EventType_getproperty_DRM_METADATA_INFO_AVAILABLE, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "ENTERING_LIVE", EventType_getproperty_ENTERING_LIVE, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "MEDIA_OPENED", EventType_getproperty_MEDIA_OPENED, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "MEDIA_STOPPED", EventType_getproperty_MEDIA_STOPPED, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ NULL, NULL, NULL, 0 }
};


/**
 * @brief
 * @param ctx
 * @param thisObject
 */
static void EventType_init(JSContextRef ctx, JSObjectRef thisObject)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
}


/**
 * @brief
 * @param thisObject
 */
static void EventType_finalize(JSObjectRef thisObject)
{
	LOG("[AAMP_JS] %s()", __FUNCTION__);
}

static const JSClassDefinition EventType_object_def =
{
	0,
	kJSClassAttributeNone,
	"__EventType",
	NULL,
	EventType_staticprops,
	NULL,
	EventType_init,
	EventType_finalize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};


/**
 * @brief
 * @retval
 */
static JSClassRef EventType_class_ref() {
	static JSClassRef _classRef = NULL;
	if (!_classRef) {
		_classRef = JSClassCreate(&EventType_object_def);
	}
	return _classRef;
}


/**
 * @brief
 * @param context
 * @retval
 */
JSObjectRef AAMP_JS_AddEventTypeClass(JSGlobalContextRef context)
{
	LOG("[AAMP_JS] %s() context=%p", __FUNCTION__, context);

	JSObjectRef obj = JSObjectMake(context, EventType_class_ref(), context);

	return obj;
}


/**
 * @brief
 * @param context
 * @param timeMS
 * @param szName
 * @param szContent
 * @retval
 */
JSObjectRef AAMP_JS_CreateTimedMetadata(JSContextRef context, double timeMS, const char* szName, const char* szContent)
{
	JSStringRef name;

	JSObjectRef timedMetadata = JSObjectMake(context, NULL, NULL);
	if (timedMetadata) {
		bool bGenerateID = true;

		name = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(context, timedMetadata, name, JSValueMakeNumber(context, (int)timeMS), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("name");
		JSObjectSetProperty(context, timedMetadata, name, aamp_CStringToJSValue(context, szName), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("content");
		JSObjectSetProperty(context, timedMetadata, name, aamp_CStringToJSValue(context, szContent), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		// Force type=0 (HLS tag) for now.
		// Does type=1 ID3 need to be supported?
		name = JSStringCreateWithUTF8CString("type");
		JSObjectSetProperty(context, timedMetadata, name, JSValueMakeNumber(context, 0), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		// Force metadata as empty object
		JSObjectRef metadata = JSObjectMake(context, NULL, NULL);
		if (metadata) {
			name = JSStringCreateWithUTF8CString("metadata");
			JSObjectSetProperty(context, timedMetadata, name, metadata, kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(name);

			// Parse CUE metadata and TRICKMODE-RESTRICTION metadata
			if ((strcmp(szName, "#EXT-X-CUE") == 0) ||
			    (strcmp(szName, "#EXT-X-TRICKMODE-RESTRICTION") == 0)) {
				const char* szStart = szContent;

				// Advance past #EXT tag.
				for (; *szStart != ':' && *szStart != '\0'; szStart++);
				if (*szStart == ':')
					szStart++;

				// Parse comma seperated name=value list.
				while (*szStart != '\0') {
					char* szSep;
					// Find the '=' seperator.
					for (szSep = (char*)szStart; *szSep != '=' && *szSep != '\0'; szSep++);

					// Find the end of the value.
					char* szEnd = (*szSep != '\0') ? szSep + 1 : szSep;
					for (; *szEnd != ',' && *szEnd != '\0'; szEnd++);

					// Append the name / value metadata.
					if ((szStart < szSep) && (szSep < szEnd)) {
						JSValueRef value;
						char chSave = *szSep;

						*szSep = '\0';
						name = JSStringCreateWithUTF8CString(szStart);
						*szSep = chSave;

						chSave = *szEnd;
						*szEnd = '\0';
						value = aamp_CStringToJSValue(context, szSep+1);
						*szEnd = chSave;

						JSObjectSetProperty(context, metadata, name, value, kJSPropertyAttributeReadOnly, NULL);
						JSStringRelease(name);

						// If we just added the 'ID', copy into timedMetadata.id
						if (szStart[0] == 'I' && szStart[1] == 'D' && szStart[2] == '=') {
							bGenerateID = false;
							name = JSStringCreateWithUTF8CString("id");
							JSObjectSetProperty(context, timedMetadata, name, value, kJSPropertyAttributeReadOnly, NULL);
							JSStringRelease(name);
						}
					}

					szStart = (*szEnd != '\0') ? szEnd + 1 : szEnd;
				}
			}

			// Parse TARGETDURATION and CONTENT-IDENTIFIER metadata
			else {
				const char* szStart = szContent;
				// Advance past #EXT tag.
				for (; *szStart != ':' && *szStart != '\0'; szStart++);
				if (*szStart == ':')
					szStart++;

				// Stuff all content into DATA name/value pair.
				JSValueRef value = aamp_CStringToJSValue(context, szStart);
				if (strcmp(szName, "#EXT-X-TARGETDURATION") == 0) {
					// Stuff into DURATION if EXT-X-TARGETDURATION content.
					name = JSStringCreateWithUTF8CString("DURATION");
				} else {
					name = JSStringCreateWithUTF8CString("DATA");
				}
				JSObjectSetProperty(context, metadata, name, value, kJSPropertyAttributeReadOnly, NULL);
				JSStringRelease(name);
			}
		}

		// Generate an ID
		if (bGenerateID) {
			int hash = (int)timeMS;
			const char* szStart = szName;
			for (; *szStart != '\0'; szStart++) {
				hash = (hash * 33) ^ *szStart;
			}

			char buf[32];
			sprintf(buf, "%d", hash);
			name = JSStringCreateWithUTF8CString("id");
			JSObjectSetProperty(context, timedMetadata, name, aamp_CStringToJSValue(context, buf), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(name);
		}
	}

	return timedMetadata;
}


/**
 * @brief
 * @param context
 * @param playerInstanceAAMP
 */
void aamp_LoadJS(void* context, void* playerInstanceAAMP)
{
	INFO("[AAMP_JS] %s() context=%p, aamp=%p", __FUNCTION__, context, playerInstanceAAMP);

	JSGlobalContextRef jsContext = (JSGlobalContextRef)context;

	AAMP_JS* pAAMP = new AAMP_JS();
	pAAMP->_ctx = jsContext;
	if (NULL != playerInstanceAAMP)
	{
		pAAMP->_aamp = (PlayerInstanceAAMP*)playerInstanceAAMP;
	}
	else
	{
		pthread_mutex_lock(&mutex);
		if (NULL == _allocated_aamp )
		{
			_allocated_aamp = new PlayerInstanceAAMP();
			LOG("[AAMP_JS] %s:%d create aamp %p\n", __FUNCTION__, __LINE__, _allocated_aamp);
		}
		else
		{
			LOG("[AAMP_JS] %s:%d reuse aamp %p\n", __FUNCTION__, __LINE__, _allocated_aamp);
		}
		pAAMP->_aamp = _allocated_aamp;
		pthread_mutex_unlock(&mutex);
	}

	pAAMP->_listeners = NULL;

	pAAMP->_eventType = AAMP_JS_AddEventTypeClass(jsContext);
	JSValueProtect(jsContext, pAAMP->_eventType);

	pAAMP->_subscribedTags = NULL;

	JSObjectRef classObj = JSObjectMake(jsContext, AAMP_class_ref(), pAAMP);
	JSObjectRef globalObj = JSContextGetGlobalObject(jsContext);
	JSStringRef str = JSStringCreateWithUTF8CString("AAMP");
	JSObjectSetProperty(jsContext, globalObj, str, classObj, kJSPropertyAttributeReadOnly, NULL);
	JSStringRelease(str);
}


/**
 * @brief
 * @param context
 */
void aamp_UnloadJS(void* context)
{
	INFO("[AAMP_JS] %s() context=%p", __FUNCTION__, context);

	JSGlobalContextRef jsContext = (JSGlobalContextRef)context;

	JSObjectRef globalObj = JSContextGetGlobalObject(jsContext);
	JSStringRef str = JSStringCreateWithUTF8CString("AAMP");
	JSValueRef aamp = JSObjectGetProperty(jsContext, globalObj, str, NULL);

	if (aamp == NULL)
	{
		JSStringRelease(str);
		return;
	}

	JSObjectRef aampObj = JSValueToObject(jsContext, aamp, NULL);
	if (aampObj == NULL)
	{
		JSStringRelease(str);
		return;
	}

	AAMP_finalize(aampObj);

	JSObjectSetProperty(jsContext, globalObj, str, JSValueMakeUndefined(jsContext), kJSPropertyAttributeReadOnly, NULL);
	JSStringRelease(str);

	// Force a garbage collection to clean-up all AAMP objects.
	LOG("[AAMP_JS] JSGarbageCollect() context=%p", context);
	JSGarbageCollect(jsContext);
}

#ifdef __GNUC__

/**
 * @brief
 */
void __attribute__ ((destructor(101))) _aamp_term()
{
	LOG("[AAMP_JS] %s:%d\n", __FUNCTION__, __LINE__);
	pthread_mutex_lock(&mutex);
	if (NULL != _allocated_aamp)
	{
		LOG("[AAMP_JS] %s:%d stopping aamp\n", __FUNCTION__, __LINE__);
		_allocated_aamp->Stop();
		LOG("[AAMP_JS] %s:%d stopped aamp\n", __FUNCTION__, __LINE__);
	}
	pthread_mutex_unlock(&mutex);
}
#endif
