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
 * @file jsmediaplayer.cpp
 * @brief JavaScript bindings for AAMPMediaPlayer
 */
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
        #include "Module.h"
	#include <core/core.h>
	#include "ThunderAccess.h"
	#include "AampUtils.h"
#endif

#include "jsbindings-version.h"
#include "jsbindings.h"
#include "jsutils.h"
#include "jseventlistener.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef AAMP_CC_ENABLED
#include "AampCCManager.h"
#endif


extern "C"
{
	JS_EXPORT JSGlobalContextRef JSContextGetGlobalContext(JSContextRef);
	void aamp_ApplyPageHttpHeaders(PlayerInstanceAAMP *);
}

/**
 * @struct AAMPMediaPlayer_JS
 * @brief Private data structure of AAMPMediaPlayer JS object
 */
struct AAMPMediaPlayer_JS : public PrivAAMPStruct_JS
{
	/**
	 * @brief Constructor of AAMPMediaPlayer_JS structure
	 */
	AAMPMediaPlayer_JS() : _promiseCallbacks()
	{
	}
	AAMPMediaPlayer_JS(const AAMPMediaPlayer_JS&) = delete;
	AAMPMediaPlayer_JS& operator=(const AAMPMediaPlayer_JS&) = delete;

	static std::vector<AAMPMediaPlayer_JS *> _jsMediaPlayerInstances;
	std::map<std::string, JSObjectRef> _promiseCallbacks;


	/**
	 * @brief Get promise callback for an ad id
	 * @param[in] id ad id
	 */
	JSObjectRef getCallbackForAdId(std::string id) override
	{
		TRACELOG("Enter, id: %s", id);
		std::map<std::string, JSObjectRef>::const_iterator it = _promiseCallbacks.find(id);
		if (it != _promiseCallbacks.end())
		{
			WARNING("found cbObject");
			return it->second;
		}
		else
		{
			ERROR("didn't find cbObject for id: %s", id);
			return NULL;
		}
	}


	/**
	 * @brief Get promise callback for an ad id
	 * @param[in] id ad id
	 */
	void removeCallbackForAdId(std::string id) override
	{
		TRACELOG("Enter");
		std::map<std::string, JSObjectRef>::const_iterator it = _promiseCallbacks.find(id);
		if (it != _promiseCallbacks.end())
		{
			JSValueUnprotect(_ctx, it->second);
			_promiseCallbacks.erase(it);
		}
		TRACELOG("Exit");
	}


	/**
	 * @brief Save promise callback for an ad id
	 * @param[in] id ad id
	 * @param[in] cbObject promise callback object
	 */
	void saveCallbackForAdId(std::string id, JSObjectRef cbObject)
	{
		TRACELOG("Enter");
		JSObjectRef savedObject = getCallbackForAdId(id);
		if (savedObject != NULL)
		{
			JSValueUnprotect(_ctx, savedObject); //remove already saved callback
		}

		JSValueProtect(_ctx, cbObject);
		_promiseCallbacks[id] = cbObject;
		TRACELOG("Exit");
	}


	/**
	 * @brief Clear all saved promise callbacks
	 */
	void clearCallbackForAllAdIds()
	{
		TRACELOG("Enter");
		if (!_promiseCallbacks.empty())
		{
			for (auto it = _promiseCallbacks.begin(); it != _promiseCallbacks.end(); )
			{
				JSValueUnprotect(_ctx, it->second);
				_promiseCallbacks.erase(it);
			}
		}
		TRACELOG("Exit");
	}
};

/**
 * @enum ConfigParamType
 */
enum ConfigParamType
{
	ePARAM_RELOCKONTIMEOUT,
	ePARAM_RELOCKONPROGRAMCHANGE,
	ePARAM_MAX_COUNT
};

/**
 * @struct ConfigParamMap
 * @brief Data structure to map ConfigParamType and its string equivalent
 */
struct ConfigParamMap
{
	ConfigParamType paramType;
	const char* paramName;
};

/**
 * @brief Map relockConditionParamNames and its string equivalent
 */
static ConfigParamMap relockConditionParamNames[] =
{
	{ ePARAM_RELOCKONTIMEOUT, "time" },
	{ ePARAM_RELOCKONPROGRAMCHANGE, "programChange" },
	{ ePARAM_MAX_COUNT, "" }
};

std::vector<AAMPMediaPlayer_JS *> AAMPMediaPlayer_JS::_jsMediaPlayerInstances = std::vector<AAMPMediaPlayer_JS *>();

/**
 * @brief Mutex for global cache of AAMPMediaPlayer_JS instances
 */
static pthread_mutex_t jsMediaPlayerCacheMutex = PTHREAD_MUTEX_INITIALIZER;


/**
 * @brief Helper function to parse a JS property value as number
 * @param[in] ctx JS execution context
 * @param[in] jsObject JS object whose property has to be parsed
 * @param[in] prop property name
 * @param[out] value to store parsed number
 * return true if value was parsed sucessfully, false otherwise
 */
bool ParseJSPropAsNumber(JSContextRef ctx, JSObjectRef jsObject, const char *prop, double &value)
{
	bool ret = false;
	JSStringRef propName = JSStringCreateWithUTF8CString(prop);
	JSValueRef propValue = JSObjectGetProperty(ctx, jsObject, propName, NULL);
	if (JSValueIsNumber(ctx, propValue))
	{
		value = JSValueToNumber(ctx, propValue, NULL);
		INFO("Parsed value for property %s - %f", prop, value);
		ret = true;
	}
	else
	{
		WARNING("Invalid value for property %s passed", prop);
	}

	JSStringRelease(propName);
	return ret;
}


/**
 * @brief Helper function to parse a JS property value as string
 * @param[in] ctx JS execution context
 * @param[in] jsObject JS object whose property has to be parsed
 * @param[in] prop property name
 * @param[out] value to store parsed string
 * return true if value was parsed sucessfully, false otherwise
 */
bool ParseJSPropAsString(JSContextRef ctx, JSObjectRef jsObject, const char *prop, char * &value)
{
	bool ret = false;
	JSStringRef propName = JSStringCreateWithUTF8CString(prop);
	JSValueRef propValue = JSObjectGetProperty(ctx, jsObject, propName, NULL);
	if (JSValueIsString(ctx, propValue))
	{
		value = aamp_JSValueToCString(ctx, propValue, NULL);
		INFO("Parsed value for property %s - %s", prop, value);
		ret = true;
	}
	else
	{
		WARNING("Invalid value for property - %s passed", prop);
	}

	JSStringRelease(propName);
	return ret;
}


/**
 * @brief Helper function to parse a JS property value as object
 * @param[in] ctx JS execution context
 * @param[in] jsObject JS object whose property has to be parsed
 * @param[in] prop property name
 * @param[out] value to store parsed value
 * return true if value was parsed sucessfully, false otherwise
 */
bool ParseJSPropAsObject(JSContextRef ctx, JSObjectRef jsObject, const char *prop, JSValueRef &value)
{
	bool ret = false;
	JSStringRef propName = JSStringCreateWithUTF8CString(prop);
	JSValueRef propValue = JSObjectGetProperty(ctx, jsObject, propName, NULL);
	if (JSValueIsObject(ctx, propValue))
	{
		value = propValue;
		INFO("Parsed object as value for property %s", prop);
		ret = true;
	}
	else
	{
		WARNING("Invalid value for property - %s passed", prop);
	}

	JSStringRelease(propName);
	return ret;
}

/**
 * @brief Helper function to parse a JS property value as boolean
 * @param[in] ctx JS execution context
 * @param[in] jsObject JS object whose property has to be parsed
 * @param[in] prop property name
 * @param[out] value to store parsed value
 * return true if value was parsed sucessfully, false otherwise
 */
bool ParseJSPropAsBoolean(JSContextRef ctx, JSObjectRef jsObject, const char *prop, bool &value)
{
       bool ret = false;
       JSStringRef propName = JSStringCreateWithUTF8CString(prop);
       JSValueRef propValue = JSObjectGetProperty(ctx, jsObject, propName, NULL);
       if (JSValueIsBoolean(ctx, propValue))
       {
               value = JSValueToBoolean(ctx, propValue);
               INFO("Parsed value for property %s - %d", prop, value);
               ret = true;
       }
       else
       {
               WARNING("Invalid value for property - %s passed", prop);
       }

       JSStringRelease(propName);
       return ret;
}

/**
 * @brief API to release internal resources of an AAMPMediaPlayerJS object
 *  NOTE that this function does NOT free AAMPMediaPlayer_JS
 *  It is done in AAMPMediaPlayerJS_release ( APP initiated )  or AAMPMediaPlayer_JS_finalize ( GC initiated)
 * @param[in] object AAMPMediaPlayerJS object being released
 */
static void releaseNativeResources(AAMPMediaPlayer_JS *privObj)
{
	if (privObj != NULL)
	{
		INFO("Deleting AAMPMediaPlayer_JS instance:%p ", privObj);
		// clean all members of AAMPMediaPlayer_JS(privObj)
		if (privObj->_aamp != NULL)
		{
			//when finalizing JS object, don't generate state change events
			WARNING("Calling _aamp->Stop(false)");
			privObj->_aamp->Stop(false);
			privObj->clearCallbackForAllAdIds();
			if (privObj->_listeners.size() > 0)
			{
				AAMP_JSEventListener::RemoveAllEventListener(privObj);
			}
			WARNING("Deleting PlayerInstanceAAMP instance:%p", privObj->_aamp);
			SAFE_DELETE(privObj->_aamp);
		}
	}
}


/**
 * @brief API to check if AAMPMediaPlayer_JS object present in cache and release it
 *
 * @param[in] object AAMPMediaPlayerJS object
 * @return true if instance was found and native resources released, false otherwise
 */
static bool findInGlobalCacheAndRelease(AAMPMediaPlayer_JS *privObj)
{
	bool found = false;

	if (privObj != NULL)
	{
		pthread_mutex_lock(&jsMediaPlayerCacheMutex);
		for (std::vector<AAMPMediaPlayer_JS *>::iterator iter = AAMPMediaPlayer_JS::_jsMediaPlayerInstances.begin(); iter != AAMPMediaPlayer_JS::_jsMediaPlayerInstances.end(); iter++)
		{
			if (privObj == *iter)
			{
				//Remove this instance from global cache
				AAMPMediaPlayer_JS::_jsMediaPlayerInstances.erase(iter);
				found = true;
				break;
			}
		}
		pthread_mutex_unlock(&jsMediaPlayerCacheMutex);

		if (found)
		{
			//Release private resources
			releaseNativeResources(privObj);
		}
	}

	return found;
}


/**
 * @brief Helper function to parse DRM config params received from JS
 * @param[in] ctx JS execution context
 * @param[in] privObj AAMPMediaPlayer instance to set the drm configuration
 * @param[in] drmConfigParam parameters received as argument
 */
void parseDRMConfiguration (JSContextRef ctx, AAMPMediaPlayer_JS* privObj, JSValueRef drmConfigParam)
{
	JSValueRef exception = NULL;
	JSObjectRef drmConfigObj = JSValueToObject(ctx, drmConfigParam, &exception);

	if (drmConfigObj != NULL && exception == NULL)
	{
		char *prLicenseServerURL = NULL;
		char *wvLicenseServerURL = NULL;
		char *ckLicenseServerURL = NULL;
		char *keySystem = NULL;
		bool ret = false;
		ret = ParseJSPropAsString(ctx, drmConfigObj, "com.microsoft.playready", prLicenseServerURL);
		if (ret)
		{
			WARNING("Playready License Server URL config param received - %s", prLicenseServerURL);
			privObj->_aamp->SetLicenseServerURL(prLicenseServerURL, eDRM_PlayReady);

			SAFE_DELETE_ARRAY(prLicenseServerURL);
		}

		ret = ParseJSPropAsString(ctx, drmConfigObj, "com.widevine.alpha", wvLicenseServerURL);
		if (ret)
		{
			WARNING("Widevine License Server URL config param received - %s", wvLicenseServerURL);
			privObj->_aamp->SetLicenseServerURL(wvLicenseServerURL, eDRM_WideVine);

			SAFE_DELETE_ARRAY(wvLicenseServerURL);
		}

		ret = ParseJSPropAsString(ctx, drmConfigObj, "org.w3.clearkey", ckLicenseServerURL);
		if (ret)
		{
			WARNING("ClearKey License Server URL config param received - %s", ckLicenseServerURL);
			privObj->_aamp->SetLicenseServerURL(ckLicenseServerURL, eDRM_ClearKey);

			SAFE_DELETE_ARRAY(ckLicenseServerURL);
		}

		ret = ParseJSPropAsString(ctx, drmConfigObj, "preferredKeysystem", keySystem);
		if (ret)
		{
			if (strncmp(keySystem, "com.microsoft.playready", 23) == 0)
			{
				WARNING("Preferred key system config received - playready");
				privObj->_aamp->SetPreferredDRM(eDRM_PlayReady);
			}
			else if (strncmp(keySystem, "com.widevine.alpha", 18) == 0)
			{
				WARNING("Preferred key system config received - widevine");
				privObj->_aamp->SetPreferredDRM(eDRM_WideVine);
			}
			else
			{
				ERROR("Value passed preferredKeySystem(%s) not supported", keySystem);
			}
			SAFE_DELETE_ARRAY(keySystem);
		}
	}
	else
	{
		ERROR("InvalidProperty - drmConfigParam is NULL");
	}
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.load()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_load (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call load() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	bool autoPlay = true;
	bool bFinalAttempt = false;
	bool bFirstAttempt = true;
	char* url = NULL;
	char* contentType = NULL;
	char* strTraceId = NULL;

	switch(argumentCount)
	{
		case 3:
		{
			JSObjectRef argument = JSValueToObject(ctx, arguments[2], NULL);
			JSStringRef paramName = JSStringCreateWithUTF8CString("contentType");
			JSValueRef paramValue = JSObjectGetProperty(ctx, argument, paramName, NULL);
			if (JSValueIsString(ctx, paramValue))
			{
				contentType = aamp_JSValueToCString(ctx, paramValue, NULL);
			}
			JSStringRelease(paramName);

			paramName = JSStringCreateWithUTF8CString("traceId");
			paramValue = JSObjectGetProperty(ctx, argument, paramName, NULL);
			if (JSValueIsString(ctx, paramValue))
			{
				strTraceId = aamp_JSValueToCString(ctx, paramValue, NULL);
			}
			JSStringRelease(paramName);

			paramName = JSStringCreateWithUTF8CString("isInitialAttempt");
			paramValue = JSObjectGetProperty(ctx, argument, paramName, NULL);
			if (JSValueIsBoolean(ctx, paramValue))
			{
				bFirstAttempt = JSValueToBoolean(ctx, paramValue);
			}
			JSStringRelease(paramName);

			paramName = JSStringCreateWithUTF8CString("isFinalAttempt");
			paramValue = JSObjectGetProperty(ctx, argument, paramName, NULL);
			if (JSValueIsBoolean(ctx, paramValue))
			{
				bFinalAttempt = JSValueToBoolean(ctx, paramValue);
			}
			JSStringRelease(paramName);
		}
		case 2:
			autoPlay = JSValueToBoolean(ctx, arguments[1]);
		case 1:
		{
			url = aamp_JSValueToCString(ctx, arguments[0], exception);
			aamp_ApplyPageHttpHeaders(privObj->_aamp);
			{
				char* url = aamp_JSValueToCString(ctx, arguments[0], exception);
				WARNING("Calling _aamp->Tune(%s, %d, %s, %d, %d, %s)", 
					url, autoPlay, contentType, bFirstAttempt, bFinalAttempt,strTraceId);
				privObj->_aamp->Tune(url, autoPlay, contentType, bFirstAttempt, bFinalAttempt,strTraceId);

			}

			break;
		}

		default:
			ERROR("InvalidArgument - argumentCount=%d, expected atmost 3 arguments", argumentCount);
			*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute load() <= 3 arguments required");
	}

	SAFE_DELETE_ARRAY(url);
	SAFE_DELETE_ARRAY(contentType);
	SAFE_DELETE_ARRAY(strTraceId);

	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.initConfig()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_initConfig (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call initConfig() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}


	if (argumentCount == 1 && JSValueIsObject(ctx, arguments[0]))
	{
		JSValueRef _exception = NULL;
		bool ret = false;
		bool valueAsBoolean = false;
		double valueAsNumber = 0;
		char *valueAsString = NULL;
		JSValueRef valueAsObject = NULL;
		int langCodePreference = -1; // value not passed
		bool useRole = false; //default value in func arg
		int enableVideoRectangle = -1; //-1: value not passed, 0: false, 1:true

		bool jsonparsingdone=false;
		char *jsonStr = aamp_JSValueToJSONCString(ctx,arguments[0], exception);
		if (jsonStr != NULL)
		{
			WARNING("Calling _aaamp->InitConfig: %s", jsonStr);
			if(privObj->_aamp->InitAAMPConfig(jsonStr))
			{
				jsonparsingdone=true;
			}
			SAFE_DELETE_ARRAY(jsonStr);
		}
		else
		{
			ERROR("InitConfig Failed to create JSON String");
		}
		if(!jsonparsingdone)
		{
			ERROR("InitConfig Failed to parse JSON String");
		}
	}
	else
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute initConfig() - 1 argument of type IConfig required");
	}
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.play()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_play (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call play() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	{
		WARNING("Calling _aamp->SetRate(1)");
		privObj->_aamp->SetRate(AAMP_NORMAL_PLAY_RATE);
	}
	return JSValueMakeUndefined(ctx);
}

/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.detach()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_detach (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call play() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	WARNING("Calling _aamp->detach()");
	privObj->_aamp->detach();

	return JSValueMakeUndefined(ctx);
}

/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.pause()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_pause (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call pause() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	{
		WARNING("Calling _aamp->SetRate(0)");
		privObj->_aamp->SetRate(0);
	}
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.stop()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_stop (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || (privObj && !privObj->_aamp))
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call stop() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	WARNING("Calling _aamp->Stop()");
	privObj->_aamp->Stop();
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.seek()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_seek (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call seek() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount == 1 || argumentCount == 2)
	{
		double newSeekPos = JSValueToNumber(ctx, arguments[0], exception);
		bool keepPaused = (argumentCount == 2)? JSValueToBoolean(ctx, arguments[1]) : false;

		{
			WARNING("Calling _aamp->Seek(%f, %d)", newSeekPos, keepPaused);
			privObj->_aamp->Seek(newSeekPos, keepPaused);
		}
	}
	else
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1 or 2", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute seek() - 1 or 2 arguments required");
	}
	return JSValueMakeUndefined(ctx);
}

/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.GetThumbnails()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_getThumbnails (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call seek() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount == 1)
	{
		double thumbnailPosition = JSValueToNumber(ctx, arguments[0], exception);

		WARNING("Calling _aamp->GetThumbnails(%d, 0)", thumbnailPosition);
		std::string value = privObj->_aamp->GetThumbnails(thumbnailPosition, 0);
	        if (!value.empty())
        	{
				TRACELOG("Exit");
				return aamp_CStringToJSValue(ctx, value.c_str());
	        }

	}
	else if (argumentCount == 2)
	{
		double startPos = JSValueToNumber(ctx, arguments[0], exception);
		double endPos = JSValueToNumber(ctx, arguments[1], exception);
		WARNING("Calling _aamp->GetThumbnails(%d, %d)", startPos, endPos);
		std::string value = privObj->_aamp->GetThumbnails(startPos, endPos);
		if (!value.empty())
		{
			ERROR("Exit");
			return aamp_CStringToJSValue(ctx, value.c_str());
		}
	}
	else
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1 or 2", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute seek() - 1 or 2 arguments required");
	}
	return JSValueMakeUndefined(ctx);
}

/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.getAvailableThumbnailTracks()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_getAvailableThumbnailTracks (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getVideoBitrates() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	WARNING("Calling _aamp->GetAvailableThumbnailTrack()");
	std::string value = privObj->_aamp->GetAvailableThumbnailTracks();
	if (!value.empty())
	{
		ERROR("Exit");
		return aamp_CStringToJSValue(ctx, value.c_str());
	}
	return JSValueMakeUndefined(ctx);
}

/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.getAudioTrackInfo()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_getAudioTrackInfo (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getAudioTrackInfo() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	WARNING("Calling _aamp->GetAudioTrackInfo()");
	std::string value = privObj->_aamp->GetAudioTrackInfo();
	if (!value.empty())
	{
		ERROR("Exit");
		return aamp_CStringToJSValue(ctx, value.c_str());
	}
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.getPreferredAudioProperties()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_getPreferredAudioProperties (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getPreferredAudioProperties() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	WARNING("Calling _aamp->GetPrefferedAudioProperties()");
	std::string value = privObj->_aamp->GetPreferredAudioProperties();
	if (!value.empty())
	{
		TRACELOG("Exit %s", value);
		return aamp_CStringToJSValue(ctx, value.c_str());
	}
	return JSValueMakeUndefined(ctx);
}
/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.setThumbnailTrack()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_setThumbnailTrack (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call SetThumbnailTrack() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount != 1)
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute SetThumbnailTrack() - 1 argument required");
	}
	else
	{
		int thumbnailIndex = (int) JSValueToNumber(ctx, arguments[0], NULL);
		if (thumbnailIndex >= 0)
		{
			WARNING("Calling _aamp->SetThumbnailTrack(%d)", thumbnailIndex);
			return JSValueMakeBoolean(ctx, privObj->_aamp->SetThumbnailTrack(thumbnailIndex));
		}
		else
		{
			ERROR("InvalidArgument - Index should be >= 0!");
			*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Index should be >= 0!");
		}
	}
	return JSValueMakeUndefined(ctx);
}

/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.getCurrentState()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_getCurrentState (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getCurrentState() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	WARNING("Calling _aamp->GetState()");
	return JSValueMakeNumber(ctx, privObj->_aamp->GetState());
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.getDurationSec()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_getDurationSec (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	double duration = 0;
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getDurationSec() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	WARNING("Calling _aamp->GetPlaybackDuration()");
	duration = privObj->_aamp->GetPlaybackDuration();
	if (duration < 0)
	{
		ERROR("Duration returned by GetPlaybackDuration() is less than 0!");
		duration = 0;
	}

	return JSValueMakeNumber(ctx, duration);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.getCurrentPosition()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_getCurrentPosition (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	double currPosition = 0;
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getCurrentPosition() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	WARNING("Calling _aamp->GetPlaybackPosition()");
	currPosition = privObj->_aamp->GetPlaybackPosition();
	if (currPosition < 0)
	{
		ERROR("Current position returned by GetPlaybackPosition() is less than 0!");
		currPosition = 0;
	}

	return JSValueMakeNumber(ctx, currPosition);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.getVideoBitrates()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_getVideoBitrates (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getVideoBitrates() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	WARNING("Calling _aamp->GetVideoBitrates()");
	std::vector<long> bitrates = privObj->_aamp->GetVideoBitrates();
	if (!bitrates.empty())
	{
		unsigned int length = bitrates.size();
		JSValueRef* array = new JSValueRef[length];
		for (int i = 0; i < length; i++)
		{
			array[i] = JSValueMakeNumber(ctx, bitrates[i]);
		}

		JSValueRef retVal = JSObjectMakeArray(ctx, length, array, NULL);
		SAFE_DELETE_ARRAY(array);
		TRACELOG("Exit");
		return retVal;
	}
	return JSValueMakeUndefined(ctx);
}

/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.getManifest()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_getManifest (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getManifest() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	WARNING("Calling _aamp->GetManifest()");
	std::string manifest = privObj->_aamp->GetManifest();
	if (!manifest.empty())
	{
		TRACELOG("Exit");
		return aamp_CStringToJSValue(ctx, manifest.c_str());
	}
	else
	{
		TRACELOG("Exit with undefined");
		return JSValueMakeUndefined(ctx);
	}
}

/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.getAudioBitrates()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_getAudioBitrates (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getAudioBitrates() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	WARNING("Calling _aamp->GetAudioBitrates()");
	std::vector<long> bitrates = privObj->_aamp->GetAudioBitrates();
	if (!bitrates.empty())
	{
		unsigned int length = bitrates.size();
		JSValueRef* array = new JSValueRef[length];
		for (int i = 0; i < length; i++)
		{
			array[i] = JSValueMakeNumber(ctx, bitrates[i]);
		}

		JSValueRef retVal = JSObjectMakeArray(ctx, length, array, NULL);
		SAFE_DELETE_ARRAY(array);
		TRACELOG("Exit");
		return retVal;
	}
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.getCurrentVideoBitrate()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_getCurrentVideoBitrate (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getCurrentVideoBitrate() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	WARNING("Calling _aamp->GetVideoBitrate()");
	return JSValueMakeNumber(ctx, privObj->_aamp->GetVideoBitrate());
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.setVideoBitrate()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_setVideoBitrate (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setVideoBitrate() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount != 1)
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setVideoBitrate() - 1 argument required");
	}
	else
	{
		long bitrate = (long) JSValueToNumber(ctx, arguments[0], NULL);
		//bitrate 0 is for ABR
		if (bitrate >= 0)
		{
			WARNING("Calling _aamp->SetVideoBitrate(%ld)", bitrate);
			privObj->_aamp->SetVideoBitrate(bitrate);
		}
		else
		{
			ERROR("InvalidArgument - bitrate should be >= 0!");
			*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Bitrate should be >= 0!");
		}
	}
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.getCurrentAudioBitrate()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_getCurrentAudioBitrate (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getCurrentAudioBitrate() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	WARNING("Calling _aamp->GetAudioBitrate()");
	return JSValueMakeNumber(ctx, privObj->_aamp->GetAudioBitrate());
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.setAudioBitrate()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_setAudioBitrate (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setAudioBitrate() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount != 1)
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setAudioBitrate() - 1 argument required");
	}
	else
	{
		long bitrate = (long) JSValueToNumber(ctx, arguments[0], NULL);
		if (bitrate >= 0)
		{
			WARNING("Calling _aamp->SetAudioBitrate(%ld)", bitrate);
			privObj->_aamp->SetAudioBitrate(bitrate);
		}
		else
		{
			ERROR("InvalidArgument - bitrate should be >= 0!");
			*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Bitrate should be >= 0!");
		}
	}
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.getAudioTrack()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_getAudioTrack (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getAudioTrack() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	WARNING("Calling _aamp->GetAudioTrack()");
	return JSValueMakeNumber(ctx, privObj->_aamp->GetAudioTrack());
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.setAudioTrack()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_setAudioTrack (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setAudioTrack() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount != 1)
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setAudioTrack() - 1 argument required");
	}
	else  if (JSValueIsObject(ctx, arguments[0]))
	{
	/*
		 * Parmater format
		 * "audioTuple": object {
		 *    "language": "en",
		 *    "rendition": "main"
		 *    "codec": "avc", 
		 *    "channel": 6  
		 * }
		 */
		char *language = NULL;
		int channel = 0;
		char *rendition = NULL;
		char *codec = NULL;
		char *type = NULL;
		//Parse the ad object
		JSObjectRef audioProperty = JSValueToObject(ctx, arguments[0], NULL);
		if (audioProperty == NULL)
		{
			ERROR("Unable to convert argument to JSObject");
			return JSValueMakeUndefined(ctx);
		}
		JSStringRef propName = JSStringCreateWithUTF8CString("language");
		JSValueRef propValue = JSObjectGetProperty(ctx, audioProperty, propName, NULL);
		if (JSValueIsString(ctx, propValue))
		{
			language = aamp_JSValueToCString(ctx, propValue, NULL);
		}
		JSStringRelease(propName);
		
		propName = JSStringCreateWithUTF8CString("rendition");
		propValue = JSObjectGetProperty(ctx, audioProperty, propName, NULL);
		if (JSValueIsString(ctx, propValue))
		{
			rendition = aamp_JSValueToCString(ctx, propValue, NULL);
		}
		JSStringRelease(propName);
		
		propName = JSStringCreateWithUTF8CString("codec");
		propValue = JSObjectGetProperty(ctx, audioProperty, propName, NULL);
		if (JSValueIsString(ctx, propValue))
		{
			codec = aamp_JSValueToCString(ctx, propValue, NULL);
		}
		JSStringRelease(propName);

		propName = JSStringCreateWithUTF8CString("type");
		propValue = JSObjectGetProperty(ctx, audioProperty, propName, NULL);
		if (JSValueIsString(ctx, propValue))
		{
			type = aamp_JSValueToCString(ctx, propValue, NULL);
		}
		JSStringRelease(propName);

		propName = JSStringCreateWithUTF8CString("channel");
		propValue = JSObjectGetProperty(ctx, audioProperty, propName, NULL);
		if (JSValueIsNumber(ctx, propValue))
		{
			channel = JSValueToNumber(ctx, propValue, NULL);
		}
		JSStringRelease(propName);

		std::string strLanguage =  language?std::string(language):"";
		SAFE_DELETE_ARRAY(language);
		std::string strRendition = rendition?std::string(rendition):"";
		SAFE_DELETE_ARRAY(rendition);
		std::string strCodec = codec?std::string(codec):"";
		SAFE_DELETE_ARRAY(codec);
		std::string strType = type?std::string(type):"";
		SAFE_DELETE_ARRAY(type);

		WARNING("Calling _aamp->SetAudioTrack language=%s renditio=%s type=%s codec=%s channel=%d", 
			strLanguage.c_str(), strRendition.c_str(), strType.c_str(), strCodec.c_str(), channel);
		privObj->_aamp->SetAudioTrack(strLanguage, strRendition, strType, strCodec, channel);
		
	}
	else
	{
		int index = (int) JSValueToNumber(ctx, arguments[0], NULL);
		if (index >= 0)
		{
			{
				WARNING("Calling _aamp->SetAudioTrack(%d)", index);
				privObj->_aamp->SetAudioTrack(index);
			}
		}
		else
		{
			ERROR("InvalidArgument - track index should be >= 0!");
			*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Audio track index should be >= 0!");
		}
	}
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.getTextTrack()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_getTextTrack (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getTextTrack() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	WARNING("Calling _aamp->GetTextTrack()");
	return JSValueMakeNumber(ctx, privObj->_aamp->GetTextTrack());
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.setTextTrack()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_setTextTrack (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setTextTrack() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount != 1)
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setTextTrack() - 1 argument required");
	}
	else
	{
		int index = (int) JSValueToNumber(ctx, arguments[0], NULL);
		if (index >= 0)
		{
			WARNING("Calling _aamp->SetTextTrack(%d)");
			privObj->_aamp->SetTextTrack(index);
		}
		else
		{
			ERROR("InvalidArgument - track index should be >= 0!");
			*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Text track index should be >= 0!");
		}
	}
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.getVolume()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_getVolume (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getVolume() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	WARNING("Calling _aamp->GetAudioVolume()");
	return JSValueMakeNumber(ctx, privObj->_aamp->GetAudioVolume());
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.setVolume()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_setVolume (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setVolume() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	
	if(!privObj->_aamp)
	{
		ERROR("PlayerInstanceAAMP returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setVolume() on valid instance of PlayerInstanceAAMP");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount == 1)
	{
		int volume = (int) JSValueToNumber(ctx, arguments[0], exception);
		if (volume >= 0)
		{
			WARNING("Calling _aamp->SetAudioVolume(%d)", volume);
			privObj->_aamp->SetAudioVolume(volume);
		}
		else
		{
			ERROR("InvalidArgument - volume should not be a negative number");
			*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Volume should not be a negative number");
		}
	}
	else
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setVolume() - 1 argument required");
	}
	return JSValueMakeUndefined(ctx);
}

/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.setAudioLanguage()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_setAudioLanguage (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setAudioLanguage() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount == 1)
	{
		const char *lang = aamp_JSValueToCString(ctx, arguments[0], exception);
		{
			WARNING("Calling _aamp->SetLanguage(%s)", lang);
			privObj->_aamp->SetLanguage(lang);
		}
		SAFE_DELETE_ARRAY(lang);
	}
	else
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setAudioLanguage() - 1 argument required");
	}
	return JSValueMakeUndefined(ctx);
}

/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.getPlaybackRate()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_getPlaybackRate (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getPlaybackRate() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	WARNING("Calling _aamp->GetPlaybackRate()");
	return JSValueMakeNumber(ctx, privObj->_aamp->GetPlaybackRate());
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.setPlaybackRate()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_setPlaybackRate (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setPlaybackRate() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount == 1 || argumentCount == 2)
	{
		int overshootCorrection = 0;
		int rate = (int) JSValueToNumber(ctx, arguments[0], exception);
		if (argumentCount == 2)
		{
			overshootCorrection = (int) JSValueToNumber(ctx, arguments[1], exception);
		}
		{
			WARNING("Calling _aamp->SetRate(%d, %d)", rate, overshootCorrection);
			privObj->_aamp->SetRate(rate, overshootCorrection);
		}
	}
	else
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setPlaybackRate() - atleast 1 argument required");
	}
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.getSupportedKeySystems()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_getSupportedKeySystems (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getSupportedKeySystems() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	ERROR("Invoked getSupportedKeySystems");
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.setVideoMute()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_setVideoMute (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setVideoMute() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount == 1)
	{
		bool videoMute = JSValueToBoolean(ctx, arguments[0]);
		WARNING("Calling _aamp->SetVideoMute(%d)", videoMute);
		privObj->_aamp->SetVideoMute(videoMute);
	}
	else
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setVideoMute() - 1 argument required");
	}
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.setSubscribedTags()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_setSubscribedTags (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setSubscribedTags() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount != 1)
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setSubscribedTags() - 1 argument required");
	}
	else if (!aamp_JSValueIsArray(ctx, arguments[0]))
	{
		ERROR("InvalidArgument: aamp_JSValueIsArray=%d", aamp_JSValueIsArray(ctx, arguments[0]));
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setSubscribeTags() - parameter 1 is not an array");
	}
	else
	{
		std::vector<std::string> subscribedTags = aamp_StringArrayToCStringArray(ctx, arguments[0]);
		privObj->_aamp->SetSubscribedTags(subscribedTags);
		ERROR("Invoked setSubscribedTags");
	}
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.subscribeResponseHeaders()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_subscribeResponseHeaders (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call subscribeResponseHeaders() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount != 1)
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute subscribeResponseHeaders() - 1 argument required");
	}
	else if (!aamp_JSValueIsArray(ctx, arguments[0]))
	{
		ERROR("InvalidArgument: aamp_JSValueIsArray=%d", aamp_JSValueIsArray(ctx, arguments[0]));
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute subscribeResponseHeaders() - parameter 1 is not an array");
	}
	else
	{
		std::vector<std::string> responseHeaders = aamp_StringArrayToCStringArray(ctx, arguments[0]);
		WARNING("Calling _aamp->SubscribeResponseHeaders");
		privObj->_aamp->SubscribeResponseHeaders(responseHeaders);
		
	}
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.addEventListener()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_addEventListener (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call addEventListener() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount >= 2)
	{
		char* type = aamp_JSValueToCString(ctx, arguments[0], NULL);
		JSObjectRef callbackObj = JSValueToObject(ctx, arguments[1], NULL);

		if ((callbackObj != NULL) && JSObjectIsFunction(ctx, callbackObj))
		{
			AAMPEventType eventType = aampPlayer_getEventTypeFromName(type);
			WARNING("eventType='%s', %d", type, eventType);

			if ((eventType >= 0) && (eventType < AAMP_MAX_NUM_EVENTS))
			{
				AAMP_JSEventListener::AddEventListener(privObj, eventType, callbackObj);
			}
		}
		else
		{
			ERROR("callbackObj=%p, JSObjectIsFunction(context, callbackObj) is NULL", callbackObj);
			char errMsg[512];
			memset(errMsg, '\0', 512);
			snprintf(errMsg, 511, "Failed to execute addEventListener() for event %s - parameter 2 is not a function", type);
			*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, (const char*)errMsg);
		}
		SAFE_DELETE_ARRAY(type);
	}
	else
	{
		ERROR("InvalidArgument: argumentCount=%d, expected: 2", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute addEventListener() - 2 arguments required.");
	}
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.removeEventListener()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_removeEventListener (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call removeEventListener() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount >= 2)
	{
		char* type = aamp_JSValueToCString(ctx, arguments[0], NULL);
		JSObjectRef callbackObj = JSValueToObject(ctx, arguments[1], NULL);

		if ((callbackObj != NULL) && JSObjectIsFunction(ctx, callbackObj))
		{
			AAMPEventType eventType = aampPlayer_getEventTypeFromName(type);
			WARNING("eventType='%s', %d", type, eventType);

			if ((eventType >= 0) && (eventType < AAMP_MAX_NUM_EVENTS))
			{
				AAMP_JSEventListener::RemoveEventListener(privObj, eventType, callbackObj);
			}
		}
		else
		{
			ERROR("InvalidArgument: callbackObj=%p, JSObjectIsFunction(context, callbackObj) is NULL", callbackObj);
			char errMsg[512];
			memset(errMsg, '\0', 512);
			snprintf(errMsg, 511, "Failed to execute removeEventListener() for event %s - parameter 2 is not a function", type);
			*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, (const char*)errMsg);
		}
		SAFE_DELETE_ARRAY(type);
	}
	else
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute removeEventListener() - 1 argument required");
	}
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.setDRMConfig()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_setDRMConfig (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setDrmConfig() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount != 1)
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setDrmConfig() - 1 argument required");
	}
	else
	{
		if (JSValueIsObject(ctx, arguments[0]))
		{
			parseDRMConfiguration(ctx, privObj, arguments[0]);
		}
	}
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.addCustomHTTPHeader()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_addCustomHTTPHeader (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call addCustomHTTPHeader() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	//optional parameter 3 for identifying if the header is for a license request
	if (argumentCount == 2 || argumentCount == 3)
	{
		char *name = aamp_JSValueToCString(ctx, arguments[0], exception);
		std::string headerName(name);
		std::vector<std::string> headerVal;
		bool isLicenseHeader = false;

		SAFE_DELETE_ARRAY(name);

		if (aamp_JSValueIsArray(ctx, arguments[1]))
		{
			headerVal = aamp_StringArrayToCStringArray(ctx, arguments[1]);
		}
		else if (JSValueIsString(ctx, arguments[1]))
		{
			headerVal.reserve(1);
			char *value =  aamp_JSValueToCString(ctx, arguments[1], exception);
			headerVal.push_back(value);
			SAFE_DELETE_ARRAY(value);
		}

		// Don't support empty values now
		if (headerVal.size() == 0)
		{
			ERROR("InvalidArgument: Custom header's value is empty");
			*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute addCustomHTTPHeader() - 2nd argument should be a string or array of strings");
			return JSValueMakeUndefined(ctx);
		}

		if (argumentCount == 3)
		{
			isLicenseHeader = JSValueToBoolean(ctx, arguments[2]);
		}
		privObj->_aamp->AddCustomHTTPHeader(headerName, headerVal, isLicenseHeader);
	}
	else
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute addCustomHTTPHeader() - 2 arguments required");
	}
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.removeCustomHTTPHeader()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_removeCustomHTTPHeader (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call removeCustomHTTPHeader() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount == 1)
	{
		char *name = aamp_JSValueToCString(ctx, arguments[0], exception);
		std::string headerName(name);
		WARNING("Calling _aamp->AddCustomHTTPHeader(%s, ())", headerName);
		privObj->_aamp->AddCustomHTTPHeader(headerName, std::vector<std::string>());
		SAFE_DELETE_ARRAY(name);
	}
	else
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute removeCustomHTTPHeader() - 1 argument required");
	}
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.setVideoRect()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_setVideoRect (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setVideoRect() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount == 4)
	{
		int x = (int) JSValueToNumber(ctx, arguments[0], exception);
		int y = (int) JSValueToNumber(ctx, arguments[1], exception);
		int w = (int) JSValueToNumber(ctx, arguments[2], exception);
		int h = (int) JSValueToNumber(ctx, arguments[3], exception);
		{
			WARNING("Calling _aamp->SetVideoRectangle(%d, %d, %d, %d)", x, y, w, h);
			privObj->_aamp->SetVideoRectangle(x, y, w, h);
		}
	}
	else
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setVideoRect() - 1 argument required");
	}
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.setVideoZoom()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_setVideoZoom (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setVideoZoom() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount == 1)
	{
		VideoZoomMode zoom;
		char* zoomStr = aamp_JSValueToCString(ctx, arguments[0], exception);
		if (0 == strcmp(zoomStr, "none"))
		{
			zoom = VIDEO_ZOOM_NONE;
		}
		else
		{
			zoom = VIDEO_ZOOM_FULL;
		}
		WARNING("Calling _aamp->SetVideoZoom(%d)", static_cast<int>(zoom));
		privObj->_aamp->SetVideoZoom(zoom);
		SAFE_DELETE_ARRAY(zoomStr);
	}
	else
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setVideoZoom() - 1 argument required");
	}
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.release()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_release (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);

	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call release() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (false == findInGlobalCacheAndRelease(privObj))
	{
		WARNING("Invoked release of a PlayerInstanceAAMP object(%p) which was already/being released!!", privObj);
	}
	else
	{
		WARNING("Cleaned up native object for AAMPMediaPlayer_JS and PlayerInstanceAAMP object(%p)!!", privObj);
	}

	// Un-link native object from JS object
	JSObjectSetPrivate(thisObject, NULL);

	// Delete native object
	SAFE_DELETE(privObj);


	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.getAvailableAudioTracks()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMPMediaPlayerJS_getAvailableAudioTracks(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if(!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getAvailableAudioTracks() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	bool allTrack = false;
	if (argumentCount == 1)
	{
		allTrack = JSValueToBoolean(ctx, arguments[0]);
	}

	WARNING("Calling _aamp->GetAvailableAudioTracks(%d)", allTrack);
	std::string tracks = privObj->_aamp->GetAvailableAudioTracks(allTrack);
	if (!tracks.empty())
	{
		TRACELOG("Exit");
		return aamp_CStringToJSValue(ctx, tracks.c_str());
	}
	else
	{
		WARNING("Audio tracks empty");
		return JSValueMakeUndefined(ctx);
	}
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.getAvailableTextTracks()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMPMediaPlayerJS_getAvailableTextTracks(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if(!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getAvailableTextTracks() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	WARNING("Calling _aamp->GetAvailableTextTracks()");
	std::string tracks = privObj->_aamp->GetAvailableTextTracks();
	if (!tracks.empty())
	{
		TRACELOG("Exit");
		return aamp_CStringToJSValue(ctx, tracks.c_str());
	}
	else
	{
		WARNING("Text track empty");
		return JSValueMakeUndefined(ctx);
	}
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.getVideoRectangle()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMPMediaPlayerJS_getVideoRectangle(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if(!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getVideoRectangle() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	WARNING("Calling _aamp->GetVideoRectangle()");
	return aamp_CStringToJSValue(ctx, privObj->_aamp->GetVideoRectangle().c_str());
}

/**	
 * @brief API invoked from JS when executing AAMPMediaPlayer.setAlternateContent()	
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMPMediaPlayerJS_setAlternateContent(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)	
{
	TRACELOG("Enter, ctx: %p", ctx);	
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if(!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setAlternateContent() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	
	if (argumentCount == 2)
	{
		/*
		 * Parmater format
		 * "reservationObject": object {
		 *   "reservationId": "773701056",
		 *    "reservationBehavior": number
		 *    "placementRequest": {
		 *      "id": string,
		 *      "pts": number,
		 *      "url": "",
		 *    },
		 * },
		 * "promiseCallback": function
		 */
		char *reservationId = NULL;
		int reservationBehavior = -1;
		char *adId = NULL;
		long adPTS = -1;
		char *adURL = NULL;
		if (JSValueIsObject(ctx, arguments[0]))
		{
			//Parse the ad object
			JSObjectRef reservationObject = JSValueToObject(ctx, arguments[0], NULL);
			if (reservationObject == NULL)
			{
				ERROR("Unable to convert argument to JSObject");
				return JSValueMakeUndefined(ctx);
			}
			JSStringRef propName = JSStringCreateWithUTF8CString("reservationId");
			JSValueRef propValue = JSObjectGetProperty(ctx, reservationObject, propName, NULL);
			if (JSValueIsString(ctx, propValue))
			{
				reservationId = aamp_JSValueToCString(ctx, propValue, NULL);
			}
	
			JSStringRelease(propName);
			propName = JSStringCreateWithUTF8CString("reservationBehavior");
			propValue = JSObjectGetProperty(ctx, reservationObject, propName, NULL);
			if (JSValueIsNumber(ctx, propValue))
			{
				reservationBehavior = JSValueToNumber(ctx, propValue, NULL);
			}
			JSStringRelease(propName);
	
			propName = JSStringCreateWithUTF8CString("placementRequest");
			propValue = JSObjectGetProperty(ctx, reservationObject, propName, NULL);
			if (JSValueIsObject(ctx, propValue))
			{
				JSObjectRef adObject = JSValueToObject(ctx, propValue, NULL);
				JSStringRef adPropName = JSStringCreateWithUTF8CString("id");
				JSValueRef adPropValue = JSObjectGetProperty(ctx, adObject, adPropName, NULL);
				if (JSValueIsString(ctx, adPropValue))
				{
					adId = aamp_JSValueToCString(ctx, adPropValue, NULL);
				}
				JSStringRelease(adPropName);
				adPropName = JSStringCreateWithUTF8CString("pts");
				adPropValue = JSObjectGetProperty(ctx, adObject, adPropName, NULL);
				if (JSValueIsNumber(ctx, adPropValue))
				{
					adPTS = (long) JSValueToNumber(ctx, adPropValue, NULL);
				}
				JSStringRelease(adPropName);
				adPropName = JSStringCreateWithUTF8CString("url");
				adPropValue = JSObjectGetProperty(ctx, adObject, adPropName, NULL);
				if (JSValueIsString(ctx, adPropValue))
				{
					adURL = aamp_JSValueToCString(ctx, adPropValue, NULL);
				}
				JSStringRelease(adPropName);
			}
			JSStringRelease(propName);
		}
	
		JSObjectRef callbackObj = JSValueToObject(ctx, arguments[1], NULL);
		if ((callbackObj) && JSObjectIsFunction(ctx, callbackObj) && adId && reservationId && adURL)
		{
			std::string adIdStr(adId);  //CID:115002 - Resolve Forward null
			std::string adBreakId(reservationId);  //CID:115001 - Resolve Forward null
			std::string url(adURL);  //CID:115000 - Resolve Forward null

			privObj->saveCallbackForAdId(adIdStr, callbackObj); //save callback for sending status later, if ad can be played or not
			WARNING("Calling privObj->_aamp->SetAlternateContents with promiseCallback:%p", callbackObj);
			privObj->_aamp->SetAlternateContents(adBreakId, adIdStr, url);
		}
		else
		{
			ERROR("Unable to parse the promiseCallback argument");
		}
	
		SAFE_DELETE_ARRAY(reservationId);
		SAFE_DELETE_ARRAY(adURL);
		SAFE_DELETE_ARRAY(adId);
	}
	else
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 2", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setAlternateContent() - 2 argument required");
	}
	return JSValueMakeUndefined(ctx);
}

/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.setPreferredAudioLanguage()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_setPreferredAudioLanguage(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setPreferredAudioLanguage() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if( argumentCount>=1 && argumentCount<=4)
	{
		char* lanList = aamp_JSValueToCString(ctx,arguments[0], NULL);
		char *rendition = NULL;
		char *type = NULL;
		char *codecList = NULL;
		if(argumentCount >= 2) {
			rendition = aamp_JSValueToCString(ctx,arguments[1], NULL);
		}
		if(argumentCount >= 3) {
			type = aamp_JSValueToCString(ctx,arguments[2], NULL);
		}
		if(argumentCount >= 4) {
			codecList = aamp_JSValueToCString(ctx,arguments[3], NULL);
		}
		WARNING("Calling _aamp->SetPrefferedLanguages(%s, %s, %s, %s)", lanList, rendition, type, codecList);
		privObj->_aamp->SetPreferredLanguages(lanList, rendition, type, codecList);
		SAFE_DELETE_ARRAY(type);
		SAFE_DELETE_ARRAY(rendition);
		SAFE_DELETE_ARRAY(lanList);
		SAFE_DELETE_ARRAY(codecList);
	}
	else
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1, 2 , 3 or 4", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setPreferredAudioLanguage() - 1, 2 or 3 arguments required");
	}
	return JSValueMakeUndefined(ctx);
}



/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.setPreferredAudioCodec()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_setPreferredAudioCodec(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{ // placeholder - not ready for use/publishing
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setPreferredAudioCodec() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount != 1)
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setPreferredAudioCodec() - 1 argument required");
	}
	else
	{
		char *codecList = aamp_JSValueToCString(ctx,arguments[0], NULL);
		WARNING("Calling _aamp->SetPrefferedLanguages(0,0,0,%s)", codecList);
		privObj->_aamp->SetPreferredLanguages(NULL, NULL, NULL, codecList);
		SAFE_DELETE_ARRAY(codecList);
	}

	return JSValueMakeUndefined(ctx);
}

/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.notifyReservationCompletion()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMPMediaPlayerJS_notifyReservationCompletion(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if(!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call notifyReservationCompletion() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	
	if (argumentCount == 2)
	{
		const char * reservationId = aamp_JSValueToCString(ctx, arguments[0], exception);
		long time = (long) JSValueToNumber(ctx, arguments[1], exception);
		//Need an API in AAMP to notify that placements for this reservation are over and AAMP might have to trim
		//the ads to the period duration or not depending on time param
		INFO("Called reservation close for periodId:%s and time:%ld", reservationId, time);
		SAFE_DELETE_ARRAY(reservationId);
	}
	else
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 2", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute notifyReservationCompletion() - 2 argument required");
	}
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.setClosedCaptionStatus()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_setClosedCaptionStatus(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setClosedCaptionStatus() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount != 1)
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setClosedCaptionStatus() - 1 argument required");
	}
	else
	{
		bool enabled = JSValueToBoolean(ctx, arguments[0]);
		WARNING("Calling _aamp->SetCCStatus(%d)", enabled);
		privObj->_aamp->SetCCStatus(enabled);
	}
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.setTextStyleOptions()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_setTextStyleOptions(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setTextStyleOptions() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount != 1)
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setTextStyleOptions() - 1 argument required");
	}
	else
	{
		if (JSValueIsString(ctx, arguments[0]))
		{
			const char *options = aamp_JSValueToCString(ctx, arguments[0], NULL);
			WARNING("Calling _aamp->SetTextStyle(%s)", options);
			privObj->_aamp->SetTextStyle(std::string(options));
			SAFE_DELETE_ARRAY(options);
		}
		else
		{
			ERROR("InvalidArgument - Argument should be a JSON formatted string!");
			*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Argument should be a JSON formatted string!");
		}
	}
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.getTextStyleOptions()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMPMediaPlayerJS_getTextStyleOptions(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if(!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getTextStyleOptions() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	WARNING("Calling _aamp->GetTextStyle()");
	std::string options = privObj->_aamp->GetTextStyle();
	if (!options.empty())
	{
		TRACELOG("Exit");
		return aamp_CStringToJSValue(ctx, options.c_str());
	}
	else
	{
		WARNING("Text Style empty");
		return JSValueMakeUndefined(ctx);
	}
}

/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.disableContentRestrictions()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_disableContentRestrictions (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call disableContentRestrictions() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	long grace = 0;
	long time = -1;
	bool eventChange=false;
	bool updateStatus = false;
	if (argumentCount == 1 && JSValueIsObject(ctx, arguments[0]))
	{
		JSValueRef _exception = NULL;
		bool ret = false;
		bool valueAsBoolean = false;
		double valueAsNumber = 0;

		int numRelockParams = sizeof(relockConditionParamNames)/sizeof(relockConditionParamNames[0]);
		JSObjectRef unlockConditionObj = JSValueToObject(ctx, arguments[0], &_exception);
		if (unlockConditionObj == NULL || _exception != NULL)
		{
			ERROR("InvalidArgument - argument passed is NULL/not a valid object");
			*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute disableContentRestrictions() - object of unlockConditions required");
			return JSValueMakeUndefined(ctx);
		}


		for (int iter = 0; iter < numRelockParams; iter++)
		{
			ret = false;
			switch(relockConditionParamNames[iter].paramType)
			{
			case ePARAM_RELOCKONTIMEOUT:
				ret = ParseJSPropAsNumber(ctx, unlockConditionObj, relockConditionParamNames[iter].paramName, valueAsNumber);
				break;
			case ePARAM_RELOCKONPROGRAMCHANGE:
				ret = ParseJSPropAsBoolean(ctx, unlockConditionObj, relockConditionParamNames[iter].paramName, valueAsBoolean);
				break;
			default: //ePARAM_MAX_COUNT
				ret = false;
				break;
			}

			if(ret)
			{
				updateStatus = true;
				switch(relockConditionParamNames[iter].paramType)
				{
				case ePARAM_RELOCKONTIMEOUT:
					time = (long) valueAsNumber;
					break;
				case ePARAM_RELOCKONPROGRAMCHANGE:
					eventChange = valueAsBoolean;
					break;

				default: //ePARAM_MAX_COUNT
					break;
				}
			}
		}
		if(updateStatus)
		{
			privObj->_aamp->DisableContentRestrictions(grace, time, eventChange);
		}
	}
	else if(argumentCount > 1)
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1 or no argument", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute disableContentRestrictions() - 1 argument of type IConfig required");
	}
	else
	{
		//No parameter:parental control locking will be disabled until settop reboot, or explicit call to enableContentRestrictions
		grace = -1;
		privObj->_aamp->DisableContentRestrictions(grace, time, eventChange);
	}

	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.enableContentRestrictions()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_enableContentRestrictions (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call enableContentRestrictions() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	privObj->_aamp->EnableContentRestrictions();

	return JSValueMakeUndefined(ctx);
}

/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.setAuxiliaryLanguage()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
static JSValueRef AAMPMediaPlayerJS_setAuxiliaryLanguage(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if(!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setAuxiliaryLanguage() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount == 1)
	{
		const char *lang = aamp_JSValueToCString(ctx, arguments[0], NULL);
		privObj->_aamp->SetAuxiliaryLanguage(std::string(lang));
		SAFE_DELETE_ARRAY(lang);
	}
	else
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setAuxiliaryLanguage() - 1 argument required");
	}
	return JSValueMakeUndefined(ctx);
}

/**
 *  * @brief API invoked from JS when executing AAMPMediaPlayer.xreSupportedTune()
 *  * @param[in] ctx JS execution context
 *  * @param[in] function JSObject that is the function being called
 *  * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 *  * @param[in] argumentCount number of args
 *  * @param[in] arguments[] JSValue array of args
 *  * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 *  * @retval JSValue that is the function's return value
 *  */
JSValueRef AAMPMediaPlayerJS_xreSupportedTune(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call xreSupportedTune() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	if (argumentCount == 1)
	{
		bool xreSupported = JSValueToBoolean(ctx, arguments[0]);
		WARNING("Calling _aamp->XRESupportedTune(%d)", xreSupported);
		privObj->_aamp->XRESupportedTune(xreSupported);
	}
	else
	{
		ERROR("InvalidArgument - argumentCount=%d, expected: 1", argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute xreSupportedTune() - 1 argument required");
	}
	return JSValueMakeUndefined(ctx);
}

/**
 * @brief Array containing the AAMPMediaPlayer's statically declared functions
 */
static const JSStaticFunction AAMPMediaPlayer_JS_static_functions[] = {
	{ "load", AAMPMediaPlayerJS_load, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "initConfig", AAMPMediaPlayerJS_initConfig, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "play", AAMPMediaPlayerJS_play, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "detach", AAMPMediaPlayerJS_detach, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "pause", AAMPMediaPlayerJS_pause, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "stop", AAMPMediaPlayerJS_stop, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "seek", AAMPMediaPlayerJS_seek, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getCurrentState", AAMPMediaPlayerJS_getCurrentState, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getDurationSec", AAMPMediaPlayerJS_getDurationSec, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getCurrentPosition", AAMPMediaPlayerJS_getCurrentPosition, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getVideoBitrates", AAMPMediaPlayerJS_getVideoBitrates, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getAudioBitrates", AAMPMediaPlayerJS_getAudioBitrates, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getManifest", AAMPMediaPlayerJS_getManifest, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getCurrentVideoBitrate", AAMPMediaPlayerJS_getCurrentVideoBitrate, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setVideoBitrate", AAMPMediaPlayerJS_setVideoBitrate, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getCurrentAudioBitrate", AAMPMediaPlayerJS_getCurrentAudioBitrate, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setAudioBitrate", AAMPMediaPlayerJS_setAudioBitrate, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getAudioTrack", AAMPMediaPlayerJS_getAudioTrack, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getAudioTrackInfo", AAMPMediaPlayerJS_getAudioTrackInfo, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getPreferredAudioProperties", AAMPMediaPlayerJS_getPreferredAudioProperties, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setAudioTrack", AAMPMediaPlayerJS_setAudioTrack, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getTextTrack", AAMPMediaPlayerJS_getTextTrack, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setTextTrack", AAMPMediaPlayerJS_setTextTrack, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getVolume", AAMPMediaPlayerJS_getVolume, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setVolume", AAMPMediaPlayerJS_setVolume, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setAudioLanguage", AAMPMediaPlayerJS_setAudioLanguage, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getPlaybackRate", AAMPMediaPlayerJS_getPlaybackRate, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setPlaybackRate", AAMPMediaPlayerJS_setPlaybackRate, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getSupportedKeySystems", AAMPMediaPlayerJS_getSupportedKeySystems, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setVideoMute", AAMPMediaPlayerJS_setVideoMute, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setSubscribedTags", AAMPMediaPlayerJS_setSubscribedTags, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "subscribeResponseHeaders", AAMPMediaPlayerJS_subscribeResponseHeaders, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "addEventListener", AAMPMediaPlayerJS_addEventListener, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "removeEventListener", AAMPMediaPlayerJS_removeEventListener, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setDRMConfig", AAMPMediaPlayerJS_setDRMConfig, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},

	{ "addCustomHTTPHeader", AAMPMediaPlayerJS_addCustomHTTPHeader, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "removeCustomHTTPHeader", AAMPMediaPlayerJS_removeCustomHTTPHeader, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setVideoRect", AAMPMediaPlayerJS_setVideoRect, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setVideoZoom", AAMPMediaPlayerJS_setVideoZoom, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "release", AAMPMediaPlayerJS_release, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "getAvailableAudioTracks", AAMPMediaPlayerJS_getAvailableAudioTracks, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getAvailableTextTracks", AAMPMediaPlayerJS_getAvailableTextTracks, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getVideoRectangle", AAMPMediaPlayerJS_getVideoRectangle, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setAlternateContent", AAMPMediaPlayerJS_setAlternateContent, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "notifyReservationCompletion", AAMPMediaPlayerJS_notifyReservationCompletion, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setClosedCaptionStatus", AAMPMediaPlayerJS_setClosedCaptionStatus, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setTextStyleOptions", AAMPMediaPlayerJS_setTextStyleOptions, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "getTextStyleOptions", AAMPMediaPlayerJS_getTextStyleOptions, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "disableContentRestrictions", AAMPMediaPlayerJS_disableContentRestrictions, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "enableContentRestrictions", AAMPMediaPlayerJS_enableContentRestrictions, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "getThumbnail", AAMPMediaPlayerJS_getThumbnails, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "getAvailableThumbnailTracks", AAMPMediaPlayerJS_getAvailableThumbnailTracks, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setThumbnailTrack", AAMPMediaPlayerJS_setThumbnailTrack, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setPreferredAudioLanguage", AAMPMediaPlayerJS_setPreferredAudioLanguage, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setPreferredAudioCodec", AAMPMediaPlayerJS_setPreferredAudioCodec, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setAuxiliaryLanguage", AAMPMediaPlayerJS_setAuxiliaryLanguage, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "xreSupportedTune", AAMPMediaPlayerJS_xreSupportedTune, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ NULL, NULL, 0 }
};


/**
 * @brief API invoked from JS when reading value of AAMPMediaPlayer.version
 * @param[in] ctx JS execution context
 * @param[in] object JSObject to search for the property
 * @param[in] propertyName JSString containing the name of the property to get
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval property's value if object has the property, otherwise NULL
 */
JSValueRef AAMPMediaPlayerJS_getProperty_Version(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(object);
	if (!privObj || !privObj->_aamp)
	{
		ERROR("JSObjectGetPrivate returned NULL!");
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Get property version on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	return aamp_CStringToJSValue(ctx, AAMP_UNIFIED_VIDEO_ENGINE_VERSION);
}


/**
 * @brief Array containing the AAMPMediaPlayer's statically declared value properties
 */
static const JSStaticValue AAMPMediaPlayer_JS_static_values[] = {
	{ "version", AAMPMediaPlayerJS_getProperty_Version, NULL, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ NULL, NULL, NULL, 0 }
};


/**
 * @brief API invoked from JS when an object of AAMPMediaPlayerJS is destroyed
 * @param[in] object JSObject being finalized
 */
void AAMPMediaPlayer_JS_finalize(JSObjectRef object)
{
	TRACELOG("Enter, object: %p", object);

	AAMPMediaPlayer_JS *privObj = (AAMPMediaPlayer_JS *) JSObjectGetPrivate(object);
	if (privObj != NULL)
	{
		if (false == findInGlobalCacheAndRelease(privObj))
		{
			WARNING("Invoked finalize of a PlayerInstanceAAMP object(%p) which was already/being released!!", privObj);
		}
		else
		{
			WARNING("Cleaned up native object for AAMPMediaPlayer_JS and PlayerInstanceAAMP object(%p)!!", privObj);
		}
		//unlink native object from JS object
		JSObjectSetPrivate(object, NULL);
		// Delete native object
		SAFE_DELETE(privObj);
	}
	else
	{
		WARNING("Native memory already cleared, since privObj is NULL");
	}
}


/**
 * @brief Object declaration of AAMPMediaPlayer JS object
 */
static JSClassDefinition AAMPMediaPlayer_JS_object_def {
	0, /* current (and only) version is 0 */
	kJSClassAttributeNone,
	"__AAMPMediaPlayer",
	NULL,

	AAMPMediaPlayer_JS_static_values,
	AAMPMediaPlayer_JS_static_functions,

	NULL,
	AAMPMediaPlayer_JS_finalize,
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
 * @brief Creates a JavaScript class of AAMPMediaPlayer object for use with JSObjectMake
 * @retval singleton instance of JavaScript class created
 */
static JSClassRef AAMPMediaPlayer_object_ref() {
	static JSClassRef _mediaPlayerObjRef = NULL;
	if (!_mediaPlayerObjRef) {
		_mediaPlayerObjRef = JSClassCreate(&AAMPMediaPlayer_JS_object_def);
	}
	return _mediaPlayerObjRef;
}


/**
 * @brief API invoked when AAMPMediaPlayer is used along with 'new'
 * @param[in] ctx JS execution context
 * @param[in] constructor JSObject that is the constructor being called
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSObject that is the constructor's return value
 */
JSObjectRef AAMPMediaPlayer_JS_class_constructor(JSContextRef ctx, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter, ctx: %p", ctx);

	std::string appName;
	if (argumentCount > 0)
	{
		if (JSValueIsString(ctx, arguments[0]))
		{
			char *value =  aamp_JSValueToCString(ctx, arguments[0], exception);
			appName.assign(value);
			INFO("AAMPMediaPlayer created with app name: %s", appName.c_str());
			SAFE_DELETE_ARRAY(value);
		}
	}

	AAMPMediaPlayer_JS* privObj = new AAMPMediaPlayer_JS();

	privObj->_ctx = JSContextGetGlobalContext(ctx);
	privObj->_aamp = new PlayerInstanceAAMP(NULL, NULL);
	if (!appName.empty())
	{
		WARNING("Calling _aamp->SetAppName(%s)", appName);
		privObj->_aamp->SetAppName(appName);
	}
	privObj->_listeners.clear();


	// NOTE : Association of JSObject and AAMPMediaPlayer_JS native object will be deleted only in
	// AAMPMediaPlayerJS_release ( APP initiated )  or AAMPMediaPlayer_JS_finalize ( GC initiated)
	// There is chance that aamp_UnloadJS is called then functions on aamp object is called from JS script.
	// In this case AAMPMediaPlayer_JS should be available to access
	JSObjectRef newObj = JSObjectMake(ctx, AAMPMediaPlayer_object_ref(), privObj);

	pthread_mutex_lock(&jsMediaPlayerCacheMutex);
	AAMPMediaPlayer_JS::_jsMediaPlayerInstances.push_back(privObj);
	pthread_mutex_unlock(&jsMediaPlayerCacheMutex);

	// Add a dummy event listener without any function callback.
	// Upto JS application to register a common callback function for AAMP to notify ad resolve status
	// or individually as part of setAlternateContent call. NULL checks added in eventlistener to avoid undesired behaviour
	AAMP_JSEventListener::AddEventListener(privObj, AAMP_EVENT_AD_RESOLVED, NULL);

	// Required for viper-player
	JSStringRef fName = JSStringCreateWithUTF8CString("toString");
	JSStringRef fString = JSStringCreateWithUTF8CString("return \"[object __AAMPMediaPlayer]\";");
	JSObjectRef toStringFunc = JSObjectMakeFunction(ctx, NULL, 0, NULL, fString, NULL, 0, NULL);

	JSObjectSetProperty(ctx, newObj, fName, toStringFunc, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete, NULL);

	JSStringRelease(fName);
	JSStringRelease(fString);

	return newObj;
}


/**
 * @brief Class declaration of AAMPMediaPlayer JS object
 */
static JSClassDefinition AAMPMediaPlayer_JS_class_def {
	0, /* current (and only) version is 0 */
	kJSClassAttributeNone,
	"__AAMPMediaPlayer__class",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	AAMPMediaPlayer_JS_class_constructor,
	NULL,
	NULL
};


/**
 * @brief Clear any remaining/active AAMPPlayer instances
 */
void ClearAAMPPlayerInstances(void)
{
	pthread_mutex_lock(&jsMediaPlayerCacheMutex);
	ERROR("Number of active jsmediaplayer instances: %d", AAMPMediaPlayer_JS::_jsMediaPlayerInstances.size());
	while(AAMPMediaPlayer_JS::_jsMediaPlayerInstances.size() > 0)
	{
		AAMPMediaPlayer_JS *obj = AAMPMediaPlayer_JS::_jsMediaPlayerInstances.back();
		releaseNativeResources(obj);
		AAMPMediaPlayer_JS::_jsMediaPlayerInstances.pop_back();
	}
	pthread_mutex_unlock(&jsMediaPlayerCacheMutex);
}

#ifdef PLATCO

class XREReceiver_onEventHandler
{
public:
	static void handle(JSContextRef ctx, std::string method, size_t argumentCount, const JSValueRef arguments[])
	{
		if(handlers_map.count(method) != 0)
		{
			handlers_map.at(method)(ctx, argumentCount, arguments);
		}
		else
		{
			ERROR("no handler for method %s", method.c_str());
		}
	}

	static std::string findTextTrackWithLang(JSContextRef ctx, std::string selectedLang)
	{
		const auto textTracks = AampCCManager::GetInstance()->getLastTextTracks();
		ERROR("[XREReceiver]: found %d text tracks", (int)textTracks.size());

		if(!selectedLang.empty() && isdigit(selectedLang[0]))
		{
			ERROR("[XREReceiver] trying to parse selected lang as index"\);
			try
			{
				//input index starts from 1, not from 0, hence '-1'
				int idx = std::stoi(selectedLang)-1;
				ERROR("[XREReceiver] parsed index = %d", idx);
				return textTracks.at(idx).instreamId;
			}
			catch(const std::exception& e)
			{
				ERROR("[XREReceiver] exception during parsing lang selection %s", e.what());
			}
		}

		for(const auto& track : textTracks)
		{
			ERROR("[XREReceiver] found language '%s', expected '%s'", track.language.c_str(), selectedLang.c_str());
			if(selectedLang == track.language)
			{
				return track.instreamId;
			}
		}

		ERROR("[XREReceiver] cannot find text track matching the selected language, defaulting to 'CC1'");
		return "CC1";
	}

private:

	static void handle_onClosedCaptions(JSContextRef ctx, size_t argumentCount, const JSValueRef arguments[])
	{
		ERROR("[XREReceiver]");
		if(argumentCount != 2)
		{
			ERROR("[XREReceiver] wrong argument count (expected 2) %d", argumentCount);
			return;
		}

		JSObjectRef argument = JSValueToObject(ctx, arguments[1], NULL);

		JSStringRef param_enable 		= JSStringCreateWithUTF8CString("enable");
		JSStringRef param_setOptions	= JSStringCreateWithUTF8CString("setOptions");
		JSStringRef param_setTrack		= JSStringCreateWithUTF8CString("setTrack");

		JSValueRef param_enable_value 		= JSObjectGetProperty(ctx, argument, param_enable, NULL);
		JSValueRef param_setOptions_value	= JSObjectGetProperty(ctx, argument, param_setOptions, NULL);
		JSValueRef param_setTrack_value		= JSObjectGetProperty(ctx, argument, param_setTrack, NULL);

		if(JSValueIsBoolean(ctx, param_enable_value))
		{
			const bool enable_value = JSValueToBoolean(ctx, param_enable_value);
			ERROR("[XREReceiver]: received enable boolean %d", enable_value);

#ifdef AAMP_CC_ENABLED
			AampCCManager::GetInstance()->SetStatus(enable_value);
#endif
			if(enable_value)
			{
				const auto textTracks = AampCCManager::GetInstance()->getLastTextTracks();
				std::string defaultTrack;
				if(!textTracks.empty())
				{
					defaultTrack = textTracks.front().instreamId;
				}

				if(defaultTrack.empty())
					defaultTrack = "CC1";

				ERROR("[XREReceiver]:found %d tracks, selected default textTrack = '%s'", (int)textTracks.size(), defaultTrack.c_str());

#ifdef AAMP_CC_ENABLED
				AampCCManager::GetInstance()->SetTrack(defaultTrack);
#endif
			}
		}

		if(JSValueIsObject(ctx, param_setOptions_value))
		{
			ERROR("[XREReceiver]: received setOptions, ignoring for now");
		}

		if(JSValueIsString(ctx, param_setTrack_value))
		{
			char* lang = aamp_JSValueToCString(ctx, param_setTrack_value, NULL);
			ERROR("[XREReceiver]: received setTrack language:  %s", lang);

			std::string lang_str;
			lang_str.assign(lang);
			SAFE_DELETE_ARRAY(lang);

			std::string textTrack = findTextTrackWithLang(ctx, lang_str);

			ERROR("[XREReceiver]: selected textTrack = '%s'", textTrack.c_str());

#ifdef AAMP_CC_ENABLED
			AampCCManager::GetInstance()->SetTrack(textTrack);
#endif

		}

		JSStringRelease(param_enable);
		JSStringRelease(param_setOptions);
		JSStringRelease(param_setTrack);
	}

	using Handler_t = std::function<void(JSContextRef, size_t, const JSValueRef[])>;
	static std::unordered_map<std::string, Handler_t> handlers_map;
};

std::unordered_map<std::string, XREReceiver_onEventHandler::Handler_t>  XREReceiver_onEventHandler::handlers_map = {
	{ std::string{"onClosedCaptions"}, &XREReceiver_onEventHandler::handle_onClosedCaptions}
};

// temporary patch to avoid JavaScript exception in webapps referencing XREReceiverObject in builds that don't support it

/**
 * @brief API invoked from JS when executing XREReceiver.oneven() method)
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef XREReceiverJS_onevent (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	INFO("[XREReceiver]: arg count - %d", argumentCount);
	if (argumentCount > 0)
	{
		if (JSValueIsString(ctx, arguments[0]))
		{
			char* value =  aamp_JSValueToCString(ctx, arguments[0], exception);
			std::string method;
			method.assign(value);
			XREReceiver_onEventHandler::handle(ctx, method, argumentCount, arguments);

			SAFE_DELETE_ARRAY(value);
		}
	}
	return JSValueMakeUndefined(ctx);
}

static const JSStaticFunction XREReceiver_JS_static_functions[] = 
{
	{ "onEvent", XREReceiverJS_onevent, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ NULL, NULL, 0 }
};

/**
 * @brief API invoked when AAMPMediaPlayer is used along with 'new'
 * @param[in] ctx JS execution context
 * @param[in] constructor JSObject that is the constructor being called
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSObject that is the constructor's return value
 */
JSObjectRef XREReceiver_JS_class_constructor(JSContextRef ctx, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
        TRACELOG("Enter, ctx: %p", ctx);
	*exception = aamp_GetException(ctx, AAMPJS_GENERIC_ERROR, "Cannot create an object of XREReceiver");
        TRACELOG("Exit");
	return NULL;
}

static void XREReceiver_JS_finalize(JSObjectRef thisObject)
{
	WARNING(" object=%p", thisObject);
}

static JSClassDefinition XREReceiver_JS_class_def {
	0, // version
	kJSClassAttributeNone, // attributes: JSClassAttributes
	"__XREReceiver_class", // className: *const c_char
	NULL, // parentClass: JSClassRef
	NULL, // staticValues: *const JSStaticValue
	XREReceiver_JS_static_functions, // staticFunctions: *const JSStaticFunction
	NULL, // initialize: JSObjectInitializeCallback
	XREReceiver_JS_finalize, // finalize: JSObjectFinalizeCallback
	NULL, // hasProperty: JSObjectHasPropertyCallback
	NULL, // getProperty: JSObjectGetPropertyCallback
	NULL, // setProperty: JSObjectSetPropertyCallback
	NULL, // deleteProperty: JSObjectDeletePropertyCallback
	NULL, // getPropertyNames: JSObjectGetPropertyNamesCallback
	NULL, // callAsFunction: JSObjectCallAsFunctionCallback
	XREReceiver_JS_class_constructor, // callAsConstructor: JSObjectCallAsConstructorCallback,
	NULL, // hasInstance: JSObjectHasInstanceCallback
	NULL  // convertToType: JSObjectConvertToTypeCallback
};

void LoadXREReceiverStub(void* context)
{
	TRACELOG("Enter context = %p", context);
	JSGlobalContextRef jsContext = (JSGlobalContextRef)context;

	JSClassRef myClass = JSClassCreate(&XREReceiver_JS_class_def);
	JSObjectRef classObj = JSObjectMake(jsContext, myClass, NULL);
	JSObjectRef globalObj = JSContextGetGlobalObject(jsContext);

	JSStringRef str = JSStringCreateWithUTF8CString("XREReceiver");
	JSObjectSetProperty(jsContext, globalObj, str, classObj, kJSPropertyAttributeReadOnly, NULL);

}
#endif // PLATCO


/**
 * @brief Loads AAMPMediaPlayer JS constructor into JS context
 * @param[in] context JS execution context
 */
void AAMPPlayer_LoadJS(void* context)
{
	WARNING("context = %p", context);
	JSGlobalContextRef jsContext = (JSGlobalContextRef)context;

	JSObjectRef globalObj = JSContextGetGlobalObject(jsContext);

	JSClassRef mediaPlayerClass = JSClassCreate(&AAMPMediaPlayer_JS_class_def);
	JSObjectRef classObj = JSObjectMakeConstructor(jsContext, mediaPlayerClass, AAMPMediaPlayer_JS_class_constructor);
	JSValueProtect(jsContext, classObj);

	JSStringRef str = JSStringCreateWithUTF8CString("AAMPMediaPlayer");
	JSObjectSetProperty(jsContext, globalObj, str, classObj, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, NULL);

	JSClassRelease(mediaPlayerClass);
	JSStringRelease(str);
	
#ifdef PLATCO
	LoadXREReceiverStub(context);
#endif
	
}

/**
 * @brief Removes the AAMPMediaPlayer constructor from JS context
 * @param[in] context JS execution context
 */
void AAMPPlayer_UnloadJS(void* context)
{
	INFO("context=%p", context);

	JSValueRef exception = NULL;
	JSGlobalContextRef jsContext = (JSGlobalContextRef)context;

	//Clear all active js mediaplayer instances and its resources
	ClearAAMPPlayerInstances();

	JSObjectRef globalObj = JSContextGetGlobalObject(jsContext);
	JSStringRef str = JSStringCreateWithUTF8CString("AAMPMediaPlayer");

	JSValueRef playerConstructor = JSObjectGetProperty(jsContext, globalObj, str, &exception);

	if (playerConstructor == NULL || exception != NULL)
	{
		JSStringRelease(str);
		return;
	}

	JSObjectRef playerObj = JSValueToObject(jsContext, playerConstructor, &exception);
	if (playerObj == NULL || exception != NULL)
	{
		JSStringRelease(str);
		return;
	}

	if (!JSObjectIsConstructor(jsContext, playerObj))
	{
		JSStringRelease(str);
		return;
	}

	JSValueUnprotect(jsContext, playerConstructor);
	JSObjectSetProperty(jsContext, globalObj, str, JSValueMakeUndefined(jsContext), kJSPropertyAttributeReadOnly, NULL);
	JSStringRelease(str);

	// Force a garbage collection to clean-up all AAMP objects.
	WARNING("JSGarbageCollect() context=%p", context);
	JSGarbageCollect(jsContext);
}
