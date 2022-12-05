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
 * @file jscontroller-jsbindings.cpp
 * @brief JavaScript bindings for AAMP_JSController
 */


#include <JavaScriptCore/JavaScript.h>

#include "jsbindings.h"
#include "jseventlistener.h"
#include "jsutils.h"
#include "priv_aamp.h"

#include <stdio.h>
#include <string.h>

extern "C"
{

	void aamp_LoadJSController(JSGlobalContextRef context);

	void aamp_UnloadJSController(JSGlobalContextRef context);

	void setAAMPPlayerInstance(PlayerInstanceAAMP *, int);

	void unsetAAMPPlayerInstance(PlayerInstanceAAMP *);

	void aamp_SetPageHttpHeaders(const char* headers);

	void aamp_ApplyPageHttpHeaders(PlayerInstanceAAMP *);
}
//global variable to hold the custom http headers
static std::map<std::string, std::string> g_PageHttpHeaders;

/**
 * @struct AAMP_JSController
 * @brief Data structure of AAMP_JSController JS object
 */
struct AAMP_JSController : public PrivAAMPStruct_JS
{
	AAMP_JSController() : _aampSessionID(0), _licenseServerUrl()
	{}
	int _aampSessionID;
	std::string _licenseServerUrl;
};

/**
 * @brief Global AAMP_JSController object
 */
AAMP_JSController* _globalController = NULL;


/**
 * @brief Set the instance of PlayerInstanceAAMP and session id
 * @param[in] aamp instance of PlayerInstanceAAMP
 * @param[in] sessionID session id
 */
void setAAMPPlayerInstance(PlayerInstanceAAMP *aamp, int sessionID)
{
        LOG_TRACE("setAAMPPlayerInstance (%p, id=%d)", aamp, sessionID);
	if (_globalController == NULL)
	{
		return;
	}

	_globalController->_aamp = aamp;
	_globalController->_aampSessionID = sessionID;

	if (_globalController->_listeners.size() > 0)
	{
		std::multimap<AAMPEventType, void*>::iterator listenerIter;

		for (listenerIter = _globalController->_listeners.begin(); listenerIter != _globalController->_listeners.end(); listenerIter++)
		{
			AAMP_JSEventListener *listener = (AAMP_JSEventListener *)listenerIter->second;
			_globalController->_aamp->AddEventListener(listenerIter->first, listener);
		}
	}
	if (!_globalController->_licenseServerUrl.empty())
	{
		_globalController->_aamp->SetLicenseServerURL(_globalController->_licenseServerUrl.c_str());
	}
}


/**
 * @brief Remove the PlayerInstanceAAMP stored earlier
 * @param[in] aamp instance of PlayerInstanceAAMP to be removed
 */
void unsetAAMPPlayerInstance(PlayerInstanceAAMP *aamp)
{
        if (_globalController == NULL || _globalController->_aamp != aamp)
        {
                return;
        }

        if (_globalController->_listeners.size() > 0)
        {
		std::multimap<AAMPEventType, void*>::iterator listenerIter;

		for (listenerIter = _globalController->_listeners.begin(); listenerIter != _globalController->_listeners.end(); listenerIter++)
		{
			AAMP_JSEventListener *listener = (AAMP_JSEventListener *)listenerIter->second;
			if (_globalController->_aamp)
			{
				_globalController->_aamp->RemoveEventListener(listenerIter->first, listener);
			}
		}
        }
        _globalController->_aamp = NULL;
}



/**
 * @brief Callback invoked from JS to get the CC enabled property
 * @param[in] context JS execution context
 * @param[in] thisObject JSObject to search for the property
 * @param[in] propertyName JSString containing the name of the property to get
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval property's value if object has the property, otherwise NULL
 */
static JSValueRef AAMPJSC_getProperty_closedCaptionEnabled(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	
        LOG_TRACE("Enter");
	AAMP_JSController* obj = (AAMP_JSController*) JSObjectGetPrivate(thisObject);

	if (obj == NULL)
	{
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP_JSController.closedCaptionEnabled on instances of AAMP_JSController");
		return JSValueMakeUndefined(context);
	}

        LOG_ERROR_EX("AAMP_JSController.closedCaptionEnabled has been deprecated!!");
	return JSValueMakeBoolean(context, false);
}


/**
 * @brief Callback invoked from JS to set the CC enabled property
 * @param[in] context JS exception context
 * @param[in] thisObject JSObject on which to set the property's value
 * @param[in] propertyName JSString containing the name of the property to set
 * @param[in] value JSValue to use as the property's value
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval true if the property was set, otherwise false
 */
static bool AAMPJSC_setProperty_closedCaptionEnabled(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
	
    	LOG_TRACE("Enter");
	AAMP_JSController* obj = (AAMP_JSController*) JSObjectGetPrivate(thisObject);

	if (obj == NULL)
	{
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP_JSController.closedCaptionEnabled on instances of AAMP_JSController");
	}
	else if (!JSValueIsBoolean(context, value))
	{
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP_JSController.closedCaptionEnabled' - value passed is not boolean");

	}

        LOG_ERROR_EX("AAMP_JSController.closedCaptionEnabled has been deprecated!!");
	return false;
}


/**
 * @brief Callback invoked from JS to get the session id property
 * @param[in] context JS execution context
 * @param[in] thisObject JSObject to search for the property
 * @param[in] propertyName JSString containing the name of the property to get
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval property's value if object has the property, otherwise NULL
 */
static JSValueRef AAMPJSC_getProperty_aampSessionID(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
	
        LOG_TRACE("Enter");


	AAMP_JSController *aampObj = (AAMP_JSController *) JSObjectGetPrivate(thisObject);

	if (aampObj == NULL)
	{
		return JSValueMakeUndefined(context);
	}

	if (aampObj->_aampSessionID == 0)
	{
		return JSValueMakeUndefined(context);
	}

	return JSValueMakeNumber(context, aampObj->_aampSessionID);
}


/**
 * @brief Array containing the AAMP_JSController's statically declared value properties
 */
static const JSStaticValue AAMP_JSController_static_values[] =
{
	{"closedCaptionEnabled", AAMPJSC_getProperty_closedCaptionEnabled, AAMPJSC_setProperty_closedCaptionEnabled, kJSPropertyAttributeDontDelete },
	{"aampSessionID", AAMPJSC_getProperty_aampSessionID, NULL, kJSPropertyAttributeDontDelete },
	{NULL, NULL, NULL, 0}
};


/**
 * @brief Callback invoked from JS to add an event listener
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMPJSC_addEventListener(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JSController *aampObj = (AAMP_JSController *)JSObjectGetPrivate(thisObject);

	if (aampObj == NULL)
	{
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP_JSController.addEventListener on instances of AAMP_JSController");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount >= 2)
	{
		char* type = aamp_JSValueToCString(context, arguments[0], NULL);
		JSObjectRef callbackFunc = JSValueToObject(context, arguments[1], NULL);

		if ((callbackFunc != NULL) && JSObjectIsFunction(context, callbackFunc))
		{
			AAMPEventType eventType = aampPlayer_getEventTypeFromName(type);
            		LOG_WARN_EX("eventType='%s', %d", type, eventType);
			if ((eventType >= 0) && (eventType < AAMP_MAX_NUM_EVENTS))
			{
				AAMP_JSEventListener::AddEventListener(aampObj, eventType, callbackFunc);
			}
		}
		else
		{
            		LOG_ERROR_EX("callbackFunc = %p, JSObjectIsFunction(context, callbackFunc) is NULL", callbackFunc);
			char errMsg[512];
			memset(errMsg, '\0', 512);
			snprintf(errMsg, 511, "Failed to execute addEventListener() for event %s - parameter 2 is not a function", type);
			*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, (const char*)errMsg);
		}
		SAFE_DELETE_ARRAY(type);
	}
	else
	{
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP_JSController.addEventListener' - 2 arguments required");
	}

	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback from JS to remove an event listener
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMPJSC_removeEventListener(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JSController *aampObj = (AAMP_JSController *)JSObjectGetPrivate(thisObject);
	JSObjectRef callbackFunc;

	if (aampObj == NULL)
	{
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP_JSController.removeEventListener on instances of AAMP_JSController");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount >= 2)
	{
		char* type = aamp_JSValueToCString(context, arguments[0], NULL);
		JSObjectRef callbackFunc = JSValueToObject(context, arguments[1], NULL);

		if ((callbackFunc != NULL) && JSObjectIsFunction(context, callbackFunc))
		{
			AAMPEventType eventType = aampPlayer_getEventTypeFromName(type);
			LOG_TRACE("eventType='%s', %d",type, eventType);

			if ((eventType >= 0) && (eventType < AAMP_MAX_NUM_EVENTS))
			{
				AAMP_JSEventListener::RemoveEventListener(aampObj, eventType, callbackFunc);
			}
		}
		else
		{
            		LOG_ERROR_EX("callbackFunc = %p, JSObjectIsFunction(context, callbackFunc) is NULL", callbackFunc);
			char errMsg[512];
			memset(errMsg, '\0', 512);
			snprintf(errMsg, 511, "Failed to execute removeEventListener() for event %s - parameter 2 is not a function", type);
			*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, (const char*)errMsg);
		}
		SAFE_DELETE_ARRAY(type);
	}
	else
	{
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP_JSController.removeEventListener' - 2 arguments required");
	}

	return JSValueMakeUndefined(context);
}


/**
 * @brief Callback from JS to set a license server URL
 * @param[in] context JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMPJSC_setLicenseServerUrl(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	LOG_TRACE("Enter");
	AAMP_JSController *aampObj = (AAMP_JSController *)JSObjectGetPrivate(thisObject);

	if (aampObj == NULL)
	{
		*exception = aamp_GetException(context, AAMPJS_MISSING_OBJECT, "Can only call AAMP_JSController.setLicenseServerUrl on instances of AAMP_JSController");
		return JSValueMakeUndefined(context);
	}

	if (argumentCount == 1)
	{
		char *url = aamp_JSValueToCString(context, arguments[0], exception);
		if (strlen(url) > 0)
		{
			aampObj->_licenseServerUrl = std::string(url);
			if (aampObj->_aamp != NULL)
			{
				aampObj->_aamp->SetLicenseServerURL(aampObj->_licenseServerUrl.c_str());
			}
		}
		SAFE_DELETE_ARRAY(url);
	}
	else
	{
		*exception = aamp_GetException(context, AAMPJS_INVALID_ARGUMENT, "Failed to execute 'AAMP_JSController.setLicenseServerURL' - 1 argument required");
	}

	return JSValueMakeUndefined(context);
}


/**
 * @brief Array containing the AAMP_JSController's statically declared functions
 */
static const JSStaticFunction AAMP_JSController_static_methods[] =
{
	{"addEventListener", AAMPJSC_addEventListener, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{"removeEventListener", AAMPJSC_removeEventListener, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{"setLicenseServerUrl", AAMPJSC_setLicenseServerUrl, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{NULL, NULL, 0}
};


/**
 * @brief Callback invoked when an object of AAMP_JSController is finalized
 * @param[in] thisObj JSObject being finalized
 */
void AAMP_JSController_finalize(JSObjectRef thisObj)
{

    	LOG_WARN_EX("AAMP_finalize: object=%p", thisObj);
	AAMP_JSController *aampObj = (AAMP_JSController*) JSObjectGetPrivate(thisObj);

	if (aampObj == NULL)
		return;

	if (aampObj->_ctx != NULL)
	{
		if (aampObj->_listeners.size() > 0)
		{
			AAMP_JSEventListener::RemoveAllEventListener(aampObj);
		}
	}

	JSObjectSetPrivate(thisObj, NULL);

	SAFE_DELETE(aampObj);
}


/**
 * @brief callback invoked when an AAMP_JSController is used along with 'new'
 * @param[in] context JS execution context
 * @param[in] constructor JSObject that is the constructor being called
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSObject that is the constructor's return value
 */
static JSObjectRef AAMP_JSController_class_constructor(JSContextRef context, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	*exception = aamp_GetException(context, AAMPJS_GENERIC_ERROR, "Cannot create an object of AAMP_JSController");
	return NULL;
}

/**
 * @brief Structure contains properties and callbacks of AAMP_JSController object
 */
static const JSClassDefinition AAMP_JSController_class_def =
{
	0,
	kJSClassAttributeNone,
	"__AAMPJSController__class",
	NULL,
	AAMP_JSController_static_values,
	AAMP_JSController_static_methods,
	NULL,
	NULL, // _globalController is reused, so don't clean when one object goes out of scope
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	AAMP_JSController_class_constructor,
	NULL,
	NULL
};

/**
 * @brief Sets the custom http headers from received json string
 * @param[in] headerJson http headers json in string format
 */
void aamp_SetPageHttpHeaders(const char* headerJson)
{
	//headerJson is expected to be in the format "[{\"name\":\"X-PRIVACY-SETTINGS\",\"value\":\"lmt=1,us_privacy=1-Y-\"},    {\"name\":\"abc\",\"value\":\"xyz\"}]"
	g_PageHttpHeaders.clear();
	if(nullptr != headerJson && '\0' != headerJson[0])
	{
        	LOG_WARN_EX("aamp_SetPageHttpHeaders headerJson=%s", headerJson);
		cJSON *parentJsonObj = cJSON_Parse(headerJson);
		cJSON *jsonObj = nullptr;
		if(nullptr != parentJsonObj)
		{
			jsonObj = parentJsonObj->child;
		}
		
		while(nullptr != jsonObj)
		{
			cJSON *child = jsonObj->child;
			std::string key = "";
			std::string val = "";
			while( nullptr != child )
			{
				if(strcmp (child->string,"name") == 0)
				{
					key = std::string(child->valuestring);
				}
				else if(strcmp (child->string,"value") == 0)
				{
					val = std::string(child->valuestring);
				}
				child = child->next;
			}
			if(!key.empty() && key == "X-PRIVACY-SETTINGS")/* RDK-35182: Condition is added to filter out header "X-PRIVACY-SETTINGS" since other headres like X-Forwarded-For/User-Agent/Accept-Language/ are appearing in case of x1 **/
			{
				//insert key value pairs one by one
				g_PageHttpHeaders.insert(std::make_pair(key, val));
			}
			jsonObj = jsonObj->next;
		}
		if(parentJsonObj)
			cJSON_Delete(parentJsonObj);
	}
}
/**
 * @brief applies the parsed custom http headers into the aamp
 * @param[in] aampObject main aamp object
 */
void aamp_ApplyPageHttpHeaders(PlayerInstanceAAMP * aampObject)
{
        LOG_WARN_EX("aamp_ApplyPageHttpHeaders aampObject=%p", aampObject);
	if(NULL != aampObject)
	{
		aampObject->AddPageHeaders(g_PageHttpHeaders);
	}
}

/**
 * @brief Loads AAMP_JSController JS object into JS execution context
 * @param[in] context JS execution context
 */
void aamp_LoadJSController(JSGlobalContextRef context)
{

    	LOG_WARN_EX("aamp_LoadJSController context=%p", context);
	AAMP_JSController* aampObj = new AAMP_JSController();
	aampObj->_ctx = context;
	aampObj->_aampSessionID = 0;
	aampObj->_listeners.clear();
	aampObj->_licenseServerUrl = std::string();

	_globalController = aampObj;

	// DELIA-47534: For this ticket we have repurposed AAMP.version to return the UVE bindings version
	// When at some point in future we deprecate legacy bindings or aamp_LoadJS() please don't forget
	// to retain this support to avoid backward compatibility issues.
	aamp_LoadJS(context, NULL);
	AAMPPlayer_LoadJS(context);

	JSClassRef classDef = JSClassCreate(&AAMP_JSController_class_def);
	JSObjectRef classObj = JSObjectMake(context, classDef, aampObj);
	JSObjectRef globalObj = JSContextGetGlobalObject(context);
	JSStringRef str = JSStringCreateWithUTF8CString("AAMP_JSController");
	JSObjectSetProperty(context, globalObj, str, classObj, kJSPropertyAttributeReadOnly, NULL);
	JSClassRelease(classDef);
	JSStringRelease(str);
}


/**
 * @brief Removes the AAMP_JSController instance from JS context
 * @param[in] context JS execution context
 */
void aamp_UnloadJSController(JSGlobalContextRef context)
{

    	LOG_WARN_EX("aamp_UnloadJSController context=%p", context);

	aamp_UnloadJS(context);
	AAMPPlayer_UnloadJS(context);

	JSObjectRef globalObj = JSContextGetGlobalObject(context);
	JSStringRef str = JSStringCreateWithUTF8CString("AAMP_JSController");
	JSValueRef aamp = JSObjectGetProperty(context, globalObj, str, NULL);

	if (aamp == NULL)
	{
		JSStringRelease(str);
		return;
	}

	JSObjectRef aampObj = JSValueToObject(context, aamp, NULL);
	if (aampObj == NULL || JSObjectGetPrivate(aampObj) == NULL)
	{
		JSStringRelease(str);
		return;
	}

	AAMP_JSController_finalize(aampObj);
	_globalController = NULL;

	// Per comments in DELIA-48964, use JSObjectDeleteProperty instead of JSObjectSetProperty when trying to invalidate a read-only property
	JSObjectDeleteProperty(context, globalObj, str, NULL);
	JSStringRelease(str);

        LOG_TRACE("JSGarbageCollect(%p)", context);

        
	JSGarbageCollect(context);
}
