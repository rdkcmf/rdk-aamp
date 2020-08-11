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

/**
 *   @brief  Initialize a newly created object.
 *   @note   To be implemented by sub classes
 *   @param  tuneType to set type of object.
 *   @retval true on success
 *   @retval false on failure
 */
AAMPStatusType StreamAbstractionAAMP_OTA::Init(TuneType tuneType)
{
    AAMPStatusType retval = eAAMPSTATUS_OK;
    return retval;
}

/**
 * @brief StreamAbstractionAAMP_MPD Constructor
 * @param aamp pointer to PrivateInstanceAAMP object associated with player
 * @param seek_pos Seek position
 * @param rate playback rate
 */
StreamAbstractionAAMP_OTA::StreamAbstractionAAMP_OTA(class PrivateInstanceAAMP *aamp,double seek_pos, float rate): StreamAbstractionAAMP(aamp)
{ // STUB
}

/**
 * @brief StreamAbstractionAAMP_OTA Destructor
 */
StreamAbstractionAAMP_OTA::~StreamAbstractionAAMP_OTA()
{ // STUB
}

/**
 *   @brief  Starts streaming.
 */
void StreamAbstractionAAMP_OTA::Start(void)
{
	std::string id = "3";
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
	/*
	Request : {"jsonrpc": "2.0","id": 4,"method": "Controller.1.activate", "params": { "callsign": "org.rdk.MediaPlayer" }}
	Response : {"jsonrpc": "2.0","id": 4,"result": null}
	*/
	aamp_PostJsonRPC(id, "Controller.1.activate", "{\"callsign\":\"org.rdk.MediaPlayer\"}" );
	/*
	Request : {"jsonrpc":"2.0", "id":3, "method":"org.rdk.MediaPlayer.1.create", "params":{ "id" : "MainPlayer", "tag" : "MyApp"} }
	Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true } }
	*/
	aamp_PostJsonRPC(id, "org.rdk.MediaPlayer.1.create", "{\"id\":\"MainPlayer\",\"tag\":\"MyApp\"}");
	// inform (MediaRite) player instance on which wayland display it should draw the video. This MUST be set before load/play is called.
	/*
	Request : {"jsonrpc":"2.0", "id":3, "method":"org.rdk.MediaPlayer.1.setWaylandDisplay", "params":{"id" : "MainPlayer","display" : "westeros-123"} }
	Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true} }
	*/
	aamp_PostJsonRPC( id, "org.rdk.MediaPlayer.1.setWaylandDisplay", "{\"id\":\"MainPlayer\",\"display\":\"" + waylandDisplay + "\"}" );
	/*
	Request : {"jsonrpc":"2.0", "id":3, "method": "org.rdk.MediaPlayer.1.load", "params":{ "id":"MainPlayer", "url":"live://...", "autoplay": true} }
	Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true } }
	*/
	aamp_PostJsonRPC(id, "org.rdk.MediaPlayer.1.load","{\"id\":\"MainPlayer\",\"url\":\""+url+"\",\"autoplay\":true}" );
	/*
	Request : {"jsonrpc":"2.0", "id":3, "method": "org.rdk.MediaPlayer.1.play", "params":{ "id":"MainPlayer"} }
	Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true } }
	*/
  
	// below harmless, but not needed, given use of autoplay above
	aamp_PostJsonRPC(id, "org.rdk.MediaPlayer.1.play", "{\"id\":\"MainPlayer\"}");
}

/**
*   @brief  Stops streaming.
*/
void StreamAbstractionAAMP_OTA::Stop(bool clearChannelData)
{ // STUB
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
 */
void StreamAbstractionAAMP_OTA::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat)
{
    primaryOutputFormat = FORMAT_ISO_BMFF;
    audioOutputFormat = FORMAT_NONE;
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







