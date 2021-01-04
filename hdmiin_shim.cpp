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
*/

#define HDMIINPUT_CALLSIGN "org.rdk.HdmiInput.1"

/**
 * @brief StreamAbstractionAAMP_HDMIIN Constructor
 * @param aamp pointer to PrivateInstanceAAMP object associated with player
 * @param seek_pos Seek position
 * @param rate playback rate
 */
StreamAbstractionAAMP_HDMIIN::StreamAbstractionAAMP_HDMIIN(class PrivateInstanceAAMP *aamp,double seek_pos, float rate)
                             : StreamAbstractionAAMP_VIDEOIN("HDMIIN", HDMIINPUT_CALLSIGN,aamp,seek_pos,rate)
{
	aamp->SetContentType("HDMI_IN");
}
		   
/**
 * @brief StreamAbstractionAAMP_HDMIIN Destructor
 */
StreamAbstractionAAMP_HDMIIN::~StreamAbstractionAAMP_HDMIIN()
{
	AAMPLOG_WARN("%s:%d destructor ",__FUNCTION__,__LINE__);
}

/**
 *   @brief  Starts streaming.
 */
void StreamAbstractionAAMP_HDMIIN::Start(void)
{
	const char *url = aamp->GetManifestUrl().c_str();
	int hdmiInputPort = -1;
	if( sscanf(url, "hdmiin://localhost/deviceid/%d", &hdmiInputPort ) == 1 )
	{
		StartHelper(hdmiInputPort,"startHdmiInput");
	}
}

/**
*   @brief  Stops streaming.
*/
void StreamAbstractionAAMP_HDMIIN::Stop(bool clearChannelData)
{
	StopHelper("stopHdmiInput");
}

/**
 *   @brief  Initialize a newly created object.
 *   @note   To be implemented by sub classes
 *   @param  tuneType to set type of object.
 *   @retval eAAMPSTATUS_OK
 */
AAMPStatusType StreamAbstractionAAMP_HDMIIN::Init(TuneType tuneType)
{
	AAMPStatusType bretVal = StreamAbstractionAAMP_VIDEOIN::Init(tuneType);
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
	RegisterAllEvents();
#endif
	return bretVal;
}

/**
 * @brief To get the available thumbnail tracks.
 * @ret available thumbnail tracks
 */
std::vector<StreamInfo*> StreamAbstractionAAMP_HDMIIN::GetAvailableThumbnailTracks(void)
{ // STUB
	AAMPLOG_WARN("StreamAbstractionAAMP_HDMIIN:%s:%d",__FUNCTION__,__LINE__);
	return std::vector<StreamInfo*>();
}

/**
 * @brief To set the thumbnail track by index.
 * @ret True or False indicating success failure.
 */
bool StreamAbstractionAAMP_HDMIIN::SetThumbnailTrack(int thumbnailIndex)
{
	(void)thumbnailIndex;	/* unused */
	return false;
}

/**
 * @brief To get thumbnail range data.
 * @ret vector containg multiple thumbnail tile info.
 */
std::vector<ThumbnailData> StreamAbstractionAAMP_HDMIIN::GetThumbnailRangeData(double start, double end, std::string *baseurl, int *raw_w, int *raw_h, int *width, int *height)
{
        return std::vector<ThumbnailData>();
}

