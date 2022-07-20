/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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

/**
 * @file fragmentcollector_mpd.h
 * @brief Fragment collector MPEG DASH declarations
 */

#ifndef FRAGMENTCOLLECTOR_MPD_H_
#define FRAGMENTCOLLECTOR_MPD_H_

#include "StreamAbstractionAAMP.h"
#include "AampJsonObject.h" /**< For JSON parsing */
#include <string>
#include <stdint.h>
#include "libdash/IMPD.h"
#include "libdash/INode.h"
#include "libdash/IDASHManager.h"
#include "libdash/IProducerReferenceTime.h"
#include "libdash/xml/Node.h"
#include "libdash/helpers/Time.h"
#include "libdash/xml/DOMParser.h"
#include <libxml/xmlreader.h>
#include <thread>
#include "admanager_mpd.h"

using namespace dash;
using namespace std;
using namespace dash::mpd;
using namespace dash::xml;
using namespace dash::helpers;
#define MAX_MANIFEST_DOWNLOAD_RETRY_MPD 2

/*Common MPD util functions (admanager_mpd.cpp and fragmentcollector_mpd.cpp */
double aamp_GetPeriodNewContentDuration(dash::mpd::IMPD *mpd, IPeriod * period, uint64_t &curEndNumber);
/**
 * @fn aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset
 * @param period period of segment
 */
double aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset(IPeriod * period);
/**
 * @fn aamp_GetPeriodDuration
 * @param  mpd manifest ptr
 * @param  periodIndex Index of the current period
 */
double aamp_GetPeriodDuration(dash::mpd::IMPD *mpd, int periodIndex, uint64_t mpdDownloadTime = 0);
/**
 * @fn aamp_ProcessNode
 * @param[in] reader Pointer to reader object
 * @param[in] url    manifest url
 */
Node* aamp_ProcessNode(xmlTextReaderPtr *reader, std::string url, bool isAd = false);
/**
 * @fn aamp_GetDurationFromRepresentation
 * @param mpd manifest ptr
 */
uint64_t aamp_GetDurationFromRepresentation(dash::mpd::IMPD *mpd);

/**
 * @struct ProfileInfo
 * @brief Manifest file adaptation and representation info
 */ 
struct ProfileInfo
{
	int adaptationSetIndex;
	int representationIndex;
};

/**
 * @struct FragmentDescriptor
 * @brief Stores information of dash fragment
 */
struct FragmentDescriptor
{
private :
	std::string matchingBaseURL;
public :
	std::string manifestUrl;
	uint32_t Bandwidth;
	std::string RepresentationID;
	uint64_t Number;
	double Time;
	bool bUseMatchingBaseUrl;

	FragmentDescriptor() : manifestUrl(""), Bandwidth(0), Number(0), Time(0), RepresentationID(""),matchingBaseURL(""),bUseMatchingBaseUrl(false)
	{
	}
	
	FragmentDescriptor(const FragmentDescriptor& p) : manifestUrl(p.manifestUrl), Bandwidth(p.Bandwidth), RepresentationID(p.RepresentationID), Number(p.Number), Time(p.Time),matchingBaseURL(p.matchingBaseURL),bUseMatchingBaseUrl(p.bUseMatchingBaseUrl)
	{
	}

	FragmentDescriptor& operator=(const FragmentDescriptor &p)
	{
		manifestUrl = p.manifestUrl;
		RepresentationID.assign(p.RepresentationID);
		Bandwidth = p.Bandwidth;
		Number = p.Number;
		Time = p.Time;
		matchingBaseURL = p.matchingBaseURL;
		return *this;
	}
	std::string GetMatchingBaseUrl() const
	{
		return matchingBaseURL;
	}

	void ClearMatchingBaseUrl()
	{
		matchingBaseURL.clear();
	}
	void AppendMatchingBaseUrl( const std::vector<IBaseUrl *>*baseUrls )
	{
		if( baseUrls && baseUrls->size()>0 )
		{
			const std::string &url = baseUrls->at(0)->GetUrl();
			if( url.empty() )
			{
			}
			else if( aamp_IsAbsoluteURL(url) )
			{
				if(bUseMatchingBaseUrl)
				{
					std::string prefHost = aamp_getHostFromURL(manifestUrl);
					for (auto & item : *baseUrls) {
						std::string itemUrl = item->GetUrl();
						std::string host  = aamp_getHostFromURL(itemUrl);
						if(0 == prefHost.compare(host))
						{
							matchingBaseURL = item->GetUrl();
							return;
						}
					}
				}
				matchingBaseURL = url;
			}
			else if( url.rfind("/",0)==0 )
			{
				matchingBaseURL = aamp_getHostFromURL(matchingBaseURL);
				matchingBaseURL += url;
				AAMPLOG_WARN( "baseURL with leading /" );                     
			}
			else
			{
				if( !matchingBaseURL.empty() && matchingBaseURL.back() != '/' )
				{ // add '/' delimiter only if parent baseUrl doesn't already end with one
					matchingBaseURL += "/";
				}
				matchingBaseURL += url;
			}
		}
	}
};

/**
 * @class StreamAbstractionAAMP_MPD
 * @brief Fragment collector for MPEG DASH
 */
class StreamAbstractionAAMP_MPD : public StreamAbstractionAAMP
{
public:
	/**
	 * @fn StreamAbstractionAAMP_MPD
	 * @param aamp pointer to PrivateInstanceAAMP object associated with player
	 * @param seek_pos Seek position
	 * @param rate playback rate
	 */
	StreamAbstractionAAMP_MPD(AampLogManager *logObj, class PrivateInstanceAAMP *aamp,double seekpos, float rate);
	/**
	 * @fn ~StreamAbstractionAAMP_MPD
	 */
	~StreamAbstractionAAMP_MPD();
	/**
         * @fn StreamAbstractionAAMP_MPD Copy constructor disabled
         *
         */
  	StreamAbstractionAAMP_MPD(const StreamAbstractionAAMP_MPD&) = delete;
	/**
         * @fn StreamAbstractionAAMP_MPD assignment operator disabled
         *
         */
	StreamAbstractionAAMP_MPD& operator=(const StreamAbstractionAAMP_MPD&) = delete;
	/**
	 * @fn DumpProfiles
	 */
	void DumpProfiles(void) override;
	/**
	 * @fn Start
	 * @return void
	 */
	void Start() override;
	/**
	 * @fn Stop
	 * @param  clearChannelData - ignored.
	 */
	void Stop(bool clearChannelData) override;
	/**
	 * @fn Init
	 * @param  tuneType to set type of object.
	 */
	AAMPStatusType Init(TuneType tuneType) override;
	/**
	 * @fn GetStreamFormat
	 * @param[out]  primaryOutputFormat - format of primary track
	 * @param[out]  audioOutputFormat - format of audio track
	 * @param[out]  auxOutputFormat - format of aux audio track
	 * @param[out]  subtitleOutputFormat - format of sutbtile track
	 */
	void GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxOutputFormat, StreamOutputFormat &subtitleOutputFormat) override;
	/**
	 * @fn GetStreamPosition
	 */
	double GetStreamPosition() override;
	/**
	 * @fn GetMediaTrack
	 * @param[in]  type - track type
	 */
	MediaTrack* GetMediaTrack(TrackType type) override;
	/**
	 * @fn GetFirstPTS
	 */
	double GetFirstPTS() override;
	/**
	 * @fn GetStartTimeOfFirstPTS
	 */
	double GetStartTimeOfFirstPTS() override;
	/**
	 * @fn GetBWIndex
	 * @param[in] bitrate Bitrate to lookup profile
	 */
	int GetBWIndex(long bitrate) override;
	/**
	 * @fn GetVideoBitrates
	 */
	std::vector<long> GetVideoBitrates(void) override;
	/**
	 * @fn GetAudioBitrates
	 */
	std::vector<long> GetAudioBitrates(void) override;
	/**
	 * @fn GetMaxBitrate
	 */
	long GetMaxBitrate(void) override;
	/**
	 * @fn StopInjection
	 * @return void
	 */
	void StopInjection(void) override;
	/**
	 * @fn StartInjection
	 * @return void
	 */
	void StartInjection(void) override;
	double GetBufferedDuration();
	void SeekPosUpdate(double secondsRelativeToTuneTime) {seekPosition = secondsRelativeToTuneTime; }
	virtual void SetCDAIObject(CDAIObject *cdaiObj) override;
	/**
	 * @fn GetAvailableAudioTracks
	 * @param[in] tracks - available audio tracks in period
 	 * @param[in] trackIndex - index of current audio track
	 */
	virtual std::vector<AudioTrackInfo> & GetAvailableAudioTracks(bool allTrack=false) override;
	/**
         * @brief Gets all/current available text tracks
         * @retval vector of tracks
         */
	virtual std::vector<TextTrackInfo>& GetAvailableTextTracks(bool allTrack=false) override;
	
	/**
         * @fn GetProfileCount
         * 
         */
	int GetProfileCount();
	/**
	 * @fn GetProfileIndexForBandwidth
	 * @param mTsbBandwidth - bandwidth to identify profile index from list
	 */
	int GetProfileIndexForBandwidth(long mTsbBandwidth);
	/**
	 * @fn GetAvailableThumbnailTracks
	 */
	std::vector<StreamInfo*> GetAvailableThumbnailTracks(void) override;
	/**
	 * @fn GetAvailableVideoTracks
	 */
	std::vector<StreamInfo*> GetAvailableVideoTracks(void) override;
	/**
	 * @fn SetThumbnailTrack
	 * @param thumbnail index value indicating the track to select
	 */
	bool SetThumbnailTrack(int) override;
	/**
	 * @fn GetThumbnailRangeData
	 * @param tStart start duration of thumbnail data.
	 * @param tEnd end duration of thumbnail data.
	 * @param *baseurl base url of thumbnail images.
	 * @param *raw_w absolute width of the thumbnail spritesheet.
	 * @param *raw_h absolute height of the thumbnail spritesheet.
	 * @param *width width of each thumbnail tile.
	 * @param *height height of each thumbnail tile.
	 */
	std::vector<ThumbnailData> GetThumbnailRangeData(double,double, std::string*, int*, int*, int*, int*) override;

	// ideally below would be private, but called from MediaStreamContext
	/**
	 * @fn GetAdaptationSetAtIndex
	 * @param[in] idx - Adaptation Set Index
	 */
	const IAdaptationSet* GetAdaptationSetAtIndex(int idx);
	/**
	 * @fn GetAdaptationSetAndRepresetationIndicesForProfile
	 * @param[in] idx - Profile Index
	 */
	struct ProfileInfo GetAdaptationSetAndRepresetationIndicesForProfile(int profileIndex);
	int64_t GetMinUpdateDuration() { return mMinUpdateDurationMs;}
	/**
	 * @fn FetchFragment
	 * @param pMediaStreamContext Track object pointer
	 * @param media media descriptor string
	 * @param fragmentDuration duration of fragment in seconds
	 * @param isInitializationSegment true if fragment is init fragment
	 * @param curlInstance curl instance to be used to fetch
	 * @param discontinuity true if fragment is discontinuous
	 * @param pto unscaled pto value from mpd
	 * @param scale timeScale value from mpd
	 */
	bool FetchFragment( class MediaStreamContext *pMediaStreamContext, std::string media, double fragmentDuration, bool isInitializationSegment, unsigned int curlInstance, bool discontinuity = false, double pto = 0 , uint32_t scale = 0);
	/**
	 * @fn PushNextFragment 
	 * @param pMediaStreamContext Track object
	 * @param curlInstance instance of curl to be used to fetch
	 */
	bool PushNextFragment( class MediaStreamContext *pMediaStreamContext, unsigned int curlInstance);
	/**
	 * @fn GetFirstPeriodStartTime
	 */
	double GetFirstPeriodStartTime(void);
	void MonitorLatency();
	void StartSubtitleParser() override;
	void PauseSubtitleParser(bool pause) override;
	/**
	 * @fn GetCurrPeriodTimeScale
	 */
	uint32_t GetCurrPeriodTimeScale();
	dash::mpd::IMPD *GetMPD( void );
	IPeriod *GetPeriod( void );
	/**
	 * @fn GetPreferredTextRepresentation
	 * @param[in] adaptationSet Adaptation set object
	 * @param[out] selectedRepIdx - Selected representation index
	 * @param[out] selectedRepBandwidth - selected audio track bandwidth
 	 */
	void GetPreferredTextRepresentation(IAdaptationSet *adaptationSet, int &selectedRepIdx,	uint32_t &selectedRepBandwidth, uint64_t &score, std::string &name, std::string &codec);
	static Accessibility getAccessibilityNode(void *adaptationSet);
	static Accessibility getAccessibilityNode(AampJsonObject &accessNode);
	/**
	 * @fn GetBestTextTrackByLanguage
	 * @param[out] selectedTextTrack selected representation Index
	 */
	bool GetBestTextTrackByLanguage( TextTrackInfo &selectedTextTrack);
	/**
	 * @fn ParseTrackInformation
	 * @param adaptationSet Adaptation Node
	 * @param MediaType type of media
	 * @param[out] aTracks audio tracks
	 * @param[out] tTracks text tracks
	 */
	void ParseTrackInformation(IAdaptationSet *adaptationSet, uint32_t iAdaptationIndex, MediaType media, std::vector<AudioTrackInfo> &aTracks, std::vector<TextTrackInfo> &tTracks);
private:
	/**
	 * @fn printSelectedTrack
	 * @param[in] trackIndex - selected track index
	 * @param[in] media - Media type
	 */
	void printSelectedTrack(const std::string &trackIndex, MediaType media);
	/**
	 * @fn AdvanceTrack
	 * @return void
	 */
	void AdvanceTrack(int trackIdx, bool trickPlay, double delta, bool *waitForFreeFrag, bool *exitFetchLoop, bool *bCacheFullState);
	/**
	 * @fn FetcherLoop
	 * @return void
	 */
	void FetcherLoop();
	/**
	 * @fn GetStreamInfo
	 * @param[in]  idx - profile index.
	 */
	StreamInfo* GetStreamInfo(int idx) override;
	/**
	 * @fn CheckLLProfileAvailable
	 * @param mpd Pointer to manifest
	 */
	bool CheckLLProfileAvailable(IMPD *mpd);
	/**
	 * @fn ParseMPDLLData
	 * @param[In] mpd Pointer to FragmentCollector
	 * @param[Out] stAampLLDashServiceData Reference to LowLatency element parsed data
	 * @retval bool true if successfully Parsed Low Latency elements. Else false
	 */
	bool ParseMPDLLData(MPD* mpd, AampLLDashServiceData &stAampLLDashServiceData);
	/**
	 * @fn UpdateMPD
	 * @param init retrievePlaylistFromCache true to try to get from cache
	 */
	AAMPStatusType UpdateMPD(bool init = false);
	/**
	 * @fn FindServerUTCTime
	 * @param mpd:  MPD top level element
	 * @param root: XML root node
	 */ 
	bool FindServerUTCTime(Node* root);
	/**
	 * @fn FindTimedMetadata
	 * @param mpd MPD top level element
	 * @param root XML root node
	 * @param init true if this is the first playlist download for a tune/seek/trickplay
	 * @param reportBulkMeta true if bulkTimedMetadata feature is enabled
	 */
	void FindTimedMetadata(MPD* mpd, Node* root, bool init = false, bool reportBulkMet = false);
	/**
	 * @fn ProcessPeriodSupplementalProperty
	 * @param node SupplementalProperty node
	 * @param[out] AdID AD Id
	 * @param startMS start time in MS
	 * @param durationMS duration in MS
	 * @param isInit true if its the first playlist download
	 * @param reportBulkMeta true if bulk metadata is enabled
	 */
	void ProcessPeriodSupplementalProperty(Node* node, std::string& AdID, uint64_t startMS, uint64_t durationMS, bool isInit, bool reportBulkMeta=false);
	/**
	 * @fn ProcessPeriodAssetIdentifier
	 * @param node AssetIdentifier node
	 * @param startMS start time MS
	 * @param durationMS duration MS
	 * @param AssetID Asset Id
	 * @param ProviderID Provider Id
	 * @param isInit true if its the first playlist download
	 * @param reportBulkMeta true if bulk metadata is enabled
	 */
	void ProcessPeriodAssetIdentifier(Node* node, uint64_t startMS, uint64_t durationMS, std::string& assetID, std::string& providerID,bool isInit, bool reportBulkMeta=false);
	/**
	 * @fn ProcessEventStream
	 * @param[in] period instance.
	 * @param reportBulkMeta true if bulk metadata is enabled
	 */
	bool ProcessEventStream(uint64_t startMS, IPeriod * period, bool reportBulkMeta);
	/**
	 * @fn ProcessStreamRestrictionList
	 * @param node StreamRestrictionListType node
	 * @param AdID Ad Id
	 * @param startMS start time MS
	 * @param isInit true if its the first playlist download
	 * @param reportBulkMeta true if bulk metadata is enabled
	 */
	void ProcessStreamRestrictionList(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta);
	/**
	 * @fn ProcessStreamRestriction
	 * @param node StreamRestriction xml node
	 * @param AdID Ad ID
	 * @param startMS Start time in MS
	 * @param isInit true if its the first playlist download
	 * @param reportBulkMeta true if bulk metadata is enabled
	 */
	void ProcessStreamRestriction(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta);
	/**
	 * @fn ProcessStreamRestrictionExt
	 * @param node Ext child of StreamRestriction xml node
	 * @param AdID Ad ID
	 * @param startMS start time in ms
	 * @param isInit true if its the first playlist download
	 * @param reportBulkMeta true if bulk metadata is enabled
	 */
	void ProcessStreamRestrictionExt(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta);
	/**
	 * @fn ProcessTrickModeRestriction
	 * @param node TrickModeRestriction xml node
	 * @param AdID Ad ID
	 * @param startMS start time in ms
	 * @param isInit true if its the first playlist download
	 * @param reportBulkMeta true if bulk metadata is enabled
	 */
	void ProcessTrickModeRestriction(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta);
	/**
	 * @fn FetchAndInjectInitFragments
	 * @param discontinuity number of tracks and discontinuity true if discontinuous fragment
	 */
	void FetchAndInjectInitFragments(bool discontinuity = false);
	/**
	 * @fn FetchAndInjectInitialization
	 * @param trackIdx,discontinuity  number of tracks and discontinuity true if discontinuous fragment
	 */
	void FetchAndInjectInitialization(int trackIdx, bool discontinuity = false);
	/**
	 * @fn StreamSelection
	 * @param newTune true if this is a new tune
	 */
	void StreamSelection(bool newTune = false, bool forceSpeedsChangedEvent = false);
	/**
	 * @fn CheckForInitalClearPeriod
	 */
	bool CheckForInitalClearPeriod();
	/**
	 * @fn PushEncryptedHeaders
	 * @return void
	 */
	void PushEncryptedHeaders();
	/**
	 * @fn GetProfileIdxForBandwidthNotification
	 * @param bandwidth - bandwidth to identify profile index from list
	 */
	int GetProfileIdxForBandwidthNotification(uint32_t bandwidth);
	/**
	 * @fn GetCurrentMimeType
	 * @param MediaType type of media
	 * @retval mimeType
	 */
	std::string GetCurrentMimeType(MediaType mediaType);
	/**
	 * @fn UpdateTrackInfo
	 */
	AAMPStatusType UpdateTrackInfo(bool modifyDefaultBW, bool periodChanged, bool resetTimeLineIndex=false);
	/**
	 * @fn SkipFragments
	 * @param pMediaStreamContext Media track object
	 * @param skipTime time to skip in seconds
	 * @param updateFirstPTS true to update first pts state variable
 	 */
	double SkipFragments( class MediaStreamContext *pMediaStreamContext, double skipTime, bool updateFirstPTS = false, bool skipToEnd = false);	
	/**
	 * @fn SkipToEnd
	 * @param pMediaStreamContext Track object pointer
 	 */
	void SkipToEnd( class MediaStreamContext *pMediaStreamContext); //Added to support rewind in multiperiod assets
	/**
	 * @fn ProcessContentProtection 
	 * @param adaptationSet Adaptation set object
	 * @param mediaType type of track
	 */
	void ProcessContentProtection(IAdaptationSet * adaptationSet,MediaType mediaType, std::shared_ptr<AampDrmHelper> drmHelper = nullptr);
	/**
	 * @fn SeekInPeriod
	 * @param seekPositionSeconds seek positon in seconds
	 */
	void SeekInPeriod( double seekPositionSeconds, bool skipToEnd = false);	
	/**
	 * @fn ApplyLiveOffsetWorkaroundForSAP
	 * @param seekPositionSeconds seek positon in seconds.
 	 */
	void ApplyLiveOffsetWorkaroundForSAP(double seekPositionSeconds);
	/**
	 * @fn GetCulledSeconds
	 */
	double GetCulledSeconds();
	/**
	 * @fn UpdateCulledAndDurationFromPeriodInfo
	 */
	void UpdateCulledAndDurationFromPeriodInfo();
	/**
	 * @fn UpdateLanguageList
	 * @return void
	 */
	void UpdateLanguageList();
	/**
	 * @fn GetBestAudioTrackByLanguage
	 * @param desiredRepIdx [out] selected representation Index
	 * @param CodecType [out] selected codec type
	 * @param ac4Tracks parsed track from preselection node
	 * @param audioTrackIndex selected audio track index
	 */
	int GetBestAudioTrackByLanguage(int &desiredRepIdx,AudioType &selectedCodecType, std::vector<AudioTrackInfo> &ac4Tracks, std::string &audioTrackIndex);
	int GetPreferredAudioTrackByLanguage();
	/**
	 * @fn CheckProducerReferenceTimeUTCTimeMatch
	 * @param pRT Pointer to ProducerReferenceTime
	 */
	bool CheckProducerReferenceTimeUTCTimeMatch(IProducerReferenceTime *pRT);
	/**
	 * @fn PrintProducerReferenceTimeAtrributes
	 * @param pRT Pointer to ProducerReferenceTime
	 */
	void PrintProducerReferenceTimeAtrributes(IProducerReferenceTime *pRT);
	/**
	 * @fn GetProducerReferenceTimeForAdaptationSet
	 * @param adaptationSet Pointer to AdaptationSet
	 */
	IProducerReferenceTime *GetProducerReferenceTimeForAdaptationSet(IAdaptationSet *adaptationSet);
	/**
	 * @fn GetLanguageForAdaptationSet
	 * @param adaptationSet Pointer to adaptation set
	 * @retval language of adaptation set
	 */
	std::string GetLanguageForAdaptationSet( IAdaptationSet *adaptationSet );
	/**
	 * @fn GetMpdFromManfiest
	 * @param manifest buffer pointer
	 * @param mpd MPD object of manifest
	 * @param manifestUrl manifest url
	 * @param init true if this is the first playlist download for a tune/seek/trickplay
	 */
	AAMPStatusType GetMpdFromManfiest(const GrowableBuffer &manifest, MPD * &mpd, std::string manifestUrl, bool init = false);
	/**
	 * @fn GetDrmPrefs
	 * @param The UUID for the DRM type
	 */
	int GetDrmPrefs(const std::string& uuid);
	/**
	 * @fn GetPreferredDrmUUID
	 */
	std::string GetPreferredDrmUUID();
	/**
	 * @fn IsEmptyPeriod
	 * @param period period to check whether it is empty
	 * @param isFogPeriod true if it is fog period
	 * @retval Return true on empty Period
	 */
	bool IsEmptyPeriod(IPeriod *period, bool isFogPeriod = false);
	/**
	 * @fn IsEmptyAdaptation
	 * @param Adaptation Adaptationto check whether it is empty
	 * @param isFogPeriod true if it is fog period
	 */
	bool IsEmptyAdaptation(IAdaptationSet *adaptationSet, bool isFogPeriod);
	/**
	 * @fn GetAvailableVSSPeriods
	 * @param PeriodIds vector of new Early Available Periods
	 */
	void GetAvailableVSSPeriods(std::vector<IPeriod*>& PeriodIds);
	/**
	 * @fn CheckForVssTags
	 */
	bool CheckForVssTags();
	/**
	 * @fn GetVssVirtualStreamID
	 */
	std::string GetVssVirtualStreamID();
	/**
	 * @fn IsMatchingLanguageAndMimeType
	 * @param[in] type - media type
	 * @param[in] lang - language to be matched
	 * @param[in] adaptationSet - adaptation to be checked for
	 * @param[out] representionIndex - represention within adaptation with matching params
	 */
	bool IsMatchingLanguageAndMimeType(MediaType type, std::string lang, IAdaptationSet *adaptationSet, int &representationIndex);
	/**
	 * @fn GetFragmentUrl
	 * @param[out] fragmentUrl fragment url
	 * @param fragmentDescriptor descriptor
	 * @param media media information string
	 */
	void GetFragmentUrl( std::string& fragmentUrl, const FragmentDescriptor *fragmentDescriptor, std::string media);
	double GetEncoderDisplayLatency();
	/**
	 * @fn StartLatencyMonitorThread
	 * @return void
	 */
	void StartLatencyMonitorThread();
	LatencyStatus GetLatencyStatus() { return latencyStatus; }
	/**
	 * @fn GetContentProtection
	 * @param[In] adaptation set and media type
	 */
	vector<IDescriptor*> GetContentProtection(const IAdaptationSet *adaptationSet, MediaType mediaType);
	/**
	 * @fn GetPreferredCodecIndex
	 * @param adaptationSet Adaptation set object
	 * @param[out] selectedRepIdx - Selected representation index
	 * @param[out] selectedCodecType type of desired representation
	 * @param[out] selectedRepBandwidth - selected audio track bandwidth
	 * @param disableEC3 whether EC3 deabled by config
	 * @param disableATMOS whether ATMOS audio deabled by config
 	 */
	bool GetPreferredCodecIndex(IAdaptationSet *adaptationSet, int &selectedRepIdx, AudioType &selectedCodecType, 
	uint32_t &selectedRepBandwidth, uint32_t &bestScore, bool disableEC3, bool disableATMOS, bool disableAC4, bool &disabled);
 	
	/**
         * @brief Get the audio track information from all period
         * updated member variable mAudioTracksAll
         * @return void
         */
	void PopulateAudioTracks(void);
	
	/**
         * @brief Get the audio track information from all preselection node of the period
         * @param period Node ans IMPDElement 
         * @return void
         */
	void ParseAvailablePreselections(IMPDElement *period, std::vector<AudioTrackInfo> & audioAC4Tracks);
	
	/**
	 * @fn PopulateTrackInfo
	 * @param media - Media type 
	 * @param - Do need to reset vector?
	 * @retun none
	 */
	void PopulateTrackInfo(MediaType media, bool reset=false);

	std::mutex mStreamLock;
	bool fragmentCollectorThreadStarted;
	std::set<std::string> mLangList;
	double seekPosition;
	float rate;
	std::thread *fragmentCollectorThreadID;
	pthread_t createDRMSessionThreadID;
	std::thread *deferredDRMRequestThread;
	bool deferredDRMRequestThreadStarted;
	bool mAbortDeferredLicenseLoop;
	bool drmSessionThreadStarted;
	dash::mpd::IMPD *mpd;
	class MediaStreamContext *mMediaStreamContext[AAMP_TRACK_COUNT];
	int mNumberOfTracks;
	int mCurrentPeriodIdx;
	double mEndPosition;
	bool mIsLiveStream;    	    	   /**< Stream is live or not; won't change during runtime. */
	bool mIsLiveManifest;   	   /**< Current manifest is dynamic or static; may change during runtime. eg: Hot DVR. */
	StreamInfo* mStreamInfo;
	bool mUpdateStreamInfo;		   /**< Indicates mStreamInfo needs to be updated */
	double mPrevStartTimeSeconds;
	std::string mPrevLastSegurlMedia;
	long mPrevLastSegurlOffset; 	   /**< duration offset from beginning of TSB */
	double mPeriodEndTime;
	double mPeriodStartTime;
	double mPeriodDuration;
	uint64_t mMinUpdateDurationMs;
	double mTSBDepth;
	double mPresentationOffsetDelay;
	uint64_t mLastPlaylistDownloadTimeMs;
	double mFirstPTS;
	double mStartTimeOfFirstPTS;
	double mVideoPosRemainder;
	double mFirstFragPTS[AAMP_TRACK_COUNT];
	AudioType mAudioType;
	int mPrevAdaptationSetCount;
	std::vector<long> mBitrateIndexVector;
	// In case of streams with multiple video Adaptation Sets, A profile
	// is a combination of an Adaptation Set and Representation within
	// that Adaptation Set. Hence we need a mapping from a profile to
	// corresponding Adaptation Set and Representation Index
	std::map<int, struct ProfileInfo> mProfileMaps;
	
	bool mIsFogTSB;
	IPeriod *mCurrentPeriod;
	std::string mBasePeriodId;
	double mBasePeriodOffset;
	class PrivateCDAIObjectMPD *mCdaiObject;
	std::shared_ptr<AampDrmHelper> mLastDrmHelper;
	std::vector<std::string> mEarlyAvailablePeriodIds;
	std::map<std::string, struct EarlyAvailablePeriodInfo> mEarlyAvailableKeyIDMap;
	std::queue<std::string> mPendingKeyIDs;
	int mCommonKeyDuration;

	// DASH does not use abr manager to store the supported bandwidth values,
	// hence storing max TSB bandwith in this variable which will be used for VideoEnd Metric data via
	// StreamAbstractionAAMP::GetMaxBitrate function,
	long mMaxTSBBandwidth;

	double mLiveEndPosition;
	double mCulledSeconds;
	bool mAdPlayingFromCDN;   /*Note: TRUE: Ad playing currently & from CDN. FALSE: Ad "maybe playing", but not from CDN.*/
	double mAvailabilityStartTime;
	std::map<std::string, int> mDrmPrefs;
	int mMaxTracks; /* Max number of tracks for this session */
	double mServerUtcTime;
	double mDeltaTime;
	double mHasServerUtcTime;
 	/**
	 * @fn GetPeriodStartTime
	 * @param mpd : pointer manifest
	 * @param periodIndex
	 */	
	double GetPeriodStartTime(IMPD *mpd, int periodIndex);
	/**
	 * @fn GetPeriodDuration
	 * @param mpd : pointer manifest
	 * @param periodIndex Index of the current period
 	 */
	double GetPeriodDuration(IMPD *mpd, int periodIndex);
	/**
	 * @fn GetPeriodEndTime
	 * @param mpd : pointer manifest
	 * @param periodIndex Index of the current period
	 * @param mpdRefreshTime : time when manifest was downloade
	 */
	double GetPeriodEndTime(IMPD *mpd, int periodIndex, uint64_t mpdRefreshTime);
	/**
	 * @fn isAdbreakStart
	 * @param[in] period instance.
	 * @param[in] startMS start time in milli seconds.
	 * @param[in] eventBreakVec vector of EventBreakInfo structure.
	 */
	bool isAdbreakStart(IPeriod *period, uint64_t &startMS, std::vector<EventBreakInfo> &eventBreakVec);
	/**
	 * @fn onAdEvent
	 */
	bool onAdEvent(AdEvent evt);
	bool onAdEvent(AdEvent evt, double &adOffset);
 	/**
	 * @fn SetAudioTrackInfo
	 * @param[in] tracks - available audio tracks in period
	 * @param[in] trackIndex - index of current audio track
	 */	
	void SetAudioTrackInfo(const std::vector<AudioTrackInfo> &tracks, const std::string &trackIndex);
	void SetTextTrackInfo(const std::vector<TextTrackInfo> &tracks, const std::string &trackIndex);
	/**
	 * @fn FindPeriodGapsAndReport
	 */
	void FindPeriodGapsAndReport();
	
#ifdef AAMP_MPD_DRM
	/**
	 * @fn ProcessEAPLicenseRequest
	 */
	void ProcessEAPLicenseRequest(void);
	/**
	 * @fn StartDeferredDRMRequestThread
	 * @param mediaType type of track
	 */
	void StartDeferredDRMRequestThread(MediaType mediaType);
	/**
	 * @fn ProcessVssContentProtection
	 * @param drmHelper created
	 * @param mediaType type of track
	 */
	void ProcessVssContentProtection(std::shared_ptr<AampDrmHelper> drmHelper, MediaType mediaType);
	/**
	 * @fn CreateDrmHelper
	 * @param adaptationSet Adaptation set object
	 * @param mediaType type of track
	 */
	std::shared_ptr<AampDrmHelper> CreateDrmHelper(IAdaptationSet * adaptationSet,MediaType mediaType);
#endif
	std::vector<StreamInfo*> thumbnailtrack;
	std::vector<TileInfo> indexedTileInfo;
	double mFirstPeriodStartTime; /*< First period start time for progress report*/

	LatencyStatus latencyStatus; 		 /**< Latency status of the playback*/
	LatencyStatus prevLatencyStatus;	 /**< Previous latency status of the playback*/
	bool latencyMonitorThreadStarted;	 /**< Monitor latency thread  status*/
	pthread_t latencyMonitorThreadID;	 /**< Fragment injector thread id*/
	int mProfileCount;			 /**< Total video profile count*/
};

#endif //FRAGMENTCOLLECTOR_MPD_H_
/**
 * @}
 */
