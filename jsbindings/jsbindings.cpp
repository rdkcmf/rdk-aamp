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

#include "jsbindings-version.h"
#include "jsutils.h"
#include "main_aamp.h"
#include "priv_aamp.h"

#ifdef AAMP_CC_ENABLED
#include "AampCCManager.h"
#endif

static class PlayerInstanceAAMP* _allocated_aamp = NULL;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

extern void ClearAAMPPlayerInstances();

extern "C"
{

	/**
	 * @brief Get the global JS execution context
	 * @param[in] JSContextRef JS execution context
	 * @retval global execution context
	 */
	JS_EXPORT JSGlobalContextRef JSContextGetGlobalContext(JSContextRef);

	JSObjectRef AAMP_JS_AddEventTypeClass(JSGlobalContextRef context);
	void aamp_ApplyPageHttpHeaders(PlayerInstanceAAMP *);
}

/**
 * @struct AAMP_JS
 * @brief Data structure of AAMP object
 */
struct AAMP_JS
{
	JSGlobalContextRef _ctx;
	class PlayerInstanceAAMP* _aamp;
	class AAMP_JSListener* _listeners;
	int  iPlayerId; /*An int variable iPlayerID to store Playerid */
        bool bInfoEnabled; /*A bool variable bInfoEnabled for INFO logging check*/
	JSObjectRef _eventType;
	JSObjectRef _subscribedTags;
	JSObjectRef _promiseCallback;	/* Callback function for JS promise resolve/reject.*/
};


/**
 * @brief callback invoked when AAMP is used along with 'new'
 * @param[in] context JS execution context
 * @param[in] constructor JSObject that is the constructor being called
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSObject that is the constructor's return value
 */
static JSObjectRef AAMP_class_constructor(JSContextRef context, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	*exception = aamp_GetException(context, AAMPJS_GENERIC_ERROR, "Cannot create an object of AAMP");
	return NULL;
}


/**
 * @brief Callback invoked from JS to get the closedCaptionEnabled property value
 * @param[in] context JS execution context
 * @param[in] thisObject JSObject to search for the property
 * @param[in] propertyName JSString containing the name of the property to get
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval property's value if object has the property, otherwise NULL
 */
static JSValueRef AAMP_getProperty_closedCaptionEnabled(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	
        LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if (pAAMP == NULL)
	{
              	LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.closedCaptionEnabled on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to set the closedCaptionEnabled property value
 * @param[in] context JS exception context
 * @param[in] thisObject JSObject on which to set the property's value
 * @param[in] propertyName JSString containing the name of the property to set
 * @param[in] value JSValue to use as the property's value
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval true if the property was set, otherwise false
 */
static bool AAMP_setProperty_closedCaptionEnabled(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
        LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if (pAAMP == NULL)
	{
                LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.closedCaptionEnabled on instances of AAMP");
		return false;
	}
	return true;
}


/**
 * @brief Callback invoked from JS to get the initialBufferTime property value
 * @param[in] context JS execution context
 * @param[in] thisObject JSObject to search for the property
 * @param[in] propertyName JSString containing the name of the property to get
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval property's value if object has the property, otherwise NULL
 */
static JSValueRef AAMP_getProperty_initialBufferTime(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{

        LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if (pAAMP == NULL)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.initialBufferTime on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to set the initialBufferTime property value
 * @param[in] context JS exception context
 * @param[in] thisObject JSObject on which to set the property's value
 * @param[in] propertyName JSString containing the name of the property to set
 * @param[in] value JSValue to use as the property's value
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval true if the property was set, otherwise false
 */
static bool AAMP_setProperty_initialBufferTime(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
        LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if (pAAMP == NULL)
	{
                LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.initialBufferTime on instances of AAMP");
		return false;
	}
	return true;
}


/**
 * @brief Callback invoked from JS to get the trickPlayEnabled property value
 * @param[in] context JS execution context
 * @param[in] thisObject JSObject to search for the property
 * @param[in] propertyName JSString containing the name of the property to get
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval property's value if object has the property, otherwise NULL
 */
static JSValueRef AAMP_getProperty_trickPlayEnabled(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if (pAAMP == NULL)
	{		
                LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.trickPlayEnabled on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to set the trickPlayEnabled property value
 * @param[in] context JS exception context
 * @param[in] thisObject JSObject on which to set the property's value
 * @param[in] propertyName JSString containing the name of the property to set
 * @param[in] value JSValue to use as the property's value
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval true if the property was set, otherwise false
 */
static bool AAMP_setProperty_trickPlayEnabled(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{

        LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if (pAAMP == NULL)
	{
                LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.trickPlayEnabled on instances of AAMP");
		return false;
	}
	return true;
}


/**
 * @brief Callback invoked from JS to get the EventType property value
 * @param[in] context JS execution context
 * @param[in] thisObject JSObject to search for the property
 * @param[in] propertyName JSString containing the name of the property to get
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval property's value if object has the property, otherwise NULL
 */
static JSValueRef AAMP_getProperty_EventType(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if (pAAMP == NULL)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.EventType on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
	return pAAMP->_eventType;
}


/**
 * @brief Callback invoked from JS to get the mediaType property value
 * @param[in] context JS execution context
 * @param[in] thisObject JSObject to search for the property
 * @param[in] propertyName JSString containing the name of the property to get
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval property's value if object has the property, otherwise NULL
 */
static JSValueRef AAMP_getProperty_MediaType(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (pAAMP == NULL)
	{
         	LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
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
 * @brief Callback invoked from JS to get the version property value
 * @param[in] context JS execution context
 * @param[in] thisObject JSObject to search for the property
 * @param[in] propertyName JSString containing the name of the property to get
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval property's value if object has the property, otherwise NULL
 */
static JSValueRef AAMP_getProperty_Version(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (pAAMP == NULL)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.version on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	return aamp_CStringToJSValue(context, AAMP_UNIFIED_VIDEO_ENGINE_VERSION);
}


/**
 * @brief Callback invoked from JS to get the audioLanguage property value
 * @param[in] context JS execution context
 * @param[in] thisObject JSObject to search for the property
 * @param[in] propertyName JSString containing the name of the property to get
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval property's value if object has the property, otherwise NULL
 */
static JSValueRef AAMP_getProperty_AudioLanguage(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (pAAMP == NULL)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.audioLanguage on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	const char* language = pAAMP->_aamp->GetCurrentAudioLanguage();
	LOG_INFO(pAAMP,"_aamp->GetCurrentAudioLanguage() %s",language);
	return aamp_CStringToJSValue(context, language);
}

/**
 * @brief Callback invoked from JS to get the currentDRM property value
 * @param[in] context JS execution context
 * @param[in] thisObject JSObject to search for the property
 * @param[in] propertyName JSString containing the name of the property to get
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval property's value if object has the property, otherwise NULL
 */
static JSValueRef AAMP_getProperty_CurrentDRM(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (pAAMP == NULL)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.currentDRM on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	const char* drm = pAAMP->_aamp->GetCurrentDRM();
	LOG_INFO(pAAMP,"_aamp->GetCurrentDRM() %s",drm);
	return aamp_CStringToJSValue(context, drm);
}


/**
 * @brief Callback invoked from JS to get the timedMetadata property value
 * @param[in] context JS execution context
 * @param[in] thisObject JSObject to search for the property
 * @param[in] propertyName JSString containing the name of the property to get
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval property's value if object has the property, otherwise NULL
 */
static JSValueRef AAMP_getProperty_timedMetadata(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (pAAMP == NULL)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.timedMetadata on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	PrivateInstanceAAMP* privAAMP = (pAAMP->_aamp != NULL) ? pAAMP->_aamp->aamp : NULL;
	if (privAAMP == NULL)
	{
                LOG_ERROR_EX("privAAMP not initialized");
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "AAMP.timedMetadata - initialization error");
		return JSValueMakeUndefined(context);
	}

	int32_t length = privAAMP->timedMetadata.size();

	JSValueRef* array = new JSValueRef[length];
	for (int32_t i = 0; i < length; i++)
	{
		TimedMetadata item = privAAMP->timedMetadata.at(i);
		JSObjectRef ref = aamp_CreateTimedMetadataJSObject(context, item._timeMS, item._name.c_str(), item._content.c_str(), item._id.c_str(), item._durationMS);
		array[i] = ref;
	}

	JSValueRef prop = JSObjectMakeArray(context, length, array, NULL);
	SAFE_DELETE_ARRAY(array);

	return prop;
}


/**
 * @brief Callback invoked from JS to set the stallErrorCode property value
 * @param[in] context JS exception context
 * @param[in] thisObject JSObject on which to set the property's value
 * @param[in] propertyName JSString containing the name of the property to set
 * @param[in] value JSValue to use as the property's value
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval true if the property was set, otherwise false
 */
static bool AAMP_setProperty_stallErrorCode(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (pAAMP == NULL)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.stallErrorCode on instances of AAMP");
		return false;
	}
        LOG_WARN(pAAMP,"_aamp->SetStallErrorCode context=%p  value=%d exception=%p ",context, value, exception);
	pAAMP->_aamp->SetStallErrorCode(JSValueToNumber(context, value, exception));
	return true;
}


/**
 * @brief Callback invoked from JS to set the stallTimeout property value
 * @param[in] context JS exception context
 * @param[in] thisObject JSObject on which to set the property's value
 * @param[in] propertyName JSString containing the name of the property to set
 * @param[in] value JSValue to use as the property's value
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval true if the property was set, otherwise false
 */
static bool AAMP_setProperty_stallTimeout(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (pAAMP == NULL)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.stallTimeout on instances of AAMP");
		return false;
	}
        LOG_WARN(pAAMP,"_aamp->SetStallTimeout context=%p  value=%d exception=%p",context, value, exception);
	pAAMP->_aamp->SetStallTimeout(JSValueToNumber(context, value, exception));
	return true;
}


/**
 * @brief Callback invoked from JS to set the reportInterval property value
 * @param[in] context JS exception context
 * @param[in] thisObject JSObject on which to set the property's value
 * @param[in] propertyName JSString containing the name of the property to set
 * @param[in] value JSValue to use as the property's value
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval true if the property was set, otherwise false
 */
static bool AAMP_setProperty_reportInterval(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (pAAMP == NULL)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.reportInterval on instances of AAMP");
		return false;
	}
        LOG_WARN(pAAMP,"_aamp->SetReportInterval context=%p  value=%d exception=%p ",context, value, exception);
	pAAMP->_aamp->SetReportInterval(JSValueToNumber(context, value, exception));
	return true;
}

/**
 * @brief Callback invoked from JS to set the enableNativeCC property value
 * @param[in] context JS exception context
 * @param[in] thisObject JSObject on which to set the property's value
 * @param[in] propertyName JSString containing the name of the property to set
 * @param[in] value JSValue to use as the property's value
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval true if the property was set, otherwise false
 */
static bool AAMP_setProperty_enableNativeCC(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (pAAMP == NULL)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.reportInterval on instances of AAMP");
		return false;
	}
        LOG_WARN(pAAMP,"_aamp->SetNativeCCRendering context=%p  value=%d ",context, value);
	pAAMP->_aamp->SetNativeCCRendering(JSValueToBoolean(context, value));
	return true;
}

/**
 * @brief Callback invoked from JS to set the preferred CEA format property value
 * @param[in] context JS exception context
 * @param[in] thisObject JSObject on which to set the property's value
 * @param[in] propertyName JSString containing the name of the property to set
 * @param[in] value JSValue to use as the property's value
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval true if the property was set, otherwise false
 */
static bool AAMP_setProperty_preferredCEAFormat(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (pAAMP == NULL)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.reportInterval on instances of AAMP");
		return false;
	}
        LOG_WARN(pAAMP,"_aamp->SetCEAFormat context=%p  value=%d exception=%p ",context, value, exception);
	pAAMP->_aamp->SetCEAFormat((int)JSValueToNumber(context, value, exception));
	return true;
}

/**
 * @brief Array containing the AAMP's statically declared value properties
 */
static const JSStaticValue AAMP_static_values[] =
{
	{"closedCaptionEnabled", AAMP_getProperty_closedCaptionEnabled, AAMP_setProperty_closedCaptionEnabled, kJSPropertyAttributeDontDelete },
	{"initialBufferTime", AAMP_getProperty_initialBufferTime, AAMP_setProperty_initialBufferTime, kJSPropertyAttributeDontDelete },
	{"trickPlayEnabled", AAMP_getProperty_trickPlayEnabled, AAMP_setProperty_trickPlayEnabled, kJSPropertyAttributeDontDelete },
	{"EventType", AAMP_getProperty_EventType, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{"mediaType", AAMP_getProperty_MediaType, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{"version", AAMP_getProperty_Version, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{"audioLanguage", AAMP_getProperty_AudioLanguage, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{"currentDRM", AAMP_getProperty_CurrentDRM, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{"timedMetadata", AAMP_getProperty_timedMetadata, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{"stallErrorCode", NULL, AAMP_setProperty_stallErrorCode, kJSPropertyAttributeDontDelete },
	{"stallTimeout", NULL, AAMP_setProperty_stallTimeout, kJSPropertyAttributeDontDelete },
	{"reportInterval", NULL, AAMP_setProperty_reportInterval, kJSPropertyAttributeDontDelete },
	{"enableNativeCC", NULL, AAMP_setProperty_enableNativeCC, kJSPropertyAttributeDontDelete },
	{"preferredCEAFormat", NULL, AAMP_setProperty_preferredCEAFormat, kJSPropertyAttributeDontDelete },
	{NULL, NULL, NULL, 0}
};

/**
 * @brief Array containing the Event's statically declared functions
 */
static const JSStaticFunction Event_staticfunctions[] =
{
	{ NULL, NULL, 0 }
};


/**
 * @brief Callback invoked from JS when an object of Event is first created
 * @param[in] ctx JS execution context
 * @param[in] thisObject JSObject being created
 */
static void Event_init(JSContextRef ctx, JSObjectRef thisObject)
{
	//LOG_TRACE("Enter");
}


/**
 * @brief Callback invoked when an object of Event is finalized
 * @param[in] thisObject JSObject being finalized
 */
static void Event_finalize(JSObjectRef thisObject)
{
	//noisy - large (>400) burst of logging seen during garbage collection
	//LOG_TRACE("Enter");

	const AAMPEvent* pEvent = (const AAMPEvent*)JSObjectGetPrivate(thisObject);
	JSObjectSetPrivate(thisObject, NULL);
}


static JSClassRef Event_class_ref();


/**
 * @brief callback invoked when Event is used along with 'new'
 * @param[in] ctx JS execution context
 * @param[in] constructor JSObject that is the constructor being called
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] execption pointer to a JSValueRef in which to return an exception, if any
 * @retval JSObject that is the constructor's return value
 */
static JSObjectRef Event_constructor(JSContextRef ctx, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* execption)
{
	//LOG_TRACE("Enter");
	return JSObjectMake(ctx, Event_class_ref(), NULL);
}


/**
 * @brief Structure contains properties and callbacks of Event object
 */
static const JSClassDefinition Event_object_def =
{
	0,
	kJSClassAttributeNone,
	"__Event__AAMP",
	NULL,
	NULL,
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
 * @brief Creates a JavaScript class of Event for use with JSObjectMake
 * @retval singleton instance of JavaScript class created
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
 * @brief Event listener impl for AAMP events
 */
class AAMP_JSListener : public AAMPEventObjectListener
{
public:

	static void AddEventListener(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback);

	static void RemoveEventListener(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback);

	/**
	 * @brief AAMP_JSListener Constructor
     * @param[in] aamp instance of AAMP_JS
     * @param[in] type event type
     * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_JSListener(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback)
		: _aamp(aamp)
		, _type(type)
		, _jsCallback(jsCallback)
		, _pNext(NULL)
	{
                LOG_TRACE("ctx=%p, type=%d, jsCallback=%p", _aamp->_ctx, _type, _jsCallback);
		JSValueProtect(_aamp->_ctx, _jsCallback);
	}


	/**
	 * @brief AAMP_JSListener Destructor
	 */
	virtual ~AAMP_JSListener()
	{
		LOG_TRACE("ctx=%p, type=%d, jsCallback=%p", _aamp->_ctx, _type, _jsCallback);
		JSValueUnprotect(_aamp->_ctx, _jsCallback);
	}

	/**
	 * @brief AAMP_JSListener Copy Constructor
	 */
	AAMP_JSListener(const AAMP_JSListener&) = delete;

	/**
	 * @brief AAMP_JSListener Assignment operator overloading
	 */
	AAMP_JSListener& operator=(const AAMP_JSListener&) = delete;

	/**
	 * @brief Dispatch JS event for the corresponding AAMP event
	 * @param[in] e AAMP event object
	 */
	void Event(const AAMPEventPtr &e)
	{
		AAMPEventType evtType = e->getType();
		if(evtType != AAMP_EVENT_PROGRESS && evtType != AAMP_EVENT_AD_PLACEMENT_PROGRESS)//log all events except progress which spams
                        LOG_WARN( _aamp,"ctx=%p, type=%d, jsCallback=%p", _aamp->_ctx,evtType, _jsCallback);

		JSObjectRef eventObj = JSObjectMake(_aamp->_ctx, Event_class_ref(), NULL);
		if (eventObj) {
			JSValueProtect(_aamp->_ctx, eventObj);
			JSStringRef name = JSStringCreateWithUTF8CString("type");
			JSObjectSetProperty(_aamp->_ctx, eventObj, name, JSValueMakeNumber(_aamp->_ctx, evtType), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(name);
			setEventProperties(e, _aamp->_ctx, eventObj);
			JSValueRef args[1] = { eventObj };
			if (evtType == AAMP_EVENT_AD_RESOLVED)
			{
				if (_aamp->_promiseCallback != NULL)
				{
					JSObjectCallAsFunction(_aamp->_ctx, _aamp->_promiseCallback, NULL, 1, args, NULL);
				}
				else
				{
                                        LOG_WARN( _aamp,"No promise callback registered ctx=%p, jsCallback=%p", _aamp->_ctx, _aamp->_promiseCallback);

				}
			}
			else
			{
				JSObjectCallAsFunction(_aamp->_ctx, _jsCallback, NULL, 1, args, NULL);
			}
			JSValueUnprotect(_aamp->_ctx, eventObj);
		}
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] e AAMP event object
	 * @param[in] context JS execution context
	 * @param[out] eventObj JS event object
	 */
	virtual void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
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
 * @brief Event listener impl for REPORT_PROGRESS AAMP event
 */
class AAMP_JSListener_Progress : public AAMP_JSListener
{
public:

	/**
	 * @brief AAMP_JSListener_Progress Constructor
     	 * @param[in] aamp instance of AAMP_JS
     	 * @param[in] type event type
    	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_JSListener_Progress(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] e AAMP event object
	 * @param[in] context JS execution context
	 * @param[out] eventObj JS event object
	 */
	void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
	{
		ProgressEventPtr evt = std::dynamic_pointer_cast<ProgressEvent>(e);

		JSStringRef name;

		name = JSStringCreateWithUTF8CString("durationMiliseconds");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getDuration()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("positionMiliseconds");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("playbackSpeed");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getSpeed()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("startMiliseconds");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getStart()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("endMiliseconds");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getEnd()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("currentPTS");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getPTS()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("videoBufferedMiliseconds");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getBufferedDuration()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("timecode");
		JSObjectSetProperty(context, eventObj, name, aamp_CStringToJSValue(context, evt->getSEITimeCode()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);
	}
};

/**
 * @class AAMP_JSListener_BitRateChanged
 * @brief Event listener impl for BITRATE_CHANGED AAMP event
 */
class AAMP_JSListener_BitRateChanged : public AAMP_JSListener
{
public:

	/**
	 * @brief AAMP_JSListener_BitRateChanged Constructor
     	 * @param[in] aamp instance of AAMP_JS
    	 * @param[in] type event type
    	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_JSListener_BitRateChanged(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] e AAMP event object
	 * @param[in] context JS execution context
	 * @param[out] eventObj JS event object
	 */
	void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
	{
		BitrateChangeEventPtr evt = std::dynamic_pointer_cast<BitrateChangeEvent>(e);

		JSStringRef name;
		name = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getTime()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("bitRate");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getBitrate()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("description");
		JSObjectSetProperty(context, eventObj, name, aamp_CStringToJSValue(context, evt->getDescription().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("width");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getWidth()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("height");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getHeight()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);
		
		name = JSStringCreateWithUTF8CString("framerate");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getFrameRate()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("position");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		name = JSStringCreateWithUTF8CString("cappedProfile");
                JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getCappedProfileStatus()), kJSPropertyAttributeReadOnly, NULL);

		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("displayWidth");
                JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getDisplayWidth()), kJSPropertyAttributeReadOnly, NULL);

                JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("displayHeight");
                JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getDisplayHeight()), kJSPropertyAttributeReadOnly, NULL);

                JSStringRelease(name);
	}
};

/**
 * @class AAMP_JSListener_SpeedChanged
 * @brief Event listener impl for SPEED_CHANGED AAMP event
 */
class AAMP_JSListener_SpeedChanged : public AAMP_JSListener
{
public:

        /**
         * @brief AAMP_JSListener_SpeedChanged Constructor
         * @param[in] aamp instance of AAMP_JS
         * @param[in] type event type
         * @param[in] jsCallback callback to be registered as listener
         */
	AAMP_JSListener_SpeedChanged(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] e AAMP event object
	 * @param[in] context JS execution context
	 * @param[out] eventObj JS event object
	 */
	void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
	{
		SpeedChangedEventPtr evt = std::dynamic_pointer_cast<SpeedChangedEvent>(e);

		JSStringRef name;

		name = JSStringCreateWithUTF8CString("speed");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getRate()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("reason");
		JSObjectSetProperty(context, eventObj, name, aamp_CStringToJSValue(context, "unknown"), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);
	}
};

/**
 * @class AAMP_JSListener_TuneFailed
 * @brief Event listener impl for TUNE_FAILED AAMP event
 */
class AAMP_JSListener_TuneFailed : public AAMP_JSListener
{
public:

        /**
         * @brief AAMP_JSListener_TuneFailed Constructor
         * @param[in] aamp instance of AAMP_JS
         * @param[in] type event type
         * @param[in] jsCallback callback to be registered as listener
         */
	AAMP_JSListener_TuneFailed(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] e AAMP event object
	 * @param[in] context JS execution context
	 * @param[out] eventObj JS event object
	 */
	void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
	{
		MediaErrorEventPtr evt = std::dynamic_pointer_cast<MediaErrorEvent>(e);

		int code = evt->getCode();
		const char* description = evt->getDescription().c_str();

		JSStringRef name = JSStringCreateWithUTF8CString("code");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, code), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("description");
		JSObjectSetProperty(context, eventObj, name, aamp_CStringToJSValue(context, description), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("shouldRetry");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeBoolean(context, evt->shouldRetry()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);
		
		if(-1 != evt->getClass()) //Only send verbose error for secclient/secmanager DRM failures
		{
			name = JSStringCreateWithUTF8CString("class");
			JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getClass()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(name);
			
			name = JSStringCreateWithUTF8CString("reason");
			JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getReason()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(name);
			
			name = JSStringCreateWithUTF8CString("businessStatus");
			JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getBusinessStatus()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(name);
		}
	}
};



/**
 * @class AAMP_JSListener_DRMMetadata
 * @brief Class handles JS Listener for DRM meta data operation
 */
class AAMP_JSListener_DRMMetadata : public AAMP_JSListener
{
public:

        /**
         * @brief AAMP_JSListener_DRMMetadata Constructor
         * @param aamp instance of Aamp_JS
         * @param type AampEvent type
         * @param jsCallback callback to be registered as listener
         */
        AAMP_JSListener_DRMMetadata(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
        {
        }

        /**
         * @brief set the aamp event properties
         * @param e Aamp Event pointer
         * @param context JS execution context
         * @param eventObj JS object reference
         * @retval None
         */
        void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
        {
		DrmMetaDataEventPtr evt = std::dynamic_pointer_cast<DrmMetaDataEvent>(e);
		JSStringRef name;

		int code = evt->getAccessStatusValue();
		const char* description = evt->getAccessStatus().c_str();

                LOG_WARN_EX("AAMP_JSListener_DRMMetadata code %d Description %s", code, description);
		name = JSStringCreateWithUTF8CString("code");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, code), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("description");
		JSObjectSetProperty(context, eventObj, name, aamp_CStringToJSValue(context, description), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);
        }
};

/**
 * @class AAMP_JSListener_AnomalyReport
 * @brief AAMP_JSListener_AnomalyReport to receive anomalyreport
 */
class AAMP_JSListener_AnomalyReport : public AAMP_JSListener
{
public:

        /**
         * @brief AAMP_JSListener_DRMMetadata Constructor
         * @param aamp  Aamp_JS
         * @param type AampEvent type
         * @param jsCallback callback to be registered as listener
         */
        AAMP_JSListener_AnomalyReport(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
        {
        }

        /**
         * @brief
         * @param e Aamp Event pointer
         * @param context JS execution context
         * @param eventObj JS object reference
         * @retval void
         */
        void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
        {
		AnomalyReportEventPtr evt = std::dynamic_pointer_cast<AnomalyReportEvent>(e);
		JSStringRef name;

		int severity = evt->getSeverity();
		const char* description = evt->getMessage().c_str();

                LOG_WARN_EX("AAMP_JSListener_AnomalyReport severity %d Description %s",severity,description);
		name = JSStringCreateWithUTF8CString("severity");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, severity), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("description");
		JSObjectSetProperty(context, eventObj, name, aamp_CStringToJSValue(context, description), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);
        }
};

/**
 * @class AAMP_JSListener_MetricsData 
 * @brief AAMP_JSListener_MetricsData to receive aamp metrics
 */
class AAMP_JSListener_MetricsData : public AAMP_JSListener
{
public:

        /**
         * @brief AAMP_JSListener_DRMMetadata Constructor
         * @param aamp instance of AAMP_JS
         * @param type AampEvent type
         * @param jsCallback callback to be registered as listener
         */
	AAMP_JSListener_MetricsData(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
        {
        }

        /**
         * @brief Set the Aamp Event properties
         * @param e Aamp Event pointer
         * @param context JS execution context
         * @param eventObj JS event object
         * @retval None
         */
        void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
        {
		MetricsDataEventPtr evt = std::dynamic_pointer_cast<MetricsDataEvent>(e);
		JSStringRef strJSObj;

		strJSObj = JSStringCreateWithUTF8CString("metricType");
		JSObjectSetProperty(context, eventObj, strJSObj, JSValueMakeNumber(context, evt->getMetricsDataType()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(strJSObj);

		strJSObj = JSStringCreateWithUTF8CString("traceID");
		JSObjectSetProperty(context, eventObj, strJSObj, aamp_CStringToJSValue(context, evt->getMetricUUID().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(strJSObj);

		strJSObj = JSStringCreateWithUTF8CString("metricData");
		JSObjectSetProperty(context, eventObj, strJSObj, aamp_CStringToJSValue(context, evt->getMetricsData().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(strJSObj);
        }
};


/**
 * @class AAMP_JSListener_CCHandleReceived
 * @brief Event listener impl for CC_HANDLE_RECEIVED AAMP event
 */
class AAMP_JSListener_CCHandleReceived : public AAMP_JSListener
{
public:
        /**
         * @brief AAMP_JSListener_CCHandleReceived Constructor
         * @param[in] aamp instance of AAMP_JS
         * @param[in] type event type
         * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_JSListener_CCHandleReceived(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] e AAMP event object
	 * @param[in] context JS execution context
	 * @param[out] eventObj JS event object
	 */
	void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
	{
		CCHandleEventPtr evt = std::dynamic_pointer_cast<CCHandleEvent>(e);
		JSStringRef name;

		name = JSStringCreateWithUTF8CString("decoderHandle");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getCCHandle()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);
	}
};

/**
 * @class AAMP_JSListener_VideoMetadata
 * @brief Event listener impl for VIDEO_METADATA AAMP event
 */
class AAMP_JSListener_VideoMetadata : public AAMP_JSListener
{
public:

        /**
         * @brief AAMP_JSListener_VideoMetadata Constructor
         * @param[in] aamp instance of AAMP_JS
         * @param[in] type event type
         * @param[in] jsCallback callback to be registered as listener
         */
	AAMP_JSListener_VideoMetadata(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] e AAMP event object
	 * @param[in] context JS execution context
	 * @param[out] eventObj JS event object
	 */
	void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
	{
		MediaMetadataEventPtr evt = std::dynamic_pointer_cast<MediaMetadataEvent>(e);

		JSStringRef name;
		name = JSStringCreateWithUTF8CString("durationMiliseconds");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getDuration()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		int count = evt->getLanguagesCount();
		const std::vector<std::string> &langVect = evt->getLanguages();
		JSValueRef* array = new JSValueRef[count];
		for (int32_t i = 0; i < count; i++)
		{
			JSValueRef lang = aamp_CStringToJSValue(context, langVect[i].c_str());
			array[i] = lang;
			//JSValueRelease(lang);
		}
		JSValueRef prop = JSObjectMakeArray(context, count, array, NULL);
		SAFE_DELETE_ARRAY(array);

		name = JSStringCreateWithUTF8CString("languages");
		JSObjectSetProperty(context, eventObj, name, prop, kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		count = evt->getBitratesCount();
		const std::vector<long> &bitrateVect = evt->getBitrates();
		array = new JSValueRef[count];
		for (int32_t i = 0; i < count; i++)
		{
			array[i] = JSValueMakeNumber(context, bitrateVect[i]);
		}
		prop = JSObjectMakeArray(context, count, array, NULL);
		SAFE_DELETE_ARRAY(array);

		name = JSStringCreateWithUTF8CString("bitrates");
		JSObjectSetProperty(context, eventObj, name, prop, kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		count = evt->getSupportedSpeedCount();
		const std::vector<float> &speedVect = evt->getSupportedSpeeds();
		array = new JSValueRef[count];
		for (int32_t i = 0; i < count; i++)
		{
			array[i] = JSValueMakeNumber(context, speedVect[i]);
		}
		prop = JSObjectMakeArray(context, count, array, NULL);
		SAFE_DELETE_ARRAY(array);

		name = JSStringCreateWithUTF8CString("playbackSpeeds");
		JSObjectSetProperty(context, eventObj, name, prop, kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("width");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getWidth()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("height");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, evt->getHeight()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("hasDrm");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeBoolean(context, evt->hasDrm()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);
	}
};

/**
 * @class AAMP_JSListener_BulkTimedMetadata
 * @brief Event listener impl for BULK_TIMED_METADATA AAMP event
 */
class AAMP_JSListener_BulkTimedMetadata : public AAMP_JSListener
{
public:

       /**
        * @brief AAMP_JSListener_TimedMetadata Constructor
        * @param[in] aamp instance of AAMP_JS
        * @param[in] type event type
        * @param[in] jsCallback callback to be registered as listener
        */
	AAMP_JSListener_BulkTimedMetadata(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] e AAMP event object
	 * @param[in] context JS execution context
	 * @param[out] eventObj JS event object
	 */
	void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
	{
		BulkTimedMetadataEventPtr evt = std::dynamic_pointer_cast<BulkTimedMetadataEvent>(e);
		JSStringRef name = JSStringCreateWithUTF8CString("timedMetadatas");
		JSObjectSetProperty(context, eventObj, name, aamp_CStringToJSValue(context, evt->getContent().c_str()),  kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);
	}
};


/**
 * @class AAMP_JSListener_TimedMetadata
 * @brief Event listener impl for TIMED_METADATA AAMP event
 */
class AAMP_JSListener_TimedMetadata : public AAMP_JSListener
{
public:

        /**
         * @brief AAMP_JSListener_TimedMetadata Constructor
         * @param[in] aamp instance of AAMP_JS
         * @param[in] type event type
         * @param[in] jsCallback callback to be registered as listener
         */
	AAMP_JSListener_TimedMetadata(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] e AAMP event object
	 * @param[in] context JS execution context
	 * @param[out] eventObj JS event object
	 */
	void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
	{
		TimedMetadataEventPtr evt = std::dynamic_pointer_cast<TimedMetadataEvent>(e);

		JSObjectRef timedMetadata = aamp_CreateTimedMetadataJSObject(context, evt->getTime(), evt->getName().c_str(), evt->getContent().c_str(), evt->getId().c_str(), evt->getDuration());
        	if (timedMetadata) {
                	JSValueProtect(context, timedMetadata);
			JSStringRef name = JSStringCreateWithUTF8CString("timedMetadata");
			JSObjectSetProperty(context, eventObj, name, timedMetadata, kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(name);
        		JSValueUnprotect(context, timedMetadata);
		}
	}
};

/**
 * @class AAMP_JSListener_ContentGap
 * @brief Event listener impl for AAMP_EVENT_CONTENT_GAP event.
 */
class AAMP_JSListener_ContentGap : public AAMP_JSListener
{
public:
	/**
	 * @brief AAMP_JSListener_ContentGap Constructor
	 * @param[in] aamp instance of AAMP_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_JSListener_ContentGap(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] e AAMP event object
	 * @param[in] context JS execution context
	 * @param[out] eventObj JS event object
	 */
	void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
	{
		ContentGapEventPtr evt = std::dynamic_pointer_cast<ContentGapEvent>(e);
		JSStringRef name;

		double time = evt->getTime();
		double durationMs = evt->getDuration();

		name = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, std::round(time)), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		if (durationMs >= 0)
		{
			name = JSStringCreateWithUTF8CString("duration");
			JSObjectSetProperty(context, eventObj, name, JSValueMakeNumber(context, (int)durationMs), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(name);
		}
	}
};

/**
 * @class AAMP_JSListener_StatusChanged
 * @brief Event listener for STATUS_CHANGED AAMP event
 */
class AAMP_JSListener_StatusChanged : public AAMP_JSListener
{
public:

        /**
         * @brief AAMP_JSListener_StatusChanged Constructor
         * @param[in] aamp instance of AAMP_JS
         * @param[in] type event type
         * @param[in] jsCallback callback to be registered as listener
         */
	AAMP_JSListener_StatusChanged(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] e AAMP event object
	 * @param[in] context JS execution context
	 * @param[out] eventObj JS event object
	 */
	void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
	{
		StateChangedEventPtr evt = std::dynamic_pointer_cast<StateChangedEvent>(e);

		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("state");
		JSObjectSetProperty(context, eventObj, prop, JSValueMakeNumber(context, evt->getState()), kJSPropertyAttributeReadOnly, NULL);
                JSStringRelease(prop);

	}
};


/**
 * @class AAMP_JSListener_SpeedsChanged
 * @brief Event listener impl for SPEEDS_CHANGED AAMP event
 */
class AAMP_JSListener_SpeedsChanged : public AAMP_JSListener
{
public:

        /**
         * @brief AAMP_JSListener_SpeedsChanged Constructor
         * @param[in] aamp instance of AAMP_JS
         * @param[in] type event type
         * @param[in] jsCallback callback to be registered as listener
         */
	AAMP_JSListener_SpeedsChanged(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] e AAMP event object
	 * @param[in] context JS execution context
	 * @param[out] eventObj JS event object
	 */
	void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
	{
		SupportedSpeedsChangedEventPtr evt = std::dynamic_pointer_cast<SupportedSpeedsChangedEvent>(e);

		int count = evt->getSupportedSpeedCount();
		const std::vector<float> &speedVect = evt->getSupportedSpeeds();
		JSValueRef* array = new JSValueRef[count];
		for (int32_t i = 0; i < count; i++)
		{
			array[i] = JSValueMakeNumber(context, speedVect[i]);
		}
		JSValueRef prop = JSObjectMakeArray(context, count, array, NULL);
		SAFE_DELETE_ARRAY(array);

		JSStringRef name = JSStringCreateWithUTF8CString("playbackSpeeds");
		JSObjectSetProperty(context, eventObj, name, prop, kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);
	}
};


/**
 * @class AAMP_JSListener_AdResolved
 * @brief Event listener impl for AD_RESOLVED AAMP event
 */
class AAMP_JSListener_AdResolved : public AAMP_JSListener
{
public:

        /**
         * @brief AAMP_JSListener_AdResolved Constructor
         * @param[in] aamp instance of AAMP_JS
         * @param[in] type event type
         * @param[in] jsCallback callback to be registered as listener
         */
	AAMP_JSListener_AdResolved(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] e AAMP event object
	 * @param[in] context JS execution context
	 * @param[out] eventObj JS event object
	 */
	void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
	{
		AdResolvedEventPtr evt = std::dynamic_pointer_cast<AdResolvedEvent>(e);

		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("resolvedStatus");
		JSObjectSetProperty(context, eventObj, prop, JSValueMakeBoolean(context, evt->getResolveStatus()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("placementId");
		JSObjectSetProperty(context, eventObj, prop, aamp_CStringToJSValue(context, evt->getAdId().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("placementStartTime");
		JSObjectSetProperty(context, eventObj, prop, JSValueMakeNumber(context, evt->getStart()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("placementDuration");
		JSObjectSetProperty(context, eventObj, prop, JSValueMakeNumber(context, evt->getDuration()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};

/**
 * @class AAMP_JSListener_AdReservationStart
 * @brief Event listener impl for AD_RESOLVED AAMP event
 */
class AAMP_JSListener_AdReservationStart : public AAMP_JSListener
{
public:

        /**
         * @brief AAMP_JSListener_AdReservationStart Constructor
         * @param[in] aamp instance of AAMP_JS
         * @param[in] type event type
         * @param[in] jsCallback callback to be registered as listener
         */
	AAMP_JSListener_AdReservationStart(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] e AAMP event object
	 * @param[in] context JS execution context
	 * @param[out] eventObj JS event object
	 */
	void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
	{
		AdReservationEventPtr evt = std::dynamic_pointer_cast<AdReservationEvent>(e);

		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("adbreakId");
		JSObjectSetProperty(context, eventObj, prop, aamp_CStringToJSValue(context, evt->getAdBreakId().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(context, eventObj, prop, JSValueMakeNumber(context, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};

/**
 * @class AAMP_JSListener_AdReservationEnd
 * @brief Event listener impl for AD_RESOLVED AAMP event
 */
class AAMP_JSListener_AdReservationEnd : public AAMP_JSListener
{
public:

       /**
        * @brief AAMP_JSListener_AdReservationEnd Constructor
        * @param[in] aamp instance of AAMP_JS
        * @param[in] type event type
        * @param[in] jsCallback callback to be registered as listener
        */
	AAMP_JSListener_AdReservationEnd(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] e AAMP event object
	 * @param[in] context JS execution context
	 * @param[out] eventObj JS event object
	 */
	void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
	{
		AdReservationEventPtr evt = std::dynamic_pointer_cast<AdReservationEvent>(e);

		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("adbreakId");
		JSObjectSetProperty(context, eventObj, prop, aamp_CStringToJSValue(context, evt->getAdBreakId().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(context, eventObj, prop, JSValueMakeNumber(context, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};

/**
 * @class AAMP_JSListener_AdPlacementStart
 * @brief Event listener impl for AD_RESOLVED AAMP event
 */
class AAMP_JSListener_AdPlacementStart : public AAMP_JSListener
{
public:

        /**
         * @brief AAMP_JSListener_AdPlacementStart Constructor
         * @param[in] aamp instance of AAMP_JS
         * @param[in] type event type
         * @param[in] jsCallback callback to be registered as listener
         */
	AAMP_JSListener_AdPlacementStart(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] e AAMP event object
	 * @param[in] context JS execution context
	 * @param[out] eventObj JS event object
	 */
	void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
	{
		AdPlacementEventPtr evt = std::dynamic_pointer_cast<AdPlacementEvent>(e);

		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("adId");
		JSObjectSetProperty(context, eventObj, prop, aamp_CStringToJSValue(context, evt->getAdId().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(context, eventObj, prop, JSValueMakeNumber(context, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};

/**
 * @class AAMP_JSListener_AdPlacementEnd
 * @brief Event listener impl for AD_RESOLVED AAMP event
 */
class AAMP_JSListener_AdPlacementEnd : public AAMP_JSListener
{
public:

        /**
         * @brief AAMP_JSListener_AdPlacementEnd Constructor
         * @param[in] aamp instance of AAMP_JS
         * @param[in] type event type
         * @param[in] jsCallback callback to be registered as listener
         */
	AAMP_JSListener_AdPlacementEnd(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] e AAMP event object
	 * @param[in] context JS execution context
	 * @param[out] eventObj JS event object
	 */
	void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
	{
		AdPlacementEventPtr evt = std::dynamic_pointer_cast<AdPlacementEvent>(e);

		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("adId");
		JSObjectSetProperty(context, eventObj, prop, aamp_CStringToJSValue(context, evt->getAdId().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(context, eventObj, prop, JSValueMakeNumber(context, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};

/**
 * @class AAMP_JSListener_AdProgress
 * @brief Event listener impl for REPORT_AD_PROGRESS AAMP event
 */
class AAMP_JSListener_AdProgress : public AAMP_JSListener
{
public:

	/**
	 * @brief AAMP_JSListener_AdProgress Constructor
         * @param[in] aamp instance of AAMP_JS
         * @param[in] type event type
         * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_JSListener_AdProgress(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 *
	 * @param[in] e         AAMP event object
	 * @param[in] context   JS execution context
	 * @param[out] eventObj JS event object
	 */
	void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
	{
		AdPlacementEventPtr evt = std::dynamic_pointer_cast<AdPlacementEvent>(e);

		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("adId");
		JSObjectSetProperty(context, eventObj, prop, aamp_CStringToJSValue(context, evt->getAdId().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(context, eventObj, prop, JSValueMakeNumber(context, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};

/**
 * @class AAMP_JSListener_AdPlacementEror
 * @brief Event listener impl for AD_RESOLVED AAMP event
 */
class AAMP_JSListener_AdPlacementEror : public AAMP_JSListener
{
public:

        /**
         * @brief AAMP_JSListener_AdPlacementEror Constructor
         * @param[in] aamp instance of AAMP_JS
         * @param[in] type event type
         * @param[in] jsCallback callback to be registered as listener
         */
	AAMP_JSListener_AdPlacementEror(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] e AAMP event object
	 * @param[in] context JS execution context
	 * @param[out] eventObj JS event object
	 */
	void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
	{
		AdPlacementEventPtr evt = std::dynamic_pointer_cast<AdPlacementEvent>(e);

		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("adId");
		JSObjectSetProperty(context, eventObj, prop, aamp_CStringToJSValue(context, evt->getAdId().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(context, eventObj, prop, JSValueMakeNumber(context, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("error");
		JSObjectSetProperty(context, eventObj, prop, JSValueMakeNumber(context, evt->getErrorCode()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};

/**
 * @class AAMP_JSListener_BufferingChanged
 * @brief Event listener impl for (AAMP_EVENT_BUFFER_UNDERFLOW) AAMP event
 */
class AAMP_JSListener_BufferingChanged: public AAMP_JSListener
{
public:

        /**
         * @brief AAMP_JSListener_BufferingChanged Constructor
         * @param[in] aamp instance of AAMP_JS
         * @param[in] type event type
         * @param[in] jsCallback callback to be registered as listener
         */
	AAMP_JSListener_BufferingChanged(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] e AAMP event object
	 * @param[in] context JS execution context
	 * @param[out] eventObj JS event object
	 */
	void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
	{
		BufferingChangedEventPtr evt = std::dynamic_pointer_cast<BufferingChangedEvent>(e);
		JSStringRef prop;

		/* e.data.bufferingChanged.buffering buffering started(underflow ended) = true, buffering end(underflow started) = false*/
		prop = JSStringCreateWithUTF8CString("status");
		JSObjectSetProperty(context, eventObj, prop, JSValueMakeBoolean(context, evt->buffering()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/**
 * @class AAMP_JSListener_Id3Metadata
 * @brief Event listener impl for (AAMP_JSListener_Id3Metadata) AAMP event
 */
class AAMP_JSListener_Id3Metadata: public AAMP_JSListener
{
public:

        /**
         * @brief AAMP_JSListener_Id3Metadata Constructor
         * @param[in] aamp instance of AAMP_JS
         * @param[in] type event type
         * @param[in] jsCallback callback to be registered as listener
         */
	AAMP_JSListener_Id3Metadata(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] e AAMP event object
	 * @param[in] context JS execution context
	 * @param[out] eventObj JS event object
	 */
	void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
	{
		ID3MetadataEventPtr evt = std::dynamic_pointer_cast<ID3MetadataEvent>(e);
		std::vector<uint8_t> data = evt->getMetadata();
		int len = evt->getMetadataSize();
		JSStringRef prop;

		JSValueRef* array = new JSValueRef[len];
		for (int32_t i = 0; i < len; i++)
		{
			array[i] = JSValueMakeNumber(context, data[i]);
		}

		prop = JSStringCreateWithUTF8CString("data");
		JSObjectSetProperty(context, eventObj, prop, JSObjectMakeArray(context, len, array, NULL), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
		SAFE_DELETE_ARRAY(array);

		prop = JSStringCreateWithUTF8CString("length");
		JSObjectSetProperty(context, eventObj, prop, JSValueMakeNumber(context, len), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};

/**
 * @class AAMP_JSListener_DrmMessage
 * @brief Event listener impl for (AAMP_EVENT_DRM_MESSAGE) AAMP event
 */
class AAMP_JSListener_DrmMessage : public AAMP_JSListener
{
public:

	/**
	* @brief AAMP_JSListener_DrmMessage Constructor
	* @param[in] aamp instance of AAMP_JS
	* @param[in] type event type
	* @param[in] jsCallback callback to be registered as listener
	*/
	AAMP_JSListener_DrmMessage(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	* @brief Set JS event properties
	* @param[in] e AAMP event object
	* @param[in] context JS execution context
	* @param[out] eventObj JS event object
	*/
	void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
	{
		DrmMessageEventPtr evt = std::dynamic_pointer_cast<DrmMessageEvent>(e);
		JSStringRef prop;
		prop = JSStringCreateWithUTF8CString("data");
		JSObjectSetProperty(context, eventObj, prop, aamp_CStringToJSValue(context, evt->getMessage().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};

/**
 * @class AAMP_JSListener_ContentProtectionData
 * @brief Event listener impl for (AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE) AAMP event
 */
class AAMP_JSListener_ContentProtectionData : public AAMP_JSListener
{
public:

	/**
	 * @brief AAMP_JSListener_ContentProtectionData Constructor
	 * @param[in] aamp instance of AAMP_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_JSListener_ContentProtectionData(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback) : AAMP_JSListener(aamp, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] e AAMP event object
	 * @param[in] context JS execution context
	 * @param[out] eventObj JS event object
	 */
	void setEventProperties(const AAMPEventPtr& e, JSContextRef context, JSObjectRef eventObj)
	{
		ContentProtectionDataEventPtr evt = std::dynamic_pointer_cast<ContentProtectionDataEvent>(e);
		std::vector<uint8_t> keyId = evt->getKeyID();
		int len = keyId.size();
		JSStringRef prop;
		JSValueRef* array = new JSValueRef[len];
		for (int32_t i = 0; i < len; i++)
		{
			array[i] = JSValueMakeNumber(context, keyId[i]);
		}

		prop = JSStringCreateWithUTF8CString("keyID");
		JSObjectSetProperty(context, eventObj, prop, JSObjectMakeArray(context, len, array, NULL), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
		SAFE_DELETE_ARRAY(array);

		prop = JSStringCreateWithUTF8CString("streamType");
		JSObjectSetProperty(context, eventObj, prop, aamp_CStringToJSValue(context, evt->getStreamType().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

	}
};


/**
 * @brief Callback invoked from JS to add an event listener for a particular event
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_addEventListener(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if (pAAMP == NULL)
	{
                LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
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
                        LOG_TRACE("eventType='%s', %d", type, eventType);

			if ((eventType >= 0) && (eventType < AAMP_MAX_NUM_EVENTS))
			{
				AAMP_JSListener::AddEventListener(pAAMP, eventType, callbackObj);
			}
			SAFE_DELETE_ARRAY(type);
		}
		else
		{
                        LOG_ERROR_EX("callbackObj=%p, JSObjectIsFunction(context, callbackObj) is NULL", callbackObj);
			*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.addEventListener' - parameter 2 is not a function");
		}
	}
	else
	{
                LOG_ERROR_EX("InvalidArgument: argumentCount=%d, expected: 2", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.addEventListener' - 2 arguments required.");
	}

	return JSValueMakeUndefined(context);
}


/**
 * @brief Adds a JS function as listener for a particular event
 * @param[in] aamp jsObj instance of AAMP_JS
 * @param[in] type event type
 * @param[in] jsCallback callback to be registered as listener
 */
void AAMP_JSListener::AddEventListener(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback)
{
        LOG_TRACE("(%p, %d, %p)", aamp, type, jsCallback);

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
	else if(type == AAMP_EVENT_MEDIA_METADATA)
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
	else if(type == AAMP_EVENT_BULK_TIMED_METADATA)
	{
		pListener = new AAMP_JSListener_BulkTimedMetadata(aamp, type, jsCallback);
	}
	else if(type == AAMP_EVENT_TIMED_METADATA)
	{
		pListener = new AAMP_JSListener_TimedMetadata(aamp, type, jsCallback);
	}
	else if(type == AAMP_EVENT_CONTENT_GAP)
	{
		pListener = new AAMP_JSListener_ContentGap(aamp, type, jsCallback);
	}
	else if(type == AAMP_EVENT_STATE_CHANGED)
	{
		pListener = new AAMP_JSListener_StatusChanged(aamp, type, jsCallback);
	}
	else if(type == AAMP_EVENT_SPEEDS_CHANGED)
	{
		pListener = new AAMP_JSListener_SpeedsChanged(aamp, type, jsCallback);
	}
	else if (type == AAMP_EVENT_REPORT_ANOMALY)
	{
		pListener = new AAMP_JSListener_AnomalyReport(aamp, type, jsCallback);
	}
	else if (type == AAMP_EVENT_REPORT_METRICS_DATA)
	{
		pListener = new AAMP_JSListener_MetricsData(aamp, type, jsCallback);
	}
	else if (type == AAMP_EVENT_DRM_METADATA)
	{
		pListener = new AAMP_JSListener_DRMMetadata(aamp, type, jsCallback);
	}
	else if(type == AAMP_EVENT_AD_RESOLVED)
	{
		pListener = new AAMP_JSListener_AdResolved(aamp, type, jsCallback);
	}
	else if(type == AAMP_EVENT_AD_RESERVATION_START)
	{
		pListener = new AAMP_JSListener_AdReservationStart(aamp, type, jsCallback);
	}
	else if(type == AAMP_EVENT_AD_RESERVATION_END)
	{
		pListener = new AAMP_JSListener_AdReservationEnd(aamp, type, jsCallback);
	}
	else if(type == AAMP_EVENT_AD_PLACEMENT_START)
	{
		pListener = new AAMP_JSListener_AdPlacementStart(aamp, type, jsCallback);
	}
	else if(type == AAMP_EVENT_AD_PLACEMENT_END)
	{
		pListener = new AAMP_JSListener_AdPlacementEnd(aamp, type, jsCallback);
	}
	else if(type == AAMP_EVENT_AD_PLACEMENT_PROGRESS)
	{
		pListener = new AAMP_JSListener_AdProgress(aamp, type, jsCallback);
	}
	else if(type == AAMP_EVENT_AD_PLACEMENT_ERROR)
	{
		pListener = new AAMP_JSListener_AdPlacementEror(aamp, type, jsCallback);
	}
	else if(type == AAMP_EVENT_BUFFERING_CHANGED)
	{
		pListener = new AAMP_JSListener_BufferingChanged(aamp, type, jsCallback);
	}
	else if(type == AAMP_EVENT_ID3_METADATA)
	{
		pListener = new AAMP_JSListener_Id3Metadata(aamp, type, jsCallback);
	}
	else if(type == AAMP_EVENT_DRM_MESSAGE)
	{
		pListener = new AAMP_JSListener_DrmMessage(aamp, type, jsCallback);
	}
	else if(type == AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE)
	{
		pListener = new AAMP_JSListener_ContentProtectionData(aamp, type, jsCallback);
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
 * @brief Callback invoked from JS to remove an event listener
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_removeEventListener(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
        LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if (pAAMP == NULL)
	{
                LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.removeEventListener on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount >= 2)
	{
		JSObjectRef callbackObj = JSValueToObject(context, arguments[1], NULL);

		if ((callbackObj != NULL) && (JSObjectIsFunction(context, callbackObj)))
		{
			char* type = aamp_JSValueToCString(context, arguments[0], NULL);
			AAMPEventType eventType = aamp_getEventTypeFromName(type);
                        LOG_TRACE("eventType='%s', %d", type, eventType);

			if ((eventType >= 0) && (eventType < AAMP_MAX_NUM_EVENTS))
			{

				AAMP_JSListener::RemoveEventListener(pAAMP, eventType, callbackObj);
			}
			SAFE_DELETE_ARRAY(type);
		}
		else
		{
                        LOG_ERROR_EX("InvalidArgument: callbackObj=%p, JSObjectIsFunction(context, callbackObj) is NULL", callbackObj);
			*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.removeEventListener' - parameter 2 is not a function");
		}
	}
	else
	{
                LOG_ERROR_EX("InvalidArgument: argumentCount=%d, expected: 2", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.removeEventListener' - 2 arguments required.");
	}

	return JSValueMakeUndefined(context);
}


/**
 * @brief Removes a JS listener for a particular event
 * @param[in] aamp instance of AAMP_JS
 * @param[in] type event type
 * @param[in] jsCallback callback to be removed as listener
 */
void AAMP_JSListener::RemoveEventListener(AAMP_JS* aamp, AAMPEventType type, JSObjectRef jsCallback)
{
        LOG_TRACE("(%p, %d, %p)", aamp, type, jsCallback);
	AAMP_JSListener** ppListener = &aamp->_listeners;
	while (*ppListener != NULL)
	{
		AAMP_JSListener* pListener = *ppListener;
		if ((pListener->_type == type) && (pListener->_jsCallback == jsCallback))
		{
			*ppListener = pListener->_pNext;
                        LOG_WARN_EX(" type=%d,pListener= %p", type, pListener);
			aamp->_aamp->RemoveEventListener(type, pListener);
			SAFE_DELETE(pListener);
			return;
		}
		ppListener = &pListener->_pNext;
	}
}


/**
 * @brief Callback invoked from JS to set AAMP object's properties
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setProperties(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
        LOG_TRACE("Enter");
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to get AAMP object's properties
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_getProperties(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
        LOG_TRACE("Enter");
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to start playback for requested URL
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_tune(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if(!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.tune on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	char* contentType = NULL;
	bool bFinalAttempt = false;
	bool bFirstAttempt = true;
	switch(argumentCount)
	{
		case 4:
			{
				bFinalAttempt = JSValueToBoolean(context, arguments[3]);
			}
		case 3:
			{
				bFirstAttempt = JSValueToBoolean(context, arguments[2]);
			}
		case 2:
			{
				contentType = aamp_JSValueToCString(context, arguments[1], exception);
			}
		case 1:
			{
				char* url = aamp_JSValueToCString(context, arguments[0], exception);
				aamp_ApplyPageHttpHeaders(pAAMP->_aamp);
				{
                                        LOG_WARN(pAAMP," _aamp->Tune(%d, %s, %d, %d)", true, contentType, bFirstAttempt, bFinalAttempt);
					pAAMP->_aamp->Tune(url, true, contentType, bFirstAttempt, bFinalAttempt);                  
				}
				SAFE_DELETE_ARRAY(url);
			}
			SAFE_DELETE_ARRAY(contentType);
			break;
		default:
                        LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1 to 4", argumentCount);
			*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.tune' - 1 argument required");
			break;
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to start playback for requested URL
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_load(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
                LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.load on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount == 1 || argumentCount == 2)
	{
		char* contentType = NULL;
		char* strTraceId = NULL;
		char* strAuthToken = NULL;
		bool bFinalAttempt = false;
		bool bFirstAttempt = true;
		if (argumentCount == 2 && JSValueIsObject(context, arguments[1]))
		{
			JSObjectRef argument = JSValueToObject(context, arguments[1], NULL);
			JSStringRef paramName = JSStringCreateWithUTF8CString("contentType");
			JSValueRef paramValue = JSObjectGetProperty(context, argument, paramName, NULL);
			if (JSValueIsString(context, paramValue))
			{
				contentType = aamp_JSValueToCString(context, paramValue, NULL);
			}
			JSStringRelease(paramName);

			paramName = JSStringCreateWithUTF8CString("traceId");
			paramValue = JSObjectGetProperty(context, argument, paramName, NULL);
			if (JSValueIsString(context, paramValue))
			{
				strTraceId = aamp_JSValueToCString(context, paramValue, NULL);
			}
			JSStringRelease(paramName);

			paramName = JSStringCreateWithUTF8CString("isInitialAttempt");
			paramValue = JSObjectGetProperty(context, argument, paramName, NULL);
			if (JSValueIsBoolean(context, paramValue))
			{
				bFirstAttempt = JSValueToBoolean(context, paramValue);
			}
			JSStringRelease(paramName);

			paramName = JSStringCreateWithUTF8CString("isFinalAttempt");
			paramValue = JSObjectGetProperty(context, argument, paramName, NULL);
			if (JSValueIsBoolean(context, paramValue))
			{
				bFinalAttempt = JSValueToBoolean(context, paramValue);
			}
			JSStringRelease(paramName);

			paramName = JSStringCreateWithUTF8CString("authToken");
			paramValue = JSObjectGetProperty(context, argument, paramName, NULL);
			if (JSValueIsString(context, paramValue))
			{
				strAuthToken = aamp_JSValueToCString(context, paramValue, NULL);
			}
			JSStringRelease(paramName);
		}

		char* url = aamp_JSValueToCString(context, arguments[0], exception);
		aamp_ApplyPageHttpHeaders(pAAMP->_aamp);
		if (strAuthToken != NULL){
                        LOG_WARN(pAAMP,"authToken provided by the App");
			pAAMP->_aamp->SetSessionToken(strAuthToken);
		}
		{
                        LOG_WARN(pAAMP," _aamp->Tune(%d, %d, %d, %d, %d)", true, contentType, bFirstAttempt, bFinalAttempt, strTraceId);
			pAAMP->_aamp->Tune(url, true, contentType, bFirstAttempt, bFinalAttempt, strTraceId);
		}

		SAFE_DELETE_ARRAY(url);
		SAFE_DELETE_ARRAY(contentType);
		SAFE_DELETE_ARRAY(strTraceId);
	}
	else
	{
       		LOG_ERROR_EX("InvalidArgument: argumentCount=%d, expected: 1 or 2", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.load' - 1 or 2 argument required");
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to stop active playback
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_stop(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{

        LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if(!pAAMP)
	{
       		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.stop on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
   	LOG_WARN(pAAMP," aamp->stop()");
	pAAMP->_aamp->Stop();
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to set playback rate
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setRate(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
        LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if(!pAAMP)
	{
                LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setRate on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount < 1)
	{
       		LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 2", argumentCount);
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

                LOG_WARN(pAAMP,"rate=%f, overshoot=%d", rate, overshoot);
		{
           		LOG_WARN(pAAMP," _aamp->SetRate(%d, %d)", rate, overshoot);
			pAAMP->_aamp->SetRate(rate, overshoot);
		}
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to perform seek to a particular playback position
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_seek(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{

        LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if(!pAAMP)
	{
       		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.seek on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
        	LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1",argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.seek' - 1 argument required");
	}
	else
	{
		double position = JSValueToNumber(context, arguments[0], exception);
		{
                        
            		LOG_WARN(pAAMP,"_aamp->Seek(%g)",position);
			pAAMP->_aamp->Seek(position);
		}
	}
	return JSValueMakeUndefined(context);
}



/**
 * @brief Callback invoked from JS to perform seek to live point
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_seekToLive(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
    	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if(!pAAMP)
	{
        	LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.seekToLive on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
	{
        	LOG_WARN(pAAMP," _aamp->SeekToLive()");
		pAAMP->_aamp->SeekToLive();
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to set video rectangle co-ordinates
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setRect(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
    	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if(!pAAMP)
	{
        	LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setRect on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 4)
	{
        	LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 4", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setRect' - 4 arguments required");
	}
	else
	{
		int x = JSValueToNumber(context, arguments[0], exception);
		int y = JSValueToNumber(context, arguments[1], exception);
		int w = JSValueToNumber(context, arguments[2], exception);
		int h = JSValueToNumber(context, arguments[3], exception);
		{
            		LOG_WARN(pAAMP,"  _aamp->SetVideoRectangle(%d, %d, %d, %d)", x,y,w,h);
			pAAMP->_aamp->SetVideoRectangle(x, y, w, h);
		}
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to set video mute status
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setVideoMute(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
    	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
        	LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setVideoMute on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{

        	LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setVideoMute' - 1 argument required");
	}
	else
	{
		bool muted = JSValueToBoolean(context, arguments[0]);
        	LOG_WARN(pAAMP," _aamp->SetVideoMute(%d)", muted);
		pAAMP->_aamp->SetVideoMute(muted);
		// pAAMP->_aamp->SetSubtitleMute(muted);
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to set audio volume
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setAudioVolume(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{

    	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
        	LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setAudioVolume on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
        	LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setAudioVolume' - 1 argument required");
	}
	else
	{
		unsigned int volume = JSValueToNumber(context, arguments[0], exception);
        	LOG_WARN(pAAMP," aamp->SetAudioVolume(%d)", volume);
		pAAMP->_aamp->SetAudioVolume(volume);
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to set preferred zoom setting
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setZoom(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
    	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if(!pAAMP)
	{

        	LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setZoom on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
        	LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setZoom' - 1 argument required");
	}
	else
	{
		VideoZoomMode zoom;
		char* zoomStr = aamp_JSValueToCString(context, arguments[0], exception);
        	LOG_WARN(pAAMP,"zoomStr %s", zoomStr);
		if (0 == strcmp(zoomStr, "none"))
		{
			zoom = VIDEO_ZOOM_NONE;
		}
		else
		{
			zoom = VIDEO_ZOOM_FULL;
		}
		pAAMP->_aamp->SetVideoZoom(zoom);
		SAFE_DELETE_ARRAY(zoomStr);
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to set preferred audio language
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setLanguage(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{

    	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if(!pAAMP)
	{
        	LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setLanguage on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
        	LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setLanguage' - 1 argument required");
	}
	else
	{
		char* lang = aamp_JSValueToCString(context, arguments[0], exception);
		{
            		LOG_WARN(pAAMP," _aamp->SetLKanguage(%s)", lang);
			pAAMP->_aamp->SetLanguage(lang);
		}
		SAFE_DELETE_ARRAY(lang);
	}
	return JSValueMakeUndefined(context);
}

/**
 * @brief Callback invoked from JS to set list of subscribed tags
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setSubscribeTags(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{

    	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if(!pAAMP)
	{
        	LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.subscribedTags on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
        	LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setSubscribeTags' - 1 argument required");
	}
	else if (!aamp_JSValueIsArray(context, arguments[0]))
	{
        	LOG_ERROR(pAAMP,"InvalidArgument: aamp_JSValueIsArray=%d", aamp_JSValueIsArray(context, arguments[0]));
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
            		LOG_WARN(pAAMP," _aamp->SetSubscribedTags");
			pAAMP->_aamp->SetSubscribedTags(subscribedTags);
		}
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to add a custom HTTP header/s
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_addCustomHTTPHeader(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{

    	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{

        	LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.addCustomHTTPHeader on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 2)
	{

        	LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 2", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.addCustomHTTPHeader' - 2 argument required");
	}
	else
	{
		char *name = aamp_JSValueToCString(context, arguments[0], exception);
		std::string headerName(name);
		std::vector<std::string> headerVal;

		SAFE_DELETE_ARRAY(name);

		if (aamp_JSValueIsArray(context, arguments[1]))
		{
			headerVal = aamp_StringArrayToCStringArray(context, arguments[1]);
		}
		else if (JSValueIsString(context, arguments[1]))
		{
			headerVal.reserve(1);
			char *value =  aamp_JSValueToCString(context, arguments[1], exception);
			headerVal.push_back(value);
			SAFE_DELETE_ARRAY(value);
		}

		// Don't support empty values now
		if (headerVal.size() == 0)
		{
 
            		LOG_ERROR(pAAMP,"InvalidArgument: Custom header's value is empty");
			*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.addCustomHTTPHeader' - 2nd argument should be a string or array of strings");
			return JSValueMakeUndefined(context);
		}
                LOG_WARN(pAAMP,"  _aamp->AddCustomHTTPHeader headerName= %s headerVal= %p",headerName.c_str(), headerVal);
		pAAMP->_aamp->AddCustomHTTPHeader(headerName, headerVal);
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to remove custom HTTP headers
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_removeCustomHTTPHeader(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{

    	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
                
        	LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.removeCustomHTTPHeader on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
        	LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.removeCustomHTTPHeader' - 1 argument required");
	}
	else
	{
		char *name = aamp_JSValueToCString(context, arguments[0], exception);
		std::string headerName(name);
		LOG_WARN(pAAMP,"  AAMP_removeCustomHTTPHeader headerName= %s",headerName.c_str());
		pAAMP->_aamp->AddCustomHTTPHeader(headerName, std::vector<std::string>());
		SAFE_DELETE_ARRAY(name);

	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to set an ad
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setAds(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
        
    	LOG_TRACE("Enter");
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to get list of audio tracks
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_getAvailableAudioTracks(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
 
   	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
		
        	LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.getAvailableAudioTracks on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
	bool allTrack = false;
	if (argumentCount == 1)
	{
		allTrack = JSValueToBoolean(context, arguments[0]);
	}
	std::string tracks = pAAMP->_aamp->GetAvailableAudioTracks(allTrack);
	if (!tracks.empty())
	{
       		LOG_WARN(pAAMP,"Exit _aamp->GetAvailableAudioTracks(%d) tracks [%s]",allTrack,tracks.c_str());
		return aamp_CStringToJSValue(context, tracks.c_str());
	}
	else
	{
       		LOG_WARN(pAAMP," _aamp->GetAvailableAudioTracks(%d) tracks=NULL", allTrack);
		return JSValueMakeUndefined(context);
	}
}

/**
 * @brief Callback invoked from JS to get current audio track
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_getAudioTrack(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{

   	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
        	LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.getAudioTrack on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
	return JSValueMakeNumber(context, pAAMP->_aamp->GetAudioTrack());
}

/**
 * @brief Callback invoked from JS to get current audio track
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_getAudioTrackInfo(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{

   	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
        	LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.getAudioTrackInfo on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
    	LOG_INFO(pAAMP," _aamp->GetAudioTrackInfo()");
	std::string track = pAAMP->_aamp->GetAudioTrackInfo();
	if (!track.empty())
	{
		LOG_INFO(pAAMP," _aamp->GetAudioTrackInfo() [%s]",track.c_str()); 
		return aamp_CStringToJSValue(context, track.c_str());
	}
	else
	{
        	LOG_WARN(pAAMP,"  _aamp->GetAudioTrackInfo() track=NULL");
		return JSValueMakeUndefined(context);
	}
}

/**
 * @brief Callback invoked from JS to get current text track info
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_getTextTrackInfo(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
    	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
        	LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.getTextTrackInfo on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
	std::string track = pAAMP->_aamp->GetTextTrackInfo();
	if (!track.empty())
	{
		LOG_INFO(pAAMP,"_aamp->GetTextTrackInfo [%s]",track.c_str());
		return aamp_CStringToJSValue(context, track.c_str());
	}
	else
	{   	LOG_WARN(pAAMP,"_aamp->GetTextTrackInfo track=NULL");    
		return JSValueMakeUndefined(context);
	}
}

/**
 * @brief Callback invoked from JS to get current audio track
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_getPreferredAudioProperties(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{

    	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
        	LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.getPreferredAudioProperties on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
	std::string audioPreference = pAAMP->_aamp->GetPreferredAudioProperties();
	if (!audioPreference.empty())
	{    
        	LOG_INFO(pAAMP," _aamp->GetPreferredAudioProperties() [%s]",audioPreference.c_str());
		return aamp_CStringToJSValue(context, audioPreference.c_str());
	}
	else
	{
        	LOG_WARN(pAAMP,"_aamp->GetPreferredAudioProperties() audioPreference=NULL");
		return JSValueMakeUndefined(context);
	}
}

/**
 * @brief Callback invoked from JS to set audio track
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setAudioTrack(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
    	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
        	LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setAudioTrack on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
        	LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setAudioTrack' - 1 argument required");
	}
	else
	{
		int index = (int) JSValueToNumber(context, arguments[0], NULL);
		if (index >= 0)
		{
			{
                		LOG_WARN(pAAMP," _aamp->SetAudioTrack(%d)", index);
				pAAMP->_aamp->SetAudioTrack(index);
			}
		}
		else
		{
            		LOG_ERROR(pAAMP,"InvalidArgument: Track index should be >= 0!");
			*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setAudioTrack' - argument should be >= 0");
		}
	}
	return JSValueMakeUndefined(context);
}

/**
 * @brief Callback invoked from JS to get preferred text properties
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_getPreferredTextProperties(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{

    	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{

        	LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.getPreferredTextProperties on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
	std::string textPreference = pAAMP->_aamp->GetPreferredTextProperties();
	if (!textPreference.empty())
	{
		LOG_INFO(pAAMP," _aamp->GetPreferredTextProperties() [%s]",textPreference.c_str());
		return aamp_CStringToJSValue(context, textPreference.c_str());
	}
	else
	{
		LOG_WARN(pAAMP," _aamp->GetPreferredTextProperties() textPreference=NULL"); 
		return JSValueMakeUndefined(context);
	}
}

/**
 * @brief Callback invoked from JS to get list of text tracks
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_getAvailableTextTracks(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	
    	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.getAvailableTextTracks on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
	bool allTrack = false;
	if (argumentCount == 1)
	{
		allTrack = JSValueToBoolean(context, arguments[0]);
	}
	std::string tracks = pAAMP->_aamp->GetAvailableTextTracks(allTrack);
	if (!tracks.empty())
	{
		LOG_WARN(pAAMP,"_aamp->GetAvailableTextTracks(%d) [%s]",allTrack,tracks.c_str());
		return aamp_CStringToJSValue(context, tracks.c_str());
	}
	else
	{
        	LOG_WARN(pAAMP,"_aamp->GetAvailableTextTracks(%d) tracks=NULL",allTrack);
		return JSValueMakeUndefined(context);
	}
}


/**
 * @brief Callback invoked from JS to get current text track index
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_getTextTrack(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.getTextTrack on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
        LOG_INFO(pAAMP," _aamp->GetTextTrack()");
	return JSValueMakeNumber(context, pAAMP->_aamp->GetTextTrack());
}


/**
 * @brief Callback invoked from JS to set text track
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setTextTrack(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setTextTrack on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
        	LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1 or 2", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setTextTrack' - atleast 1 argument required");
	}
	else
	{
		int index = (int) JSValueToNumber(context, arguments[0], NULL);
		if (index >= MUTE_SUBTITLES_TRACKID) // -1 disable subtitles, >= 0 subtitle track index
		{
                        LOG_WARN(pAAMP," _aamp->SetTextTrack(%d)", index);
			pAAMP->_aamp->SetTextTrack(index);
		}
		else
		{
                       	LOG_ERROR(pAAMP,"InvalidArgument - track index should be >= 0!");
			*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Text track index should be >= -1 !");
		}
	}
	return JSValueMakeUndefined(context);
}

/**
 * @brief Callback invoked from JS to get list of video tracks
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_getAvailableVideoTracks(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.getAvailableAudioTracks on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
	std::string tracks = pAAMP->_aamp->GetAvailableVideoTracks();
	if (!tracks.empty())
	{
		LOG_INFO(pAAMP," _aamp->GetAvailableVideoTracks()[%s]",tracks.c_str());
		return aamp_CStringToJSValue(context, tracks.c_str());
	}
	else
	{
		LOG_INFO(pAAMP," _aamp->GetAvailableVideoTracks() tracks=NULL");
		return JSValueMakeUndefined(context);
	}
}

/**
 * @brief Callback invoked from JS to set video track
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setVideoTracks(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setVideoTrack on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	std::vector<long> bitrateList;
	for (int i = 0; i < argumentCount; i++)
	{
		long bitrate  = JSValueToNumber(context, arguments[i], exception) ;
		LOG_WARN(pAAMP,"_aamp->SetVideoTracks argCount:%d value:%ld",i, bitrate);
		bitrateList.push_back(bitrate);
	}
	pAAMP->_aamp->SetVideoTracks(bitrateList);

	return JSValueMakeUndefined(context);
}

/**
 * @brief Callback invoked from JS to set license server URL
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setLicenseServerURL(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setLicenseServerURL on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
               	LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1",argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setLicenseServerURL' - 1 argument required");
	}
	else
	{
		const char *url = aamp_JSValueToCString(context, arguments[0], exception);
                LOG_WARN(pAAMP," _aamp->SetLicenseServerURL(%s)", url);
		pAAMP->_aamp->SetLicenseServerURL(url);
		SAFE_DELETE_ARRAY(url);
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to set the preferred DRM
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setPreferredDRM(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setPreferredDRM on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
	        LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1",argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setPreferredDRM' - 1 argument required");
	}
	else
	{
		const char *drm = aamp_JSValueToCString(context, arguments[0], exception);
		if (strncasecmp(drm, "widevine", 8) == 0)
		{
			LOG_WARN(pAAMP,"_aamp->SetPreferredDRM  WideWine");
			pAAMP->_aamp->SetPreferredDRM(eDRM_WideVine);
		}
		if (strncasecmp(drm, "playready", 9) == 0)
		{
		        LOG_WARN(pAAMP,"_aamp->SetPreferredDRM  PlayReady");
			pAAMP->_aamp->SetPreferredDRM(eDRM_PlayReady);
		}
		SAFE_DELETE_ARRAY(drm);
	}
	return JSValueMakeUndefined(context);
}

/**
 * @brief Callback invoked from JS to en/disable anonymous request
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setAnonymousRequest(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setAnonymousRequest on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
        	LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setAnonymousRequest' - 1 argument required");
	}
	else
	{
		bool isAnonymousReq = JSValueToBoolean(context, arguments[0]);
        	LOG_WARN(pAAMP," _aamp->SetAnonymousRequest(%d)", isAnonymousReq);
		pAAMP->_aamp->SetAnonymousRequest(isAnonymousReq);
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to set vod trickplay FPS
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setVODTrickplayFPS(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
        LOG_TRACE("Enter");
        AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
        if (!pAAMP)
        {
                LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
                *exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setAnonymousRequest on instances of AAMP");
                return JSValueMakeUndefined(context);
        }

        if (argumentCount != 1)
        {
               	LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1", argumentCount);
                *exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setAnonymousRequest' - 1 argument required");
        }
        else
        {
                int vodTrickplayFPS = (int)JSValueToNumber(context, arguments[0], exception);
                LOG_WARN(pAAMP," _aamp->SetVODTrickplayFPS(%d)", vodTrickplayFPS);
                pAAMP->_aamp->SetVODTrickplayFPS(vodTrickplayFPS);
        }
        return JSValueMakeUndefined(context);
}

/**
 * @brief Callback invoked from JS to set alternate playback content URLs
 *
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 *
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setAlternateContent(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	
        LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.SetAlternateContents() on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 2)
	{
                LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 2", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.SetAlternateContent' - 1 argument required");
	}
	else
	{
		/*
		 * Parmater format
		 "reservationObject": object {
		 	"reservationId": "773701056",
			"reservationBehavior": number
			"placementRequest": {
			 "id": string,
		         "pts": number,
		         "url": "",
		     },
		 },
		 "promiseCallback": function
	 	 */
		char *reservationId = NULL;
		int reservationBehavior = -1;
		char *adId = NULL;
		long adPTS = -1;
		char *adURL = NULL;
		if (JSValueIsObject(context, arguments[0]))
		{
			//Parse the ad object
			JSObjectRef reservationObject = JSValueToObject(context, arguments[0], NULL);
			if (reservationObject == NULL)
			{
                               	LOG_ERROR(pAAMP,"Unable to convert argument to JSObject");
				return JSValueMakeUndefined(context);
			}
			JSStringRef propName = JSStringCreateWithUTF8CString("reservationId");
			JSValueRef propValue = JSObjectGetProperty(context, reservationObject, propName, NULL);
			if (JSValueIsString(context, propValue))
			{
				reservationId = aamp_JSValueToCString(context, propValue, NULL);
			}
			JSStringRelease(propName);

			propName = JSStringCreateWithUTF8CString("reservationBehavior");
			propValue = JSObjectGetProperty(context, reservationObject, propName, NULL);
			if (JSValueIsNumber(context, propValue))
			{
				reservationBehavior = JSValueToNumber(context, propValue, NULL);
			}
			JSStringRelease(propName);

			propName = JSStringCreateWithUTF8CString("placementRequest");
			propValue = JSObjectGetProperty(context, reservationObject, propName, NULL);
			if (JSValueIsObject(context, propValue))
			{
				JSObjectRef adObject = JSValueToObject(context, propValue, NULL);

				JSStringRef adPropName = JSStringCreateWithUTF8CString("id");
				JSValueRef adPropValue = JSObjectGetProperty(context, adObject, adPropName, NULL);
				if (JSValueIsString(context, adPropValue))
				{
					adId = aamp_JSValueToCString(context, adPropValue, NULL);
				}
				JSStringRelease(adPropName);

				adPropName = JSStringCreateWithUTF8CString("pts");
				adPropValue = JSObjectGetProperty(context, adObject, adPropName, NULL);
				if (JSValueIsNumber(context, adPropValue))
				{
					adPTS = (long) JSValueToNumber(context, adPropValue, NULL);
				}
				JSStringRelease(adPropName);

				adPropName = JSStringCreateWithUTF8CString("url");
				adPropValue = JSObjectGetProperty(context, adObject, adPropName, NULL);
				if (JSValueIsString(context, adPropValue))
				{
					adURL = aamp_JSValueToCString(context, adPropValue, NULL);
				}
				JSStringRelease(adPropName);
			}
			JSStringRelease(propName);
		}

		JSObjectRef callbackObj = JSValueToObject(context, arguments[1], NULL);

		if (callbackObj != NULL && JSObjectIsFunction(context, callbackObj) && reservationId && adId && adURL)
		{
			if (pAAMP->_promiseCallback)
			{
				JSValueUnprotect(context, pAAMP->_promiseCallback);
			}
			pAAMP->_promiseCallback = callbackObj;
			JSValueProtect(context, pAAMP->_promiseCallback);
			std::string adBreakId(reservationId);  //CID:89434 - Resolve Forward null
			std::string adIdStr(adId);  //CID:89725 - Resolve Forward null
			std::string url(adURL);  //CID:86272 - Resolve Forward null
                        LOG_WARN(pAAMP,"pAAMP->_aamp->SetAlternateContents with promiseCallback:%p", callbackObj);
			pAAMP->_aamp->SetAlternateContents(adBreakId, adIdStr, url);
		}
		else
		{
                       LOG_ERROR(pAAMP,"Unable to parse the promiseCallback argument");

		}
		SAFE_DELETE_ARRAY(reservationId);
		SAFE_DELETE_ARRAY(adURL);
		SAFE_DELETE_ARRAY(adId);
	}
	return JSValueMakeUndefined(context);
}

/**
 * @brief Notify AAMP that the reservation is complete
 *
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 *
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_notifyReservationCompletion(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.notifyReservationCompletion() on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 2)
	{
        	LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 2", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.notifyReservationCompletion' - 1 argument required");
	}
	else
	{
		const char * reservationId = aamp_JSValueToCString(context, arguments[0], exception);
		long time = (long) JSValueToNumber(context, arguments[1], exception);
		//Need an API in AAMP to notify that placements for this reservation are over and AAMP might have to trim
		//the ads to the period duration or not depending on time param
                LOG_WARN(pAAMP,"Called reservation close for periodId:%s and time:%ld", reservationId, time);
		SAFE_DELETE_ARRAY(reservationId);
	}
	return JSValueMakeUndefined(context);
}

/**
 * @brief Callback invoked from JS to set linear trickplay FPS
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setLinearTrickplayFPS(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
        LOG_TRACE("Enter");
        AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
        if (!pAAMP)
        {
                LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
                *exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setAnonymousRequest on instances of AAMP");
                return JSValueMakeUndefined(context);
        }

        if (argumentCount != 1)
        {
               	LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1", argumentCount);
                *exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setAnonymousRequest' - 1 argument required");
        }
        else
        {
                int linearTrickplayFPS = (int)JSValueToNumber(context, arguments[0], exception);
		LOG_WARN(pAAMP," _aamp->SetLinearTrickplayFPS(%d)", linearTrickplayFPS);
                pAAMP->_aamp->SetLinearTrickplayFPS(linearTrickplayFPS);
        }
        return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to set live offset
 *
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 *
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setLiveOffset(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setLiveOffset on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
       	 	LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setLiveOffset' - 1 argument required");
	}
	else
	{
		double liveOffset = (double)JSValueToNumber(context, arguments[0], exception);
		LOG_WARN(pAAMP," _aamp->SetLiveOffset(%lf)", liveOffset);
		pAAMP->_aamp->SetLiveOffset(liveOffset);
	}
	return JSValueMakeUndefined(context);
}

/**
 * @brief Callback invoked from JS to set live offset for 4K
 *
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 *
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setLiveOffset4K(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setLiveOffset4K on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
		LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setLiveOffset4K' - 1 argument required");
	}
	else
	{
		double liveOffset = (double)JSValueToNumber(context, arguments[0], exception);
		LOG_WARN(pAAMP," _aamp->SetLiveOffset4K(%lf)",liveOffset);
		pAAMP->_aamp->SetLiveOffset4K(liveOffset);
	}
	return JSValueMakeUndefined(context);
}

/**
 * @brief Callback invoked from JS to set download stall timeout value
 *
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 *
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setDownloadStallTimeout(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setDownloadStallTimeout on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
        	LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setDownloadStallTimeout' - 1 argument required");
	}
	else
	{
		long stallTimeout = (long)JSValueToNumber(context, arguments[0], exception);
		LOG_WARN(pAAMP," _aamp->SetDownloadStallTimeout(%ld)",stallTimeout);
		pAAMP->_aamp->SetDownloadStallTimeout(stallTimeout);
	}

	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to set download start timeout value
 *
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 *
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setDownloadStartTimeout(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setDownlodStartTimeout on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
        	LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setDownloadStartTimeout' - 1 argument required");
	}
	else
	{
		long startTimeout = (long)JSValueToNumber(context, arguments[0], exception);
        	LOG_WARN(pAAMP,"SetDownloadStartTimeout %ld",startTimeout);
		pAAMP->_aamp->SetDownloadStartTimeout(startTimeout);
	}

	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to set network timeout value
 *
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 *
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setNetworkTimeout(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setNetworkTimeout on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
		
       		LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setNetworkTimeout' - 1 argument required");
	}
	else
	{
		double networkTimeout = (double)JSValueToNumber(context, arguments[0], exception);
        	LOG_WARN(pAAMP," _aamp->SetNetworkTimeout %lf",networkTimeout);
		pAAMP->_aamp->SetNetworkTimeout(networkTimeout);
	}

	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to enable/disable CC rendering
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setClosedCaptionStatus(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setClosedCaptionStatus on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
		LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1", argumentCount);                
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setClosedCaptionStatus' - 1 argument required");
	}
	else
	{
		bool enabled = JSValueToBoolean(context, arguments[0]);
        	LOG_WARN(pAAMP,"_aamp->SetCCStatus(%d)", enabled);
		pAAMP->_aamp->SetCCStatus(enabled);
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to set text style
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setTextStyleOptions(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setTextStyleOptions on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
        	LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setTextStyleOptions' - 1 argument required");
	}
	else
	{
		if (JSValueIsString(context, arguments[0]))
		{
			const char *options = aamp_JSValueToCString(context, arguments[0], NULL);
            		LOG_WARN(pAAMP,"_aamp->SetTextStyle(%s)", options);
			pAAMP->_aamp->SetTextStyle(std::string(options));
			SAFE_DELETE_ARRAY(options);

		}
		else
		{
            		LOG_ERROR(pAAMP,"InvalidArgument: Argument should be JSON formatted string");
			*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setTextStyleOptions' - argument should be JSON formatted string");
		}
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to get text style
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_getTextStyleOptions(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.getTextStyleOptions on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
	std::string options = pAAMP->_aamp->GetTextStyle();
	if (!options.empty())
	{
		LOG_INFO(pAAMP,"_aamp->GetTextStyle() [%s]",options.c_str());
		return aamp_CStringToJSValue(context, options.c_str());
	}
	else
	{
		LOG_WARN(pAAMP,"_aamp->GetTextStyle() options=NULL");
		return JSValueMakeUndefined(context);
	}
}


/**
 * @brief Callback invoked from JS to set the preferred language format
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setLanguageFormat(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setLanguageFormat on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 2)
	{
       		LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setLanguageFormat' - 2 argument required");
	}
	else
	{
		int preferredFormat = (int) JSValueToNumber(context, arguments[0], NULL);
		bool useRole = JSValueToBoolean(context, arguments[1]);
        	LOG_WARN(pAAMP," aamp->SetLanguageFormat(%d, %d)", preferredFormat, useRole);
		pAAMP->_aamp->SetLanguageFormat((LangCodePreference) preferredFormat, useRole);
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to set the license caching config
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setLicenseCaching(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setLicenseCaching on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
		
        	LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setLicenseCaching' - 1 argument required");
	}
	else
	{
		bool enabled = JSValueToBoolean(context, arguments[0]);
		LOG_WARN(pAAMP," _aamp->SetLicenseCaching %d)",enabled);
		pAAMP->_aamp->SetLicenseCaching(enabled);
	}
	return JSValueMakeUndefined(context);
}

/**
 * @brief Callback invoked from JS to set auxiliary audio language
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setAuxiliaryLanguage(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	if(!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setAuxiliaryLanguage on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
		LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setAuxiliaryLanguage' - 1 argument required");
	}
	else
	{
		char* lang = aamp_JSValueToCString(context, arguments[0], exception);
        	LOG_WARN(pAAMP," _aamp->SetAuxiliaryLanguage(%s)", lang);
		pAAMP->_aamp->SetAuxiliaryLanguage(lang);
		SAFE_DELETE_ARRAY(lang);
	}
	return JSValueMakeUndefined(context);
}
/**
 * @brief Callback invoked from JS to get playback stats
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_getPlayeBackStats(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP || !pAAMP->_aamp)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.getPlayeBackStats on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
	return aamp_CStringToJSValue(context, pAAMP->_aamp->GetPlaybackStats().c_str());
}

/**
 *  * @brief Callback invoked from JS to set xre supported tune
 *  * @param[in] context JS execution context
 *  * @param[in] function JSObject that is the function being called
 *  * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 *  * @param[in] argumentCount number of args
 *  * @param[in] arguments[] JSValue array of args
 *  * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 *  * @retval JSValue that is the function's return value
 *  */
static JSValueRef AAMP_xreSupportedTune(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.xreSupprotedTune on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
	if (argumentCount != 1)
	{
		LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.xreSupprotedTune' - 1 argument required");
	}
	else
	{
		bool xreSupported = JSValueToBoolean(context, arguments[0]);
        	LOG_WARN(pAAMP," _aamp->XRESupportedTune(%d)",xreSupported);
		pAAMP->_aamp->XRESupportedTune(xreSupported);
	}
	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback invoked from JS to set content protection data update timeout value on key rotation
 *
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 *
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setContentProtectionDataUpdateTimeout(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if (!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setContentProtectionDataUpdateTimeout on instances of AAMP");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount != 1)
	{
		LOG_ERROR(pAAMP,"InvalidArgument: argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setContentProtectionDataUpdateTimeout' - 1 argument required");
	}
	else
	{
		int contentProtectionDataUpdateTimeout = (int)JSValueToNumber(context, arguments[0], exception);
		LOG_WARN(pAAMP," _aamp->SetContentProtectionDataUpdateTimeout %d",contentProtectionDataUpdateTimeout);
		pAAMP->_aamp->SetContentProtectionDataUpdateTimeout(contentProtectionDataUpdateTimeout);
	}
	return JSValueMakeUndefined(context);
}

/**
 * @brief Callback invoked from JS to update content protection data value on key rotation
 *
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 *
 * @retval JSValue that is the function's return value
 */

static JSValueRef AAMP_setContentProtectionDataConfig(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
	if(!pAAMP)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setContentProtectionDataConfig on instances of AAMP");
		return JSValueMakeUndefined(context);
	}
	if (argumentCount == 1 && JSValueIsObject(context, arguments[0]))
	{
		const char *jsonbuffer = aamp_JSValueToJSONCString(context,arguments[0], exception);
        	LOG_WARN(pAAMP," Response json call ProcessContentProtection %s",jsonbuffer);
		pAAMP->_aamp->ProcessContentProtectionDataConfig(jsonbuffer);
		SAFE_DELETE_ARRAY(jsonbuffer);
	}
	else
	{
        	LOG_ERROR(pAAMP,"IvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute setContentProtectionDataConfig() - 1 argument of type IConfig required");
	}
	return JSValueMakeUndefined(context);
}

/**
 * @brief Callback invoked from JS to Enable/Disable Dynamic DRM Config support
 *
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 *
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMP_setRuntimeDRMConfig(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
       LOG_TRACE("Enter");
       AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject);
       if (!pAAMP)
       {
               LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
               *exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP.setDynamicDRMConfig on instances of AAMP");
               return JSValueMakeUndefined(context);
       }
       if(argumentCount != 1)
       {
               LOG_ERROR(pAAMP,"IvalidArgument - argumentCount=%d, expected: 1", argumentCount);
               *exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP.setDynamicDRMConfig - 1 argument required");
       }
       else
       {
               bool DynamicDRMSupport = (bool)JSValueToBoolean(context, arguments[0]);
               LOG_WARN(pAAMP," _aamp->SetRuntimeDRMConfigSupport %d",DynamicDRMSupport);
               pAAMP->_aamp->SetRuntimeDRMConfigSupport(DynamicDRMSupport);
       }
       return JSValueMakeUndefined(context);
}


/**
 * @brief Array containing the AAMP's statically declared functions
 */
static const JSStaticFunction AAMP_staticfunctions[] =
{
	{ "addEventListener", AAMP_addEventListener, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "removeEventListener", AAMP_removeEventListener, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setProperties", AAMP_setProperties, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "getProperties", AAMP_getProperties, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "tune", AAMP_tune, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "load", AAMP_load, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
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
	{ "getAvailableVideoTracks", AAMP_getAvailableVideoTracks, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setVideoTracks", AAMP_setVideoTracks, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "getAvailableAudioTracks", AAMP_getAvailableAudioTracks, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "getAudioTrack", AAMP_getAudioTrack, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "getAudioTrackInfo", AAMP_getAudioTrackInfo, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "getTextTrackInfo", AAMP_getTextTrackInfo, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "getPreferredAudioProperties", AAMP_getPreferredAudioProperties, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "getPreferredTextProperties", AAMP_getPreferredTextProperties, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setAudioTrack", AAMP_setAudioTrack, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "getAvailableTextTracks", AAMP_getAvailableTextTracks, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "getTextTrack", AAMP_getTextTrack, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setTextTrack", AAMP_setTextTrack, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setVideoMute", AAMP_setVideoMute, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setAudioVolume", AAMP_setAudioVolume, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "addCustomHTTPHeader", AAMP_addCustomHTTPHeader, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "removeCustomHTTPHeader", AAMP_removeCustomHTTPHeader, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setLicenseServerURL", AAMP_setLicenseServerURL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setPreferredDRM", AAMP_setPreferredDRM, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setAnonymousRequest", AAMP_setAnonymousRequest, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },

	{ "setVODTrickplayFPS", AAMP_setVODTrickplayFPS, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },

	{ "setLinearTrickplayFPS", AAMP_setLinearTrickplayFPS, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setLiveOffset", AAMP_setLiveOffset, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setLiveOffset4K", AAMP_setLiveOffset4K, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setDownloadStallTimeout", AAMP_setDownloadStallTimeout, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setDownloadStartTimeout", AAMP_setDownloadStartTimeout, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setNetworkTimeout", AAMP_setNetworkTimeout, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setAlternateContent", AAMP_setAlternateContent, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "notifyReservationCompletion", AAMP_notifyReservationCompletion, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setClosedCaptionStatus", AAMP_setClosedCaptionStatus, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setTextStyleOptions", AAMP_setTextStyleOptions, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "getTextStyleOptions", AAMP_getTextStyleOptions, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setLanguageFormat", AAMP_setLanguageFormat, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setLicenseCaching", AAMP_setLicenseCaching, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setAuxiliaryLanguage", AAMP_setAuxiliaryLanguage, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "xreSupportedTune", AAMP_xreSupportedTune, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getPlaybackStatistics", AAMP_getPlayeBackStats, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setContentProtectionDataConfig", AAMP_setContentProtectionDataConfig, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setContentProtectionDataUpdateTimeout", AAMP_setContentProtectionDataUpdateTimeout, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "configRuntimeDRM", AAMP_setRuntimeDRMConfig, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ NULL, NULL, 0 }
};


/**
 * @brief Callback invoked when an object of AAMP is finalized
 * @param[in] thisObject JSObject being finalized
 */
static void AAMP_finalize(JSObjectRef thisObject)
{
        
	AAMP_JS* pAAMP = (AAMP_JS*)JSObjectGetPrivate(thisObject); 
	LOG_WARN_EX("object=%p", thisObject);
	if (pAAMP == NULL)
	{
		LOG_ERROR_EX("JSObjectGetPrivate returned NULL!");
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
		if(pAAMP->_promiseCallback) {
			JSValueUnprotect(pAAMP->_ctx, pAAMP->_promiseCallback);
		}
	}

	pthread_mutex_lock(&mutex);
	if (NULL != _allocated_aamp)
	{
		//when finalizing JS object, don't generate state change events
        	LOG_WARN(pAAMP," aamp->Stop(false)");
		_allocated_aamp->Stop(false);
        	LOG_WARN(pAAMP,"delete aamp %p",_allocated_aamp);
		SAFE_DELETE(_allocated_aamp);
	}
	pthread_mutex_unlock(&mutex);
	SAFE_DELETE(pAAMP);

#ifdef AAMP_CC_ENABLED
	//disable CC rendering so that state will not be persisted between two different sessions.
	logprintf("[%s] Disabling CC", __FUNCTION__);
	AampCCManager::GetInstance()->SetStatus(false);
#endif
}


/**
 * @brief Structure contains properties and callbacks of AAMP object
 */
static const JSClassDefinition AAMP_class_def =
{
	0,
	kJSClassAttributeNone,
	"__AAMP__class",
	NULL,
	AAMP_static_values,
	AAMP_staticfunctions,
	NULL,
	NULL, // _allocated_aamp is reused, so don't clean when one object goes out of scope
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
 * @brief Creates a JavaScript class of AAMP for use with JSObjectMake
 * @retval singleton instance of JavaScript class created
 */
static JSClassRef AAMP_class_ref() {
	static JSClassRef _classRef = NULL;
	if (!_classRef) {
		_classRef = JSClassCreate(&AAMP_class_def);
	}
	return _classRef;
}


/**
 * @brief Callback invoked from JS to get the AD_PLAYBACK_STARTED property value
 * @param[in] context JS execution context
 * @param[in] thisObject JSObject to search for the property
 * @param[in] propertyName JSString containing the name of the property to get
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval property's value if object has the property, otherwise NULL
 */
static JSValueRef EventType_getproperty_AD_PLAYBACK_STARTED(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG_TRACE("Enter");
	return aamp_CStringToJSValue(context, "adPlaybackStarted");
}


/**
 * @brief Callback invoked from JS to get the AD_PLAYBACK_COMPLETED property value
 * @param[in] context JS execution context
 * @param[in] thisObject JSObject to search for the property
 * @param[in] propertyName JSString containing the name of the property to get
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval property's value if object has the property, otherwise NULL
 */
static JSValueRef EventType_getproperty_AD_PLAYBACK_COMPLETED(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG_TRACE("Enter");
	return aamp_CStringToJSValue(context, "adPlaybackCompleted");
}


/**
 * @brief Callback invoked from JS to get the AD_PLAYBACK_INTERRUPTED property value
 * @param[in] context JS execution context
 * @param[in] thisObject JSObject to search for the property
 * @param[in] propertyName JSString containing the name of the property to get
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval property's value if object has the property, otherwise NULL
 */
static JSValueRef EventType_getproperty_AD_PLAYBACK_INTERRUPTED(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG_TRACE("Enter");
	return aamp_CStringToJSValue(context, "adPlaybackInterrupted");
}


/**
 * @brief Callback invoked from JS to get the BUFFERING_BEGIN property value
 * @param[in] context JS execution context
 * @param[in] thisObject JSObject to search for the property
 * @param[in] propertyName JSString containing the name of the property to get
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval property's value if object has the property, otherwise NULL
 */
static JSValueRef EventType_getproperty_BUFFERING_BEGIN(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG_TRACE("Enter");
	return aamp_CStringToJSValue(context, "bufferingBegin");
}


/**
 * @brief Callback invoked from JS to get the BUFFERING_END property value
 * @param[in] context JS execution context
 * @param[in] thisObject JSObject to search for the property
 * @param[in] propertyName JSString containing the name of the property to get
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval property's value if object has the property, otherwise NULL
 */
static JSValueRef EventType_getproperty_BUFFERING_END(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG_TRACE("Enter");
	return aamp_CStringToJSValue(context, "bufferingEnd");
}


/**
 * @brief Callback invoked from JS to get the DECODER_AVAILABLE property value
 * @param[in] context JS execution context
 * @param[in] thisObject JSObject to search for the property
 * @param[in] propertyName JSString containing the name of the property to get
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval property's value if object has the property, otherwise NULL
 */
static JSValueRef EventType_getproperty_DECODER_AVAILABLE(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG_TRACE("Enter");
	return aamp_CStringToJSValue(context, "decoderAvailable");
}


/**
 * @brief Callback invoked from JS to get the DRM_METADATA_INFO_AVAILABLE property value
 * @param[in] context JS execution context
 * @param[in] thisObject JSObject to search for the property
 * @param[in] propertyName JSString containing the name of the property to get
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval property's value if object has the property, otherwise NULL
 */
static JSValueRef EventType_getproperty_DRM_METADATA_INFO_AVAILABLE(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG_TRACE("Enter");
	return aamp_CStringToJSValue(context, "drmMetadataInfoAvailable");
}





/**
 * @brief Callback invoked from JS to get the DRM_METADATA property value
 * @param[in] context JS execution context
 * @param[in] thisObject JSObject to search for the property
 * @param[in] propertyName JSString containing the name of the property to get
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval property's value if object has the property, otherwise NULL
 */
static JSValueRef EventType_getproperty_DRM_METADATA(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
        LOG_TRACE("Enter");
        return aamp_CStringToJSValue(context, "drmMetadata");
}




/**
 * @brief Callback invoked from JS to get the ENTERING_LIVE property value
 * @param[in] context JS execution context
 * @param[in] thisObject JSObject to search for the property
 * @param[in] propertyName JSString containing the name of the property to get
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval property's value if object has the property, otherwise NULL
 */
static JSValueRef EventType_getproperty_ENTERING_LIVE(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG_TRACE("Enter");
	return aamp_CStringToJSValue(context, "enteringLive");
}


/**
 * @brief Callback invoked from JS to get the MEDIA_OPENED property value
 * @param[in] context JS execution context
 * @param[in] thisObject JSObject to search for the property
 * @param[in] propertyName JSString containing the name of the property to get
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval property's value if object has the property, otherwise NULL
 */
static JSValueRef EventType_getproperty_MEDIA_OPENED(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG_TRACE("Enter");
	return aamp_CStringToJSValue(context, "mediaOpened");
}


/**
 * @brief Callback invoked from JS to get the MEDIA_STOPPED property value
 * @param[in] context JS execution context
 * @param[in] thisObject JSObject to search for the property
 * @param[in] propertyName JSString containing the name of the property to get
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval property's value if object has the property, otherwise NULL
 */
static JSValueRef EventType_getproperty_MEDIA_STOPPED(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	LOG_TRACE("Enter");
	return aamp_CStringToJSValue(context, "mediaStopped");
}


/**
 * @brief Array containing the EventType's statically declared value properties
 */
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
 * @brief Callback invoked from JS when an object of EventType is first created
 * @param[in] ctx JS execution context
 * @param[in] thisObject JSObject being created
 */
static void EventType_init(JSContextRef ctx, JSObjectRef thisObject)
{
	LOG_TRACE("Enter");
}


/**
 * @brief Callback invoked when an object of EventType is finalized
 * @param[in] thisObject JSObject being finalized
 */
static void EventType_finalize(JSObjectRef thisObject)
{
	LOG_TRACE("Enter");
}


/**
 * @brief Structure contains properties and callbacks of EventType object
 */
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
 * @brief Creates a JavaScript class of EventType for use with JSObjectMake
 * @retval singleton instance of JavaScript class created
 */
static JSClassRef EventType_class_ref() {
	static JSClassRef _classRef = NULL;
	if (!_classRef) {
		_classRef = JSClassCreate(&EventType_object_def);
	}
	return _classRef;
}


/**
 * @brief Create a EventType JS object
 * @param[in] context JS execute context
 * @retval JSObject of EventType
 */
JSObjectRef AAMP_JS_AddEventTypeClass(JSGlobalContextRef context)
{
        LOG_TRACE("context=%p", context);

	JSObjectRef obj = JSObjectMake(context, EventType_class_ref(), context);

	return obj;
}


/**
 *   @brief  Load aamp JS bindings.
 */
void aamp_LoadJS(void* context, void* playerInstanceAAMP)
{
    	LOG_WARN_EX("context=%p, aamp=%p", context, playerInstanceAAMP);
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
			_allocated_aamp = new PlayerInstanceAAMP(NULL, NULL);
        		LOG_WARN_EX("create aamp %p", _allocated_aamp);
					}
		else
		{
            		LOG_WARN_EX("reuse aamp %p", _allocated_aamp);

		}
		pAAMP->_aamp = _allocated_aamp;
		pthread_mutex_unlock(&mutex);
	}

	pAAMP->_listeners = NULL;

    	//Get PLAYER ID and store for future use in logging
	pAAMP->iPlayerId = pAAMP->_aamp->GetId();
    	//Get jsinfo config for INFO logging
	pAAMP->bInfoEnabled =  pAAMP->_aamp->IsJsInfoLoggingEnabled();
	
	// DELIA-48250 Set tuned event configuration to playlist indexed
	pAAMP->_aamp->SetTuneEventConfig(eTUNED_EVENT_ON_PLAYLIST_INDEXED);
	// DELIA-48278 Set EnableVideoRectangle to false, this is tied to westeros config
	pAAMP->_aamp->EnableVideoRectangle(false);

	pAAMP->_eventType = AAMP_JS_AddEventTypeClass(jsContext);
	JSValueProtect(jsContext, pAAMP->_eventType);

	pAAMP->_subscribedTags = NULL;
	pAAMP->_promiseCallback = NULL;
	AAMP_JSListener::AddEventListener(pAAMP, AAMP_EVENT_AD_RESOLVED, NULL);

	JSObjectRef classObj = JSObjectMake(jsContext, AAMP_class_ref(), pAAMP);
	JSObjectRef globalObj = JSContextGetGlobalObject(jsContext);
	JSStringRef str = JSStringCreateWithUTF8CString("AAMP");
	JSObjectSetProperty(jsContext, globalObj, str, classObj, kJSPropertyAttributeReadOnly, NULL);
	JSStringRelease(str);
}

/**
 * @brief  Unload aamp JS bindings.
 *
 */
void aamp_UnloadJS(void* context)
{
	
     	LOG_WARN_EX("context=%p", context);
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

	// Only a single AAMP object is available in this JS context which is added at the time of webpage load
	// So it makes sense to remove the object when webpage is unloaded.
	AAMP_finalize(aampObj);

	// Per comments in DELIA-48964, use JSObjectDeleteProperty instead of JSObjectSetProperty when trying to invalidate a read-only property
	JSObjectDeleteProperty(jsContext, globalObj, str, NULL);
	JSStringRelease(str);

	// Force a garbage collection to clean-up all AAMP objects.
        LOG_TRACE("JSGarbageCollect() context=%p", context);
	JSGarbageCollect(jsContext);
}

#ifdef __GNUC__

/**
 * @brief Stops any prevailing AAMP instances before exit of program
 */
void __attribute__ ((destructor(101))) _aamp_term()
{
	
    	LOG_TRACE("Enter");
	pthread_mutex_lock(&mutex);
	if (NULL != _allocated_aamp)
	{
        	LOG_WARN_EX("stopping aamp");
		//when finalizing JS object, don't generate state change events
		_allocated_aamp->Stop(false);
        	LOG_WARN_EX("stopped aamp");
		delete _allocated_aamp;
		_allocated_aamp = NULL;
	}
	pthread_mutex_unlock(&mutex);

	//Clear any active js mediaplayer instances on term
	ClearAAMPPlayerInstances();
}
#endif
