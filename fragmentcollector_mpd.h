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

/**
 * @brief Common MPD util functions (admanager_mpd.cpp and fragmentcollector_mpd.cpp 
 *
 */
double aamp_GetPeriodNewContentDuration(dash::mpd::IMPD *mpd, IPeriod * period, uint64_t &curEndNumber);
/**
 *   @brief  Get difference between first segment start time and presentation offset from period
 *   @param  period Segment period  
 *   @retval start time delta in seconds
 */
double aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset(IPeriod * period);
/**
 *   @brief  Get Period Duration
 *   @param  mpd manifest file
 *   @param  periodIndex Period Index
 *   @retval period duration in milli seconds
 */
double aamp_GetPeriodDuration(dash::mpd::IMPD *mpd, int periodIndex, uint64_t mpdDownloadTime = 0);
/**
 * @brief Get xml node form reader
 *
 * @param[in] reader Pointer to reader object
 * @param[in] url    manifest url
 *
 * @retval xml node
 */
Node* aamp_ProcessNode(xmlTextReaderPtr *reader, std::string url, bool isAd = false);
/**
 * @brief Get duration though representation iteration
 * @retval duration in milliseconds
 */
uint64_t aamp_GetDurationFromRepresentation(dash::mpd::IMPD *mpd);

/**
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
	 * @brief StreamAbstractionAAMP_MPD Constructor
    	 * @param aamp pointer to PrivateInstanceAAMP object associated with player
    	 * @param seekpos Seek position
     	 * @param rate playback rate
    	 */
	StreamAbstractionAAMP_MPD(AampLogManager *logObj, class PrivateInstanceAAMP *aamp,double seekpos, float rate);
	/**
         * @brief StreamAbstractionAAMP_MPD Destructor
         */
	~StreamAbstractionAAMP_MPD();
	/**
         * @brief Copy constructor disabled
         *
         */
	StreamAbstractionAAMP_MPD(const StreamAbstractionAAMP_MPD&) = delete;
	/**
         * @brief assignment operator disabled
         *
         */
	StreamAbstractionAAMP_MPD& operator=(const StreamAbstractionAAMP_MPD&) = delete;
	/**
         * @brief Stub implementation
         */
	void DumpProfiles(void) override;
	/**
         * @brief  Starts streaming.
         */
	void Start() override;
	/**
         *   @brief  Stops streaming.
         *
         *   @param  clearChannelData - ignored.
         */
	void Stop(bool clearChannelData) override;
	/**
         *   @brief  Initialize a newly created object.
         *   @note   To be implemented by sub classes
         *   @param  tuneType to set type of object.
         *   @retval true on success
         *   @retval false on failure
         */
	AAMPStatusType Init(TuneType tuneType) override;
	/**
         * @brief Get output format of stream.
         *
         * @param[out]  primaryOutputFormat - format of primary track
         * @param[out]  audioOutputFormat - format of audio track
         */
	void GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxOutputFormat) override;
	/**
         * @brief Get current stream position.
         *
         * @retval current position of stream.
         */
	double GetStreamPosition() override;
	/**
         *   @brief Return MediaTrack of requested type
         *
         *   @param[in]  type - track type
         *   @retval MediaTrack pointer.
         */
	MediaTrack* GetMediaTrack(TrackType type) override;
	/**
         *   @brief  Get PTS of first sample.
         *
         *   @retval PTS of first sample
         */
	double GetFirstPTS() override;
	/**
         *   @brief  Get Start time PTS of first sample.
         *
         *   @retval start time of first sample
         */
	double GetStartTimeOfFirstPTS() override;
	/**
         * @brief Get index of profile corresponds to bandwidth
         * @param[in] bitrate Bitrate to lookup profile
         * @retval profile index
         */
	int GetBWIndex(long bitrate) override;
	/**
         * @brief To get the available video bitrates.
         * @return available video bitrates
         */
	std::vector<long> GetVideoBitrates(void) override;
	/**
         * @brief To get the available audio bitrates.
         * @return available audio bitrates
         */
	std::vector<long> GetAudioBitrates(void) override;
	/**
         * @brief Gets Max Bitrate avialable for current playback.
         * @return long MAX video bitrates
         */
	long GetMaxBitrate(void) override;
	/**
         * @brief  Stops injecting fragments to StreamSink.
         */
	void StopInjection(void) override;
	/**
         * @brief  Start injecting fragments to StreamSink.
         */
	void StartInjection(void) override;
	double GetBufferedDuration();
	void SeekPosUpdate(double secondsRelativeToTuneTime) {seekPosition = secondsRelativeToTuneTime; }
	virtual void SetCDAIObject(CDAIObject *cdaiObj) override;
	/**
         * @brief To Get all tha available audio tracks
         *
         * @param[in] allTrack - True/False for all track
         * @return available audio tracks in period
         */
	virtual std::vector<AudioTrackInfo> & GetAvailableAudioTracks(bool allTrack=false) override;
	/**
         * @brief Gets number of profiles
         * @retval number of profiles
         */
	int GetProfileCount();
	/**
         * @brief Get profile index for TsbBandwidth
         * @param mTsbBandwidth - bandwidth to identify profile index from list
         * @retval profile index of the current bandwidth
         */	
	int GetProfileIndexForBandwidth(long mTsbBandwidth);
	/**
         * @brief To get the available thumbnail tracks.
         * @return available thumbnail tracks.
         */
	std::vector<StreamInfo*> GetAvailableThumbnailTracks(void) override;
	/**
          * @brief To get the available video tracks.
          * @return available video tracks.
          */
	std::vector<StreamInfo*> GetAvailableVideoTracks(void) override;
	/**
         * @fn SetThumbnailTrack
         * @brief Function to set thumbnail track for processing
         *
         * @param thumbnailIndex index value indicating the track to select
         * @return bool true on success.
         */
	bool SetThumbnailTrack(int) override;
	/**
         * @fn GetThumbnailRangeData
         * @brief Function to fetch the thumbnail data.
         *
         * @param tStart start duration of thumbnail data.
         * @param tEnd end duration of thumbnail data.
         * @param baseurl base url of thumbnail images.
         * @param raw_w absolute width of the thumbnail spritesheet.
         * @param raw_h absolute height of the thumbnail spritesheet.
         * @param width width of each thumbnail tile.
         * @param height height of each thumbnail tile.
         * @return Updated vector of available thumbnail data.
         */
	std::vector<ThumbnailData> GetThumbnailRangeData(double,double, std::string*, int*, int*, int*, int*) override;

	// ideally below would be private, but called from MediaStreamContext
	/**
         * @brief Get Adaptation Set at given index for the current period
         *
         * @param[in] idx - Adaptation Set Index
         * @retval Adaptation Set at given Index
         */
	const IAdaptationSet* GetAdaptationSetAtIndex(int idx);
	/**
         * @brief Get Adaptation Set and Representation Index for given profile
         *
         * @param[in] profileIndex - Profile Index
         * @retval Adaptation Set and Representation Index pair for given profile
         */
	struct ProfileInfo GetAdaptationSetAndRepresetationIndicesForProfile(int profileIndex);
	/**
	 * @brief Get the Minimun updaed duration
	 *
	 */
	int64_t GetMinUpdateDuration() { return mMinUpdateDurationMs;}
	/**
	 * @brief Fetch and cache a fragment
     	 *
     	 * @param pMediaStreamContext Track object pointer
     	 * @param media media descriptor string
     	 * @param fragmentDuration duration of fragment in seconds
     	 * @param isInitializationSegment true if fragment is init fragment
     	 * @param curlInstance curl instance to be used to fetch
     	 * @param discontinuity true if fragment is discontinuous
     	 * @param pto unscaled pto value from mpd
     	 * @param scale timeScale value from mpd
     	 * @retval true on fetch success
    	 */
	bool FetchFragment( class MediaStreamContext *pMediaStreamContext, std::string media, double fragmentDuration, bool isInitializationSegment, unsigned int curlInstance, bool discontinuity = false, double pto = 0 , uint32_t scale = 0);
	/**
         * @brief Fetch and push next fragment
         * @param pMediaStreamContext Track object
         * @param curlInstance instance of curl to be used to fetch
         * @retval true if push is done successfully
         */
	bool PushNextFragment( class MediaStreamContext *pMediaStreamContext, unsigned int curlInstance);
	/**
         * @brief Get Period Start Time.
         *
         * @retval Period Start Time.
         */
	double GetFirstPeriodStartTime(void);
	/**
         * @brief Monitor Live End Latency and Encoder Display Latency
         * @retval void
         */
	void MonitorLatency();
	/**
         * @brief Set subtitle start state
         *
         */
	void StartSubtitleParser() override;
	/**
         * @brief Set subtitle pause state
         *
         */
	void PauseSubtitleParser(bool pause) override;
	/**
         *   @brief  Get timescale from current period
         *   @retval timescale
         */
	uint32_t GetCurrPeriodTimeScale();
	dash::mpd::IMPD *GetMPD( void );
	IPeriod *GetPeriod( void );
private:
	/**
	 * @brief Fetches and caches audio fragment parallelly for video fragment.
     	 */
	void AdvanceTrack(int trackIdx, bool trickPlay, double delta, bool *waitForFreeFrag, bool *exitFetchLoop, bool *bCacheFullState);
	/**
         * @brief Fetches and caches fragments in a loop
         */
	void FetcherLoop();
	/**
         *   @brief Get stream information of a profile from subclass.
         *
         *   @param[in]  idx - profile index.
         *   @retval stream information corresponding to index.
         */
	StreamInfo* GetStreamInfo(int idx) override;
	/**
         * @brief Check if LLProfile is Available in MPD
         * @param mpd Manifest file pointer
         * @retval bool true if LL profile. Else false
         */
	bool CheckLLProfileAvailable(IMPD *mpd);
	/**
         * @brief Parse MPD LL elements
         * @param[in] mpd Pointer to FragmentCollector
         * @param[out] stAampLLDashServiceData Reference to LowLatency element parsed data
         * @retval bool true if successfully Parsed Low Latency elements. Else false
         */
	bool ParseMPDLLData(MPD* mpd, AampLLDashServiceData &stAampLLDashServiceData);
	/**
         * @brief Update MPD manifest
         * @param init retrievePlaylistFromCache true to try to get from cache
         * @retval true on success
         */
	AAMPStatusType UpdateMPD(bool init = false);
	/**
         * @brief Read UTCTiming element
         * @param root XML root node
         * @retval Return true if UTCTiming element is available in the manifest
         */
	bool FindServerUTCTime(Node* root);
	/**
         * @brief Find timed metadata from mainifest
         * @param mpd MPD top level element
         * @param root XML root node
         * @param init true if this is the first playlist download for a tune/seek/trickplay
         * @param reportBulkMeta true if bulkTimedMetadata feature is enabled
         */
	void FindTimedMetadata(MPD* mpd, Node* root, bool init = false, bool reportBulkMet = false);
	/**
         * @brief Process supplemental property of a period
         * @param node SupplementalProperty node
         * @param[out] AdID AD Id
         * @param startMS start time in MS
         * @param durationMS duration in MS
         * @param isInit true if its the first playlist download
         * @param reportBulkMeta true if bulk metadata is enabled
         */
	void ProcessPeriodSupplementalProperty(Node* node, std::string& AdID, uint64_t startMS, uint64_t durationMS, bool isInit, bool reportBulkMeta=false);
	/**
         * @brief Process Period AssetIdentifier
         * @param node AssetIdentifier node
         * @param startMS start time MS
         * @param durationMS duration MS
         * @param assetID Asset Id
         * @param providerID Provider Id
         * @param isInit true if its the first playlist download
         * @param reportBulkMeta true if bulk metadata is enabled
         */
	void ProcessPeriodAssetIdentifier(Node* node, uint64_t startMS, uint64_t durationMS, std::string& assetID, std::string& providerID,bool isInit, bool reportBulkMeta=false);
	/**
         *   @brief Process ad event stream.
         *
         *   @param[in] startMS Break start time in milli seconds
         *   @param[in] period Period instance.
         */
 	bool ProcessEventStream(uint64_t startMS, IPeriod * period, bool reportBulkMeta);
	/**
         * @brief Process Stream restriction list
         * @param node StreamRestrictionListType node
         * @param AdID Ad Id
         * @param startMS start time MS
         * @param isInit true if its the first playlist download
         * @param reportBulkMeta true if bulk metadata is enabled
         */
	void ProcessStreamRestrictionList(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta);
	/**
         * @brief Process stream restriction
         * @param node StreamRestriction xml node
         * @param AdID Ad ID
         * @param startMS Start time in MS
         * @param isInit true if its the first playlist download
         * @param reportBulkMeta true if bulk metadata is enabled
         */
	void ProcessStreamRestriction(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta);
	/**
         * @brief Process stream restriction extension
         * @param node Ext child of StreamRestriction xml node
         * @param AdID Ad ID
         * @param startMS start time in ms
         * @param isInit true if its the first playlist download
         * @param reportBulkMeta true if bulk metadata is enabled
         */
	void ProcessStreamRestrictionExt(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta);
	/**
         * @brief Process trick mode restriction
         * @param node TrickModeRestriction xml node
         * @param AdID Ad ID
         * @param startMS start time in ms
         * @param isInit true if its the first playlist download
         * @param reportBulkMeta true if bulk metadata is enabled
         */
	void ProcessTrickModeRestriction(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta);
	/**
         * @brief Fetch and inject initialization fragment for all available tracks
         * @param discontinuity number of tracks and discontinuity true if discontinuous fragment
         */
	void FetchAndInjectInitFragments(bool discontinuity = false);
	/**
         * @brief Fetch and inject initialization fragment for media type
	 * @param trackIdx number of tracks and discontinuity true if discontinuous fragment
         */
	void FetchAndInjectInitialization(int trackIdx, bool discontinuity = false);
	/**
         * @brief Does stream selection
         * @param newTune true if this is a new tune
         */
	void StreamSelection(bool newTune = false, bool forceSpeedsChangedEvent = false);
	/**
         * @brief Check if current period is clear
         * @retval true if clear period
         */
	bool CheckForInitalClearPeriod();
	/**
         * @brief Push encrypted headers if available
         */
	void PushEncryptedHeaders();
	/**
         * @brief Get profile index for bandwidth notification
         * @param bandwidth - bandwidth to identify profile index from list
         * @retval profile index of the current bandwidth
         */
	int GetProfileIdxForBandwidthNotification(uint32_t bandwidth);
	/**
         * @brief Updates track information based on current state
         */
	AAMPStatusType UpdateTrackInfo(bool modifyDefaultBW, bool periodChanged, bool resetTimeLineIndex=false);
	/**
         * @brief Skip fragments by given time
         * @param pMediaStreamContext Media track object
         * @param skipTime time to skip in seconds
         * @param updateFirstPTS true to update first pts state variable
         * @retval skip fragment duration
         */
	double SkipFragments( class MediaStreamContext *pMediaStreamContext, double skipTime, bool updateFirstPTS = false, bool skipToEnd = false);	
	/**
	 * @brief Skip to end of track
     	 * @param pMediaStreamContext Track object pointer
     	 */
	void SkipToEnd( class MediaStreamContext *pMediaStreamContext); //Added to support rewind in multiperiod assets
	/**
         * @brief Process content protection of adaptation
         * @param adaptationSet Adaptation set object
         * @param mediaType type of track
         */
	void ProcessContentProtection(IAdaptationSet * adaptationSet,MediaType mediaType, std::shared_ptr<AampDrmHelper> drmHelper = nullptr);
	/**
	 * @brief Seek current period by a given time
     	 * @param seekPositionSeconds seek positon in seconds
     	 */
	void SeekInPeriod( double seekPositionSeconds, bool skipToEnd = false);	
	/**
         * @brief Find the fragment based on the system clock after the SAP.
         * @param seekPositionSeconds seek positon in seconds.
         **/
	void ApplyLiveOffsetWorkaroundForSAP(double seekPositionSeconds);
 	/**
         * @brief Update culling state for live manifests
         */
	double GetCulledSeconds();
	/**
         * @brief Update culled and duration value from periodinfo      
         */
	void UpdateCulledAndDurationFromPeriodInfo();
	/**
         * @brief Update language list state variables
         */
	void UpdateLanguageList();
	/**
         * @brief Get the best Audio track by Language, role, and/or codec type
         *
         * @param desiredRepIdx [out] selected representation Index
         * @param selectedCodecType [out] selected codec type
         * @param ac4Tracks parsed track from preselection node
         * @param audioTrackIndex selected audio track index
         * @return int selected representation index
         */
	int GetBestAudioTrackByLanguage(int &desiredRepIdx,AudioType &selectedCodecType, std::vector<AudioTrackInfo> &ac4Tracks, std::string &audioTrackIndex);
	/**
          * @brief Get the preferred audio by language
	  */
	int GetPreferredAudioTrackByLanguage();
	/**
         * @brief Check if ProducerReferenceTime UTCTime type Matches with Other UTCtime type declaration
         * @param pRT Pointer to ProducerReferenceTime
         * @retval bool true if Match exist. Else false
         */
	bool CheckProducerReferenceTimeUTCTimeMatch(IProducerReferenceTime *pRT);
	/**
         * @brief Print ProducerReferenceTime parsed data
         * @param pRT Pointer to ProducerReferenceTime
         * @retval void
         */
	void PrintProducerReferenceTimeAtrributes(IProducerReferenceTime *pRT);
	/**
         * @brief Check if ProducerReferenceTime available in AdaptationSet
         * @param adaptationSet Pointer to AdaptationSet
         * @retval IProducerReferenceTime* Porinter to parsed ProducerReferenceTime data
         */
	IProducerReferenceTime *GetProducerReferenceTimeForAdaptationSet(IAdaptationSet *adaptationSet);
	/**
         * @brief Get the language for an adaptation set
         * @param adaptationSet Pointer to adaptation set
         * @retval language of adaptation set
         */
	std::string GetLanguageForAdaptationSet( IAdaptationSet *adaptationSet );
	/**
 	 * @brief Get mpd object of manifest
     	 * @param manifest manifest buffer pointer
     	 * @param mpd MPD object of manifest
     	 * @param manifestUrl manifest url
         * @param init true if this is the first playlist download for a tune/seek/trickplay
         * @retval AAMPStatusType indicates if success or fail
         */
	AAMPStatusType GetMpdFromManfiest(const GrowableBuffer &manifest, MPD * &mpd, std::string manifestUrl, bool init = false);
	/**
         * @brief Get the DRM preference value.
         * @param uuid The UUID for the DRM type.
         * @return The preference level for the DRM type.
         */
	int GetDrmPrefs(const std::string& uuid);
	/**
	 * @brief Get the UUID of preferred DRM.
         * @return The UUID of preferred DRM
         */
	std::string GetPreferredDrmUUID();
	/**
         * @brief Check if Period is empty or not
         * @param period Period to find empty or not
         * @param isFogPeriod True/False isFogPeriod
         * @retval Return true on empty Period
         */
	bool IsEmptyPeriod(IPeriod *period, bool isFogPeriod = false);
	/**
         * @brief Check new early available periods
         * @param PeriodIds vector of new Early Available Perids
         */
	void GetAvailableVSSPeriods(std::vector<IPeriod*>& PeriodIds);
	/**
         * @brief Check for VSS tags
         * @retval bool true if found, false otherwise
         */
	bool CheckForVssTags();
	/**
         * @brief GetVssVirtualStreamID from manifest
         * @retval return Virtual stream ID string
         */
	std::string GetVssVirtualStreamID();
	/**
         * @brief To check if the adaptation set is having matching language and supported mime type
         *
         * @param[in] type - media type
         * @param[in] lang - language to be matched
         * @param[in] adaptationSet - adaptation to be checked for
         * @param[out] representationIndex - represention within adaptation with matching params
         * @return bool true if the params are matching
         */
	bool IsMatchingLanguageAndMimeType(MediaType type, std::string lang, IAdaptationSet *adaptationSet, int &representationIndex);
	/**
         * @brief Generates fragment url from media information
         * @param[out] fragmentUrl fragment url
     	 * @param fragmentDescriptor descriptor
     	 * @param media media information string
     	 */
	void GetFragmentUrl( std::string& fragmentUrl, const FragmentDescriptor *fragmentDescriptor, std::string media);
	/**
         * @brief Get the Latency of the Display
         */
	double GetEncoderDisplayLatency();
	/**
         * @brief Starts Latency monitor loop
         */
	void StartLatencyMonitorThread();
	/**
         * @brief Get the latency status
         */
	LatencyStatus GetLatencyStatus() { return latencyStatus; }
	/**
         * @brief Get content protection from represetation/adaptation field
         * @param[in] adaptationSet adaptation set 
         * @param[in] mediaType media type
         * @retval content protections if present. Else NULL.
         */
	vector<IDescriptor*> GetContentProtection(const IAdaptationSet *adaptationSet, MediaType mediaType);
	/**
         * @brief Get representation index from preferred codec list
    	 * @param adaptationSet Adaptation set object
    	 * @param [out] selectedRepIdx - Selected representation index
    	 * @param[out] selectedCodecType type of desired representation
    	 * @param [out] selectedRepBandwidth - selected audio track bandwidth
    	 * @param [in] disableEC3 whether EC3 deabled by config
    	 * @param [in] disableATMOS whether ATMOS audio deabled by config 
    	 * @retval whether track selected or not
    	 */
	bool GetPreferredCodecIndex(IAdaptationSet *adaptationSet, int &selectedRepIdx, AudioType &selectedCodecType, 
	uint32_t &selectedRepBandwidth, uint32_t &bestScore, bool disableEC3, bool disableATMOS, bool disableAC4);
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
	bool mIsLiveStream;        /**< Stream is live or not; won't change during runtime */
	bool mIsLiveManifest;      /**< Current manifest is dynamic or static; may change during runtime. eg: Hot DVR */
	StreamInfo* mStreamInfo;
	bool mUpdateStreamInfo;    /**< Indicates mStreamInfo needs to be updated */
	double mPrevStartTimeSeconds;
	std::string mPrevLastSegurlMedia;
	long mPrevLastSegurlOffset; /**<duration offset from beginning of TSB */
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
	 * @brief Get start time of current period
	 * @param mpd : pointer manifest
	 * @param periodIndex
	 * @retval current period's start time
	 */
	double GetPeriodStartTime(IMPD *mpd, int periodIndex);
	/**
	 * @brief Get duration of current period
	 * @param mpd pointer manifest
	 * @param periodIndex Index of the period
	 * @retval current period's duration
	 */
	double GetPeriodDuration(IMPD *mpd, int periodIndex);
	/**
	 * @brief Get end time of current period
	 * @param mpd : pointer manifest
	 * @param periodIndex
	 * @param mpdRefreshTime : time when manifest was downloaded
	 * @retval current period's end time
	 */
	double GetPeriodEndTime(IMPD *mpd, int periodIndex, uint64_t mpdRefreshTime);

	/**
	 *   @brief Check whether the period has any valid ad.
	 *
	 *   @param[in] period instance.
	 *   @param[in] startMS Break start time in milli seconds.
	 *   @param[in] eventBreakVec vector of EventBreakInfo structure.
	 *   @retval true/false is ad is start
	 */
	bool isAdbreakStart(IPeriod *period, uint64_t &startMS, std::vector<EventBreakInfo> &eventBreakVec);
	bool onAdEvent(AdEvent evt);
	/**
	 * @brief State change to Ad event
	 * @param evt Adevnt
	 * @param adOffset Ad plcement duration 
	 *
	 */
	bool onAdEvent(AdEvent evt, double &adOffset);
	
	/**
	 * @brief To set the audio tracks of current period
	 *
	 * @param[in] tracks - available audio tracks in period
	 * @param[in] trackIndex - index of current audio track
	 * @return void
	 */
	void SetAudioTrackInfo(const std::vector<AudioTrackInfo> &tracks, const std::string &trackIndex);
	/**
	 * @brief To set the text tracks of current period
	 *
	 * @param[in] tracks - available text tracks in period
	 * @param[in] trackIndex - index of current text track
	 * @return void
	 */
	void SetTextTrackInfo(const std::vector<TextTrackInfo> &tracks, const std::string &trackIndex);
	/**
	 * @brief Check if Period is empty or not
	 * @param Period
	 * @retval Return true on empty Period
	 */
	void FindPeriodGapsAndReport();
	
#ifdef AAMP_MPD_DRM
	/**
	 * @brief Process Early Available License Request
	 * @param drmHelper early created drmHelper
	 * @param mediaType type of track
	 * @param string periodId of EAP
	 */
	void ProcessEAPLicenseRequest(void);
	/**
	 * @brief Start Deferred License Request
	 * @param drmHelper early created drmHelper
	 * @param mediaType type of track
	 */
	void StartDeferredDRMRequestThread(MediaType mediaType);
	/**
    	 * @brief Process content protection of vss EAP
     	 * @param drmHelper created
     	 * @param mediaType type of track
     	 */
	void ProcessVssContentProtection(std::shared_ptr<AampDrmHelper> drmHelper, MediaType mediaType);
    	/**
	 * @brief Create DRM helper from ContentProtection
	 * @param adaptationSet Adaptation set object
	 * @param mediaType type of track
	 * @retval shared_ptr of AampDrmHelper
	 */
	std::shared_ptr<AampDrmHelper> CreateDrmHelper(IAdaptationSet * adaptationSet,MediaType mediaType);
#endif
	std::vector<StreamInfo*> thumbnailtrack;
	std::vector<TileInfo> indexedTileInfo;
	double mFirstPeriodStartTime;     /**< First period start time for progress report*/

	LatencyStatus latencyStatus;      /**< Latency status of the playback*/
	LatencyStatus prevLatencyStatus;  /**< Previous latency status of the playback*/
	bool latencyMonitorThreadStarted; /**< Monitor latency thread  status*/
	pthread_t latencyMonitorThreadID; /**< Fragment injector thread id*/
	int mProfileCount;                /**< Total video profile count*/
};

#endif //FRAGMENTCOLLECTOR_MPD_H_
/**
 * @}
 */
