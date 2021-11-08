/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2021 RDK Management
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
 * @file fragmentcollector_VIDEOIN.cpp
 * @brief shim for dispatching UVE HDMI input playback
 */
#include "videoin_shim.h"
#include "priv_aamp.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>
#include "AampUtils.h"


#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS

#include <core/core.h>
#include <websocket/websocket.h>

using namespace std;
using namespace WPEFramework;

#define RDKSHELL_CALLSIGN "org.rdk.RDKShell.1"

#define UHD_4K_WIDTH 3840
#define UHD_4K_HEIGHT 2160
#endif
/**
 *   @brief  Initialize a newly created object.
 *   @note   To be implemented by sub classes
 *   @param  tuneType to set type of object.
 *   @retval eAAMPSTATUS_OK
 */
AAMPStatusType StreamAbstractionAAMP_VIDEOIN::Init(TuneType tuneType)
{
	AAMPLOG_WARN("%s Function not implemented",mName.c_str());
}

AAMPStatusType StreamAbstractionAAMP_VIDEOIN::InitHelper(TuneType tuneType)
{
	AAMPStatusType retval = eAAMPSTATUS_OK;
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
        RegisterAllEvents();
#endif
	return retval;
}

/**
 * @brief StreamAbstractionAAMP_VIDEOIN Constructor
 * @param aamp pointer to PrivateInstanceAAMP object associated with player
 * @param seek_pos Seek position
 * @param rate playback rate
 */
StreamAbstractionAAMP_VIDEOIN::StreamAbstractionAAMP_VIDEOIN( const std::string name, const std::string callSign, AampLogManager *logObj,  class PrivateInstanceAAMP *aamp,double seek_pos, float rate)
                               : mName(name),
                               mRegisteredEvents(),
                               StreamAbstractionAAMP(logObj, aamp),
                               mTuned(false),
                               videoInputPort(-1)
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
                               ,thunderAccessObj(callSign, logObj),
				thunderRDKShellObj(RDKSHELL_CALLSIGN, logObj)
#endif
{
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
	AAMPLOG_WARN("%s Constructor",mName.c_str());
    thunderAccessObj.ActivatePlugin();
#endif
}

/**
 * @brief StreamAbstractionAAMP_VIDEOIN Destructor
 */
StreamAbstractionAAMP_VIDEOIN::~StreamAbstractionAAMP_VIDEOIN()
{
	AAMPLOG_WARN("%s destructor",mName.c_str());
	for (auto const& evtName : mRegisteredEvents) {
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
		thunderAccessObj.UnSubscribeEvent(_T(evtName));
#endif
	}
	mRegisteredEvents.clear();
}

/**
 *   @brief  Starts streaming.
 */
void StreamAbstractionAAMP_VIDEOIN::Start(void)
{
	AAMPLOG_WARN("%s Function not implemented",mName.c_str());
}

/**
 *   @brief  calls start on video in specified by port and method name
 */

void StreamAbstractionAAMP_VIDEOIN::StartHelper(int port, const std::string & methodName)
{
	AAMPLOG_WARN("%s method:%s port:%d",mName.c_str(),methodName.c_str(),port);

	videoInputPort = port;
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
		JsonObject param;
		JsonObject result;
		param["portId"] = videoInputPort;
		thunderAccessObj.InvokeJSONRPC(methodName, param, result);
#endif
}

/**
*   @brief  Stops streaming.
*/
void StreamAbstractionAAMP_VIDEOIN::StopHelper(const std::string & methodName)
{
	if( videoInputPort>=0 )
	{
		AAMPLOG_WARN("%s method:%s",mName.c_str(),methodName.c_str());

#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
    		JsonObject param;
    		JsonObject result;
    		thunderAccessObj.InvokeJSONRPC(methodName, param, result);
#endif

		videoInputPort = -1;
	}
}

/**
*   @brief  Stops streaming.
*/
void StreamAbstractionAAMP_VIDEOIN::Stop(bool clearChannelData)
{
	AAMPLOG_WARN("%s Function not implemented",mName.c_str());
}

bool StreamAbstractionAAMP_VIDEOIN::GetScreenResolution(int & screenWidth, int & screenHeight)
{
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
	return false;
#else
	JsonObject param;
	JsonObject result;
	bool bRetVal = false;

	if( thunderRDKShellObj.InvokeJSONRPC("getScreenResolution", param, result) )
	{
		screenWidth = result["w"].Number();
		screenHeight = result["h"].Number();
		AAMPLOG_INFO("%s screenWidth:%d screenHeight:%d ",mName.c_str(),screenWidth, screenHeight);
		bRetVal = true;
	}
	return bRetVal;
#endif
}

/**
 * @brief SetVideoRectangle sets the position coordinates (x,y) & size (w,h)
 *
 * @param[in] x,y - position coordinates of video rectangle
 * @param[in] wxh - width & height of video rectangle
 */

void StreamAbstractionAAMP_VIDEOIN::SetVideoRectangle(int x, int y, int w, int h)
{
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
	int screenWidth = 0;
	int screenHeight = 0;
	float width_ratio = 1.0, height_ratio = 1.0;
	if(GetScreenResolution(screenWidth,screenHeight))
        {
		/*Temporary fix as hdmiin/composite in expects the scaling in 4k space*/
		if((0 != screenWidth) && (0 != screenHeight))
		{
			width_ratio = UHD_4K_WIDTH / screenWidth;
			height_ratio = UHD_4K_HEIGHT / screenHeight;
		}
	}

	JsonObject param;
	JsonObject result;
	param["x"] = (int) (x * width_ratio);
	param["y"] = (int) (y * height_ratio);
	param["w"] = (int) (w * width_ratio);
	param["h"] = (int) (h * height_ratio);
	AAMPLOG_WARN("%s x:%d y:%d w:%d h:%d w-ratio:%f h-ratio:%f",mName.c_str(),x,y,w,h,width_ratio,height_ratio);
	thunderAccessObj.InvokeJSONRPC("setVideoRectangle", param, result);
#endif
}

/**
 * @brief Stub implementation
 */
void StreamAbstractionAAMP_VIDEOIN::DumpProfiles(void)
{ // STUB
	AAMPLOG_WARN("%s Function not implemented",mName.c_str());
}

/**
 * @brief Get output format of stream.
 *
 * @param[out]  primaryOutputFormat - format of primary track
 * @param[out]  audioOutputFormat - format of audio track
 * @param[out]  auxAudioOutputFormat - format of aux track
 */
void StreamAbstractionAAMP_VIDEOIN::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat)
{ // STUB
	AAMPLOG_WARN("%s ",mName.c_str());
    primaryOutputFormat = FORMAT_INVALID;
    audioOutputFormat = FORMAT_INVALID;
    //auxAudioOutputFormat = FORMAT_INVALID;
}

/**
 *   @brief Return MediaTrack of requested type
 *
 *   @param[in]  type - track type
 *   @retval MediaTrack pointer.
 */
MediaTrack* StreamAbstractionAAMP_VIDEOIN::GetMediaTrack(TrackType type)
{ // STUB
	AAMPLOG_WARN("%s ",mName.c_str());
    return NULL;
}

/**
 * @brief Get current stream position.
 *
 * @retval current position of stream.
 */
double StreamAbstractionAAMP_VIDEOIN::GetStreamPosition()
{ // STUB
	AAMPLOG_WARN("%s ",mName.c_str());
    return 0.0;
}

/**
 *   @brief Get stream information of a profile from subclass.
 *
 *   @param[in]  idx - profile index.
 *   @retval stream information corresponding to index.
 */
StreamInfo* StreamAbstractionAAMP_VIDEOIN::GetStreamInfo(int idx)
{ // STUB
	AAMPLOG_WARN("%s ",mName.c_str());
    return NULL;
}

/**
 *   @brief  Get PTS of first sample.
 *
 *   @retval PTS of first sample
 */
double StreamAbstractionAAMP_VIDEOIN::GetFirstPTS()
{ // STUB
	AAMPLOG_WARN("%s ",mName.c_str());
    return 0.0;
}

/**
 *   @brief  Get Start time PTS of first sample.
 *
 *   @retval start time of first sample
 */
double StreamAbstractionAAMP_VIDEOIN::GetStartTimeOfFirstPTS()
{ // STUB
	AAMPLOG_WARN("%s ",mName.c_str());
    return 0.0;
}

double StreamAbstractionAAMP_VIDEOIN::GetBufferedDuration()
{ // STUB
	AAMPLOG_WARN("%s ",mName.c_str());
	return -1.0;
}

bool StreamAbstractionAAMP_VIDEOIN::IsInitialCachingSupported()
{
	AAMPLOG_WARN("%s ",mName.c_str());
	return false;
}

/**
 * @brief Get index of profile corresponds to bandwidth
 * @param[in] bitrate Bitrate to lookup profile
 * @retval profile index
 */
int StreamAbstractionAAMP_VIDEOIN::GetBWIndex(long bitrate)
{
	AAMPLOG_WARN("%s ",mName.c_str());
    return 0;
}

/**
 * @brief To get the available video bitrates.
 * @ret available video bitrates
 */
std::vector<long> StreamAbstractionAAMP_VIDEOIN::GetVideoBitrates(void)
{ // STUB
	AAMPLOG_WARN("%s ",mName.c_str());
    return std::vector<long>();
}

/*
* @brief Gets Max Bitrate avialable for current playback.
* @ret long MAX video bitrates
*/
long StreamAbstractionAAMP_VIDEOIN::GetMaxBitrate()
{ // STUB
	AAMPLOG_WARN("%s ",mName.c_str());
    return 0;
}

/**
 * @brief To get the available audio bitrates.
 * @ret available audio bitrates
 */
std::vector<long> StreamAbstractionAAMP_VIDEOIN::GetAudioBitrates(void)
{ // STUB
	AAMPLOG_WARN("%s ",mName.c_str());
    return std::vector<long>();
}

/**
*   @brief  Stops injecting fragments to StreamSink.
*/
void StreamAbstractionAAMP_VIDEOIN::StopInjection(void)
{ // STUB - discontinuity related
	AAMPLOG_WARN("%s ",mName.c_str());
}

/**
*   @brief  Start injecting fragments to StreamSink.
*/
void StreamAbstractionAAMP_VIDEOIN::StartInjection(void)
{ // STUB - discontinuity related
	AAMPLOG_WARN("%s ",mName.c_str());
}



/**
 *   @brief  Registers  Event to input plugin and to mRegisteredEvents list for later use.
 *   @param[in] eventName : Event name
 *  @param[in] functionHandler : Event funciton pointer
 */
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
void StreamAbstractionAAMP_VIDEOIN::RegisterEvent (string eventName, std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> functionHandler)
{
	bool bSubscribed;
	bSubscribed = thunderAccessObj.SubscribeEvent(_T(eventName), functionHandler);
	if(bSubscribed)
	{
		mRegisteredEvents.push_back(eventName);
	}
}

/**
 *   @brief  Registers all Events to input plugin
 */
void StreamAbstractionAAMP_VIDEOIN::RegisterAllEvents ()
{
	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> inputStatusChangedMethod = std::bind(&StreamAbstractionAAMP_VIDEOIN::OnInputStatusChanged, this, std::placeholders::_1);

	RegisterEvent("onInputStatusChanged",inputStatusChangedMethod);

	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> signalChangedMethod = std::bind(&StreamAbstractionAAMP_VIDEOIN::OnSignalChanged, this, std::placeholders::_1);

	RegisterEvent("onSignalChanged",signalChangedMethod);
}

/**
 *   @brief  Gets  onSignalChanged and translates into aamp events
 */
void StreamAbstractionAAMP_VIDEOIN::OnInputStatusChanged(const JsonObject& parameters)
{
	std::string message;
	parameters.ToString(message);
	AAMPLOG_WARN("%s",message.c_str());

	std::string strStatus = parameters["status"].String();

	if(0 == strStatus.compare("started"))
	{
		if(!mTuned){
			aamp->SendTunedEvent(false);
			mTuned = true;
			aamp->LogFirstFrame();
			aamp->LogTuneComplete();
		}
		aamp->SetState(eSTATE_PLAYING);
	}
	else if(0 == strStatus.compare("stopped"))
	{
		aamp->SetState(eSTATE_STOPPED);
	}
}

/**
 *   @brief  Gets  onSignalChanged and translates into aamp events
 */
void StreamAbstractionAAMP_VIDEOIN::OnSignalChanged (const JsonObject& parameters)
{
	std::string message;
	parameters.ToString(message);
	AAMPLOG_WARN("%s",message.c_str());

	std::string strReason;
	std::string strStatus = parameters["signalStatus"].String();

	if(0 == strStatus.compare("noSignal"))
	{
		strReason = "NO_SIGNAL";
	}
	else if (0 == strStatus.compare("unstableSignal"))
	{
		strReason = "UNSTABLE_SIGNAL";
	}
	else if (0 == strStatus.compare("notSupportedSignal"))
	{
		strReason = "NOT_SUPPORTED_SIGNAL";
	}
	else if (0 == strStatus.compare("stableSignal"))
	{
		// Only Generate after started event, this can come after temp loss of signal.
		if(mTuned){
			aamp->SetState(eSTATE_PLAYING);
		}
	}

	if(!strReason.empty())
	{
		AAMPLOG_WARN("GENERATING BLOCKED EVNET :%s",strReason.c_str());
		aamp->SendBlockedEvent(strReason);
	}
}
#endif

