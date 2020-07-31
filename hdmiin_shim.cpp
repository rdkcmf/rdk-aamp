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
 * @file fragmentcollector_HDMIIN.cpp
 * @brief shim for dispatching UVE HDMI input playback
 */
#include "hdmiin_shim.h"
#include "priv_aamp.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>
#include "AampUtils.h"

/**
HdmiInput thunder plugin reference: https://wiki.rdkcentral.com/display/RDK/HDMI++Input
 Refer RDK-26059
 
Note: portId takes int values. portId can be extracted from the response of existing org.rdk.HdmiInput.1.getHDMIInputDevices API.
 Request : {"jsonrpc":"2.0", "id":3, "method":"org.rdk.HdmiInput.1.getHDMIInputDevices"}
 Response: {"jsonrpc":"2.0","id":3,"result":{"devices":[{"id":0,"locator":"hdmiin://localhost/deviceid/0","connected":"false"}],"success":true}} )
 URL will be in format: hdmiin://localhost/deviceid/<port>
 i.e. hdmiin://localhost/deviceid/0

 Testing Note: Anyone working on a blob board will need HDCP keys from Scott to make HDMI-in work. But should work as-is on CAD1.0 hardware.
*/

/**
 *   @brief  Initialize a newly created object.
 *   @note   To be implemented by sub classes
 *   @param  tuneType to set type of object.
 *   @retval true on success
 *   @retval false on failure
 */
AAMPStatusType StreamAbstractionAAMP_HDMIIN::Init(TuneType tuneType)
{
    AAMPStatusType retval = eAAMPSTATUS_OK;
    return retval;
}

/**
 * @brief StreamAbstractionAAMP_HDMIIN Constructor
 * @param aamp pointer to PrivateInstanceAAMP object associated with player
 * @param seek_pos Seek position
 * @param rate playback rate
 */
StreamAbstractionAAMP_HDMIIN::StreamAbstractionAAMP_HDMIIN(class PrivateInstanceAAMP *aamp,double seek_pos, float rate): StreamAbstractionAAMP(aamp), hdmiInputPort(-1)

{
	AAMPLOG_WARN("StreamAbstractionAAMP_HDMIIN:%s:%d constructor  activating org.rdk.HdmiInput ",__FUNCTION__,__LINE__);
	std::string response = aamp_PostJsonRPC("3", "Controller.1.activate", "{\"callsign\":\"org.rdk.HdmiInput\"}" );
	logprintf( "StreamAbstractionAAMP_HDMIIN:%s:%d response '%s'\n", __FUNCTION__, __LINE__, response.c_str());
}
		   
/**
 * @brief StreamAbstractionAAMP_HDMIIN Destructor
 */
StreamAbstractionAAMP_HDMIIN::~StreamAbstractionAAMP_HDMIIN()
{
	AAMPLOG_WARN("StreamAbstractionAAMP_HDMIIN:%s:%d destructor ",__FUNCTION__,__LINE__);
}

/**
 *   @brief  Starts streaming.
 */
void StreamAbstractionAAMP_HDMIIN::Start(void)
{
	const char *url = aamp->GetManifestUrl().c_str();
	AAMPLOG_WARN("StreamAbstractionAAMP_HDMIIN:%s:%d url:%s",__FUNCTION__,__LINE__,url);
	hdmiInputPort = -1;
	if( sscanf(url, "hdmiin://localhost/deviceid/%d", &hdmiInputPort ) == 1 )
	{
		AAMPLOG_WARN("StreamAbstractionAAMP_HDMIIN:%s:%d port is %d ",__FUNCTION__,__LINE__, hdmiInputPort);

		std::string params = "{\"portId\":" + std::to_string(hdmiInputPort) + "}";
		std::string success = aamp_PostJsonRPC( "3", "org.rdk.HdmiInput.1.startHdmiInput", params );
		logprintf( "StreamAbstractionAAMP_HDMIIN:%s:%d response '%s'\n", __FUNCTION__, __LINE__, success.c_str());
		/*
		Request: {"jsonrpc":"2.0", "id":3, "method":"org.rdk.HdmiInput.1.startHdmiInput", "params":{"portId":0}}
		Response: {"jsonrpc":"2.0","id":3,"result":{"success":true}}
		 */
	}
}

/**
*   @brief  Stops streaming.
*/
void StreamAbstractionAAMP_HDMIIN::Stop(bool clearChannelData)
{ // STUB
	AAMPLOG_WARN("StreamAbstractionAAMP_HDMIIN:%s:%d",__FUNCTION__,__LINE__);
	if( hdmiInputPort>=0 )
	{
		std::string  success = aamp_PostJsonRPC( "3", "org.rdk.HdmiInput.1.stopHdmiInput", "null" );
		logprintf( "StreamAbstractionAAMP_HDMIIN:%s:%d response '%s'\n", __FUNCTION__, __LINE__, success.c_str());
		hdmiInputPort = -1;
	/*
	 Request: {"jsonrpc":"2.0", "id":3, "method":"org.rdk.HdmiInput.1.stopHdmiInput"}
	 Response: {"jsonrpc":"2.0","id":3,"result":{"success":true}}
	*/
	}
}

/**
 * @brief Stub implementation
 */
void StreamAbstractionAAMP_HDMIIN::DumpProfiles(void)
{ // STUB
	AAMPLOG_WARN("StreamAbstractionAAMP_HDMIIN:%s:%d",__FUNCTION__,__LINE__);
}

/**
 * @brief Get output format of stream.
 *
 * @param[out]  primaryOutputFormat - format of primary track
 * @param[out]  audioOutputFormat - format of audio track
 */
void StreamAbstractionAAMP_HDMIIN::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat)
{ // STUB
	AAMPLOG_WARN("StreamAbstractionAAMP_HDMIIN:%s:%d",__FUNCTION__,__LINE__);
    primaryOutputFormat = FORMAT_NONE;
    audioOutputFormat = FORMAT_NONE;
}

/**
 *   @brief Return MediaTrack of requested type
 *
 *   @param[in]  type - track type
 *   @retval MediaTrack pointer.
 */
MediaTrack* StreamAbstractionAAMP_HDMIIN::GetMediaTrack(TrackType type)
{ // STUB
	AAMPLOG_WARN("StreamAbstractionAAMP_HDMIIN:%s:%d",__FUNCTION__,__LINE__);
    return NULL;
}

/**
 * @brief Get current stream position.
 *
 * @retval current position of stream.
 */
double StreamAbstractionAAMP_HDMIIN::GetStreamPosition()
{ // STUB
	AAMPLOG_WARN("StreamAbstractionAAMP_HDMIIN:%s:%d",__FUNCTION__,__LINE__);
    return 0.0;
}

/**
 *   @brief Get stream information of a profile from subclass.
 *
 *   @param[in]  idx - profile index.
 *   @retval stream information corresponding to index.
 */
StreamInfo* StreamAbstractionAAMP_HDMIIN::GetStreamInfo(int idx)
{ // STUB
	AAMPLOG_WARN("StreamAbstractionAAMP_HDMIIN:%s:%d",__FUNCTION__,__LINE__);
    return NULL;
}

/**
 *   @brief  Get PTS of first sample.
 *
 *   @retval PTS of first sample
 */
double StreamAbstractionAAMP_HDMIIN::GetFirstPTS()
{ // STUB
	AAMPLOG_WARN("StreamAbstractionAAMP_HDMIIN:%s:%d",__FUNCTION__,__LINE__);
    return 0.0;
}

double StreamAbstractionAAMP_HDMIIN::GetBufferedDuration()
{ // STUB
	AAMPLOG_WARN("StreamAbstractionAAMP_HDMIIN:%s:%d",__FUNCTION__,__LINE__);
	return -1.0;
}

bool StreamAbstractionAAMP_HDMIIN::IsInitialCachingSupported()
{
	AAMPLOG_WARN("StreamAbstractionAAMP_HDMIIN:%s:%d",__FUNCTION__,__LINE__);
	return false;
}

/**
 * @brief Get index of profile corresponds to bandwidth
 * @param[in] bitrate Bitrate to lookup profile
 * @retval profile index
 */
int StreamAbstractionAAMP_HDMIIN::GetBWIndex(long bitrate)
{
	AAMPLOG_WARN("StreamAbstractionAAMP_HDMIIN:%s:%d",__FUNCTION__,__LINE__);
    return 0;
}

/**
 * @brief To get the available video bitrates.
 * @ret available video bitrates
 */
std::vector<long> StreamAbstractionAAMP_HDMIIN::GetVideoBitrates(void)
{ // STUB
	AAMPLOG_WARN("StreamAbstractionAAMP_HDMIIN:%s:%d",__FUNCTION__,__LINE__);
    return std::vector<long>();
}

/*
* @brief Gets Max Bitrate avialable for current playback.
* @ret long MAX video bitrates
*/
long StreamAbstractionAAMP_HDMIIN::GetMaxBitrate()
{ // STUB
	AAMPLOG_WARN("StreamAbstractionAAMP_HDMIIN:%s:%d",__FUNCTION__,__LINE__);
    return 0;
}

/**
 * @brief To get the available audio bitrates.
 * @ret available audio bitrates
 */
std::vector<long> StreamAbstractionAAMP_HDMIIN::GetAudioBitrates(void)
{ // STUB
	AAMPLOG_WARN("StreamAbstractionAAMP_HDMIIN:%s:%d",__FUNCTION__,__LINE__);
    return std::vector<long>();
}

/**
*   @brief  Stops injecting fragments to StreamSink.
*/
void StreamAbstractionAAMP_HDMIIN::StopInjection(void)
{ // STUB - discontinuity related
	AAMPLOG_WARN("StreamAbstractionAAMP_HDMIIN:%s:%d",__FUNCTION__,__LINE__);
}

/**
*   @brief  Start injecting fragments to StreamSink.
*/
void StreamAbstractionAAMP_HDMIIN::StartInjection(void)
{ // STUB - discontinuity related
	AAMPLOG_WARN("StreamAbstractionAAMP_HDMIIN:%s:%d",__FUNCTION__,__LINE__);
}
