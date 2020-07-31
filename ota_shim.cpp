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


#include "priv_aamp.h"
#include "ota_shim.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>



struct MyRpcResponse
{
	char buffer[1024];
	size_t length;
};
static size_t MyRpcWriteFunction( void *buffer, size_t size, size_t nmemb, void *context )
{
	struct MyRpcResponse *response = (struct MyRpcResponse *)context;
	size_t numBytes = size*nmemb;
	if( numBytes>0 )
	{
		if( response->length + numBytes < sizeof(response->buffer)+1 )
		{ // enough space to append
			memcpy( &response->buffer[response->length], buffer, numBytes );
			response->length += numBytes;
		}
	}
	return numBytes;
}

/**
 * @brief aamp_PostJsonRPC posts JSONRPC data
 * @param[in] id string containing player id
 * @param[in] method string containing JSON method
 * @param[in] params string containing params:value pair list
 */
bool aamp_PostJsonRPC( std::string id, std::string method, std::string params )
{
	bool rc = false;
	CURL *curlhandle= curl_easy_init();
	if( curlhandle )
	{
		curl_easy_setopt( curlhandle, CURLOPT_URL, "http://127.0.0.1:9998/jsonrpc" ); // local thunder

		struct curl_slist *headers = NULL;
		headers = curl_slist_append( headers, "Content-Type: application/json" );
		curl_easy_setopt(curlhandle, CURLOPT_HTTPHEADER, headers);

		std::string data = "{\"jsonrpc\":\"2.0\",\"id\":"+id+",\"method\":\""+method+"\",\"params\":"+params+"}";
		logprintf( "%s:%d data: %s\n", __FUNCTION__, __LINE__, data.c_str() );
		curl_easy_setopt(curlhandle, CURLOPT_POSTFIELDS, data.c_str() );

		struct MyRpcResponse response; // receives response data
		response.length = 0;
		curl_easy_setopt(curlhandle, CURLOPT_WRITEFUNCTION, MyRpcWriteFunction);
		curl_easy_setopt(curlhandle, CURLOPT_WRITEDATA, &response);

		CURLcode res = curl_easy_perform(curlhandle);
		if( res == CURLE_OK )
		{
			long http_code = -1;
			curl_easy_getinfo(curlhandle, CURLINFO_RESPONSE_CODE, &http_code);
			response.buffer[response.length] = 0x00; // add nul terminator
			logprintf( "%s:%d HTTP %ld len:%d Response: %s\n",
					  __FUNCTION__, __LINE__, http_code, response.length, response.buffer );
			rc = true;
		}
		else
		{
			logprintf( "%s:%d failed: %s", __FUNCTION__, __LINE__, curl_easy_strerror(res));
		}
		curl_slist_free_all( headers );
		curl_easy_cleanup(curlhandle);
	}
	return rc;
}


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
    // aamp->IsTuneTypeNew = false;
    return retval;
}

/**
 * @brief StreamAbstractionAAMP_MPD Constructor
 * @param aamp pointer to PrivateInstanceAAMP object associated with player
 * @param seek_pos Seek position
 * @param rate playback rate
 */
StreamAbstractionAAMP_OTA::StreamAbstractionAAMP_OTA(class PrivateInstanceAAMP *aamp,double seek_pos, float rate): StreamAbstractionAAMP(aamp), id("dummy")
//,fragmentCollectorThreadStarted(false), fragmentCollectorThreadID(0)
{
    //MediaRiteplayerActivate(id);
    //MediaRiteplayerCreate(id);
//    trickplayMode = (rate != AAMP_NORMAL_PLAY_RATE);
}

/**
 * @brief StreamAbstractionAAMP_OTA Destructor
 */
StreamAbstractionAAMP_OTA::~StreamAbstractionAAMP_OTA()
{
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
	Request : {"jsonrpc": "2.0","id": 4,"method": "Controller.1.activate", "params": { "callsign": "WebKitBrowser" }}
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
	aamp_PostJsonRPC(id, "org.rdk.MediaPlayer.1.play", "{\"id\":\"MainPlayer\"}"); // note: not needed, assuming above autoplay works
}

/**
*   @brief  Stops streaming.
*/
void StreamAbstractionAAMP_OTA::Stop(bool clearChannelData)
{ // STUB
	aamp_PostJsonRPC(id, "org.rdk.MediaPlayer.1.stop", "{\"id\":\"MainPlayer\"}");
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
{
    return NULL;//mPriv->GetMediaTrack(type);
}

/**
 * @brief Get current stream position.
 *
 * @retval current position of stream.
 */
double StreamAbstractionAAMP_OTA::GetStreamPosition()
{
    return 0.0;
}

/**
 *   @brief Get stream information of a profile from subclass.
 *
 *   @param[in]  idx - profile index.
 *   @retval stream information corresponding to index.
 */
StreamInfo* StreamAbstractionAAMP_OTA::GetStreamInfo(int idx)
{
    return NULL;
}

/**
 *   @brief  Get PTS of first sample.
 *
 *   @retval PTS of first sample
 */
double StreamAbstractionAAMP_OTA::GetFirstPTS()
{
    return 0.0;
}

double StreamAbstractionAAMP_OTA::GetBufferedDuration()
{
	return -1.0;
}

bool StreamAbstractionAAMP_OTA::IsInitialCachingSupported()
{
	return false;
}

/**
 * @brief Get index of profile corresponds to bandwidth
 * @param[in] bitrate Bitrate to lookup profile
 * @retval profile index
 */
int StreamAbstractionAAMP_OTA::GetBWIndex(long bitrate)
{
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







