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

#include "StreamAbstractionAAMP.h"
#include "MockStreamAbstractionAAMP.h"

MockStreamAbstractionAAMP *g_mockStreamAbstractionAAMP = nullptr;

StreamAbstractionAAMP::StreamAbstractionAAMP(AampLogManager *logObj, PrivateInstanceAAMP* aamp)
{
}

StreamAbstractionAAMP::~StreamAbstractionAAMP()
{
}

void StreamAbstractionAAMP::DisablePlaylistDownloads()
{
}

bool StreamAbstractionAAMP::IsMuxedStream()
{
    return false;
}

double StreamAbstractionAAMP::GetLastInjectedFragmentPosition()
{
    return 0.0;
}

double StreamAbstractionAAMP::GetBufferedVideoDurationSec()
{
    return 0.0;
}

void StreamAbstractionAAMP::MuteSubtitles(bool mute)
{
}

void StreamAbstractionAAMP::RefreshSubtitles()
{
}

long StreamAbstractionAAMP::GetVideoBitrate(void)
{
    return 0;
}

void StreamAbstractionAAMP::SetAudioTrackInfoFromMuxedStream(std::vector<AudioTrackInfo>& vector)
{
}

long StreamAbstractionAAMP::GetAudioBitrate(void)
{
    return 0;
}

int StreamAbstractionAAMP::GetAudioTrack()
{
    return 0;
}

bool StreamAbstractionAAMP::GetCurrentAudioTrack(AudioTrackInfo &audioTrack)
{
    return false;
}

int StreamAbstractionAAMP::GetTextTrack()
{
    return 0;
}

bool StreamAbstractionAAMP::GetCurrentTextTrack(TextTrackInfo &textTrack)
{
    return 0;
}

bool StreamAbstractionAAMP::IsInitialCachingSupported()
{
	return false;
}

void StreamAbstractionAAMP::NotifyPlaybackPaused(bool paused)
{
    if (g_mockStreamAbstractionAAMP != nullptr)
    {
        g_mockStreamAbstractionAAMP->NotifyPlaybackPaused(paused);
    }
}

bool StreamAbstractionAAMP::IsEOSReached()
{
    return false;
}

bool StreamAbstractionAAMP::GetPreferredLiveOffsetFromConfig()
{
    return false;
}

void MediaTrack::OnSinkBufferFull()
{
}

BufferHealthStatus MediaTrack::GetBufferStatus()
{
    return BUFFER_STATUS_GREEN;
}

bool StreamAbstractionAAMP::SetTextStyle(const std::string &options)
{
    return false;
}
