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

#include "aampgstplayer.h"
#include "MockAampGstPlayer.h"
#include "priv_aamp.h"
#include "AampLogManager.h"

MockAAMPGstPlayer *g_mockAampGstPlayer = nullptr;

AAMPGstPlayer::AAMPGstPlayer(AampLogManager *logObj, PrivateInstanceAAMP *aamp)
{
}

AAMPGstPlayer::~AAMPGstPlayer()
{
}

void AAMPGstPlayer::Configure(StreamOutputFormat format, StreamOutputFormat audioFormat, StreamOutputFormat auxFormat, StreamOutputFormat subFormat, bool bESChangeStatus, bool forwardAudioToAux, bool setReadyAfterPipelineCreation)
{
}

void AAMPGstPlayer::SendCopy( MediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double duration) 
{
}

void AAMPGstPlayer::SendTransfer(MediaType mediaType, GrowableBuffer* pBuffer, double fpts, double fdts, double fDuration, bool initFragment, bool discontinuity)
{
}

void AAMPGstPlayer::EndOfStreamReached(MediaType mediaType)
{
}

void AAMPGstPlayer::Stream(void)
{
}

void AAMPGstPlayer::Stop(bool keepLastFrame)
{
}

void AAMPGstPlayer::DumpStatus(void)
{
}

void AAMPGstPlayer::Flush(double position, int rate, bool shouldTearDown)
{
}

bool AAMPGstPlayer::SetPlayBackRate ( double rate )
{
	return true;
}

bool AAMPGstPlayer::AdjustPlayBackRate(double position, double rate)
{ 
	return true; 
}

bool AAMPGstPlayer::Pause(bool pause, bool forceStopGstreamerPreBuffering)
{ 
	return true;
}

long AAMPGstPlayer::GetDurationMilliseconds(void)
{ 
	return 0; 
}

long AAMPGstPlayer::GetPositionMilliseconds(void)
{ 
	return 0; 
}

long long AAMPGstPlayer::GetVideoPTS(void)
{ 
	return 0; 
}

unsigned long AAMPGstPlayer::getCCDecoderHandle(void)
{ 
	return 0; 
}

void AAMPGstPlayer::SetVideoRectangle(int x, int y, int w, int h)
{
}

void AAMPGstPlayer::SetVideoZoom(VideoZoomMode zoom)
{
}

void AAMPGstPlayer::SetVideoMute(bool muted)
{
}

void AAMPGstPlayer::SetSubtitleMute(bool muted)
{
}

void AAMPGstPlayer::SetSubtitlePtsOffset(std::uint64_t pts_offset)
{
}

void AAMPGstPlayer::SetAudioVolume(int volume)
{
}

bool AAMPGstPlayer::Discontinuity( MediaType mediaType) 
{ 
	return true; 
}

bool AAMPGstPlayer::CheckForPTSChangeWithTimeout(long timeout) 
{ 
	return true; 
}

bool AAMPGstPlayer::IsCacheEmpty(MediaType mediaType)
{ 
	return true; 
}

void AAMPGstPlayer::ResetEOSSignalledFlag()
{
}

void AAMPGstPlayer::NotifyFragmentCachingComplete()
{
}

void AAMPGstPlayer::NotifyFragmentCachingOngoing()
{
}

void AAMPGstPlayer::GetVideoSize(int &w, int &h)
{
}

void AAMPGstPlayer::QueueProtectionEvent(const char *protSystemId, const void *ptr, size_t len, MediaType type) 
{
}

void AAMPGstPlayer::ClearProtectionEvent() 
{
}

void AAMPGstPlayer::SignalTrickModeDiscontinuity() 
{
}

void AAMPGstPlayer::SeekStreamSink(double position, double rate) 
{
}

std::string AAMPGstPlayer::GetVideoRectangle() 
{ 
	return std::string(); 
}

void AAMPGstPlayer::StopBuffering(bool forceStop)
{
}

void AAMPGstPlayer::InitializeAAMPGstreamerPlugins(AampLogManager *mLogObj)
{
}

