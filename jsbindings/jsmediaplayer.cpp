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


#include "jsbindings.h"
#include "jsutils.h"
#include "jseventlistener.h"
#include <vector>

#define AAMP_UNIFIED_VIDEO_ENGINE_VERSION "0.5"

/**
 * @struct AAMPMediaPlayer_JS
 * @brief Private data structure of AAMPMediaPlayer JS object
 */
struct AAMPMediaPlayer_JS : public PrivAAMPStruct_JS
{
	static std::vector<AAMPMediaPlayer_JS *> _jsMediaPlayerInstances;
};

extern "C"
{
	JS_EXPORT JSGlobalContextRef JSContextGetGlobalContext(JSContextRef);
}

std::vector<AAMPMediaPlayer_JS *> AAMPMediaPlayer_JS::_jsMediaPlayerInstances = std::vector<AAMPMediaPlayer_JS *>();

static pthread_mutex_t jsMediaPlayerCacheMutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief API to release internal resources of an AAMPMediaPlayerJS object
 * @param[in] object AAMPMediaPlayerJS object being released
 */
void AAMPMediaPlayer_JS_release(AAMPMediaPlayer_JS *privObj)
{
	if (privObj != NULL)
	{
		ERROR("[%s] Deleting AAMPMediaPlayer_JS instance:%p \n", __FUNCTION__, privObj);
		if (privObj->_aamp != NULL)
		{
			privObj->_aamp->Stop();
			if (privObj->_listeners.size() > 0)
			{
				AAMP_JSEventListener::RemoveAllEventListener(privObj);
			}
			ERROR("[%s] Deleting PlayerInstanceAAMP instance:%p\n", __FUNCTION__, privObj->_aamp);
			delete privObj->_aamp;
			privObj->_aamp = NULL;
		}

		delete privObj;
	}
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
		JSStringRef keyName = JSStringCreateWithUTF8CString("com.microsoft.playready");
		JSValueRef keyValue = JSObjectGetProperty(ctx, drmConfigObj, keyName, NULL);

		if (JSValueIsString(ctx, keyValue))
		{
			char *prLicenceServerURL = aamp_JSValueToCString(ctx, keyValue, NULL);
			ERROR("%s(): Playready License Server URL config param received - %s", __FUNCTION__, prLicenceServerURL);
			privObj->_aamp->SetLicenseServerURL(prLicenceServerURL, eDRM_PlayReady);

			delete[] prLicenceServerURL;
		}
		JSStringRelease(keyName);

		keyName = JSStringCreateWithUTF8CString("com.widevine.alpha");
		keyValue = JSObjectGetProperty(ctx, drmConfigObj, keyName, NULL);
		if (JSValueIsString(ctx, keyValue))
		{
			char *wvLicenceServerURL = aamp_JSValueToCString(ctx, keyValue, NULL);
			ERROR("%s(): Widevine License Server URL config param received - %s", __FUNCTION__, wvLicenceServerURL);
			privObj->_aamp->SetLicenseServerURL(wvLicenceServerURL, eDRM_WideVine);

			delete[] wvLicenceServerURL;
		}
		JSStringRelease(keyName);

		keyName = JSStringCreateWithUTF8CString("preferredKeysystem");
		keyValue = JSObjectGetProperty(ctx, drmConfigObj, keyName, NULL);
		if (JSValueIsString(ctx, keyValue))
		{
			char *keySystem = aamp_JSValueToCString(ctx, keyValue, NULL);
			if (strncmp(keySystem, "com.microsoft.playready", 23) == 0)
			{
				ERROR("%s(): Preferred key system config received - playready", __FUNCTION__);
				privObj->_aamp->SetPreferredDRM(eDRM_PlayReady);
			}
			else if (strncmp(keySystem, "com.widevine.alpha", 18) == 0)
			{
				ERROR("%s(): Preferred key system config received - widevine", __FUNCTION__);
				privObj->_aamp->SetPreferredDRM(eDRM_WideVine);
			}
			else
			{
				LOG("%s(): InvalidProperty - preferredKeySystem received", __FUNCTION__);
			}
			delete[] keySystem;
		}
		JSStringRelease(keyName);
	}
	else
	{
		ERROR("%s(): InvalidProperty - drmConfigParam is NULL", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call load() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount == 1)
	{
		char* url = aamp_JSValueToCString(ctx, arguments[0], exception);
		privObj->_aamp->Tune(url);
		delete [] url;
	}
	else
	{
		ERROR("%s(): InvalidArgument - argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute load() - 1 argument required");
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call initConfig() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount == 1 && JSValueIsObject(ctx, arguments[0]))
	{
		JSValueRef _exception = NULL;
		JSObjectRef initConfigObj = JSValueToObject(ctx, arguments[0], &_exception);
		if (initConfigObj == NULL || _exception != NULL)
		{
			ERROR("%s(): InvalidArgument - argument passed is NULL/not a valid object", __FUNCTION__);
			*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute initConfig() - object of IConfig required");
			return JSValueMakeUndefined(ctx);
		}

		JSStringRef initialBitrateStr = JSStringCreateWithUTF8CString("initialBitrate");
		JSValueRef initialBitrateValue = JSObjectGetProperty(ctx, initConfigObj, initialBitrateStr, NULL);
		if (JSValueIsNumber(ctx, initialBitrateValue))
		{
			long initialBitrate = (long) JSValueToNumber(ctx, initialBitrateValue, NULL);
			ERROR("%s(): initBitrate config param received - %d", __FUNCTION__, initialBitrate);
			privObj->_aamp->SetInitialBitrate(initialBitrate);
		}
		else
		{
			LOG("%s(): InvalidProperty - initialBitrate passed", __FUNCTION__);
		}
		JSStringRelease(initialBitrateStr);

		JSStringRef initialBitrate4KStr = JSStringCreateWithUTF8CString("initialBitrate4K");
		JSValueRef initialBitrate4KValue = JSObjectGetProperty(ctx, initConfigObj, initialBitrate4KStr, NULL);
		if (JSValueIsNumber(ctx, initialBitrate4KValue))
		{
			long initialBitrate4K = (long) JSValueToNumber(ctx, initialBitrate4KValue, NULL);
			ERROR("%s(): initBitrate config param received - %d", __FUNCTION__, initialBitrate4K);
			privObj->_aamp->SetInitialBitrate4K(initialBitrate4K);
		}
		else
		{
			LOG("%s(): InvalidProperty - initialBitrate4K passed", __FUNCTION__);
		}
		JSStringRelease(initialBitrate4KStr);

		JSStringRef initialBufferStr = JSStringCreateWithUTF8CString("initialBuffer");
		JSValueRef initialBufferValue = JSObjectGetProperty(ctx, initConfigObj, initialBufferStr, NULL);
		if (JSValueIsNumber(ctx, initialBufferValue))
		{
			int initialBuffer = (int) JSValueToNumber(ctx, initialBufferValue, NULL);
			ERROR("%s(): initialBuffer config param received - %d", __FUNCTION__, initialBuffer);
		}
		else
		{
			LOG("%s(): InvalidProperty - initialBuffer passed", __FUNCTION__);
		}
		JSStringRelease(initialBufferStr);

		JSStringRef playbackBufferStr = JSStringCreateWithUTF8CString("playbackBuffer");
		JSValueRef playbackBufferValue = JSObjectGetProperty(ctx, initConfigObj, playbackBufferStr, NULL);
		if (JSValueIsNumber(ctx, playbackBufferValue))
		{
			int playbackBuffer = (int) JSValueToNumber(ctx, playbackBufferValue, NULL);
			ERROR("%s(): playbackBuffer config param received - %d", __FUNCTION__, playbackBuffer);
		}
		else
		{
			LOG("%s(): InvalidProperty - playbackBuffer passed", __FUNCTION__);
		}
		JSStringRelease(playbackBufferStr);

		JSStringRef offsetStr = JSStringCreateWithUTF8CString("offset");
		JSValueRef offsetValue = JSObjectGetProperty(ctx, initConfigObj, offsetStr, NULL);
		if (JSValueIsNumber(ctx, offsetValue))
		{
			int offset = (int) JSValueToNumber(ctx, offsetValue, NULL);
			ERROR("%s(): offset config param received - %d", __FUNCTION__, offset);
			privObj->_aamp->Seek(offset);
		}
		else
		{
			LOG("%s(): InvalidProperty - offset passed", __FUNCTION__);
		}
		JSStringRelease(offsetStr);

		JSStringRef networkTimeoutStr = JSStringCreateWithUTF8CString("networkTimeout");
		JSValueRef networkTimeoutValue = JSObjectGetProperty(ctx, initConfigObj, networkTimeoutStr, NULL);
		if (JSValueIsNumber(ctx, networkTimeoutValue))
		{
			int networkTimeout = (int) JSValueToNumber(ctx, networkTimeoutValue, NULL);
			ERROR("%s(): networkTimeout config param received - %d", __FUNCTION__, networkTimeout);
			privObj->_aamp->SetNetworkTimeout(networkTimeout);
		}
		else
		{
			LOG("%s(): InvalidProperty - networkTimeout passed", __FUNCTION__);
		}
		JSStringRelease(networkTimeoutStr);

		JSStringRef downloadBufferStr = JSStringCreateWithUTF8CString("downloadBuffer");
		JSValueRef downloadBufferValue = JSObjectGetProperty(ctx, initConfigObj, downloadBufferStr, NULL);
		if (JSValueIsNumber(ctx, downloadBufferValue))
		{
			int downloadBuffer = (int) JSValueToNumber(ctx, downloadBufferValue, NULL);
			ERROR("%s(): downloadBuffer config param received - %d", __FUNCTION__, downloadBuffer);
			privObj->_aamp->SetDownloadBufferSize(downloadBuffer);
		}
		else
		{
			LOG("%s(): InvalidProperty - downloadBuffer passed", __FUNCTION__);
		}
		JSStringRelease(downloadBufferStr);

		JSStringRef minBitrateStr = JSStringCreateWithUTF8CString("minBitrate");
		JSValueRef minBitrateValue = JSObjectGetProperty(ctx, initConfigObj, minBitrateStr, NULL);
		if (JSValueIsNumber(ctx, minBitrateValue))
		{
			long minBitrate = (long) JSValueToNumber(ctx, minBitrateValue, NULL);
			ERROR("%s(): minBitrate config param received - %d", __FUNCTION__, minBitrate);
		}
		else
		{
			LOG("%s(): InvalidProperty - minBitrate passed", __FUNCTION__);
		}
		JSStringRelease(minBitrateStr);

		JSStringRef maxBitrateStr = JSStringCreateWithUTF8CString("maxBitrate");
		JSValueRef maxBitrateValue = JSObjectGetProperty(ctx, initConfigObj, maxBitrateStr, NULL);
		if (JSValueIsNumber(ctx, maxBitrateValue))
		{
			long maxBitrate = (long) JSValueToNumber(ctx, maxBitrateValue, NULL);
			ERROR("%s(): maxBitrate config param received - %d", __FUNCTION__, maxBitrate);
		}
		else
		{
			LOG("%s(): InvalidProperty - maxBitrate passed", __FUNCTION__);
		}
		JSStringRelease(maxBitrateStr);

		JSStringRef preferredAudioLanguageStr = JSStringCreateWithUTF8CString("preferredAudioLanguage");
		JSValueRef preferredAudioLanguageValue = JSObjectGetProperty(ctx, initConfigObj, preferredAudioLanguageStr, NULL);
		if (JSValueIsString(ctx, preferredAudioLanguageValue))
		{
			char *preferredAudioLanguage = aamp_JSValueToCString(ctx, preferredAudioLanguageValue, NULL);
			ERROR("%s(): preferredAudioLanguage config param received - %s", __FUNCTION__, preferredAudioLanguage);
			privObj->_aamp->SetLanguage(preferredAudioLanguage);
			delete[] preferredAudioLanguage;
		}
		else
		{
			LOG("%s(): InvalidProperty - preferredAudioLanguage passed", __FUNCTION__);
		}
		JSStringRelease(preferredAudioLanguageStr);

		JSStringRef tsbLengthStr = JSStringCreateWithUTF8CString("timeShiftBufferLength");
		JSValueRef tsbLengthValue = JSObjectGetProperty(ctx, initConfigObj, tsbLengthStr, NULL);
		if (JSValueIsNumber(ctx, tsbLengthValue))
		{
			int tsbLength = (int) JSValueToNumber(ctx, tsbLengthValue, NULL);
			ERROR("%s(): timeShiftBufferLength config param received - %d", __FUNCTION__, tsbLength);
		}
		else
		{
			LOG("%s(): InvalidProperty - timeShiftBufferLength passed", __FUNCTION__);
		}
		JSStringRelease(tsbLengthStr);

		JSStringRef drmConfigStr = JSStringCreateWithUTF8CString("drmConfig");
		JSValueRef drmConfigValue = JSObjectGetProperty(ctx, initConfigObj, drmConfigStr, NULL);
		if (JSValueIsObject(ctx, drmConfigValue))
		{
			parseDRMConfiguration(ctx, privObj, drmConfigValue);
		}
		else
		{
			LOG("%s(): InvalidProperty - drmConfig passed", __FUNCTION__);
		}
		JSStringRelease(drmConfigStr);

		JSStringRef liveOffsetStr = JSStringCreateWithUTF8CString("liveOffset");
		JSValueRef liveOffsetValue = JSObjectGetProperty(ctx, initConfigObj, liveOffsetStr, NULL);
		if (JSValueIsNumber(ctx, liveOffsetValue))
		{
			int liveOffset = (int) JSValueToNumber(ctx, liveOffsetValue, NULL);
			ERROR("%s(): liveOffset config param received - %d", __FUNCTION__, liveOffset);
			privObj->_aamp->SetLiveOffset(liveOffset);
		}
		else
		{
			LOG("%s(): InvalidProperty - liveOffset passed", __FUNCTION__);
		}
		JSStringRelease(liveOffsetStr);

	}
	else
	{
		ERROR("%s(): InvalidArgument - argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute initConfig() - 1 argument of type IConfig required");
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call play() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	privObj->_aamp->SetRate(1.0);
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call pause() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	privObj->_aamp->SetRate(0);
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call stop() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	privObj->_aamp->Stop();
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call seek() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount == 1)
	{
		double newSeekPos = JSValueToNumber(ctx, arguments[0], exception);
		privObj->_aamp->Seek(newSeekPos);
	}
	else
	{
		ERROR("%s(): InvalidArgument - argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute seek() - 1 argument required");
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getCurrentState() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	double duration = 0;
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getDurationSec() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	duration = privObj->_aamp->GetPlaybackDuration();
	if (duration < 0)
	{
		ERROR("%s(): Duration returned by GetPlaybackDuration() is less than 0!", __FUNCTION__);
		duration = 0;
	}

	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	double currPosition = 0;
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getCurrentPosition() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	currPosition = privObj->_aamp->GetPlaybackPosition();
	if (currPosition < 0)
	{
		ERROR("%s(): Current position returned by GetPlaybackPosition() is less than 0!", __FUNCTION__);
		currPosition = 0;
	}

	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getVideoBitrates() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
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
		delete [] array;
		TRACELOG("Exit %s()", __FUNCTION__);
		return retVal;
	}
	return JSValueMakeUndefined(ctx);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getAudioBitrates() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
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
		delete [] array;
		TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getCurrentVideoBitrate() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setVideoBitrate() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount != 1)
	{
		ERROR("%s(): InvalidArgument - argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setVideoBitrate() - 1 argument required");
	}
	else
	{
		long bitrate = (long) JSValueToNumber(ctx, arguments[0], NULL);
		//bitrate 0 is for ABR
		if (bitrate >= 0)
		{
			privObj->_aamp->SetVideoBitrate(bitrate);
		}
		else
		{
			ERROR("%s(): InvalidArgument - argument should be >= 0!", __FUNCTION__, argumentCount);
			*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Argument should be >= 0!");
		}
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getCurrentAudioBitrate() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setAudioBitrate() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount != 1)
	{
		ERROR("%s(): InvalidArgument - argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setAudioBitrate() - 1 argument required");
	}
	else
	{
		long bitrate = (long) JSValueToNumber(ctx, arguments[0], NULL);
		if (bitrate >= 0)
		{
			privObj->_aamp->SetAudioBitrate(bitrate);
		}
		else
		{
			ERROR("%s(): InvalidArgument - argument should be >= 0!", __FUNCTION__, argumentCount);
			*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Argument should be >= 0!");
		}
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getAudioTrack() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	ERROR("%s(): Invoked getAudioTrack", __FUNCTION__);
	TRACELOG("Exit %s()", __FUNCTION__);
	return JSValueMakeUndefined(ctx);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setAudioTrack() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount != 1)
	{
		ERROR("%s(): InvalidArgument - argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setAudioTrack() - 1 argument required");
	}
	else
	{
		ERROR("%s(): Invoked setAudioTrack", __FUNCTION__);
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getTextTrack() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	ERROR("%s(): Invoked getTextTrack", __FUNCTION__);
	TRACELOG("Exit %s()", __FUNCTION__);
	return JSValueMakeUndefined(ctx);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setTextTrack() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount != 1)
	{
		ERROR("%s(): InvalidArgument - argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setTextTrack() - 1 argument required");
	}
	else
	{
		ERROR("%s(): Invoked setTextTrack", __FUNCTION__);
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getVolume() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setVolume() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount == 1)
	{
		int volume = (int) JSValueToNumber(ctx, arguments[0], exception);
		if (volume >= 0)
		{
			privObj->_aamp->SetAudioVolume(volume);
		}
		else
		{
			ERROR("%s(): InvalidArgument - argument should not be a negative number", __FUNCTION__, argumentCount);
			*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Argument should not be a negative number");
		}
	}
	else
	{
		ERROR("%s(): InvalidArgument - argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setVolume() - 1 argument required");
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getPlaybackRate() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setPlaybackRate() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount == 1 || argumentCount == 2)
	{
		int overshootCorrection = 0;
		float rate = (int) JSValueToNumber(ctx, arguments[0], exception);
		if (argumentCount == 2)
		{
			overshootCorrection = (int) JSValueToNumber(ctx, arguments[1], exception);
		}
		privObj->_aamp->SetRate(rate, overshootCorrection);
	}
	else
	{
		ERROR("%s(): InvalidArgument - argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setPlaybackRate() - atleast 1 argument required");
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call getSupportedKeySystems() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}
	ERROR("%s(): Invoked getSupportedKeySystems", __FUNCTION__);
	TRACELOG("Exit %s()", __FUNCTION__);
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.setProtectionSchemeInterface()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_setProtectionSchemeInterface (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setProtectionSchemeInterface() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount != 1)
	{
		ERROR("%s(): InvalidArgument - argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setProtectionSchemeInterface() - 1 argument required");
	}
	else
	{
		ERROR("%s(): Invoked setProtectionSchemeInterface", __FUNCTION__);
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setVideoMute() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount == 1)
	{
		bool videoMute = JSValueToBoolean(ctx, arguments[0]);
		privObj->_aamp->SetVideoMute(videoMute);
		ERROR("%s(): Invoked setVideoMute", __FUNCTION__);
	}
	else
	{
		ERROR("%s(): InvalidArgument - argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setVideoMute() - 1 argument required");
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setSubscribedTags() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount != 1)
	{
		ERROR("%s(): InvalidArgument - argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setSubscribedTags() - 1 argument required");
	}
	else if (!aamp_JSValueIsArray(ctx, arguments[0]))
	{
		ERROR("[AAMP_JS] %s() InvalidArgument: aamp_JSValueIsArray=%d", __FUNCTION__, aamp_JSValueIsArray(ctx, arguments[0]));
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setSubscribeTags() - parameter 1 is not an array");
	}
	else
	{
		std::vector<std::string> subscribedTags = aamp_StringArrayToCStringArray(ctx, arguments[0]);
		privObj->_aamp->SetSubscribedTags(subscribedTags);
		ERROR("%s(): Invoked setSubscribedTags", __FUNCTION__);
	}
	TRACELOG("Exit %s()", __FUNCTION__);
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief API invoked from JS when executing AAMPMediaPlayer.updateAlternateContent()
 * @param[in] ctx JS execution context
 * @param[in] function JSObject that is the function being called
 * @param[in] thisObject JSObject that is the 'this' variable in the function's scope
 * @param[in] argumentCount number of args
 * @param[in] arguments[] JSValue array of args
 * @param[out] exception pointer to a JSValueRef in which to return an exception, if any
 * @retval JSValue that is the function's return value
 */
JSValueRef AAMPMediaPlayerJS_updateAlternateContent (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call updateAlternateContent() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount != 1)
	{
		ERROR("%s(): InvalidArgument - argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute updateAlternateContent() - 1 argument required");
	}
	else
	{
		ERROR("%s(): Invoked updateAlternateContent", __FUNCTION__);
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call addEventListener() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount >= 2)
	{
		char* type = aamp_JSValueToCString(ctx, arguments[0], NULL);
		JSObjectRef callbackObj = JSValueToObject(ctx, arguments[1], NULL);

		if (callbackObj != NULL && JSObjectIsFunction(ctx, callbackObj))
		{
			AAMPEventType eventType = aampPlayer_getEventTypeFromName(type);
			LOG("%s() eventType='%s', %d", __FUNCTION__, type, eventType);

			if ((eventType >= 0) && (eventType < AAMP_MAX_NUM_EVENTS))
			{
				AAMP_JSEventListener::AddEventListener(privObj, eventType, callbackObj);
			}
		}
		else
		{
			ERROR("%s() callbackObj=%p, JSObjectIsFunction(context, callbackObj)=%d", __FUNCTION__, callbackObj, JSObjectIsFunction(ctx, callbackObj));
			char errMsg[512];
			memset(errMsg, '\0', 512);
			snprintf(errMsg, 511, "Failed to execute addEventListener() for event %s - parameter 2 is not a function", type);
			*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, (const char*)errMsg);
		}

		delete[] type;
	}
	else
	{
		ERROR("[AAMP_JS] %s() InvalidArgument: argumentCount=%d, expected: 2", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute addEventListener() - 2 arguments required.");
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call removeEventListener() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount >= 2)
	{
		char* type = aamp_JSValueToCString(ctx, arguments[0], NULL);
		JSObjectRef callbackObj = JSValueToObject(ctx, arguments[1], NULL);

		if (callbackObj != NULL && JSObjectIsFunction(ctx, callbackObj))
		{
			AAMPEventType eventType = aampPlayer_getEventTypeFromName(type);
			LOG("[AAMP_JS] %s() eventType='%s', %d", __FUNCTION__, type, eventType);

			if ((eventType >= 0) && (eventType < AAMP_MAX_NUM_EVENTS))
			{
				AAMP_JSEventListener::RemoveEventListener(privObj, eventType, callbackObj);
			}
		}
		else
		{
			ERROR("%s() InvalidArgument: callbackObj=%p, JSObjectIsFunction(context, callbackObj)=%d", __FUNCTION__, callbackObj, JSObjectIsFunction(ctx, callbackObj));
			char errMsg[512];
			memset(errMsg, '\0', 512);
			snprintf(errMsg, 511, "Failed to execute removeEventListener() for event %s - parameter 2 is not a function", type);
			*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, (const char*)errMsg);
		}

		delete[] type;
	}
	else
	{
		ERROR("%s(): InvalidArgument - argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute removeEventListener() - 1 argument required");
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setDrmConfig() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount != 1)
	{
		ERROR("%s(): InvalidArgument - argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setDrmConfig() - 1 argument required");
	}
	else
	{
		if (JSValueIsObject(ctx, arguments[0]))
		{
			parseDRMConfiguration(ctx, privObj, arguments[0]);
		}
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call addCustomHTTPHeader() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount == 2)
	{
		char *name = aamp_JSValueToCString(ctx, arguments[0], exception);
		std::string headerName(name);
		std::vector<std::string> headerVal;

		delete[] name;

		if (aamp_JSValueIsArray(ctx, arguments[1]))
		{
			headerVal = aamp_StringArrayToCStringArray(ctx, arguments[1]);
		}
		else if (JSValueIsString(ctx, arguments[1]))
		{
			headerVal.reserve(1);
			char *value =  aamp_JSValueToCString(ctx, arguments[1], exception);
			headerVal.push_back(value);
			delete[] value;
		}

		// Don't support empty values now
		if (headerVal.size() == 0)
		{
			ERROR("%s() InvalidArgument: Custom header's value is empty", __FUNCTION__, argumentCount);
			*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute addCustomHTTPHeader() - 2nd argument should be a string or array of strings");
			return JSValueMakeUndefined(ctx);
		}

		privObj->_aamp->AddCustomHTTPHeader(headerName, headerVal);
	}
	else
	{
		ERROR("%s(): InvalidArgument - argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute addCustomHTTPHeader() - 2 arguments required");
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call removeCustomHTTPHeader() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount == 1)
	{
		char *name = aamp_JSValueToCString(ctx, arguments[0], exception);
		std::string headerName(name);
		privObj->_aamp->AddCustomHTTPHeader(headerName, std::vector<std::string>());
		delete[] name;
	}
	else
	{
		ERROR("%s(): InvalidArgument - argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute removeCustomHTTPHeader() - 1 argument required");
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
		*exception = aamp_GetException(ctx, AAMPJS_MISSING_OBJECT, "Can only call setVideoRect() on instances of AAMPPlayer");
		return JSValueMakeUndefined(ctx);
	}

	if (argumentCount == 4)
	{
		int x = (int) JSValueToNumber(ctx, arguments[0], exception);
		int y = (int) JSValueToNumber(ctx, arguments[1], exception);
		int w = (int) JSValueToNumber(ctx, arguments[2], exception);
		int h = (int) JSValueToNumber(ctx, arguments[3], exception);
		privObj->_aamp->SetVideoRectangle(x,y,w,h);
	}
	else
	{
		ERROR("%s(): InvalidArgument - argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setVideoRect() - 1 argument required");
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(thisObject);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
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
		privObj->_aamp->SetVideoZoom(zoom);
		delete[] zoomStr;
	}
	else
	{
		ERROR("%s(): InvalidArgument - argumentCount=%d, expected: 1", __FUNCTION__, argumentCount);
		*exception = aamp_GetException(ctx, AAMPJS_INVALID_ARGUMENT, "Failed to execute setVideoZoom() - 1 argument required");
	}
	TRACELOG("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);
	//Release all resources

	TRACELOG("Exit %s()", __FUNCTION__);
	return JSValueMakeUndefined(ctx);
}


/**
 * @brief Array containing the AAMPMediaPlayer's statically declared functions
 */
static const JSStaticFunction AAMPMediaPlayer_JS_static_functions[] = {
	{ "load", AAMPMediaPlayerJS_load, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "initConfig", AAMPMediaPlayerJS_initConfig, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "play", AAMPMediaPlayerJS_play, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "pause", AAMPMediaPlayerJS_pause, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "stop", AAMPMediaPlayerJS_stop, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "seek", AAMPMediaPlayerJS_seek, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getCurrentState", AAMPMediaPlayerJS_getCurrentState, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getDurationSec", AAMPMediaPlayerJS_getDurationSec, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getCurrentPosition", AAMPMediaPlayerJS_getCurrentPosition, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getVideoBitrates", AAMPMediaPlayerJS_getVideoBitrates, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getAudioBitrates", AAMPMediaPlayerJS_getAudioBitrates, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getCurrentVideoBitrate", AAMPMediaPlayerJS_getCurrentVideoBitrate, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setVideoBitrate", AAMPMediaPlayerJS_setVideoBitrate, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getCurrentAudioBitrate", AAMPMediaPlayerJS_getCurrentAudioBitrate, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setAudioBitrate", AAMPMediaPlayerJS_setAudioBitrate, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getAudioTrack", AAMPMediaPlayerJS_getAudioTrack, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setAudioTrack", AAMPMediaPlayerJS_setAudioTrack, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getTextTrack", AAMPMediaPlayerJS_getTextTrack, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setTextTrack", AAMPMediaPlayerJS_setTextTrack, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getVolume", AAMPMediaPlayerJS_getVolume, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setVolume", AAMPMediaPlayerJS_setVolume, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getPlaybackRate", AAMPMediaPlayerJS_getPlaybackRate, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setPlaybackRate", AAMPMediaPlayerJS_setPlaybackRate, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "getSupportedKeySystems", AAMPMediaPlayerJS_getSupportedKeySystems, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setProtectionSchemeInterface", AAMPMediaPlayerJS_setProtectionSchemeInterface, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setVideoMute", AAMPMediaPlayerJS_setVideoMute, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setSubscribedTags", AAMPMediaPlayerJS_setSubscribedTags, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "updateAlternateContent", AAMPMediaPlayerJS_updateAlternateContent, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "addEventListener", AAMPMediaPlayerJS_addEventListener, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "removeEventListener", AAMPMediaPlayerJS_removeEventListener, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},
	{ "setDRMConfig", AAMPMediaPlayerJS_setDRMConfig, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly},

	{ "addCustomHTTPHeader", AAMPMediaPlayerJS_addCustomHTTPHeader, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "removeCustomHTTPHeader", AAMPMediaPlayerJS_removeCustomHTTPHeader, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setVideoRect", AAMPMediaPlayerJS_setVideoRect, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "setVideoZoom", AAMPMediaPlayerJS_setVideoZoom, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
	{ "release", AAMPMediaPlayerJS_release, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
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
	TRACELOG("Enter %s()", __FUNCTION__);
	AAMPMediaPlayer_JS* privObj = (AAMPMediaPlayer_JS*)JSObjectGetPrivate(object);
	if (!privObj)
	{
		ERROR("%s(): Error - JSObjectGetPrivate returned NULL!", __FUNCTION__);
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
	ERROR("Enter %s()", __FUNCTION__);

	bool isFound = false;
	AAMPMediaPlayer_JS *privObj = (AAMPMediaPlayer_JS *) JSObjectGetPrivate(object);

	pthread_mutex_lock(&jsMediaPlayerCacheMutex);
	//Remove this instance from global cache
	for (std::vector<AAMPMediaPlayer_JS *>::iterator iter = AAMPMediaPlayer_JS::_jsMediaPlayerInstances.begin(); iter != AAMPMediaPlayer_JS::_jsMediaPlayerInstances.end(); iter++)
	{
		if (privObj == *iter)
		{
			AAMPMediaPlayer_JS::_jsMediaPlayerInstances.erase(iter);
			isFound = true;
			break;
		}
	}
	pthread_mutex_unlock(&jsMediaPlayerCacheMutex);

	if (isFound)
	{
		//Release private resources
		AAMPMediaPlayer_JS_release(privObj);
	}
	else
	{
		ERROR("%s:%d [WARN]Invoked finalize of a AAMPMediaPlayer_JS object(%p) which was already/being released!!\n", __FUNCTION__, __LINE__, privObj);
	}
	JSObjectSetPrivate(object, NULL);
	ERROR("Exit %s()", __FUNCTION__);
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
	TRACELOG("Enter %s()", __FUNCTION__);

	AAMPMediaPlayer_JS* privObj = new AAMPMediaPlayer_JS();

	privObj->_ctx = JSContextGetGlobalContext(ctx);
	privObj->_aamp = new PlayerInstanceAAMP();
	privObj->_listeners.clear();

	JSObjectRef newObj = JSObjectMake(ctx, AAMPMediaPlayer_object_ref(), privObj);

	pthread_mutex_lock(&jsMediaPlayerCacheMutex);
	AAMPMediaPlayer_JS::_jsMediaPlayerInstances.push_back(privObj);
	pthread_mutex_unlock(&jsMediaPlayerCacheMutex);

	// Required for viper-player
	JSStringRef fName = JSStringCreateWithUTF8CString("toString");
	JSStringRef fString = JSStringCreateWithUTF8CString("return \"[object __AAMPMediaPlayer]\";");
	JSObjectRef toStringFunc = JSObjectMakeFunction(ctx, NULL, 0, NULL, fString, NULL, 0, NULL);

	JSObjectSetProperty(ctx, newObj, fName, toStringFunc, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum | kJSPropertyAttributeDontDelete, NULL);

	JSStringRelease(fName);
	JSStringRelease(fString);

	TRACELOG("Exit %s()", __FUNCTION__);
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
	ERROR("Number of active jsmediaplayer instances: %d\n", AAMPMediaPlayer_JS::_jsMediaPlayerInstances.size());
	while(AAMPMediaPlayer_JS::_jsMediaPlayerInstances.size() > 0)
	{
		AAMPMediaPlayer_JS *obj = AAMPMediaPlayer_JS::_jsMediaPlayerInstances.back();
		AAMPMediaPlayer_JS_release(obj);
		AAMPMediaPlayer_JS::_jsMediaPlayerInstances.pop_back();
	}
	pthread_mutex_unlock(&jsMediaPlayerCacheMutex);
}


/**
 * @brief Loads AAMPMediaPlayer JS constructor into JS context
 * @param[in] context JS execution context
 */
void AAMPPlayer_LoadJS(void* context)
{
	TRACELOG("Enter %s()", __FUNCTION__);
	ERROR("Enter %s(), context = %p", __FUNCTION__, context);
	JSGlobalContextRef jsContext = (JSGlobalContextRef)context;

	JSObjectRef globalObj = JSContextGetGlobalObject(jsContext);

	JSClassRef mediaPlayerClass = JSClassCreate(&AAMPMediaPlayer_JS_class_def);
	JSObjectRef classObj = JSObjectMakeConstructor(jsContext, mediaPlayerClass, AAMPMediaPlayer_JS_class_constructor);
	JSValueProtect(jsContext, classObj);

	JSStringRef str = JSStringCreateWithUTF8CString("AAMPMediaPlayer");
	JSObjectSetProperty(jsContext, globalObj, str, classObj, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, NULL);

	JSClassRelease(mediaPlayerClass);
	JSStringRelease(str);
	TRACELOG("Exit %s()", __FUNCTION__);
}


/**
 * @brief Removes the AAMPMediaPlayer constructor from JS context
 * @param[in] context JS execution context
 */
void AAMPPlayer_UnloadJS(void* context)
{
	INFO("[AAMP_JS] %s() context=%p", __FUNCTION__, context);

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
	LOG("[AAMP_JS] JSGarbageCollect() context=%p", context);
	JSGarbageCollect(jsContext);
	TRACELOG("Exit %s()", __FUNCTION__);
}
