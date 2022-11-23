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

#include "fragmentcollector_progressive.h"

StreamAbstractionAAMP_PROGRESSIVE::StreamAbstractionAAMP_PROGRESSIVE(AampLogManager *logObj, class PrivateInstanceAAMP *aamp,double seek_pos, float rate): StreamAbstractionAAMP(logObj, aamp)
{
}

StreamAbstractionAAMP_PROGRESSIVE::~StreamAbstractionAAMP_PROGRESSIVE()
{
}

void StreamAbstractionAAMP_PROGRESSIVE::DumpProfiles(void) {  }

AAMPStatusType StreamAbstractionAAMP_PROGRESSIVE::Init(TuneType tuneType) { return eAAMPSTATUS_OK; }

void StreamAbstractionAAMP_PROGRESSIVE::Start() {  }

void StreamAbstractionAAMP_PROGRESSIVE::Stop(bool clearChannelData) {  }

void StreamAbstractionAAMP_PROGRESSIVE::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat, StreamOutputFormat &subtitleOutputFormat) {  }

double StreamAbstractionAAMP_PROGRESSIVE::GetStreamPosition() { return 0; }

double StreamAbstractionAAMP_PROGRESSIVE::GetFirstPTS() { return 0; }

double StreamAbstractionAAMP_PROGRESSIVE::GetStartTimeOfFirstPTS() { return 0; }

MediaTrack* StreamAbstractionAAMP_PROGRESSIVE::GetMediaTrack(TrackType type) { return nullptr; }

double StreamAbstractionAAMP_PROGRESSIVE::GetBufferedDuration (void) { return 0; }

bool StreamAbstractionAAMP_PROGRESSIVE::IsInitialCachingSupported() { return false; }

int StreamAbstractionAAMP_PROGRESSIVE::GetBWIndex(long bandwidth) { return 0; }

std::vector<long> StreamAbstractionAAMP_PROGRESSIVE::GetVideoBitrates(void) { std::vector<long> temp; return temp; }

std::vector<long> StreamAbstractionAAMP_PROGRESSIVE::GetAudioBitrates(void) { std::vector<long> temp; return temp; }

void StreamAbstractionAAMP_PROGRESSIVE::StopInjection(void) {  }

void StreamAbstractionAAMP_PROGRESSIVE::StartInjection(void) {  }

std::vector<StreamInfo*> StreamAbstractionAAMP_PROGRESSIVE::GetAvailableVideoTracks(void) { std::vector<StreamInfo*> temp; return temp; }

std::vector<StreamInfo*> StreamAbstractionAAMP_PROGRESSIVE::GetAvailableThumbnailTracks(void) { std::vector<StreamInfo*> temp; return temp; }

bool StreamAbstractionAAMP_PROGRESSIVE::SetThumbnailTrack(int) { return false; }

std::vector<ThumbnailData> StreamAbstractionAAMP_PROGRESSIVE::GetThumbnailRangeData(double, double, std::string*, int*, int*, int*, int*) { std::vector<ThumbnailData> temp; return temp; }

StreamInfo* StreamAbstractionAAMP_PROGRESSIVE::GetStreamInfo(int idx) { return nullptr; }

long StreamAbstractionAAMP_PROGRESSIVE::GetMaxBitrate()
{ 
    return 0;
}
