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
 * @file ota_shim.cpp
 * @brief shim for dispatching UVE OTA ATSC playback
 */

#include "AampUtils.h"
#include "ota_shim.h"
#include "priv_aamp.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>

#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS

#include <core/core.h>
#include <websocket/websocket.h>

using namespace std;
using namespace WPEFramework;
#endif

#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS

#define MEDIAPLAYER_CALLSIGN "org.rdk.MediaPlayer.1"
#define MEDIASETTINGS_CALLSIGN "org.rdk.MediaSettings.1"
#define APP_ID "MainPlayer"

#define RDKSHELL_CALLSIGN "org.rdk.RDKShell.1"

ATSCGlobalSettings gATSCSettings;

void StreamAbstractionAAMP_OTA::onPlayerStatusHandler(const JsonObject& parameters) {
	std::string message;
	parameters.ToString(message);

	JsonObject playerData = parameters[APP_ID].Object();
	AAMPLOG_TRACE( "[OTA_SHIM]Received event : message : %s ",  message.c_str());
	/* For detailed event data, we can print or use details like
	   playerData["locator"].String(), playerData["length"].String(), playerData["position"].String() */

	std::string currState = playerData["playerStatus"].String();
	bool blockedReasonChanged = false;
	std::string reason("");
	if(0 == currState.compare("BLOCKED"))
	{
		reason = playerData["blockedReason"].String();
		if(0 != reason.compare(prevBlockedReason))
		{
			blockedReasonChanged = true;
		}
	}

	if(0 != prevState.compare(currState) || blockedReasonChanged)
	{
		PrivAAMPState state = eSTATE_IDLE;
		prevBlockedReason.clear();
		AAMPLOG_WARN( "[OTA_SHIM] State changed from %s to %s ",  prevState.c_str(), currState.c_str());
		prevState = currState;
		if(0 == currState.compare("PENDING"))
		{
			state = eSTATE_PREPARING;
		}else if((0 == currState.compare("BLOCKED")) && (0 != reason.compare("NOT_BLOCKED")))
		{
			std::string ratingString;
			JsonObject ratingObj = playerData["rating"].Object();
			ratingObj.ToString(ratingString);
			AAMPLOG_WARN( "[OTA_SHIM] Received BLOCKED event from player with REASON: %s Current Ratings: %s",  reason.c_str(), ratingString.c_str());

			aamp->SendAnomalyEvent(ANOMALY_WARNING,"BLOCKED REASON:%s", reason.c_str());
			aamp->SendBlockedEvent(reason);
			state = eSTATE_BLOCKED;
			prevBlockedReason = reason;
		}else if(0 == currState.compare("PLAYING"))
		{
			if(!tuned){
				aamp->SendTunedEvent(false);
				/* For consistency, during first tune, first move to
				 PREPARED state to match normal IPTV flow sequence */
				aamp->SetState(eSTATE_PREPARED);
				tuned = true;
				aamp->LogFirstFrame();
				aamp->LogTuneComplete();
			}
			std::string ratingString;
			JsonObject ratingObj = playerData["rating"].Object();
			ratingObj.ToString(ratingString);
			AAMPLOG_WARN( "[OTA_SHIM] PLAYING STATE Current Ratings : %s", ratingString.c_str());
			state = eSTATE_PLAYING;
		}else if(0 == currState.compare("DONE"))
		{
			if(tuned){
				tuned = false;
			}
			state = eSTATE_COMPLETE;
		}else
		{
			if(0 == currState.compare("IDLE"))
			{
				aamp->SendAnomalyEvent(ANOMALY_WARNING, "ATSC Tuner Idle");
			}else{
				/* Currently plugin lists only "IDLE","ERROR","PROCESSING","PLAYING"&"DONE" */
				AAMPLOG_INFO( "[OTA_SHIM] Unsupported state change!");
			}
			/* Need not set a new state hence returning */
			return;
		}
		aamp->SetState(state);
	}

	if((0 == currState.compare("PLAYING")) || (0 == currState.compare("BLOCKED")) &&  0 == reason.compare("SERVICE_PIN_LOCKED"))
	{
		if(PopulateMetaData(playerData))
		{
			SendMediaMetadataEvent();

			// genereate Notify bitrate event if video w/h is changed
			// this is lagacy event used by factory test app to get video info
			if( (miPrevmiVideoWidth != miVideoWidth) ||  (miPrevmiVideoHeight != miVideoHeight) )
			{
				miPrevmiVideoWidth = miVideoWidth;
				miPrevmiVideoHeight = miVideoHeight;
				aamp->NotifyBitRateChangeEvent(mVideoBitrate, eAAMP_BITRATE_CHANGE_BY_OTA, miVideoWidth, miVideoHeight, mFrameRate, 0, false, mVideoScanType, mAspectRatioWidth, mAspectRatioHeight);
			}
		}
	}

}

/**
*   @brief  reads metadata properties from player status object and return true if any of data is changed
*/
bool StreamAbstractionAAMP_OTA::PopulateMetaData(const JsonObject& playerData)
{
	bool isDataChanged = false;
	std::string ratingString;
	JsonObject ratingObj = playerData["rating"].Object();
	ratingObj.ToString(ratingString);
	
	if( mPCRating != ratingString )
	{
		AAMPLOG_INFO( "[OTA_SHIM]ratings changed : old:%s new:%s ", mPCRating.c_str(), ratingString.c_str());
		mPCRating = ratingString;
		isDataChanged = true;
	}

	int tempSSI = playerData["ssi"].Number();

	if(tempSSI != mSsi)
	{
		AAMPLOG_INFO( "[OTA_SHIM]SSI changed : old:%d new:%d ", mSsi, tempSSI);
		mSsi = tempSSI;
		isDataChanged = true;
	}

	/* Video info   */
	JsonObject videoInfoObj = playerData["videoInfo"].Object();

	VideoScanType tempScanType = (videoInfoObj["progressive"].Boolean() ? eVIDEOSCAN_PROGRESSIVE : eVIDEOSCAN_INTERLACED);
	if(mVideoScanType != tempScanType)
	{
		AAMPLOG_INFO( "[OTA_SHIM]Scan type changed : old:%d new:%d ", mVideoScanType, tempScanType);
		isDataChanged = true;
		mVideoScanType = tempScanType;
	}

	float tempframeRate = 0.0;
	float frameRateN = static_cast<float> (videoInfoObj["frameRateN"].Number());
	float frameRateD = static_cast<float> (videoInfoObj["frameRateD"].Number());
	if((0 != frameRateN) && (0 != frameRateD))
	{
		tempframeRate = frameRateN / frameRateD;

		if( mFrameRate != tempframeRate)
		{
			AAMPLOG_INFO( "[OTA_SHIM] mFrameRate changed : old:%f new:%f ", mFrameRate, tempframeRate);
			isDataChanged = true;
			mFrameRate = tempframeRate;
		}
	}

	int tempAspectRatioWidth = videoInfoObj["aspectRatioWidth"].Number();
	if( tempAspectRatioWidth != mAspectRatioWidth)
	{
		isDataChanged = true;
		AAMPLOG_INFO( "[OTA_SHIM] mAspectRatioWidth changed : old:%d new:%d ", mAspectRatioWidth, tempAspectRatioWidth);
		mAspectRatioWidth = tempAspectRatioWidth;
	}

	int tempAspectRatioHeight = videoInfoObj["aspectRatioHeight"].Number();
	if( mAspectRatioHeight != tempAspectRatioHeight)
	{
		AAMPLOG_INFO( "[OTA_SHIM] tempAspectRatioHeight  : old:%d new:%d ", mAspectRatioHeight, tempAspectRatioHeight);
		isDataChanged = true;
		mAspectRatioHeight = tempAspectRatioHeight;
	}

	int tempVideoWidth =  videoInfoObj["width"].Number();
	if( miVideoWidth != tempVideoWidth)
	{
		AAMPLOG_INFO( "[OTA_SHIM] miVideoWidth  : old:%d new:%d ",  miVideoWidth, tempVideoWidth);
		miVideoWidth = tempVideoWidth;
		isDataChanged = true;
	}

	int tempVideoHeight =  videoInfoObj["height"].Number();
	if( miVideoHeight != tempVideoHeight)
	{
		AAMPLOG_INFO( "[OTA_SHIM] miVideoHeight  : old:%d new:%d ",  miVideoHeight, tempVideoHeight);
		miVideoHeight = tempVideoHeight;
		isDataChanged = true;
	}

	std::string tempVideoCodec = videoInfoObj["codec"].String();
	if(0 != mVideoCodec.compare(tempVideoCodec))
	{
		AAMPLOG_INFO( "[OTA_SHIM] mVideoCodec : old:%s new:%s ",  mVideoCodec.c_str(), tempVideoCodec.c_str());
		mVideoCodec = tempVideoCodec;
		isDataChanged = true;
	}

	mHdrType = videoInfoObj["hdrType"].String();

	/* Audio Info   */
	JsonObject audioInfoObj = playerData["audioInfo"].Object();

	std::string tempAudioCodec = audioInfoObj["codec"].String();
	if(0 != mAudioCodec.compare(tempAudioCodec))
	{
		AAMPLOG_INFO( "[OTA_SHIM] tempAudioCodec : old:%s new:%s ",  mAudioCodec.c_str(), mAudioCodec.c_str());
		mAudioCodec = tempAudioCodec;
		isDataChanged = true;
	}

	std::string tempAudioMixType = audioInfoObj["mixType"].String();
	if(0 != mAudioMixType.compare(tempAudioMixType))
	{
		AAMPLOG_INFO( "[OTA_SHIM] tempAudioMixType : old:%s new:%s ",  mAudioMixType.c_str(), tempAudioMixType.c_str());
		mAudioMixType = tempAudioMixType;
		isDataChanged = true;
	}

	bool tempIsAtmos =  audioInfoObj["isAtmos"].Boolean();

	if( mIsAtmos != tempIsAtmos)
	{
		AAMPLOG_INFO( "[OTA_SHIM] -- mIsAtmos  : old:%d new:%d ",  mIsAtmos, tempIsAtmos);
		mIsAtmos = tempIsAtmos;
		isDataChanged = true;
	}

	if( isDataChanged )
	{
		mVideoBitrate = videoInfoObj["bitrate"].Number();
		mAudioBitrate = audioInfoObj["bitrate"].Number();
	}

	return isDataChanged;
}

void StreamAbstractionAAMP_OTA::SendMediaMetadataEvent()
{
	if(aamp->IsEventListenerAvailable(AAMP_EVENT_MEDIA_METADATA))
	{
		MediaMetadataEventPtr event = std::make_shared<MediaMetadataEvent>(-1/*duration*/, miVideoWidth, miVideoHeight, false/*hasDrm*/,true/*isLive*/, ""/*drmtype*/, -1/*programStartTime*/);

		// This is video bitrate
		event->addBitrate(mVideoBitrate);
		event->addSupportedSpeed(1);
		event->SetVideoMetaData(mFrameRate,mVideoScanType,mAspectRatioWidth,mAspectRatioHeight, mVideoCodec,  mHdrType, mPCRating,mSsi);
		event->SetAudioMetaData(mAudioCodec,mAudioMixType,mIsAtmos);
		event->addAudioBitrate(mAudioBitrate);
		aamp->SendEvent(event,AAMP_EVENT_ASYNC_MODE);
	}
}

#endif

/**
 *   @brief  Initialize a newly created object.
 *   @note   To be implemented by sub classes
 *   @param  tuneType to set type of object.
 *   @retval true on success
 *   @retval false on failure
 */
AAMPStatusType StreamAbstractionAAMP_OTA::Init(TuneType tuneType)
{
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
    logprintf( "[OTA_SHIM]Inside %s CURL ACCESS", __FUNCTION__ );
    AAMPStatusType retval = eAAMPSTATUS_OK;
#else
    AAMPLOG_INFO( "[OTA_SHIM]Inside" );
    prevState = "IDLE";
	
    //initialize few veriables, it will invalidate mediametadata/Notifybitrate events
    miVideoWidth = 0;
    miVideoHeight = 0;
    miPrevmiVideoWidth  = 0;
    miPrevmiVideoHeight  = 0;

    prevBlockedReason = "";
    tuned = false;

    aamp->SetContentType("OTA");

    thunderAccessObj.ActivatePlugin();
    mediaSettingsObj.ActivatePlugin();
    std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> actualMethod = std::bind(&StreamAbstractionAAMP_OTA::onPlayerStatusHandler, this, std::placeholders::_1);

    //mEventSubscribed flag updated for tracking event subscribtion
    mEventSubscribed = thunderAccessObj.SubscribeEvent(_T("onPlayerStatus"), actualMethod);

    AAMPStatusType retval = eAAMPSTATUS_OK;

    //activate RDK Shell - not required as this plugin is already activated
    // thunderRDKShellObj.ActivatePlugin();

#endif
    return retval;
}

/**
 * @brief StreamAbstractionAAMP_OTA Constructor
 * @param aamp pointer to PrivateInstanceAAMP object associated with player
 * @param seek_pos Seek position
 * @param rate playback rate
 */
StreamAbstractionAAMP_OTA::StreamAbstractionAAMP_OTA(AampLogManager *logObj, class PrivateInstanceAAMP *aamp,double seek_pos, float rate)
                          : StreamAbstractionAAMP(logObj, aamp)
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
                            , tuned(false),mEventSubscribed(false),
                            thunderAccessObj(MEDIAPLAYER_CALLSIGN, logObj),
                            mediaSettingsObj(MEDIASETTINGS_CALLSIGN, logObj),
                            thunderRDKShellObj(RDKSHELL_CALLSIGN,logObj),
                            mPCRating(),mSsi(-1),mFrameRate(0),mVideoScanType(eVIDEOSCAN_UNKNOWN),mAspectRatioWidth(0),mAspectRatioHeight(0),
                            mVideoCodec(),mHdrType(),mAudioBitrate(0),mAudioCodec(),mAudioMixType(),mIsAtmos(false),
                            miVideoWidth(0),miVideoHeight(0),miPrevmiVideoWidth(0),miPrevmiVideoHeight(0)
#endif
{ // STUB
}

/**
 * @brief StreamAbstractionAAMP_OTA Destructor
 */
StreamAbstractionAAMP_OTA::~StreamAbstractionAAMP_OTA()
{
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
        /*
        Request : {"jsonrpc":"2.0", "id":3, "method": "org.rdk.MediaPlayer.1.release", "params":{ "id":"MainPlayer", "tag" : "MyApp"} }
        Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true } }
        */
        std::string id = "3";
        std:: string response = aamp_PostJsonRPC(id, "org.rdk.MediaPlayer.1.release", "{\"id\":\"MainPlayer\",\"tag\" : \"MyApp\"}");
        logprintf( "StreamAbstractionAAMP_OTA:%s:%d response '%s'\n", __FUNCTION__, __LINE__, response.c_str());
#else
        JsonObject param;
        JsonObject result;
	param["id"] = APP_ID;
	param["tag"] = "MyApp";
        thunderAccessObj.InvokeJSONRPC("release", param, result);

	// unsubscribing only if subscribed
	if (mEventSubscribed)
	{
		thunderAccessObj.UnSubscribeEvent(_T("onPlayerStatus"));
		mEventSubscribed = false;
	}
	else
	{
		AAMPLOG_WARN("[OTA_SHIM]OTA Destructor finds Player Status Event not Subscribed !! ");
	}

	AAMPLOG_INFO("[OTA_SHIM]StreamAbstractionAAMP_OTA Destructor called !! ");
#endif
}

/**
 *   @brief  Starts streaming.
 */
void StreamAbstractionAAMP_OTA::Start(void)
{
	std::string id = "3";
        std::string response;
	const char *display = getenv("WAYLAND_DISPLAY");
	std::string waylandDisplay;
	if( display )
	{
		waylandDisplay = display;
		logprintf( "WAYLAND_DISPLAY: '%s'\n", display );
	}
	else
	{
		logprintf( "WAYLAND_DISPLAY: NULL!\n" );
	}
	std::string url = aamp->GetManifestUrl();
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
        logprintf( "[OTA_SHIM]Inside %s CURL ACCESS\n", __FUNCTION__ );
	/*
	Request : {"jsonrpc": "2.0","id": 4,"method": "Controller.1.activate", "params": { "callsign": "org.rdk.MediaPlayer" }}
	Response : {"jsonrpc": "2.0","id": 4,"result": null}
	*/
	response = aamp_PostJsonRPC(id, "Controller.1.activate", "{\"callsign\":\"org.rdk.MediaPlayer\"}" );
        logprintf( "StreamAbstractionAAMP_OTA:%s:%d response '%s'\n", __FUNCTION__, __LINE__, response.c_str());
        response.clear();
	/*
	Request : {"jsonrpc":"2.0", "id":3, "method":"org.rdk.MediaPlayer.1.create", "params":{ "id" : "MainPlayer", "tag" : "MyApp"} }
	Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true } }
	*/
	response = aamp_PostJsonRPC(id, "org.rdk.MediaPlayer.1.create", "{\"id\":\"MainPlayer\",\"tag\":\"MyApp\"}");
        logprintf( "StreamAbstractionAAMP_OTA:%s:%d response '%s'\n", __FUNCTION__, __LINE__, response.c_str());
        response.clear();
	// inform (MediaRite) player instance on which wayland display it should draw the video. This MUST be set before load/play is called.
	/*
	Request : {"jsonrpc":"2.0", "id":3, "method":"org.rdk.MediaPlayer.1.setWaylandDisplay", "params":{"id" : "MainPlayer","display" : "westeros-123"} }
	Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true} }
	*/
	response = aamp_PostJsonRPC( id, "org.rdk.MediaPlayer.1.setWaylandDisplay", "{\"id\":\"MainPlayer\",\"display\":\"" + waylandDisplay + "\"}" );
        logprintf( "StreamAbstractionAAMP_OTA:%s:%d response '%s'\n", __FUNCTION__, __LINE__, response.c_str());
        response.clear();
	/*
	Request : {"jsonrpc":"2.0", "id":3, "method": "org.rdk.MediaPlayer.1.load", "params":{ "id":"MainPlayer", "url":"live://...", "autoplay": true} }
	Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true } }
	*/
	response = aamp_PostJsonRPC(id, "org.rdk.MediaPlayer.1.load","{\"id\":\"MainPlayer\",\"url\":\""+url+"\",\"autoplay\":true}" );
        logprintf( "StreamAbstractionAAMP_OTA:%s:%d response '%s'\n", __FUNCTION__, __LINE__, response.c_str());
        response.clear();
	/*
	Request : {"jsonrpc":"2.0", "id":3, "method": "org.rdk.MediaPlayer.1.play", "params":{ "id":"MainPlayer"} }
	Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true } }
	*/
  
	// below play request harmless, but not needed, given use of autoplay above
	// response = aamp_PostJsonRPC(id, "org.rdk.MediaPlayer.1.play", "{\"id\":\"MainPlayer\"}");
        // logprintf( "StreamAbstractionAAMP_OTA:%s:%d response '%s'\n", __FUNCTION__, __LINE__, response.c_str());

#else
	AAMPLOG_INFO( "[OTA_SHIM] url : %s ", url.c_str());
	JsonObject result;

	SetPreferredAudioLanguages();

	JsonObject createParam;
	createParam["id"] = APP_ID;
	createParam["tag"] = "MyApp";
        thunderAccessObj.InvokeJSONRPC("create", createParam, result);

	JsonObject displayParam;
	displayParam["id"] = APP_ID;
	displayParam["display"] = waylandDisplay;
        thunderAccessObj.InvokeJSONRPC("setWaylandDisplay", displayParam, result);

	JsonObject loadParam;
	loadParam["id"] = APP_ID;
	loadParam["url"] = url;
	loadParam["autoplay"] = true;
	thunderAccessObj.InvokeJSONRPC("load", loadParam, result);

	// below play request harmless, but not needed, given use of autoplay above
	//JsonObject playParam;
	//playParam["id"] = APP_ID;
        //thunderAccessObj.InvokeJSONRPC("play", playParam, result);
#endif
}

/**
*   @brief  Stops streaming.
*/
void StreamAbstractionAAMP_OTA::Stop(bool clearChannelData)
{
	/*StreamAbstractionAAMP::Stop is being called twice
	  PrivateInstanceAAMP::Stop calls Stop with clearChannelData set to true
	  PrivateInstanceAAMP::TeardownStream calls Stop with clearChannelData set to false
	  Hence avoiding the Stop with clearChannelData set to false*/
	if(!clearChannelData)
		return;

#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
        /*
        Request : {"jsonrpc":"2.0", "id":3, "method": "org.rdk.MediaPlayer.1.stop", "params":{ "id":"MainPlayer"} }
        Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true } }
        */
        std::string id = "3";
        std::string response = aamp_PostJsonRPC(id, "org.rdk.MediaPlayer.1.stop", "{\"id\":\"MainPlayer\"}");
        logprintf( "StreamAbstractionAAMP_OTA:%s:%d response '%s'\n", __FUNCTION__, __LINE__, response.c_str());
#else
        JsonObject param;
        JsonObject result;
        param["id"] = APP_ID;
        thunderAccessObj.InvokeJSONRPC("stop", param, result);
#endif
}

#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
bool StreamAbstractionAAMP_OTA::GetScreenResolution(int & screenWidth, int & screenHeight)
{
	 JsonObject param;
	 JsonObject result;
	 bool bRetVal = false;

	 if( thunderRDKShellObj.InvokeJSONRPC("getScreenResolution", param, result) )
	 {
		 screenWidth = result["w"].Number();
		 screenHeight = result["h"].Number();
		 AAMPLOG_INFO( "StreamAbstractionAAMP_OTA: screenWidth:%d screenHeight:%d  ",screenWidth, screenHeight);
		 bRetVal = true;
	 }
	 return bRetVal;
}
#endif

/**
 * @brief SetVideoRectangle sets the position coordinates (x,y) & size (w,h)
 *
 * @param[in] x,y - position coordinates of video rectangle
 * @param[in] wxh - width & height of video rectangle
 */
void StreamAbstractionAAMP_OTA::SetVideoRectangle(int x, int y, int w, int h)
{
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
        /*
        Request : {"jsonrpc":"2.0", "id":3, "method": "org.rdk.MediaPlayer.1.setVideoRectangle", "params":{ "id":"MainPlayer", "x":0, "y":0, "w":1280, "h":720} }
        Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true } }
        */
        std::string id = "3";
        std::string response = aamp_PostJsonRPC(id, "org.rdk.MediaPlayer.1.setVideoRectangle", "{\"id\":\"MainPlayer\", \"x\":" + to_string(x) + ", \"y\":" + to_string(y) + ", \"w\":" + to_string(w) + ", \"h\":" + std::to_string(h) + "}");
        logprintf( "StreamAbstractionAAMP_OTA:%s:%d response '%s'\n", __FUNCTION__, __LINE__, response.c_str());
#else
        JsonObject param;
        JsonObject result;
        param["id"] = APP_ID;
        param["x"] = x;
        param["y"] = y;
        param["w"] = w;
        param["h"] = h;
        int screenWidth = 0;
        int screenHeight = 0;
        if(GetScreenResolution(screenWidth,screenHeight))
        {
		JsonObject meta;
		meta["resWidth"] = screenWidth;
		meta["resHeight"] = screenHeight;
		param["meta"] = meta;
        }

        thunderAccessObj.InvokeJSONRPC("setVideoRectangle", param, result);
#endif
}

/**
 * @brief NotifyAudioTrackChange To notify audio track change.Currently not used
 * as mediaplayer does not have support yet.
 *
 * @param[in] tracks - updated audio track info
 * @param[in]
 */
void StreamAbstractionAAMP_OTA::NotifyAudioTrackChange(const std::vector<AudioTrackInfo> &tracks)
{
    if ((0 != mAudioTracks.size()) && (tracks.size() != mAudioTracks.size()))
    {
        aamp->NotifyAudioTracksChanged();
    }
    return;
}

/**
 *   @brief Get the list of available audio tracks
 *
 *   @return std::vector<AudioTrackInfo> List of available audio tracks
 */
std::vector<AudioTrackInfo> & StreamAbstractionAAMP_OTA::GetAvailableAudioTracks()
{
    if (mAudioTrackIndex.empty())
        GetAudioTracks();

    return mAudioTracks;
}

/**
 *   @brief Get current audio track
 *
 *   @return int - index of current audio track
 */
int StreamAbstractionAAMP_OTA::GetAudioTrack()
{
    int index = -1;
    if (mAudioTrackIndex.empty())
        GetAudioTracks();

    if (!mAudioTrackIndex.empty())
    {
        for (auto it = mAudioTracks.begin(); it != mAudioTracks.end(); it++)
        {
            if (it->index == mAudioTrackIndex)
            {
                index = std::distance(mAudioTracks.begin(), it);
            }
        }
    }
    return index;
}

/**
 * @brief SetPreferredAudioLanguages set the preferred audio language list
 *
 * @param[in]
 * @param[in]
 */
void StreamAbstractionAAMP_OTA::SetPreferredAudioLanguages()
{
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
#else
	JsonObject properties;
	bool modifiedLang = false;
	bool modifiedRend = false;
	//AAMPLOG_WARN( "[OTA_SHIM]aamp->preferredLanguagesString : %s, gATSCSettings.preferredLanguages : %s aamp->preferredRenditionString : %s gATSCSettings.preferredRendition : %s",  aamp->preferredLanguagesString.c_str(),gATSCSettings.preferredLanguages.c_str(), aamp->preferredRenditionString.c_str(), gATSCSettings.preferredRendition.c_str());fflush(stdout);

	if((0 != aamp->preferredLanguagesString.length()) && (aamp->preferredLanguagesString != gATSCSettings.preferredLanguages)){
		properties["preferredAudioLanguage"] = aamp->preferredLanguagesString.c_str();
		modifiedLang = true;
	}
	if((0 != aamp->preferredRenditionString.length()) && (aamp->preferredRenditionString != gATSCSettings.preferredRendition)){

		if(0 == aamp->preferredRenditionString.compare("VISUALLY_IMPAIRED")){
			properties["visuallyImpaired"] = true;
			modifiedRend = true;
		}else if(0 == aamp->preferredRenditionString.compare("NORMAL")){
			properties["visuallyImpaired"] = false;
			modifiedRend = true;
		}else{
			/*No rendition settings to MediaSettings*/
		}
	}
	if(modifiedLang || modifiedRend)
	{
		bool rpcResult = false;
		JsonObject result;
		JsonObject param;

		param["properties"] = properties;
		rpcResult = mediaSettingsObj.InvokeJSONRPC("setProperties", param, result);
		if (rpcResult){
			if (!result["success"].Boolean()){
				std::string responseStr;
				result.ToString(responseStr);
				AAMPLOG_WARN( "[OTA_SHIM] setProperties API failed result:%s", responseStr.c_str());
			}else{
				std::string paramStr;
				param.ToString(paramStr);
				AAMPLOG_WARN( "[OTA_SHIM] setProperties success with param:%s", paramStr.c_str());
				/*Thunder call success save global settings*/
				if(modifiedLang){
					gATSCSettings.preferredLanguages = aamp->preferredLanguagesString;
				}
				if(modifiedRend){
					gATSCSettings.preferredRendition = aamp->preferredRenditionString;
				}
			}
		}
	}
#endif
}

/**
 * @brief SetAudioTrackByLanguage set the audio language
 *
 * @param[in] lang : Audio Language to be set
 * @param[in]
 */
void StreamAbstractionAAMP_OTA::SetAudioTrackByLanguage(const char* lang)
{
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
#else
    JsonObject param;
    JsonObject result;
    JsonObject properties;
    int index = -1;

    if(NULL != lang)
    {
       if(mAudioTrackIndex.empty())
           GetAudioTracks();

       std::vector<AudioTrackInfo>::iterator itr;
       for(itr = mAudioTracks.begin(); itr != mAudioTracks.end(); itr++)
       {
           if(0 == strcmp(lang, itr->language.c_str()))
           {
               index = std::distance(mAudioTracks.begin(), itr);
               break;
           }
       }
    }
    if(-1 != index)
    {
        SetAudioTrack(index);
    }
    return;
#endif
}
/**
 * @brief GetAudioTracks get the available audio tracks for the selected service / media
 *
 * @param[in]
 * @param[in]
 */
void StreamAbstractionAAMP_OTA::GetAudioTracks()
{
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
#else
    JsonObject param;
    JsonObject result;
    JsonArray attributesArray;
    std::vector<AudioTrackInfo> aTracks;
    std::string aTrackIdx = "";
    std::string index;
    std::string output;
    JsonArray outputArray;
    JsonObject audioData;
    int i = 0,arrayCount = 0;
    long bandwidth = -1;
    int currentTrackPk = 0;

    currentTrackPk = GetAudioTrackInternal();

    attributesArray.Add("pk"); // int - Unique primary key dynamically allocated. Used for track selection.
    attributesArray.Add("name"); // 	Name to display in the UI when doing track selection
    attributesArray.Add("type");  // e,g "MPEG4_AAC" "MPEG2" etc
    attributesArray.Add("description"); //Track description supplied by the content provider
    attributesArray.Add("language"); //ISO 639-2 three character text language (terminology variant per DVB standard, i.e. "deu" instead of "ger")
    attributesArray.Add("contentType"); // e.g "DIALOGUE" , "EMERGENCY" , "MUSIC_AND_EFFECTS" etc
    attributesArray.Add("mixType"); // Signaled audio mix type - orthogonal to AudioTrackType; For example, ac3 could be either surround or stereo.e.g "STEREO" , "SURROUND_SOUND"
    attributesArray.Add("isSelected"); // Is Currently selected track

    param["id"] = APP_ID;
    param["attributes"] = attributesArray;

    thunderAccessObj.InvokeJSONRPC("getAudioTracks", param, result);

    result.ToString(output);
    AAMPLOG_TRACE( "[OTA_SHIM]:audio track output : %s ",  output.c_str());
    outputArray = result["table"].Array();
    arrayCount = outputArray.Length();

    for(i = 0; i < arrayCount; i++)
    {
        index = to_string(i);
        audioData = outputArray[i].Object();

        if(currentTrackPk == audioData["pk"].Number()){
            aTrackIdx = to_string(i);
        }

        std::string languageCode;
        languageCode = Getiso639map_NormalizeLanguageCode(audioData["language"].String(),aamp->GetLangCodePreference());
        aTracks.push_back(AudioTrackInfo(index, /*idx*/ languageCode, /*lang*/ audioData["contentType"].String(), /*rend*/ audioData["name"].String(), /*name*/ audioData["type"].String(), /*codecStr*/ (int)audioData["pk"].Number(), /*primaryKey*/ audioData["contentType"].String(), /*contentType*/ audioData["mixType"].String() /*mixType*/));
    }

    mAudioTracks = aTracks;
    mAudioTrackIndex = aTrackIdx;
    return;
#endif
}

/**
 * @brief GetAudioTrackInternal get the primary key for the selected audio
 *
 * @param[in]
 * @param[in]
 */
int StreamAbstractionAAMP_OTA::GetAudioTrackInternal()
{
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
    return 0;
#else
    int pk = 0;
    JsonObject param;
    JsonObject result;

    AAMPLOG_TRACE("[OTA_SHIM]Entered ");
    param["id"] = APP_ID;
    thunderAccessObj.InvokeJSONRPC("getAudioTrack", param, result);
    pk = result["pk"].Number();
    return pk;
#endif
}

/**
 * @brief SetAudioTrack sets a specific audio track
 *
 * @param[in] Index of the audio track.
 * @param[in]
 */
void StreamAbstractionAAMP_OTA::SetAudioTrack(int trackId)
{
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
#else
    JsonObject param;
    JsonObject result;

    param["id"] = APP_ID;

    param["trackPk"] = mAudioTracks[trackId].primaryKey;

    thunderAccessObj.InvokeJSONRPC("setAudioTrack", param, result);
    if (result["success"].Boolean()) {
        mAudioTrackIndex = to_string(trackId);
    }
    return;
#endif
}

/**
 *   @brief Get the list of available text tracks
 *
 *   @return std::vector<TextTrackInfo> List of available text tracks
 */
std::vector<TextTrackInfo> & StreamAbstractionAAMP_OTA::GetAvailableTextTracks()
{
	AAMPLOG_TRACE("[OTA_SHIM]");
	if (mTextTracks.empty())
		GetTextTracks();

	return mTextTracks;
}

/**
 * @brief GetTextTracks get the available text tracks for the selected service / media
 *
 * @param[in]
 * @param[in]
 */
void StreamAbstractionAAMP_OTA::GetTextTracks()
{
	AAMPLOG_TRACE("[OTA_SHIM]");
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
#else
	JsonObject param;
	JsonObject result;
	JsonArray attributesArray;
	std::vector<TextTrackInfo> txtTracks;
	std::string output;
	JsonArray outputArray;
	JsonObject textData;
	int arrayCount = 0;

	attributesArray.Add("pk"); // int - Unique primary key dynamically allocated. Used for track selection.
	attributesArray.Add("name"); //  Name to display in the UI when doing track selection
	attributesArray.Add("type");  // Specific track type for the track - "CC" for ATSC Closed caption
	attributesArray.Add("description"); //Track description supplied by the content provider
	attributesArray.Add("language"); //ISO 639-2 three character text language
	attributesArray.Add("contentType"); // Track content type e.g "HEARING_IMPAIRED", "EASY_READER"
	attributesArray.Add("ccServiceNumber"); // Set to 1-63 for 708 CC Subtitles and 0 for 608
	attributesArray.Add("isSelected"); // Is Currently selected track
	attributesArray.Add("ccTypeIs708"); // Is 708 cc track
	attributesArray.Add("ccType"); // Actual cc track type

	param["id"] = APP_ID;
	param["attributes"] = attributesArray;

	thunderAccessObj.InvokeJSONRPC("getSubtitleTracks", param, result);

	result.ToString(output);
	AAMPLOG_TRACE( "[OTA_SHIM]:text track output : %s ", output.c_str());
	outputArray = result["table"].Array();
	arrayCount = outputArray.Length();

	std::string txtTrackIdx = "";
	std::string instreamId;
	int ccIndex = 0;

	for(int i = 0; i < arrayCount; i++)
	{
		std::string trackType;
		textData = outputArray[i].Object();
		trackType = textData["type"].String();
		if(0 == trackType.compare("CC"))
		{

			std::string empty;
			std::string index = std::to_string(ccIndex++);
			std::string serviceNo;
			int ccServiceNumber = -1;
			std::string languageCode = Getiso639map_NormalizeLanguageCode(textData["language"].String(),aamp->GetLangCodePreference());

			ccServiceNumber = textData["ccServiceNumber"].Number();
			/*Plugin info : ccServiceNumber	int Set to 1-63 for 708 CC Subtitles and 1-4 for 608/TEXT*/
			if(textData["ccType"].String() == std::string{"CC708"})
			{
				if((ccServiceNumber >= 1) && (ccServiceNumber <= 63))
				{
					/*708 CC*/
					serviceNo = "SERVICE";
					serviceNo.append(std::to_string(ccServiceNumber));
				}
				else
				{
					AAMPLOG_WARN( "[OTA_SHIM]:unexpected text track for 708 CC");
				}
			}
			else if(textData["ccType"].String() == std::string{"CC608"})
			{
				if((ccServiceNumber >= 1) && (ccServiceNumber <= 4))
				{
					/*608 CC*/
					serviceNo = "CC";
					serviceNo.append(std::to_string(ccServiceNumber));
				}
				else
				{
					AAMPLOG_WARN( "[OTA_SHIM]:unexpected text track for 608 CC");
				}
			}
			else if(textData["ccType"].String() == std::string{"TEXT"})
			{
				if((ccServiceNumber >= 1) && (ccServiceNumber <= 4))
				{
					/*TEXT CC*/
					serviceNo = "TXT";
					serviceNo.append(std::to_string(ccServiceNumber));
				}
				else
				{
					AAMPLOG_WARN( "[OTA_SHIM]:unexpected text track for TEXT CC");
				}
			}
			else
			{
				AAMPLOG_WARN( "[OTA_SHIM]:unexpected ccType: '%s'",  textData["ccType"].String().c_str());
			}

			txtTracks.push_back(TextTrackInfo(index, languageCode, true, empty, textData["name"].String(), serviceNo, empty, (int)textData["pk"].Number()));
			//values shared: index, language, isCC, rendition-empty, name, instreamId, characteristics-empty, primarykey
			AAMPLOG_WARN("[OTA_SHIM]:: Text Track - index:%s lang:%s, isCC:true, rendition:empty, name:%s, instreamID:%s, characteristics:empty, primarykey:%d",  index.c_str(), languageCode.c_str(), textData["name"].String().c_str(), serviceNo.c_str(), (int)textData["pk"].Number());
		}
	}

	if (txtTracks.empty())
	{
		std::string empty;
		// Push dummy track , when not published,
		// it is obseved that even if track is not published
		// CC1 is present
		txtTracks.push_back(TextTrackInfo("0", "und", true, empty, "Undetermined", "CC1", empty, 0 ));
	}

	mTextTracks = txtTracks;
	return;
#endif
}

/**
 * @brief Disable Restrictions (unlock) till seconds mentioned
 *
 * @param[in] grace - seconds from current time, grace period, grace = -1 will allow an unlimited grace period
 * @param[in] time - seconds from current time,time till which the channel need to be kept unlocked
 * @param[in] eventChange - disable restriction handling till next program event boundary
 */
void StreamAbstractionAAMP_OTA::DisableContentRestrictions(long grace, long time, bool eventChange)
{
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
#else
	JsonObject param;
	JsonObject result;
	param["id"] = APP_ID;
	if(-1 == grace){

		param["grace"] = -1;
		param["time"] = -1;
		param["eventChange"] = false;
		AAMPLOG_WARN( "[OTA_SHIM] unlocked till next reboot or explicit enable" );
	}else{
		param["grace"] = 0;
		param["time"] = time;
		param["eventChange"] = eventChange;

		if(-1 != time)
			AAMPLOG_WARN( "[OTA_SHIM] unlocked for %ld sec ", time);

		if(eventChange)
			AAMPLOG_WARN( "[OTA_SHIM] unlocked till next program ");
	}
	thunderAccessObj.InvokeJSONRPC("disableContentRestrictionsUntil", param, result);

#endif
}

/**
 * @brief Enable Content Restriction (lock)
 *
 * @param[in]
 * @param[in]
 */
void StreamAbstractionAAMP_OTA::EnableContentRestrictions()
{
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
#else
	AAMPLOG_WARN( "[OTA_SHIM] locked ");
	JsonObject param;
	JsonObject result;
	param["id"] = APP_ID;
	thunderAccessObj.InvokeJSONRPC("enableContentRestrictions", param, result);

#endif
}

/**
 * @brief Stub implementation
 */
void StreamAbstractionAAMP_OTA::DumpProfiles(void)
{ // STUB
}

/**
 * @brief Get output format of stream.
 *
 * @param[out]  primaryOutputFormat - format of primary track
 * @param[out]  audioOutputFormat - format of audio track
 * @param[out]  auxAudioOutputFormat - format of aux audio track
 */
void StreamAbstractionAAMP_OTA::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat)
{
    primaryOutputFormat = FORMAT_INVALID;
    audioOutputFormat = FORMAT_INVALID;
	auxAudioOutputFormat = FORMAT_INVALID;
}

/**
 *   @brief Return MediaTrack of requested type
 *
 *   @param[in]  type - track type
 *   @retval MediaTrack pointer.
 */
MediaTrack* StreamAbstractionAAMP_OTA::GetMediaTrack(TrackType type)
{ // STUB
    return NULL;
}

/**
 * @brief Get current stream position.
 *
 * @retval current position of stream.
 */
double StreamAbstractionAAMP_OTA::GetStreamPosition()
{ // STUB
    return 0.0;
}

/**
 *   @brief Get stream information of a profile from subclass.
 *
 *   @param[in]  idx - profile index.
 *   @retval stream information corresponding to index.
 */
StreamInfo* StreamAbstractionAAMP_OTA::GetStreamInfo(int idx)
{ // STUB
    return NULL;
}

/**
 *   @brief  Get PTS of first sample.
 *
 *   @retval PTS of first sample
 */
double StreamAbstractionAAMP_OTA::GetFirstPTS()
{ // STUB
    return 0.0;
}

/**
 *   @brief  Get Start time PTS of first sample.
 *
 *   @retval start time of first sample
 */
double StreamAbstractionAAMP_OTA::GetStartTimeOfFirstPTS()
{ // STUB
    return 0.0;
}

double StreamAbstractionAAMP_OTA::GetBufferedDuration()
{ // STUB
	return -1.0;
}

bool StreamAbstractionAAMP_OTA::IsInitialCachingSupported()
{ // STUB
	return false;
}

/**
 * @brief Get index of profile corresponds to bandwidth
 * @param[in] bitrate Bitrate to lookup profile
 * @retval profile index
 */
int StreamAbstractionAAMP_OTA::GetBWIndex(long bitrate)
{ // STUB
    return 0;
}

/**
 * @brief To get the available video bitrates.
 * @ret available video bitrates
 */
std::vector<long> StreamAbstractionAAMP_OTA::GetVideoBitrates(void)
{ // STUB
    return std::vector<long>();
}

/*
* @brief Gets Max Bitrate avialable for current playback.
* @ret long MAX video bitrates
*/
long StreamAbstractionAAMP_OTA::GetMaxBitrate()
{ // STUB
    return 0;
}

/**
 * @brief To get the available audio bitrates.
 * @ret available audio bitrates
 */
std::vector<long> StreamAbstractionAAMP_OTA::GetAudioBitrates(void)
{ // STUB
    return std::vector<long>();
}

/**
 * @brief To get the available thumbnail tracks.
 * @ret available thumbnail tracks
 */
std::vector<StreamInfo*> StreamAbstractionAAMP_OTA::GetAvailableThumbnailTracks(void)
{ // STUB
	return std::vector<StreamInfo*>();
}

/**
 * @fn SetThumbnailTrack
 * @brief Function to set thumbnail track for processing
 *
 * @param thumbnail index value indicating the track to select
 * @return bool true on success.
 */
bool StreamAbstractionAAMP_OTA::SetThumbnailTrack(int thumbnailIndex)
{
	(void) thumbnailIndex;	/* unused */
	return false;
}

/**
 * @brief To get the available thumbnail tracks.
 * @ret available thumbnail tracks
 */
std::vector<ThumbnailData> StreamAbstractionAAMP_OTA::GetThumbnailRangeData(double start, double end, std::string *baseurl, int *raw_w, int *raw_h, int *width, int *height)
{
	return std::vector<ThumbnailData>();
}

/**
*   @brief  Stops injecting fragments to StreamSink.
*/
void StreamAbstractionAAMP_OTA::StopInjection(void)
{ // STUB - discontinuity related
}

/**
*   @brief  Start injecting fragments to StreamSink.
*/
void StreamAbstractionAAMP_OTA::StartInjection(void)
{ // STUB - discontinuity related
}
