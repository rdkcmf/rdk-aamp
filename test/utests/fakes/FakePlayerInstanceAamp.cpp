/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License") {  }
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

#include "main_aamp.h"
#include "MockPlayerInstanceAAMP.h"

MockPlayerInstanceAAMP *g_mockPlayerInstanceAAMP = nullptr;

	PlayerInstanceAAMP::PlayerInstanceAAMP(StreamSink* streamSink, std::function< void(uint8_t *, int, int, int) > exportFrames) {  }
	PlayerInstanceAAMP::~PlayerInstanceAAMP() {  }

	void PlayerInstanceAAMP::Tune(const char *mainManifestUrl, const char *contentType, bool bFirstAttempt, bool bFinalAttempt,const char *traceUUID,bool audioDecoderStreamSync) {  }
	void PlayerInstanceAAMP::Tune(const char *mainManifestUrl, bool autoPlay, const char *contentType, bool bFirstAttempt, bool bFinalAttempt, const char *traceUUID,bool audioDecoderStreamSync) {  }
    void PlayerInstanceAAMP::Stop(bool sendStateChangeEvent) {  }
	void PlayerInstanceAAMP::ResetConfiguration() {  }
	void PlayerInstanceAAMP::SetRate(float rate, int overshootcorrection) {  }
	void PlayerInstanceAAMP::PauseAt(double  position) {  }
	void PlayerInstanceAAMP::Seek(double  secondsRelativeToTuneTime, bool keepPaused) {  }
	void PlayerInstanceAAMP::SeekToLive(bool keepPaused) {  }
	void PlayerInstanceAAMP::SetRateAndSeek(int rate, double  secondsRelativeToTuneTime) { }
	void PlayerInstanceAAMP::SetSlowMotionPlayRate (float rate ) {  }
	void PlayerInstanceAAMP::detach() {  }
	void PlayerInstanceAAMP::RegisterEvents(EventListener* eventListener) {  }
	void PlayerInstanceAAMP::UnRegisterEvents(EventListener* eventListener) {  }
	void PlayerInstanceAAMP::SetVideoRectangle(int x, int y, int w, int h) {  }
	void PlayerInstanceAAMP::SetVideoZoom(VideoZoomMode zoom) {  }
	void PlayerInstanceAAMP::SetVideoMute(bool muted) {  }
	void PlayerInstanceAAMP::SetSubtitleMute(bool muted) {  }
	void PlayerInstanceAAMP::SetAudioVolume(int volume) {  }
	void PlayerInstanceAAMP::SetLanguage(const char*  language) {  }
	void PlayerInstanceAAMP::SetSubscribedTags(std::vector<std::string> subscribedTags) {  }
	void PlayerInstanceAAMP::SubscribeResponseHeaders(std::vector<std::string> responseHeaders) {  }
	void PlayerInstanceAAMP::LoadJS(void* context) {  }
	void PlayerInstanceAAMP::UnloadJS(void* context) {  }
	void PlayerInstanceAAMP::AddEventListener(AAMPEventType eventType, EventListener* eventListener) {  }
	void PlayerInstanceAAMP::RemoveEventListener(AAMPEventType eventType, EventListener* eventListener) {  }
	void PlayerInstanceAAMP::InsertAd(const char *url, double  positionSeconds) {  }
	void PlayerInstanceAAMP::AddPageHeaders(std::map<std::string, std::string> customHttpHeaders) {  }
	void PlayerInstanceAAMP::AddCustomHTTPHeader(std::string headerName, std::vector<std::string> headerValue, bool isLicenseHeader) {  }
	void PlayerInstanceAAMP::SetLicenseServerURL(const char *url, DRMSystems type) {  }
	void PlayerInstanceAAMP::SetPreferredDRM(DRMSystems drmType) {  }
	void PlayerInstanceAAMP::SetStereoOnlyPlayback(bool bValue) {  }
	void PlayerInstanceAAMP::SetBulkTimedMetaReport(bool bValue) {  }
	void PlayerInstanceAAMP::SetRetuneForUnpairedDiscontinuity(bool bValue) {  }
	void PlayerInstanceAAMP::SetRetuneForGSTInternalError(bool bValue) {  }
	void PlayerInstanceAAMP::SetAnonymousRequest(bool isAnonymous) {  }
	void PlayerInstanceAAMP::SetAvgBWForABR(bool useAvgBW) {  }
	void PlayerInstanceAAMP::SetPreCacheTimeWindow(int nTimeWindow) {  }
	void PlayerInstanceAAMP::SetVODTrickplayFPS(int vodTrickplayFPS) {  }
	void PlayerInstanceAAMP::SetLinearTrickplayFPS(int linearTrickplayFPS) {  }
	void PlayerInstanceAAMP::SetLiveOffset(double liveoffset) {  }
	void PlayerInstanceAAMP::SetLiveOffset4K(double liveoffset) {  }
	void PlayerInstanceAAMP::SetStallErrorCode(int errorCode) {  }
	void PlayerInstanceAAMP::SetStallTimeout(int timeoutMS) {  }
	void PlayerInstanceAAMP::SetReportInterval(int reportInterval) {  }
	void PlayerInstanceAAMP::SetInitFragTimeoutRetryCount(int count) {  }
	void PlayerInstanceAAMP::SetVideoBitrate(long bitrate) {  }
	void PlayerInstanceAAMP::SetAudioBitrate(long bitrate) {  }
	void PlayerInstanceAAMP::SetInitialBitrate(long bitrate) {  }
	void PlayerInstanceAAMP::SetInitialBitrate4K(long bitrate4K) {  }
	void PlayerInstanceAAMP::SetNetworkTimeout(double  timeout) {  }
	void PlayerInstanceAAMP::SetManifestTimeout(double  timeout) {  }
	void PlayerInstanceAAMP::SetPlaylistTimeout(double  timeout) {  }
	void PlayerInstanceAAMP::SetDownloadBufferSize(int bufferSize) {  }
	void PlayerInstanceAAMP::SetNetworkProxy(const char * proxy) {  }
	void PlayerInstanceAAMP::SetLicenseReqProxy(const char * licenseProxy) {  }
	void PlayerInstanceAAMP::SetDownloadStallTimeout(long stallTimeout) {  }
	void PlayerInstanceAAMP::SetDownloadStartTimeout(long startTimeout) {  }
	void PlayerInstanceAAMP::SetDownloadLowBWTimeout(long lowBWTimeout) {  }
	void PlayerInstanceAAMP::SetPreferredSubtitleLanguage(const char*  language) {  }
	void PlayerInstanceAAMP::SetAlternateContents(const std::string &adBreakId, const std::string &adId, const std::string &url) {  }
	void PlayerInstanceAAMP::SetParallelPlaylistDL(bool bValue) {  }
    void PlayerInstanceAAMP::ManageAsyncTuneConfig(const char*  url) {  }
	void PlayerInstanceAAMP::SetAsyncTuneConfig(bool bValue) {  }
	void PlayerInstanceAAMP::SetParallelPlaylistRefresh(bool bValue) {  }
	void PlayerInstanceAAMP::SetWesterosSinkConfig(bool bValue) {  }
	void PlayerInstanceAAMP::SetLicenseCaching(bool bValue) {  }
	void PlayerInstanceAAMP::SetOutputResolutionCheck(bool bValue) {  }
	void PlayerInstanceAAMP::SetMatchingBaseUrlConfig(bool bValue) {  }
	void PlayerInstanceAAMP::SetPropagateUriParameters(bool bValue) {  }
    void PlayerInstanceAAMP::ApplyArtificialDownloadDelay(unsigned int DownloadDelayInMs) {  }
	void PlayerInstanceAAMP::SetSslVerifyPeerConfig(bool bValue) {  }
	void PlayerInstanceAAMP::SetNewABRConfig(bool bValue) {  }
	void PlayerInstanceAAMP::SetNewAdBreakerConfig(bool bValue) {  }
	void PlayerInstanceAAMP::SetVideoTracks(std::vector<long> bitrates) {  }
	void PlayerInstanceAAMP::SetAppName(std::string name) {  }
	void PlayerInstanceAAMP::SetPreferredLanguages(const char*  languageList, const char *preferredRendition, const char *preferredType, const char*  codecList, const char*  labelList) {  } 
	void PlayerInstanceAAMP::SetPreferredTextLanguages(const char*  param) {  } 
	void PlayerInstanceAAMP::SetAudioTrack(std::string language, std::string rendition, std::string type, std::string codec, unsigned int channel, std::string label) {  }
	void PlayerInstanceAAMP::SetPreferredCodec(const char *codecList) {  }
	void PlayerInstanceAAMP::SetPreferredLabels(const char *lableList) {  }
	void PlayerInstanceAAMP::SetPreferredRenditions(const char *renditionList) {  }
	void PlayerInstanceAAMP::SetTuneEventConfig(int tuneEventType) {  }
	void PlayerInstanceAAMP::EnableVideoRectangle(bool rectProperty) {  }
	void PlayerInstanceAAMP::SetRampDownLimit(int limit) {  }
	void PlayerInstanceAAMP::SetInitRampdownLimit(int limit) {  }
	void PlayerInstanceAAMP::SetMinimumBitrate(long bitrate) {  }
	void PlayerInstanceAAMP::SetMaximumBitrate(long bitrate) {  }
	void PlayerInstanceAAMP::SetSegmentInjectFailCount(int value) {  }
	void PlayerInstanceAAMP::SetSegmentDecryptFailCount(int value) {  }
	void PlayerInstanceAAMP::SetInitialBufferDuration(int durationSec) {  }
	void PlayerInstanceAAMP::SetNativeCCRendering(bool enable) {  }
	void PlayerInstanceAAMP::SetAudioTrack(int trackId) {  }
	void PlayerInstanceAAMP::SetTextTrack(int trackId, char *ccData) {  }
	void PlayerInstanceAAMP::SetCCStatus(bool enabled) {  }
	void PlayerInstanceAAMP::SetTextStyle(const std::string &options) 
    { 
    	if (g_mockPlayerInstanceAAMP != nullptr)
    	{
	    	g_mockPlayerInstanceAAMP->SetTextStyle(options);
	    }
    }
	void PlayerInstanceAAMP::SetLanguageFormat(LangCodePreference preferredFormat, bool useRole) {  }
	void PlayerInstanceAAMP::SetCEAFormat(int format) {  }
	void PlayerInstanceAAMP::SetSessionToken(std::string sessionToken) {  }
	void PlayerInstanceAAMP::SetMaxPlaylistCacheSize(int cacheSize) {  }
	void PlayerInstanceAAMP::EnableSeekableRange(bool enabled) {  }
	void PlayerInstanceAAMP::SetReportVideoPTS(bool enabled) {  }
	void PlayerInstanceAAMP::SetDisable4K(bool value) {  }
	void PlayerInstanceAAMP::DisableContentRestrictions(long grace, long time, bool eventChange) {  }
	void PlayerInstanceAAMP::EnableContentRestrictions() {  }
	void PlayerInstanceAAMP::AsyncStartStop() {  }
	void PlayerInstanceAAMP::PersistBitRateOverSeek(bool value) {  }
	void PlayerInstanceAAMP::SetPausedBehavior(int behavior) {  }
	void PlayerInstanceAAMP::SetUseAbsoluteTimeline(bool configState) {  }
	void PlayerInstanceAAMP::XRESupportedTune(bool xreSupported) {  }
	void PlayerInstanceAAMP::EnableAsyncOperation() {  }
	void PlayerInstanceAAMP::SetRepairIframes(bool configState) {  }
	void PlayerInstanceAAMP::SetAuxiliaryLanguage(const std::string &language) {  }
	void PlayerInstanceAAMP::SetLicenseCustomData(const char *customData) {  }
	void PlayerInstanceAAMP::SetContentProtectionDataUpdateTimeout(int timeout) {  }
	void PlayerInstanceAAMP::ProcessContentProtectionDataConfig(const char *jsonbuffer) {  }
	void PlayerInstanceAAMP::SetRuntimeDRMConfigSupport(bool DynamicDRMSupported) {  }
	bool PlayerInstanceAAMP::IsLive() { return false; }
	bool PlayerInstanceAAMP::GetVideoMute(void) { return false; }
	bool PlayerInstanceAAMP::GetCCStatus(void) { return false; }
	bool PlayerInstanceAAMP::SetThumbnailTrack(int thumbIndex) { return false; }
	bool PlayerInstanceAAMP::InitAAMPConfig(char *jsonStr) { return false; }
	int PlayerInstanceAAMP::GetVideoZoom(void) { return 0; }
	int PlayerInstanceAAMP::GetAudioVolume(void) { return 0; }
	int PlayerInstanceAAMP::GetPlaybackRate(void) { return 0; }
	int PlayerInstanceAAMP::GetRampDownLimit(void) { return 0; }
	int PlayerInstanceAAMP::GetInitialBufferDuration(void) { return 0; }
	int PlayerInstanceAAMP::GetAudioTrack() { return 0; }
	int PlayerInstanceAAMP::GetTextTrack() { return 0; }
	long PlayerInstanceAAMP::GetVideoBitrate(void) { return 0; }
	long PlayerInstanceAAMP::GetAudioBitrate(void) { return 0; }
	long PlayerInstanceAAMP::GetInitialBitrate(void) { return 0; }
	long PlayerInstanceAAMP::GetInitialBitrate4k(void) { return 0; }
	long PlayerInstanceAAMP::GetMinimumBitrate(void) { return 0; }
	long PlayerInstanceAAMP::GetMaximumBitrate(void) { return 0; }
	double PlayerInstanceAAMP::GetPlaybackPosition(void) { return 0; }
	double PlayerInstanceAAMP::GetPlaybackDuration(void) { return 0; }
	const char* PlayerInstanceAAMP::GetCurrentAudioLanguage() { return ""; }
	const char* PlayerInstanceAAMP::GetCurrentDRM() { return ""; }
    const char* PlayerInstanceAAMP::GetPreferredLanguages() { return ""; }
	DRMSystems PlayerInstanceAAMP::GetPreferredDRM() { return eDRM_NONE; }
	std::vector<long> PlayerInstanceAAMP::GetVideoBitrates(void) { static std::vector<long> temp; return temp; }
	std::vector<long> PlayerInstanceAAMP::GetAudioBitrates(void) { static std::vector<long> temp; return temp; }
    std::string PlayerInstanceAAMP::GetManifest(void) { return nullptr; }
	std::string PlayerInstanceAAMP::GetAvailableVideoTracks() { return nullptr; }
	std::string PlayerInstanceAAMP::GetAvailableAudioTracks(bool allTrack) { return nullptr; }
	std::string PlayerInstanceAAMP::GetAvailableTextTracks(bool allTrack) { return nullptr; }
	std::string PlayerInstanceAAMP::GetVideoRectangle() { return nullptr; }
	std::string PlayerInstanceAAMP::GetAudioTrackInfo() { return nullptr; }
	std::string PlayerInstanceAAMP::GetTextTrackInfo() { return nullptr; }
	std::string PlayerInstanceAAMP::GetPreferredAudioProperties() { return nullptr; }
	std::string PlayerInstanceAAMP::GetPreferredTextProperties() { return nullptr; }
	std::string PlayerInstanceAAMP::GetTextStyle() { return nullptr; }
	std::string PlayerInstanceAAMP::GetAvailableThumbnailTracks(void) { return nullptr; }
	std::string PlayerInstanceAAMP::GetThumbnails(double  sduration, double  eduration) { return nullptr; }
	std::string PlayerInstanceAAMP::GetAAMPConfig() { return nullptr; }
	std::string PlayerInstanceAAMP::GetPlaybackStats() { return nullptr; }
