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
 * @file jseventlistener.cpp
 * @brief Event Listner impl for PrivAAMPStruct_JS object
 */


#include "jseventlistener.h"
#include "jsevent.h"
#include "jsutils.h"
#include "vttCue.h"


/**
 * @class AAMP_Listener_PlaybackStateChanged
 * @brief Event listener impl for AAMP_EVENT_STATE_CHANGED event.
 */
class AAMP_Listener_PlaybackStateChanged : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_PlaybackStateChanged Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_PlaybackStateChanged(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		StateChangedEventPtr evt = std::dynamic_pointer_cast<StateChangedEvent>(ev);

		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("state");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getState()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}

};


/**
 * @class AAMP_Listener_ProgressUpdate
 * @brief Event listener impl for AAMP_EVENT_PROGRESS event.
 */
class AAMP_Listener_ProgressUpdate : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_ProgressUpdate Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_ProgressUpdate(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		ProgressEventPtr evt = std::dynamic_pointer_cast<ProgressEvent>(ev);

		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("durationMiliseconds");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getDuration()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("positionMiliseconds");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("playbackSpeed");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getSpeed()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("startMiliseconds");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getStart()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("endMiliseconds");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getEnd()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("currentPTS");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getPTS()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("videoBufferedMiliseconds");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getBufferedDuration()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("timecode");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, evt->getSEITimeCode()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/**
 * @class AAMP_Listener_SpeedChanged
 * @brief Event listener impl for AAMP_EVENT_SPEED_CHANGED event.
 */
class AAMP_Listener_SpeedChanged : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_SpeedChanged Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_SpeedChanged(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		SpeedChangedEventPtr evt = std::dynamic_pointer_cast<SpeedChangedEvent>(ev);

		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("speed");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getRate()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("reason");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, "unknown"), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/**
 * @class AAMP_Listener_BufferingChanged
 * @brief Event listener impl for AAMP_EVENT_BUFFERING_CHANGED event.
 */
class AAMP_Listener_BufferingChanged : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_BufferingChanged Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_BufferingChanged(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		BufferingChangedEventPtr evt = std::dynamic_pointer_cast<BufferingChangedEvent>(ev);

		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("buffering");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeBoolean(p_obj->_ctx, evt->buffering()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/**
 * @class AAMP_Listener_PlaybackFailed
 * @brief Event listener impl for AAMP_EVENT_TUNE_FAILED event.
 */
class AAMP_Listener_PlaybackFailed : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_PlaybackFailed Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_PlaybackFailed(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		MediaErrorEventPtr evt = std::dynamic_pointer_cast<MediaErrorEvent>(ev);

		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("code");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getCode()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("description");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, evt->getDescription().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("shouldRetry");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeBoolean(p_obj->_ctx, evt->shouldRetry()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/**
 * @class AAMP_Listener_MediaMetadata
 * @brief Event listener impl for AAMP_EVENT_MEDIA_METADATA event.
 */
class AAMP_Listener_MediaMetadata : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_MediaMetadata Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_MediaMetadata(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		MediaMetadataEventPtr evt = std::dynamic_pointer_cast<MediaMetadataEvent>(ev);

		JSStringRef prop;
		prop = JSStringCreateWithUTF8CString("durationMiliseconds");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getDuration()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		int count = evt->getLanguagesCount();
		const std::vector<std::string> &langVect = evt->getLanguages();
		JSValueRef* array = new JSValueRef[count];
		for (int32_t i = 0; i < count; i++)
		{
			JSValueRef lang = aamp_CStringToJSValue(p_obj->_ctx, langVect[i].c_str());
			array[i] = lang;
		}
		JSValueRef propValue = JSObjectMakeArray(p_obj->_ctx, count, array, NULL);
		SAFE_DELETE_ARRAY(array);

		prop = JSStringCreateWithUTF8CString("languages");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, propValue, kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		count = evt->getBitratesCount();
		const std::vector<long> &bitrateVect = evt->getBitrates();
		array = new JSValueRef[count];
		for (int32_t i = 0; i < count; i++)
		{
			array[i] = JSValueMakeNumber(p_obj->_ctx, bitrateVect[i]);
		}
		propValue = JSObjectMakeArray(p_obj->_ctx, count, array, NULL);
		SAFE_DELETE_ARRAY(array);

		prop = JSStringCreateWithUTF8CString("bitrates");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, propValue, kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		count = evt->getSupportedSpeedCount();
		const std::vector<int> &speedVect = evt->getSupportedSpeeds();
		array = new JSValueRef[count];
		for (int32_t i = 0; i < count; i++)
		{
			array[i] = JSValueMakeNumber(p_obj->_ctx, speedVect[i]);
		}
		propValue = JSObjectMakeArray(p_obj->_ctx, count, array, NULL);
		SAFE_DELETE_ARRAY(array);

		prop = JSStringCreateWithUTF8CString("playbackSpeeds");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, propValue, kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("programStartTime");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getProgramStartTime()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("width");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getWidth()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("height");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getHeight()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("hasDrm");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeBoolean(p_obj->_ctx, evt->hasDrm()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("isLive");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeBoolean(p_obj->_ctx, evt->isLive()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("DRM");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, evt->getDrmType().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		//ratings
		if(!evt->getRatings().empty())
		{
			prop = JSStringCreateWithUTF8CString("ratings");
			JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, evt->getRatings().c_str()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}

		//ssi
		if(evt->getSsi() >= 0 )
		{
			prop = JSStringCreateWithUTF8CString("ssi");
			JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getSsi()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}

		//framerate
		if(evt->getFrameRate() > 0 )
		{
			prop = JSStringCreateWithUTF8CString("framerate");
			JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getFrameRate()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}

		if(eVIDEOSCAN_UNKNOWN != evt->getVideoScanType())
		{
			prop = JSStringCreateWithUTF8CString("progressive");
			JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeBoolean(p_obj->_ctx, ((eVIDEOSCAN_PROGRESSIVE == evt->getVideoScanType())?true:false)), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}
		//aspect ratio
		if((0 != evt->getAspectRatioWidth()) && (0 != evt->getAspectRatioHeight()))
		{
			prop = JSStringCreateWithUTF8CString("aspectRatioWidth");
			JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getAspectRatioWidth()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);

			prop = JSStringCreateWithUTF8CString("aspectRatioHeight");
			JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getAspectRatioHeight()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}

		//VideoCodec
		if(!evt->getVideoCodec().empty())
		{
			prop = JSStringCreateWithUTF8CString("videoCodec");
			JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, evt->getVideoCodec().c_str()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}

		//HdrType
		if(!evt->getHdrType().empty())
		{
			prop = JSStringCreateWithUTF8CString("hdrType");
			JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, evt->getHdrType().c_str()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}

		//AudioBitrate
		const std::vector<long> &audioBitrateVect = evt->getAudioBitrates();
		count = audioBitrateVect.size();
		if(count > 0 )
		{
			array = new JSValueRef[count];
			for (int32_t i = 0; i < count; i++)
			{
				array[i] = JSValueMakeNumber(p_obj->_ctx, audioBitrateVect[i]);
			}
			propValue = JSObjectMakeArray(p_obj->_ctx, count, array, NULL);
			delete [] array;

			prop = JSStringCreateWithUTF8CString("audioBitrates");
			JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, propValue, kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}


		//AudioCodec
		if(!evt->getAudioCodec().empty())
		{
			prop = JSStringCreateWithUTF8CString("audioCodec");
			JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, evt->getAudioCodec().c_str()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}

		//AudioMixType
		if(!evt->getAudioMixType().empty())
		{
			prop = JSStringCreateWithUTF8CString("audioMixType");
			JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, evt->getAudioMixType().c_str()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}

		//AtmosInfo
		prop = JSStringCreateWithUTF8CString("isAtmos");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeBoolean(p_obj->_ctx,evt->getAtmosInfo()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/**
 * @class AAMP_Listener_SpeedsChanged
 * @brief Event listener impl for AAMP_EVENT_SPEEDS_CHANGED event.
 */
class AAMP_Listener_SpeedsChanged : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_SpeedsChanged Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_SpeedsChanged(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		SupportedSpeedsChangedEventPtr evt = std::dynamic_pointer_cast<SupportedSpeedsChangedEvent>(ev);

		int count = evt->getSupportedSpeedCount();
		const std::vector<int> &speedVect = evt->getSupportedSpeeds();
		JSValueRef* array = new JSValueRef[count];
		for (int32_t i = 0; i < count; i++)
		{
			array[i] = JSValueMakeNumber(p_obj->_ctx, speedVect[i]);
		}
		JSValueRef propValue = JSObjectMakeArray(p_obj->_ctx, count, array, NULL);
		SAFE_DELETE_ARRAY(array);

		JSStringRef prop = JSStringCreateWithUTF8CString("playbackSpeeds");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, propValue, kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};

/**
 * @class AAMP_Listener_Seeked
 * @brief Event listener impl for AAMP_EVENT_SEEKED event.
 */
class AAMP_Listener_Seeked : public AAMP_JSEventListener
{
public:
	/**
	* @brief AAMP_Listener_Seeked Constructor
	* @param[in] aamp instance of PrivAAMPStruct_JS
	* @param[in] type event type
	* @param[in] jsCallback callback to be registered as listener
	*/
	AAMP_Listener_Seeked(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		SeekedEventPtr evt = std::dynamic_pointer_cast<SeekedEvent>(ev);
		JSStringRef prop = JSStringCreateWithUTF8CString("position");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/**
 * @class AAMP_Listener_TuneProfiling
 * @brief Event listener impl for AAMP_EVENT_TUNE_PROFILING event.
 */
class AAMP_Listener_TuneProfiling : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_TuneProfiling Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_TuneProfiling(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		TuneProfilingEventPtr evt = std::dynamic_pointer_cast<TuneProfilingEvent>(ev);
                JSStringRef prop;
                const char* microData = evt->getProfilingData().c_str();

                LOG("AAMP_Listener_TuneProfiling microData %s", microData);
                prop = JSStringCreateWithUTF8CString("microData");
                JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, microData), kJSPropertyAttributeReadOnly, NULL);
                JSStringRelease(prop);
	}

};

/**
 * @class AAMP_Listener_CCHandleAvailable
 * @brief Event listener impl for AAMP_EVENT_CC_HANDLE_RECEIVED, event.
 */
class AAMP_Listener_CCHandleAvailable : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_CCHandleAvailable Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_CCHandleAvailable(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		CCHandleEventPtr evt = std::dynamic_pointer_cast<CCHandleEvent>(ev);

		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("decoderHandle");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getCCHandle()), kJSPropertyAttributeReadOnly, NULL);
                JSStringRelease(prop);
	}

};


/**
 * @class AAMP_Listener_DRMMetadata
 * @brief Event listener impl for AAMP_EVENT_DRM_METADATA event.
 */
class AAMP_Listener_DRMMetadata : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_DRMMetadata Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_DRMMetadata(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		DrmMetaDataEventPtr evt = std::dynamic_pointer_cast<DrmMetaDataEvent>(ev);
		JSStringRef prop;

		int code = evt->getAccessStatusValue();
		const char* description = evt->getAccessStatus().c_str();

		ERROR("AAMP_Listener_DRMMetadata code %d Description %s", code, description);
		prop = JSStringCreateWithUTF8CString("code");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, code), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("description");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, description), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}

};


/**
 * @class AAMP_Listener_AnomalyReport
 * @brief Event listener impl for AAMP_EVENT_REPORT_ANOMALY event.
 */
class AAMP_Listener_AnomalyReport : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_AnomalyReport Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_AnomalyReport(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		AnomalyReportEventPtr evt = std::dynamic_pointer_cast<AnomalyReportEvent>(ev);
		JSStringRef prop;

		int severity = evt->getSeverity();
		const char* description = evt->getMessage().c_str();

		ERROR("AAMP_Listener_AnomalyReport severity %d Description %s", severity, description);
		prop = JSStringCreateWithUTF8CString("severity");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, severity), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("description");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, description), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}

};


/**
 * @class AAMP_Listener_VTTCueData
 * @brief Event listener impl for AAMP_EVENT_WEBVTT_CUE_DATA event.
 */
class AAMP_Listener_VTTCueData : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_VTTCueData Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_VTTCueData(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		WebVttCueEventPtr evt = std::dynamic_pointer_cast<WebVttCueEvent>(ev);

		JSStringRef prop;
		VTTCue *cue = evt->getCueData();

		prop = JSStringCreateWithUTF8CString("start");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, cue->mStart), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("duration");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, cue->mDuration), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("text");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, cue->mText.c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}

};


/**
 * @class AAMP_Listener_TimedMetadata
 * @brief Event listener impl for AAMP_EVENT_TIMED_METADATA event.
 */
class AAMP_Listener_TimedMetadata : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_TimedMetadata Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_TimedMetadata(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		TimedMetadataEventPtr evt = std::dynamic_pointer_cast<TimedMetadataEvent>(ev);

		JSObjectRef timedMetadata = aamp_CreateTimedMetadataJSObject(p_obj->_ctx, evt->getTime(), evt->getName().c_str(), evt->getContent().c_str(), evt->getId().c_str(), evt->getDuration());
		if (timedMetadata)
		{
			JSValueProtect(p_obj->_ctx, timedMetadata);
			JSStringRef prop = JSStringCreateWithUTF8CString("timedMetadata");
			JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, timedMetadata, kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
			JSValueUnprotect(p_obj->_ctx, timedMetadata);
		}
	}
};

/**
 * @class AAMP_Listener_BulkTimedMetadata
 * @brief Event listener impl for BULK_TIMED_METADATA AAMP event
 */
class AAMP_Listener_BulkTimedMetadata : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_BulkTimedMetadata Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
        AAMP_Listener_BulkTimedMetadata(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
        {
        }


        /**
         * @brief Set JS event properties
         */
        void SetEventProperties(const AAMPEventPtr& ev,  JSObjectRef eventObj)
        {
		BulkTimedMetadataEventPtr evt = std::dynamic_pointer_cast<BulkTimedMetadataEvent>(ev);
		JSStringRef name = JSStringCreateWithUTF8CString("timedMetadatas");
		JSObjectSetProperty(p_obj->_ctx, eventObj, name, aamp_CStringToJSValue(p_obj->_ctx, evt->getContent().c_str()),  kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);
        }
};


/**
 * @class AAMP_Listener_AdResolved
 *
 * @brief Event listener impl for AAMP_EVENT_AD_RESOLVED event
 */
class AAMP_Listener_AdResolved : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_AdResolved Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_AdResolved(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		AdResolvedEventPtr evt = std::dynamic_pointer_cast<AdResolvedEvent>(ev);
		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("resolvedStatus");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeBoolean(p_obj->_ctx, evt->getResolveStatus()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("placementId");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, evt->getAdId().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("placementStartTime");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getStart()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("placementDuration");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getDuration()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/**
 * @class AAMP_Listener_AdReservationStart
 *
 * @brief Event listener impl for AAMP_EVENT_AD_RESERVATION_START event
 */
class AAMP_Listener_AdReservationStart : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_AdReservationStart Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_AdReservationStart(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		AdReservationEventPtr evt = std::dynamic_pointer_cast<AdReservationEvent>(ev);
		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("adbreakId");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, evt->getAdBreakId().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/**
 * @class AAMP_Listener_AdReservationEnd
 *
 * @brief Event listener impl for AAMP_EVENT_AD_RESERVATION_END event
 */
class AAMP_Listener_AdReservationEnd : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_AdReservationEnd Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_AdReservationEnd(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		AdReservationEventPtr evt = std::dynamic_pointer_cast<AdReservationEvent>(ev);
		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("adbreakId");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, evt->getAdBreakId().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/**
 * @class AAMP_Listener_AdPlacementStart
 *
 * @brief Event listener impl for AAMP_EVENT_AD_PLACEMENT_START event
 */
class AAMP_Listener_AdPlacementStart : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_AdPlacementStart Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_AdPlacementStart(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		AdPlacementEventPtr evt = std::dynamic_pointer_cast<AdPlacementEvent>(ev);
		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("adId");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, evt->getAdId().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/**
 * @class AAMP_Listener_AdPlacementEnd
 *
 * @brief Event listener impl for AAMP_EVENT_AD_PLACEMENT_END event
 */
class AAMP_Listener_AdPlacementEnd : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_AdPlacementEnd Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_AdPlacementEnd(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		AdPlacementEventPtr evt = std::dynamic_pointer_cast<AdPlacementEvent>(ev);
		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("adId");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, evt->getAdId().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/**
 * @class AAMP_Listener_AdProgress
 *
 * @brief Event listener impl for AAMP_EVENT_AD_PLACEMENT_PROGRESS event
 */
class AAMP_Listener_AdProgress : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_AdProgress Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_AdProgress(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		AdPlacementEventPtr evt = std::dynamic_pointer_cast<AdPlacementEvent>(ev);
		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("adId");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, evt->getAdId().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/**
 * @class AAMP_Listener_AdPlacementError
 *
 * @brief Event listener impl for AAMP_EVENT_AD_PLACEMENT_ERROR event
 */
class AAMP_Listener_AdPlacementError : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_AdPlacementError Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_AdPlacementError(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		AdPlacementEventPtr evt = std::dynamic_pointer_cast<AdPlacementEvent>(ev);
		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("adId");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, evt->getAdId().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("error");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getErrorCode()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};


/*
 * @class AAMP_Listener_BitrateChanged
 * @brief Event listener impl for AAMP_EVENT_BITRATE_CHANGED event.
 */
class AAMP_Listener_BitrateChanged : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_BitrateChanged Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_BitrateChanged(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		BitrateChangeEventPtr evt = std::dynamic_pointer_cast<BitrateChangeEvent>(ev);
		JSStringRef name;
		name = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, name, JSValueMakeNumber(p_obj->_ctx, evt->getTime()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("bitRate");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, name, JSValueMakeNumber(p_obj->_ctx, evt->getBitrate()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("description");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, name, aamp_CStringToJSValue(p_obj->_ctx, evt->getDescription().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("width");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, name, JSValueMakeNumber(p_obj->_ctx, evt->getWidth()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("height");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, name, JSValueMakeNumber(p_obj->_ctx, evt->getHeight()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("framerate");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, name, JSValueMakeNumber(p_obj->_ctx, evt->getFrameRate()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("position");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, name, JSValueMakeNumber(p_obj->_ctx, evt->getPosition()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("cappedProfile");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, name, JSValueMakeNumber(p_obj->_ctx, evt->getCappedProfileStatus()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("displayWidth");
                JSObjectSetProperty(p_obj->_ctx, jsEventObj, name, JSValueMakeNumber(p_obj->_ctx, evt->getDisplayWidth()), kJSPropertyAttributeReadOnly, NULL);
                JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("displayHeight");
                JSObjectSetProperty(p_obj->_ctx, jsEventObj, name, JSValueMakeNumber(p_obj->_ctx, evt->getDisplayHeight()), kJSPropertyAttributeReadOnly, NULL);
                JSStringRelease(name);

		if(eVIDEOSCAN_UNKNOWN != evt->getScanType())
		{
			name = JSStringCreateWithUTF8CString("progressive");
			JSObjectSetProperty(p_obj->_ctx, jsEventObj, name, JSValueMakeBoolean(p_obj->_ctx, ((eVIDEOSCAN_PROGRESSIVE == evt->getScanType())?true:false)), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(name);
		}

		if((0 != evt->getAspectRatioWidth()) && (0 != evt->getAspectRatioHeight()))
		{
			name = JSStringCreateWithUTF8CString("aspectRatioWidth");
			JSObjectSetProperty(p_obj->_ctx, jsEventObj, name, JSValueMakeNumber(p_obj->_ctx, evt->getAspectRatioWidth()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(name);

			name = JSStringCreateWithUTF8CString("aspectRatioHeight");
			JSObjectSetProperty(p_obj->_ctx, jsEventObj, name, JSValueMakeNumber(p_obj->_ctx, evt->getAspectRatioHeight()), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(name);
		}

	}
};

/**
 * @class AAMP_Listener_Id3Metadata
 * @brief Event listener impl for AAMP_EVENT_ID3_METADATA event.
 */
class AAMP_Listener_Id3Metadata: public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_Id3Metadata Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_Id3Metadata(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] e AAMP event object
	 * @param[out] eventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		ID3MetadataEventPtr evt = std::dynamic_pointer_cast<ID3MetadataEvent>(ev);
		std::vector<uint8_t> data = evt->getMetadata();
		int len = evt->getMetadataSize();
		JSStringRef prop;

		JSValueRef* array = new JSValueRef[len];
		for (int32_t i = 0; i < len; i++)
		{
			array[i] = JSValueMakeNumber(p_obj->_ctx, data[i]);
		}
		prop = JSStringCreateWithUTF8CString("schemeIdUri");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, evt->getSchemeIdUri().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("value");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, evt->getValue().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("timeScale");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getTimeScale()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("presentationTime");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getPresentationTime()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("eventDuration");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getEventDuration()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("id");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getId()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("timestampOffset");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, evt->getTimestampOffset()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("data");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSObjectMakeArray(p_obj->_ctx, len, array, NULL), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
		SAFE_DELETE_ARRAY(array);

		prop = JSStringCreateWithUTF8CString("length");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, len), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};

/**
 * @class AAMP_Listener_Blocked
 * @brief Event listener impl for AAMP_EVENT_BLOCKED event.
 */
class AAMP_Listener_Blocked: public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_Blocked Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_Blocked(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set JS event properties
	 * @param[in] e AAMP event object
	 * @param[out] eventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		BlockedEventPtr evt = std::dynamic_pointer_cast<BlockedEvent>(ev);
		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("reason");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, evt->getReason().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};

/**
 * @class AAMP_Listener_ContentGap
 * @brief Event listener impl for AAMP_EVENT_CONTENT_GAP event.
 */
class AAMP_Listener_ContentGap : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_ContentGap Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_ContentGap(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		ContentGapEventPtr evt = std::dynamic_pointer_cast<ContentGapEvent>(ev);
		JSStringRef prop;

		double time = evt->getTime();
		double durationMs = evt->getDuration();

		prop = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, std::round(time)), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		if (durationMs >= 0)
		{
			prop = JSStringCreateWithUTF8CString("duration");
			JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, JSValueMakeNumber(p_obj->_ctx, (int)durationMs), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(prop);
		}
	}
};


/**
 * @class AAMP_Listener_HTTPResponseHeader
 * @brief Event listener impl for AAMP_EVENT_HTTP_RESPONSE_HEADER event.
 */
class AAMP_Listener_HTTPResponseHeader : public AAMP_JSEventListener
{
public:
	/**
	 * @brief AAMP_Listener_HTTPResponseHeader Constructor
	 * @param[in] aamp instance of PrivAAMPStruct_JS
	 * @param[in] type event type
	 * @param[in] jsCallback callback to be registered as listener
	 */
	AAMP_Listener_HTTPResponseHeader(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
		: AAMP_JSEventListener(obj, type, jsCallback)
	{
	}

	/**
	 * @brief Set properties to JS event object
	 * @param[in] ev AAMP event object
	 * @param[out] jsEventObj JS event object
	 */
	void SetEventProperties(const AAMPEventPtr& ev, JSObjectRef jsEventObj)
	{
		HTTPResponseHeaderEventPtr evt = std::dynamic_pointer_cast<HTTPResponseHeaderEvent>(ev);

		JSStringRef prop;

		prop = JSStringCreateWithUTF8CString("header");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, evt->getHeader().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);

		prop = JSStringCreateWithUTF8CString("response");
		JSObjectSetProperty(p_obj->_ctx, jsEventObj, prop, aamp_CStringToJSValue(p_obj->_ctx, evt->getResponse().c_str()), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(prop);
	}
};

/**
 * @brief AAMP_JSEventListener Constructor
 * @param[in] obj instance of PrivAAMPStruct_JS
 * @param[in] type event type
 * @param[in] jsCallback callback for the event type
 */
AAMP_JSEventListener::AAMP_JSEventListener(PrivAAMPStruct_JS *obj, AAMPEventType type, JSObjectRef jsCallback)
	: p_obj(obj)
	, p_type(type)
	, p_jsCallback(jsCallback)
{
	if (p_jsCallback != NULL)
	{
		JSValueProtect(p_obj->_ctx, p_jsCallback);
	}
}


/**
 * @brief AAMP_JSEventListener Destructor
 */
AAMP_JSEventListener::~AAMP_JSEventListener()
{
	if (p_jsCallback != NULL)
	{
		JSValueUnprotect(p_obj->_ctx, p_jsCallback);
	}
}


/**
 * @brief Callback invoked for dispatching event
 * @param[in] e event object
 */
void AAMP_JSEventListener::Event(const AAMPEventPtr& e)
{
	AAMPEventType evtType = e->getType();
	LOG("%s() type=%d, jsCallback=%p", __FUNCTION__, evtType, p_jsCallback);
	if (evtType < 0 || evtType >= AAMP_MAX_NUM_EVENTS)
	{
		return;
	}

	JSObjectRef event = createNewAAMPJSEvent(p_obj->_ctx, aampPlayer_getNameFromEventType(evtType), false, false);
	if (event)
	{
		JSGlobalContextRef ctx = p_obj->_ctx;
		JSValueProtect(ctx, event);
		SetEventProperties(e, event);
		//send this event through promise callback if an event listener is not registered
		if (p_type == AAMP_EVENT_AD_RESOLVED && p_jsCallback == NULL)
		{
			AdResolvedEventPtr evt = std::dynamic_pointer_cast<AdResolvedEvent>(e);
			std::string adId = evt->getAdId();
			JSObjectRef cbObj = p_obj->getCallbackForAdId(adId);
			if (cbObj != NULL)
			{
				aamp_dispatchEventToJS(ctx, cbObj, event);
				p_obj->removeCallbackForAdId(adId); //promise callbacks are intended for a single-time use for an ad id
			}
			else
			{
				ERROR("AAMP_JSEventListener::%s() No promise callback registered ctx=%p, jsCallback=%p", __FUNCTION__, ctx, cbObj);
			}
		}
		else if (p_jsCallback != NULL)
		{
			aamp_dispatchEventToJS(ctx, p_jsCallback, event);
		}
		else
		{
			ERROR("AAMP_JSEventListener::%s() Callback registered is (%p) for event=%d", __FUNCTION__, p_jsCallback, p_type);
		}
		JSValueUnprotect(ctx, event);
	}
}



/**
 * @brief Adds a JS function as listener for a particular event
 * @param[in] jsObj instance of PrivAAMPStruct_JS
 * @param[in] type event type
 * @param[in] jsCallback callback to be registered as listener
 */
void AAMP_JSEventListener::AddEventListener(PrivAAMPStruct_JS* obj, AAMPEventType type, JSObjectRef jsCallback)
{
	LOG("AAMP_JSEventListener::%s (%p, %d, %p)", __FUNCTION__, obj, type, jsCallback);

	AAMP_JSEventListener* pListener = NULL;

	switch(type)
	{
		case AAMP_EVENT_STATE_CHANGED:
			pListener = new AAMP_Listener_PlaybackStateChanged(obj, type, jsCallback);
			break;
		case AAMP_EVENT_PROGRESS:
			pListener = new AAMP_Listener_ProgressUpdate(obj, type, jsCallback);
			break;
		case AAMP_EVENT_SPEED_CHANGED:
			pListener = new AAMP_Listener_SpeedChanged(obj, type, jsCallback);
			break;
		case AAMP_EVENT_BUFFERING_CHANGED:
			pListener = new AAMP_Listener_BufferingChanged(obj, type, jsCallback);
			break;
		case AAMP_EVENT_TUNE_FAILED:
			pListener = new AAMP_Listener_PlaybackFailed(obj, type, jsCallback);
			break;
		case AAMP_EVENT_MEDIA_METADATA:
			pListener = new AAMP_Listener_MediaMetadata(obj, type, jsCallback);
			break;
		case AAMP_EVENT_SPEEDS_CHANGED:
			pListener = new AAMP_Listener_SpeedsChanged(obj, type, jsCallback);
			break;
		case AAMP_EVENT_SEEKED:
			pListener = new AAMP_Listener_Seeked(obj, type, jsCallback);
			break;
		case AAMP_EVENT_TUNE_PROFILING:
			pListener = new AAMP_Listener_TuneProfiling(obj, type, jsCallback);
			break;
		case AAMP_EVENT_CC_HANDLE_RECEIVED:
			pListener = new AAMP_Listener_CCHandleAvailable(obj, type, jsCallback);
			break;
		case AAMP_EVENT_DRM_METADATA:
			pListener = new AAMP_Listener_DRMMetadata(obj, type, jsCallback);
			break;
		case AAMP_EVENT_REPORT_ANOMALY:
			pListener = new AAMP_Listener_AnomalyReport(obj, type, jsCallback);
			break;
		case AAMP_EVENT_WEBVTT_CUE_DATA:
			pListener = new AAMP_Listener_VTTCueData(obj, type, jsCallback);
			break;
		case AAMP_EVENT_BULK_TIMED_METADATA:
			pListener = new AAMP_Listener_BulkTimedMetadata(obj, type, jsCallback);
			break;
		case AAMP_EVENT_TIMED_METADATA:
			pListener = new AAMP_Listener_TimedMetadata(obj, type, jsCallback);
			break;
		case AAMP_EVENT_HTTP_RESPONSE_HEADER:
			pListener = new AAMP_Listener_HTTPResponseHeader(obj, type, jsCallback);
			break;
		case AAMP_EVENT_BITRATE_CHANGED:
			pListener = new AAMP_Listener_BitrateChanged(obj, type, jsCallback);
			break;
		// Following events are not having payload and hence falls under default case
		// AAMP_EVENT_EOS, AAMP_EVENT_TUNED, AAMP_EVENT_ENTERING_LIVE
		case AAMP_EVENT_AD_RESOLVED:
			pListener = new AAMP_Listener_AdResolved(obj, type, jsCallback);
			break;
		case AAMP_EVENT_AD_RESERVATION_START:
			pListener = new AAMP_Listener_AdReservationStart(obj, type, jsCallback);
			break;
		case AAMP_EVENT_AD_RESERVATION_END:
			pListener = new AAMP_Listener_AdReservationEnd(obj, type, jsCallback);
			break;
		case AAMP_EVENT_AD_PLACEMENT_START:
			pListener = new AAMP_Listener_AdPlacementStart(obj, type, jsCallback);
			break;
		case AAMP_EVENT_AD_PLACEMENT_END:
			pListener = new AAMP_Listener_AdPlacementEnd(obj, type, jsCallback);
			break;
		case AAMP_EVENT_AD_PLACEMENT_PROGRESS:
			pListener = new AAMP_Listener_AdProgress(obj, type, jsCallback);
			break;
		case AAMP_EVENT_AD_PLACEMENT_ERROR:
			pListener = new AAMP_Listener_AdPlacementError(obj, type, jsCallback);
			break;
		case AAMP_EVENT_ID3_METADATA:
			pListener = new AAMP_Listener_Id3Metadata(obj, type, jsCallback);
			break;
		case AAMP_EVENT_BLOCKED:
			pListener = new AAMP_Listener_Blocked(obj, type, jsCallback);
			break;
		case AAMP_EVENT_CONTENT_GAP:
			pListener = new AAMP_Listener_ContentGap(obj, type, jsCallback);
			break;
		// Following events are not having payload and hence falls under default case
		// AAMP_EVENT_EOS, AAMP_EVENT_TUNED, AAMP_EVENT_ENTERING_LIVE,
		// AAMP_EVENT_AUDIO_TRACKS_CHANGED, AAMP_EVENT_TEXT_TRACKS_CHANGED
		default:
			pListener = new AAMP_JSEventListener(obj, type, jsCallback);
			break;
	}

	if (obj->_aamp != NULL)
	{
		obj->_aamp->AddEventListener(type, pListener);
	}

	obj->_listeners.insert({type, (void *)pListener});
}


/**
 * @brief Removes a JS listener for a particular event
 * @param[in] jsObj instance of PrivAAMPStruct_JS
 * @param[in] type event type
 * @param[in] jsCallback callback to be removed as listener
 */
void AAMP_JSEventListener::RemoveEventListener(PrivAAMPStruct_JS* obj, AAMPEventType type, JSObjectRef jsCallback)
{
	LOG("AAMP_JSEventListener::%s (%p, %d, %p)", __FUNCTION__, obj, type, jsCallback);

	if (obj->_listeners.count(type) > 0)
	{

		typedef std::multimap<AAMPEventType, void*>::iterator listenerIter_t;
		std::pair<listenerIter_t, listenerIter_t> range = obj->_listeners.equal_range(type);
		for(listenerIter_t iter = range.first; iter != range.second; )
		{
			AAMP_JSEventListener *listener = (AAMP_JSEventListener *)iter->second;
			if (listener->p_jsCallback == jsCallback)
			{
				if (obj->_aamp != NULL)
				{
					obj->_aamp->RemoveEventListener(iter->first, listener);
				}
				iter = obj->_listeners.erase(iter);
				SAFE_DELETE(listener);
			}
			else
			{
				iter++;
			}
		}
	}
}


/**
 * @brief Remove all JS listeners registered
 * @param[in] jsObj instance of PrivAAMPStruct_JS
 */
void AAMP_JSEventListener::RemoveAllEventListener(PrivAAMPStruct_JS * obj)
{
	LOG("AAMP_JSEventListener::%s obj(%p) listeners remaining(%d)", __FUNCTION__, obj, obj->_listeners.size());

	for (auto listenerIter = obj->_listeners.begin(); listenerIter != obj->_listeners.end();)
	{
		AAMP_JSEventListener *listener = (AAMP_JSEventListener *)listenerIter->second;
		if (obj->_aamp != NULL)
		{
			obj->_aamp->RemoveEventListener(listenerIter->first, listener);
		}
		listenerIter = obj->_listeners.erase(listenerIter);
		SAFE_DELETE(listener);
	}

	obj->_listeners.clear();

}
