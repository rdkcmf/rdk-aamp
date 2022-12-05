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

#ifndef AAMP_MOCK_STREAM_ABSTRACTION_AAMP_H
#define AAMP_MOCK_STREAM_ABSTRACTION_AAMP_H

#include <gmock/gmock.h>
#include "StreamAbstractionAAMP.h"

class MockStreamAbstractionAAMP : public StreamAbstractionAAMP
{
public:

    MockStreamAbstractionAAMP(AampLogManager *logObj, PrivateInstanceAAMP *aamp) : StreamAbstractionAAMP(logObj, aamp) { }

    MOCK_METHOD(void, NotifyPlaybackPaused, (bool paused));

    MOCK_METHOD(void, DumpProfiles, ());

    MOCK_METHOD(AAMPStatusType, Init, (TuneType tuneType));

    MOCK_METHOD(void, Start, ());

    MOCK_METHOD(void, Stop, (bool clearChannelData));

    MOCK_METHOD(void, GetStreamFormat, (StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat, StreamOutputFormat &subtitleOutputFormat));

    MOCK_METHOD(double, GetStreamPosition, ());

    MOCK_METHOD(double, GetFirstPTS, ());

    MOCK_METHOD(double, GetStartTimeOfFirstPTS, ());

    MOCK_METHOD(MediaTrack*, GetMediaTrack, (TrackType type));

    MOCK_METHOD(double, GetBufferedDuration, ());

    MOCK_METHOD(int, GetBWIndex, (long bandwidth));

    MOCK_METHOD(std::vector<long>, GetVideoBitrates, ());

    MOCK_METHOD(std::vector<long>, GetAudioBitrates, ());

    MOCK_METHOD(void, StartInjection, ());

    MOCK_METHOD(void, StopInjection, ());

    MOCK_METHOD(void, SeekPosUpdate, (double secondsRelativeToTuneTime));

    MOCK_METHOD(std::vector<StreamInfo*>, GetAvailableVideoTracks, ());

    MOCK_METHOD(std::vector<StreamInfo*>, GetAvailableThumbnailTracks, ());

    MOCK_METHOD(bool, SetThumbnailTrack, (int));

    MOCK_METHOD(std::vector<ThumbnailData>, GetThumbnailRangeData, (double, double, std::string*, int*, int*, int*, int*));

    MOCK_METHOD(StreamInfo* , GetStreamInfo, (int idx));

    MOCK_METHOD(bool , Is4KStream, (int &height, long &bandwidth));
};

extern MockStreamAbstractionAAMP *g_mockStreamAbstractionAAMP;

#endif /* AAMP_MOCK_STREAM_ABSTRACTION_AAMP_H */
