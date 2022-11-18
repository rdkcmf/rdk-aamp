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

#include "fragmentcollector_mpd.h"

struct EarlyAvailablePeriodInfo
{
	EarlyAvailablePeriodInfo() : periodId(), isLicenseProcessed(false), isLicenseFailed(false), helper(nullptr){}
	std::string periodId;
	std::shared_ptr<AampDrmHelper> helper;
	bool isLicenseProcessed;
	bool isLicenseFailed;
};

StreamAbstractionAAMP_MPD::StreamAbstractionAAMP_MPD(AampLogManager *logObj, class PrivateInstanceAAMP *aamp,double seek_pos, float rate)
    : StreamAbstractionAAMP(logObj, aamp)
{
}

StreamAbstractionAAMP_MPD::~StreamAbstractionAAMP_MPD()
{
}

Accessibility StreamAbstractionAAMP_MPD::getAccessibilityNode(AampJsonObject &accessNode)
{
   	Accessibility accessibilityNode;
    return accessibilityNode;
}

void StreamAbstractionAAMP_MPD::DumpProfiles(void) {  }

AAMPStatusType StreamAbstractionAAMP_MPD::Init(TuneType tuneType) { return eAAMPSTATUS_OK; }

void StreamAbstractionAAMP_MPD::Start() {  }

void StreamAbstractionAAMP_MPD::Stop(bool clearChannelData) {  }

void StreamAbstractionAAMP_MPD::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat, StreamOutputFormat &subtitleOutputFormat) {  }

double StreamAbstractionAAMP_MPD::GetStreamPosition() { return 0; }

double StreamAbstractionAAMP_MPD::GetFirstPTS() { return 0; }

double StreamAbstractionAAMP_MPD::GetStartTimeOfFirstPTS() { return 0; }

MediaTrack* StreamAbstractionAAMP_MPD::GetMediaTrack(TrackType type) { return nullptr; }

double StreamAbstractionAAMP_MPD::GetBufferedDuration (void) { return 0; }

int StreamAbstractionAAMP_MPD::GetBWIndex(long bandwidth) { return 0; }

std::vector<long> StreamAbstractionAAMP_MPD::GetVideoBitrates(void) { std::vector<long> temp; return temp; }

std::vector<long> StreamAbstractionAAMP_MPD::GetAudioBitrates(void) { std::vector<long> temp; return temp; }

void StreamAbstractionAAMP_MPD::StopInjection(void) {  }

void StreamAbstractionAAMP_MPD::StartInjection(void) {  }

std::vector<StreamInfo*> StreamAbstractionAAMP_MPD::GetAvailableVideoTracks(void) { std::vector<StreamInfo*> temp; return temp; }

std::vector<StreamInfo*> StreamAbstractionAAMP_MPD::GetAvailableThumbnailTracks(void) { std::vector<StreamInfo*> temp; return temp; }

bool StreamAbstractionAAMP_MPD::SetThumbnailTrack(int) { return false; }

std::vector<ThumbnailData> StreamAbstractionAAMP_MPD::GetThumbnailRangeData(double, double, std::string*, int*, int*, int*, int*) { std::vector<ThumbnailData> temp; return temp; }

StreamInfo* StreamAbstractionAAMP_MPD::GetStreamInfo(int idx) { return nullptr; }

double StreamAbstractionAAMP_MPD::GetFirstPeriodStartTime(void)
{
    return 0.0;
}

uint32_t StreamAbstractionAAMP_MPD::GetCurrPeriodTimeScale()
{
    return 0;
}

int StreamAbstractionAAMP_MPD::GetProfileCount()
{
    return 0;
}

int StreamAbstractionAAMP_MPD::GetProfileIndexForBandwidth(long mTsbBandwidth)
{
    return 0;
}

long StreamAbstractionAAMP_MPD::GetMaxBitrate()
{
    return 0;
}

void StreamAbstractionAAMP_MPD::StartSubtitleParser()
{
}

void StreamAbstractionAAMP_MPD::PauseSubtitleParser(bool pause)
{
}

void StreamAbstractionAAMP_MPD::SetCDAIObject(CDAIObject *cdaiObj)
{
}

std::vector<AudioTrackInfo>& StreamAbstractionAAMP_MPD::GetAvailableAudioTracks(bool allTrack)
{
    return mAudioTracksAll;
}

std::vector<TextTrackInfo>& StreamAbstractionAAMP_MPD::GetAvailableTextTracks(bool allTrack)
{
    return mTextTracksAll;
}

void StreamAbstractionAAMP_MPD::InitSubtitleParser(char *data)
{
}

void StreamAbstractionAAMP_MPD::ResetSubtitle()
{
}

void StreamAbstractionAAMP_MPD::MuteSubtitleOnPause()
{
}

void StreamAbstractionAAMP_MPD::ResumeSubtitleOnPlay(bool mute, char *data)
{
}

void  StreamAbstractionAAMP_MPD::ResumeSubtitleAfterSeek(bool mute, char *data)
{
}

void StreamAbstractionAAMP_MPD::MuteSidecarSubtitles(bool mute)
{
}

bool StreamAbstractionAAMP_MPD::Is4KStream(int &height, long &bandwidth)
{
    return false;
}

bool StreamAbstractionAAMP_MPD::SetTextStyle(const std::string &options)
{
    return false;
}
