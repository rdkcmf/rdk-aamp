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

#include "fragmentcollector_hls.h"

StreamAbstractionAAMP_HLS::StreamAbstractionAAMP_HLS(AampLogManager *logObj, class PrivateInstanceAAMP *aamp,double seekpos, float rate) : StreamAbstractionAAMP(logObj, aamp)
{
}

StreamAbstractionAAMP_HLS::~StreamAbstractionAAMP_HLS()
{
}

void StreamAbstractionAAMP_HLS::DumpProfiles(void) {  }

AAMPStatusType StreamAbstractionAAMP_HLS::Init(TuneType tuneType) { return eAAMPSTATUS_OK; }

void StreamAbstractionAAMP_HLS::Start() {  }

void StreamAbstractionAAMP_HLS::Stop(bool clearChannelData) {  }

void StreamAbstractionAAMP_HLS::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat, StreamOutputFormat &subtitleOutputFormat) {  }

double StreamAbstractionAAMP_HLS::GetFirstPTS() { return 0; }

MediaTrack* StreamAbstractionAAMP_HLS::GetMediaTrack(TrackType type) { return nullptr; }

double StreamAbstractionAAMP_HLS::GetBufferedDuration (void) { return 0; }

int StreamAbstractionAAMP_HLS::GetBWIndex(long bandwidth) { return 0; }

std::vector<long> StreamAbstractionAAMP_HLS::GetVideoBitrates(void) { std::vector<long> temp; return temp; }

std::vector<long> StreamAbstractionAAMP_HLS::GetAudioBitrates(void) { std::vector<long> temp; return temp; }

void StreamAbstractionAAMP_HLS::StopInjection(void) {  }

void StreamAbstractionAAMP_HLS::StartInjection(void) {  }

std::vector<StreamInfo*> StreamAbstractionAAMP_HLS::GetAvailableVideoTracks(void) { std::vector<StreamInfo*> temp; return temp; }

std::vector<StreamInfo*> StreamAbstractionAAMP_HLS::GetAvailableThumbnailTracks(void) { std::vector<StreamInfo*> temp; return temp; }

bool StreamAbstractionAAMP_HLS::SetThumbnailTrack(int) { return false; }

std::vector<ThumbnailData> StreamAbstractionAAMP_HLS::GetThumbnailRangeData(double, double, std::string*, int*, int*, int*, int*) { std::vector<ThumbnailData> temp; return temp; }

StreamInfo* StreamAbstractionAAMP_HLS::GetStreamInfo(int idx) { return nullptr; }

void StreamAbstractionAAMP_HLS::StartSubtitleParser() { }

void StreamAbstractionAAMP_HLS::PauseSubtitleParser(bool pause) { }

void StreamAbstractionAAMP_HLS::NotifyFirstVideoPTS(unsigned long long pts, unsigned long timeScale){ }

void StreamAbstractionAAMP_HLS::SeekPosUpdate(double secondsRelativeToTuneTime) { }

void StreamAbstractionAAMP_HLS::ChangeMuxedAudioTrackIndex(std::string& index) { }
