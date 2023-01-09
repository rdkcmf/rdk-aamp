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

#include "priv_aamp.h"
#include "MockPrivateInstanceAAMP.h"

//Enable the define below to get AAMP logging out when running tests
//#define ENABLE_LOGGING
#define TEST_LOG_LEVEL eLOGLEVEL_TRACE

MockPrivateInstanceAAMP *g_mockPrivateInstanceAAMP = nullptr;

PrivateInstanceAAMP::PrivateInstanceAAMP(AampConfig *config) : mConfig (config)
{
}

PrivateInstanceAAMP::~PrivateInstanceAAMP()
{
}

size_t PrivateInstanceAAMP::HandleSSLWriteCallback ( char *ptr, size_t size, size_t nmemb, void* userdata )
{
	return 0;
}

size_t PrivateInstanceAAMP::HandleSSLHeaderCallback ( const char *ptr, size_t size, size_t nmemb, void* user_data )
{
	return 0;
}

int PrivateInstanceAAMP::HandleSSLProgressCallback ( void *clientp, double dltotal, double dlnow, double ultotal, double ulnow )
{
	return 0;
}

void PrivateInstanceAAMP::SetStreamSink(StreamSink* streamSink)
{
}

void PrivateInstanceAAMP::GetState(PrivAAMPState& state)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->GetState(state);
	}
}

void PrivateInstanceAAMP::SetState(PrivAAMPState state)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->SetState(state);
	}
}

void PrivateInstanceAAMP::Stop()
{
}

bool PrivateInstanceAAMP::IsActiveInstancePresent()
{
	return true;
}

void PrivateInstanceAAMP::StartPausePositionMonitoring(long long pausePositionMilliseconds)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->StartPausePositionMonitoring(pausePositionMilliseconds);
	}
}

void PrivateInstanceAAMP::StopPausePositionMonitoring(std::string reason)
{
	if (g_mockPrivateInstanceAAMP != nullptr)
	{
		g_mockPrivateInstanceAAMP->StopPausePositionMonitoring(reason);
	}
}

AampCacheHandler * PrivateInstanceAAMP::getAampCacheHandler()
{
	return nullptr;
}

void PrivateInstanceAAMP::Tune(const char *mainManifestUrl, bool autoPlay, const char *contentType, bool bFirstAttempt, bool bFinalAttempt,const char *pTraceID,bool audioDecoderStreamSync)
{
}

void PrivateInstanceAAMP::detach()
{
}

void PrivateInstanceAAMP::NotifySpeedChanged(float rate, bool changeState)
{
}

void PrivateInstanceAAMP::LogPlayerPreBuffered(void)
{
}

bool PrivateInstanceAAMP::IsLive()
{
	return false;
}

void PrivateInstanceAAMP::NotifyOnEnteringLive()
{
}

bool PrivateInstanceAAMP::GetPauseOnFirstVideoFrameDisp(void)
{
	return false;
}

long long PrivateInstanceAAMP::GetPositionMilliseconds()
{
	return 0;
}

bool PrivateInstanceAAMP::SetStateBufferingIfRequired()
{
	return false;
}

void PrivateInstanceAAMP::NotifyFirstBufferProcessed()
{
}

void PrivateInstanceAAMP::StopDownloads()
{
}

void PrivateInstanceAAMP::ResumeDownloads()
{
}

void PrivateInstanceAAMP::EnableDownloads()
{
}

void PrivateInstanceAAMP::AcquireStreamLock()
{
}

void PrivateInstanceAAMP::TuneHelper(TuneType tuneType, bool seekWhilePaused)
{
}

void PrivateInstanceAAMP::ReleaseStreamLock()
{
}

bool PrivateInstanceAAMP::IsFragmentCachingRequired()
{
	return false;
}

void PrivateInstanceAAMP::TeardownStream(bool newTune)
{
}

void PrivateInstanceAAMP::SetVideoRectangle(int x, int y, int w, int h)
{
}

void PrivateInstanceAAMP::SetVideoZoom(VideoZoomMode zoom)
{
}

bool PrivateInstanceAAMP::TryStreamLock()
{
	return false;
}

void PrivateInstanceAAMP::SetVideoMute(bool muted)
{
}

void PrivateInstanceAAMP::SetSubtitleMute(bool muted)
{
}

void PrivateInstanceAAMP::SetAudioVolume(int volume)
{
}

void PrivateInstanceAAMP::AddEventListener(AAMPEventType eventType, EventListener* eventListener)
{
}

void PrivateInstanceAAMP::RemoveEventListener(AAMPEventType eventType, EventListener* eventListener)
{
}

std::shared_ptr<AampDrmHelper> PrivateInstanceAAMP::GetCurrentDRM(void)
{
	return nullptr;
}

void PrivateInstanceAAMP::AddCustomHTTPHeader(std::string headerName, std::vector<std::string> headerValue, bool isLicenseHeader)
{
}

void PrivateInstanceAAMP::SetLiveOffsetAppRequest(bool LiveOffsetAppRequest)
{
}

long long PrivateInstanceAAMP::GetDurationMs()
{
	return 0;
}

ContentType PrivateInstanceAAMP::GetContentType() const
{
	return ContentType_UNKNOWN;
}

void PrivateInstanceAAMP::SetAlternateContents(const std::string &adBreakId, const std::string &adId, const std::string &url)
{
}

void SetPreferredLanguages(const char *languageList, const char *preferredRendition, const char *preferredType, const char *codecList, const char *labelList )
{
}

std::string PrivateInstanceAAMP::GetPreferredAudioProperties()
{
	return nullptr;
}

std::string PrivateInstanceAAMP::GetPreferredTextProperties()
{
	return nullptr;
}

void PrivateInstanceAAMP::SetPreferredTextLanguages(const char *param )
{
}

DRMSystems PrivateInstanceAAMP::GetPreferredDRM()
{
	return eDRM_NONE;
}

std::string PrivateInstanceAAMP::GetAvailableVideoTracks()
{
	return nullptr;
}

void PrivateInstanceAAMP::SetVideoTracks(std::vector<long> bitrateList)
{
}

std::string PrivateInstanceAAMP::GetAudioTrackInfo()
{
	return nullptr;
}

std::string PrivateInstanceAAMP::GetTextTrackInfo()
{
	return nullptr;
}

int PrivateInstanceAAMP::GetTextTrack()
{
	return 0;
}

std::string PrivateInstanceAAMP::GetAvailableTextTracks(bool allTrack)
{
	return nullptr;
}

std::string PrivateInstanceAAMP::GetAvailableAudioTracks(bool allTrack)
{
	return nullptr;
}

std::string PrivateInstanceAAMP::GetVideoRectangle()
{
	return nullptr;
}

void PrivateInstanceAAMP::SetAppName(std::string name)
{
}

int PrivateInstanceAAMP::GetAudioTrack()
{
	return 0;
}

void PrivateInstanceAAMP::SetCCStatus(bool enabled)
{
}

bool PrivateInstanceAAMP::GetCCStatus(void)
{
	return false;
}

void PrivateInstanceAAMP::SetTextStyle(const std::string &options)
{
}

std::string PrivateInstanceAAMP::GetTextStyle()
{
	return nullptr;
}

std::string PrivateInstanceAAMP::GetThumbnailTracks()
{
	return nullptr;
}

std::string PrivateInstanceAAMP::GetThumbnails(double tStart, double tEnd)
{
	return nullptr;
}

void PrivateInstanceAAMP::DisableContentRestrictions(long grace, long time, bool eventChange)
{
}

void PrivateInstanceAAMP::EnableContentRestrictions()
{
}

MediaFormat PrivateInstanceAAMP::GetMediaFormatType(const char *url)
{
	return eMEDIAFORMAT_UNKNOWN;
}

void PrivateInstanceAAMP::SetEventPriorityAsyncTune(bool bValue)
{
}

bool PrivateInstanceAAMP::IsTuneCompleted()
{
	return false;
}

void PrivateInstanceAAMP::TuneFail(bool fail)
{
}

std::string PrivateInstanceAAMP::GetPlaybackStats()
{
	return nullptr;
}

void PrivateInstanceAAMP::individualization(const std::string& payload)
{
}

void PrivateInstanceAAMP::SetTextTrack(int trackId, char *data)
{
}

bool PrivateInstanceAAMP::LockGetPositionMilliseconds()
{
	return false;
}

void PrivateInstanceAAMP::UnlockGetPositionMilliseconds()
{
}

void PrivateInstanceAAMP::SetPreferredLanguages(char const*, char const*, char const*, char const*, char const*)
{
}
