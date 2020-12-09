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

//moc parental control
#include <fstream>
StreamAbstractionAAMP_OTA* StreamAbstractionAAMP_OTA::_instance = nullptr;
cTimer StreamAbstractionAAMP_OTA::m_pcTimer;
cTimer StreamAbstractionAAMP_OTA::m_lockTimer;
bool StreamAbstractionAAMP_OTA::m_isPcConfigRead = false;
std::vector<pcConfigItem> StreamAbstractionAAMP_OTA::m_pcProgramList;
int StreamAbstractionAAMP_OTA::m_pcProgramCount = 0;
std::vector<std::string> StreamAbstractionAAMP_OTA::currentPgmRestrictions;


#define MEDIAPLAYER_CALLSIGN "org.rdk.MediaPlayer.1"
#define MEDIASETTINGS_CALLSIGN "org.rdk.MediaSettings.1"
#define APP_ID "MainPlayer"

#define RDKSHELL_CALLSIGN "org.rdk.RDKShell.1"

void StreamAbstractionAAMP_OTA::onPlayerStatusHandler(const JsonObject& parameters) {
	std::string message;
	parameters.ToString(message);

	JsonObject playerData = parameters[APP_ID].Object();
	AAMPLOG_TRACE( "[OTA_SHIM]%s Received event : message : %s ", __FUNCTION__, message.c_str());
	/* For detailed event data, we can print or use details like
	   playerData["locator"].String(), playerData["length"].String(), playerData["position"].String() */

	std::string currState = playerData["playerStatus"].String();
	if(0 != prevState.compare(currState))
	{
		PrivAAMPState state = eSTATE_IDLE;
		AAMPLOG_INFO( "[OTA_SHIM]%s State changed from %s to %s ", __FUNCTION__, prevState.c_str(), currState.c_str());
		prevState = currState;
		if(0 == currState.compare("PROCESSING"))
		{
			state = eSTATE_PREPARING;
		}else if(0 == currState.compare("ERROR"))
		{
			aamp->SendAnomalyEvent(ANOMALY_WARNING, "ATSC Tuner Error");
			state = eSTATE_BUFFERING;
		}else if(0 == currState.compare("PLAYING"))
		{
			if(!tuned){
				aamp->SendTunedEvent();
				/* For consistency, during first tune, first move to
				 PREPARED state to match normal IPTV flow sequence */
				aamp->SetState(eSTATE_PREPARED);
                                aamp->SetContentType("OTA");
				tuned = true;
				aamp->LogFirstFrame();
				aamp->LogTuneComplete();
			}
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
				AAMPLOG_INFO( "[OTA_SHIM]%s Unsupported state change!", __FUNCTION__);
			}
			/* Need not set a new state hence returning */
			return;
		}
		aamp->SetState(state);
	}
}

/*[PC API platform integration]Needs to rewrite handler to process PC JsonObject input*/
void StreamAbstractionAAMP_OTA::onContentRestrictedHandler(bool locked) {
	std::vector<std::string> restrictionList;
	/*Default values set for testing purpose*/
	//restrictionList.push_back("TV-14");
	//restrictionList.push_back("TV-MA");

	if(locked)
		aamp->SendContentRestrictedEvent("locked", true, StreamAbstractionAAMP_OTA::_instance->currentPgmRestrictions);
	else
		aamp->SendContentRestrictedEvent("unlocked", false, StreamAbstractionAAMP_OTA::_instance->currentPgmRestrictions);
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
    AAMPLOG_INFO( "[OTA_SHIM]Inside %s ", __FUNCTION__ );
    prevState = "IDLE";
    tuned = false;

    thunderAccessObj.ActivatePlugin();
    mediaSettingsObj.ActivatePlugin();
    std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> actualMethod = std::bind(&StreamAbstractionAAMP_OTA::onPlayerStatusHandler, this, std::placeholders::_1);

    thunderAccessObj.SubscribeEvent(_T("onPlayerStatus"), actualMethod);

    /*[PC API platform integration]Subscribe for Content Restricted Event*/

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
StreamAbstractionAAMP_OTA::StreamAbstractionAAMP_OTA(class PrivateInstanceAAMP *aamp,double seek_pos, float rate)
                          : StreamAbstractionAAMP(aamp)
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
                            , tuned(false),restrictionsOta(),
                            thunderAccessObj(MEDIAPLAYER_CALLSIGN),
                            mediaSettingsObj(MEDIASETTINGS_CALLSIGN),
                            thunderRDKShellObj(RDKSHELL_CALLSIGN)
#endif
{ // STUB
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
#else
//moc parental control
	StreamAbstractionAAMP_OTA::_instance = this;
	m_pcTimer.setCallback(mocProgramChangeCallback);
	m_lockTimer.setCallback(mocLockCallback);
	m_pcTimer.setInterval(100);
	m_unlocktimeout = false;
	m_unlockpc = false;
	m_unlockfull = false;

	std::string pcCfgPath = "/opt/pc.cfg";

	if ((!m_isPcConfigRead) && (!pcCfgPath.empty()))
        {
		std::ifstream f(pcCfgPath, std::ifstream::in | std::ifstream::binary);
		if (f.good())
		{
			//logprintf("opened pc.cfg");
			std::string buf;
			while (f.good())
			{
				std::getline(f, buf);
				ProcessPcConfigEntry(buf);
			}
			f.close();
			m_isPcConfigRead = true;
			m_pcProgramCount = 0;
		}
	}
#endif
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

	thunderAccessObj.UnSubscribeEvent(_T("onPlayerStatus"));

//moc parental control
	m_pcTimer.stop();

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
	AAMPLOG_INFO( "[OTA_SHIM]Inside %s : url : %s ", __FUNCTION__ , url.c_str());
	JsonObject result;

	SetPreferredAudioLanguage();

	if(0 != aamp->mRestrictions.size())
	{
		//AAMPLOG_INFO( "[OTA_SHIM]Content Restrictions set by user");
		ApplyContentRestrictions(aamp->mRestrictions);
	}

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
		 AAMPLOG_INFO( "StreamAbstractionAAMP_OTA:%s:%d screenWidth:%d screenHeight:%d  ",__FUNCTION__, __LINE__,screenWidth, screenHeight);
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
 *   @brief Get current audio track
 *
 *   @return int - index of current audio track
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
 * @brief SetPreferredAudioLanguage set the preferred audio language list
 *
 * @param[in]
 * @param[in]
 */
void StreamAbstractionAAMP_OTA::SetPreferredAudioLanguage()
{
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
#else
    JsonObject result;
    JsonObject param;
    JsonObject properties;

    if(aamp->preferredLanguagesList.size() > 0) {
        properties["preferredAudioLanguage"] = aamp->preferredLanguagesString.c_str();
        param["properties"] = properties;
        mediaSettingsObj.InvokeJSONRPC("setProperties", param, result);
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
               memcpy(aamp->language, itr->language.c_str(),itr->language.length());
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
    AAMPLOG_TRACE( "[OTA_SHIM]:%s:%d audio track output : %s ", __FUNCTION__, __LINE__, output.c_str());
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
        languageCode = Getiso639map_NormalizeLanguageCode(audioData["language"].String());
        aTracks.push_back(AudioTrackInfo(index, /*idx*/ languageCode,/* lang */ audioData["name"].String(),	/* name*/ audioData["type"].String(), /* codecStr  */ (int)audioData["pk"].Number()));           //pk
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

    AAMPLOG_TRACE("[OTA_SHIM]Entered %s ", __FUNCTION__);
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
        memset(aamp->language, 0, sizeof(aamp->language));
        strncpy (aamp->language, mAudioTracks[trackId].language.c_str(),mAudioTracks[trackId].language.length());
    }
    return;
#endif
}

/**
 * @brief Set Content Restrictions set by the App
 *
 * @param[in] Restrictions to be applied to plugin
 */
void StreamAbstractionAAMP_OTA::ApplyContentRestrictions(std::vector<std::string> restrictions)
{
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
#else
	//AAMPLOG_INFO( "[OTA_SHIM]Content Restrictions set by user:");
        if(0 != restrictions.size())
        {
		/*[PC API platform integration]Needs to set the ratings to plugin*/

		/*Log message can be added if required
		for (int i = 0; i < restrictions.size(); i++)
                {
			AAMPLOG_INFO( "[OTA_SHIM] %s", restrictions[i].c_str());
		}*/

		/*Temporarily save the Restrictions till PC plugin APIs are ready*/
		restrictionsOta = restrictions;
        }

	m_pcTimer.start();
#endif
}
/**
 * @brief Get Content Restrictions
 *
 *   @return std::vector<std::string> list of restrictions
 */
std::vector<std::string> StreamAbstractionAAMP_OTA::GetContentRestrictions()
{
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
	return std::vector<std::string>();
#else
	//AAMPLOG_INFO( "[OTA_SHIM]%s: Content Restrictions from platform:", __FUNCTION__);
	/*[PC API platform integration]Needs to fill a vector using ratings from plugin and sent as return value*/

	/*Temporarily use the ota saved Restrictions till PC plugin APIs are ready*/
	return restrictionsOta;
#endif
}

/**
 * @brief Disable Restrictions (unlock) till seconds mentioned
 *
 * @param[in] secondsRelativeToCurrentTime Seconds Relative to current time
 * @param[in]
 */
void StreamAbstractionAAMP_OTA::DisableContentRestrictions(long secondsRelativeToCurrentTime)
{
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
#else
	if(-1 == secondsRelativeToCurrentTime){
		//pc temporary change
		m_unlockfull = true;

		AAMPLOG_WARN( "[OTA_SHIM]%s: unlocked till next reboot or explicit enable", __FUNCTION__ );
		/*[PC API platform integration]Make plugin call to disable restrictions*/
	}else{
		//pc temporary change
		m_unlocktimeout = true;
		m_lockTimer.setInterval((secondsRelativeToCurrentTime) * 1000);
		m_lockTimer.start();

		AAMPLOG_WARN( "[OTA_SHIM]%s: unlocked for %ld sec ", __FUNCTION__, secondsRelativeToCurrentTime);
		/*[PC API platform integration]Make plugin call to disable restrictions*/
	}

#endif
}

/**
 * @brief Disable Restrictions (unlock) till next program
 *
 * @param[in] untilProgramChange
 * @param[in]
 */
void StreamAbstractionAAMP_OTA::DisableContentRestrictions(bool untilProgramChange)
{
#ifndef USE_CPP_THUNDER_PLUGIN_ACCESS
#else
	AAMPLOG_WARN( "[OTA_SHIM]%s: unlocked till next program ", __FUNCTION__);
	/*[PC API platform integration]Make plugin call to disable restrictions*/

	//pc temporary change
	m_unlockpc = true;
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
	AAMPLOG_WARN( "[OTA_SHIM]%s: locked ", __FUNCTION__);
	/*[PC API platform integration]Make plugin call to enable restrictions*/

	//pc temporary change
	m_unlockpc = false;
	m_unlocktimeout = false;
	m_unlockfull = false;
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
    primaryOutputFormat = FORMAT_ISO_BMFF;
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

#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
//moc parental control

/**
* @brief helper function to avoid dependency on unsafe sscanf while reading strings
* @param bufPtr pointer to CString buffer to scan
* @param prefixPtr - prefix string to match in bufPtr
* @param valueCopyPtr receives allocated copy of string following prefix (skipping delimiting whitesace) if prefix found
* @retval 0 if prefix not present or error
* @retval 1 if string extracted/copied to valueCopyPtr
*/
int StreamAbstractionAAMP_OTA::ReadConfigStringHelper(std::string buf, const char *prefixPtr, const char **valueCopyPtr)
{
        int ret = 0;
        std::size_t pos = buf.find(prefixPtr);
        if (pos != std::string::npos)
        {
                pos += strlen(prefixPtr);
                if (*valueCopyPtr != NULL)
                {
                        free((void *)*valueCopyPtr);
                        *valueCopyPtr = NULL;
                }
                *valueCopyPtr = strdup(buf.substr(pos).c_str());
                if (*valueCopyPtr)
                {
                        ret = 1;
                }
        }
        return ret;
}


/**
* @brief helper function to extract numeric value from given buf after removing prefix
* @param buf String buffer to scan
* @param prefixPtr - prefix string to match in bufPtr
* @param value - receives numeric value after extraction
* @retval 0 if prefix not present or error
* @retval 1 if string converted to numeric value
*/

int StreamAbstractionAAMP_OTA::ReadConfigNumericHelper(std::string buf, const char* prefixPtr, long& value)
{
        int ret = 0;

        try
        {
                std::size_t pos = buf.find(prefixPtr);
                if (pos != std::string::npos)
                {
                        pos += strlen(prefixPtr);
                        std::string valStr = buf.substr(pos);
                        value = std::stol(valStr);
                        ret = 1;
                }
        }
        catch(exception& e)
        {
                // NOP
        }

        return ret;
}


/**
 * @brief Process config entries,i and update pc input settings
 *        based on the config setting.
 * @param cfg config to process
 */
void StreamAbstractionAAMP_OTA::ProcessPcConfigEntry(std::string cfg)
{
        if (!cfg.empty() && cfg.at(0) != '#')
        { // ignore comments
                //Removing unnecessary spaces and newlines
                trim(cfg);
		long value;
		char * tmpValue = NULL;
		pcConfigItem tempConfigItem;

                if (ReadConfigNumericHelper(cfg, "programDuration=", value) == 1)
                {
                        tempConfigItem.pgmDuration = value;
                        //logprintf("programDuration=%ld", value);
                }

                if (ReadConfigStringHelper(cfg, "restrictions=", (const char**)&tmpValue))
                {
                        if (tmpValue)
                        {
				tempConfigItem.pgmRestrictions = tmpValue;
                                //logprintf("restrictions = %s", tmpValue);
                                //logprintf("restrictions saved = %s", tempConfigItem.pgmRestrictions.c_str());

                                free(tmpValue);
                                tmpValue = NULL;
                        }
			m_pcProgramList.push_back(tempConfigItem);
                }
        }
}



void StreamAbstractionAAMP_OTA::mocProgramChangeCallback()
{
	if(true == m_isPcConfigRead)
	{
		if(m_pcProgramCount >= m_pcProgramList.size())
			m_pcProgramCount = 0;

		bool locked = false;

                //logprintf("Current Program restrictions = %s", m_pcProgramList[m_pcProgramCount].pgmRestrictions.c_str());

		int restrictionCount = StreamAbstractionAAMP_OTA::_instance->restrictionsOta.size();
		std::size_t pos = std::string::npos;
                for (int i = 0; i < restrictionCount; i++)
                {
			pos = m_pcProgramList[m_pcProgramCount].pgmRestrictions.find(StreamAbstractionAAMP_OTA::_instance->restrictionsOta[i]);
		        if (pos != std::string::npos)
		        {
				//logprintf("restriction : %s is present in the current program's restriction list\n", StreamAbstractionAAMP_OTA::_instance->restrictionsOta[i].c_str());
				locked = true;
				break;
		        }
                }
		//No need to check StreamAbstractionAAMP_OTA::_instance->m_unlockpc
		if((StreamAbstractionAAMP_OTA::_instance->m_unlocktimeout) ||
		   (StreamAbstractionAAMP_OTA::_instance->m_unlockfull))
		{
			locked = false;
			//logprintf("Unlocked by user");
		}

		std::string delimiter = ",";
		StreamAbstractionAAMP_OTA::_instance->currentPgmRestrictions.clear();
		std::string s = m_pcProgramList[m_pcProgramCount].pgmRestrictions;
		size_t position = 0;
		std::string token;
		while ((position = s.find(delimiter)) != std::string::npos) {
			token = s.substr(0, position);
			std::cout << token << std::endl;
			s.erase(0, position + delimiter.length());
			StreamAbstractionAAMP_OTA::_instance->currentPgmRestrictions.push_back(token);
		}
		StreamAbstractionAAMP_OTA::_instance->currentPgmRestrictions.push_back(s);

		StreamAbstractionAAMP_OTA::_instance->onContentRestrictedHandler(locked);
                //logprintf("Timer set for = %ld", m_pcProgramList[m_pcProgramCount].pgmDuration);
		m_pcTimer.setInterval((m_pcProgramList[m_pcProgramCount++].pgmDuration) * 1000);

		if(StreamAbstractionAAMP_OTA::_instance->m_unlockpc)
			StreamAbstractionAAMP_OTA::_instance->m_unlockpc = false;
	}
}

void StreamAbstractionAAMP_OTA::mocLockCallback()
{
        //logprintf("Reset unlock set with timeout");
	StreamAbstractionAAMP_OTA::_instance->m_unlocktimeout = false;
	m_lockTimer.stop();
}

/***
 * @brief : Constructor.
 * @return   : nil.
 */
cTimer::cTimer()
{
    clear = false;
    interval = 0;
}

/***
 * @brief : Destructor.
 * @return   : nil.
 */
cTimer::~cTimer()
{
    this->clear = true;
}

/***
 * @brief : start timer thread.
 * @return   : <bool> False if timer thread couldn't be started.
 */
bool cTimer::start()
{
    if (interval <= 0 && callBack_function == NULL) {
        return false;
    }
    this->clear = false;
    std::thread timerThread([=]() {
            while (true) {
            if (this->clear) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
            if (this->clear) return;
            this->callBack_function();
            }
            });
   timerThread.detach();
    return true;
}

/***
 * @brief : stop timer thread.
 * @return   : nil
 */
void cTimer::stop()
{
    this->clear = true;
}

/***
 * @brief     : Set interval in which the given function should be invoked.
 * @param1[in]   : function which has to be invoked on timed intervals
 * @param2[in]  : timer interval val.
 * @return     : nil
 */
void cTimer::setCallback(void (*function)())
{
    this->callBack_function = function;
}

/***
 * @brief     : Set interval in which the given function should be invoked.
 * @param1[in]   : function which has to be invoked on timed intervals
 * @param2[in]  : timer interval val.
 * @return     : nil
 */
void cTimer::setInterval(int val)
{
    this->interval = val;
}
#endif
