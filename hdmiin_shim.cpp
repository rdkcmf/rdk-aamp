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
 * @file hdmiin_shim.cpp
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
* HdmiInput thunder plugin reference: https://wiki.rdkcentral.com/display/RDK/HDMI++Input
*/

#define HDMIINPUT_CALLSIGN "org.rdk.HdmiInput.1"


StreamAbstractionAAMP_HDMIIN::StreamAbstractionAAMP_HDMIIN(AampLogManager *logObj, class PrivateInstanceAAMP *aamp,double seek_pos, float rate)
                             : StreamAbstractionAAMP_VIDEOIN("HDMIIN", HDMIINPUT_CALLSIGN,logObj,aamp,seek_pos,rate)
{
	aamp->SetContentType("HDMI_IN");
}
		   

StreamAbstractionAAMP_HDMIIN::~StreamAbstractionAAMP_HDMIIN()
{
	AAMPLOG_WARN("destructor ");
}

AAMPStatusType StreamAbstractionAAMP_HDMIIN::Init(TuneType tuneType)
{
	AAMPStatusType retval = eAAMPSTATUS_OK;
	retval = InitHelper(tuneType);

#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
	std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> videoInfoUpdatedMethod = std::bind(&StreamAbstractionAAMP_HDMIIN::OnVideoStreamInfoUpdate, this, std::placeholders::_1);
	RegisterEvent("videoStreamInfoUpdate", videoInfoUpdatedMethod);
#endif
	return retval;
}


void StreamAbstractionAAMP_HDMIIN::Start(void)
{
	const char *url = aamp->GetManifestUrl().c_str();
	int hdmiInputPort = -1;
	if( sscanf(url, "hdmiin://localhost/deviceid/%d", &hdmiInputPort ) == 1 )
	{
		StartHelper(hdmiInputPort,"startHdmiInput");
	}
}


void StreamAbstractionAAMP_HDMIIN::Stop(bool clearChannelData)
{
	StopHelper("stopHdmiInput");
}


std::vector<StreamInfo*> StreamAbstractionAAMP_HDMIIN::GetAvailableVideoTracks(void)
{ // STUB
	AAMPLOG_WARN("StreamAbstractionAAMP_HDMIIN");
	return std::vector<StreamInfo*>();
}


std::vector<StreamInfo*> StreamAbstractionAAMP_HDMIIN::GetAvailableThumbnailTracks(void)
{ // STUB
	AAMPLOG_WARN("StreamAbstractionAAMP_HDMIIN");
	return std::vector<StreamInfo*>();
}


bool StreamAbstractionAAMP_HDMIIN::SetThumbnailTrack(int thumbnailIndex)
{
	(void)thumbnailIndex;	/* unused */
	return false;
}


std::vector<ThumbnailData> StreamAbstractionAAMP_HDMIIN::GetThumbnailRangeData(double start, double end, std::string *baseurl, int *raw_w, int *raw_h, int *width, int *height)
{
        return std::vector<ThumbnailData>();
}


#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS

void StreamAbstractionAAMP_HDMIIN::OnVideoStreamInfoUpdate(const JsonObject& parameters)
{
        std::string message;
        parameters.ToString(message);
        AAMPLOG_WARN("%s",message.c_str());

        JsonObject videoInfoObj = parameters;
        VideoScanType videoScanType = (videoInfoObj["progressive"].Boolean() ? eVIDEOSCAN_PROGRESSIVE : eVIDEOSCAN_INTERLACED);

	double frameRate = 0.0;
	double frameRateN = static_cast<double> (videoInfoObj["frameRateN"].Number());
	double frameRateD = static_cast<double> (videoInfoObj["frameRateD"].Number());
	if((0 != frameRateN) && (0 != frameRateD))
		frameRate = frameRateN / frameRateD;

	aamp->NotifyBitRateChangeEvent(0, eAAMP_BITRATE_CHANGE_BY_HDMIIN, videoInfoObj["width"].Number(), videoInfoObj["height"].Number(), frameRate, 0, false, videoScanType, 0, 0);
}
#endif
