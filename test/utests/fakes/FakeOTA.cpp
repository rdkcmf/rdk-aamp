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

#include "ota_shim.h"

StreamAbstractionAAMP_OTA::StreamAbstractionAAMP_OTA(AampLogManager *logObj, class PrivateInstanceAAMP *aamp,double seek_pos, float rate)
                          : StreamAbstractionAAMP(logObj, aamp)
{
}

StreamAbstractionAAMP_OTA::~StreamAbstractionAAMP_OTA()
{
}

void StreamAbstractionAAMP_OTA::DumpProfiles(void) {  }

AAMPStatusType StreamAbstractionAAMP_OTA::Init(TuneType tuneType) { return eAAMPSTATUS_OK; }

void StreamAbstractionAAMP_OTA::Start() {  }

void StreamAbstractionAAMP_OTA::Stop(bool clearChannelData) {  }

void StreamAbstractionAAMP_OTA::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat, StreamOutputFormat &subtitleOutputFormat) {  }

double StreamAbstractionAAMP_OTA::GetStreamPosition() { return 0; }

double StreamAbstractionAAMP_OTA::GetFirstPTS() { return 0; }

double StreamAbstractionAAMP_OTA::GetStartTimeOfFirstPTS() { return 0; }

MediaTrack* StreamAbstractionAAMP_OTA::GetMediaTrack(TrackType type) { return nullptr; }

double StreamAbstractionAAMP_OTA::GetBufferedDuration (void) { return 0; }

bool StreamAbstractionAAMP_OTA::IsInitialCachingSupported() { return false; }

int StreamAbstractionAAMP_OTA::GetBWIndex(long bandwidth) { return 0; }

std::vector<long> StreamAbstractionAAMP_OTA::GetVideoBitrates(void) { std::vector<long> temp; return temp; }

std::vector<long> StreamAbstractionAAMP_OTA::GetAudioBitrates(void) { std::vector<long> temp; return temp; }

void StreamAbstractionAAMP_OTA::StopInjection(void) {  }

void StreamAbstractionAAMP_OTA::StartInjection(void) {  }

std::vector<AudioTrackInfo> &StreamAbstractionAAMP_OTA::GetAvailableAudioTracks(bool allTrack) { return mAudioTracks; };

std::vector<TextTrackInfo> &StreamAbstractionAAMP_OTA::GetAvailableTextTracks(bool allTrack) { return mTextTracks; };

int StreamAbstractionAAMP_OTA::GetAudioTrack() { return 0; }

bool StreamAbstractionAAMP_OTA::GetCurrentAudioTrack(AudioTrackInfo &audioTrack) { return false; }

void StreamAbstractionAAMP_OTA::SetVideoRectangle(int x, int y, int w, int h) {}

std::vector<StreamInfo*> StreamAbstractionAAMP_OTA::GetAvailableVideoTracks(void) { std::vector<StreamInfo*> temp; return temp; }

std::vector<StreamInfo*> StreamAbstractionAAMP_OTA::GetAvailableThumbnailTracks(void) { std::vector<StreamInfo*> temp; return temp; }

bool StreamAbstractionAAMP_OTA::SetThumbnailTrack(int) { return false; }

std::vector<ThumbnailData> StreamAbstractionAAMP_OTA::GetThumbnailRangeData(double, double, std::string*, int*, int*, int*, int*) { std::vector<ThumbnailData> temp; return temp; }

void StreamAbstractionAAMP_OTA::SetAudioTrack (int index) {}

void StreamAbstractionAAMP_OTA::SetAudioTrackByLanguage(const char* lang) {}

void StreamAbstractionAAMP_OTA::SetPreferredAudioLanguages() {}

void StreamAbstractionAAMP_OTA::DisableContentRestrictions(long grace, long time, bool eventChange) {}

void StreamAbstractionAAMP_OTA::EnableContentRestrictions() {}

StreamInfo* StreamAbstractionAAMP_OTA::GetStreamInfo(int idx) { return nullptr; }

long StreamAbstractionAAMP_OTA::GetMaxBitrate()
{ 
    return 0;
}


