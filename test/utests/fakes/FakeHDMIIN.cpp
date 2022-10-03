/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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

#include "videoin_shim.h"
#include "hdmiin_shim.h"
#include "compositein_shim.h"

#define HDMIINPUT_CALLSIGN "org.rdk.HdmiInput.1"
#define COMPOSITEINPUT_CALLSIGN "org.rdk.CompositeInput.1"

StreamAbstractionAAMP_VIDEOIN::StreamAbstractionAAMP_VIDEOIN( const std::string name, const std::string callSign, AampLogManager *logObj,  class PrivateInstanceAAMP *aamp,double seek_pos, float rate)
                               : StreamAbstractionAAMP(logObj, aamp)
{
}

StreamAbstractionAAMP_VIDEOIN::~StreamAbstractionAAMP_VIDEOIN()
{
}

void StreamAbstractionAAMP_VIDEOIN::DumpProfiles(void) {  }

AAMPStatusType StreamAbstractionAAMP_VIDEOIN::Init(TuneType tuneType) { return eAAMPSTATUS_OK; }

void StreamAbstractionAAMP_VIDEOIN::Start() {  }

void StreamAbstractionAAMP_VIDEOIN::Stop(bool clearChannelData) {  }

void StreamAbstractionAAMP_VIDEOIN::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat, StreamOutputFormat &subtitleOutputFormat) {  }

double StreamAbstractionAAMP_VIDEOIN::GetStreamPosition() { return 0; }

double StreamAbstractionAAMP_VIDEOIN::GetFirstPTS() { return 0; }

double StreamAbstractionAAMP_VIDEOIN::GetStartTimeOfFirstPTS() { return 0; }

MediaTrack* StreamAbstractionAAMP_VIDEOIN::GetMediaTrack(TrackType type) { return nullptr; }

double StreamAbstractionAAMP_VIDEOIN::GetBufferedDuration (void) { return 0; }

bool StreamAbstractionAAMP_VIDEOIN::IsInitialCachingSupported() { return false; }

int StreamAbstractionAAMP_VIDEOIN::GetBWIndex(long bandwidth) { return 0; }

std::vector<long> StreamAbstractionAAMP_VIDEOIN::GetVideoBitrates(void) { std::vector<long> temp; return temp; }

std::vector<long> StreamAbstractionAAMP_VIDEOIN::GetAudioBitrates(void) { std::vector<long> temp; return temp; }

void StreamAbstractionAAMP_VIDEOIN::StopInjection(void) {  }

void StreamAbstractionAAMP_VIDEOIN::StartInjection(void) {  }

StreamInfo* StreamAbstractionAAMP_VIDEOIN::GetStreamInfo(int idx) { return nullptr; }

long StreamAbstractionAAMP_VIDEOIN::GetMaxBitrate()
{
    return 0;
}

void StreamAbstractionAAMP_VIDEOIN::SetVideoRectangle(int x, int y, int w, int h)
{
}

StreamAbstractionAAMP_HDMIIN::StreamAbstractionAAMP_HDMIIN(AampLogManager *logObj, class PrivateInstanceAAMP *aamp,double seek_pos, float rate)
                             : StreamAbstractionAAMP_VIDEOIN("HDMIIN", HDMIINPUT_CALLSIGN,logObj,aamp,seek_pos,rate)
{
}

StreamAbstractionAAMP_HDMIIN::~StreamAbstractionAAMP_HDMIIN()
{
}

AAMPStatusType StreamAbstractionAAMP_HDMIIN::Init(TuneType tuneType)
{
        return eAAMPSTATUS_OK;
}

void StreamAbstractionAAMP_HDMIIN::Start(void)
{
}

void StreamAbstractionAAMP_HDMIIN::Stop(bool clearChannelData)
{
}

std::vector<StreamInfo*> StreamAbstractionAAMP_HDMIIN::GetAvailableVideoTracks(void)
{
	return std::vector<StreamInfo*>();
}

std::vector<StreamInfo*> StreamAbstractionAAMP_HDMIIN::GetAvailableThumbnailTracks(void)
{
	return std::vector<StreamInfo*>();
}

bool StreamAbstractionAAMP_HDMIIN::SetThumbnailTrack(int thumbnailIndex)
{
    return false;
}

std::vector<ThumbnailData> StreamAbstractionAAMP_HDMIIN::GetThumbnailRangeData(double start, double end, std::string *baseurl, int *raw_w, int *raw_h, int *width, int *height)
{
    return std::vector<ThumbnailData>();
}

StreamAbstractionAAMP_COMPOSITEIN::StreamAbstractionAAMP_COMPOSITEIN(AampLogManager *logObj, class PrivateInstanceAAMP *aamp,double seek_pos, float rate)
                             : StreamAbstractionAAMP_VIDEOIN("COMPOSITEIN", COMPOSITEINPUT_CALLSIGN, logObj, aamp,seek_pos,rate)
{
}

StreamAbstractionAAMP_COMPOSITEIN::~StreamAbstractionAAMP_COMPOSITEIN()
{
}

AAMPStatusType StreamAbstractionAAMP_COMPOSITEIN::Init(TuneType tuneType)
{
    return eAAMPSTATUS_OK;
}

void StreamAbstractionAAMP_COMPOSITEIN::Start(void)
{
}

void StreamAbstractionAAMP_COMPOSITEIN::Stop(bool clearChannelData)
{
}

std::vector<StreamInfo*> StreamAbstractionAAMP_COMPOSITEIN::GetAvailableVideoTracks(void)
{
	return std::vector<StreamInfo*>();
}

std::vector<StreamInfo*> StreamAbstractionAAMP_COMPOSITEIN::GetAvailableThumbnailTracks(void)
{
	return std::vector<StreamInfo*>();
}

bool StreamAbstractionAAMP_COMPOSITEIN::SetThumbnailTrack(int thumbnailIndex)
{
    return false;
}

std::vector<ThumbnailData> StreamAbstractionAAMP_COMPOSITEIN::GetThumbnailRangeData(double start, double end, std::string *baseurl, int *raw_w, int *raw_h, int *width, int *height)
{
    return std::vector<ThumbnailData>();
}
