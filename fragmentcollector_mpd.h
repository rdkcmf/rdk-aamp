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
uint64_t aamp_GetPeriodNewContentDuration(IPeriod * period, uint64_t &curEndNumber);
double aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset(IPeriod * period);
double aamp_GetPeriodDuration(dash::mpd::IMPD *mpd, int periodIndex, uint64_t mpdDownloadTime = 0);
Node* aamp_ProcessNode(xmlTextReaderPtr *reader, std::string url, bool isAd = false);
uint64_t aamp_GetDurationFromRepresentation(dash::mpd::IMPD *mpd);

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
	const std::vector<IBaseUrl *>*baseUrls;
	std::string matchingBaseURL;
public :
	std::string manifestUrl;
	uint32_t Bandwidth;
	std::string RepresentationID;
	uint64_t Number;
	double Time;
	bool bUseMatchingBaseUrl;

	FragmentDescriptor() : manifestUrl(""), baseUrls (NULL), Bandwidth(0), Number(0), Time(0), RepresentationID(""),matchingBaseURL(""),bUseMatchingBaseUrl(false)
	{
	}
	
	FragmentDescriptor(const FragmentDescriptor& p) : manifestUrl(p.manifestUrl), baseUrls(p.baseUrls), Bandwidth(p.Bandwidth), RepresentationID(p.RepresentationID), Number(p.Number), Time(p.Time),matchingBaseURL(p.matchingBaseURL),bUseMatchingBaseUrl(p.bUseMatchingBaseUrl)
	{
	}

	FragmentDescriptor& operator=(const FragmentDescriptor &p)
	{
		manifestUrl = p.manifestUrl;
		baseUrls = p.baseUrls;
		RepresentationID.assign(p.RepresentationID);
		Bandwidth = p.Bandwidth;
		Number = p.Number;
		Time = p.Time;
		matchingBaseURL = p.matchingBaseURL;
		return *this;
	}

	const std::vector<IBaseUrl *>*  GetBaseURLs() const
	{
		return baseUrls;
	}

	std::string GetMatchingBaseUrl() const
	{
		return matchingBaseURL;
	}
	void SetBaseURLs(const std::vector<IBaseUrl *>* baseurls )
	{
		if(baseurls)
		{
			this->baseUrls = baseurls;
			if(this->baseUrls->size() > 0 )
			{
				// use baseurl which matches with host from manifest.
				if(bUseMatchingBaseUrl)
				{
					std::string prefHost = aamp_getHostFromURL(manifestUrl);
					for (auto & item : *this->baseUrls) {
						std::string itemUrl =item->GetUrl();
						std::string host  = aamp_getHostFromURL(itemUrl);
						if(0 == prefHost.compare(host))
						{
							this->matchingBaseURL = item->GetUrl();
							return; // return here, we are done
						}
					}
				}
				//we are here means useMatchingBaseUrl not enabled or host did not match
				// hence initialize default to first baseurl
				this->matchingBaseURL = this->baseUrls->at(0)->GetUrl();
			}
			else
			{
				this->matchingBaseURL.clear();
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
	StreamAbstractionAAMP_MPD(class PrivateInstanceAAMP *aamp,double seekpos, float rate);
	~StreamAbstractionAAMP_MPD();
	StreamAbstractionAAMP_MPD(const StreamAbstractionAAMP_MPD&) = delete;
	StreamAbstractionAAMP_MPD& operator=(const StreamAbstractionAAMP_MPD&) = delete;
	void DumpProfiles(void) override;
	void Start() override;
	void Stop(bool clearChannelData) override;
	AAMPStatusType Init(TuneType tuneType) override;
	void GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxOutputFormat) override;
	double GetStreamPosition() override;
	MediaTrack* GetMediaTrack(TrackType type) override;
	double GetFirstPTS() override;
	double GetStartTimeOfFirstPTS() override;
	int GetBWIndex(long bitrate) override;
	std::vector<long> GetVideoBitrates(void) override;
	std::vector<long> GetAudioBitrates(void) override;
	long GetMaxBitrate(void) override;
	void StopInjection(void) override;
	void StartInjection(void) override;
	double GetBufferedDuration();
	void SeekPosUpdate(double secondsRelativeToTuneTime) { };
	virtual void SetCDAIObject(CDAIObject *cdaiObj) override;
	int GetProfileCount();
	int GetProfileIndexForBandwidth(long mTsbBandwidth);

	std::vector<StreamInfo*> GetAvailableThumbnailTracks(void) override;
	bool SetThumbnailTrack(int) override;
	std::vector<ThumbnailData> GetThumbnailRangeData(double,double, std::string*, int*, int*, int*, int*) override;

	// ideally below would be private, but called from MediaStreamContext
	const IAdaptationSet* GetAdaptationSetAtIndex(int idx);
	struct ProfileInfo GetAdaptationSetAndRepresetationIndicesForProfile(int profileIndex);
	int64_t GetMinUpdateDuration() { return mMinUpdateDurationMs;}
	bool FetchFragment( class MediaStreamContext *pMediaStreamContext, std::string media, double fragmentDuration, bool isInitializationSegment, unsigned int curlInstance, bool discontinuity = false, double pto = 0 , uint32_t scale = 0 );
	bool PushNextFragment( class MediaStreamContext *pMediaStreamContext, unsigned int curlInstance);
	double GetFirstPeriodStartTime(void);
	void StartSubtitleParser() override;
	void PauseSubtitleParser(bool pause) override;
	bool mperiodChanged[AAMP_TRACK_COUNT];
	bool mpendingPtoProcessing[AAMP_TRACK_COUNT];
	double mtempDelta[AAMP_TRACK_COUNT];
	double mFirstBufferScaledPts[AAMP_TRACK_COUNT];

private:
	void AdvanceTrack(int trackIdx, bool trickPlay, double delta, bool *waitForFreeFrag, bool *exitFetchLoop, bool *bCacheFullState);
	void FetcherLoop();
	StreamInfo* GetStreamInfo(int idx) override;
	AAMPStatusType UpdateMPD(bool init = false);
	bool FindServerUTCTime(Node* root);
	void FindTimedMetadata(MPD* mpd, Node* root, bool init = false, bool reportBulkMet = false);
	void ProcessPeriodSupplementalProperty(Node* node, std::string& AdID, uint64_t startMS, uint64_t durationMS, bool isInit, bool reportBulkMeta=false);
	void ProcessPeriodAssetIdentifier(Node* node, uint64_t startMS, uint64_t durationMS, std::string& assetID, std::string& providerID,bool isInit, bool reportBulkMeta=false);
	bool ProcessEventStream(uint64_t startMS, IPeriod * period, bool reportBulkMeta);
	void ProcessStreamRestrictionList(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta);
	void ProcessStreamRestriction(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta);
	void ProcessStreamRestrictionExt(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta);
	void ProcessTrickModeRestriction(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta);
	void FetchAndInjectInitialization(bool discontinuity = false);
	void StreamSelection(bool newTune = false, bool forceSpeedsChangedEvent = false);
	bool CheckForInitalClearPeriod();
	void PushEncryptedHeaders();
	int GetProfileIdxForBandwidthNotification(uint32_t bandwidth);
	AAMPStatusType UpdateTrackInfo(bool modifyDefaultBW, bool periodChanged, bool resetTimeLineIndex=false);
	double SkipFragments( class MediaStreamContext *pMediaStreamContext, double skipTime, bool updateFirstPTS = false);
	void SkipToEnd( class MediaStreamContext *pMediaStreamContext); //Added to support rewind in multiperiod assets
	void ProcessContentProtection(IAdaptationSet * adaptationSet,MediaType mediaType, std::shared_ptr<AampDrmHelper> drmHelper = nullptr);
	void SeekInPeriod( double seekPositionSeconds);
	double GetCulledSeconds();
	void UpdateCulledAndDurationFromPeriodInfo();
	void UpdateLanguageList();
	int GetBestAudioTrackByLanguage(int &desiredRepIdx,AudioType &selectedCodecType);
	int GetPreferredAudioTrackByLanguage();
	std::string GetLanguageForAdaptationSet( IAdaptationSet *adaptationSet );
	AAMPStatusType GetMpdFromManfiest(const GrowableBuffer &manifest, MPD * &mpd, std::string manifestUrl, bool init = false);
	int GetDrmPrefs(const std::string& uuid);
	std::string GetPreferredDrmUUID();
	bool IsEmptyPeriod(IPeriod *period, bool isFogPeriod = false);
	void GetAvailableVSSPeriods(std::vector<IPeriod*>& PeriodIds);
	bool CheckForVssTags();
	std::string GetVssVirtualStreamID();
	bool IsMatchingLanguageAndMimeType(MediaType type, std::string lang, IAdaptationSet *adaptationSet, int &representationIndex);
	void GetFragmentUrl( std::string& fragmentUrl, const FragmentDescriptor *fragmentDescriptor, std::string media);
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
	bool mIsLiveStream;        //Stream is live or not; won't change during runtime.
	bool mIsLiveManifest;      //Current manifest is dynamic or static; may change during runtime. eg: Hot DVR.
	StreamInfo* mStreamInfo;
	bool mUpdateStreamInfo; //Indicates mStreamInfo needs to be updated
	double mPrevStartTimeSeconds;
	std::string mPrevLastSegurlMedia;
	long mPrevLastSegurlOffset; //duration offset from beginning of TSB
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
	
	double GetPeriodStartTime(IMPD *mpd, int periodIndex);
	double GetPeriodDuration(IMPD *mpd, int periodIndex);
	double GetPeriodEndTime(IMPD *mpd, int periodIndex, uint64_t mpdRefreshTime);

	bool isAdbreakStart(IPeriod *period, uint64_t &startMS, std::vector<EventBreakInfo> &eventBreakVec);
	bool onAdEvent(AdEvent evt);
	bool onAdEvent(AdEvent evt, double &adOffset);
	
	void SetAudioTrackInfo(const std::vector<AudioTrackInfo> &tracks, const std::string &trackIndex);
	void SetTextTrackInfo(const std::vector<TextTrackInfo> &tracks, const std::string &trackIndex);
	void FindPeriodGapsAndReport();
	
#ifdef AAMP_MPD_DRM
	void ProcessEAPLicenseRequest(void);
	void StartDeferredDRMRequestThread(MediaType mediaType);
	void ProcessVssContentProtection(std::shared_ptr<AampDrmHelper> drmHelper, MediaType mediaType);
	std::shared_ptr<AampDrmHelper> CreateDrmHelper(IAdaptationSet * adaptationSet,MediaType mediaType);
#endif
	std::vector<StreamInfo*> thumbnailtrack;
	std::vector<TileInfo> indexedTileInfo;
	double mFirstPeriodStartTime; /*< First period start time for progress report*/
};

#endif //FRAGMENTCOLLECTOR_MPD_H_
/**
 * @}
 */
