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
 * @file fragmentcollector_mpd.cpp
 * @brief Fragment collector implementation of MPEG DASH
 */
#include "iso639map.h"
#include "fragmentcollector_mpd.h"
#include "priv_aamp.h"
#include "AampDRMSessionManager.h"
#include "AampConstants.h"
#include "SubtecFactory.hpp"
#include <stdlib.h>
#include <string.h>
#include "_base64.h"
#include <pthread.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <set>
#include <iomanip>
#include <ctime>
#include <inttypes.h>
#include <libxml/xmlreader.h>
#include <math.h>
#include <cmath> // For double abs(double)
#include <algorithm>
#include <cctype>
#include <regex>
#include "AampCacheHandler.h"
#include "AampUtils.h"
#include "AampRfc.h"
//#define DEBUG_TIMELINE

#ifdef AAMP_CC_ENABLED
#include "AampCCManager.h"
#endif



/**
 * @addtogroup AAMP_COMMON_TYPES
 * @{
 */
#define SEGMENT_COUNT_FOR_ABR_CHECK 5
#define DEFAULT_INTERVAL_BETWEEN_MPD_UPDATES_MS 3000
#define TIMELINE_START_RESET_DIFF 4000000000
#define MAX_DELAY_BETWEEN_MPD_UPDATE_MS (6000)
#define MIN_DELAY_BETWEEN_MPD_UPDATE_MS (500) // 500mSec
#define MIN_TSB_BUFFER_DEPTH 6 //6 seconds from 4.3.3.2.2 in https://dashif.org/docs/DASH-IF-IOP-v4.2-clean.htm
#define VSS_DASH_EARLY_AVAILABLE_PERIOD_PREFIX "vss-"
#define INVALID_VOD_DURATION  (0)

/**
 * Macros for extended audio codec check as per ETSI-TS-103-420-V1.2.1
 */
#define SUPPLEMENTAL_PROPERTY_TAG "SupplementalProperty"
#define SCHEME_ID_URI_EC3_EXT_CODEC "tag:dolby.com,2018:dash:EC3_ExtensionType:2018"
#define EC3_EXT_VALUE_AUDIO_ATMOS "JOC"

#define MEDIATYPE_VIDEO "video"
#define MEDIATYPE_AUDIO "audio"
#define MEDIATYPE_TEXT "text"
#define MEDIATYPE_AUX_AUDIO "aux-audio"
#define MEDIATYPE_IMAGE "image"

static double ParseISO8601Duration(const char *ptr);

static double ComputeFragmentDuration( uint32_t duration, uint32_t timeScale )
{
	double newduration = 2.0;
	if( duration && timeScale )
	{
		newduration =  (double)duration / (double)timeScale;
		newduration = ceil(newduration * 1000.0) / 1000.0;
		return newduration;
	}
	AAMPLOG_WARN( "%s:%d bad fragment duration", __FUNCTION__, __LINE__ );
	return newduration;
}
	
class SegmentTemplates
{ //  SegmentTemplate can be split info common (Adaptation Set) and representation-specific parts
private:
	const ISegmentTemplate *segmentTemplate1; // primary (representation)
	const ISegmentTemplate *segmentTemplate2; // secondary (adaptation set)
	
public:
	SegmentTemplates(const SegmentTemplates &other) = delete;
	SegmentTemplates& operator=(const SegmentTemplates& other) = delete;

	SegmentTemplates( const ISegmentTemplate *representation, const ISegmentTemplate *adaptationSet ) : segmentTemplate1(0),segmentTemplate2(0)
	{
		segmentTemplate1 = representation;
		segmentTemplate2 = adaptationSet;
	}
	~SegmentTemplates()
	{
	}

	bool HasSegmentTemplate()
	{
		return segmentTemplate1 || segmentTemplate2;
	}
	
	std::string Getmedia()
	{
		std::string media;
		if( segmentTemplate1 ) media = segmentTemplate1->Getmedia();
		if( media.empty() && segmentTemplate2 ) media = segmentTemplate2->Getmedia();
		return media;
	}
	
	const ISegmentTimeline *GetSegmentTimeline()
	{
		const ISegmentTimeline *segmentTimeline = NULL;
		if( segmentTemplate1 ) segmentTimeline = segmentTemplate1->GetSegmentTimeline();
		if( !segmentTimeline && segmentTemplate2 ) segmentTimeline = segmentTemplate2->GetSegmentTimeline();
		return segmentTimeline;
	}
	
	uint32_t GetTimescale()
	{
		uint32_t timeScale = 0;
		if( segmentTemplate1 ) timeScale = segmentTemplate1->GetTimescale();
		// if timescale missing in template ,GetTimeScale returns 1
		if((timeScale==1 || timeScale==0) && segmentTemplate2 ) timeScale = segmentTemplate2->GetTimescale();
		return timeScale;
	}

	uint32_t GetDuration()
	{
		uint32_t duration = 0;
		if( segmentTemplate1 ) duration = segmentTemplate1->GetDuration();
		if( duration==0 && segmentTemplate2 ) duration = segmentTemplate2->GetDuration();
		return duration;
	}
	
	long GetStartNumber()
	{
		long startNumber = 0;
		if( segmentTemplate1 ) startNumber = segmentTemplate1->GetStartNumber();
		if( startNumber==0 && segmentTemplate2 ) startNumber = segmentTemplate2->GetStartNumber();
		return startNumber;
	}

	uint64_t GetPresentationTimeOffset()
	{
		uint64_t presentationOffset = 0;
		if(segmentTemplate1 ) presentationOffset = segmentTemplate1->GetPresentationTimeOffset();
		if( presentationOffset==0 && segmentTemplate2) presentationOffset = segmentTemplate2->GetPresentationTimeOffset();
		return presentationOffset;
	}
	
	std::string Getinitialization()
	{
		std::string initialization;
		if( segmentTemplate1 ) initialization = segmentTemplate1->Getinitialization();
		if( initialization.empty() && segmentTemplate2 ) initialization = segmentTemplate2->Getinitialization();
		return initialization;
	}
}; // SegmentTemplates

/**
 * @brief Check if the given period is empty
 */
static bool IsEmptyPeriod(IPeriod *period);


/**
 * @class MediaStreamContext
 * @brief MPD media track
 */
class MediaStreamContext : public MediaTrack
{
public:
	/**
	 * @brief MediaStreamContext Constructor
	 * @param type Type of track
	 * @param context  MPD collector context
	 * @param aamp Pointer to associated aamp instance
	 * @param name Name of the track
	 */
	MediaStreamContext(TrackType type, StreamAbstractionAAMP_MPD* ctx, PrivateInstanceAAMP* aamp, const char* name) :
			MediaTrack(type, aamp, name),
			mediaType((MediaType)type), adaptationSet(NULL), representation(NULL),
			fragmentIndex(0), timeLineIndex(0), fragmentRepeatCount(0), fragmentOffset(0),
			eos(false), fragmentTime(0), periodStartOffset(0), timeStampOffset(0), index_ptr(NULL), index_len(0),
			lastSegmentTime(0), lastSegmentNumber(0), lastSegmentDuration(0), adaptationSetIdx(0), representationIndex(0), profileChanged(true),
			adaptationSetId(0), fragmentDescriptor(), context(ctx), initialization(""),
			mDownloadedFragment(), discontinuity(false), mSkipSegmentOnError(true),
			downloadedDuration(0)
	{
		memset(&mDownloadedFragment, 0, sizeof(GrowableBuffer));
		fragmentDescriptor.bUseMatchingBaseUrl = ISCONFIGSET(eAAMPConfig_MatchBaseUrl);
	}

	/**
	 * @brief MediaStreamContext Destructor
	 */
	~MediaStreamContext()
	{
		aamp_Free(&mDownloadedFragment);
	}

	/**
	 * @brief MediaStreamContext Copy Constructor
	 */
	 MediaStreamContext(const MediaStreamContext&) = delete;

	/**
	 * @brief MediaStreamContext Assignment operator overloading
	 */
	 MediaStreamContext& operator=(const MediaStreamContext&) = delete;


	/**
	 * @brief Get the context of media track. To be implemented by subclasses
	 * @retval Context of track.
	 */
	StreamAbstractionAAMP* GetContext()
	{
		return context;
	}

	/**
	 * @brief Receives cached fragment and injects to sink.
	 *
	 * @param[in] cachedFragment - contains fragment to be processed and injected
	 * @param[out] fragmentDiscarded - true if fragment is discarded.
	 */
	void InjectFragmentInternal(CachedFragment* cachedFragment, bool &fragmentDiscarded)
	{
		aamp->ProcessID3Metadata(cachedFragment->fragment.ptr, cachedFragment->fragment.len, (MediaType) type, timeStampOffset);
		aamp->SendStreamTransfer((MediaType)type, &cachedFragment->fragment,
					cachedFragment->position, cachedFragment->position, cachedFragment->duration);
		fragmentDiscarded = false;
	} // InjectFragmentInternal

	/**
	 * @brief Fetch and cache a fragment
	 * @param fragmentUrl url of fragment
	 * @param curlInstance curl instance to be used to fetch
	 * @param position position of fragment in seconds
	 * @param duration duration of fragment in seconds
	 * @param range byte range
	 * @param initSegment true if fragment is init fragment
	 * @param discontinuity true if fragment is discontinuous
	 * @retval true on success
	 */
	bool CacheFragment(std::string fragmentUrl, unsigned int curlInstance, double position, double duration, const char *range = NULL, bool initSegment = false, bool discontinuity = false
		, bool playingAd = false
	)
	{
		bool ret = false;

		fragmentDurationSeconds = duration;
		ProfilerBucketType bucketType = aamp->GetProfilerBucketForMedia(mediaType, initSegment);
		CachedFragment* cachedFragment = GetFetchBuffer(true);
		long http_code = 0;
		long bitrate = 0;
		double downloadTime = 0;
		MediaType actualType = (MediaType)(initSegment?(eMEDIATYPE_INIT_VIDEO+mediaType):mediaType); //Need to revisit the logic

		if(!initSegment && mDownloadedFragment.ptr)
		{
			ret = true;
			cachedFragment->fragment.ptr = mDownloadedFragment.ptr;
			cachedFragment->fragment.len = mDownloadedFragment.len;
			cachedFragment->fragment.avail = mDownloadedFragment.avail;
			memset(&mDownloadedFragment, 0, sizeof(GrowableBuffer));
		}
		else
		{
			std::string effectiveUrl;
			int iFogError = -1;
			int iCurrentRate = aamp->rate; //  Store it as back up, As sometimes by the time File is downloaded, rate might have changed due to user initiated Trick-Play
			ret = aamp->LoadFragment(bucketType, fragmentUrl,effectiveUrl, &cachedFragment->fragment, curlInstance,
						range, actualType, &http_code, &downloadTime, &bitrate, &iFogError, fragmentDurationSeconds );

			if (iCurrentRate != AAMP_NORMAL_PLAY_RATE)
			{
				if(actualType == eMEDIATYPE_VIDEO)
				{
					actualType = eMEDIATYPE_IFRAME;
				}
				else if(actualType == eMEDIATYPE_INIT_VIDEO)
				{
					actualType = eMEDIATYPE_INIT_IFRAME;
				}
				//CID:101284 - To resolve the deadcode
			}

			//update videoend info
			aamp->UpdateVideoEndMetrics( actualType,
									bitrate? bitrate : fragmentDescriptor.Bandwidth,
									(iFogError > 0 ? iFogError : http_code),effectiveUrl,duration, downloadTime);
		}

		context->mCheckForRampdown = false;
		if(bitrate > 0 && bitrate != fragmentDescriptor.Bandwidth)
		{
			AAMPLOG_INFO("%s:%d Bitrate changed from %u to %ld", __FUNCTION__, __LINE__, fragmentDescriptor.Bandwidth, bitrate);
			fragmentDescriptor.Bandwidth = bitrate;
			context->SetTsbBandwidth(bitrate);
			mDownloadedFragment.ptr = cachedFragment->fragment.ptr;
			mDownloadedFragment.avail = cachedFragment->fragment.avail;
			mDownloadedFragment.len = cachedFragment->fragment.len;
			memset(&cachedFragment->fragment, 0, sizeof(GrowableBuffer));
			ret = false;
		}
		else if (!ret)
		{
			aamp_Free(&cachedFragment->fragment);
			if( aamp->DownloadsAreEnabled())
			{
				logprintf("%s:%d LoadFragment failed", __FUNCTION__, __LINE__);

				if (initSegment)
				{
					logprintf("%s:%d Init fragment fetch failed. fragmentUrl %s", __FUNCTION__, __LINE__, fragmentUrl.c_str());
				}

				if (mSkipSegmentOnError)
				{
					// Skip segment on error, and increse fail count
					segDLFailCount += 1;
				}
				else
				{
					// Rampdown already attempted on same segment
					// Reset flag for next fetch
					mSkipSegmentOnError = true;
				}
				if (MAX_SEG_DOWNLOAD_FAIL_COUNT <= segDLFailCount)
				{
					if(!playingAd)	//If playingAd, we are invalidating the current Ad in onAdEvent().
					{
						if (!initSegment)
						{
							AAMPLOG_ERR("%s:%d Not able to download fragments; reached failure threshold sending tune failed event",__FUNCTION__, __LINE__);
							aamp->SendDownloadErrorEvent(AAMP_TUNE_FRAGMENT_DOWNLOAD_FAILURE, http_code);
						}
						else
						{
							// When rampdown limit is not specified, init segment will be ramped down, this wil
							AAMPLOG_ERR("%s:%d Not able to download init fragments; reached failure threshold sending tune failed event",__FUNCTION__, __LINE__);
							aamp->SendDownloadErrorEvent(AAMP_TUNE_INIT_FRAGMENT_DOWNLOAD_FAILURE, http_code);
						}
					}
				}
				// DELIA-32287 - Profile RampDown check and rampdown is needed only for Video . If audio fragment download fails
				// should continue with next fragment,no retry needed .
				else if ((eTRACK_VIDEO == type) && !(context->CheckForRampDownLimitReached()))
				{
					// Attempt rampdown
					if (context->CheckForRampDownProfile(http_code))
					{
						context->mCheckForRampdown = true;
						if (!initSegment)
						{
							// Rampdown attempt success, download same segment from lower profile.
							mSkipSegmentOnError = false;
						}
						AAMPLOG_WARN( "StreamAbstractionAAMP_MPD::%s:%d > Error while fetching fragment:%s, failedCount:%d. decrementing profile",
								__FUNCTION__, __LINE__, fragmentUrl.c_str(), segDLFailCount);
					}
					else
					{
						if(!playingAd && initSegment)
						{
							// Already at lowest profile, send error event for init fragment.
							AAMPLOG_ERR("%s:%d Not able to download init fragments; reached failure threshold sending tune failed event",__FUNCTION__, __LINE__);
							aamp->SendDownloadErrorEvent(AAMP_TUNE_INIT_FRAGMENT_DOWNLOAD_FAILURE, http_code);
						}
						else
						{
							AAMPLOG_WARN("StreamAbstractionAAMP_MPD::%s:%d Already at the lowest profile, skipping segment", __FUNCTION__,__LINE__);
							context->mRampDownCount = 0;
						}
					}
				}
				else if (AAMP_IS_LOG_WORTHY_ERROR(http_code))
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_MPD::%s:%d > Error on fetching %s fragment. failedCount:%d",
							__FUNCTION__, __LINE__, name, segDLFailCount);
					// For init fragment, rampdown limit is reached. Send error event.
					if(!playingAd && initSegment)
					{
						aamp->SendDownloadErrorEvent(AAMP_TUNE_INIT_FRAGMENT_DOWNLOAD_FAILURE, http_code);
					}
				}
			}
		}
		else
		{
			cachedFragment->position = position;
			cachedFragment->duration = duration;
			cachedFragment->discontinuity = discontinuity;
#ifdef AAMP_DEBUG_INJECT
			if (discontinuity)
			{
				logprintf("%s:%d Discontinuous fragment", __FUNCTION__, __LINE__);
			}
			if ((1 << type) & AAMP_DEBUG_INJECT)
			{
				cachedFragment->uri.assign(fragmentUrl);
			}
#endif
			segDLFailCount = 0;
			if ((eTRACK_VIDEO == type) && (!initSegment))
			{
				// reset count on video fragment success
				context->mRampDownCount = 0;
			}
			UpdateTSAfterFetch();
			ret = true;
		}
		return ret;
	}


	/**
	 * @brief Listener to ABR profile change
	 */
	void ABRProfileChanged(void)
	{
		struct ProfileInfo profileMap = context->GetAdaptationSetAndRepresetationIndicesForProfile(context->currentProfileIndex);
		// Get AdaptationSet Index and Representation Index from the corresponding profile
		int adaptIdxFromProfile = profileMap.adaptationSetIndex;
		int reprIdxFromProfile = profileMap.representationIndex;
		if (!((adaptationSetIdx == adaptIdxFromProfile) && (representationIndex == reprIdxFromProfile)))
		{
			const IAdaptationSet *pNewAdaptationSet = context->GetAdaptationSetAtIndex(adaptIdxFromProfile);
			IRepresentation *pNewRepresentation = pNewAdaptationSet->GetRepresentation().at(reprIdxFromProfile);
			if(representation != NULL)
			{
				logprintf("StreamAbstractionAAMP_MPD::%s:%d - ABR %dx%d[%d] -> %dx%d[%d]", __FUNCTION__, __LINE__,
						representation->GetWidth(), representation->GetHeight(), representation->GetBandwidth(),
						pNewRepresentation->GetWidth(), pNewRepresentation->GetHeight(), pNewRepresentation->GetBandwidth());
				adaptationSetIdx = adaptIdxFromProfile;
				adaptationSet = pNewAdaptationSet;
				adaptationSetId = adaptationSet->GetId();
				representationIndex = reprIdxFromProfile;
				representation = pNewRepresentation;
				const std::vector<IBaseUrl *>*baseUrls = &representation->GetBaseURLs();
				if (baseUrls->size() != 0)
				{
					fragmentDescriptor.SetBaseURLs(baseUrls);
				}
				fragmentDescriptor.Bandwidth = representation->GetBandwidth();
				fragmentDescriptor.RepresentationID.assign(representation->GetId());
				profileChanged = true;
			}
			else
			{
				AAMPLOG_WARN("%s:%d :  representation is null", __FUNCTION__, __LINE__);  //CID:83962 - Null Returns
			}
		}
		else
		{
			traceprintf("StreamAbstractionAAMP_MPD::%s:%d - Not switching ABR %dx%d[%d] ", __FUNCTION__, __LINE__,
					representation->GetWidth(), representation->GetHeight(), representation->GetBandwidth());
		}

	}

	double GetBufferedDuration()
	{
		double position = aamp->GetPositionMs() / 1000.00;
		if(downloadedDuration >= position)
		{
			// If player faces buffering, this will be 0
			return (downloadedDuration - position);
		}
		else
		{
			// This avoids negative buffer, expecting
			// downloadedDuration never exceeds position in normal case.
			// Other case happens when contents are yet to be injected.
			downloadedDuration = 0;
			return downloadedDuration;
		}
	}


	/**
	 * @brief Notify discontinuity during trick-mode as PTS re-stamping is done in sink
	 */
	void SignalTrickModeDiscontinuity()
	{
		aamp->SignalTrickModeDiscontinuity();
	}

	/**
	 * @brief Returns if the end of track reached.
	 */
	bool IsAtEndOfTrack()
	{
		return eosReached;
	}

	MediaType mediaType;
	struct FragmentDescriptor fragmentDescriptor;
	const IAdaptationSet *adaptationSet;
	const IRepresentation *representation;
	int fragmentIndex;
	int timeLineIndex;
	int fragmentRepeatCount;
	uint64_t fragmentOffset;
	bool eos;
	bool profileChanged;
	bool discontinuity;
	GrowableBuffer mDownloadedFragment;

	double fragmentTime;
	double downloadedDuration;
	double periodStartOffset;
	uint64_t timeStampOffset;
	char *index_ptr;
	size_t index_len;
	uint64_t lastSegmentTime;
	uint64_t lastSegmentNumber;
	uint64_t lastSegmentDuration;
	int adaptationSetIdx;
	int representationIndex;
	StreamAbstractionAAMP_MPD* context;
	std::string initialization;
	uint32_t adaptationSetId;
	bool mSkipSegmentOnError;
}; // MediaStreamContext

/**
 * @class HeaderFetchParams
 * @brief Holds information regarding initialization fragment
 */
class HeaderFetchParams
{
public:
	HeaderFetchParams() : context(NULL), pMediaStreamContext(NULL), initialization(""), fragmentduration(0),
		isinitialization(false), discontinuity(false)
	{
	}
	HeaderFetchParams(const HeaderFetchParams&) = delete;
	HeaderFetchParams& operator=(const HeaderFetchParams&) = delete;
	class StreamAbstractionAAMP_MPD *context;
	class MediaStreamContext *pMediaStreamContext;
	string initialization;
	double fragmentduration;
	bool isinitialization;
	bool discontinuity;
};

/**
 * @class FragmentDownloadParams
 * @brief Holds data of fragment to be downloaded
 */
class FragmentDownloadParams
{
public:
	class StreamAbstractionAAMP_MPD *context;
	class MediaStreamContext *pMediaStreamContext;
	bool playingLastPeriod;
	long long lastPlaylistUpdateMS;
};

struct EarlyAvailablePeriodInfo
{
	EarlyAvailablePeriodInfo() : periodId(), isLicenseProcessed(false), isLicenseFailed(false), helper(nullptr){}
	std::string periodId;
	std::shared_ptr<AampDrmHelper> helper;
	bool isLicenseProcessed;
	bool isLicenseFailed;
};

static bool IsIframeTrack(IAdaptationSet *adaptationSet);


/**
 * @brief StreamAbstractionAAMP_MPD Constructor
 * @param aamp pointer to PrivateInstanceAAMP object associated with player
 * @param seek_pos Seek position
 * @param rate playback rate
 */
StreamAbstractionAAMP_MPD::StreamAbstractionAAMP_MPD(class PrivateInstanceAAMP *aamp,double seek_pos, float rate): StreamAbstractionAAMP(aamp),
	fragmentCollectorThreadStarted(false), mLangList(), seekPosition(seek_pos), rate(rate), fragmentCollectorThreadID(0), createDRMSessionThreadID(0),
	drmSessionThreadStarted(false), mpd(NULL), mNumberOfTracks(0), mCurrentPeriodIdx(0), mEndPosition(0), mIsLiveStream(true), mIsLiveManifest(true),
	mStreamInfo(NULL), mPrevStartTimeSeconds(0), mPrevLastSegurlMedia(""), mPrevLastSegurlOffset(0),
	mPeriodEndTime(0), mPeriodStartTime(0), mPeriodDuration(0), mMinUpdateDurationMs(DEFAULT_INTERVAL_BETWEEN_MPD_UPDATES_MS),
	mLastPlaylistDownloadTimeMs(0), mFirstPTS(0), mStartTimeOfFirstPTS(0), mAudioType(eAUDIO_UNKNOWN),
	mPrevAdaptationSetCount(0), mBitrateIndexVector(), mProfileMaps(), mIsFogTSB(false),
	mCurrentPeriod(NULL), mBasePeriodId(""), mBasePeriodOffset(0), mCdaiObject(NULL), mLiveEndPosition(0), mCulledSeconds(0)
	,mAdPlayingFromCDN(false)
	,mMaxTSBBandwidth(0), mTSBDepth(0)
	,mVideoPosRemainder(0)
	,mPresentationOffsetDelay(0)
	,mUpdateStreamInfo(false)
	,mAvailabilityStartTime(-1)
	,mFirstPeriodStartTime(0)
	,mDrmPrefs({{CLEARKEY_UUID, 1}, {WIDEVINE_UUID, 2}, {PLAYREADY_UUID, 3}})// Default values, may get changed due to config file
	,mLastDrmHelper()
	,deferredDRMRequestThread(NULL), deferredDRMRequestThreadStarted(false), mCommonKeyDuration(0)
	,mEarlyAvailableKeyIDMap(), mPendingKeyIDs(), mAbortDeferredLicenseLoop(false), mEarlyAvailablePeriodIds(), thumbnailtrack(), indexedTileInfo()
	, mMaxTracks(0)
{
	this->aamp = aamp;
	memset(&mMediaStreamContext, 0, sizeof(mMediaStreamContext));
	for (int i=0; i<AAMP_TRACK_COUNT; i++) mFirstFragPTS[i] = 0.0;
	GetABRManager().clearProfiles();
	mLastPlaylistDownloadTimeMs = aamp_GetCurrentTimeMS();

	// setup DRM prefs from config
	int highestPref = 0;
	#if 0
	std::vector<std::string> values;
	if (gpGlobalConfig->getMatchingUnknownKeys("drm-preference.", values))
	{
		for(auto&& item : values)
		{
			int i = atoi(item.substr(item.find(".") + 1).c_str());
			mDrmPrefs[gpGlobalConfig->getUnknownValue(item)] = i;
			if (i > highestPref)
			{
				highestPref = i;
			}
		}
	}
	#endif

	// Get the highest number
	for (auto const& pair: mDrmPrefs)
	{
		if(pair.second > highestPref)
		{
			highestPref = pair.second;
		}
	}
	
	// Give preference based on GetPreferredDRM.
	switch (aamp->GetPreferredDRM())
	{
		case eDRM_WideVine:
		{
			AAMPLOG_INFO("DRM Selected: WideVine");
			mDrmPrefs[WIDEVINE_UUID] = highestPref+1;
		}
			break;

		case eDRM_ClearKey:
		{
			AAMPLOG_INFO("DRM Selected: ClearKey");
			mDrmPrefs[CLEARKEY_UUID] = highestPref+1;
		}
			break;

		case eDRM_PlayReady:
		default:
		{
			AAMPLOG_INFO("DRM Selected: PlayReady");
			mDrmPrefs[PLAYREADY_UUID] = highestPref+1;
		}
			break;
	}

	AAMPLOG_INFO("DRM prefs");
	for (auto const& pair: mDrmPrefs) {
		AAMPLOG_INFO("{ %s, %d }", pair.first.c_str(), pair.second);
	}

	trickplayMode = (rate != AAMP_NORMAL_PLAY_RATE);
}

/**
 * @brief Check if mime type is compatible with media type
 * @param mimeType mime type
 * @param mediaType media type
 * @retval true if compatible
 */
static bool IsCompatibleMimeType(const std::string& mimeType, MediaType mediaType)
{
	bool isCompatible = false;

	switch ( mediaType )
	{
		case eMEDIATYPE_VIDEO:
			if (mimeType == "video/mp4")
				isCompatible = true;
			break;

		case eMEDIATYPE_AUDIO:
		case eMEDIATYPE_AUX_AUDIO:
			if ((mimeType == "audio/webm") ||
				(mimeType == "audio/mp4"))
				isCompatible = true;
			break;

		case eMEDIATYPE_SUBTITLE:
			if ((mimeType == "application/ttml+xml") ||
				(mimeType == "text/vtt") ||
				(mimeType == "application/mp4"))
				isCompatible = true;
			break;

		default:
			break;
	}

	return isCompatible;
}

/**
 * @brief Get Additional tag property value from any child node of MPD
 * @param Pointer to MPD child node, Tage Name , Property Name,
 * SchemeIdUri (if the propery mapped against scheme Id , default value is empty)
 * @retval return the property name if found, if not found return empty string
 */
static bool IsAtmosAudio(const IMPDElement *nodePtr)
{
	bool isAtmos = false;

	if (!nodePtr){
		AAMPLOG_ERR("%s:%d > API Failed due to Invalid Arguments", __FUNCTION__, __LINE__);
	}else{
		std::vector<INode*> childNodeList = nodePtr->GetAdditionalSubNodes();
		for (size_t j=0; j < childNodeList.size(); j++) {
			INode* childNode = childNodeList.at(j);
			const std::string& name = childNode->GetName();
			if (name == SUPPLEMENTAL_PROPERTY_TAG ) {
				if (childNode->HasAttribute("schemeIdUri")){
					const std::string& schemeIdUri = childNode->GetAttributeValue("schemeIdUri");
					if (schemeIdUri == SCHEME_ID_URI_EC3_EXT_CODEC ){
						if (childNode->HasAttribute("value")) {
							std::string value = childNode->GetAttributeValue("value");
							AAMPLOG_INFO("%s:%d > Recieved %s tag property value as %s ",
							__FUNCTION__, __LINE__, SUPPLEMENTAL_PROPERTY_TAG, value.c_str());
							if (value == EC3_EXT_VALUE_AUDIO_ATMOS){
								isAtmos = true;
								break;
							}

						}
					}
					else
					{
						AAMPLOG_WARN("%s:%d : schemeIdUri is not equals to  SCHEME_ID_URI_EC3_EXT_CODEC ", __FUNCTION__, __LINE__);  //CID:84346 - Null Returns
					}
				}
			}
		}
	}

	return isAtmos;
}
/**
 * @brief Get representation index of desired codec
 * @param adaptationSet Adaptation set object
 * @param[out] selectedCodecType type of desired representation
 * @retval index of desired representation
 */
static int GetDesiredCodecIndex(IAdaptationSet *adaptationSet, AudioType &selectedCodecType, uint32_t &selectedRepBandwidth,
				bool disableEC3,bool disableATMOS)
{
	int selectedRepIdx = -1;
	if(adaptationSet != NULL)
	{
		const std::vector<IRepresentation *> representation = adaptationSet->GetRepresentation();
		// check for codec defined in Adaptation Set
		const std::vector<string> adapCodecs = adaptationSet->GetCodecs();
		for (int representationIndex = 0; representationIndex < representation.size(); representationIndex++)
		{
			const dash::mpd::IRepresentation *rep = representation.at(representationIndex);
			uint32_t bandwidth = rep->GetBandwidth();
			const std::vector<string> codecs = rep->GetCodecs();
			AudioType audioType = eAUDIO_UNKNOWN;
			string codecValue="";
			// check if Representation includec codec
			if(codecs.size())
				codecValue=codecs.at(0);
			else if(adapCodecs.size()) // else check if Adaptation has codec defn
				codecValue = adapCodecs.at(0);
			// else no codec defined , go with unknown

			if (codecValue == "ec+3")
			{
	#ifndef __APPLE__
				audioType = eAUDIO_ATMOS;
	#endif
			}
			else if (codecValue == "ec-3")
			{
				audioType = eAUDIO_DDPLUS;
				/*
				* check whether ATMOS Flag is set as per ETSI TS 103 420
				*/
				if (IsAtmosAudio(rep)){
					AAMPLOG_INFO("%s:%d > Setting audio codec as eAUDIO_ATMOS as per ETSI TS 103 420",
						 __FUNCTION__, __LINE__);
					audioType = eAUDIO_ATMOS;
				}
			}
			else if( codecValue == "opus" || codecValue.find("vorbis") != std::string::npos )
			{
				audioType = eAUDIO_UNSUPPORTED;
			}
			else if( codecValue == "aac" || codecValue.find("mp4") != std::string::npos )
			{
				audioType = eAUDIO_AAC;
			}
			/*
			* By default the audio profile selection priority is set as ATMOS then DD+ then AAC
			* Note that this check comes after the check of selected language.
			* disableATMOS: avoid use of ATMOS track
			* disableEC3: avoid use of DDPLUS and ATMOS tracks
			*/
			if ((selectedCodecType == eAUDIO_UNKNOWN && (audioType != eAUDIO_UNSUPPORTED || selectedRepBandwidth == 0)) || // Select any profile for the first time, reject unsupported streams then
				(selectedCodecType == audioType && bandwidth>selectedRepBandwidth) || // same type but better quality
				(selectedCodecType < eAUDIO_ATMOS && audioType == eAUDIO_ATMOS && !disableATMOS && !disableEC3) || // promote to atmos
				(selectedCodecType < eAUDIO_DDPLUS && audioType == eAUDIO_DDPLUS && !disableEC3) || // promote to ddplus
				(selectedCodecType != eAUDIO_AAC && audioType == eAUDIO_AAC && disableEC3) || // force AAC
				(selectedCodecType == eAUDIO_UNSUPPORTED) // anything better than nothing
				)
			{
				selectedRepIdx = representationIndex;
				selectedCodecType = audioType;
				selectedRepBandwidth = bandwidth;
				AAMPLOG_INFO("StreamAbstractionAAMP_MPD::%s %d  > SelectedRepIndex : %d ,selectedCodecType : %d, selectedRepBandwidth: %d", __FUNCTION__, __LINE__, selectedRepIdx, selectedCodecType, selectedRepBandwidth);
			}
		}
	}
	else
	{
		AAMPLOG_WARN("%s:%d :  adaptationSet  is null", __FUNCTION__, __LINE__);  //CID:85233 - Null Returns
	}
	return selectedRepIdx;
}

/**
 * @brief Get representation index of desired video codec
 * @param adaptationSet Adaptation set object
 * @param[out] selectedRepIdx index of desired representation
 * @retval index of desired representation
 */
static int GetDesiredVideoCodecIndex(IAdaptationSet *adaptationSet)
{
	const std::vector<IRepresentation *> representation = adaptationSet->GetRepresentation();
	int selectedRepIdx = -1;
	for (int representationIndex = 0; representationIndex < representation.size(); representationIndex++)
	{
		const dash::mpd::IRepresentation *rep = representation.at(representationIndex);

		const std::vector<string> adapCodecs = adaptationSet->GetCodecs();
		const std::vector<string> codecs = rep->GetCodecs();
		string codecValue="";
		if(codecs.size())
			codecValue=codecs.at(0);
		else if(adapCodecs.size())
			codecValue = adapCodecs.at(0);

		//Ignore vp8 and vp9 codec video profiles(webm)
		if(codecValue.find("vp") == std::string::npos)
		{
			selectedRepIdx = representationIndex;
		}
	}
	return selectedRepIdx;
}

/**
 * @brief Return the name corresponding to the Media Type
 * @param mediaType media type
 * @retval the name of the mediaType
 */
static const char* getMediaTypeName( MediaType mediaType )
{
	switch(mediaType)
	{
		case eMEDIATYPE_VIDEO:
                        return MEDIATYPE_VIDEO;
                case eMEDIATYPE_AUDIO:
                        return MEDIATYPE_AUDIO;
                case eMEDIATYPE_SUBTITLE:
                        return MEDIATYPE_TEXT;
                case eMEDIATYPE_IMAGE:
                        return MEDIATYPE_IMAGE;
				case eMEDIATYPE_AUX_AUDIO:
					return MEDIATYPE_AUX_AUDIO;
                default:
                        return NULL;
	}
}

/**
 * @brief Check if adaptation set is of a given media type
 * @param adaptationSet adaptation set
 * @param mediaType media type
 * @retval true if adaptation set is of the given media type
 */
static bool IsContentType(IAdaptationSet *adaptationSet, MediaType mediaType )
{
	const char *name = getMediaTypeName(mediaType);
	if(name != NULL)
	{
		if (adaptationSet->GetContentType() == name)
		{
			return true;
		}
		else if (adaptationSet->GetContentType() == "muxed")
		{
			logprintf("excluding muxed content");
		}
		else
		{
			if (IsCompatibleMimeType(adaptationSet->GetMimeType(), mediaType) )
			{
				return true;
			}
			const std::vector<IRepresentation *> &representation = adaptationSet->GetRepresentation();
			for (int i = 0; i < representation.size(); i++)
			{
				const IRepresentation * rep = representation.at(i);
				if (IsCompatibleMimeType(rep->GetMimeType(), mediaType) )
				{
					return true;
				}
			}

			const std::vector<IContentComponent *>contentComponent = adaptationSet->GetContentComponent();
			for( int i = 0; i < contentComponent.size(); i++)
			{
				if (contentComponent.at(i)->GetContentType() == name)
				{
					return true;
				}
			}
		}
	}
	else
	{
		AAMPLOG_WARN("%s:%d : name is null", __FUNCTION__, __LINE__);  //CID:86093 - Null Returns
	}
	return false;
}

/**
 * @brief read unsigned 32 bit value and update buffer pointer
 * @param[in][out] pptr buffer
 * @retval 32 bit value
 */
static unsigned int Read32( const char **pptr)
{
	const char *ptr = *pptr;
	unsigned int rc = 0;
	for (int i = 0; i < 4; i++)
	{
		rc <<= 8;
		rc |= (unsigned char)*ptr++;
	}
	*pptr = ptr;
	return rc;
}


/**
 * @brief Parse segment index box
 * @note The SegmentBase indexRange attribute points to Segment Index Box location with segments and random access points.
 * @param start start of box
 * @param size size of box
 * @param segmentIndex segment index
 * @param[out] referenced_size referenced size
 * @param[out] referenced_duration referenced duration
 * @retval true on success
 */
static bool ParseSegmentIndexBox( const char *start, size_t size, int segmentIndex, unsigned int *referenced_size, float *referenced_duration, unsigned int *firstOffset)
{
	if (!start)
	{
		// If the fragment pointer is NULL then return from here, no need to process it further.
		return false;
	}

	const char **f = &start;

	unsigned int len = Read32(f);
	if (len != size)
	{
		AAMPLOG_WARN("Wrong size in ParseSegmentIndexBox %d found, %zu expected", len, size);
		if (firstOffset) *firstOffset = 0;
		return false;
	}

	unsigned int type = Read32(f);
	if (type != 'sidx')
	{
		AAMPLOG_WARN("Wrong type in ParseSegmentIndexBox %c%c%c%c found, %zu expected",
					 (type >> 24) % 0xff, (type >> 16) & 0xff, (type >> 8) & 0xff, type & 0xff, size);
		if (firstOffset) *firstOffset = 0;
		return false;

	}

	unsigned int version = Read32(f);
	unsigned int reference_ID = Read32(f);
	unsigned int timescale = Read32(f);
	unsigned int earliest_presentation_time = Read32(f);
	unsigned int first_offset = Read32(f);
	unsigned int count = Read32(f);

	if (firstOffset)
	{
		*firstOffset = first_offset;

		return true;
	}

	if( segmentIndex<count )
	{
		start += 12*segmentIndex;
		*referenced_size = Read32(f);
		*referenced_duration = Read32(f)/(float)timescale;
		unsigned int flags = Read32(f);

		return true;
	}

	return false;
}


/**
 * @brief Replace matching token with given number
 * @param str String in which operation to be performed
 * @param from token
 * @param toNumber number to replace token
 * @retval position
 */
static int replace(std::string& str, const std::string& from, uint64_t toNumber )
{
	int rc = 0;
	size_t tokenLength = from.length();

	for (;;)
	{
		bool done = true;
		size_t pos = 0;
		for (;;)
		{
			pos = str.find('$', pos);
			if (pos == std::string::npos)
			{
				break;
			}
			size_t next = str.find('$', pos + 1);
			if(next != 0)
			{
				if (str.substr(pos + 1, tokenLength) == from)
				{
					size_t formatLen = next - pos - tokenLength - 1;
					char buf[256];
					if (formatLen > 0)
					{
						std::string format = str.substr(pos + tokenLength + 1, formatLen);
						sprintf(buf, format.c_str(), toNumber);
						tokenLength += formatLen;
					}
					else
					{
						sprintf(buf, "%" PRIu64 "", toNumber);
					}
					str.replace(pos, tokenLength + 2, buf);
					done = false;
					rc++;
					break;
				}
				pos = next + 1;
			}
			else
			{
				AAMPLOG_WARN("%s:%d: next is not found ", __FUNCTION__, __LINE__);  //CID:81252 - checked return
				break;
			}
		}
		if (done) break;
	}

	return rc;
}


/**
 * @brief Replace matching token with given string
 * @param str String in which operation to be performed
 * @param from token
 * @param toString string to replace token
 * @retval position
 */
static int replace(std::string& str, const std::string& from, const std::string& toString )
{
	int rc = 0;
	size_t tokenLength = from.length();

	for (;;)
	{
		bool done = true;
		size_t pos = 0;
		for (;;)
		{
			pos = str.find('$', pos);
			if (pos == std::string::npos)
			{
				break;
			}
			size_t next = str.find('$', pos + 1);
			if(next != 0)
			{
				if (str.substr(pos + 1, tokenLength) == from)
				{
					str.replace(pos, tokenLength + 2, toString);
					done = false;
					rc++;
					break;
				}
				pos = next + 1;
			}
			else
			{
				AAMPLOG_WARN("%s:%d: Error at  next", __FUNCTION__, __LINE__);  //CID:81346 - checked return
				break;
			}
		}

		if (done) break;
	}

	return rc;
}


/**
 * @brief Generates fragment url from media information
 * @param[out] fragmentUrl fragment url
 * @param fragmentDescriptor descriptor
 * @param media media information string
 */
void StreamAbstractionAAMP_MPD::GetFragmentUrl( std::string& fragmentUrl, const FragmentDescriptor *fragmentDescriptor, std::string media)
{
	std::string constructedUri = fragmentDescriptor->GetMatchingBaseUrl();
	if( media.compare(0, 7, "http://")==0 || media.compare(0, 8, "https://")==0 )
	{	// don't pre-pend baseurl if media starts with http:// or https://
		constructedUri.clear();
	}
	else if (!constructedUri.empty())
	{
		if(ISCONFIGSET(eAAMPConfig_DASHIgnoreBaseURLIfSlash))
		{
			if (constructedUri == "/")
			{
				logprintf("%s:%d ignoring baseurl /", __FUNCTION__, __LINE__);
				constructedUri.clear();
			}
		}

		//Add '/' to BaseURL if not already available.
		if( constructedUri.compare(0, 7, "http://")==0 || constructedUri.compare(0, 8, "https://")==0 )
		{
			if( constructedUri.back() != '/' )
			{
				constructedUri += '/';
			}
		}
	}
	else
	{
		AAMPLOG_TRACE("%s:%d BaseURL not available", __FUNCTION__, __LINE__);
	}
	constructedUri += media;

	replace(constructedUri, "Bandwidth", fragmentDescriptor->Bandwidth);
	replace(constructedUri, "RepresentationID", fragmentDescriptor->RepresentationID);
	replace(constructedUri, "Number", fragmentDescriptor->Number);
	replace(constructedUri, "Time", fragmentDescriptor->Time );

	aamp_ResolveURL(fragmentUrl, fragmentDescriptor->manifestUrl, constructedUri.c_str(),ISCONFIGSET(eAAMPConfig_PropogateURIParam));
}

/**
 * @brief Gets a curlInstance index for a given MediaType
 * @param type the stream MediaType
 * @retval AampCurlInstance index to curl_easy_perform session
 */
static AampCurlInstance getCurlInstanceByMediaType(MediaType type)
{
	AampCurlInstance instance;

	switch (type)
	{
	case eMEDIATYPE_VIDEO:
		instance = eCURLINSTANCE_VIDEO;
		break;
	case eMEDIATYPE_AUDIO:
		instance = eCURLINSTANCE_AUDIO;
		break;
	case eMEDIATYPE_SUBTITLE:
		instance = eCURLINSTANCE_SUBTITLE;
		break;
	default:
		instance = eCURLINSTANCE_VIDEO;
		break;
	}

	return instance;
}

static void deIndexTileInfo(std::vector<TileInfo> &indexedTileInfo)
{
	logprintf("In %s indexedTileInfo size=%d",__FUNCTION__,indexedTileInfo.size());
	for(int i=0;i<indexedTileInfo.size();i++)
	{
		if( indexedTileInfo[i].url )
		{
			free( (char *)indexedTileInfo[i].url );
			indexedTileInfo[i].url = NULL;
		}
	}
	indexedTileInfo.clear();
	traceprintf("%s exiting",__FUNCTION__);

}
/**
 * @brief Fetch and cache a fragment
 *
 * @param pMediaStreamContext Track object pointer
 * @param media media descriptor string
 * @param fragmentDuration duration of fragment in seconds
 * @param isInitializationSegment true if fragment is init fragment
 * @param curlInstance curl instance to be used to fetch
 * @param discontinuity true if fragment is discontinuous
 * @retval true on fetch success
 */
bool StreamAbstractionAAMP_MPD::FetchFragment(MediaStreamContext *pMediaStreamContext, std::string media, double fragmentDuration, bool isInitializationSegment, unsigned int curlInstance, bool discontinuity)
{ // given url, synchronously download and transmit associated fragment
	bool retval = true;
	std::string fragmentUrl;
	GetFragmentUrl(fragmentUrl, &pMediaStreamContext->fragmentDescriptor, media);
	//CID:96900 - Removex the len variable which is initialized but not used
	float position;
	if(isInitializationSegment)
	{
		if(!(pMediaStreamContext->initialization.empty()) && (0 == pMediaStreamContext->initialization.compare(fragmentUrl))&& !discontinuity)
		{
			AAMPLOG_TRACE("We have pushed the same initailization segment for %s skipping", getMediaTypeName(MediaType(pMediaStreamContext->type)));
			return retval;
		}
		else
		{
			pMediaStreamContext->initialization = std::string(fragmentUrl);
		}
	}
	position = pMediaStreamContext->fragmentTime;

	float duration = fragmentDuration;
	if(rate > AAMP_NORMAL_PLAY_RATE)
	{
		position = position/rate;
		AAMPLOG_INFO("StreamAbstractionAAMP_MPD::%s:%d rate %f pMediaStreamContext->fragmentTime %f updated position %f",
				__FUNCTION__, __LINE__, rate, pMediaStreamContext->fragmentTime, position);
		int  vodTrickplayFPS;
		GETCONFIGVALUE(eAAMPConfig_VODTrickPlayFPS,vodTrickplayFPS); 
		duration = duration/rate * vodTrickplayFPS;
		//aamp->disContinuity();
	}
//	logprintf("%s:%d [%s] mFirstFragPTS %f  position %f -> %f ", __FUNCTION__, __LINE__, pMediaStreamContext->name, mFirstFragPTS[pMediaStreamContext->mediaType], position, mFirstFragPTS[pMediaStreamContext->mediaType]+position);
	position += mFirstFragPTS[pMediaStreamContext->mediaType];
	bool fragmentCached = pMediaStreamContext->CacheFragment(fragmentUrl, curlInstance, position, duration, NULL, isInitializationSegment, discontinuity
		,(mCdaiObject->mAdState == AdState::IN_ADBREAK_AD_PLAYING));
	// Check if we have downloaded the fragment and waiting for init fragment download on
	// bitrate switching before caching it.
	bool fragmentSaved = (NULL != pMediaStreamContext->mDownloadedFragment.ptr);

	if (!fragmentCached)
	{
		if(!fragmentSaved)
		{
		logprintf("StreamAbstractionAAMP_MPD::%s:%d failed. fragmentUrl %s fragmentTime %f", __FUNCTION__, __LINE__, fragmentUrl.c_str(), pMediaStreamContext->fragmentTime);
			if(mCdaiObject->mAdState == AdState::IN_ADBREAK_AD_PLAYING && (isInitializationSegment || pMediaStreamContext->segDLFailCount >= MAX_AD_SEG_DOWNLOAD_FAIL_COUNT))
			{
				logprintf("StreamAbstractionAAMP_MPD::%s:%d [CDAI] Ad fragment not available. Playback failed.", __FUNCTION__, __LINE__);
				mCdaiObject->mAdFailed = true;
			}
		}
		retval = false;
	}

	/**In the case of ramp down same fragment will be retried
	 *Avoid fragmentTime update in such scenarios.
	 *In other cases if it's success or failure, AAMP will be going
	 *For next fragment so update fragmentTime with fragment duration
	 */
	if(!mCheckForRampdown && !fragmentSaved)
	{
		if(rate > 0)
		{
			pMediaStreamContext->fragmentTime += fragmentDuration;
			if(pMediaStreamContext->mediaType == eMEDIATYPE_VIDEO) mBasePeriodOffset += fragmentDuration;
		}
		else
		{
			pMediaStreamContext->fragmentTime -= fragmentDuration;
			if(pMediaStreamContext->mediaType == eMEDIATYPE_VIDEO) mBasePeriodOffset -= fragmentDuration;
			if(pMediaStreamContext->fragmentTime < 0)
			{
				pMediaStreamContext->fragmentTime = 0;
			}
		}
		pMediaStreamContext->fragmentTime = ceil(pMediaStreamContext->fragmentTime * 1000.0) / 1000.0;
	}
	return retval;
}

/**
 * @brief Fetch and push next fragment
 * @param pMediaStreamContext Track object
 * @param curlInstance instance of curl to be used to fetch
 * @retval true if push is done successfully
 */
bool StreamAbstractionAAMP_MPD::PushNextFragment( class MediaStreamContext *pMediaStreamContext, unsigned int curlInstance)
{
	bool retval=false;

	SegmentTemplates segmentTemplates(pMediaStreamContext->representation->GetSegmentTemplate(),
					pMediaStreamContext->adaptationSet->GetSegmentTemplate() );
#ifdef DEBUG_TIMELINE
	logprintf("%s:%d Type[%d] timeLineIndex %d fragmentRepeatCount %u PeriodDuration:%f mCurrentPeriodIdx:%d mPeriodStartTime %f", __FUNCTION__, __LINE__,pMediaStreamContext->type,
				pMediaStreamContext->timeLineIndex, pMediaStreamContext->fragmentRepeatCount,mPeriodDuration,mCurrentPeriodIdx,mPeriodStartTime );
#endif
	if( segmentTemplates.HasSegmentTemplate() )
	{
		std::string media = segmentTemplates.Getmedia();
		const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
		uint32_t timeScale = segmentTemplates.GetTimescale();

		if (segmentTimeline)
		{
			std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
			if(!timelines.empty())
			{
#ifdef DEBUG_TIMELINE
				logprintf("%s:%d Type[%d] timelineCnt=%d timeLineIndex:%d FDTime=%f L=%" PRIu64 " [fragmentTime = %f,  mLiveEndPosition = %f]", __FUNCTION__, __LINE__,
					pMediaStreamContext->type ,timelines.size(),pMediaStreamContext->timeLineIndex,pMediaStreamContext->fragmentDescriptor.Time,pMediaStreamContext->lastSegmentTime
					, pMediaStreamContext->fragmentTime, mLiveEndPosition);
#endif
				if ((pMediaStreamContext->timeLineIndex >= timelines.size()) || (pMediaStreamContext->timeLineIndex < 0)
						||(AdState::IN_ADBREAK_AD_PLAYING == mCdaiObject->mAdState &&
							((rate > AAMP_NORMAL_PLAY_RATE && pMediaStreamContext->fragmentTime >= mLiveEndPosition)
							 ||(rate < 0 && pMediaStreamContext->fragmentTime <= 0))))
				{
					AAMPLOG_INFO("%s:%d Type[%d] EOS. timeLineIndex[%d] size [%lu]",__FUNCTION__, __LINE__,pMediaStreamContext->type, pMediaStreamContext->timeLineIndex, timelines.size());
					pMediaStreamContext->eos = true;
				}
				else
				{
					// Presentation Offset handling - When period is splitted for Ads, additional segments
					// for last period will be added which are not required to play.Need to play based on
					// presentationtimeoffset attribute

					// When new period starts , need to check if PresentationOffset exists and its
					// different from startTime of first timeline
					// ->During refresh of manifest, fragmentDescriptor.Time != 0.Not to update PTSOffset
					// ->During period change or start of playback , fragmentDescriptor.Time=0. Need to
					//      update with PTSOffset
					uint64_t presentationTimeOffset = segmentTemplates.GetPresentationTimeOffset();
					uint32_t tScale = segmentTemplates.GetTimescale();
					uint64_t periodStart = 0;
					string startTimeStr = mpd->GetPeriods().at(mCurrentPeriodIdx)->GetStart();
					if(!startTimeStr.empty())
					{
						periodStart = (ParseISO8601Duration(startTimeStr.c_str()) / 10000);
						pMediaStreamContext->timeStampOffset = (periodStart - (presentationTimeOffset/tScale));
					}else
					{
						pMediaStreamContext->timeStampOffset = 0;
					}

					if (presentationTimeOffset > 0 && pMediaStreamContext->lastSegmentDuration ==  0
						&& pMediaStreamContext->fragmentDescriptor.Time == 0)
					{
						// Check the first timeline starttime.
						int index = 0;
						uint64_t startTime = 0;
						ITimeline *timeline = timelines.at(index);
						// Some timeline may not have attribute for timeline , check it .
						map<string, string> attributeMap = timeline->GetRawAttributes();
						if(attributeMap.find("t") != attributeMap.end())
						{
							startTime = timeline->GetStartTime();
						}
						else
						{
							startTime = presentationTimeOffset;
						}

						// This logic comes into picture if startTime is different from
						// presentation Offset
						if(startTime != presentationTimeOffset)
						{
							// if startTime is 0 or if no timeline attribute "t"
							if(startTime == 0)
							{
								AAMPLOG_INFO("%s:%d Type[%d] Setting start time with PTSOffset:%" PRIu64 "",__FUNCTION__, __LINE__,pMediaStreamContext->type,presentationTimeOffset);
								startTime = presentationTimeOffset;
							}
							else
							{
								// Non zero startTime , need to traverse and find the right
								// line index and repeat number
								uint32_t duration =0;
								uint32_t repeatCount =0;
								uint64_t nextStartTime = 0;
								int offsetNumber = 0;
								// This loop is to go to the right index and segment number
								//based on presentationTimeOffset
								pMediaStreamContext->fragmentRepeatCount = 0;
								while(index<timelines.size())
								{
									timeline = timelines.at(index);
									map<string, string> attributeMap = timeline->GetRawAttributes();
									if(attributeMap.find("t") != attributeMap.end())
									{
										startTime = timeline->GetStartTime();
									}
									else
									{
										startTime = nextStartTime;
									}
									duration = timeline->GetDuration();
									// For Dynamic segment timeline content
									if (0 == startTime && 0 != duration)
									{
										startTime = nextStartTime;
									}
									repeatCount = timeline->GetRepeatCount();
									nextStartTime = startTime+((uint64_t)(repeatCount+1)*duration);
									// found the right index
									if(nextStartTime > (presentationTimeOffset+1))
									{
										// if timeline has repeat value ,go to correct offset
										if (repeatCount != 0)
										{
											uint64_t segmentStartTime = startTime;
											for(int i=0; i<repeatCount; i++)
											{
												segmentStartTime += duration;
												if(segmentStartTime > (presentationTimeOffset+1))
												{       // found the right offset
													break;
												}
												startTime = segmentStartTime;
												offsetNumber++;
												pMediaStreamContext->fragmentRepeatCount++;
											}
										}
										break; // break the while loop
									}
									// Add all the repeat count before going to next timeline
									offsetNumber += (repeatCount+1);
									index++;
								}
								// After exit of loop , update the fields
								if(index != timelines.size())
								{
									pMediaStreamContext->fragmentDescriptor.Number += offsetNumber;
									pMediaStreamContext->timeLineIndex = index;
									AAMPLOG_INFO("%s:%d Type[%d] skipping fragments[%d] to Index:%d FNum=%d Repeat:%d", __FUNCTION__, __LINE__, pMediaStreamContext->type,offsetNumber,index,pMediaStreamContext->fragmentDescriptor.Number,pMediaStreamContext->fragmentRepeatCount);
								}
							}
						}
						// Modify the descriptor time to start download
						pMediaStreamContext->fragmentDescriptor.Time = startTime;
#ifdef DEBUG_TIMELINE
					logprintf("%s:%d Type[%d] timelineCnt=%d timeLineIndex:%d FDTime=%f L=%" PRIu64 " [fragmentTime = %f,  mLiveEndPosition = %f]", __FUNCTION__, __LINE__,
						pMediaStreamContext->type ,timelines.size(),pMediaStreamContext->timeLineIndex,pMediaStreamContext->fragmentDescriptor.Time,pMediaStreamContext->lastSegmentTime,
						pMediaStreamContext->fragmentTime, mLiveEndPosition);
#endif
					}
					else if (pMediaStreamContext->fragmentRepeatCount == 0)
					{
						ITimeline *timeline = timelines.at(pMediaStreamContext->timeLineIndex);
						uint64_t startTime = 0;
						map<string, string> attributeMap = timeline->GetRawAttributes();
						if(attributeMap.find("t") != attributeMap.end())
						{
							// If there is a presentation offset time, update start time to that value.
							startTime = timeline->GetStartTime();
						}
						else
						{ // DELIA-35059
							startTime = pMediaStreamContext->fragmentDescriptor.Time;
						}
						if(mIsLiveStream)
						{
							// After mpd refresh , Time will be 0. Need to traverse to the right fragment for playback
							if(0 == pMediaStreamContext->fragmentDescriptor.Time)
							{
								uint32_t duration =0;
								uint32_t repeatCount =0;
								uint64_t nextStartTime = 0;
								int index = pMediaStreamContext->timeLineIndex;
								// This for loop is to go to the right index based on LastSegmentTime
			
								for(;index<timelines.size();index++)
								{
									timeline = timelines.at(index);
									map<string, string> attributeMap = timeline->GetRawAttributes();
									if(attributeMap.find("t") != attributeMap.end())
									{
										startTime = timeline->GetStartTime();
									}
									else
									{
										startTime = pMediaStreamContext->fragmentDescriptor.Time;
									}

									duration = timeline->GetDuration();
									// For Dynamic segment timeline content
									if (0 == startTime && 0 != duration)
									{
										startTime = nextStartTime;
									}
									repeatCount = timeline->GetRepeatCount();
									nextStartTime = startTime+((uint64_t)(repeatCount+1)*duration);  //CID:98056 - Resolve Overfloew Before Widen
									if(pMediaStreamContext->lastSegmentTime < nextStartTime)
									{
										break;
									}
									pMediaStreamContext->fragmentDescriptor.Number += (repeatCount+1);
								}// end of for

								
								/*
								*  Boundary check added to handle the edge case leading to crash,
								*  reported in DELIA-30316.
								*/
								if(index == timelines.size())
								{
									logprintf("%s:%d Type[%d] Boundary Condition !!! Index(%d) reached Max.Start=%" PRIu64 " Last=%" PRIu64 " ",__FUNCTION__, __LINE__,
										pMediaStreamContext->type,index,startTime,pMediaStreamContext->lastSegmentTime);
									index--;
									startTime = pMediaStreamContext->lastSegmentTime;
									pMediaStreamContext->fragmentRepeatCount = repeatCount+1;
								}

#ifdef DEBUG_TIMELINE
								logprintf("%s:%d Type[%d] t=%" PRIu64 " L=%" PRIu64 " d=%d r=%d Index=%d Num=%" PRIu64 " FTime=%f",__FUNCTION__, __LINE__, pMediaStreamContext->type,
								startTime,pMediaStreamContext->lastSegmentTime, duration, repeatCount,index,
								pMediaStreamContext->fragmentDescriptor.Number,pMediaStreamContext->fragmentTime);
#endif
								pMediaStreamContext->timeLineIndex = index;
								// Now we reached the right row , need to traverse the repeat index to reach right node
								// Whenever new fragments arrive inside the same timeline update fragment number,repeat count and startNumber.
								// If first fragment start Number is zero, check lastSegmentDuration of period timeline for update.
								while((pMediaStreamContext->fragmentRepeatCount < repeatCount && startTime < pMediaStreamContext->lastSegmentTime) ||
									(startTime == 0 && pMediaStreamContext->lastSegmentTime == 0 && pMediaStreamContext->lastSegmentDuration != 0))
								{
									startTime += duration;
									pMediaStreamContext->fragmentDescriptor.Number++;
									pMediaStreamContext->fragmentRepeatCount++;
								}
#ifdef DEBUG_TIMELINE
								logprintf("%s:%d Type[%d] t=%" PRIu64 " L=%" PRIu64 " d=%d r=%d fragRep=%d Index=%d Num=%" PRIu64 " FTime=%f",__FUNCTION__, __LINE__, pMediaStreamContext->type,
								startTime,pMediaStreamContext->lastSegmentTime, duration, repeatCount,pMediaStreamContext->fragmentRepeatCount,pMediaStreamContext->timeLineIndex,
								pMediaStreamContext->fragmentDescriptor.Number,pMediaStreamContext->fragmentTime);
#endif
							}
						}// if starttime
						if(0 == pMediaStreamContext->timeLineIndex)
						{
							AAMPLOG_INFO("%s:%d Type[%d] update startTime to %" PRIu64 , __FUNCTION__, __LINE__,pMediaStreamContext->type, startTime);
						}
						pMediaStreamContext->fragmentDescriptor.Time = startTime;
#ifdef DEBUG_TIMELINE
						logprintf("%s:%d Type[%d] Setting startTime to %" PRIu64 , __FUNCTION__, __LINE__,pMediaStreamContext->type, startTime);
#endif
					}// if fragRepeat == 0

					ITimeline *timeline = timelines.at(pMediaStreamContext->timeLineIndex);
					uint32_t repeatCount = timeline->GetRepeatCount();
					uint32_t duration = timeline->GetDuration();
#ifdef DEBUG_TIMELINE
					logprintf("%s:%d Type[%d] FDt=%f L=%" PRIu64 " d=%d r=%d fragrep=%d x=%d num=%lld",__FUNCTION__, __LINE__,
					pMediaStreamContext->type,pMediaStreamContext->fragmentDescriptor.Time,
					pMediaStreamContext->lastSegmentTime, duration, repeatCount,pMediaStreamContext->fragmentRepeatCount,
					pMediaStreamContext->timeLineIndex,pMediaStreamContext->fragmentDescriptor.Number);
#endif
					if ((pMediaStreamContext->fragmentDescriptor.Time > pMediaStreamContext->lastSegmentTime) || (0 == pMediaStreamContext->lastSegmentTime))
					{
						double fragmentDuration = ComputeFragmentDuration(duration,timeScale);
						double endTime  = (mPeriodStartTime+(mPeriodDuration/1000));
						ITimeline *firstTimeline = timelines.at(0);
						double positionInPeriod = 0;
						if((firstTimeline->GetRawAttributes().find("t") != firstTimeline->GetRawAttributes().end()) && pMediaStreamContext->lastSegmentDuration > 0)
						{
							// 't' in first timeline is expected.
							positionInPeriod = (pMediaStreamContext->lastSegmentDuration - firstTimeline->GetStartTime()) / timeScale;
						}
#ifdef DEBUG_TIMELINE
						logprintf("%s:%d Type[%d] presenting FDt%f Number(%lld) Last=%" PRIu64 " Duration(%d) FTime(%f) endTime:%f",__FUNCTION__, __LINE__,
							pMediaStreamContext->type,pMediaStreamContext->fragmentDescriptor.Time,pMediaStreamContext->fragmentDescriptor.Number,pMediaStreamContext->lastSegmentTime,duration,pMediaStreamContext->fragmentTime,endTime);
#endif
						retval = true;
						if(!mPeriodDuration || (mPeriodDuration !=0 && (mPeriodStartTime + positionInPeriod) < endTime))
						{
							retval = FetchFragment( pMediaStreamContext, media, fragmentDuration, false, curlInstance);
						}
						else
						{
							AAMPLOG_WARN("%s:%d Type[%d] Skipping Fetchfragment, Number(%lld) fragment beyond duration. fragmentPosition: %lf periodEndTime : %lf", __FUNCTION__, __LINE__ , pMediaStreamContext->type
								, pMediaStreamContext->fragmentDescriptor.Number, positionInPeriod , endTime);
						}
						if(retval)
						{
							pMediaStreamContext->lastSegmentTime = pMediaStreamContext->fragmentDescriptor.Time;
							pMediaStreamContext->lastSegmentDuration = pMediaStreamContext->fragmentDescriptor.Time + duration;
							// pMediaStreamContext->downloadedDuration is introduced to calculate the bufferedduration value.
							// Update position in period after fragment download
							positionInPeriod += fragmentDuration;
							if(mIsFogTSB || (mIsLiveStream && !mIsLiveManifest && !ISCONFIGSET(eAAMPConfig_UseAbsoluteTimeline)))
							{
								// Fog linear, or cDVR changed from static => dynamic without abstimeline reporting.
								pMediaStreamContext->downloadedDuration = pMediaStreamContext->fragmentTime + aamp->culledOffset;
							}
							else if(mIsLiveStream && ISCONFIGSET(eAAMPConfig_UseAbsoluteTimeline))
							{
								// Non-fog Linear with absolute position reporting
								pMediaStreamContext->downloadedDuration = (GetPeriodStartTime(mpd, mCurrentPeriodIdx) - mAvailabilityStartTime) + positionInPeriod;
							}
							else
							{
								// For VOD and non-fog linear without Absolute timeline
								// calculate relative position in manifest
								// For VOD, culledSeconds will be 0, and for linear it is added to first period start
								pMediaStreamContext->downloadedDuration = aamp->culledSeconds + GetPeriodStartTime(mpd, mCurrentPeriodIdx) - GetPeriodStartTime(mpd, 0) + positionInPeriod;
							}
						}
						else if((mIsFogTSB && !mAdPlayingFromCDN) && pMediaStreamContext->mDownloadedFragment.ptr)
						{
							pMediaStreamContext->profileChanged = true;
							profileIdxForBandwidthNotification = GetProfileIdxForBandwidthNotification(pMediaStreamContext->fragmentDescriptor.Bandwidth);
							FetchAndInjectInitialization();
							UpdateRampdownProfileReason();
							return false;
						}
						else if( mCheckForRampdown && pMediaStreamContext->mediaType == eMEDIATYPE_VIDEO)
						{
							// DELIA-31780 - On audio fragment download failure (http500), rampdown was attempted .
							// rampdown is only needed for video fragments not for audio.
							// second issue : after rampdown lastSegmentTime was going into "0" . When this combined with mpd refresh immediately after rampdown ,
							// startTime is set to start of Period . This caused audio fragment download from "0" resulting in PTS mismatch and mute
							// Fix : Only do lastSegmentTime correction for video not for audio
							//	 lastSegmentTime to be corrected with duration of last segment attempted .
							return retval; /* Incase of fragment download fail, no need to increase the fragment number to download next fragment,
									 * instead check the same fragment in lower profile. */
						}
					}
					else if (rate < 0)
					{
#ifdef DEBUG_TIMELINE
						logprintf("%s:%d Type[%d] presenting %f" , __FUNCTION__, __LINE__,pMediaStreamContext->type,pMediaStreamContext->fragmentDescriptor.Time);
#endif
						pMediaStreamContext->lastSegmentTime = pMediaStreamContext->fragmentDescriptor.Time;
						pMediaStreamContext->lastSegmentDuration = pMediaStreamContext->fragmentDescriptor.Time + duration;
						double fragmentDuration = ComputeFragmentDuration(duration,timeScale);
						retval = FetchFragment( pMediaStreamContext, media, fragmentDuration, false, curlInstance);
						if (!retval && ((mIsFogTSB && !mAdPlayingFromCDN) && pMediaStreamContext->mDownloadedFragment.ptr))
						{
							pMediaStreamContext->profileChanged = true;
							profileIdxForBandwidthNotification = GetProfileIdxForBandwidthNotification(pMediaStreamContext->fragmentDescriptor.Bandwidth);
							FetchAndInjectInitialization();
							UpdateRampdownProfileReason();
							return false;
						}
					}
					else if(pMediaStreamContext->mediaType == eMEDIATYPE_VIDEO &&
							((pMediaStreamContext->lastSegmentTime - pMediaStreamContext->fragmentDescriptor.Time) > TIMELINE_START_RESET_DIFF))
					{
						if(!mIsLiveStream || !aamp->IsLiveAdjustRequired())
						{
							pMediaStreamContext->lastSegmentTime = pMediaStreamContext->fragmentDescriptor.Time - 1;
							return false;
						}
						logprintf("%s:%d Calling ScheduleRetune to handle start-time reset lastSegmentTime=%" PRIu64 " start-time=%f" , __FUNCTION__, __LINE__, pMediaStreamContext->lastSegmentTime, pMediaStreamContext->fragmentDescriptor.Time);
						aamp->ScheduleRetune(eDASH_ERROR_STARTTIME_RESET, pMediaStreamContext->mediaType);
					}
					else
					{
#ifdef DEBUG_TIMELINE
						logprintf("%s:%d Type[%d] Before skipping. fragmentDescriptor.Time %f lastSegmentTime %" PRIu64 " Index=%d fragRep=%d,repMax=%d Number=%lld",__FUNCTION__, __LINE__,pMediaStreamContext->type,
							pMediaStreamContext->fragmentDescriptor.Time, pMediaStreamContext->lastSegmentTime,pMediaStreamContext->timeLineIndex,
							pMediaStreamContext->fragmentRepeatCount , repeatCount,pMediaStreamContext->fragmentDescriptor.Number);
#endif
						while(pMediaStreamContext->fragmentDescriptor.Time < pMediaStreamContext->lastSegmentTime &&
								pMediaStreamContext->fragmentRepeatCount < repeatCount )
							{
								if(rate > 0)
								{
									pMediaStreamContext->fragmentDescriptor.Time += duration;
									pMediaStreamContext->fragmentDescriptor.Number++;
									pMediaStreamContext->fragmentRepeatCount++;
								}
							}
#ifdef DEBUG_TIMELINE
						logprintf("%s:%d Type[%d] After skipping. fragmentDescriptor.Time %f lastSegmentTime %" PRIu64 " Index=%d Number=%lld",__FUNCTION__, __LINE__,pMediaStreamContext->type,
								pMediaStreamContext->fragmentDescriptor.Time, pMediaStreamContext->lastSegmentTime,pMediaStreamContext->timeLineIndex,pMediaStreamContext->fragmentDescriptor.Number);
#endif
					}
					if(rate > 0)
					{
						pMediaStreamContext->fragmentDescriptor.Time += duration;
						pMediaStreamContext->fragmentDescriptor.Number++;
						pMediaStreamContext->fragmentRepeatCount++;
						if( pMediaStreamContext->fragmentRepeatCount > repeatCount)
						{
							pMediaStreamContext->fragmentRepeatCount = 0;
							pMediaStreamContext->timeLineIndex++;
						}
#ifdef DEBUG_TIMELINE
							logprintf("%s:%d Type[%d] After Incr. fragmentDescriptor.Time %f lastSegmentTime %" PRIu64 " Index=%d fragRep=%d,repMax=%d Number=%lld",__FUNCTION__, __LINE__,pMediaStreamContext->type,
							pMediaStreamContext->fragmentDescriptor.Time, pMediaStreamContext->lastSegmentTime,pMediaStreamContext->timeLineIndex,
							pMediaStreamContext->fragmentRepeatCount , repeatCount,pMediaStreamContext->fragmentDescriptor.Number);
#endif
					}
					else
					{
						pMediaStreamContext->fragmentDescriptor.Time -= duration;
						pMediaStreamContext->fragmentDescriptor.Number--;
						pMediaStreamContext->fragmentRepeatCount--;
						if( pMediaStreamContext->fragmentRepeatCount < 0)
						{
							pMediaStreamContext->timeLineIndex--;
							if(pMediaStreamContext->timeLineIndex >= 0)
							{
								pMediaStreamContext->fragmentRepeatCount = timelines.at(pMediaStreamContext->timeLineIndex)->GetRepeatCount();
							}
						}
					}
				}
			}
			else
			{
				AAMPLOG_WARN("%s:%d :  timelines is null", __FUNCTION__, __LINE__);  //CID:81702 ,82851 - Null Returns
			}
		}
		else
		{
#ifdef DEBUG_TIMELINE
			logprintf("%s:%d segmentTimeline not available", __FUNCTION__, __LINE__);
#endif

			double currentTimeSeconds = (double)aamp_GetCurrentTimeMS() / 1000;
			
			
			uint32_t duration = segmentTemplates.GetDuration();
			double fragmentDuration =  ComputeFragmentDuration(duration,timeScale);
			long startNumber = segmentTemplates.GetStartNumber();

			if (0 == pMediaStreamContext->lastSegmentNumber)
			{
				if (mIsLiveStream)
				{
					double liveTime = currentTimeSeconds - aamp->mLiveOffset;
					if(liveTime < mPeriodStartTime)
					{
						// Not to go beyond the period , as that is checked in skipfragments
						liveTime = mPeriodStartTime;
					}
						
					pMediaStreamContext->lastSegmentNumber = (long long)((liveTime - mPeriodStartTime) / fragmentDuration) + startNumber;
					pMediaStreamContext->fragmentDescriptor.Time = liveTime;
					AAMPLOG_INFO("%s %d Printing fragmentDescriptor.Number %" PRIu64 " Time=%f  ", __FUNCTION__, __LINE__, pMediaStreamContext->lastSegmentNumber, pMediaStreamContext->fragmentDescriptor.Time);
				}
				else
				{
					if (rate < 0)
					{
						pMediaStreamContext->fragmentDescriptor.Time = mPeriodEndTime;
					}
					else
					{
						pMediaStreamContext->fragmentDescriptor.Time = mPeriodStartTime;
					}
				}
			}

			// Recalculate the fragmentDescriptor.Time after periodic manifest updates
			if (mIsLiveStream && 0 == pMediaStreamContext->fragmentDescriptor.Time)
			{
				pMediaStreamContext->fragmentDescriptor.Time = mPeriodStartTime;
				
				if(pMediaStreamContext->lastSegmentNumber > startNumber )
				{
					pMediaStreamContext->fragmentDescriptor.Time += ((pMediaStreamContext->lastSegmentNumber - startNumber) * fragmentDuration);
				}
			}
			/**
			 *Find out if we reached end/beginning of period.
			 *First block in this 'if' is for VOD, where boundaries are 0 and PeriodEndTime
			 *Second block is for LIVE, where boundaries are
						 *  mPeriodStartTime and currentTime
			 */
			if ((!mIsLiveStream && ((mPeriodEndTime && (pMediaStreamContext->fragmentDescriptor.Time > mPeriodEndTime))
							|| (rate < 0 )))
					|| (mIsLiveStream && ((pMediaStreamContext->fragmentDescriptor.Time >= mPeriodEndTime)
							|| (pMediaStreamContext->fragmentDescriptor.Time < mPeriodStartTime))))  //CID:93022 - No effect
			{
				AAMPLOG_INFO("%s:%d EOS. fragmentDescriptor.Time=%f mPeriodEndTime=%lu mPeriodStartTime %lu  currentTimeSeconds %f FTime=%f",__FUNCTION__, __LINE__, pMediaStreamContext->fragmentDescriptor.Time, mPeriodEndTime, mPeriodStartTime, currentTimeSeconds, pMediaStreamContext->fragmentTime);
				pMediaStreamContext->lastSegmentNumber =0; // looks like change in period may happen now. hence reset lastSegmentNumber
				pMediaStreamContext->eos = true;
			}
			else if(mIsLiveStream && (pMediaStreamContext->fragmentDescriptor.Time + fragmentDuration) >= (currentTimeSeconds-mPresentationOffsetDelay))
			{
				int sleepTime = mMinUpdateDurationMs;
				sleepTime = (sleepTime > MAX_DELAY_BETWEEN_MPD_UPDATE_MS) ? MAX_DELAY_BETWEEN_MPD_UPDATE_MS : sleepTime;
				sleepTime = (sleepTime < 200) ? 200 : sleepTime;
				AAMPLOG_INFO("%s:%d Next fragment Not Available yet: fragmentDescriptor.Time %f currentTimeSeconds %f sleepTime %d ", __FUNCTION__, __LINE__, pMediaStreamContext->fragmentDescriptor.Time, currentTimeSeconds, sleepTime);
				aamp->InterruptableMsSleep(sleepTime);
				retval = false;
			}
			else
			{
				if (mIsLiveStream)
				{
					pMediaStreamContext->fragmentDescriptor.Number = pMediaStreamContext->lastSegmentNumber;
				}
				retval = FetchFragment(pMediaStreamContext, media, fragmentDuration, false, curlInstance);
				if( mCheckForRampdown )
				{
					/* NOTE : This case needs to be validated with the segmentTimeline not available stream */
					return retval;

				}

				if (rate > 0)
				{
					pMediaStreamContext->fragmentDescriptor.Number++;
					pMediaStreamContext->fragmentDescriptor.Time += fragmentDuration;
				}
				else
				{
					pMediaStreamContext->fragmentDescriptor.Number--;
					pMediaStreamContext->fragmentDescriptor.Time -= fragmentDuration;
				}
				pMediaStreamContext->lastSegmentNumber = pMediaStreamContext->fragmentDescriptor.Number;
			}
		}
	}
	else
	{
		ISegmentBase *segmentBase = pMediaStreamContext->representation->GetSegmentBase();
		if (segmentBase)
		{ // single-segment
			std::string fragmentUrl;
			GetFragmentUrl(fragmentUrl, &pMediaStreamContext->fragmentDescriptor, "");
			if (!pMediaStreamContext->index_ptr)
			{ // lazily load index
				std::string range = segmentBase->GetIndexRange();
				uint64_t start;
				sscanf(range.c_str(), "%" PRIu64 "-%" PRIu64 "", &start, &pMediaStreamContext->fragmentOffset);

				ProfilerBucketType bucketType = aamp->GetProfilerBucketForMedia(pMediaStreamContext->mediaType, true);
				MediaType actualType = (MediaType)(eMEDIATYPE_INIT_VIDEO+pMediaStreamContext->mediaType);
				std::string effectiveUrl;
				long http_code;
				double downloadTime;
				int iFogError = -1;
				int iCurrentRate = aamp->rate; //  Store it as back up, As sometimes by the time File is downloaded, rate might have changed due to user initiated Trick-Play
				pMediaStreamContext->index_ptr = aamp->LoadFragment(bucketType, fragmentUrl, effectiveUrl,&pMediaStreamContext->index_len, curlInstance, range.c_str(),&http_code, &downloadTime, actualType,&iFogError);

				if (iCurrentRate != AAMP_NORMAL_PLAY_RATE)
				{
					if(actualType == eMEDIATYPE_VIDEO)
					{
						actualType = eMEDIATYPE_IFRAME;
					}
					//CID:101284 - To resolve  deadcode
					else if(actualType == eMEDIATYPE_INIT_VIDEO)
					{
						actualType = eMEDIATYPE_INIT_IFRAME;
					}
				}

				//update videoend info
				aamp->UpdateVideoEndMetrics( actualType,
										pMediaStreamContext->fragmentDescriptor.Bandwidth,
										(iFogError > 0 ? iFogError : http_code),effectiveUrl,pMediaStreamContext->fragmentDescriptor.Time, downloadTime);

				pMediaStreamContext->fragmentOffset++; // first byte following packed index

				if (pMediaStreamContext->index_ptr)
				{
					unsigned int firstOffset;
					ParseSegmentIndexBox(pMediaStreamContext->index_ptr, pMediaStreamContext->index_len, 0, NULL, NULL, &firstOffset);
					pMediaStreamContext->fragmentOffset += firstOffset;
				}

				if (pMediaStreamContext->fragmentIndex != 0 && pMediaStreamContext->index_ptr)
				{
					unsigned int referenced_size;
					float fragmentDuration;
					AAMPLOG_INFO("%s:%d current fragmentIndex = %d", __FUNCTION__, __LINE__, pMediaStreamContext->fragmentIndex);
					//Find the offset of previous fragment in new representation
					for (int i = 0; i < pMediaStreamContext->fragmentIndex; i++)
					{
						if (ParseSegmentIndexBox(pMediaStreamContext->index_ptr, pMediaStreamContext->index_len, i, &referenced_size, &fragmentDuration, NULL))
						{
							pMediaStreamContext->fragmentOffset += referenced_size;
						}
					}
				}
			}
			if (pMediaStreamContext->index_ptr)
			{
				unsigned int referenced_size;
				float fragmentDuration;
				if (ParseSegmentIndexBox(pMediaStreamContext->index_ptr, pMediaStreamContext->index_len, pMediaStreamContext->fragmentIndex++, &referenced_size, &fragmentDuration, NULL))
				{
					char range[128];
					sprintf(range, "%" PRIu64 "-%" PRIu64 "", pMediaStreamContext->fragmentOffset, pMediaStreamContext->fragmentOffset + referenced_size - 1);
					AAMPLOG_INFO("%s:%d %s [%s]", __FUNCTION__, __LINE__,getMediaTypeName(pMediaStreamContext->mediaType), range);
					if(pMediaStreamContext->CacheFragment(fragmentUrl, curlInstance, pMediaStreamContext->fragmentTime, fragmentDuration, range ))
					{
						pMediaStreamContext->fragmentTime += fragmentDuration;
						pMediaStreamContext->fragmentOffset += referenced_size;
						//pMediaStreamContext->fragmentTime = ceil(pMediaStreamContext->fragmentTime * 1000.0) / 1000.0;
						retval = true;
					}
				}
				else
				{ // done with index
					if( pMediaStreamContext->index_ptr )
					{
						g_free(pMediaStreamContext->index_ptr);
						pMediaStreamContext->index_ptr = NULL;
					}
					pMediaStreamContext->eos = true;
				}
			}
			else
			{
				pMediaStreamContext->eos = true;
			}
		}
		else
		{
			ISegmentList *segmentList = pMediaStreamContext->representation->GetSegmentList();
			if (segmentList)
			{
				const std::vector<ISegmentURL*>segmentURLs = segmentList->GetSegmentURLs();
				if (pMediaStreamContext->fragmentIndex >= segmentURLs.size() ||  pMediaStreamContext->fragmentIndex < 0)
				{
					pMediaStreamContext->eos = true;
				}
				else if(!segmentURLs.empty())
				{
					ISegmentURL *segmentURL = segmentURLs.at(pMediaStreamContext->fragmentIndex);
					if(segmentURL != NULL)
					{

						std::map<string,string> rawAttributes = segmentList->GetRawAttributes();
						if(rawAttributes.find("customlist") == rawAttributes.end()) //"CheckForFogSegmentList")
						{
							std::string fragmentUrl;
							GetFragmentUrl(fragmentUrl, &pMediaStreamContext->fragmentDescriptor,  segmentURL->GetMediaURI());
							AAMPLOG_INFO("%s [%s]", getMediaTypeName(pMediaStreamContext->mediaType), segmentURL->GetMediaRange().c_str());
							if(!pMediaStreamContext->CacheFragment(fragmentUrl, curlInstance, pMediaStreamContext->fragmentTime, 0.0, segmentURL->GetMediaRange().c_str() ))
							{
								logprintf("StreamAbstractionAAMP_MPD::%s:%d failed. fragmentUrl %s fragmentTime %f", __FUNCTION__, __LINE__, fragmentUrl.c_str(), pMediaStreamContext->fragmentTime);
							}
						}
						else //We are procesing the custom segment list provided by Fog for DASH TSB
						{
							uint32_t timeScale = segmentList->GetTimescale();
							string durationStr = segmentURL->GetRawAttributes().at("d");
							string startTimestr = segmentURL->GetRawAttributes().at("s");
							long long duration = stoll(durationStr);
							long long startTime = stoll(startTimestr);
							if(startTime > pMediaStreamContext->lastSegmentTime || 0 == pMediaStreamContext->lastSegmentTime || rate < 0 )
							{
								/*
									Added to inject appropriate initialization header in
									the case of fog custom mpd
								*/
								if(eMEDIATYPE_VIDEO == pMediaStreamContext->mediaType)
								{
									uint32_t bitrate = 0;
									std::map<string,string> rawAttributes =  segmentURL->GetRawAttributes();
									if(rawAttributes.find("bitrate") == rawAttributes.end()){
										bitrate = pMediaStreamContext->fragmentDescriptor.Bandwidth;
									}else{
										string bitrateStr = rawAttributes["bitrate"];
										bitrate = stoi(bitrateStr);
									}
									if(pMediaStreamContext->fragmentDescriptor.Bandwidth != bitrate || pMediaStreamContext->profileChanged)
									{
										pMediaStreamContext->fragmentDescriptor.Bandwidth = bitrate;
										pMediaStreamContext->profileChanged = true;
										profileIdxForBandwidthNotification = GetProfileIdxForBandwidthNotification(bitrate);
										FetchAndInjectInitialization();
										UpdateRampdownProfileReason();
										return false; //Since we need to check WaitForFreeFragmentCache
									}
								}
								double fragmentDuration = ComputeFragmentDuration(duration,timeScale);
								pMediaStreamContext->lastSegmentTime = startTime;
								retval = FetchFragment(pMediaStreamContext, segmentURL->GetMediaURI(), fragmentDuration, false, curlInstance);
								if( mCheckForRampdown )
								{
									/* This case needs to be validated with the segmentList available stream */

									return retval;
								}
							}
							else if(pMediaStreamContext->mediaType == eMEDIATYPE_VIDEO && duration > 0 && ((pMediaStreamContext->lastSegmentTime - startTime) > TIMELINE_START_RESET_DIFF))
							{
								logprintf("%s:%d START-TIME RESET in TSB period, lastSegmentTime=%" PRIu64 " start-time=%lld duration=%lld", __FUNCTION__, __LINE__, pMediaStreamContext->lastSegmentTime, startTime, duration);
								pMediaStreamContext->lastSegmentTime = startTime - 1;
								return retval;
							}
							else
							{
								int index = pMediaStreamContext->fragmentIndex + 1;
								int listSize = segmentURLs.size();

								/*Added this block to reduce the skip overhead for custom mpd after
								 *MPD refresh
								*/
								int nextIndex = ((pMediaStreamContext->lastSegmentTime - startTime) / duration) - 5;
								while(nextIndex > 0 && nextIndex < listSize)
								{
									segmentURL = segmentURLs.at(nextIndex);
									string startTimestr = segmentURL->GetRawAttributes().at("s");
									startTime = stoll(startTimestr);
									if(startTime > pMediaStreamContext->lastSegmentTime)
									{
										nextIndex -= 5;
										continue;
									}
									else
									{
										index = nextIndex;
										break;
									}
								}

								while(startTime < pMediaStreamContext->lastSegmentTime && index < listSize)
								{
									segmentURL = segmentURLs.at(index);
									string startTimestr = segmentURL->GetRawAttributes().at("s");
									startTime = stoll(startTimestr);
									index++;
								}
								pMediaStreamContext->fragmentIndex = index - 1;
								AAMPLOG_TRACE("%s:%d PushNextFragment Exit : startTime %lld lastSegmentTime %lu index = %d", __FUNCTION__, __LINE__, startTime, pMediaStreamContext->lastSegmentTime, pMediaStreamContext->fragmentIndex);
							}
						}
						if(rate > 0)
						{
							pMediaStreamContext->fragmentIndex++;
						}
						else
						{
							pMediaStreamContext->fragmentIndex--;
						}
					}
					else
					{
						AAMPLOG_WARN("%s:%d :  segmentURL    is null", __FUNCTION__, __LINE__);  //CID:82493 ,86180 - Null Returns
					}
				}
				else
				{
					logprintf("StreamAbstractionAAMP_MPD::%s:%d SegmentUrl is empty", __FUNCTION__, __LINE__);
				}
			}
			else
			{
				AAMPLOG_ERR("%s:%d not-yet-supported mpd format",__FUNCTION__,__LINE__);
			}
		}
	}
	return retval;
}


/**
 * @brief Seek current period by a given time
 * @param seekPositionSeconds seek positon in seconds
 */
void StreamAbstractionAAMP_MPD::SeekInPeriod( double seekPositionSeconds)
{
	for (int i = 0; i < mNumberOfTracks; i++)
	{
		if (eMEDIATYPE_SUBTITLE == i)
		{
			double skipTime = seekPositionSeconds;
			if (mMediaStreamContext[eMEDIATYPE_AUDIO]->fragmentTime != seekPositionSeconds)
				skipTime = mMediaStreamContext[eMEDIATYPE_AUDIO]->fragmentTime;
			SkipFragments(mMediaStreamContext[i], skipTime, true);
		}
		else
		{
			SkipFragments(mMediaStreamContext[i], seekPositionSeconds, true);
		}
	}
}



/**
 * @brief Skip to end of track
 * @param pMediaStreamContext Track object pointer
 */
void StreamAbstractionAAMP_MPD::SkipToEnd( MediaStreamContext *pMediaStreamContext)
{
	SegmentTemplates segmentTemplates(pMediaStreamContext->representation->GetSegmentTemplate(),
					pMediaStreamContext->adaptationSet->GetSegmentTemplate() );
	if( segmentTemplates.HasSegmentTemplate() )
	{
		const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
		if (segmentTimeline)
		{
			std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
			if(!timelines.empty())
			{
				uint32_t repeatCount = 0;
				for(int i = 0; i < timelines.size(); i++)
				{
					ITimeline *timeline = timelines.at(i);
					repeatCount += (timeline->GetRepeatCount() + 1);
				}
				pMediaStreamContext->fragmentDescriptor.Number = pMediaStreamContext->fragmentDescriptor.Number + repeatCount - 1;
				pMediaStreamContext->timeLineIndex = timelines.size() - 1;
				pMediaStreamContext->fragmentRepeatCount = timelines.at(pMediaStreamContext->timeLineIndex)->GetRepeatCount();
			}
			else
			{
				AAMPLOG_WARN("%s:%d :  timelines is null", __FUNCTION__, __LINE__);  //CID:82016,84031 - Null Returns
			}
		}
		else
		{
			double segmentDuration = ComputeFragmentDuration(segmentTemplates.GetDuration(), segmentTemplates.GetTimescale() );
			double startTime = mPeriodStartTime;
			int number = 0;
			while(startTime < mPeriodEndTime)
			{
				startTime += segmentDuration;
				number++;
			}
			pMediaStreamContext->fragmentDescriptor.Number = pMediaStreamContext->fragmentDescriptor.Number + number - 1;
		}
	}
	else
	{
		ISegmentList *segmentList = pMediaStreamContext->representation->GetSegmentList();
		if (segmentList)
		{
			const std::vector<ISegmentURL*> segmentURLs = segmentList->GetSegmentURLs();
			pMediaStreamContext->fragmentIndex = segmentURLs.size() - 1;
		}
		else
		{
			AAMPLOG_ERR("%s:%d not-yet-supported mpd format",__FUNCTION__,__LINE__);
		}
	}
}


/**
 * @brief Skip fragments by given time
 * @param pMediaStreamContext Media track object
 * @param skipTime time to skip in seconds
 * @param updateFirstPTS true to update first pts state variable
 * @retval
 */
double StreamAbstractionAAMP_MPD::SkipFragments( MediaStreamContext *pMediaStreamContext, double skipTime, bool updateFirstPTS)
{
	SegmentTemplates segmentTemplates(pMediaStreamContext->representation->GetSegmentTemplate(),
					pMediaStreamContext->adaptationSet->GetSegmentTemplate() );
	if( segmentTemplates.HasSegmentTemplate() )
	{
		 AAMPLOG_INFO("%s:%d Enter : Type[%d] timeLineIndex %d fragmentRepeatCount %d fragmentTime %f skipTime %f segNumber %lu", __FUNCTION__, __LINE__,pMediaStreamContext->type,
								pMediaStreamContext->timeLineIndex, pMediaStreamContext->fragmentRepeatCount, pMediaStreamContext->fragmentTime, skipTime, pMediaStreamContext->fragmentDescriptor.Number);

		gboolean firstFrag = true;

		std::string media = segmentTemplates.Getmedia();
		const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
		do
		{
			if (segmentTimeline)
			{
				uint32_t timeScale = segmentTemplates.GetTimescale();
				std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
				if (pMediaStreamContext->timeLineIndex >= timelines.size())
				{
					AAMPLOG_INFO("%s:%d Type[%d] EOS. timeLineIndex[%d] size [%lu]",__FUNCTION__,__LINE__,pMediaStreamContext->type, pMediaStreamContext->timeLineIndex, timelines.size());
					pMediaStreamContext->eos = true;
					break;
				}
				else
				{
					ITimeline *timeline = timelines.at(pMediaStreamContext->timeLineIndex);
					uint32_t repeatCount = timeline->GetRepeatCount();
					if (pMediaStreamContext->fragmentRepeatCount == 0)
					{
						map<string, string> attributeMap = timeline->GetRawAttributes();
						if(attributeMap.find("t") != attributeMap.end())
						{
							uint64_t startTime = timeline->GetStartTime();
							pMediaStreamContext->fragmentDescriptor.Time = startTime;
						}
					}
					uint32_t duration = timeline->GetDuration();
					double fragmentDuration = ComputeFragmentDuration(duration,timeScale);
					double nextPTS = (double)(pMediaStreamContext->fragmentDescriptor.Time + duration)/timeScale;
					double firstPTS = (double)pMediaStreamContext->fragmentDescriptor.Time/timeScale;
//					AAMPLOG_TRACE("%s:%d [%s] firstPTS %f nextPTS %f duration %f skipTime %f", __FUNCTION__, __LINE__, pMediaStreamContext->name, firstPTS, nextPTS, fragmentDuration, skipTime);
					if (firstFrag && updateFirstPTS){
						if (pMediaStreamContext->type == eTRACK_AUDIO && (mFirstFragPTS[eTRACK_VIDEO] || mFirstPTS || mVideoPosRemainder)){
							/* need to adjust audio skipTime/seekPosition so 1st audio fragment sent matches start of 1st video fragment being sent */
							double newSkipTime = skipTime + (mFirstFragPTS[eTRACK_VIDEO] - firstPTS); /* adjust for audio/video frag start PTS differences */
							newSkipTime -= mVideoPosRemainder;   /* adjust for mVideoPosRemainder, which is (video seekposition/skipTime - mFirstPTS) */
							newSkipTime += fragmentDuration/4.0; /* adjust for case where video start is near end of current audio fragment by adding to the audio skipTime, pushing it into the next fragment, if close(within this adjustment) */
//							AAMPLOG_INFO("%s:%d newSkiptime %f, skipTime %f  mFirstFragPTS[vid] %f  firstPTS %f  mFirstFragPTS[vid]-firstPTS %f mVideoPosRemainder %f  fragmentDuration/4.0 %f", __FUNCTION__, __LINE__, newSkipTime, skipTime, mFirstFragPTS[eTRACK_VIDEO], firstPTS, mFirstFragPTS[eTRACK_VIDEO]-firstPTS, mVideoPosRemainder,  fragmentDuration/4.0);
							skipTime = newSkipTime;
						}
						firstFrag = false;
						mFirstFragPTS[pMediaStreamContext->mediaType] = firstPTS;
					}
					if (skipTime >= fragmentDuration)
					{
						skipTime -= fragmentDuration;
						pMediaStreamContext->fragmentTime += fragmentDuration;
						pMediaStreamContext->fragmentTime = ceil(pMediaStreamContext->fragmentTime * 1000.0) / 1000.0;
						pMediaStreamContext->fragmentDescriptor.Time += duration;
						pMediaStreamContext->fragmentDescriptor.Number++;
						pMediaStreamContext->fragmentRepeatCount++;
						if( pMediaStreamContext->fragmentRepeatCount > repeatCount)
						{
							pMediaStreamContext->fragmentRepeatCount= 0;
							pMediaStreamContext->timeLineIndex++;
						}
						continue;  /* continue to next fragment */
					}
					else if (-(skipTime) >= fragmentDuration)
					{
						skipTime += fragmentDuration;
						pMediaStreamContext->fragmentTime -= fragmentDuration;
						pMediaStreamContext->fragmentTime = ceil(pMediaStreamContext->fragmentTime * 1000.0) / 1000.0;
						pMediaStreamContext->fragmentDescriptor.Time -= duration;
						pMediaStreamContext->fragmentDescriptor.Number--;
						pMediaStreamContext->fragmentRepeatCount--;
						if( pMediaStreamContext->fragmentRepeatCount < 0)
						{
							pMediaStreamContext->timeLineIndex--;
							if(pMediaStreamContext->timeLineIndex >= 0)
							{
								pMediaStreamContext->fragmentRepeatCount = timelines.at(pMediaStreamContext->timeLineIndex)->GetRepeatCount();
							}
						}

						continue;  /* continue to next fragment */
					}
					if (abs(skipTime) < fragmentDuration)
					{ // last iteration
						AAMPLOG_INFO("%s:%d [%s] firstPTS %f, nextPTS %f  skipTime %f  fragmentDuration %f ", __FUNCTION__, __LINE__, pMediaStreamContext->name, firstPTS, nextPTS, skipTime, fragmentDuration);
						if (updateFirstPTS)
						{
							/*Keep the lower PTS */
							if ( ((mFirstPTS == 0) || (firstPTS < mFirstPTS)) && (pMediaStreamContext->type == eTRACK_VIDEO))
							{
								AAMPLOG_INFO("%s:%d [%s] mFirstPTS %f -> %f ", __FUNCTION__, __LINE__, pMediaStreamContext->name, mFirstPTS, firstPTS);
								mFirstPTS = firstPTS;
								mVideoPosRemainder = skipTime;
								if(ISCONFIGSET(eAAMPConfig_MidFragmentSeek))
								{
									mFirstPTS += mVideoPosRemainder;
									if(mVideoPosRemainder > fragmentDuration/2)
									{
										if(aamp->GetInitialBufferDuration() == 0)
										{
											PrivAAMPState state;
											aamp->GetState(state);
											if(state == eSTATE_SEEKING)
											{
												// To prevent underflow when seeked to end of fragment.
												// Added +1 to ensure next fragment is fetched.
												SETCONFIGVALUE(AAMP_STREAM_SETTING,eAAMPConfig_InitialBuffer,(int)fragmentDuration + 1);
												aamp->midFragmentSeekCache = true;
											}
										}
									}
									else if(aamp->midFragmentSeekCache)
									{
										// Resetting fragment cache when seeked to first half of the fragment duration.
										SETCONFIGVALUE(AAMP_STREAM_SETTING,eAAMPConfig_InitialBuffer,0);
										aamp->midFragmentSeekCache = false;
									}

								}
								AAMPLOG_INFO("%s:%d [%s] mFirstPTS %f  mVideoPosRemainder %f", __FUNCTION__, __LINE__, pMediaStreamContext->name, mFirstPTS, mVideoPosRemainder);
							}
						}
						skipTime = 0;
						if (pMediaStreamContext->type == eTRACK_AUDIO){
//							AAMPLOG_TRACE("%s audio/video PTS offset %f  audio %f video %f", __FUNCTION__, firstPTS-mFirstPTS, firstPTS, mFirstPTS);
							if (abs(firstPTS - mFirstPTS)> 1.00){
								AAMPLOG_WARN("%s audio/video PTS offset Large %f  audio %f video %f", __FUNCTION__,  firstPTS-mFirstPTS, firstPTS, mFirstPTS);
							}
						}
						break;
					}
				}
			}
			else
			{
				if(0 == pMediaStreamContext->fragmentDescriptor.Time)
				{
					if (rate < 0)
					{
						pMediaStreamContext->fragmentDescriptor.Time = mPeriodEndTime;
					}
					else
					{
						pMediaStreamContext->fragmentDescriptor.Time = mPeriodStartTime;
					}
				}

				if(pMediaStreamContext->fragmentDescriptor.Time > mPeriodEndTime || (rate < 0 && pMediaStreamContext->fragmentDescriptor.Time <= 0))
				{
					AAMPLOG_INFO("%s:%d Type[%d] EOS. fragmentDescriptor.Time=%f",__FUNCTION__,__LINE__,pMediaStreamContext->type, pMediaStreamContext->fragmentDescriptor.Time);
					pMediaStreamContext->eos = true;
					break;
				}
				else
				{
					double segmentDuration = ComputeFragmentDuration( segmentTemplates.GetDuration(), segmentTemplates.GetTimescale() );
					if (skipTime >= segmentDuration)
					{
						uint64_t number = (skipTime / segmentDuration) + 1; // Number is 1-based index
						double fragmentTimeFromNumber = ceil((segmentDuration * (number - 1)) * 1000.0) / 1000.0;
						pMediaStreamContext->fragmentDescriptor.Number += number;
						pMediaStreamContext->fragmentTime = fragmentTimeFromNumber;
						pMediaStreamContext->fragmentDescriptor.Time = fragmentTimeFromNumber;
						pMediaStreamContext->lastSegmentNumber = pMediaStreamContext->fragmentDescriptor.Number;
						skipTime -= fragmentTimeFromNumber;
						break;
					}
					else if (-(skipTime) >= segmentDuration)
					{
						pMediaStreamContext->fragmentDescriptor.Number--;
						pMediaStreamContext->fragmentTime -= segmentDuration;
						pMediaStreamContext->fragmentTime = ceil(pMediaStreamContext->fragmentTime * 1000.0) / 1000.0;
						pMediaStreamContext->fragmentDescriptor.Time -= segmentDuration;
						pMediaStreamContext->lastSegmentNumber = pMediaStreamContext->fragmentDescriptor.Number;
						skipTime += segmentDuration;
					}
					else if(skipTime == 0)
					{
						// Linear or VOD in both the cases if offset is set to 0 then this code will execute.
						// if no offset set then there is back up code in PushNextFragment function
						// which will take care of setting fragmentDescriptor.Time
						// based on live offset(linear) or period start ( vod ) on (pMediaStreamContext->lastSegmentNumber ==0 ) condition
						pMediaStreamContext->lastSegmentNumber = pMediaStreamContext->fragmentDescriptor.Number;
						pMediaStreamContext->fragmentDescriptor.Time = mPeriodStartTime;
						break;
					}
					else if(abs(skipTime) < segmentDuration)
					{
						break;
					}
				}
			}
			if( skipTime==0 ) AAMPLOG_WARN( "XIONE-941" );
		}while(true); // was while(skipTime != 0);

		AAMPLOG_INFO("%s:%d Exit :Type[%d] timeLineIndex %d fragmentRepeatCount %d fragmentDescriptor.Number %" PRIu64 " fragmentTime %f FTime:%f", __FUNCTION__, __LINE__,pMediaStreamContext->type,
				pMediaStreamContext->timeLineIndex, pMediaStreamContext->fragmentRepeatCount, pMediaStreamContext->fragmentDescriptor.Number, pMediaStreamContext->fragmentTime,pMediaStreamContext->fragmentDescriptor.Time);
	}
	else
	{
		ISegmentBase *segmentBase = pMediaStreamContext->representation->GetSegmentBase();
		if (segmentBase)
		{ // single-segment
			std::string range = segmentBase->GetIndexRange();
			if (!pMediaStreamContext->index_ptr)
			{   // lazily load index
				std::string fragmentUrl;
				GetFragmentUrl(fragmentUrl, &pMediaStreamContext->fragmentDescriptor, "");
				ProfilerBucketType bucketType = aamp->GetProfilerBucketForMedia(pMediaStreamContext->mediaType, true);
				MediaType actualType = (MediaType)(eMEDIATYPE_INIT_VIDEO+pMediaStreamContext->mediaType);
				std::string effectiveUrl;
				long http_code;
				double downloadTime;
				int iFogError = -1;
				pMediaStreamContext->index_ptr = aamp->LoadFragment(bucketType, fragmentUrl, effectiveUrl,&pMediaStreamContext->index_len, pMediaStreamContext->mediaType, range.c_str(),&http_code, &downloadTime, actualType,&iFogError);
			}
			if (pMediaStreamContext->index_ptr)
			{
				unsigned int referenced_size = 0;
				float fragmentDuration = 0.00;
				float fragmentTime = 0.00;
				int fragmentIndex =0;
				while (fragmentTime < skipTime)
				{
					if (ParseSegmentIndexBox(pMediaStreamContext->index_ptr, pMediaStreamContext->index_len, fragmentIndex++, &referenced_size, &fragmentDuration, NULL))
					{
						fragmentTime += fragmentDuration;
						//fragmentTime = ceil(fragmentTime * 1000.0) / 1000.0;
						pMediaStreamContext->fragmentOffset += referenced_size;
					}
					else
					{
						// done with index
						if( pMediaStreamContext->index_ptr )
						{
							g_free(pMediaStreamContext->index_ptr);
							pMediaStreamContext->index_ptr = NULL;
						}
						pMediaStreamContext->eos = true;
						break;
					}
				}
				
				mFirstPTS = fragmentTime;
				//updated seeked position
				pMediaStreamContext->fragmentIndex = fragmentIndex;
				pMediaStreamContext->fragmentTime = fragmentTime;
			}
			else
			{
				pMediaStreamContext->eos = true;
			}
		}
		else
		{
		ISegmentList *segmentList = pMediaStreamContext->representation->GetSegmentList();
		if (segmentList)
		{
			AAMPLOG_INFO("%s:%d Enter : fragmentIndex %d skipTime %f", __FUNCTION__, __LINE__,
					pMediaStreamContext->fragmentIndex, skipTime);
			const std::vector<ISegmentURL*> segmentURLs = segmentList->GetSegmentURLs();
			double segmentDuration = 0;
			if(!segmentURLs.empty())
			{
				std::map<string,string> rawAttributes = segmentList->GetRawAttributes();
				uint32_t timescale = segmentList->GetTimescale();
				bool isFogTsb = !(rawAttributes.find("customlist") == rawAttributes.end());
				if(!isFogTsb)
				{
					segmentDuration = ComputeFragmentDuration( segmentList->GetDuration() , timescale);
				}
				else if(pMediaStreamContext->type == eTRACK_AUDIO)
				{
					MediaStreamContext *videoContext = mMediaStreamContext[eMEDIATYPE_VIDEO];
					if(videoContext != NULL)
					{
						const std::vector<ISegmentURL*> vidSegmentURLs = videoContext->representation->GetSegmentList()->GetSegmentURLs();
						if(!vidSegmentURLs.empty())
						{
							string videoStartStr = vidSegmentURLs.at(0)->GetRawAttributes().at("s");
							string audioStartStr = segmentURLs.at(0)->GetRawAttributes().at("s");
							long long videoStart = stoll(videoStartStr);
							long long audioStart = stoll(audioStartStr);
							long long diff = audioStart - videoStart;
							logprintf("Printing diff value for adjusting %lld",diff);
							if(diff > 0)
							{
								double diffSeconds = double(diff) / timescale;
								skipTime -= diffSeconds;
							}
						}
						else
						{
							logprintf("StreamAbstractionAAMP_MPD::%s:%d Video SegmentUrl is empty", __FUNCTION__, __LINE__);
						}
					}
					else
					{
						AAMPLOG_WARN("%s:%d :  videoContext  is null", __FUNCTION__, __LINE__);  //CID:82361 - Null Returns
					}
				}

				while (skipTime != 0)
				{

					if ((pMediaStreamContext->fragmentIndex >= segmentURLs.size()) || (pMediaStreamContext->fragmentIndex < 0))
					{
						pMediaStreamContext->eos = true;
						break;
					}
					else
					{
						//Calculate the individual segment duration for fog tsb
						if(isFogTsb)
						{
							ISegmentURL* segmentURL = segmentURLs.at(pMediaStreamContext->fragmentIndex);
							string durationStr = segmentURL->GetRawAttributes().at("d");
							long long duration = stoll(durationStr);
							segmentDuration = ComputeFragmentDuration(duration,timescale);
						}
						if (skipTime >= segmentDuration)
						{
							pMediaStreamContext->fragmentIndex++;
							skipTime -= segmentDuration;
							pMediaStreamContext->fragmentTime += segmentDuration;
							pMediaStreamContext->fragmentTime = ceil(pMediaStreamContext->fragmentTime * 1000.0) / 1000.0;
						}
						else if (-(skipTime) >= segmentDuration)
						{
							pMediaStreamContext->fragmentIndex--;
							skipTime += segmentDuration;
							pMediaStreamContext->fragmentTime -= segmentDuration;
							pMediaStreamContext->fragmentTime = ceil(pMediaStreamContext->fragmentTime * 1000.0) / 1000.0;
						}
						else
						{
							skipTime = 0;
							break;
						}
					}
				}
			}
			else
			{
				logprintf("StreamAbstractionAAMP_MPD::%s:%d SegmentUrl is empty", __FUNCTION__, __LINE__);
			}

			AAMPLOG_INFO("%s:%d Exit : fragmentIndex %d segmentDuration %f", __FUNCTION__, __LINE__,
					pMediaStreamContext->fragmentIndex, segmentDuration);
		}
		else
		{
			AAMPLOG_ERR("%s:%d not-yet-supported mpd format",__FUNCTION__,__LINE__);
		}
		}
	}
	return skipTime;
}


/**
 * @brief Add attriblutes to xml node
 * @param reader xmlTextReaderPtr
 * @param node xml Node
 */
static void AddAttributesToNode(xmlTextReaderPtr *reader, Node *node)
{
	if (xmlTextReaderHasAttributes(*reader))
	{
		while (xmlTextReaderMoveToNextAttribute(*reader))
		{
			std::string key = (const char *)xmlTextReaderConstName(*reader);
			if(!key.empty())
			{
				std::string value = (const char *)xmlTextReaderConstValue(*reader);
				node->AddAttribute(key, value);
			}
			else
			{
				AAMPLOG_WARN("%s:%d :  key   is null", __FUNCTION__, __LINE__);  //CID:85916 - Null Returns
			}
		}
	}
}



/**
 * @brief Get mpd object of manifest
 * @param manifest buffer pointer
 * @param mpd MPD object of manifest
 * @param manifestUrl manifest url
 * @param init true if this is the first playlist download for a tune/seek/trickplay
 * @retval AAMPStatusType indicates if success or fail
*/
AAMPStatusType StreamAbstractionAAMP_MPD::GetMpdFromManfiest(const GrowableBuffer &manifest, MPD * &mpd, std::string manifestUrl, bool init)
{
	AAMPStatusType ret = eAAMPSTATUS_GENERIC_ERROR;
	xmlTextReaderPtr reader = xmlReaderForMemory(manifest.ptr, (int) manifest.len, NULL, NULL, 0);
	if (reader != NULL)
	{
		if (xmlTextReaderRead(reader))
		{
			Node *root = aamp_ProcessNode(&reader, manifestUrl);
			if(root != NULL)
			{
				uint32_t fetchTime = Time::GetCurrentUTCTimeInSec();
				mpd = root->ToMPD();
				if (mpd)
				{
					mpd->SetFetchTime(fetchTime);
#if 1
					bool bMetadata = ISCONFIGSET(eAAMPConfig_BulkTimedMetaReport);
					mIsLiveManifest = !(mpd->GetType() == "static");
					aamp->SetIsLive(mIsLiveManifest);
					FindTimedMetadata(mpd, root, init, bMetadata);
					if(!init)
					{
						aamp->ReportTimedMetadata(false);
					}
					ret = AAMPStatusType::eAAMPSTATUS_OK;
#else
					size_t prevPrdCnt = mCdaiObject->mAdBreaks.size();
					FindTimedMetadata(mpd, root, init);
					size_t newPrdCnt = mCdaiObject->mAdBreaks.size();
					if(prevPrdCnt < newPrdCnt)
					{
						static int myctr = 0;
						std::string filename = "/tmp/manifest.mpd_" + std::to_string(myctr++);
						WriteFile(filename.c_str(),manifest.ptr, manifest.len);
					}
#endif
				}
				else
				{
					ret = AAMPStatusType::eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
				}
				delete root;
			}
			else if (root == NULL)
			{
				ret = AAMPStatusType::eAAMPSTATUS_MANIFEST_PARSE_ERROR;
			}
		}
		else if (xmlTextReaderRead(reader) == -1)
		{
			ret = AAMPStatusType::eAAMPSTATUS_MANIFEST_PARSE_ERROR;
		}
		xmlFreeTextReader(reader);
	}
	
	return ret;
}

/**
 * @brief Get xml node form reader
 *
 * @param[in] reader Pointer to reader object
 * @param[in] url    manifest url
 *
 * @retval xml node
 */
Node* aamp_ProcessNode(xmlTextReaderPtr *reader, std::string url, bool isAd)
{
	int type = xmlTextReaderNodeType(*reader);

	if (type != WhiteSpace && type != Text)
	{
		while (type == Comment || type == WhiteSpace)
		{
			xmlTextReaderRead(*reader);
			type = xmlTextReaderNodeType(*reader);
		}

		Node *node = new Node();
		node->SetType(type);
		node->SetMPDPath(Path::GetDirectoryPath(url));

		const char *name = (const char *)xmlTextReaderConstName(*reader);
		if (name == NULL)
		{
			delete node;
			return NULL;
		}

		int         isEmpty = xmlTextReaderIsEmptyElement(*reader);

		node->SetName(name);

		AddAttributesToNode(reader, node);

		if(isAd && !strcmp("Period", name))
		{
			//Making period ids unique. It needs for playing same ad back to back.
			static int UNIQ_PID = 0;
			std::string periodId = std::to_string(UNIQ_PID++) + "-";
			if(node->HasAttribute("id"))
			{
				periodId += node->GetAttributeValue("id");
			}
			node->AddAttribute("id", periodId);
		}

		if (isEmpty)
			return node;

		Node    *subnode = NULL;
		int     ret = xmlTextReaderRead(*reader);

		while (ret == 1)
		{
			if (!strcmp(name, (const char *)xmlTextReaderConstName(*reader)))
			{
				return node;
			}

			subnode = aamp_ProcessNode(reader, url, isAd);

			if (subnode != NULL)
				node->AddSubNode(subnode);

			ret = xmlTextReaderRead(*reader);
		}

		return node;
	}
	else if (type == Text)
	{
		xmlChar * text = xmlTextReaderReadString(*reader);

		if (text != NULL)
		{
			Node *node = new Node();
			node->SetType(type);
			node->SetText((const char*)text);
			xmlFree(text);
			return node;
		}
	}
	return NULL;
}

//Multiply two ints without overflow
inline std::uint64_t safeMultiply(const int first, const int second)
{
    return static_cast<std::uint64_t>(first) * second;
}
/**
 * @brief Parse duration from ISO8601 string
 * @param ptr ISO8601 string
 * @param[out] durationMs duration in milliseconds
 */
static double ParseISO8601Duration(const char *ptr)
{
	int years = 0;
	int months = 0;
	int days = 0;
	int hour = 0;
	int minute = 0;
	double seconds = 0.0;
	uint64_t returnValue = 0;
	int indexforM = 0,indexforT=0;

	//ISO 8601 does not specify specific values for months in a day
	//or days in a year, so use 30 days/month and 365 days/year
	static constexpr auto kMonthDays = 30;
	static constexpr auto kYearDays = 365;
	static constexpr auto kMinuteSecs = 60;
	static constexpr auto kHourSecs = kMinuteSecs * 60;
	static constexpr auto kDaySecs = kHourSecs * 24;
	static constexpr auto kMonthSecs = kMonthDays * kDaySecs;
	static constexpr auto kYearSecs = kDaySecs * kYearDays;
	
	// ISO 8601 allow for number of years(Y), months(M), days(D) before the "T" 
	// and hours(H), minutes(M), and seconds after the "T"
	
	const char* durationPtr = strchr(ptr, 'T');
	indexforT = (int)(durationPtr - ptr);
	const char* pMptr = strchr(ptr, 'M');
	if(NULL != pMptr)
	{
		indexforM = (int)(pMptr - ptr);
	}
	
	if (ptr[0] == 'P')
	{
		ptr++;
		if (ptr != durationPtr)
		{
			const char *temp = strchr(ptr, 'Y');
			if (temp)
			{	sscanf(ptr, "%dY", &years);
				logprintf("%s:%d : years %d", __FUNCTION__, __LINE__, years);
				ptr = temp + 1;
			}
			temp = strchr(ptr, 'M');
			if (temp && ( NULL != pMptr && indexforM < indexforT ) )
			{
				sscanf(ptr, "%dM", &months);
				ptr = temp + 1;
			}
			temp = strchr(ptr, 'D');
			if (temp)
			{
				sscanf(ptr, "%dD", &days);
				ptr = temp + 1;
			}
		}
		if (ptr == durationPtr)
		{
			ptr++;
			const char* temp = strchr(ptr, 'H');
			if (temp)
			{
				sscanf(ptr, "%dH", &hour);
				ptr = temp + 1;
			}
			temp = strchr(ptr, 'M');
			if (temp && ( NULL != pMptr && indexforM > indexforT ) )
			{
				sscanf(ptr, "%dM", &minute);
				ptr = temp + 1;
			}
			temp = strchr(ptr, 'S');
			if (temp)
			{
				sscanf(ptr, "%lfS", &seconds);
				ptr = temp + 1;
			}
		}
	}
	else
	{
		logprintf("%s:%d - Invalid input %s", __FUNCTION__, __LINE__, ptr);
	}
	
	returnValue += seconds;
	
	//Guard against overflow by casting first term
	returnValue += safeMultiply(kMinuteSecs, minute);
	returnValue += safeMultiply(kHourSecs, hour);
	returnValue += safeMultiply(kDaySecs, days);
	returnValue += safeMultiply(kMonthSecs, months);
	returnValue += safeMultiply(kYearSecs, years);
	
	return returnValue * 1000;
}


/**
 * @brief Parse XML NS
 * @param fullName full name of node
 * @param[out] ns namespace
 * @param[out] name name after :
 */
static void ParseXmlNS(const std::string& fullName, std::string& ns, std::string& name)
{
	size_t found = fullName.find(':');
	if (found != std::string::npos)
	{
		ns = fullName.substr(0, found);
		name = fullName.substr(found+1);
	}
	else
	{
		ns = "";
		name = fullName;
	}
}

#ifdef AAMP_MPD_DRM

extern void *CreateDRMSession(void *arg);


/**
 * @brief Get the DRM preference value.
 * @param The UUID for the DRM type.
 * @return The preference level for the DRM type.
 */
int StreamAbstractionAAMP_MPD::GetDrmPrefs(const std::string& uuid)
{
	auto iter = mDrmPrefs.find(uuid);

	if (iter != mDrmPrefs.end())
	{
		return iter->second;
	}

	return 0; // Unknown DRM
}

/**
 * @brief Get the UUID of preferred DRM.
 * @param None
 * @return The UUID of preferred DRM
 */
std::string StreamAbstractionAAMP_MPD::GetPreferredDrmUUID()
{
	int selectedPref = 0;
	std::string selectedUuid = "";
	for (auto iter : mDrmPrefs)
	{
		if( iter.second > selectedPref){
			selectedPref = iter.second;
			selectedUuid = iter.first;
		}
	}
	return selectedUuid; // return uuid of preferred DRM
}

/**
 * @brief Create DRM helper from ContentProtection
 * @param adaptationSet Adaptation set object
 * @param mediaType type of track
 * @retval shared_ptr of AampDrmHelper
 */
std::shared_ptr<AampDrmHelper> StreamAbstractionAAMP_MPD::CreateDrmHelper(IAdaptationSet * adaptationSet,MediaType mediaType)
{
	const vector<IDescriptor*> contentProt = adaptationSet->GetContentProtection();
	unsigned char* data = NULL;
	unsigned char *outData = NULL;
	size_t outDataLen  = 0;
	size_t dataLength = 0;
	std::shared_ptr<AampDrmHelper> tmpDrmHelper;
	std::shared_ptr<AampDrmHelper> drmHelper = nullptr;
	DrmInfo drmInfo;
	std::string contentMetadata;
	std::string cencDefaultData;
	bool forceSelectDRM = false; 
	const char *pMp4Protection = "mpeg:dash:mp4protection";

	AAMPLOG_TRACE("%s:%d [HHH] contentProt.size= %d", __FUNCTION__, __LINE__, contentProt.size());
	for (unsigned iContentProt = 0; iContentProt < contentProt.size(); iContentProt++)
	{
		// extract the UUID
		std::string schemeIdUri = contentProt.at(iContentProt)->GetSchemeIdUri();
		if(schemeIdUri.find(pMp4Protection) != std::string::npos )
		{
			std::string Value = contentProt.at(iContentProt)->GetValue();
			//ToDo check the value key and use the same along with custom attribute such as default_KID
			auto attributesMap = contentProt.at(iContentProt)->GetRawAttributes();
			if(attributesMap.find("cenc:default_KID") != attributesMap.end())
			{
				cencDefaultData=attributesMap["cenc:default_KID"];
				AAMPLOG_INFO("%s:%d cencDefaultData= %s", __FUNCTION__, __LINE__, cencDefaultData.c_str());
			}
		}
	
		// Look for UUID in schemeIdUri by matching any UUID to maintian backwards compatibility
		std::regex rgx(".*([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[1-5][0-9a-fA-F]{3}-[89abAB][0-9a-fA-F]{3}-[0-9a-fA-F]{12}).*");
		std::smatch uuid;
		if (!std::regex_search(schemeIdUri, uuid, rgx))
		{
			AAMPLOG_WARN("%s:%d (%s) got schemeID empty at ContentProtection node-%d", __FUNCTION__, __LINE__, getMediaTypeName(mediaType), iContentProt);
			continue;
		}

		drmInfo.method = eMETHOD_AES_128;
		drmInfo.mediaFormat = eMEDIAFORMAT_DASH;
		drmInfo.systemUUID = uuid[1];
		drmInfo.bPropagateUriParams = ISCONFIGSET(eAAMPConfig_PropogateURIParam);
		//Convert UUID to all lowercase
		std::transform(drmInfo.systemUUID.begin(), drmInfo.systemUUID.end(), drmInfo.systemUUID.begin(), [](unsigned char ch){ return std::tolower(ch); });

		// Extract the PSSH data
		const vector<INode*> node = contentProt.at(iContentProt)->GetAdditionalSubNodes();
		if (!node.empty())
		{
			for(int i=0; i < node.size(); i++)
			{
				std::string tagName = node.at(i)->GetName();
				if (tagName.find("pssh") != std::string::npos)
				{
					string psshData = node.at(i)->GetText();
					data = base64_Decode(psshData.c_str(), &dataLength);
					if (0 == dataLength)
					{
						AAMPLOG_WARN("%s:%d base64_Decode of pssh resulted in 0 length", __FUNCTION__, __LINE__);
						if (data)
						{
							free(data);
							data = NULL;
						}
					}
				}
			}
		}

		// A special PSSH is used to signal data to append to the widevine challenge request
		if (drmInfo.systemUUID == CONSEC_AGNOSTIC_UUID)
		{
			contentMetadata = aamp_ExtractWVContentMetadataFromPssh((const char*)data, dataLength);
			if (data)
			{
				free(data);
				data = NULL;
			}
			continue;
		}

		if (aamp->mIsWVKIDWorkaround && drmInfo.systemUUID == CLEARKEY_UUID ){
			/* WideVine KeyID workaround present , UUID change from clear key to widevine **/
			AAMPLOG_INFO("%s:%d WideVine KeyID workaround present, processing KeyID from clear key to WV Data", __FUNCTION__, __LINE__);
			drmInfo.systemUUID = WIDEVINE_UUID;
			forceSelectDRM = true; /** Need this variable to understand widevine has choosen **/
			outData = aamp->ReplaceKeyIDPsshData(data, dataLength, outDataLen );
			if (outData){
				if(data) free(data);
				data = 	outData;
				dataLength = outDataLen;
			}
		}
	
		// Try and create a DRM helper
		if (!AampDrmHelperEngine::getInstance().hasDRM(drmInfo))
		{
			AAMPLOG_WARN("%s:%d (%s) Failed to locate DRM helper for UUID %s", __FUNCTION__, __LINE__, getMediaTypeName(mediaType), drmInfo.systemUUID.c_str());
			/** Preferred DRM configured and it is failed hhen exit here */
			if(aamp->isPreferredDRMConfigured && (GetPreferredDrmUUID() == drmInfo.systemUUID) && !aamp->mIsWVKIDWorkaround){
				AAMPLOG_ERR("%s:%d (%s) Preffered DRM Failed to locate with UUID %s", __FUNCTION__, __LINE__, getMediaTypeName(mediaType), drmInfo.systemUUID.c_str());
				break;
			}
		}
		else if (data && dataLength)
		{
			tmpDrmHelper = AampDrmHelperEngine::getInstance().createHelper(drmInfo);

			if (!tmpDrmHelper->parsePssh(data, dataLength))
			{
				AAMPLOG_WARN("%s:%d (%s) Failed to Parse PSSH from the DRM InitData", __FUNCTION__, __LINE__, getMediaTypeName(mediaType));
			}
			else
			{
				if (forceSelectDRM){
					AAMPLOG_INFO("%s:%d (%s) If Widevine DRM Selected due to Widevine KeyID workaround",
						__FUNCTION__, __LINE__, getMediaTypeName(mediaType));
					drmHelper = tmpDrmHelper;
					forceSelectDRM = false; /* reset flag */
					/** No need to progress further**/
					free(data);
					data = NULL;
					break;
				}

				// Track the best DRM available to use
				else if ((!drmHelper) || (GetDrmPrefs(drmInfo.systemUUID) > GetDrmPrefs(drmHelper->getUuid())))
				{
					logprintf("%s:%d (%s) Created DRM helper for UUID %s and best to use", __FUNCTION__, __LINE__, getMediaTypeName(mediaType), drmInfo.systemUUID.c_str());
					drmHelper = tmpDrmHelper;
				}
			}
		}
		else
		{
			AAMPLOG_WARN("%s:%d (%s) No PSSH data available from the stream for UUID %s", __FUNCTION__, __LINE__, getMediaTypeName(mediaType), drmInfo.systemUUID.c_str());
			/** Preferred DRM configured and it is failed then exit here */
			if(aamp->isPreferredDRMConfigured && (GetPreferredDrmUUID() == drmInfo.systemUUID)&& !aamp->mIsWVKIDWorkaround){
				AAMPLOG_ERR("%s:%d (%s) No PSSH data available for Preffered DRM with UUID  %s", __FUNCTION__, __LINE__, getMediaTypeName(mediaType), drmInfo.systemUUID.c_str());
				break;
			}
		}

		if (data)
		{
			free(data);
			data = NULL;
		}
	}

	if(drmHelper)
	{
		drmHelper->setDrmMetaData(contentMetadata);
		drmHelper->setDefaultKeyID(cencDefaultData);
	}

	return drmHelper;
}

/**
 * @brief Process content protection of vss EAP
 * @param drmHelper created
 * @param mediaType type of track
 */
void StreamAbstractionAAMP_MPD::ProcessVssContentProtection(std::shared_ptr<AampDrmHelper> drmHelper, MediaType mediaType)
{
	if((drmHelper) && (!drmHelper->compare(mLastDrmHelper)))
	{
		hasDrm = true;
		aamp->licenceFromManifest = true;
		DrmSessionParams* sessionParams = new DrmSessionParams;
		if(sessionParams != NULL)
		{
			sessionParams->aamp = aamp;
			sessionParams->drmHelper = drmHelper;
			sessionParams->stream_type = mediaType;

			if(drmSessionThreadStarted) //In the case of license rotation
			{
				void *value_ptr = NULL;
				int rc = pthread_join(createDRMSessionThreadID, &value_ptr);
				if (rc != 0)
				{
					AAMPLOG_ERR("%s:%d (%s) pthread_join returned %d for createDRMSession Thread", __FUNCTION__, __LINE__, getMediaTypeName(mediaType), rc);
				}
				drmSessionThreadStarted = false;
			}
			/*
			*
			* Memory allocated for data via base64_Decode() and memory for sessionParams
			* is released in CreateDRMSession.
			*/
			if(0 == pthread_create(&createDRMSessionThreadID,NULL,CreateDRMSession,sessionParams))
			{
				AAMPLOG_INFO("%s:%d Thread created",__FUNCTION__, __LINE__);
				drmSessionThreadStarted = true;
				mLastDrmHelper = drmHelper;
				aamp->setCurrentDrm(drmHelper);
			}
			else
			{
				AAMPLOG_ERR("%s:%d (%s) pthread_create failed for CreateDRMSession : error code %d, %s", __FUNCTION__, __LINE__, getMediaTypeName(mediaType), errno, strerror(errno));
			}
		}
		else
		{
			AAMPLOG_WARN("%s:%d :  sessionParams  is null", __FUNCTION__, __LINE__);  //CID:85612 - Null Returns
		}
	}
	else
	{
		AAMPLOG_WARN("%s:%d (%s) Skipping creation of session for duplicate helper", __FUNCTION__, __LINE__, getMediaTypeName(mediaType));
	}
}


/**
 * @brief Process content protection of adaptation
 * @param adaptationSet Adaptation set object
 * @param mediaType type of track
 */
void StreamAbstractionAAMP_MPD::ProcessContentProtection(IAdaptationSet * adaptationSet, MediaType mediaType, std::shared_ptr<AampDrmHelper> drmHelper)
{
	if(nullptr == drmHelper)
	{
		drmHelper = CreateDrmHelper(adaptationSet, mediaType);
	}

	if((drmHelper) && (!drmHelper->compare(mLastDrmHelper)))
	{
		hasDrm = true;
		aamp->licenceFromManifest = true;
		DrmSessionParams* sessionParams = new DrmSessionParams;
		sessionParams->aamp = aamp;
		sessionParams->drmHelper = drmHelper;
		sessionParams->stream_type = mediaType;

		if(drmSessionThreadStarted) //In the case of license rotation
		{
			void *value_ptr = NULL;
			int rc = pthread_join(createDRMSessionThreadID, &value_ptr);
			if (rc != 0)
			{
				AAMPLOG_ERR("%s:%d (%s) pthread_join returned %d for createDRMSession Thread", __FUNCTION__, __LINE__, getMediaTypeName(mediaType), rc);
			}
			drmSessionThreadStarted = false;
		}
		/*
		*
		* Memory allocated for data via base64_Decode() and memory for sessionParams
		* is released in CreateDRMSession.
		*/
		if(0 == pthread_create(&createDRMSessionThreadID,NULL,CreateDRMSession,sessionParams))
		{
			drmSessionThreadStarted = true;
			mLastDrmHelper = drmHelper;
			aamp->setCurrentDrm(drmHelper);
		}
		else
		{
			AAMPLOG_ERR("%s:%d (%s) pthread_create failed for CreateDRMSession : error code %d, %s", __FUNCTION__, __LINE__, getMediaTypeName(mediaType), errno, strerror(errno));
		}
	}
	else if (!drmHelper)
	{
		AAMPLOG_ERR("%s:%d (%s) Failed to create DRM helper", __FUNCTION__, __LINE__, getMediaTypeName(mediaType));
	}
	else
	{
		AAMPLOG_WARN("%s:%d (%s) Skipping creation of session for duplicate helper", __FUNCTION__, __LINE__, getMediaTypeName(mediaType));
	}
}

#else

/**
 * @brief
 * @param adaptationSet
 * @param mediaType
 */
void StreamAbstractionAAMP_MPD::ProcessContentProtection(IAdaptationSet * adaptationSet,MediaType mediaType, std::shared_ptr<AampDrmHelper> drmHelper)
{
	logprintf("MPD DRM not enabled");
}
#endif


/**
 *   @brief  GetFirstSegment start time from period
 *   @param  period
 *   @retval start time
 */
uint64_t GetFirstSegmentStartTime(IPeriod * period)
{
	uint64_t startTime = 0;
	const std::vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();

	const ISegmentTemplate *representation = NULL;
	const ISegmentTemplate *adaptationSet = NULL;
	if( adaptationSets.size() > 0 )
	{
		IAdaptationSet * firstAdaptation = adaptationSets.at(0);
		if(firstAdaptation != NULL)
		{
			adaptationSet = firstAdaptation->GetSegmentTemplate();
			const std::vector<IRepresentation *> representations = firstAdaptation->GetRepresentation();
			if (representations.size() > 0)
			{
				representation = representations.at(0)->GetSegmentTemplate();
			}
		}
	}
	SegmentTemplates segmentTemplates(representation,adaptationSet);
	
	if( segmentTemplates.HasSegmentTemplate() )
	{
		const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
		if (segmentTimeline)
		{
			std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
			if(timelines.size() > 0)
			{
				startTime = timelines.at(0)->GetStartTime();
			}
		}
	}
	return startTime;
}

/**
 *   @brief  GetPeriod Segment timescale from period
 *   @param  period
 *   @retval timescale
 */
uint32_t GetPeriodSegmentTimeScale(IPeriod * period)
{
	uint64_t timeScale = 0;
	const std::vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();

	const ISegmentTemplate *representation = NULL;
	const ISegmentTemplate *adaptationSet = NULL;
	if( adaptationSets.size() > 0 )
	{
		IAdaptationSet * firstAdaptation = adaptationSets.at(0);
		if(firstAdaptation != NULL)
		{
			adaptationSet = firstAdaptation->GetSegmentTemplate();
			const std::vector<IRepresentation *> representations = firstAdaptation->GetRepresentation();
			if (representations.size() > 0)
			{
				representation = representations.at(0)->GetSegmentTemplate();
			}
		}
	}
	SegmentTemplates segmentTemplates(representation,adaptationSet);

	if( segmentTemplates.HasSegmentTemplate() )
	{
		timeScale = segmentTemplates.GetTimescale();
	}
	return timeScale;
}

uint64_t aamp_GetPeriodNewContentDuration(IPeriod * period, uint64_t &curEndNumber)
{
	uint64_t durationMs = 0;

	const std::vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();
	const ISegmentTemplate *representation = NULL;
	const ISegmentTemplate *adaptationSet = NULL;
	if( adaptationSets.size() > 0 )
	{
		IAdaptationSet * firstAdaptation = adaptationSets.at(0);
		if(firstAdaptation != NULL)
		{
			adaptationSet = firstAdaptation->GetSegmentTemplate();
			const std::vector<IRepresentation *> representations = firstAdaptation->GetRepresentation();
			if (representations.size() > 0)
			{
				representation = representations.at(0)->GetSegmentTemplate();
			}
		}
		
		SegmentTemplates segmentTemplates(representation,adaptationSet);

		if (segmentTemplates.HasSegmentTemplate())
		{
			const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
			if (segmentTimeline)
			{
				uint32_t timeScale = segmentTemplates.GetTimescale();
				std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
				uint64_t startNumber = segmentTemplates.GetStartNumber();
				int timeLineIndex = 0;
				while (timeLineIndex < timelines.size())
				{
					ITimeline *timeline = timelines.at(timeLineIndex);
					uint32_t segmentCount = timeline->GetRepeatCount() + 1;
					uint32_t timelineDurationMs = ComputeFragmentDuration(timeline->GetDuration(),timeScale) * 1000;
					for(int i=0;i<segmentCount;i++)
					{
						if(startNumber > curEndNumber)
						{
							durationMs += timelineDurationMs;
							curEndNumber = startNumber;
						}
						startNumber++;
					}
					timeLineIndex++;
				}
			}
		}
	}
	return durationMs;
}


/**
 *   @brief  Get difference between first segment start time and presentation offset from period
 *   @param  period
 *   @retval start time delta in seconds
 */
double aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset(IPeriod * period)
{
	double duration = 0;

	const std::vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();
	const ISegmentTemplate *representation = NULL;
	const ISegmentTemplate *adaptationSet = NULL;
	if( adaptationSets.size() > 0 )
	{
		IAdaptationSet * firstAdaptation = adaptationSets.at(0);
		if(firstAdaptation != NULL)
		{
			adaptationSet = firstAdaptation->GetSegmentTemplate();
			const std::vector<IRepresentation *> representations = firstAdaptation->GetRepresentation();
			if (representations.size() > 0)
			{
				representation = representations.at(0)->GetSegmentTemplate();
			}
		}

		SegmentTemplates segmentTemplates(representation,adaptationSet);

		if (segmentTemplates.HasSegmentTemplate())
		{
			const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
			if (segmentTimeline)
			{
				uint32_t timeScale = segmentTemplates.GetTimescale();
				uint64_t presentationTimeOffset = segmentTemplates.GetPresentationTimeOffset();
				AAMPLOG_TRACE("%s tscale: %" PRIu32 " offset : %" PRIu64 "", __FUNCTION__, timeScale, presentationTimeOffset);
				std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
				if(timelines.size() > 0)
				{
					ITimeline *timeline = timelines.at(0);
					uint64_t deltaBwFirstSegmentAndOffset = 0;
					if(timeline != NULL)
					{
						uint64_t timelineStart = timeline->GetStartTime();
						if(timelineStart > presentationTimeOffset)
						{
							deltaBwFirstSegmentAndOffset = timelineStart - presentationTimeOffset;
						}
						duration = (double) deltaBwFirstSegmentAndOffset / timeScale;
						AAMPLOG_TRACE("%s timeline start : %" PRIu64 " offset delta : %lf", __FUNCTION__, timelineStart,duration);
					}
				}
			}
		}
	}
	return duration;
}

/**
 * @brief Parse CC streamID and language from value
 *
 * @param[in] input - input value
 * @param[out] id - stream ID value
 * @param[out] lang - language value
 * @return void
 */
void ParseCCStreamIDAndLang(std::string input, std::string &id, std::string &lang)
{
	// Expected formats : 	eng;deu
	// 			CC1=eng;CC2=deu
	//			1=lang:eng;2=lang:deu
	//			1=lang:eng;2=lang:eng,war:1,er:1
		size_t delim = input.find('=');
		if (delim != std::string::npos)
		{
				id = input.substr(0, delim);
				lang = input.substr(delim + 1);

		//Parse for additional fields
				delim = lang.find(':');
				if (delim != std::string::npos)
				{
						size_t count = lang.find(',');
						if (count != std::string::npos)
						{
								count = (count - delim - 1);
						}
						lang = lang.substr(delim + 1, count);
				}
		}
		else
		{
				lang = input;
		}
}

/**
 * @brief Get start time of current period
 * @param mpd : pointer manifest
 * @param periodIndex
 * @retval current period's start time
 */
double StreamAbstractionAAMP_MPD::GetPeriodStartTime(IMPD *mpd, int periodIndex)
{
	double periodStart = 0;
	double  periodStartMs = 0;
	if(mpd != NULL)
	{
		int periodCnt= mpd->GetPeriods().size();
		if(periodIndex < periodCnt)
		{
			string startTimeStr = mpd->GetPeriods().at(periodIndex)->GetStart();
			if(!startTimeStr.empty())
			{
				periodStartMs = ParseISO8601Duration(startTimeStr.c_str()) + (aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset(mpd->GetPeriods().at(periodIndex)) * 1000);
				periodStart =  mAvailabilityStartTime + (periodStartMs / 1000);
				AAMPLOG_INFO("StreamAbstractionAAMP_MPD::%s:%d - MPD periodIndex %d AvailStartTime %f periodStart %f %s", __FUNCTION__, __LINE__, periodIndex, mAvailabilityStartTime, periodStart,startTimeStr.c_str());
			}
			else
			{
				double durationTotal = 0;
				for(int idx=0;idx < periodIndex; idx++)
				{
					string durationStr = mpd->GetPeriods().at(idx)->GetDuration();
					uint64_t  periodDurationMs = 0;
					if(!durationStr.empty())
					{
						periodDurationMs = ParseISO8601Duration(durationStr.c_str());
						durationTotal += periodDurationMs;
					}
					else
					{
						durationTotal += aamp_GetPeriodDuration(mpd, periodIndex, mLastPlaylistDownloadTimeMs);
					}
				}
				periodStart =  mAvailabilityStartTime + ((double)durationTotal / (double)1000);
				AAMPLOG_INFO("StreamAbstractionAAMP_MPD::%s:%d - MPD periodIndex %d periodStart %f", __FUNCTION__, __LINE__, periodIndex, periodStart);
			}
		}
	}
	else
	{
		AAMPLOG_WARN("%s:%d :  mpd is null", __FUNCTION__, __LINE__);  //CID:83436 Null Returns
	}
	return periodStart;
}


/**
 * @brief Get duration of current period
 * @param mpd : pointer manifest
 * @param periodIndex
 * @retval current period's duration
 */
double StreamAbstractionAAMP_MPD::GetPeriodDuration(IMPD *mpd, int periodIndex)
{
	double periodDuration = 0;
	uint64_t  periodDurationMs = 0;
	if(mpd != NULL)
	{
		int periodCnt= mpd->GetPeriods().size();
		if(periodIndex < periodCnt)
		{
			string durationStr = mpd->GetPeriods().at(periodIndex)->GetDuration();
			if(!durationStr.empty())
			{
				periodDurationMs = ParseISO8601Duration(durationStr.c_str());
				periodDuration = ((double)periodDurationMs / (double)1000);
				AAMPLOG_INFO("StreamAbstractionAAMP_MPD::%s:%d - MPD periodIndex:%d periodDuration %f", __FUNCTION__, __LINE__, periodIndex, periodDuration);
			}
			else
			{
				if(periodCnt == 1 && periodIndex == 0)
				{
					std::string durationStr =  mpd->GetMediaPresentationDuration();
					if(!durationStr.empty())
					{
						periodDurationMs = ParseISO8601Duration( durationStr.c_str());
					}
					else
					{
						periodDurationMs = aamp_GetPeriodDuration(mpd, periodIndex, mLastPlaylistDownloadTimeMs);
					}
					periodDuration = ((double)periodDurationMs / (double)1000);
					AAMPLOG_INFO("StreamAbstractionAAMP_MPD::%s:%d [MediaPresentation] - MPD periodIndex:%d periodDuration %f", __FUNCTION__, __LINE__, periodIndex, periodDuration);
				}
				else
				{
					string curStartStr = mpd->GetPeriods().at(periodIndex)->GetStart();
					string nextStartStr = "";
					if(periodIndex+1 < periodCnt)
					{
						nextStartStr = mpd->GetPeriods().at(periodIndex+1)->GetStart();
					}
					if(!curStartStr.empty() && (!nextStartStr.empty()) && !mIsFogTSB )
					{
						double  curPeriodStartMs = 0;
						double  nextPeriodStartMs = 0;
						curPeriodStartMs = ParseISO8601Duration(curStartStr.c_str()) + (aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset(mpd->GetPeriods().at(periodIndex)) * 1000);
						nextPeriodStartMs = ParseISO8601Duration(nextStartStr.c_str()) + (aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset(mpd->GetPeriods().at(periodIndex+1)) * 1000);
						periodDurationMs = nextPeriodStartMs - curPeriodStartMs;
						periodDuration = ((double)periodDurationMs / (double)1000);
						AAMPLOG_INFO("StreamAbstractionAAMP_MPD::%s:%d [StartTime based] - MPD periodIndex:%d periodDuration %f", __FUNCTION__, __LINE__, periodIndex, periodDuration);
					}
					else
					{
						periodDurationMs = aamp_GetPeriodDuration(mpd, periodIndex, mLastPlaylistDownloadTimeMs);
						periodDuration = ((double)periodDurationMs / (double)1000);
						AAMPLOG_INFO("StreamAbstractionAAMP_MPD::%s:%d [Segments based] - MPD periodIndex:%d periodDuration %f", __FUNCTION__, __LINE__, periodIndex, periodDuration);
					}
				}
			}
		}
	}
	else
	{
		AAMPLOG_WARN("%s:%d :  mpd is null", __FUNCTION__, __LINE__);  //CID:83436 Null Returns
	}
	return periodDurationMs;
}


/**
 * @brief Get end time of current period
 * @param mpd : pointer manifest
 * @param periodIndex
 * @param mpdRefreshTime : time when manifest was downloaded
 * @retval current period's end time
 */
double StreamAbstractionAAMP_MPD::GetPeriodEndTime(IMPD *mpd, int periodIndex, uint64_t mpdRefreshTime)
{
	double periodStartMs = 0;
	double periodDurationMs = 0;
	double periodEndTime = 0;
	double availablilityStart = 0;
	IPeriod *period = NULL;
	if(mpd != NULL)
	{
			period = mpd->GetPeriods().at(periodIndex);
	}
	else
	{
		AAMPLOG_WARN("%s:%d : mpd is null", __FUNCTION__, __LINE__);  //CID:80459 , 80529- Null returns
	}
	if(period != NULL)
	{
		string startTimeStr = period->GetStart();
		periodDurationMs = GetPeriodDuration(mpd, periodIndex);

		if((0 > mAvailabilityStartTime) && !(mpd->GetType() == "static"))
		{
			AAMPLOG_WARN("%s:%d :  availabilityStartTime required to calculate period duration not present in MPD", __FUNCTION__, __LINE__);
		}
		else if(0 == periodDurationMs)
		{
			AAMPLOG_WARN("%s:%d : Could not get valid period duration to calculate end time", __FUNCTION__, __LINE__);
		}
		else
		{
			if(startTimeStr.empty())
			{
				AAMPLOG_WARN("%s:%d :  Period startTime required to calculate period duration not present in MPD", __FUNCTION__, __LINE__);
			}
			else
			{
				periodStartMs = ParseISO8601Duration(startTimeStr.c_str()) + (aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset(period)* 1000);
			}
			periodEndTime = mAvailabilityStartTime + ((double)(periodStartMs + periodDurationMs) /1000);
		}
		AAMPLOG_INFO("StreamAbstractionAAMP_MPD::%s:%d - MPD periodIndex:%d periodEndTime %f", __FUNCTION__, __LINE__, periodIndex, periodEndTime);
	}
	else
	{
		AAMPLOG_WARN("%s:%d :period is null", __FUNCTION__, __LINE__);  //CID:85519- Null returns
	}
	return periodEndTime;
}

/**
 *   @brief  Get Period Duration
 *   @param  mpd
 *   @param  periodIndex
 *   @retval period duration in milli seconds
  */
double aamp_GetPeriodDuration(dash::mpd::IMPD *mpd, int periodIndex, uint64_t mpdDownloadTime)
{
	double durationMs = 0;
	auto periods = mpd->GetPeriods();
	IPeriod * period = periods.at(periodIndex);

	std::string tempString = period->GetDuration();
	if(!tempString.empty())
	{
		durationMs = ParseISO8601Duration( tempString.c_str());
	}
	//DELIA-45784 Calculate duration from @mediaPresentationDuration for a single period VOD stream having empty @duration.This is added as a fix for voot stream seekposition timestamp issue.
	size_t numPeriods = mpd->GetPeriods().size();
	if(0 == durationMs && mpd->GetType() == "static" && numPeriods == 1)
	{
		std::string durationStr =  mpd->GetMediaPresentationDuration();
		if(!durationStr.empty())
		{
			durationMs = ParseISO8601Duration( durationStr.c_str());
		}
		else
		{
			AAMPLOG_WARN("%s:%d : mediaPresentationDuration missing in period %s", __FUNCTION__, __LINE__, period->GetId().c_str());
		}
	}
	if(0 == durationMs)
	{
		const std::vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();
		const ISegmentTemplate *representation = NULL;
		const ISegmentTemplate *adaptationSet = NULL;
		if (adaptationSets.size() > 0)
		{
			IAdaptationSet * firstAdaptation = adaptationSets.at(0);
			if(firstAdaptation != NULL)
			{
				adaptationSet = firstAdaptation->GetSegmentTemplate();
				const std::vector<IRepresentation *> representations = firstAdaptation->GetRepresentation();
				if (representations.size() > 0)
				{
					representation = representations.at(0)->GetSegmentTemplate();
				}
			
				SegmentTemplates segmentTemplates(representation,adaptationSet);
	
				if( segmentTemplates.HasSegmentTemplate() )
				{
					const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
					uint32_t timeScale = segmentTemplates.GetTimescale();
					//Calculate period duration by adding up the segment durations in timeline
					if (segmentTimeline)
					{
						std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
						int timeLineIndex = 0;
						while (timeLineIndex < timelines.size())
						{
							ITimeline *timeline = timelines.at(timeLineIndex);
							uint32_t repeatCount = timeline->GetRepeatCount();
							double timelineDurationMs = ComputeFragmentDuration(timeline->GetDuration(),timeScale) * 1000;
							durationMs += ((repeatCount + 1) * timelineDurationMs);
							AAMPLOG_TRACE("%s timeLineIndex[%d] size [%lu] updated durationMs[%lf]", __FUNCTION__, timeLineIndex, timelines.size(), durationMs);
							timeLineIndex++;
						}
					}
					else
					{
						std::string periodStartStr = period->GetStart();
						if(!periodStartStr.empty())
						{
							//If it's last period find period duration using mpd download time
							//and minimumUpdatePeriod
							std::string durationStr =  mpd->GetMediaPresentationDuration();
							if(!durationStr.empty() && mpd->GetType() == "static")
							{
								double periodStart = 0;
								double totalDuration = 0;
								periodStart = ParseISO8601Duration( periodStartStr.c_str() );
								totalDuration = ParseISO8601Duration( durationStr.c_str() );
								durationMs = totalDuration - periodStart;
							}
							else if(periodIndex == (periods.size() - 1))
							{
								std::string minimumUpdatePeriodStr = mpd->GetMinimumUpdatePeriod();
								std::string availabilityStartStr = mpd->GetAvailabilityStarttime();
								std::string publishTimeStr;
								auto attributesMap = mpd->GetRawAttributes();
								if(attributesMap.find("publishTime") != attributesMap.end())
								{
									publishTimeStr = attributesMap["publishTime"];
								}

								if(!publishTimeStr.empty())
								{
									mpdDownloadTime = (uint64_t)ISO8601DateTimeToUTCSeconds(publishTimeStr.c_str()) * 1000;
								}

								if(0 == mpdDownloadTime)
								{
									AAMPLOG_WARN("%s:%d :  mpdDownloadTime required to calculate period duration not provided", __FUNCTION__, __LINE__);
								}
								else if(minimumUpdatePeriodStr.empty())
								{
									AAMPLOG_WARN("%s:%d :  minimumUpdatePeriod required to calculate period duration not present in MPD", __FUNCTION__, __LINE__);
								}
								else if(availabilityStartStr.empty())
								{
									AAMPLOG_WARN("%s:%d :  availabilityStartTime required to calculate period duration not present in MPD", __FUNCTION__, __LINE__);
								}
								else
								{
									double periodStart = 0;
									double availablilityStart = 0;
									double minUpdatePeriod = 0;
									periodStart = ParseISO8601Duration( periodStartStr.c_str() );
									availablilityStart = ISO8601DateTimeToUTCSeconds(availabilityStartStr.c_str()) * 1000;
									minUpdatePeriod = ParseISO8601Duration( minimumUpdatePeriodStr.c_str() );
									AAMPLOG_INFO("%s:%d : periodStart %lf availabilityStartTime %lf minUpdatePeriod %lf mpdDownloadTime %lf", __FUNCTION__, __LINE__, periodStart, availablilityStart, minUpdatePeriod, mpdDownloadTime);
									double periodEndTime = mpdDownloadTime + minUpdatePeriod;
									double periodStartTime = availablilityStart + periodStart;
									durationMs = periodEndTime - periodStartTime;
									if(durationMs <= 0)
									{
										AAMPLOG_WARN("%s:%d : Invalid period duration periodStartTime %lf periodEndTime %lf durationMs %lf", __FUNCTION__, __LINE__, periodStartTime, periodEndTime, durationMs);
										durationMs = 0;
									}
								}
							}
							//We can calculate period duration by subtracting startime from next period start time.
							else
							{
								std::string nextPeriodStartStr = periods.at(periodIndex + 1)->GetStart();
								if(!nextPeriodStartStr.empty())
								{
									double periodStart = 0;
									double nextPeriodStart = 0;
									periodStart = ParseISO8601Duration( periodStartStr.c_str() );
									nextPeriodStart = ParseISO8601Duration( nextPeriodStartStr.c_str() );
									durationMs = nextPeriodStart - periodStart;
									if(durationMs <= 0)
									{
										AAMPLOG_WARN("%s:%d : Invalid period duration periodStartTime %lf nextPeriodStart %lf durationMs %lf", __FUNCTION__, __LINE__, periodStart, nextPeriodStart, durationMs);
										durationMs = 0;
									}
								}
								else
								{
									AAMPLOG_WARN("%s:%d : Next period startTime missing periodIndex %d", __FUNCTION__, __LINE__, periodIndex);
								}
							}
						}
						else
						{
							AAMPLOG_WARN("%s:%d : Start time and duration missing in period %s", __FUNCTION__, __LINE__, period->GetId().c_str());
						}
					}
				}
				else
				{
					const std::vector<IRepresentation *> representations = firstAdaptation->GetRepresentation();
					if (representations.size() > 0)
					{
						ISegmentList *segmentList = representations.at(0)->GetSegmentList();
						if (segmentList)
						{
							const std::vector<ISegmentURL*> segmentURLs = segmentList->GetSegmentURLs();
							if(!segmentURLs.empty())
							{
								durationMs += ComputeFragmentDuration( segmentList->GetDuration(), segmentList->GetTimescale()) * 1000;
							}
							else
							{
								AAMPLOG_WARN("%s:%d :  segmentURLs  is null", __FUNCTION__, __LINE__);  //CID:82729 - Null Returns
							}
						}
						else
						{
							AAMPLOG_ERR("%s:%d not-yet-supported mpd format",__FUNCTION__,__LINE__);
						}
					}
				}
			}
			else
			{
				AAMPLOG_WARN("%s:%d :  firstAdaptation is null", __FUNCTION__, __LINE__);  //CID:84261 - Null Returns
			}
		}
	}
	return durationMs;
}

/**
 *   @brief  Initialize a newly created object.
 *   @note   To be implemented by sub classes
 *   @param  tuneType to set type of object.
 *   @retval true on success
 *   @retval false on failure
 */
AAMPStatusType StreamAbstractionAAMP_MPD::Init(TuneType tuneType)
{
	bool forceSpeedsChangedEvent = false;
	bool pushEncInitFragment = false;
	AAMPStatusType retval = eAAMPSTATUS_OK;
	aamp->CurlInit(eCURLINSTANCE_VIDEO, AAMP_TRACK_COUNT, aamp->GetNetworkProxy());
	mCdaiObject->ResetState();

	aamp->mStreamSink->ClearProtectionEvent();
  #ifdef AAMP_MPD_DRM
	AampDRMSessionManager *sessionMgr = aamp->mDRMSessionManager;
	bool forceClearSession = (!ISCONFIGSET(eAAMPConfig_SetLicenseCaching) && (tuneType == eTUNETYPE_NEW_NORMAL));
	sessionMgr->clearDrmSession(forceClearSession);
	sessionMgr->clearFailedKeyIds();
	sessionMgr->setSessionMgrState(SessionMgrState::eSESSIONMGR_ACTIVE);
	sessionMgr->setLicenseRequestAbort(false);
  #endif
	aamp->licenceFromManifest = false;
	bool newTune = aamp->IsNewTune();

	if(newTune)
	{
	//Clear previously stored vss early period ids
		mEarlyAvailablePeriodIds.clear();
		mEarlyAvailableKeyIDMap.clear();
		while(!mPendingKeyIDs.empty())
			mPendingKeyIDs.pop();
	}

	aamp->IsTuneTypeNew = newTune;

#ifdef AAMP_MPD_DRM
	pushEncInitFragment = newTune || (eTUNETYPE_RETUNE == tuneType);
#endif

	for (int i = 0; i < AAMP_TRACK_COUNT; i++)
	{
		aamp->SetCurlTimeout(aamp->mNetworkTimeoutMs, (AampCurlInstance)i);
	}

	AAMPStatusType ret = UpdateMPD(true);
	if (ret == eAAMPSTATUS_OK)
	{
		std::string manifestUrl = aamp->GetManifestUrl();
		mMaxTracks = (rate == AAMP_NORMAL_PLAY_RATE) ? AAMP_TRACK_COUNT : 1;
		double offsetFromStart = seekPosition;
		uint64_t durationMs = 0;
		mNumberOfTracks = 0;
		bool mpdDurationAvailable = false;
		std::string tempString;
		if(mpd != NULL)
		{
			tempString =  mpd->GetMediaPresentationDuration();
		}
		else
		{
			AAMPLOG_WARN("%s:%d :  mpd is null", __FUNCTION__, __LINE__);  //CID:81139 , 81645 ,82315.83556- Null Returns
		}
		if(!tempString.empty())
		{
			durationMs = ParseISO8601Duration( tempString.c_str() );
			mpdDurationAvailable = true;
			logprintf("StreamAbstractionAAMP_MPD::%s:%d - MPD duration str %s val %" PRIu64 " seconds", __FUNCTION__, __LINE__, tempString.c_str(), durationMs/1000);
		}
		mIsLiveStream = !(mpd->GetType() == "static");
		aamp->SetIsLive(mIsLiveStream);
		aamp->SetIsLiveStream(mIsLiveStream);

		if(ContentType_UNKNOWN == aamp->GetContentType())
		{
			if(mIsLiveStream)
				aamp->SetContentType("LINEAR_TV");
			else
				aamp->SetContentType("VOD");
		}
		map<string, string> mpdAttributes = mpd->GetRawAttributes();
		if(mpdAttributes.find("fogtsb") != mpdAttributes.end())
		{
			mIsFogTSB = true;
			mCdaiObject->mIsFogTSB = true;
		}

		if(mIsLiveStream)
		{
			if (aamp->mIsVSS)
			{
				std::string vssVirtualStreamId = GetVssVirtualStreamID();
				
				if (!vssVirtualStreamId.empty())
				{
					AAMPLOG_INFO("%s:%d Virtual stream ID :%s", __FUNCTION__, __LINE__, vssVirtualStreamId.c_str());
					aamp->SetVssVirtualStreamID(vssVirtualStreamId);
				}
			}
			std::string tempStr = mpd->GetMinimumUpdatePeriod();
			if(!tempStr.empty())
			{
				mMinUpdateDurationMs = ParseISO8601Duration( tempStr.c_str() );
			}
			else
			{
				mMinUpdateDurationMs = DEFAULT_INTERVAL_BETWEEN_MPD_UPDATES_MS;
			}

			std::string availabilityStr = mpd->GetAvailabilityStarttime();
			if(!availabilityStr.empty())
			{
				mAvailabilityStartTime = (double)ISO8601DateTimeToUTCSeconds(availabilityStr.c_str());
			}
			logprintf("StreamAbstractionAAMP_MPD::%s:%d - AvailabilityStartTime=%f", __FUNCTION__, __LINE__, mAvailabilityStartTime);

			tempStr = mpd->GetTimeShiftBufferDepth();
			uint64_t timeshiftBufferDepthMS = 0;
			if(!tempStr.empty())
			{
				timeshiftBufferDepthMS = ParseISO8601Duration( tempStr.c_str() );
			}
			if(timeshiftBufferDepthMS)
			{
				mTSBDepth = (double)timeshiftBufferDepthMS / 1000;
				// Add valid check for minimum size requirement here
				uint64_t segmentDurationSeconds = 0;
				tempStr = mpd->GetMaxSegmentDuration();
				if(!tempStr.empty())
				{
					segmentDurationSeconds = ParseISO8601Duration( tempStr.c_str() ) / 1000;
				}
				if(mTSBDepth < ( 4 * (double)segmentDurationSeconds))
				{
					mTSBDepth = ( 4 * (double)segmentDurationSeconds);
				}
			}

			tempStr = mpd->GetSuggestedPresentationDelay();
			uint64_t presentationDelay = 0;
			if(!tempStr.empty())
			{
				presentationDelay = ParseISO8601Duration( tempStr.c_str() );
			}
			if(presentationDelay)
			{
				mPresentationOffsetDelay = (double)presentationDelay / 1000;
			}
			else
			{
				tempStr = mpd->GetMinBufferTime();
				uint64_t minimumBufferTime = 0;
				if(!tempStr.empty())
				{
					minimumBufferTime = ParseISO8601Duration( tempStr.c_str() );
				}
				if(minimumBufferTime)
				{
					mPresentationOffsetDelay = 	(double)minimumBufferTime / 1000;
				}
				else
				{
					mPresentationOffsetDelay = 2.0;
				}
			}
			mFirstPeriodStartTime = GetPeriodStartTime(mpd,0);
			if(ISCONFIGSET(eAAMPConfig_UseAbsoluteTimeline) && (aamp->mProgressReportOffset < 0))
			{
				// mProgressReportOffset calculated only once
				// Default value will be -1, since 0 is a possible offset.
				IPeriod *firstPeriod = mpd->GetPeriods().at(0);
				aamp->mProgressReportOffset = mFirstPeriodStartTime - mAvailabilityStartTime;
			}

			AAMPLOG_WARN("StreamAbstractionAAMP_MPD::%s:%d - MPD minupdateduration val %" PRIu64 " seconds mTSBDepth %f mPresentationOffsetDelay :%f StartTimeFirstPeriod: %lf offsetStartTime: %lf", __FUNCTION__, __LINE__,  mMinUpdateDurationMs/1000, mTSBDepth, mPresentationOffsetDelay, mFirstPeriodStartTime, aamp->mProgressReportOffset);
		}

		for (int i = 0; i < mMaxTracks; i++)
		{
			mMediaStreamContext[i] = new MediaStreamContext((TrackType)i, this, aamp, getMediaTypeName(MediaType(i)));
			mMediaStreamContext[i]->fragmentDescriptor.manifestUrl = manifestUrl;
			mMediaStreamContext[i]->mediaType = (MediaType)i;
			mMediaStreamContext[i]->representationIndex = -1;
		}

		uint64_t nextPeriodStart = 0;
		double currentPeriodStart = 0;
		double prevPeriodEndMs = 0; // used to find gaps between periods
		size_t numPeriods = mpd->GetPeriods().size();
		bool seekPeriods = true;

		if (ISCONFIGSET(eAAMPConfig_UseAbsoluteTimeline) && mIsLiveStream && !mIsFogTSB)
		{
			double culled = aamp->culledSeconds;
			UpdateCulledAndDurationFromPeriodInfo();
			culled = aamp->culledSeconds - culled;
			if (culled > 0 && !newTune)
			{
				offsetFromStart -= culled;
				if(offsetFromStart < 0)
				{
					offsetFromStart = 0;
					AAMPLOG_WARN("%s:%d Resetting offset from start to 0", __FUNCTION__, __LINE__);
				}
			}
		}

		for (unsigned iPeriod = 0; iPeriod < numPeriods; iPeriod++)
		{//TODO -  test with streams having multiple periods.
			IPeriod *period = mpd->GetPeriods().at(iPeriod);
			if(IsEmptyPeriod(period))
			{
				// Empty Period . Ignore processing, continue to next.
				continue;
			}
			std::string tempString = period->GetDuration();
			double  periodStartMs = 0;
			double periodDurationMs = 0;
			periodDurationMs = aamp_GetPeriodDuration(mpd, iPeriod, mLastPlaylistDownloadTimeMs);
			if (!mpdDurationAvailable)
			{
				durationMs += periodDurationMs;
				AAMPLOG_INFO("%s:%d - Updated duration %lf seconds", __FUNCTION__, __LINE__, (durationMs/1000));
			}

			if(offsetFromStart >= 0 && seekPeriods)
			{
				tempString = period->GetStart();
				if(!tempString.empty() && !mIsFogTSB)
				{
					periodStartMs = ParseISO8601Duration( tempString.c_str() );
				}
				else if (periodDurationMs)
				{
					periodStartMs = nextPeriodStart;
				}

				if(ISCONFIGSET(eAAMPConfig_UseAbsoluteTimeline) && aamp->IsLiveStream() && !mIsFogTSB && iPeriod == 0)
				{
					// Adjust start time wrt presentation time offset.
					if(!mIsLiveStream)
					{
						// Content moved from Live to VOD, and then TearDown performed
						periodStartMs += aamp->culledSeconds;
					}
					else
					{
						periodStartMs += (aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset(period) * 1000);
					}
				}

				double periodStartSeconds = periodStartMs/1000;
				double periodDurationSeconds = (double)periodDurationMs / 1000;
				if (periodDurationMs != 0)
				{
					double periodEnd = periodStartMs + periodDurationMs;
					nextPeriodStart += periodDurationMs; // set the value here, nextPeriodStart is used below to identify "Multi period assets with no period duration" if it is set to ZERO.

					// check for gaps between periods
					if(prevPeriodEndMs > 0)
					{
						double periodGap = (periodStartMs - prevPeriodEndMs)/ 1000; // current period start - prev period end will give us GAP between period
						if(std::abs(periodGap) > 0 ) // ohh we have GAP between last and current period
						{
							offsetFromStart -= periodGap; // adjust offset to accomodate gap
							if(offsetFromStart < 0 ) // this means offset is between gap, set to start of currentPeriod
							{
								offsetFromStart = 0;
							}
							AAMPLOG_WARN("%s:%d GAP betwen period found :GAP:%f  mCurrentPeriodIdx %d currentPeriodStart %f offsetFromStart %f", __FUNCTION__, __LINE__,
								periodGap, mCurrentPeriodIdx, periodStartSeconds, offsetFromStart);
						}
					}
					prevPeriodEndMs = periodEnd; // store for future use
					// Save period start time as first PTS for absolute progress reporting.
					if(ISCONFIGSET(eAAMPConfig_UseAbsoluteTimeline) && !mIsLiveStream)
					{
						// For VOD, take start time as diff between current start and first period start.
						mStartTimeOfFirstPTS = periodStartMs - ((GetPeriodStartTime(mpd, 0) - mAvailabilityStartTime) * 1000);
					}
					if(aamp->IsLiveStream())
					{
						currentPeriodStart = periodStartSeconds;
					}
					else
					{
						currentPeriodStart = periodStartSeconds - (GetPeriodStartTime(mpd, 0) - mAvailabilityStartTime);
					}
					mCurrentPeriodIdx = iPeriod;
					if (periodDurationSeconds <= offsetFromStart && iPeriod < (numPeriods - 1))
					{
						offsetFromStart -= periodDurationSeconds;
						logprintf("Skipping period %d seekPosition %f periodEnd %f offsetFromStart %f", iPeriod, seekPosition, periodEnd, offsetFromStart);
						continue;
					}
					else
					{
						seekPeriods = false;
					}
				}
				else if(periodStartSeconds <= offsetFromStart)
				{
					mCurrentPeriodIdx = iPeriod;
					currentPeriodStart = periodStartSeconds;
				}
			}
		}

		//Check added to update offsetFromStart for
		//Multi period assets with no period duration
		if(0 == nextPeriodStart)
		{
			offsetFromStart -= currentPeriodStart;
		}
		bool segmentTagsPresent = true;
		//The OR condition is added to see if segment info is available in live MPD
		//else we need to calculate fragments based on time
		if (0 == durationMs || (mpdDurationAvailable && mIsLiveStream && !mIsFogTSB))
		{
			durationMs = aamp_GetDurationFromRepresentation(mpd);
			logprintf("%s:%d - Duration after GetDurationFromRepresentation %" PRIu64 " seconds", __FUNCTION__, __LINE__, durationMs/1000);
		}

		if(0 == durationMs)
		{
			segmentTagsPresent = false;
			for(int iPeriod = 0; iPeriod < mpd->GetPeriods().size(); iPeriod++)
			{
				durationMs += GetPeriodDuration(mpd, iPeriod);
			}
			logprintf("%s:%d - Duration after adding up Period Duration %" PRIu64 " seconds", __FUNCTION__, __LINE__, durationMs/1000);
		}
		/*Do live adjust on live streams on 1. eTUNETYPE_NEW_NORMAL, 2. eTUNETYPE_SEEKTOLIVE,
		 * 3. Seek to a point beyond duration*/
		bool notifyEnteringLive = false;
		if (mIsLiveStream)
		{
			double duration = (double) durationMs / 1000;
			mLiveEndPosition = duration;
			bool liveAdjust = (eTUNETYPE_NEW_NORMAL == tuneType) && aamp->IsLiveAdjustRequired();
			
			if (eTUNETYPE_SEEKTOLIVE == tuneType)
			{
				logprintf("StreamAbstractionAAMP_MPD::%s:%d eTUNETYPE_SEEKTOLIVE", __FUNCTION__, __LINE__);
				liveAdjust = true;
				notifyEnteringLive = true;
			}
			else if (((eTUNETYPE_SEEK == tuneType) || (eTUNETYPE_RETUNE == tuneType || eTUNETYPE_NEW_SEEK == tuneType)) && (rate > 0))
			{
				double seekWindowEnd = duration - aamp->mLiveOffset;
				// check if seek beyond live point
				if (seekPosition > seekWindowEnd)
				{
					logprintf( "StreamAbstractionAAMP_MPD::%s:%d offSetFromStart[%f] seekWindowEnd[%f]",
							__FUNCTION__, __LINE__, seekPosition, seekWindowEnd);
					liveAdjust = true;
					if (eTUNETYPE_SEEK == tuneType)
					{
						notifyEnteringLive = true;
					}
				}
			}
			if (liveAdjust)
			{
				// DELIA-43662
				// After live adjust ( for Live or CDVR) , possibility of picking an empty last period exists.
				// Though its ignored in Period selection earlier , live adjust will end up picking last added empty period
				// Instead of picking blindly last period, pick the period the last period which contains some stream data
				mCurrentPeriodIdx = mpd->GetPeriods().size();
				while( mCurrentPeriodIdx>0 )
				{
					mCurrentPeriodIdx--;
					IPeriod *period = mpd->GetPeriods().at(mCurrentPeriodIdx);
					if( !IsEmptyPeriod(period) )
					{ // found last non-empty period
						break;
					}
				}
 
				if(aamp->IsLiveAdjustRequired())
				{
					if(segmentTagsPresent)
					{
						duration = (GetPeriodDuration(mpd, mCurrentPeriodIdx)) / 1000;
						currentPeriodStart = ((double)durationMs / 1000) - duration;
						offsetFromStart = duration - aamp->mLiveOffset;
						while(offsetFromStart < 0 && mCurrentPeriodIdx > 0)
						{
							AAMPLOG_INFO("%s:%d Adjusting to live offset offsetFromStart %f, mCurrentPeriodIdx %d", __FUNCTION__, __LINE__, offsetFromStart, mCurrentPeriodIdx);
							mCurrentPeriodIdx--;
							duration = (GetPeriodDuration(mpd, mCurrentPeriodIdx)) / 1000;
							currentPeriodStart = currentPeriodStart - duration;
							offsetFromStart = offsetFromStart + duration;
						}
					}
					else
					{
						//Calculate live offset based on time elements in the mpd
						double currTime = (double)aamp_GetCurrentTimeMS() / 1000;
						double liveoffset = aamp->mLiveOffset;
						if(mTSBDepth && mTSBDepth < liveoffset)
						{
							liveoffset = mTSBDepth;
						}
						
						double startTime = currTime - liveoffset;
						if(startTime < 0)
							startTime = 0;
						currentPeriodStart = ((double)durationMs / 1000);
						while(mCurrentPeriodIdx >= 0)
						{
							mPeriodStartTime =  GetPeriodStartTime(mpd, mCurrentPeriodIdx);

							duration = (GetPeriodDuration(mpd, mCurrentPeriodIdx)) / 1000;
							currentPeriodStart -= duration;
							if(mPeriodStartTime < startTime)
							{
								offsetFromStart = startTime - mPeriodStartTime;
								break;
							}
							mCurrentPeriodIdx--;
						}
					}
					AAMPLOG_WARN("%s:%d duration %f durationMs %f mCurrentPeriodIdx %d currentPeriodStart %f offsetFromStart %f", __FUNCTION__, __LINE__,
				 duration, (double)durationMs / 1000, mCurrentPeriodIdx, currentPeriodStart, offsetFromStart);
				}
				else
				{
					uint64_t  periodStartMs = 0;
					IPeriod *period = mpd->GetPeriods().at(mCurrentPeriodIdx);
					std::string tempString = period->GetStart();
					periodStartMs = ParseISO8601Duration( tempString.c_str() );
					currentPeriodStart = (double)periodStartMs/1000;
					offsetFromStart = duration - aamp->mLiveOffset - currentPeriodStart;
				}

				if (offsetFromStart < 0)
				{
					offsetFromStart = 0;
				}
				mIsAtLivePoint = true;
				logprintf( "StreamAbstractionAAMP_MPD::%s:%d - liveAdjust - Updated offSetFromStart[%f] duration [%f] currentPeriodStart[%f] MaxPeriodIdx[%d]",
						__FUNCTION__, __LINE__, offsetFromStart, duration, currentPeriodStart,mCurrentPeriodIdx);
			}

		}
		else
		{
			// Non-live - VOD/CDVR(Completed) - DELIA-30266
			if(durationMs == INVALID_VOD_DURATION)
			{
				AAMPLOG_WARN("%s:%d Duration of VOD content is 0", __FUNCTION__, __LINE__) ;
				return eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
			}

			double seekWindowEnd = (double) durationMs / 1000;
			if(seekPosition > seekWindowEnd)
			{
				for (int i = 0; i < mNumberOfTracks; i++)
				{
					mMediaStreamContext[i]->eosReached=true;
				}
				logprintf("%s:%d seek target out of range, mark EOS. playTarget:%f End:%f. ",
					__FUNCTION__,__LINE__,seekPosition, seekWindowEnd);
				return eAAMPSTATUS_SEEK_RANGE_ERROR;
			}
		}
		mPeriodStartTime =  GetPeriodStartTime(mpd, mCurrentPeriodIdx);
		mPeriodDuration =  GetPeriodDuration(mpd, mCurrentPeriodIdx);
		mPeriodEndTime = GetPeriodEndTime(mpd, mCurrentPeriodIdx, mLastPlaylistDownloadTimeMs);
		mCurrentPeriod = mpd->GetPeriods().at(mCurrentPeriodIdx);
		if(ISCONFIGSET(eAAMPConfig_UseAbsoluteTimeline) && !mIsFogTSB)
		{
			// For live stream, take period start time
			mStartTimeOfFirstPTS = (mPeriodStartTime - mAvailabilityStartTime) * 1000;
		}

		if(mCurrentPeriod != NULL)
		{
			mBasePeriodId = mCurrentPeriod->GetId();
		}
		else
		{
			AAMPLOG_WARN("%s:%d :  mCurrentPeriod  is null", __FUNCTION__, __LINE__);  //CID:84770 - Null Return
		}
		mBasePeriodOffset = offsetFromStart;

		onAdEvent(AdEvent::INIT, offsetFromStart);

		UpdateLanguageList();

		if (eTUNETYPE_SEEK == tuneType)
		{
			forceSpeedsChangedEvent = true; // Send speed change event if seek done from non-iframe period to iframe available period to inform XRE to allow trick operations.
		}

		StreamSelection(true, forceSpeedsChangedEvent);
		//DELIA-51402 - calling ReportTimedMetadata function after drm creation in order
		//to reduce the delay caused
		aamp->ReportTimedMetadata(true);
		if(mAudioType == eAUDIO_UNSUPPORTED)
		{
			retval = eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
			aamp->SendErrorEvent(AAMP_TUNE_UNSUPPORTED_AUDIO_TYPE);
		}
		else if(mNumberOfTracks)
		{
			aamp->SendEventAsync(std::make_shared<AAMPEventObject>(AAMP_EVENT_PLAYLIST_INDEXED));
			if (eTUNED_EVENT_ON_PLAYLIST_INDEXED == aamp->GetTuneEventConfig(mIsLiveStream))
			{
				if (aamp->SendTunedEvent(!aamp->GetAsyncTuneConfig()))
				{
					logprintf("aamp: mpd - sent tune event after indexing playlist");
				}
			}
			ret = UpdateTrackInfo(!newTune, true, true);

			if(eAAMPSTATUS_OK != ret)
			{
				if (ret == eAAMPSTATUS_MANIFEST_CONTENT_ERROR)
				{
					AAMPLOG_ERR("%s:%d ERROR: No playable profiles found", __FUNCTION__, __LINE__);
				}
				return ret;
			}

			if(!newTune && mIsFogTSB)
			{
				double culled = 0;
				if(mMediaStreamContext[eMEDIATYPE_VIDEO]->enabled)
				{
					culled = GetCulledSeconds();
				}
				if(culled > 0)
				{
					AAMPLOG_INFO("%s:%d Culled seconds = %f, Adjusting seekPos after considering new culled value", __FUNCTION__, __LINE__, culled);
					aamp->UpdateCullingState(culled);
				}

				double durMs = 0;
				for(int periodIter = 0; periodIter < mpd->GetPeriods().size(); periodIter++)
				{
					if(!IsEmptyPeriod(mpd->GetPeriods().at(periodIter)))
					{
						durMs += GetPeriodDuration(mpd, periodIter);
					}
				}

				double duration = (double)durMs / 1000;
				aamp->UpdateDuration(duration);
			}

			if(notifyEnteringLive)
			{
				aamp->NotifyOnEnteringLive();
			}
			SeekInPeriod( offsetFromStart);
			if(!ISCONFIGSET(eAAMPConfig_MidFragmentSeek))
			{
				seekPosition = mMediaStreamContext[eMEDIATYPE_VIDEO]->fragmentTime;
				if((0 != mCurrentPeriodIdx && !ISCONFIGSET(eAAMPConfig_UseAbsoluteTimeline)) || mIsFogTSB)
				{
					// Avoid adding period start time for Absolute progress reporting,
					// position is adjusted in TuneHelper based on current period start,
					// So just save fragmentTime.
					seekPosition += currentPeriodStart;
				}
			}
			for (int i = 0; i < mNumberOfTracks; i++)
			{
				if(0 != mCurrentPeriodIdx)
				{
					mMediaStreamContext[i]->fragmentTime = seekPosition;
				}
				mMediaStreamContext[i]->periodStartOffset = currentPeriodStart;
			}
			AAMPLOG_INFO("%s:%d offsetFromStart(%f) seekPosition(%f) currentPeriodStart(%f)",__FUNCTION__,__LINE__, offsetFromStart,seekPosition, currentPeriodStart);

			if (newTune )
			{
				std::vector<long> bitrateList;
				bitrateList.reserve(GetProfileCount());

				for (int i = 0; i < GetProfileCount(); i++)
				{
					if (!mStreamInfo[i].isIframeTrack)
					{
						bitrateList.push_back(mStreamInfo[i].bandwidthBitsPerSecond);
					}
				}

				aamp->SetState(eSTATE_PREPARING);
				//For DASH, presence of iframe track depends on current period.
				aamp->SendMediaMetadataEvent(durationMs, mLangList, bitrateList, hasDrm, aamp->mIsIframeTrackPresent, mAvailabilityStartTime);

				aamp->UpdateDuration(((double)durationMs)/1000);
				GetCulledSeconds();
				aamp->UpdateRefreshPlaylistInterval((float)mMinUpdateDurationMs / 1000);
			}
		}
		else
		{
			logprintf("No adaptation sets could be selected");
			retval = eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
		}
	}
	else
	{
		AAMPLOG_ERR("StreamAbstractionAAMP_MPD::%s:%d corrupt/invalid manifest",__FUNCTION__,__LINE__);
		retval = eAAMPSTATUS_MANIFEST_PARSE_ERROR;
	}
	if (ret == eAAMPSTATUS_OK)
	{
#ifdef AAMP_MPD_DRM
		if (pushEncInitFragment && CheckForInitalClearPeriod())
		{
			AAMPLOG_WARN("StreamAbstractionAAMP_MPD::%s:%d Pushing EncryptedHeaders", __FUNCTION__, __LINE__);
			PushEncryptedHeaders();
		}
#endif

		logprintf("StreamAbstractionAAMP_MPD::%s:%d - fetch initialization fragments", __FUNCTION__, __LINE__);
		FetchAndInjectInitialization();
	}

	return retval;
}

/**
 * @brief Get duration though representation iteration
 * @retval duration in milliseconds
 */
uint64_t aamp_GetDurationFromRepresentation(dash::mpd::IMPD *mpd)
{
	uint64_t durationMs = 0;
	size_t numPeriods = mpd->GetPeriods().size();

	for (unsigned iPeriod = 0; iPeriod < numPeriods; iPeriod++)
	{
		IPeriod *period = NULL;
		if(mpd != NULL)
		{
				period = mpd->GetPeriods().at(iPeriod);
		}
		else
		{
			AAMPLOG_WARN("%s:%d :  mpd is null", __FUNCTION__, __LINE__);  //CID:82158 - Null Returns
		}
		
		if(period != NULL)
		{
			const std::vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();
			if (adaptationSets.size() > 0)
			{
				IAdaptationSet * firstAdaptation = adaptationSets.at(0);
				ISegmentTemplate *AdapSegmentTemplate = NULL;
				ISegmentTemplate *RepSegmentTemplate = NULL;
				if (firstAdaptation)
				{
					AdapSegmentTemplate = firstAdaptation->GetSegmentTemplate();
					const std::vector<IRepresentation *> representations = firstAdaptation->GetRepresentation();
					if (representations.size() > 0)
					{
						RepSegmentTemplate  = representations.at(0)->GetSegmentTemplate();
					}
				}
				SegmentTemplates segmentTemplates(RepSegmentTemplate,AdapSegmentTemplate);
				if (segmentTemplates.HasSegmentTemplate())
				{
					std::string media = segmentTemplates.Getmedia();
					if(!media.empty())
					{
						const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
						if (segmentTimeline)
						{
							std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
							uint32_t timeScale = segmentTemplates.GetTimescale();
							int timeLineIndex = 0;
							while (timeLineIndex < timelines.size())
							{
								ITimeline *timeline = timelines.at(timeLineIndex);
								uint32_t repeatCount = timeline->GetRepeatCount();
								uint32_t timelineDurationMs = ComputeFragmentDuration(timeline->GetDuration(),timeScale) * 1000;
								durationMs += ((repeatCount + 1) * timelineDurationMs);
								traceprintf("%s period[%d] timeLineIndex[%d] size [%lu] updated durationMs[%" PRIu64 "]", __FUNCTION__, iPeriod, timeLineIndex, timelines.size(), durationMs);
								timeLineIndex++;
							}
						}
					}
					else
					{
						AAMPLOG_WARN("%s:%d :   media is null", __FUNCTION__, __LINE__);  //CID:83185 - Null Returns
					}
				}
				else
				{
					const std::vector<IRepresentation *> representations = firstAdaptation->GetRepresentation();
					if (representations.size() > 0)
					{
						ISegmentList *segmentList = representations.at(0)->GetSegmentList();
						if (segmentList)
						{
							const std::vector<ISegmentURL*> segmentURLs = segmentList->GetSegmentURLs();
							if(segmentURLs.empty())
							{
								AAMPLOG_WARN("%s:%d :  segmentURLs is null", __FUNCTION__, __LINE__);  //CID:82113 - Null Returns
							}
							durationMs += ComputeFragmentDuration(segmentList->GetDuration(), segmentList->GetTimescale()) * 1000;
						}
						else
						{
							AAMPLOG_ERR("%s:%d not-yet-supported mpd format",__FUNCTION__,__LINE__);
						}
					}
				}
			}
		}
		else
		{
			AAMPLOG_WARN("%s:%d :  period is null", __FUNCTION__, __LINE__);  //CID:81482 - Null Returns
		}
	}
	return durationMs;
}


/**
 * @brief Update MPD manifest
 * @param retrievePlaylistFromCache true to try to get from cache
 * @retval true on success
 */
AAMPStatusType StreamAbstractionAAMP_MPD::UpdateMPD(bool init)
{
	GrowableBuffer manifest;
	AAMPStatusType ret = AAMPStatusType::eAAMPSTATUS_OK;
	std::string manifestUrl = aamp->GetManifestUrl();

	// take the original url before it gets changed in GetFile
	std::string origManifestUrl = manifestUrl;
	bool gotManifest = false;
	bool retrievedPlaylistFromCache = false;
	memset(&manifest, 0, sizeof(manifest));
	if (aamp->getAampCacheHandler()->RetrieveFromPlaylistCache(manifestUrl, &manifest, manifestUrl))
	{
		logprintf("StreamAbstractionAAMP_MPD::%s:%d manifest retrieved from cache", __FUNCTION__, __LINE__);
		retrievedPlaylistFromCache = true;
	}
	if (!retrievedPlaylistFromCache)
	{
		long http_error = 0;
		double downloadTime;
		memset(&manifest, 0, sizeof(manifest));
		aamp->profiler.ProfileBegin(PROFILE_BUCKET_MANIFEST);
		aamp->SetCurlTimeout(aamp->mManifestTimeoutMs,eCURLINSTANCE_VIDEO);
		gotManifest = aamp->GetFile(manifestUrl, &manifest, manifestUrl, &http_error, &downloadTime, NULL, eCURLINSTANCE_VIDEO, true, eMEDIATYPE_MANIFEST);
		aamp->SetCurlTimeout(aamp->mNetworkTimeoutMs,eCURLINSTANCE_VIDEO);
		//update videoend info
		aamp->UpdateVideoEndMetrics(eMEDIATYPE_MANIFEST,0,http_error,manifestUrl,downloadTime);

		if (gotManifest)
		{
			aamp->mManifestUrl = manifestUrl;
			aamp->profiler.ProfileEnd(PROFILE_BUCKET_MANIFEST);
			if (mNetworkDownDetected)
			{
				mNetworkDownDetected = false;
			}
		}
		else if (aamp->DownloadsAreEnabled())
		{
			aamp->profiler.ProfileError(PROFILE_BUCKET_MANIFEST, http_error);
			if (this->mpd != NULL && (CURLE_OPERATION_TIMEDOUT == http_error || CURLE_COULDNT_CONNECT == http_error))
			{
				//Skip this for first ever update mpd request
				mNetworkDownDetected = true;
				logprintf("StreamAbstractionAAMP_MPD::%s Ignore curl timeout", __FUNCTION__);
				ret = AAMPStatusType::eAAMPSTATUS_OK;
			}
			else
			{
				aamp->SendDownloadErrorEvent(AAMP_TUNE_MANIFEST_REQ_FAILED, http_error);
				logprintf("StreamAbstractionAAMP_MPD::%s - manifest download failed", __FUNCTION__);
				ret = AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR;
			}
		}
		else // if downloads disabled
		{
			AAMPLOG_ERR("StreamAbstractionAAMP_MPD::%s - manifest download failed", __FUNCTION__);
			ret = AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR;
		}
	}
	else
	{
		gotManifest = true;
		aamp->mManifestUrl = manifestUrl;
	}

	if (gotManifest)
	{
		MPD* mpd = nullptr;
		vector<std::string> locationUrl;
		ret = GetMpdFromManfiest(manifest, mpd, manifestUrl, init);
		if (eAAMPSTATUS_OK == ret)
		{
			/* DELIA-42794: All manifest requests after the first should
			 * reference the url from the Location element. This is per MPEG
			 * specification */
			locationUrl = mpd->GetLocations();
			if( !locationUrl.empty() )
			{
				aamp->SetManifestUrl(locationUrl[0].c_str());
			}
			if (this->mpd)
			{
				delete this->mpd;
			}
			this->mpd = mpd;
			if(aamp->mIsVSS)
			{
				CheckForVssTags();
			}
			if (!retrievedPlaylistFromCache && !mIsLiveManifest)
			{
				aamp->getAampCacheHandler()->InsertToPlaylistCache(origManifestUrl, &manifest, aamp->GetManifestUrl(), mIsLiveStream,eMEDIATYPE_MANIFEST);
			}
		}
		else
		{
			logprintf("%s:%d Error while processing MPD, GetMpdFromManfiest returned %d", __FUNCTION__, __LINE__, ret);
			retrievedPlaylistFromCache = false;
		}
		aamp_Free(&manifest);
		mLastPlaylistDownloadTimeMs = aamp_GetCurrentTimeMS();
		if(mIsLiveStream && ISCONFIGSET(eAAMPConfig_EnableClientDai))
		{
			mCdaiObject->PlaceAds(mpd);
		}
	}
	else if (AAMPStatusType::eAAMPSTATUS_OK != ret)
	{
		logprintf("aamp: error on manifest fetch");
	}

	if( ret == eAAMPSTATUS_MANIFEST_PARSE_ERROR || ret == eAAMPSTATUS_MANIFEST_CONTENT_ERROR)
	{
		if(NULL != manifest.ptr && !manifestUrl.empty())
		{
			int tempDataLen = (MANIFEST_TEMP_DATA_LENGTH - 1);
			char temp[MANIFEST_TEMP_DATA_LENGTH];
			strncpy(temp, manifest.ptr, tempDataLen);
			temp[tempDataLen] = 0x00;
			logprintf("ERROR: Invalid Playlist URL: %s ret:%d", manifestUrl.c_str(),ret);
			logprintf("ERROR: Invalid Playlist DATA: %s ", temp);
		}
		aamp->SendErrorEvent(AAMP_TUNE_INVALID_MANIFEST_FAILURE);
	}

	return ret;
}

/**
 * @brief Check if Period is empty or not
 * @param Period
 * @retval Return true on empty Period
 */
bool StreamAbstractionAAMP_MPD::IsEmptyPeriod(IPeriod *period)
{
	bool isEmptyPeriod = true;
	const std::vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();
	size_t numAdaptationSets = period->GetAdaptationSets().size();
	for (int iAdaptationSet = 0; iAdaptationSet < numAdaptationSets; iAdaptationSet++)
	{
		IAdaptationSet *adaptationSet = period->GetAdaptationSets().at(iAdaptationSet);

		if (rate != AAMP_NORMAL_PLAY_RATE)
		{
			if (IsIframeTrack(adaptationSet))
			{
				isEmptyPeriod = false;
				break;
			}
		}
		else
		{
			IRepresentation *representation = NULL;
			ISegmentTemplate *segmentTemplate = adaptationSet->GetSegmentTemplate();
			if (segmentTemplate)
			{
				isEmptyPeriod = false;
			}
			else
			{
				if(adaptationSet->GetRepresentation().size() > 0)
				{
					//Get first representation in the adapatation set
					representation = adaptationSet->GetRepresentation().at(0);
				}
				if(representation)
				{
					segmentTemplate = representation->GetSegmentTemplate();
					if(segmentTemplate)
					{
						isEmptyPeriod = false;
					}
					else
					{
						ISegmentList *segmentList = representation->GetSegmentList();
						if(segmentList)
						{
							isEmptyPeriod = false;
						}
						else
						{
							ISegmentBase *segmentBase = representation->GetSegmentBase();
							if(segmentBase)
							{
								isEmptyPeriod = false;
							}
						}
					}
				}
			}

			if(!isEmptyPeriod)
			{
				// Not to loop thru all Adaptations if one found.
				break;
			}
		}
	}

	return isEmptyPeriod;
}




/**
 * @brief Find timed metadata from mainifest
 * @param mpd MPD top level element
 * @param root XML root node
 * @param init true if this is the first playlist download for a tune/seek/trickplay
 * @param reportBulkMeta true if bulkTimedMetadata feature is enabled
 */
void StreamAbstractionAAMP_MPD::FindTimedMetadata(MPD* mpd, Node* root, bool init, bool reportBulkMeta)
{
	std::vector<Node*> subNodes = root->GetSubNodes();
	if(!subNodes.empty())
		{
		uint64_t periodStartMS = 0;
		uint64_t periodDurationMS = 0;
		std::vector<std::string> newPeriods;
		// Iterate through each of the MPD's Period nodes, and ProgrameInformation.
		int periodCnt = 0;
		for (size_t i=0; i < subNodes.size(); i++) {
			Node* node = subNodes.at(i);
			if(!node)
			{
				AAMPLOG_WARN("%s:%d :  node is null", __FUNCTION__, __LINE__);  //CID:80723 - Null Returns
			}
			const std::string& name = node->GetName();
			if (name == "Period") {
				std::string AdID;
				std::string AssetID;
				std::string ProviderID;
				periodCnt++;

				// Calculate period start time and duration
				periodStartMS += periodDurationMS;
				if (node->HasAttribute("start")) {
					const std::string& value = node->GetAttributeValue("start");
					uint64_t valueMS = 0;
					if (!value.empty())
						valueMS = ParseISO8601Duration(value.c_str() );
					if (periodStartMS < valueMS)
						periodStartMS = valueMS;
				}
				periodDurationMS = 0;
				if (node->HasAttribute("duration")) {
					const std::string& value = node->GetAttributeValue("duration");
					uint64_t valueMS = 0;
					if (!value.empty())
						valueMS = ParseISO8601Duration(value.c_str() );
					periodDurationMS = valueMS;
				}
				IPeriod * period = NULL;
				if (mpd != NULL)
				{
						period = mpd->GetPeriods().at(periodCnt-1);
				}
				else
				{
					AAMPLOG_WARN("%s:%d :  mpd is null", __FUNCTION__, __LINE__);  //CID:80941 - Null Returns
				}
				std::string adBreakId("");
				if(period != NULL)
				{
					const std::string &prdId = period->GetId();
					// Iterate through children looking for SupplementProperty nodes
					std::vector<Node*> children = node->GetSubNodes();
					for (size_t j=0; j < children.size(); j++) {
						Node* child = children.at(j);
						const std::string& name = child->GetName();
						if(!name.empty())
						{
							if (name == "SupplementalProperty") {
								ProcessPeriodSupplementalProperty(child, AdID, periodStartMS, periodDurationMS, init, reportBulkMeta);
								continue;
							}
							if (name == "AssetIdentifier") {
								ProcessPeriodAssetIdentifier(child, periodStartMS, periodDurationMS, AssetID, ProviderID, init, reportBulkMeta);
								continue;
							}
							if(name == "EventStream" && "" != prdId && !(mCdaiObject->isPeriodExist(prdId))
                                				&& ((!init || (1 < periodCnt && 0 == period->GetAdaptationSets().size()))   //Take last & empty period at the MPD init AND all new periods in the MPD refresh. (No empty periods will come the middle)
                                   				|| (!mIsLiveManifest && init))) 				//to enable VOD content to send the metadata
							{
								mCdaiObject->InsertToPeriodMap(period);	//Need to do it. Because the FulFill may finish quickly
								ProcessEventStream(periodStartMS, period, reportBulkMeta);
								continue;
							}
						}
						else
						{
							AAMPLOG_WARN("%s:%d :  name is empty", __FUNCTION__, __LINE__);  //CID:80526 - Null Returns
						}
					}
					if("" != prdId)
					{
						mCdaiObject->InsertToPeriodMap(period);
						newPeriods.emplace_back(prdId);
					}
					continue;
				}
			}
			if (name == "ProgramInformation") {
				std::vector<Node*> infoNodes = node->GetSubNodes();
				for (size_t i=0; i < infoNodes.size(); i++) {
					Node* infoNode = infoNodes.at(i);
					std::string name;
					std::string ns;
					ParseXmlNS(infoNode->GetName(), ns, name);
					const std::string& infoNodeType = infoNode->GetAttributeValue("type");
					if (name == "ContentIdentifier" && (infoNodeType == "URI" || infoNodeType == "URN")) {
						if (infoNode->HasAttribute("value")) {
							const std::string& content = infoNode->GetAttributeValue("value");

							AAMPLOG_TRACE("TimedMetadata: @%1.3f #EXT-X-CONTENT-IDENTIFIER:%s", 0.0f, content.c_str());

							for (int i = 0; i < aamp->subscribedTags.size(); i++)
							{
								const std::string& tag = aamp->subscribedTags.at(i);
								if (tag == "#EXT-X-CONTENT-IDENTIFIER") {

									if(reportBulkMeta && init)
									{
										aamp->SaveTimedMetadata(0, tag.c_str(), content.c_str(), content.size());
									}
									else
									{
										aamp->SaveNewTimedMetadata(0, tag.c_str(), content.c_str(), content.size());
									}
									break;
								}
							}
						}
						continue;
					}
				}
				continue;
			}
			if (name == "SupplementalProperty" && node->HasAttribute("schemeIdUri")) {
				const std::string& schemeIdUri = node->GetAttributeValue("schemeIdUri");
#ifdef AAMP_RFC_ENABLED
				const std::string& schemeIdUriDai = RFCSettings::getSchemeIdUriDaiStream();
				if (schemeIdUri == schemeIdUriDai && node->HasAttribute("id")) {
					const std::string& ID = node->GetAttributeValue("id");
					if (ID == "identityADS" && node->HasAttribute("value")) {
						const std::string& content = node->GetAttributeValue("value");

						AAMPLOG_TRACE("TimedMetadata: @%1.3f #EXT-X-IDENTITY-ADS:%s", 0.0f, content.c_str());

						for (int i = 0; i < aamp->subscribedTags.size(); i++)
						{
							const std::string& tag = aamp->subscribedTags.at(i);
							if (tag == "#EXT-X-IDENTITY-ADS") {
								if(reportBulkMeta && init)
								{
									aamp->SaveTimedMetadata(0, tag.c_str(), content.c_str(), content.size());
								}
								else
								{
									aamp->SaveNewTimedMetadata(0, tag.c_str(), content.c_str(), content.size());
								}
								
								break;
							}
						}
						continue;
					}
					if (ID == "messageRef" && node->HasAttribute("value")) {
						const std::string& content = node->GetAttributeValue("value");

						AAMPLOG_TRACE("TimedMetadata: @%1.3f #EXT-X-MESSAGE-REF:%s", 0.0f, content.c_str());

						for (int i = 0; i < aamp->subscribedTags.size(); i++)
						{
							const std::string& tag = aamp->subscribedTags.at(i);
							if (tag == "#EXT-X-MESSAGE-REF") {
								if(reportBulkMeta && init)
								{
									aamp->SaveTimedMetadata(0, tag.c_str(), content.c_str(), content.size());
								}
								else
								{
									aamp->SaveNewTimedMetadata(0, tag.c_str(), content.c_str(), content.size());
								}
								break;
							}
						}
						continue;
					}
				}
#endif
				continue;
			}
		}
		mCdaiObject->PrunePeriodMaps(newPeriods);
	}
	else
	{
		AAMPLOG_WARN("%s:%d :  SubNodes  is empty", __FUNCTION__, __LINE__);  //CID:85449 - Null Returns
	}
}


/**
 * @brief Process supplemental property of a period
 * @param node SupplementalProperty node
 * @param[out] AdID AD Id
 * @param startMS start time in MS
 * @param durationMS duration in MS
 * @param isInit true if its the first playlist download
 * @param reportBulkMeta true if bulk metadata is enabled
 */
void StreamAbstractionAAMP_MPD::ProcessPeriodSupplementalProperty(Node* node, std::string& AdID, uint64_t startMS, uint64_t durationMS, bool isInit, bool reportBulkMeta)
{
	if (node->HasAttribute("schemeIdUri")) {
		const std::string& schemeIdUri = node->GetAttributeValue("schemeIdUri");
		
		if(!schemeIdUri.empty())
		{
#ifdef AAMP_RFC_ENABLED
			const std::string& schemeIdUriDai = RFCSettings::getSchemeIdUriDaiStream();
			if ((schemeIdUri == schemeIdUriDai) && node->HasAttribute("id")) {
				const std::string& ID = node->GetAttributeValue("id");
				if ((ID == "Tracking") && node->HasAttribute("value")) {
					const std::string& value = node->GetAttributeValue("value");
					if (!value.empty()) {
						AdID = value;

						// Check if we need to make AdID a quoted-string
						if (AdID.find(',') != std::string::npos) {
							AdID="\"" + AdID + "\"";
						}

						double duration = durationMS / 1000.0f;
						double start = startMS / 1000.0f;

						std::ostringstream s;
						s << "ID=" << AdID;
						s << ",DURATION=" << std::fixed << std::setprecision(3) << duration;
						s << ",PSN=true";

						std::string content = s.str();
						AAMPLOG_TRACE("TimedMetadata: @%1.3f #EXT-X-CUE:%s", start, content.c_str());

						for (int i = 0; i < aamp->subscribedTags.size(); i++)
						{
							const std::string& tag = aamp->subscribedTags.at(i);
							if (tag == "#EXT-X-CUE") {
								if(reportBulkMeta && isInit)
								{
									aamp->SaveTimedMetadata((long long)startMS, tag.c_str(), content.c_str(), content.size());
								}
								else
								{
									aamp->SaveNewTimedMetadata((long long)startMS, tag.c_str(), content.c_str(), content.size());
								}
								break;
							}
						}
					}
				}
			}
#endif
			if (!AdID.empty() && (schemeIdUri == "urn:scte:scte130-10:2014")) {
				std::vector<Node*> children = node->GetSubNodes();
				for (size_t i=0; i < children.size(); i++) {
					Node* child = children.at(i);
					std::string name;
					std::string ns;
					ParseXmlNS(child->GetName(), ns, name);
					if (name == "StreamRestrictionListType") {
						ProcessStreamRestrictionList(child, AdID, startMS, isInit, reportBulkMeta);
						continue;
					}
					if (name == "StreamRestrictionList") {
						ProcessStreamRestrictionList(child, AdID, startMS, isInit, reportBulkMeta);
						continue;
					}
					if (name == "StreamRestriction") {
						ProcessStreamRestriction(child, AdID, startMS, isInit, reportBulkMeta);
						continue;
					}
				}
			}
		}
		else
		{
			AAMPLOG_WARN("%s:%d : schemeIdUri  is empty", __FUNCTION__, __LINE__);  //CID:83796 - Null Returns
		}
	}
}


/**
 * @brief Process Period AssetIdentifier
 * @param node AssetIdentifier node
 * @param startMS start time MS
 * @param durationMS duration MS
 * @param AssetID Asset Id
 * @param ProviderID Provider Id
 * @param isInit true if its the first playlist download
 * @param reportBulkMeta true if bulk metadata is enabled
 */
void StreamAbstractionAAMP_MPD::ProcessPeriodAssetIdentifier(Node* node, uint64_t startMS, uint64_t durationMS, std::string& AssetID, std::string& ProviderID, bool isInit, bool reportBulkMeta)
{
	if (node->HasAttribute("schemeIdUri")) {
		const std::string& schemeIdUri = node->GetAttributeValue("schemeIdUri");
		if ((schemeIdUri == "urn:cablelabs:md:xsd:content:3.0") && node->HasAttribute("value")) {
			const std::string& value = node->GetAttributeValue("value");
			if (!value.empty()) {
				size_t pos = value.find("/Asset/");
				if (pos != std::string::npos) {
					std::string assetID = value.substr(pos+7);
					std::string providerID = value.substr(0, pos);
					double duration = durationMS / 1000.0f;
					double start = startMS / 1000.0f;

					AssetID = assetID;
					ProviderID = providerID;

					// Check if we need to make assetID a quoted-string
					if (assetID.find(',') != std::string::npos) {
						assetID="\"" + assetID + "\"";
					}

					// Check if we need to make providerID a quoted-string
					if (providerID.find(',') != std::string::npos) {
						providerID="\"" + providerID + "\"";
					}

					std::ostringstream s;
					s << "ID=" << assetID;
					s << ",PROVIDER=" << providerID;
					s << ",DURATION=" << std::fixed << std::setprecision(3) << duration;

					std::string content = s.str();
					AAMPLOG_TRACE("TimedMetadata: @%1.3f #EXT-X-ASSET-ID:%s", start, content.c_str());

					for (int i = 0; i < aamp->subscribedTags.size(); i++)
					{
						const std::string& tag = aamp->subscribedTags.at(i);
						if (tag == "#EXT-X-ASSET-ID") {
							if(reportBulkMeta && isInit)
							{
								aamp->SaveTimedMetadata((long long)startMS, tag.c_str(), content.c_str(), content.size());
							}
							else
							{
								aamp->SaveNewTimedMetadata((long long)startMS, tag.c_str(), content.c_str(), content.size());
							}
							break;
						}
					}
				}
			}
		}
		else if ((schemeIdUri == "urn:scte:dash:asset-id:upid:2015"))
		{
			double start = startMS / 1000.0f;
			std::ostringstream s;
			for(auto childNode : node->GetSubNodes())
			{
				if(childNode->GetAttributeValue("type") == "URI")
				{
					s << "ID=" << "\"" << childNode->GetAttributeValue("value") << "\"";
				}
				else if(childNode->GetAttributeValue("type") == "ADI")
				{
					s << ",SIGNAL=" << "\"" << childNode->GetAttributeValue("value") << "\"";
				}
			}
			std::string content = s.str();
			AAMPLOG_TRACE("TimedMetadata: @%1.3f #EXT-X-SOURCE-STREAM:%s", start, content.c_str());

			for (int i = 0; i < aamp->subscribedTags.size(); i++)
			{
				const std::string& tag = aamp->subscribedTags.at(i);
				if (tag == "#EXT-X-SOURCE-STREAM") {
					if(reportBulkMeta && isInit)
					{
						aamp->SaveTimedMetadata((long long)startMS, tag.c_str(), content.c_str(), content.size());
					}
					else
					{
						aamp->SaveNewTimedMetadata((long long)startMS, tag.c_str(), content.c_str(), content.size());
					}
					break;
				}
			}
		}

	}
}

/**
 *   @brief Process ad event stream.
 *
 *   @param[in] Break start time in milli seconds
 *   @param[in] Period instance.
 */
bool StreamAbstractionAAMP_MPD::ProcessEventStream(uint64_t startMS, IPeriod * period, bool reportBulkMeta)
{
	bool ret = false;
  
	const std::string &prdId = period->GetId();
	if(!prdId.empty())
	{
		uint64_t startMS1 = 0;
		//Vector of pair of scte35 binary data and correspoding duration
		std::vector<std::pair<std::string, uint32_t>> scte35Vec;
		if(isAdbreakStart(period, startMS1, scte35Vec))
		{
			AAMPLOG_WARN("%s:%d Found SCTE35 event for period %s ", __FUNCTION__, __LINE__, prdId.c_str()); 
			for(std::pair<std::string, uint32_t> scte35 : scte35Vec)
			{
				//for livestream send the timedMetadata only., because at init, control does not come here
				if(mIsLiveManifest)
				{
					aamp->FoundSCTE35(prdId, startMS, scte35.second, scte35.first);
				}
				else
				{
					//for vod, send TimedMetadata only when bulkmetadata is not enabled 
					//Control comes here only at init, so no need for an init check for bulkmetadata send
					if(reportBulkMeta)
					{
						AAMPLOG_INFO("%s:%d :  Saving timedMetadata for VOD SCTE35 event for the period, %s", __FUNCTION__, __LINE__, prdId.c_str()); 
						aamp->SaveTimedMetadata(startMS, "SCTE35", scte35.first.c_str(), scte35.first.size(), prdId.c_str(), scte35.second);
					}
					else
					{
						aamp->SaveNewTimedMetadata(startMS, "SCTE35", scte35.first.c_str(), scte35.first.size(), prdId.c_str(), scte35.second);
					}
				}
			}
			ret = true;
		}
	}
	return ret;
}

/**
 * @brief Process Stream restriction list
 * @param node StreamRestrictionListType node
 * @param AdID Ad Id
 * @param startMS start time MS
 * @param isInit true if its the first playlist download
 * @param reportBulkMeta true if bulk metadata is enabled
 */
void StreamAbstractionAAMP_MPD::ProcessStreamRestrictionList(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta)
{
	std::vector<Node*> children = node->GetSubNodes();
	if(!children.empty())
		{
		for (size_t i=0; i < children.size(); i++) {
			Node* child = children.at(i);
			std::string name;
			std::string ns;
			ParseXmlNS(child->GetName(), ns, name);
			if (name == "StreamRestriction") {
				ProcessStreamRestriction(child, AdID, startMS, isInit, reportBulkMeta);
				continue;
			}
		}
	}
	else
	{
		AAMPLOG_WARN("%s:%d :  node is null", __FUNCTION__, __LINE__);  //CID:84081 - Null Returns
	}
}


/**
 * @brief Process stream restriction
 * @param node StreamRestriction xml node
 * @param AdID Ad ID
 * @param startMS Start time in MS
 * @param isInit true if its the first playlist download
 * @param reportBulkMeta true if bulk metadata is enabled
 */
void StreamAbstractionAAMP_MPD::ProcessStreamRestriction(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta)
{
	std::vector<Node*> children = node->GetSubNodes();
	for (size_t i=0; i < children.size(); i++) {
		Node* child = children.at(i);
		if(child != NULL)
			{
			std::string name;
			std::string ns;
			ParseXmlNS(child->GetName(), ns, name);
			if (name == "Ext") {
				ProcessStreamRestrictionExt(child, AdID, startMS, isInit, reportBulkMeta);
				continue;
			}
		}
		else
		{
			AAMPLOG_WARN("%s:%d :  child is null", __FUNCTION__, __LINE__);  //CID:84810 - Null Returns
		}
	}
}


/**
 * @brief Process stream restriction extension
 * @param node Ext child of StreamRestriction xml node
 * @param AdID Ad ID
 * @param startMS start time in ms
 * @param isInit true if its the first playlist download
 * @param reportBulkMeta true if bulk metadata is enabled
 */
void StreamAbstractionAAMP_MPD::ProcessStreamRestrictionExt(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta)
{
	std::vector<Node*> children = node->GetSubNodes();
	for (size_t i=0; i < children.size(); i++) {
		Node* child = children.at(i);
		std::string name;
		std::string ns;
		ParseXmlNS(child->GetName(), ns, name);
		if (name == "TrickModeRestriction") {
			ProcessTrickModeRestriction(child, AdID, startMS, isInit, reportBulkMeta);
			continue;
		}
	}
}


/**
 * @brief Process trick mode restriction
 * @param node TrickModeRestriction xml node
 * @param AdID Ad ID
 * @param startMS start time in ms
 * @param isInit true if its the first playlist download
 * @param reportBulkMeta true if bulk metadata is enabled
 */
void StreamAbstractionAAMP_MPD::ProcessTrickModeRestriction(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta)
{
	double start = startMS / 1000.0f;

	std::string trickMode;
	if (node->HasAttribute("trickMode")) {
		trickMode = node->GetAttributeValue("trickMode");
		if(!trickMode.length())
		{
			AAMPLOG_WARN("%s:%d : trickMode is null", __FUNCTION__, __LINE__);  //CID:86122 - Null Returns
		}
	}

	std::string scale;
	if (node->HasAttribute("scale")) {
		scale = node->GetAttributeValue("scale");
	}

	std::string text = node->GetText();
	if (!trickMode.empty() && !text.empty()) {
		std::ostringstream s;
		s << "ADID=" << AdID
		  << ",MODE=" << trickMode
		  << ",LIMIT=" << text;

		if (!scale.empty()) {
			s << ",SCALE=" << scale;
		}

		std::string content = s.str();
		AAMPLOG_TRACE("TimedMetadata: @%1.3f #EXT-X-TRICKMODE-RESTRICTION:%s", start, content.c_str());

		for (int i = 0; i < aamp->subscribedTags.size(); i++)
		{
			const std::string& tag = aamp->subscribedTags.at(i);
			if (tag == "#EXT-X-TRICKMODE-RESTRICTION") {
				if(reportBulkMeta && isInit)
				{
					aamp->SaveTimedMetadata((long long)startMS, tag.c_str(), content.c_str(), content.size());
				}
				else
				{
					aamp->SaveNewTimedMetadata((long long)startMS, tag.c_str(), content.c_str(), content.size());
				}
				break;
			}
		}
	}
}


/**
 * @brief Fragment downloader thread
 * @param arg HeaderFetchParams pointer
 */
static void * TrackDownloader(void *arg)
{
	class HeaderFetchParams* fetchParms = (class HeaderFetchParams*)arg;
	if(aamp_pthread_setname(pthread_self(), "aampMPDTrackDL"))
	{
		logprintf("%s:%d: aamp_pthread_setname failed", __FUNCTION__, __LINE__);
	}
	//Calling WaitForFreeFragmentAvailable timeout as 0 since waiting for one tracks
	//init header fetch can slow down fragment downloads for other track
	if(fetchParms->pMediaStreamContext->WaitForFreeFragmentAvailable(0))
	{
		fetchParms->pMediaStreamContext->profileChanged = false;
		fetchParms->context->FetchFragment(fetchParms->pMediaStreamContext,
				fetchParms->initialization,
				fetchParms->fragmentduration,
				fetchParms->isinitialization, getCurlInstanceByMediaType(fetchParms->pMediaStreamContext->mediaType), //CurlContext 0=Video, 1=Audio)
				fetchParms->discontinuity);
		fetchParms->pMediaStreamContext->discontinuity = false;
	}
	return NULL;
}


/**
 * @brief Fragment downloader thread
 * @param arg Pointer to FragmentDownloadParams  object
 * @retval NULL
 */
static void * FragmentDownloader(void *arg)
{
	class FragmentDownloadParams* downloadParams = (class FragmentDownloadParams*) arg;
	if(aamp_pthread_setname(pthread_self(), "aampMPDFragDL"))
	{
		logprintf("%s:%d: aamp_pthread_setname failed", __FUNCTION__, __LINE__);
	}
	if (downloadParams->pMediaStreamContext->adaptationSet)
	{
		while (downloadParams->context->aamp->DownloadsAreEnabled() && !downloadParams->pMediaStreamContext->profileChanged)
		{
			int timeoutMs = downloadParams->context->GetMinUpdateDuration() - (int)(aamp_GetCurrentTimeMS() - downloadParams->lastPlaylistUpdateMS);
			if(downloadParams->pMediaStreamContext->WaitForFreeFragmentAvailable(timeoutMs))
			{
				downloadParams->context->PushNextFragment(downloadParams->pMediaStreamContext, (eCURLINSTANCE_VIDEO + downloadParams->pMediaStreamContext->mediaType));
				if (downloadParams->pMediaStreamContext->eos)
				{
					if(!downloadParams->context->aamp->IsLive() && downloadParams->playingLastPeriod)
					{
						downloadParams->pMediaStreamContext->eosReached = true;
						downloadParams->pMediaStreamContext->AbortWaitForCachedAndFreeFragment(false);
					}
					AAMPLOG_INFO("%s:%d %s EOS - Exit fetch loop", __FUNCTION__, __LINE__, downloadParams->pMediaStreamContext->name);
					break;
				}
			}
			timeoutMs = downloadParams->context->GetMinUpdateDuration() - (int)(aamp_GetCurrentTimeMS() - downloadParams->lastPlaylistUpdateMS);
			if(timeoutMs <= 0 && downloadParams->context->aamp->IsLive())
			{
				break;
			}
		}
	}
	else
	{
		logprintf("%s:%d NULL adaptationSet", __FUNCTION__, __LINE__);
	}
	return NULL;
}

/**
 * @brief Check if adaptation set is iframe track
 * @param adaptationSet Pointer to adaptainSet
 * @retval true if iframe track
 */
static bool IsIframeTrack(IAdaptationSet *adaptationSet)
{
	const std::vector<INode *> subnodes = adaptationSet->GetAdditionalSubNodes();
	for (unsigned i = 0; i < subnodes.size(); i++)
	{
		INode *xml = subnodes[i];
		if(xml != NULL)
		{
			if (xml->GetName() == "EssentialProperty")
			{
				if (xml->HasAttribute("schemeIdUri"))
				{
					const std::string& schemeUri = xml->GetAttributeValue("schemeIdUri");
					if (schemeUri == "http://dashif.org/guidelines/trickmode")
					{
						return true;
					}
					else
					{
						logprintf("%s:%d - skipping schemeUri %s", __FUNCTION__, __LINE__, schemeUri.c_str());
					}
				}
			}
			else
			{
				AAMPLOG_TRACE("%s:%d - skipping name %s", __FUNCTION__, __LINE__, xml->GetName().c_str());
			}
		}
		else
		{
			AAMPLOG_WARN("%s:%d :  xml is null", __FUNCTION__, __LINE__);  //CID:81118 - Null Returns
		}
	}
	return false;
}


/**
 * @brief Get the language for an adaptation set
 * @param adaptationSet Pointer to adaptation set
 * @retval language of adaptation set
 */
std::string StreamAbstractionAAMP_MPD::GetLanguageForAdaptationSet(IAdaptationSet *adaptationSet)
{
	std::string lang = adaptationSet->GetLang();
	// If language from adaptation is undefined , retain the current player language
	// Not to change the language .
	
//	if(lang == "und")
//	{
//		lang = aamp->language;
//	}
	lang = Getiso639map_NormalizeLanguageCode(lang,aamp->GetLangCodePreference());
	return lang;
}

/**
 * @brief Get Adaptation Set at given index for the current period
 *
 * @param[in] idx - Adaptation Set Index
 * @retval Adaptation Set at given Index
 */
const IAdaptationSet* StreamAbstractionAAMP_MPD::GetAdaptationSetAtIndex(int idx)
{
	assert(idx < mCurrentPeriod->GetAdaptationSets().size());
	return mCurrentPeriod->GetAdaptationSets().at(idx);
}

/**
 * @brief Get Adaptation Set and Representation Index for given profile
 *
 * @param[in] idx - Profile Index
 * @retval Adaptation Set and Representation Index pair for given profile
 */
struct ProfileInfo StreamAbstractionAAMP_MPD::GetAdaptationSetAndRepresetationIndicesForProfile(int profileIndex)
{
	assert(profileIndex < GetProfileCount());
	return mProfileMaps.at(profileIndex);
}

/**
 * @brief Update language list state variables
 */
void StreamAbstractionAAMP_MPD::UpdateLanguageList()
{
	size_t numPeriods = mpd->GetPeriods().size();
	for (unsigned iPeriod = 0; iPeriod < numPeriods; iPeriod++)
	{
		IPeriod *period = mpd->GetPeriods().at(iPeriod);
		if(period != NULL)
		{
			size_t numAdaptationSets = period->GetAdaptationSets().size();
			for (int iAdaptationSet = 0; iAdaptationSet < numAdaptationSets; iAdaptationSet++)
			{
				IAdaptationSet *adaptationSet = period->GetAdaptationSets().at(iAdaptationSet);
				if(adaptationSet != NULL)
				{
					if (IsContentType(adaptationSet, eMEDIATYPE_AUDIO ))
					{
						mLangList.insert( GetLanguageForAdaptationSet(adaptationSet) );
					}
				}
				else
				{
					AAMPLOG_WARN("%s:%d : adaptationSet is null", __FUNCTION__, __LINE__);  //CID:86317 - Null Returns
				}
			}
		}
		else
		{
			AAMPLOG_WARN("%s:%d :  period is null", __FUNCTION__, __LINE__);  //CID:83754 - Null Returns
		}
	}
	aamp->StoreLanguageList(mLangList);
}

#ifdef AAMP_MPD_DRM
/**
 * @brief Process Early Available License Request
 * @param drmHelper early created drmHelper
 * @param mediaType type of track
 * @param string periodId of EAP
 */
void StreamAbstractionAAMP_MPD::ProcessEAPLicenseRequest()
{
	AAMPLOG_TRACE("%s:%d Processing License request for pending KeyIDs: %d", __FUNCTION__, __LINE__, mPendingKeyIDs.size());
	if(!deferredDRMRequestThreadStarted)
	{
		// wait for thread to complete and create a new thread
		if ((deferredDRMRequestThread!= NULL) && (deferredDRMRequestThread->joinable()))
		{
			deferredDRMRequestThread->join();
			delete deferredDRMRequestThread;
			deferredDRMRequestThread = NULL;
		}

		if(NULL == deferredDRMRequestThread)
		{
			AAMPLOG_INFO("%s:%d New Deferred DRM License thread starting", __FUNCTION__, __LINE__);
			mAbortDeferredLicenseLoop = false;
			deferredDRMRequestThread = new std::thread(&StreamAbstractionAAMP_MPD::StartDeferredDRMRequestThread, this, eMEDIATYPE_VIDEO);
			deferredDRMRequestThreadStarted = true;
		}
	}
	else
	{
		AAMPLOG_TRACE("%s:%d Diferred License Request Thread already running", __FUNCTION__, __LINE__);
	}
}


/**
 * @brief Start Deferred License Request
 * @param drmHelper early created drmHelper
 * @param mediaType type of track
 */
void StreamAbstractionAAMP_MPD::StartDeferredDRMRequestThread(MediaType mediaType)
{
	AAMPLOG_INFO("%s:%d Enter", __FUNCTION__, __LINE__);
	int deferTime;
	bool exitLoop = false;
	// Start tread
	do
	{
		// Wait for KeyID entry in queue
		if(mPendingKeyIDs.empty())
		{
			if(mAbortDeferredLicenseLoop)
			{
				AAMPLOG_ERR("%s aborted", __FUNCTION__);
				break;
			}
			else
			{
				continue;
			}
		}
		else
		{
			deferTime = 0;
		}

		// Process all pending key ID requests
		while(!mPendingKeyIDs.empty())
		{
			std::string keyID = mPendingKeyIDs.front();
			if (mCommonKeyDuration > 0)
			{
				// TODO : Logic to share time for pending Key IDs
				// (mCommonKeyDuration)/(mPendingKeyIds.size())
				deferTime = aamp_GetDeferTimeMs(mCommonKeyDuration);
				// Going to sleep for deferred key process
				aamp->InterruptableMsSleep(deferTime);
				AAMPLOG_TRACE("%s:%d sleep over for deferred time:%d", __FUNCTION__, __LINE__, deferTime);
			}

			if((aamp->DownloadsAreEnabled()) && (!mEarlyAvailableKeyIDMap[keyID].isLicenseFailed))
			{
				AAMPLOG_TRACE("%s:%d Processing License request after sleep", __FUNCTION__, __LINE__);
				// Process content protection with early created helper
				ProcessVssContentProtection(mEarlyAvailableKeyIDMap[keyID].helper, mediaType);
				mEarlyAvailableKeyIDMap[keyID].isLicenseProcessed = true;
			}
			else
			{
				AAMPLOG_ERR("%s Aborted", __FUNCTION__);
				exitLoop = true;
				break;
			}
			// Remove processed keyID from FIFO queue
			mPendingKeyIDs.pop();
		}
	}
	while(!exitLoop);
	deferredDRMRequestThreadStarted = false;
	AAMPLOG_INFO("%s:%d Exit", __FUNCTION__, __LINE__);
}
#endif

int StreamAbstractionAAMP_MPD::GetBestAudioTrackByLanguage( int &desiredRepIdx, AudioType &CodecType)
{
	int bestTrack = -1;
	int bestScore = -1;
	class MediaStreamContext *pMediaStreamContext = mMediaStreamContext[eMEDIATYPE_AUDIO];
	IPeriod *period = mCurrentPeriod;
	size_t numAdaptationSets = period->GetAdaptationSets().size();
	for( int iAdaptationSet = 0; iAdaptationSet < numAdaptationSets; iAdaptationSet++)
	{
		IAdaptationSet *adaptationSet = period->GetAdaptationSets().at(iAdaptationSet);
		if( IsContentType(adaptationSet, eMEDIATYPE_AUDIO) )
		{
			int score = 0;
			std::string trackLanguage = GetLanguageForAdaptationSet(adaptationSet);

			if( aamp->preferredLanguagesList.size() > 0 )
			{
				auto iter = std::find(aamp->preferredLanguagesList.begin(), aamp->preferredLanguagesList.end(), trackLanguage);
				if(iter != aamp->preferredLanguagesList.end())
				{ // track is in preferred language list
					int distance = std::distance(aamp->preferredLanguagesList.begin(),iter);
					score += (aamp->preferredLanguagesList.size()-distance)*10000; // big bonus for language match
				}
			}
			
			if( !aamp->preferredRenditionString.empty() )
			{
				std::vector<IDescriptor *> rendition = adaptationSet->GetRole();
				for (unsigned iRendition = 0; iRendition < rendition.size(); iRendition++)
				{
					if (rendition.at(iRendition)->GetSchemeIdUri().find("urn:mpeg:dash:role:2011") != string::npos)
					{
						std::string trackRendition = rendition.at(iRendition)->GetValue();
						if( aamp->preferredRenditionString.compare(trackRendition)==0 )
						{
							score += 100;
						}
					}
				}
			}

			if( !aamp->preferredTypeString.empty() )
			{
				for( auto iter : adaptationSet->GetAccessibility() )
				{
					if (iter->GetSchemeIdUri().find("urn:mpeg:dash:role:2011") != string::npos)
					{
						std::string trackType = iter->GetValue();
						if (aamp->preferredTypeString.compare(trackType)==0 )
						{
							score += 10;
						}
					}
				}
			}
			
			AudioType selectedCodecType = eAUDIO_UNKNOWN;
			uint32_t selRepBandwidth = 0;
			bool disableATMOS = ISCONFIGSET(eAAMPConfig_DisableATMOS);
			bool disableEC3 = ISCONFIGSET(eAAMPConfig_DisableEC3);
			int audioRepresentationIndex = GetDesiredCodecIndex(adaptationSet, selectedCodecType, selRepBandwidth,disableEC3 , disableATMOS);
			switch( selectedCodecType )
			{
				case eAUDIO_ATMOS:
					if( !disableATMOS )
					{
						score += 6;
					}
					break;
					
				case eAUDIO_DDPLUS:
					if( !disableEC3 )
					{
						score += 4;
					}
					break;
					
				case eAUDIO_AAC:
					score += 2;
					break;
					
				case eAUDIO_UNKNOWN:
					score += 1;
					break;
					
				default:
					break;
			}
			AAMPLOG_INFO( "track#%d score = %d\n", iAdaptationSet, score );
			if( score > bestScore )
			{
				bestScore = score;
				bestTrack = iAdaptationSet;
				desiredRepIdx = audioRepresentationIndex;
				CodecType = selectedCodecType;
			}
		} // IsContentType(adaptationSet, eMEDIATYPE_AUDIO)
	} // next iAdaptationSet
	return bestTrack;
}

void StreamAbstractionAAMP_MPD::StartSubtitleParser()
{
	struct MediaStreamContext *subtitle = mMediaStreamContext[eMEDIATYPE_SUBTITLE];
	if (subtitle && subtitle->enabled && subtitle->mSubtitleParser)
	{
		if(aamp->IsLive())
		{ // adjust subtitle presentation start to align with AV live offset
			seekPosition -= aamp->mLiveOffset;
		}
		AAMPLOG_INFO("%s: sending init %.3f", __FUNCTION__, seekPosition);
		subtitle->mSubtitleParser->init(seekPosition, 0);
		subtitle->mSubtitleParser->mute(aamp->subtitles_muted);
		subtitle->mSubtitleParser->isLinear(mIsLiveStream);
	}
}

/**
 * @brief Set subtitle pause state
 *
 */
void StreamAbstractionAAMP_MPD::PauseSubtitleParser(bool pause)
{
	struct MediaStreamContext *subtitle = mMediaStreamContext[eMEDIATYPE_SUBTITLE];
	if (subtitle && subtitle->enabled && subtitle->mSubtitleParser)
	{
		AAMPLOG_INFO("%s: setting subtitles pause state = %d", __FUNCTION__, pause);
		subtitle->mSubtitleParser->pause(pause);
	}
}


/**
 * @brief Does stream selection
 * @param newTune true if this is a new tune
 */
void StreamAbstractionAAMP_MPD::StreamSelection( bool newTune, bool forceSpeedsChangedEvent)
{
	std::vector<AudioTrackInfo> aTracks;
	std::vector<TextTrackInfo> tTracks;
	std::string aTrackIdx;
	std::string tTrackIdx;
	mNumberOfTracks = 0;
	IPeriod *period = mCurrentPeriod;
	if(!period)
	{
		AAMPLOG_WARN("%s:%d :  period is null", __FUNCTION__, __LINE__);  //CID:84742 - Null Returns
	}
	AAMPLOG_INFO("Selected Period index %d, id %s", mCurrentPeriodIdx, period->GetId().c_str());
	for( int i = 0; i < mMaxTracks; i++ )
	{
		mMediaStreamContext[i]->enabled = false;
	}
	AudioType selectedCodecType = eAUDIO_UNKNOWN;
	int audioRepresentationIndex = -1;
	int desiredRepIdx = -1;
	int audioAdaptationSetIndex = -1;

	if (rate == AAMP_NORMAL_PLAY_RATE)
	{
		audioAdaptationSetIndex = GetBestAudioTrackByLanguage(desiredRepIdx,selectedCodecType);
		IAdaptationSet *audioAdaptationSet = NULL;
		if ( audioAdaptationSetIndex >= 0 )
		{
			audioAdaptationSet = period->GetAdaptationSets().at(audioAdaptationSetIndex);
		}
		
		if( audioAdaptationSet )
		{
			std::string lang = GetLanguageForAdaptationSet(audioAdaptationSet);
			// aamp->UpdateAudioLanguageSelection( lang.c_str(), true);
			if(desiredRepIdx != -1 )
			{
				audioRepresentationIndex = desiredRepIdx;
				mAudioType = selectedCodecType;
			}
			logprintf("StreamAbstractionAAMP_MPD::%s %d > lang[%s] AudioType[%d]", __FUNCTION__, __LINE__, lang.c_str(), selectedCodecType);
		}
		else
		{
			logprintf("StreamAbstractionAAMP_MPD::%s %d Unable to get audioAdaptationSet.", __FUNCTION__, __LINE__);
		}
	}

	for (int i = 0; i < mMaxTracks; i++)
	{
		class MediaStreamContext *pMediaStreamContext = mMediaStreamContext[i];
		size_t numAdaptationSets = period->GetAdaptationSets().size();
		int  selAdaptationSetIndex = -1;
		int selRepresentationIndex = -1;
		bool isIframeAdaptationAvailable = false;
		int videoRepresentationIdx;   //CID:118900 - selRepBandwidth variable locally declared but not reflected
		for (unsigned iAdaptationSet = 0; iAdaptationSet < numAdaptationSets; iAdaptationSet++)
		{
			IAdaptationSet *adaptationSet = period->GetAdaptationSets().at(iAdaptationSet);
			AAMPLOG_TRACE("StreamAbstractionAAMP_MPD::%s %d > Content type [%s] AdapSet [%d] ",
					__FUNCTION__, __LINE__, adaptationSet->GetContentType().c_str(), iAdaptationSet);
			if (IsContentType(adaptationSet, (MediaType)i ))
			{
				if (AAMP_NORMAL_PLAY_RATE == rate)
				{
					if (eMEDIATYPE_SUBTITLE == i && selAdaptationSetIndex == -1)
					{
						AAMPLOG_INFO("%s:%d Checking subs - mime %s lang %s selAdaptationSetIndex %d",
							__FUNCTION__, __LINE__, adaptationSet->GetMimeType().c_str(), GetLanguageForAdaptationSet(adaptationSet).c_str(), selAdaptationSetIndex);
						// TTML selection as follows:
						// 1. Text track as set from SetTextTrack API (this is confusingly named preferredTextTrack, even though it's explicitly set)
						// 2. The *actual* preferred text track, as set through the SetPreferredSubtitleLanguage API
						// 3. Not set
						const auto selectedTextTrack = aamp->GetPreferredTextTrack();

						if (!selectedTextTrack.index.empty())
						{
							if (IsMatchingLanguageAndMimeType((MediaType)i, selectedTextTrack.language, adaptationSet, selRepresentationIndex))
							{
								auto adapSetName = (adaptationSet->GetRepresentation().at(selRepresentationIndex))->GetId();
								AAMPLOG_INFO("%s:%d adapSet Id %s selName %s", __FUNCTION__, __LINE__, adapSetName.c_str(), selectedTextTrack.name.c_str());
								if (adapSetName.empty() || adapSetName == selectedTextTrack.name)
								{
									selAdaptationSetIndex = iAdaptationSet;
								}
							}
						}
						else if (IsMatchingLanguageAndMimeType((MediaType)i, aamp->mSubLanguage, adaptationSet, selRepresentationIndex) == true)
						{
							AAMPLOG_INFO("%s:%d matched default sub language %s [%d]", __FUNCTION__, __LINE__, aamp->mSubLanguage.c_str(), iAdaptationSet);
							selAdaptationSetIndex = iAdaptationSet;
						}

						if (selAdaptationSetIndex != -1)
						{
							std::string mimeType = adaptationSet->GetMimeType();
							if (mimeType.empty())
							{
								if (pMediaStreamContext->mSubtitleParser->init(0.0, 0))
								{
									pMediaStreamContext->mSubtitleParser->mute(aamp->subtitles_muted);
								}
								else
								{
									pMediaStreamContext->mSubtitleParser.reset(nullptr); 
									pMediaStreamContext->mSubtitleParser = NULL;
								}
							}
							tTrackIdx = std::to_string(selAdaptationSetIndex) + "-" + std::to_string(selRepresentationIndex);
							pMediaStreamContext->mSubtitleParser = SubtecFactory::createSubtitleParser(aamp, mimeType);
							if (!pMediaStreamContext->mSubtitleParser)
							{
								pMediaStreamContext->enabled = false;
								selAdaptationSetIndex = -1;
							}
							aamp->StopTrackDownloads(eMEDIATYPE_SUBTITLE);
						}
					}
					else if (eMEDIATYPE_AUX_AUDIO == i && aamp->IsAuxiliaryAudioEnabled())
					{
						if (aamp->GetAuxiliaryAudioLanguage() == aamp->mAudioTuple.language)
						{
							AAMPLOG_WARN("PrivateStreamAbstractionMPD::%s %d > auxiliary audio same as primary audio, set forward audio flag", __FUNCTION__, __LINE__);
							SetAudioFwdToAuxStatus(true);
							break;
						}
						else if (IsMatchingLanguageAndMimeType((MediaType)i, aamp->GetAuxiliaryAudioLanguage(), adaptationSet, selRepresentationIndex) == true)
						{
							selAdaptationSetIndex = iAdaptationSet;
						}
					}
					else if (eMEDIATYPE_AUDIO == i)
					{
						selAdaptationSetIndex = audioAdaptationSetIndex;
						selRepresentationIndex = audioRepresentationIndex;
						aTrackIdx = std::to_string(selAdaptationSetIndex) + "-" + std::to_string(selRepresentationIndex);
					}
					else if (eMEDIATYPE_VIDEO == i && !ISCONFIGSET(eAAMPConfig_AudioOnlyPlayback))
					{
						if (!isIframeAdaptationAvailable || selAdaptationSetIndex == -1)
						{
							if (!IsIframeTrack(adaptationSet))
							{
								// Got Video , confirmed its not iframe adaptation
								videoRepresentationIdx = GetDesiredVideoCodecIndex(adaptationSet);
								if (videoRepresentationIdx != -1)
								{
									selAdaptationSetIndex = iAdaptationSet;
								}
								if(!newTune)
								{
									if(GetProfileCount() == adaptationSet->GetRepresentation().size())
									{
										selRepresentationIndex = pMediaStreamContext->representationIndex;
									}
									else
									{
										selRepresentationIndex = -1; // this will be set based on profile selection
									}
								}
							}
							else
							{
								isIframeAdaptationAvailable = true;
							}
						}
						else
						{
							break;
						}
					}
				}
				else if ((!ISCONFIGSET(eAAMPConfig_AudioOnlyPlayback)) && (eMEDIATYPE_VIDEO == i))
				{
					//iframe track
					if ( IsIframeTrack(adaptationSet) )
					{
						logprintf("StreamAbstractionAAMP_MPD::%s %d > Got TrickMode track", __FUNCTION__, __LINE__);
						pMediaStreamContext->enabled = true;
						pMediaStreamContext->profileChanged = true;
						pMediaStreamContext->adaptationSetIdx = iAdaptationSet;
						mNumberOfTracks = 1;
						isIframeAdaptationAvailable = true;
						break;
					}
				}

				//Populate audio and text track info
				if (eMEDIATYPE_AUDIO == i || eMEDIATYPE_SUBTITLE == i)
				{
					IAdaptationSet *adaptationSet = period->GetAdaptationSets().at(iAdaptationSet);
					std::string lang = GetLanguageForAdaptationSet(adaptationSet);
					const std::vector<IRepresentation *> representation = adaptationSet->GetRepresentation();
					std::string codec;
					std::string group; // value of <Role>, empty if <Role> not present
					std::string type; // value of <Accessibility>
					std::string empty;
					{
						std::vector<IDescriptor *> role = adaptationSet->GetRole();
						for (unsigned iRole = 0; iRole < role.size(); iRole++)
						{
							if (role.at(iRole)->GetSchemeIdUri().find("urn:mpeg:dash:role:2011") != string::npos)
							{
								if (!group.empty())
								{
									group += ",";
								}
								group += role.at(iRole)->GetValue();
							}
						}
					}

					for( auto iter : adaptationSet->GetAccessibility() )
					{
						if (iter->GetSchemeIdUri().find("urn:mpeg:dash:role:2011") != string::npos)
						{	
							if (!type.empty())
							{
								type += ",";
							}
							type += iter->GetValue();
						}
					}

					// check for codec defined in Adaptation Set
					const std::vector<string> adapCodecs = adaptationSet->GetCodecs();
					const std::string adapMimeType = adaptationSet->GetMimeType();
					for (int representationIndex = 0; representationIndex < representation.size(); representationIndex++)
					{
						std::string index = std::to_string(iAdaptationSet) + "-" + std::to_string(representationIndex);
						const dash::mpd::IRepresentation *rep = representation.at(representationIndex);
						std::string name = rep->GetId();
						uint32_t bandwidth = rep->GetBandwidth();
						const std::vector<std::string> repCodecs = rep->GetCodecs();
						// check if Representation includes codec
						if (repCodecs.size())
						{
							codec = repCodecs.at(0);
						}
						else if (adapCodecs.size()) // else check if Adaptation has codec
						{
							codec = adapCodecs.at(0);
						}
						else
						{
							// For subtitle, it might be vtt/ttml format
							codec = adaptationSet->GetMimeType();
						}

						if (eMEDIATYPE_AUDIO == i)
						{
							AAMPLOG_WARN("StreamAbstractionAAMP_MPD::%s() %d Audio Track - lang:%s, group:%s, name:%s, codec:%s, bandwidth:%d, type:%s",
								__FUNCTION__, __LINE__, lang.c_str(), group.c_str(), name.c_str(), codec.c_str(), bandwidth, type.c_str());
							aTracks.push_back(AudioTrackInfo(index, lang, group, name, codec, bandwidth, type));
						}
						else
						{
							AAMPLOG_WARN("StreamAbstractionAAMP_MPD::%s() %d Text Track - lang:%s, isCC:0, group:%s, name:%s, codec:%s, type:%s",
								__FUNCTION__, __LINE__, lang.c_str(), group.c_str(), name.c_str(), codec.c_str(), type.c_str());
							tTracks.push_back(TextTrackInfo(index, lang, false, group, name, codec, empty, type));
						}
					}
				}
				// Look in VIDEO adaptation for inband CC track related info
				else if (eMEDIATYPE_VIDEO == i)
				{
					IAdaptationSet *adaptationSet = period->GetAdaptationSets().at(iAdaptationSet);
					std::vector<IDescriptor *> adapAccessibility = adaptationSet->GetAccessibility();
					for (int index = 0 ; index < adapAccessibility.size(); index++)
					{
						std::string schemeId = adapAccessibility.at(index)->GetSchemeIdUri();
						if (schemeId.find("urn:scte:dash:cc:cea") != string::npos)
						{
							std::string empty;
							std::string lang, id;
							std::string value = adapAccessibility.at(index)->GetValue();
							if (!value.empty())
							{
								// Expected formats : 	eng;deu
								// 			CC1=eng;CC2=deu
								//			1=lang:eng;2=lang:deu
								//			1=lang:eng;2=lang:eng,war:1,er:1
								size_t delim = value.find(';');
								while (delim != std::string::npos)
								{
									ParseCCStreamIDAndLang(value.substr(0, delim), id, lang);
									AAMPLOG_WARN("StreamAbstractionAAMP_MPD::%s() %d CC Track - lang:%s, isCC:1, group:%s, id:%s",
										__FUNCTION__, __LINE__, lang.c_str(), schemeId.c_str(), id.c_str());
									tTracks.push_back(TextTrackInfo(empty, lang, true, schemeId, empty, id, empty));
									value = value.substr(delim + 1);
									delim = value.find(';');
								}
								ParseCCStreamIDAndLang(value, id, lang);
								lang = Getiso639map_NormalizeLanguageCode(lang,aamp->GetLangCodePreference());
								AAMPLOG_WARN("StreamAbstractionAAMP_MPD::%s() %d CC Track - lang:%s, isCC:1, group:%s, id:%s",
									__FUNCTION__, __LINE__, lang.c_str(), schemeId.c_str(), id.c_str());
								tTracks.push_back(TextTrackInfo(empty, lang, true, schemeId, empty, id, empty));
							}
							else
							{
								// value = empty is highly discouraged as per spec, just added as fallback
								AAMPLOG_WARN("StreamAbstractionAAMP_MPD::%s() %d CC Track - group:%s, isCC:1", __FUNCTION__, __LINE__, schemeId.c_str());
								tTracks.push_back(TextTrackInfo(empty, empty, true, schemeId, empty, empty, empty));
							}
						}
					}
				}
			}
		} // next iAdaptationSet

		if ((eAUDIO_UNKNOWN == mAudioType) && (AAMP_NORMAL_PLAY_RATE == rate) && (eMEDIATYPE_VIDEO != i) && selAdaptationSetIndex >= 0)
		{
			AAMPLOG_WARN("StreamAbstractionAAMP_MPD::%s %d > Selected Audio Track codec is unknown", __FUNCTION__, __LINE__);
			mAudioType = eAUDIO_AAC; // assuming content is still playable
		}

		// end of adaptation loop
		if ((AAMP_NORMAL_PLAY_RATE == rate) && (pMediaStreamContext->enabled == false) && selAdaptationSetIndex >= 0)
		{
			pMediaStreamContext->enabled = true;
			pMediaStreamContext->adaptationSetIdx = selAdaptationSetIndex;
			pMediaStreamContext->representationIndex = selRepresentationIndex;
			pMediaStreamContext->profileChanged = true;
			
			/* To solve a no audio issue - Force configure gst audio pipeline/playbin in the case of multi period
			 * multi audio codec available for current decoding language on stream. For example, first period has AAC no EC3,
			 * so the player will choose AAC then start decoding, but in the forthcoming periods,
			 * if the stream has AAC and EC3 for the current decoding language then as per the EC3(default priority)
			 * the player will choose EC3 but the audio pipeline actually not configured in this case to affect this change.
			 */
			if ( aamp->previousAudioType != selectedCodecType && eMEDIATYPE_AUDIO == i )
			{
				logprintf("StreamAbstractionAAMP_MPD::%s %d > AudioType Changed %d -> %d",
						__FUNCTION__, __LINE__, aamp->previousAudioType, selectedCodecType);
				aamp->previousAudioType = selectedCodecType;
				SetESChangeStatus();
			}
			logprintf("StreamAbstractionAAMP_MPD::%s %d > Media[%s] Adaptation set[%d] RepIdx[%d] TrackCnt[%d]",
				__FUNCTION__, __LINE__, getMediaTypeName(MediaType(i)),selAdaptationSetIndex,selRepresentationIndex,(mNumberOfTracks+1) );

			ProcessContentProtection(period->GetAdaptationSets().at(selAdaptationSetIndex),(MediaType)i);
			mNumberOfTracks++;
		} // AAMP_NORMAL_PLAY_RATE

		if(selAdaptationSetIndex < 0 && rate == 1)
		{
			logprintf("StreamAbstractionAAMP_MPD::%s %d > No valid adaptation set found for Media[%s]",
				__FUNCTION__, __LINE__, getMediaTypeName(MediaType(i)));
		}

		logprintf("StreamAbstractionAAMP_MPD::%s %d > Media[%s] %s",
			__FUNCTION__, __LINE__, getMediaTypeName(MediaType(i)), pMediaStreamContext->enabled?"enabled":"disabled");

		//RDK-27796, we need this hack for cases where subtitle is not enabled, but auxiliary audio track is enabled
		if (eMEDIATYPE_AUX_AUDIO == i && pMediaStreamContext->enabled && !mMediaStreamContext[eMEDIATYPE_SUBTITLE]->enabled)
		{
			AAMPLOG_WARN("PrivateStreamAbstractionMPD::%s:%d Auxiliary enabled, but subtitle disabled, swap MediaStreamContext of both", __FUNCTION__, __LINE__);
			mMediaStreamContext[eMEDIATYPE_SUBTITLE]->enabled = mMediaStreamContext[eMEDIATYPE_AUX_AUDIO]->enabled;
			mMediaStreamContext[eMEDIATYPE_SUBTITLE]->adaptationSetIdx = mMediaStreamContext[eMEDIATYPE_AUX_AUDIO]->adaptationSetIdx;
			mMediaStreamContext[eMEDIATYPE_SUBTITLE]->representationIndex = mMediaStreamContext[eMEDIATYPE_AUX_AUDIO]->representationIndex;
			mMediaStreamContext[eMEDIATYPE_SUBTITLE]->mediaType = eMEDIATYPE_AUX_AUDIO;
			mMediaStreamContext[eMEDIATYPE_SUBTITLE]->type = eTRACK_AUX_AUDIO;
			mMediaStreamContext[eMEDIATYPE_SUBTITLE]->profileChanged = true;
			mMediaStreamContext[eMEDIATYPE_AUX_AUDIO]->enabled = false;
		}

		//Store the iframe track status in current period if there is any change
		if (!ISCONFIGSET(eAAMPConfig_AudioOnlyPlayback) && (i == eMEDIATYPE_VIDEO) && (aamp->mIsIframeTrackPresent != isIframeAdaptationAvailable))
		{
			aamp->mIsIframeTrackPresent = isIframeAdaptationAvailable;
			//Iframe tracks changed mid-stream, sent a playbackspeed changed event
			if (!newTune || forceSpeedsChangedEvent)
			{
				aamp->SendSupportedSpeedsChangedEvent(aamp->mIsIframeTrackPresent);
			}
		}
	} // next track

	if(1 == mNumberOfTracks && !mMediaStreamContext[eMEDIATYPE_VIDEO]->enabled)
	{ // what about audio+subtitles?
		if(newTune)
			logprintf("StreamAbstractionAAMP_MPD::%s:%d Audio only period", __FUNCTION__, __LINE__);
		mMediaStreamContext[eMEDIATYPE_VIDEO]->enabled = mMediaStreamContext[eMEDIATYPE_AUDIO]->enabled;
		mMediaStreamContext[eMEDIATYPE_VIDEO]->adaptationSetIdx = mMediaStreamContext[eMEDIATYPE_AUDIO]->adaptationSetIdx;
		mMediaStreamContext[eMEDIATYPE_VIDEO]->representationIndex = mMediaStreamContext[eMEDIATYPE_AUDIO]->representationIndex;
		mMediaStreamContext[eMEDIATYPE_VIDEO]->mediaType = eMEDIATYPE_VIDEO;
		mMediaStreamContext[eMEDIATYPE_VIDEO]->type = eTRACK_VIDEO;
		mMediaStreamContext[eMEDIATYPE_VIDEO]->profileChanged = true;
		mMediaStreamContext[eMEDIATYPE_AUDIO]->enabled = false;
	}

	// Set audio/text track related structures
	SetAudioTrackInfo(aTracks, aTrackIdx);
	SetTextTrackInfo(tTracks, tTrackIdx);
}

/**
 * @brief Extract bitrate info from custom mpd
 * @note Caller function should delete the vector elements after use.
 * @param adaptationSet : AdaptaionSet from which bitrate info is to be extracted
 * @param[out] representations : Representation vector gets updated with Available bit rates.
 */
static void GetBitrateInfoFromCustomMpd( const IAdaptationSet *adaptationSet, std::vector<Representation *>& representations )
{
	std::vector<xml::INode *> subNodes = adaptationSet->GetAdditionalSubNodes();
	for(int i = 0; i < subNodes.size(); i ++)
	{
		xml::INode * node = subNodes.at(i);
		if(node != NULL)
		{
			if(node->GetName() == "AvailableBitrates")
			{
				std::vector<xml::INode *> reprNodes = node->GetNodes();
				for(int reprIter = 0; reprIter < reprNodes.size(); reprIter++)
				{
					xml::INode * reprNode = reprNodes.at(reprIter);
					if(reprNode != NULL)
					{
						if(reprNode->GetName() == "Representation")
						{
							dash::mpd::Representation * repr = new dash::mpd::Representation();
							if(reprNode->HasAttribute("bandwidth"))
							{
								repr->SetBandwidth(stol(reprNode->GetAttributeValue("bandwidth")));
							}
							if(reprNode->HasAttribute("height"))
							{
								repr->SetHeight(stol(reprNode->GetAttributeValue("height")));
							}
							if(reprNode->HasAttribute("width"))
							{
								repr->SetWidth(stol(reprNode->GetAttributeValue("width")));
							}
							representations.push_back(repr);
						}
					}
					else
					{
						AAMPLOG_WARN("%s:%d :  reprNode  is null", __FUNCTION__, __LINE__);  //CID:85171 - Null Returns
					}
				}
				break;
			}
		}
		else
		{
			AAMPLOG_WARN("%s:%d :  node is null", __FUNCTION__, __LINE__);  //CID:81731 - Null Returns
		}
	}
}

/**
 * @brief Get profile index for bandwidth notification
 * @param bandwidth - bandwidth to identify profile index from list
 * @retval profile index of the current bandwidth
 */
int StreamAbstractionAAMP_MPD::GetProfileIdxForBandwidthNotification(uint32_t bandwidth)
{
	int profileIndex = 0; // Keep default index as 0

	std::vector<long>::iterator it = std::find(mBitrateIndexVector.begin(), mBitrateIndexVector.end(), (long)bandwidth);

	if (it != mBitrateIndexVector.end())
	{
		// Get index of element from iterator
		profileIndex = std::distance(mBitrateIndexVector.begin(), it);
	}

	return profileIndex;
}

/**
 * @brief Updates track information based on current state
 */
AAMPStatusType StreamAbstractionAAMP_MPD::UpdateTrackInfo(bool modifyDefaultBW, bool periodChanged, bool resetTimeLineIndex)
{
	AAMPStatusType ret = eAAMPSTATUS_OK;
	long defaultBitrate = aamp->GetDefaultBitrate();
	long iframeBitrate = aamp->GetIframeBitrate();
	bool isFogTsb = mIsFogTSB && !mAdPlayingFromCDN;	/*Conveys whether the current playback from FOG or not.*/
	long minBitrate = aamp->GetMinimumBitrate();
	long maxBitrate = aamp->GetMaximumBitrate();
	if(periodChanged)
	{
				// sometimes when period changes, period in manifest is empty hence mark it for later use when period gets filled with data.
		mUpdateStreamInfo = true;
	}

	for (int i = 0; i < mNumberOfTracks; i++)
	{
		class MediaStreamContext *pMediaStreamContext = mMediaStreamContext[i];
		if(!pMediaStreamContext)
		{
			AAMPLOG_WARN("%s:%d :  pMediaStreamContext  is null", __FUNCTION__, __LINE__);  //CID:82464,84186 - Null Returns
		}
		if(mCdaiObject->mAdState == AdState::IN_ADBREAK_AD_PLAYING)
		{
			AdNode &adNode = mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx);
			if(adNode.mpd != NULL)
			{
				pMediaStreamContext->fragmentDescriptor.manifestUrl = adNode.url.c_str();
			}
		}
		if(pMediaStreamContext->enabled)
		{
			IPeriod *period = mCurrentPeriod;
			pMediaStreamContext->adaptationSet = period->GetAdaptationSets().at(pMediaStreamContext->adaptationSetIdx);
			pMediaStreamContext->adaptationSetId = pMediaStreamContext->adaptationSet->GetId();
			std::string adapFrameRate = pMediaStreamContext->adaptationSet->GetFrameRate();
			/*Populate StreamInfo for ABR Processing*/
			if (i == eMEDIATYPE_VIDEO)
			{
				if(isFogTsb && mUpdateStreamInfo)
				{
					mUpdateStreamInfo = false;
					vector<Representation *> representations;
					GetBitrateInfoFromCustomMpd(pMediaStreamContext->adaptationSet, representations);
					int representationCount = representations.size();
					if ((representationCount != mBitrateIndexVector.size()) && mStreamInfo)
					{
						delete[] mStreamInfo;
						mStreamInfo = NULL;
					}
					if (!mStreamInfo)
					{
						mStreamInfo = new StreamInfo[representationCount];
					}
					GetABRManager().clearProfiles();
					mBitrateIndexVector.clear();
					mMaxTSBBandwidth = 0;
					for (int idx = 0; idx < representationCount; idx++)
					{
						Representation* representation = representations.at(idx);
						if(representation != NULL)
						{
							mStreamInfo[idx].bandwidthBitsPerSecond = representation->GetBandwidth();
							mStreamInfo[idx].isIframeTrack = !(AAMP_NORMAL_PLAY_RATE == rate);
							mStreamInfo[idx].resolution.height = representation->GetHeight();
							mStreamInfo[idx].resolution.width = representation->GetWidth();
							mStreamInfo[idx].resolution.framerate = 0;
							mStreamInfo[idx].enabled = false;
							//Update profile resolution with VideoEnd Metrics object.
							aamp->UpdateVideoEndProfileResolution((mStreamInfo[idx].isIframeTrack ? eMEDIATYPE_IFRAME : eMEDIATYPE_VIDEO ),
													mStreamInfo[idx].bandwidthBitsPerSecond,
													mStreamInfo[idx].resolution.width,
													mStreamInfo[idx].resolution.height);

							std::string repFrameRate = representation->GetFrameRate();
							if(repFrameRate.empty())
								repFrameRate = adapFrameRate;
							if(!repFrameRate.empty())
							{
								int val1, val2;
								sscanf(repFrameRate.c_str(),"%d/%d",&val1,&val2);
								double frate = val2? ((double)val1/val2):val1;
								mStreamInfo[idx].resolution.framerate = frate;
							}

							delete representation;
							// Skip profile by resolution, if profile capping already applied in Fog
							if(ISCONFIGSET(eAAMPConfig_LimitResolution) && aamp->mProfileCappedStatus &&  aamp->mDisplayWidth > 0 && mStreamInfo[idx].resolution.width > aamp->mDisplayWidth)
							{
								AAMPLOG_INFO ("Video Profile Ignoring resolution=%d:%d display=%d:%d Bw=%ld", mStreamInfo[idx].resolution.width, mStreamInfo[idx].resolution.height, aamp->mDisplayWidth, aamp->mDisplayHeight, mStreamInfo[idx].bandwidthBitsPerSecond);
								continue;
							}
							mBitrateIndexVector.push_back(mStreamInfo[idx].bandwidthBitsPerSecond);
							if(mStreamInfo[idx].bandwidthBitsPerSecond > mMaxTSBBandwidth)
							{
								mMaxTSBBandwidth = mStreamInfo[idx].bandwidthBitsPerSecond;
							}
						}
						else
						{
							AAMPLOG_WARN("%s:%d :  representation  is null", __FUNCTION__, __LINE__);  //CID:82489 - Null Returns
						}
					}
					pMediaStreamContext->representationIndex = 0; //Fog custom mpd has a single representation
					IRepresentation* representation = pMediaStreamContext->adaptationSet->GetRepresentation().at(0);
					pMediaStreamContext->fragmentDescriptor.Bandwidth = representation->GetBandwidth();
					aamp->profiler.SetBandwidthBitsPerSecondVideo(pMediaStreamContext->fragmentDescriptor.Bandwidth);
					profileIdxForBandwidthNotification = GetProfileIdxForBandwidthNotification(pMediaStreamContext->fragmentDescriptor.Bandwidth);
				}
				else if(!isFogTsb && mUpdateStreamInfo)
				{
					mUpdateStreamInfo = false;
					int representationCount = 0;
					for (const auto &adaptationSet: period->GetAdaptationSets())
					{
						if (IsContentType(adaptationSet, eMEDIATYPE_VIDEO))
						{
							if (IsIframeTrack(adaptationSet))
							{
								if (trickplayMode)
								{
									// count iframe representations for allocating streamInfo.
									representationCount += adaptationSet->GetRepresentation().size();
								}
							}
							else
							{
								if (!trickplayMode)
								{
									// count normal video representations for allocating streamInfo.
									representationCount += adaptationSet->GetRepresentation().size();
								}
							}
						}
					}
                                        const std::vector<IAdaptationSet *> adaptationSets= period->GetAdaptationSets();
                                        if(adaptationSets.size() > 0)
					{
                                             IAdaptationSet* adaptationSet = adaptationSets.at(0);
                			     if ((mNumberOfTracks == 1) && (IsContentType(adaptationSet, eMEDIATYPE_AUDIO)))
             				     {
                                                 representationCount += adaptationSet->GetRepresentation().size();
	                        	     }
                                       	}	
					if ((representationCount != GetProfileCount()) && mStreamInfo)
					{
						delete[] mStreamInfo;
						mStreamInfo = NULL;
						// reset representationIndex to -1 to allow updating the currentProfileIndex for period change.
						pMediaStreamContext->representationIndex = -1;
						AAMPLOG_WARN("%s:%d representationIndex set to (-1) to find currentProfileIndex", __FUNCTION__, __LINE__);
					}
					if (!mStreamInfo)
					{
						mStreamInfo = new StreamInfo[representationCount];
					}
					GetABRManager().clearProfiles();
					mBitrateIndexVector.clear();
					int addedProfiles = 0;
					size_t idx = 0;
					std::map<int, struct ProfileInfo> iProfileMaps;
					bool resolutionCheckEnabled = false;
					bool bVideoCapped = false;
					bool bIframeCapped = false;

					for (size_t adaptIdx = 0; adaptIdx < adaptationSets.size(); adaptIdx++)
					{
						IAdaptationSet* adaptationSet = adaptationSets.at(adaptIdx);
						if (IsContentType(adaptationSet, eMEDIATYPE_VIDEO))
						{
							if( IsIframeTrack(adaptationSet) )
							{
								if( !trickplayMode )
								{ // avoid using iframe profiles during normal playback
									continue;
								}
							}
							else
							{
								if( trickplayMode )
								{ // avoid using normal video tracks during trickplay
									continue;
								}
							}

							size_t numRepresentations = adaptationSet->GetRepresentation().size();
							for (size_t reprIdx = 0; reprIdx < numRepresentations; reprIdx++)
							{
								IRepresentation *representation = adaptationSet->GetRepresentation().at(reprIdx);
								mStreamInfo[idx].bandwidthBitsPerSecond = representation->GetBandwidth();
								mStreamInfo[idx].isIframeTrack = !(AAMP_NORMAL_PLAY_RATE == rate);
								mStreamInfo[idx].resolution.height = representation->GetHeight();
								mStreamInfo[idx].resolution.width = representation->GetWidth();
								mStreamInfo[idx].resolution.framerate = 0;
								std::string repFrameRate = representation->GetFrameRate();
								mStreamInfo[idx].enabled = false;
								if(repFrameRate.empty())
									repFrameRate = adapFrameRate;
								if(!repFrameRate.empty())
								{
									int val1, val2;
									sscanf(repFrameRate.c_str(),"%d/%d",&val1,&val2);
									double frate = val2? ((double)val1/val2):val1;
									mStreamInfo[idx].resolution.framerate = frate;
								}
								 // Map profile index to corresponding adaptationset and representation index
                                                                iProfileMaps[idx].adaptationSetIndex = adaptIdx;
                                                                iProfileMaps[idx].representationIndex = reprIdx;

								if (ISCONFIGSET(eAAMPConfig_LimitResolution) && aamp->mDisplayWidth > 0 && aamp->mDisplayHeight > 0)
								{
									if (representation->GetWidth() <= aamp->mDisplayWidth)
									{
										resolutionCheckEnabled = true;
									}
									else
									{
										if (mStreamInfo[idx].isIframeTrack)
											bIframeCapped = true;
										else
											bVideoCapped = true;
									}
								}
								idx++;
							}
						}
					}
					for (int pidx = 0; pidx < idx; pidx++)
					{
						if (resolutionCheckEnabled && (mStreamInfo[pidx].resolution.width > aamp->mDisplayWidth) &&
							((mStreamInfo[pidx].isIframeTrack && bIframeCapped) || 
							(!mStreamInfo[pidx].isIframeTrack && bVideoCapped)))
						{
							AAMPLOG_INFO("%s:%d Video Profile ignoring for resolution= %d:%d display= %d:%d BW=%ld", __FUNCTION__, __LINE__, mStreamInfo[pidx].resolution.width, mStreamInfo[pidx].resolution.height, aamp->mDisplayWidth, aamp->mDisplayHeight, mStreamInfo[pidx].bandwidthBitsPerSecond);
						}
						else
						{
							if ((mStreamInfo[pidx].bandwidthBitsPerSecond > minBitrate) && (mStreamInfo[pidx].bandwidthBitsPerSecond < maxBitrate))
							{
								if (ISCONFIGSET(eAAMPConfig_Disable4K) &&
									(mStreamInfo[pidx].resolution.height > 1080 || mStreamInfo[pidx].resolution.width > 1920))
								{
									continue;
								}
								GetABRManager().addProfile({
									mStreamInfo[pidx].isIframeTrack,
									mStreamInfo[pidx].bandwidthBitsPerSecond,
									mStreamInfo[pidx].resolution.width,
									mStreamInfo[pidx].resolution.height,
									"",
									pidx
								});

								mProfileMaps[addedProfiles].adaptationSetIndex = iProfileMaps[pidx].adaptationSetIndex;
								mProfileMaps[addedProfiles].representationIndex = iProfileMaps[pidx].representationIndex;
								addedProfiles++;
								if (resolutionCheckEnabled && 
									((mStreamInfo[pidx].isIframeTrack && bIframeCapped) || 
									(!mStreamInfo[pidx].isIframeTrack && bVideoCapped)))
                                                                {
                                                                        aamp->mProfileCappedStatus = true;
                                                                }

								//Update profile resolution with VideoEnd Metrics object.
								aamp->UpdateVideoEndProfileResolution(
									(mStreamInfo[pidx].isIframeTrack ? eMEDIATYPE_IFRAME : eMEDIATYPE_VIDEO ),
								mStreamInfo[pidx].bandwidthBitsPerSecond,
								mStreamInfo[pidx].resolution.width,
								mStreamInfo[pidx].resolution.height);
								if(mStreamInfo[pidx].resolution.height > 1080
									|| mStreamInfo[pidx].resolution.width > 1920)
								{
									defaultBitrate = aamp->GetDefaultBitrate4K();
									iframeBitrate  = aamp->GetIframeBitrate4K();
								}
								mStreamInfo[pidx].enabled = true;
								AAMPLOG_INFO("%s:%d Added Video Profile to ABR BW= %ld res= %d:%d display= %d:%d pc:%d", __FUNCTION__, __LINE__, mStreamInfo[pidx].bandwidthBitsPerSecond, mStreamInfo[pidx].resolution.width, mStreamInfo[pidx].resolution.height, aamp->mDisplayWidth, aamp->mDisplayHeight, aamp->mProfileCappedStatus);
							}
						}
					}
					if(adaptationSets.size() > 0)
                                       	{
                                        
                                            IAdaptationSet* adaptationSet = adaptationSets.at(0);
					    if ((mNumberOfTracks == 1) && (IsContentType(adaptationSet, eMEDIATYPE_AUDIO)))
                                             {
   						size_t numRepresentations = adaptationSet->GetRepresentation().size();
						for (size_t reprIdx = 0; reprIdx < numRepresentations; reprIdx++)
						{
								IRepresentation *representation = adaptationSet->GetRepresentation().at(reprIdx);
								mStreamInfo[idx].bandwidthBitsPerSecond = representation->GetBandwidth();
								mStreamInfo[idx].isIframeTrack = false;
								mStreamInfo[idx].resolution.height = 0;
								mStreamInfo[idx].resolution.width = 0;
								mStreamInfo[idx].resolution.framerate = 0;
								mStreamInfo[idx].enabled = true;
								std::string repFrameRate = representation->GetFrameRate();
                                                                                            
                                                                 if(repFrameRate.empty())
									repFrameRate = adapFrameRate;
								if(!repFrameRate.empty())
								{
									int val1, val2;
									sscanf(repFrameRate.c_str(),"%d/%d",&val1,&val2);
									double frate = val2? ((double)val1/val2):val1;
									mStreamInfo[idx].resolution.framerate = frate;
								}
									GetABRManager().addProfile({
										mStreamInfo[idx].isIframeTrack,
										mStreamInfo[idx].bandwidthBitsPerSecond,
										mStreamInfo[idx].resolution.width,
										mStreamInfo[idx].resolution.height,
										"",
										(int)idx
									});
									addedProfiles++;
									// Map profile index to corresponding adaptationset and representation index
									mProfileMaps[idx].adaptationSetIndex = 0;
									mProfileMaps[idx].representationIndex = reprIdx;
								        idx++;
						}
					    }
                                        }
					if (0 == addedProfiles)
					{
						ret = eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
						AAMPLOG_WARN("%s:%d No profiles found, minBitrate : %ld maxBitrate: %ld", __FUNCTION__, __LINE__, minBitrate, maxBitrate);
						return ret;
					}
					if (modifyDefaultBW)
					{
						long persistedBandwidth = aamp->GetPersistedBandwidth();
						// XIONE-2039 If Bitrate persisted over trickplay is true, set persisted BW as default init BW
						if (persistedBandwidth > 0 && (persistedBandwidth < defaultBitrate || aamp->IsBitRatePersistedOverSeek()))
						{
							defaultBitrate = persistedBandwidth;
						}
					}
				}

				if (defaultBitrate != aamp->GetDefaultBitrate())
				{
					GetABRManager().setDefaultInitBitrate(defaultBitrate);
				}
			}

			if(-1 == pMediaStreamContext->representationIndex)
			{
				if(!isFogTsb)
				{
					if(i == eMEDIATYPE_VIDEO)
					{
						if(trickplayMode)
						{
							if (iframeBitrate > 0)
							{
								GetABRManager().setDefaultIframeBitrate(iframeBitrate);
							}
							UpdateIframeTracks();
						}
						currentProfileIndex = GetDesiredProfile(false);
						// Adaptation Set Index corresponding to a particular profile
						pMediaStreamContext->adaptationSetIdx = mProfileMaps[currentProfileIndex].adaptationSetIndex;
						// Representation Index within a particular Adaptation Set
						pMediaStreamContext->representationIndex  = mProfileMaps[currentProfileIndex].representationIndex;
						pMediaStreamContext->adaptationSet = GetAdaptationSetAtIndex(pMediaStreamContext->adaptationSetIdx);
						pMediaStreamContext->adaptationSetId = pMediaStreamContext->adaptationSet->GetId();
						IRepresentation *selectedRepresentation = pMediaStreamContext->adaptationSet->GetRepresentation().at(pMediaStreamContext->representationIndex);
						// for the profile selected ,reset the abr values with default bandwidth values
						aamp->ResetCurrentlyAvailableBandwidth(selectedRepresentation->GetBandwidth(),trickplayMode,currentProfileIndex);
						aamp->profiler.SetBandwidthBitsPerSecondVideo(selectedRepresentation->GetBandwidth());
					}
					else
					{
						pMediaStreamContext->representationIndex = pMediaStreamContext->adaptationSet->GetRepresentation().size() / 2; //Select the medium profile on start
						if(i == eMEDIATYPE_AUDIO)
						{
							IRepresentation *selectedRepresentation = pMediaStreamContext->adaptationSet->GetRepresentation().at(pMediaStreamContext->representationIndex);
							aamp->profiler.SetBandwidthBitsPerSecondAudio(selectedRepresentation->GetBandwidth());
						}
					}
				}
				else
				{
					logprintf("%s:%d: [WARN] !! representationIndex is '-1' override with '0' since Custom MPD has single representation", __FUNCTION__, __LINE__);
					pMediaStreamContext->representationIndex = 0; //Fog custom mpd has single representation
				}
			}
			pMediaStreamContext->representation = pMediaStreamContext->adaptationSet->GetRepresentation().at(pMediaStreamContext->representationIndex);

			const std::vector<IBaseUrl *>*baseUrls  = &pMediaStreamContext->representation->GetBaseURLs();
			if(!baseUrls)
			{
				AAMPLOG_WARN("%s:%d :  baseUrls is null", __FUNCTION__, __LINE__);  //CID:84531 - Null Returns
			}
			if (baseUrls->size() == 0)
			{
				baseUrls = &pMediaStreamContext->adaptationSet->GetBaseURLs();
				if (baseUrls->size() == 0)
				{
					baseUrls = &mCurrentPeriod->GetBaseURLs();
					if (baseUrls->size() == 0)
					{
						baseUrls = &mpd->GetBaseUrls();
					}
				}
			}
			pMediaStreamContext->fragmentDescriptor.SetBaseURLs(baseUrls);

			pMediaStreamContext->fragmentIndex = 0;

			if(resetTimeLineIndex)
			{
				pMediaStreamContext->timeLineIndex = 0;
			}
			pMediaStreamContext->fragmentRepeatCount = 0;
			pMediaStreamContext->fragmentOffset = 0;
			pMediaStreamContext->periodStartOffset = pMediaStreamContext->fragmentTime;
			pMediaStreamContext->eos = false;
			if(0 == pMediaStreamContext->fragmentDescriptor.Bandwidth || !aamp->IsTSBSupported())
			{
				pMediaStreamContext->fragmentDescriptor.Bandwidth = pMediaStreamContext->representation->GetBandwidth();
			}
			pMediaStreamContext->fragmentDescriptor.RepresentationID.assign(pMediaStreamContext->representation->GetId());
			pMediaStreamContext->fragmentDescriptor.Time = 0;
			
			if(periodChanged)
			{
				//update period start and endtimes as period has changed.
				mPeriodEndTime = GetPeriodEndTime(mpd, mCurrentPeriodIdx, mLastPlaylistDownloadTimeMs);
				mPeriodStartTime = GetPeriodStartTime(mpd, mCurrentPeriodIdx);
				mPeriodDuration = GetPeriodDuration(mpd, mCurrentPeriodIdx);
			}

			SegmentTemplates segmentTemplates(pMediaStreamContext->representation->GetSegmentTemplate(),pMediaStreamContext->adaptationSet->GetSegmentTemplate());
			if( segmentTemplates.HasSegmentTemplate())
			{
				pMediaStreamContext->fragmentDescriptor.Number = segmentTemplates.GetStartNumber();
				AAMPLOG_INFO("StreamAbstractionAAMP_MPD::%s:%d Track %d timeLineIndex %d fragmentDescriptor.Number %lu", __FUNCTION__, __LINE__, i, pMediaStreamContext->timeLineIndex, pMediaStreamContext->fragmentDescriptor.Number);
			}

				
		}
	}
	return ret;
}



/**
 * @brief Update culling state for live manifests
 */
double StreamAbstractionAAMP_MPD::GetCulledSeconds()
{
	double newStartTimeSeconds = 0;
	double culled = 0;
	traceprintf("StreamAbstractionAAMP_MPD::%s:%d Enter", __FUNCTION__, __LINE__);
	MediaStreamContext *pMediaStreamContext = mMediaStreamContext[eMEDIATYPE_VIDEO];
	if (pMediaStreamContext->adaptationSet)
	{
		SegmentTemplates segmentTemplates(pMediaStreamContext->representation->GetSegmentTemplate(),
					pMediaStreamContext->adaptationSet->GetSegmentTemplate());
		const ISegmentTimeline *segmentTimeline = NULL;
		if(segmentTemplates.HasSegmentTemplate())
		{
			segmentTimeline = segmentTemplates.GetSegmentTimeline();
			if (segmentTimeline)
			{
				auto periods = mpd->GetPeriods();
				vector<PeriodInfo> currMPDPeriodDetails;
				for (int iter = 0; iter < periods.size(); iter++)
				{
					auto period = periods.at(iter);
					PeriodInfo periodInfo;
					periodInfo.periodId = period->GetId();
					periodInfo.duration = GetPeriodDuration(mpd, iter)/ 1000;
					periodInfo.startTime = GetFirstSegmentStartTime(period);
					periodInfo.timeScale = GetPeriodSegmentTimeScale(period);
					currMPDPeriodDetails.push_back(periodInfo);
				}

				int iter1 = 0;
				PeriodInfo currFirstPeriodInfo = currMPDPeriodDetails.at(0);
				while (iter1 < aamp->mMPDPeriodsInfo.size())
				{
					PeriodInfo prevPeriodInfo = aamp->mMPDPeriodsInfo.at(iter1);
					if(prevPeriodInfo.periodId == currFirstPeriodInfo.periodId)
					{
						/* Update culled seconds if startTime of existing periods changes
						 * and exceeds previos start time. Updates culled value for ad periods
						 * with startTime = 0 on next refresh if fragments are removed.
						 */
						if(currFirstPeriodInfo.startTime > prevPeriodInfo.startTime)
						{
							uint64_t timeDiff = currFirstPeriodInfo.startTime - prevPeriodInfo.startTime;
							culled += ((double)timeDiff / (double)prevPeriodInfo.timeScale);
							AAMPLOG_INFO("%s:%d PeriodId %s, prevStart %" PRIu64 " currStart %" PRIu64 " culled %f", __FUNCTION__, __LINE__,
												prevPeriodInfo.periodId.c_str(), prevPeriodInfo.startTime, currFirstPeriodInfo.startTime, culled);
						}
						break;
					}
					else
					{
						culled += prevPeriodInfo.duration;
						iter1++;
						logprintf("%s:%d PeriodId %s , with last known duration %f seems to have got culled", __FUNCTION__, __LINE__,
										prevPeriodInfo.periodId.c_str(), prevPeriodInfo.duration);
					}
				}
				aamp->mMPDPeriodsInfo = currMPDPeriodDetails;
			}
			else
			{
				AAMPLOG_INFO("StreamAbstractionAAMP_MPD::%s:%d NULL segmentTimeline. Hence modifying culling logic based on MPD availabilityStartTime, periodStartTime, fragment number and current time", __FUNCTION__, __LINE__);
				double newStartSegment = 0;
				ISegmentTemplate *firstSegTempate = NULL;
				
				// Recalculate the new start fragment after periodic manifest updates
				auto periods = mpd->GetPeriods();
				for (auto period : periods)
				{
					auto adaptationSets = period->GetAdaptationSets();
					for(auto adaptation : adaptationSets)
					{
						auto segTemplate = adaptation->GetSegmentTemplate();
						if(!segTemplate && adaptation->GetRepresentation().size() > 0)
						{
							segTemplate = adaptation->GetRepresentation().at(0)->GetSegmentTemplate();
						}

						if(segTemplate)
						{
							firstSegTempate = segTemplate;
							break;
						}
					}
					if(firstSegTempate)
					{
						break;
					}
				}

				if(firstSegTempate)
				{
					newStartSegment = (double)firstSegTempate->GetStartNumber();
					double fragmentDuration = ((double)segmentTemplates.GetDuration()) / segmentTemplates.GetTimescale();
					if (newStartSegment && mPrevStartTimeSeconds)
					{
						culled = (newStartSegment - mPrevStartTimeSeconds) * fragmentDuration;
						traceprintf("StreamAbstractionAAMP_MPD::%s:%d post-refresh %fs before %f (%f)", __FUNCTION__, __LINE__, newStartTimeSeconds, mPrevStartTimeSeconds, culled);
					}
					else
					{
						logprintf("StreamAbstractionAAMP_MPD::%s:%d newStartTimeSeconds %f mPrevStartTimeSeconds %F", __FUNCTION__, __LINE__, newStartSegment, mPrevStartTimeSeconds);
					}
					mPrevStartTimeSeconds = newStartSegment;
				}
			}
		}
		else
		{
			ISegmentList *segmentList = pMediaStreamContext->representation->GetSegmentList();
			if (segmentList)
			{
				std::map<string,string> rawAttributes = segmentList->GetRawAttributes();
				if(rawAttributes.find("customlist") == rawAttributes.end())
				{
					segmentTimeline = segmentList->GetSegmentTimeline();
				}
				else
				{
					//Updated logic for culling,
					vector<IPeriod*> periods =  mpd->GetPeriods();
					long duration = 0;
					long prevLastSegUrlOffset = 0;
					long newOffset = 0;
					bool offsetFound = false;
					std::string newMedia;
					for(int iPeriod = periods.size() - 1 ; iPeriod >= 0; iPeriod--)
					{
						IPeriod* period = periods.at(iPeriod);
						vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();
						if (adaptationSets.empty())
						{
							continue;
						}
						IAdaptationSet * adaptationSet = adaptationSets.at(0);
						if(!adaptationSet)
						{
							AAMPLOG_WARN("%s:%d :  adaptationSet is null", __FUNCTION__, __LINE__);  //CID:80968 - Null Returns
						}
						vector<IRepresentation *> representations = adaptationSet->GetRepresentation();


						if(representations.empty())
						{
							continue;
						}

						IRepresentation* representation = representations.at(0);
						ISegmentList *segmentList = representation->GetSegmentList();

						if(!segmentList)
						{
							continue;
						}

						duration += segmentList->GetDuration();
						vector<ISegmentURL*> segUrls = segmentList->GetSegmentURLs();
						if(!segUrls.empty())
						{
							for(int iSegurl = segUrls.size() - 1; iSegurl >= 0 && !offsetFound; iSegurl--)
							{
								std::string media = segUrls.at(iSegurl)->GetMediaURI();
								std::string offsetStr = segUrls.at(iSegurl)->GetRawAttributes().at("d");
								uint32_t offset = stol(offsetStr);
								if(0 == newOffset)
								{
									newOffset = offset;
									newMedia = media;
								}
								if(0 == mPrevLastSegurlOffset && !offsetFound)
								{
									offsetFound = true;
									break;
								}
								else if(mPrevLastSegurlMedia == media)
								{
									offsetFound = true;
									prevLastSegUrlOffset += offset;
									break;
								}
								else
								{
									prevLastSegUrlOffset += offset;
								}
							}//End of segurl for loop
						}
					} //End of Period for loop
					long offsetDiff = 0;
					long currOffset = duration - prevLastSegUrlOffset;
					if(mPrevLastSegurlOffset)
					{
						long timescale = segmentList->GetTimescale();
						offsetDiff = mPrevLastSegurlOffset - currOffset;
						if(offsetDiff > 0)
						{
							culled = (double)offsetDiff / timescale;
						}
					}
					AAMPLOG_INFO("StreamAbstractionAAMP_MPD::%s:%d PrevOffset %ld CurrentOffset %ld culled (%f)", __FUNCTION__, __LINE__, mPrevLastSegurlOffset, currOffset, culled);
					mPrevLastSegurlOffset = duration - newOffset;
					mPrevLastSegurlMedia = newMedia;
				}
			}
			else
			{
				AAMPLOG_INFO("StreamAbstractionAAMP_MPD::%s:%d NULL segmentTemplate and segmentList", __FUNCTION__, __LINE__);
			}
		}
	}
	else
	{
		logprintf("StreamAbstractionAAMP_MPD::%s:%d NULL adaptationset", __FUNCTION__, __LINE__);
	}
	return culled;
}

/**
* @brief Update culled and duration value from periodinfo
*/
void StreamAbstractionAAMP_MPD::UpdateCulledAndDurationFromPeriodInfo()
{
	IPeriod* firstPeriod = NULL;
	unsigned firstPeriodIdx = 0;
	for (unsigned iPeriod = 0; iPeriod < mpd->GetPeriods().size(); iPeriod++)
	{
		IPeriod *period = mpd->GetPeriods().at(iPeriod);
		if(!IsEmptyPeriod(period))
		{
			firstPeriod = mpd->GetPeriods().at(iPeriod);
			firstPeriodIdx = iPeriod;
			break;
		}
	}
	if(firstPeriod)
	{
		unsigned lastPeriodIdx = mpd->GetPeriods().size() - 1;
		for (unsigned iPeriod = mpd->GetPeriods().size() - 1 ; iPeriod >= 0; iPeriod--)
		{
			IPeriod *period = mpd->GetPeriods().at(iPeriod);
			if(IsEmptyPeriod(period))
			{
				// Empty Period . Ignore processing, continue to next.
				continue;
			}
			else
			{
				lastPeriodIdx = iPeriod;
				break;
			}
		}
		double firstPeriodStart = GetPeriodStartTime(mpd, firstPeriodIdx) - mAvailabilityStartTime;
		double lastPeriodStart = 0;
		if (firstPeriodIdx == lastPeriodIdx)
		{
			lastPeriodStart = firstPeriodStart;
			if(!mIsLiveManifest && mIsLiveStream)
			{
				lastPeriodStart = aamp->culledSeconds;
			}
		}
		else
		{
			// Expecting no delta between final period PTSOffset and segment start if
			// first and last periods are different.
			lastPeriodStart = GetPeriodStartTime(mpd, lastPeriodIdx) - mAvailabilityStartTime;
		}
		double culled = firstPeriodStart - aamp->culledSeconds;
		if (culled != 0 && mIsLiveManifest)
		{
			aamp->culledSeconds = firstPeriodStart;
			mCulledSeconds = aamp->culledSeconds;
		}
		aamp->mAbsoluteEndPosition = lastPeriodStart + (GetPeriodDuration(mpd, lastPeriodIdx) / 1000.00);
		if(aamp->mAbsoluteEndPosition < aamp->culledSeconds)
		{
			// Handling edge case just before dynamic => static transition.
			// This is an edge case where manifest is still dynamic,
			// Start time remains the same, but PTS offset is resetted to segment start time.
			// Updating duration as culled + total duration as manifest is changing to VOD
			AAMPLOG_WARN("%s:%d Duration is less than culled seconds, updating it wrt actual fragment duration" , __FUNCTION__, __LINE__);
			aamp->mAbsoluteEndPosition = 0;
			for (unsigned iPeriod = 0; iPeriod < mpd->GetPeriods().size(); iPeriod++)
			{
				IPeriod *period = mpd->GetPeriods().at(iPeriod);
				if(!IsEmptyPeriod(period))
				{
					aamp->mAbsoluteEndPosition += (GetPeriodDuration(mpd, iPeriod) / 1000.00);
				}
			}
			aamp->mAbsoluteEndPosition += aamp->culledSeconds;
		}

		AAMPLOG_INFO("%s:%d Culled seconds = %f, Updated culledSeconds: %lf duration: %lf", __FUNCTION__, __LINE__, culled, mCulledSeconds, aamp->mAbsoluteEndPosition);
	}
}



/**
 * @brief Fetch and inject initialization fragment
 * @param discontinuity true if discontinuous fragment
 */
void StreamAbstractionAAMP_MPD::FetchAndInjectInitialization(bool discontinuity)
{
	pthread_t trackDownloadThreadID;
	HeaderFetchParams *fetchParams = NULL;
	bool dlThreadCreated = false;
	int numberOfTracks = mNumberOfTracks;
	for (int i = 0; i < numberOfTracks; i++)
	{
		class MediaStreamContext *pMediaStreamContext = mMediaStreamContext[i];
		if(discontinuity && pMediaStreamContext->enabled)
		{
			pMediaStreamContext->discontinuity = discontinuity;
		}
		if(pMediaStreamContext->enabled && (pMediaStreamContext->profileChanged || pMediaStreamContext->discontinuity))
		{
			if (pMediaStreamContext->adaptationSet)
			{
				SegmentTemplates segmentTemplates(pMediaStreamContext->representation->GetSegmentTemplate(),
								pMediaStreamContext->adaptationSet->GetSegmentTemplate() );
				if( segmentTemplates.HasSegmentTemplate() )
				{
					std::string initialization = segmentTemplates.Getinitialization();
					if (!initialization.empty())
					{
						double fragmentDuration = 0.0;
						/*
						 * This block is added to download the initialization tracks in parallel
						 * to reduce the tune time, especially when using DRM.
						 * Moving the fragment download of first AAMPTRACK to separate thread
						 */
						if(!dlThreadCreated)
						{
							fetchParams = new HeaderFetchParams();
							fetchParams->context = this;
							fetchParams->fragmentduration = fragmentDuration;
							fetchParams->initialization = initialization;
							fetchParams->isinitialization = true;
							fetchParams->pMediaStreamContext = pMediaStreamContext;
							fetchParams->discontinuity = pMediaStreamContext->discontinuity;
							int ret = pthread_create(&trackDownloadThreadID, NULL, TrackDownloader, fetchParams);
							if(ret != 0)
							{
								logprintf("StreamAbstractionAAMP_MPD::%s:%d pthread_create failed for TrackDownloader with errno = %d, %s", __FUNCTION__, __LINE__, errno, strerror(errno));
								delete fetchParams;
							}
							else
							{
								dlThreadCreated = true;
							}
						}
						else
						{
							if(pMediaStreamContext->WaitForFreeFragmentAvailable(0))
							{
								pMediaStreamContext->profileChanged = false;

								FetchFragment(pMediaStreamContext, initialization, fragmentDuration, true, getCurlInstanceByMediaType(static_cast<MediaType>(i)), pMediaStreamContext->discontinuity);
								pMediaStreamContext->discontinuity = false;
							}
						}
					}
					else
					{
						AAMPLOG_WARN("%s:%d : initialization  is null", __FUNCTION__, __LINE__);  //CID:84853 ,86291- Null Retur
					}
				}
				else
				{
					ISegmentBase *segmentBase = pMediaStreamContext->representation->GetSegmentBase();
					if (segmentBase)
					{
						pMediaStreamContext->fragmentOffset = 0;
						if (pMediaStreamContext->index_ptr)
						{
							g_free( pMediaStreamContext->index_ptr );
							pMediaStreamContext->index_ptr = NULL;
						}
						const IURLType *urlType = segmentBase->GetInitialization();
						if (urlType)
						{
							std::string range = urlType->GetRange();
							int start, fin;
							sscanf(range.c_str(), "%d-%d", &start, &fin);
#ifdef DEBUG_TIMELINE
							logprintf("init %s %d..%d", getMediaTypeName(pMediaStreamContext->mediaType), start, fin);
#endif
							std::string fragmentUrl;
							GetFragmentUrl(fragmentUrl, &pMediaStreamContext->fragmentDescriptor, "");
							if(pMediaStreamContext->WaitForFreeFragmentAvailable(0))
							{
								pMediaStreamContext->profileChanged = false;

								if(!pMediaStreamContext->CacheFragment(fragmentUrl,
									getCurlInstanceByMediaType(pMediaStreamContext->mediaType),
									pMediaStreamContext->fragmentTime,
									0, // duration - zero for init fragment
									range.c_str(), true ))
								{
									logprintf("StreamAbstractionAAMP_MPD::%s:%d failed. fragmentUrl %s fragmentTime %f", __FUNCTION__, __LINE__, fragmentUrl.c_str(), pMediaStreamContext->fragmentTime);
								}
							}
						}
					}
					else
					{
						ISegmentList *segmentList = pMediaStreamContext->representation->GetSegmentList();
						if (segmentList)
						{
							const IURLType *urlType = segmentList->GetInitialization();
							if( !urlType )
							{
								segmentList = pMediaStreamContext->adaptationSet->GetSegmentList();
								urlType = segmentList->GetInitialization();
								if( !urlType )
								{
									AAMPLOG_WARN("%s:%d : initialization is null", __FUNCTION__, __LINE__);
									continue;
								}
							}
							std::string initialization = urlType->GetSourceURL();
							if (!initialization.empty())
							{
								double fragmentDuration = 0.0;
								/*
								 * This block is added to download the initialization tracks in parallel
								 * to reduce the tune time, especially when using DRM.
								 * Moving the fragment download of first AAMPTRACK to separate thread
								 */
								if(!dlThreadCreated)
								{
									fetchParams = new HeaderFetchParams();
									fetchParams->context = this;
									fetchParams->fragmentduration = fragmentDuration;
									fetchParams->initialization = initialization;
									fetchParams->isinitialization = true;
									fetchParams->pMediaStreamContext = pMediaStreamContext;
									int ret = pthread_create(&trackDownloadThreadID, NULL, TrackDownloader, fetchParams);
									if(ret != 0)
									{
										logprintf("StreamAbstractionAAMP_MPD::%s:%d pthread_create failed for TrackDownloader with errno = %d, %s", __FUNCTION__, __LINE__, errno, strerror(errno));
										delete fetchParams;
									}
									else
									{
										dlThreadCreated = true;
									}
								}
								else
								{
									if(pMediaStreamContext->WaitForFreeFragmentAvailable(0))
									{
										pMediaStreamContext->profileChanged = false;
										FetchFragment(pMediaStreamContext, initialization, fragmentDuration, true, getCurlInstanceByMediaType(static_cast<MediaType>(i)));
									}
								}
							}
							else
							{
								string range;
#ifdef LIBDASH_SEGMENTLIST_GET_INIT_SUPPORT
								const ISegmentURL *segmentURL = NULL;
								segmentURL = segmentList->Getinitialization();

								if (segmentURL)
								{
									range = segmentURL->GetMediaRange();
								}
#else
								const std::vector<ISegmentURL*> segmentURLs = segmentList->GetSegmentURLs();
								if (segmentURLs.size() > 0)
								{
									ISegmentURL *firstSegmentURL = segmentURLs.at(0);
									int start, fin;
									if(firstSegmentURL != NULL)
									{
										const char *firstSegmentRange = firstSegmentURL->GetMediaRange().c_str();
										AAMPLOG_INFO("firstSegmentRange %s [%s]",
												getMediaTypeName(pMediaStreamContext->mediaType), firstSegmentRange);
										if (sscanf(firstSegmentRange, "%d-%d", &start, &fin) == 2)
										{
											if (start > 1)
											{
												char range_c[64];
												sprintf(range_c, "%d-%d", 0, start - 1);
												range = range_c;
											}
											else
											{
												logprintf("StreamAbstractionAAMP_MPD::%s:%d segmentList - cannot determine range for Initialization - first segment start %d",
														__FUNCTION__, __LINE__, start);
											}
										}
									}
									else
									{
										AAMPLOG_WARN("%s:%d : firstSegmentURL is null", __FUNCTION__, __LINE__);  //CID:80649 - Null Returns
									}
								}
#endif
								if (!range.empty())
								{
									std::string fragmentUrl;
									GetFragmentUrl(fragmentUrl, &pMediaStreamContext->fragmentDescriptor, "");
									AAMPLOG_INFO("%s [%s]", getMediaTypeName(pMediaStreamContext->mediaType),
											range.c_str());
									if(pMediaStreamContext->WaitForFreeFragmentAvailable(0))
									{
										pMediaStreamContext->profileChanged = false;
										if(!pMediaStreamContext->CacheFragment(fragmentUrl,
												getCurlInstanceByMediaType(pMediaStreamContext->mediaType),
												pMediaStreamContext->fragmentTime,
												0.0, // duration - zero for init fragment
												range.c_str(),
												true ))
										{
											logprintf("StreamAbstractionAAMP_MPD::%s:%d failed. fragmentUrl %s fragmentTime %f", __FUNCTION__, __LINE__, fragmentUrl.c_str(), pMediaStreamContext->fragmentTime);
										}
									}
								}
								else
								{
									logprintf("StreamAbstractionAAMP_MPD::%s:%d segmentList - empty range string for Initialization",
											__FUNCTION__, __LINE__);
								}
							}
						}
						else
						{
							AAMPLOG_ERR("%s:%d not-yet-supported mpd format",__FUNCTION__,__LINE__);
						}
					}
				}
			}
		}
	}

	if(dlThreadCreated)
	{
		AAMPLOG_TRACE("Waiting for pthread_join trackDownloadThread");
		pthread_join(trackDownloadThreadID, NULL);
		AAMPLOG_TRACE("Joined trackDownloadThread");
		delete fetchParams;
	}
}


/**
 * @brief Check if current period is clear
 * @retval true if clear period
 */
bool StreamAbstractionAAMP_MPD::CheckForInitalClearPeriod()
{
	bool ret = true;
	vector<IDescriptor*> contentProt;
	for(int i = 0; i < mNumberOfTracks; i++)
	{
		contentProt = mMediaStreamContext[i]->adaptationSet->GetContentProtection();
		if(0 != contentProt.size())
		{
			ret = false;
			break;
		}
	}
	if(ret)
	{
		logprintf("%s %d Initial period is clear period, trying work around",__FUNCTION__,__LINE__);
	}
	return ret;
}

/**
 * @brief Push encrypted headers if available
 */
void StreamAbstractionAAMP_MPD::PushEncryptedHeaders()
{
	//Find the first period with contentProtection
	size_t numPeriods = mpd->GetPeriods().size();  //CID:96576 - Removed the  headerCount variable which is initialized but not used
	for(int i = mNumberOfTracks - 1; i >= 0; i--)
	{
		bool encryptionFound = false;
		unsigned iPeriod = 0;
		while(iPeriod < numPeriods && !encryptionFound)
		{
			IPeriod *period = mpd->GetPeriods().at(iPeriod);
			if(period != NULL)
			{
				size_t numAdaptationSets = period->GetAdaptationSets().size();
				for(unsigned iAdaptationSet = 0; iAdaptationSet < numAdaptationSets && !encryptionFound; iAdaptationSet++)
				{
					IAdaptationSet *adaptationSet = period->GetAdaptationSets().at(iAdaptationSet);
					if(adaptationSet != NULL)
					{
						if (IsContentType(adaptationSet, (MediaType)i ))
						{
							vector<IDescriptor*> contentProt;
							contentProt = adaptationSet->GetContentProtection();
							if(0 == contentProt.size())
							{
								continue;
							}
							else
							{
								// Push Encrypted headers early only if manifest has the PSSH data
								// Extract the PSSH data
								bool psshDataFound = false ;
								for (int iContentProt = 0; iContentProt < contentProt.size(); iContentProt++ )
								{
									const vector<INode*> node = contentProt.at(iContentProt)->GetAdditionalSubNodes();
									if (!node.empty())
									{
										for(int i=0; i < node.size(); i++){
											std::string tagName = node.at(i)->GetName();
											if (tagName.find("pssh") != std::string::npos)
											{
												string psshData = node.at(i)->GetText();
												if (!psshData.empty())
												{
													psshDataFound = true;
													break;
												}
											}
										}
									}
									if (psshDataFound) break;
								}
								if (!psshDataFound)
								{
									AAMPLOG_INFO("%s %d [INFO] No PSSH data in manifest, Removing early encrypted Header push"
									,__FUNCTION__,__LINE__);
									encryptionFound = true; //No need to continue further
									continue;
								}
								IRepresentation *representation = NULL;
								size_t representionIndex = 0;
								if(MediaType(i) == eMEDIATYPE_VIDEO)
								{
									size_t representationCount = adaptationSet->GetRepresentation().size();
									if(adaptationSet->GetRepresentation().at(representionIndex)->GetBandwidth() > adaptationSet->GetRepresentation().at(representationCount - 1)->GetBandwidth())
									{
										representionIndex = representationCount - 1;
									}
								}
								else if (mAudioType != eAUDIO_UNKNOWN)
								{
									AudioType selectedAudioType = eAUDIO_UNKNOWN;
									uint32_t selectedRepBandwidth = 0;									
									bool disableATMOS = ISCONFIGSET(eAAMPConfig_DisableATMOS);
									bool disableEC3 = ISCONFIGSET(eAAMPConfig_DisableEC3);
									representionIndex = GetDesiredCodecIndex(adaptationSet, selectedAudioType, selectedRepBandwidth,disableEC3 , disableATMOS);
									if(selectedAudioType != mAudioType)
									{
										continue;
									}
									logprintf("%s %d Audio type %d", __FUNCTION__, __LINE__, selectedAudioType);
								}
								else
								{
									logprintf("%s %d Audio type eAUDIO_UNKNOWN", __FUNCTION__, __LINE__);
								}
								representation = adaptationSet->GetRepresentation().at(representionIndex);

								SegmentTemplates segmentTemplates(representation->GetSegmentTemplate(), adaptationSet->GetSegmentTemplate());
								if(segmentTemplates.HasSegmentTemplate())
								{
									std::string initialization = segmentTemplates.Getinitialization();
									if (!initialization.empty())
									{
										std::string fragmentUrl;
										FragmentDescriptor *fragmentDescriptor = new FragmentDescriptor();
										fragmentDescriptor->bUseMatchingBaseUrl	=	ISCONFIGSET(eAAMPConfig_MatchBaseUrl);
										fragmentDescriptor->manifestUrl = mMediaStreamContext[eMEDIATYPE_VIDEO]->fragmentDescriptor.manifestUrl;
										ProcessContentProtection(adaptationSet, (MediaType)i);
										fragmentDescriptor->Bandwidth = representation->GetBandwidth();
										const std::vector<IBaseUrl *>*baseUrls = &representation->GetBaseURLs();
										if (baseUrls->size() == 0)
										{
											baseUrls = &adaptationSet->GetBaseURLs();
											if (baseUrls->size() == 0)
											{
												baseUrls = &period->GetBaseURLs();
												if (baseUrls->size() == 0)
												{
													baseUrls = &mpd->GetBaseUrls();
												}
											}
										}
										fragmentDescriptor->SetBaseURLs(baseUrls);
										fragmentDescriptor->RepresentationID.assign(representation->GetId());
										GetFragmentUrl(fragmentUrl,fragmentDescriptor , initialization);
										if (mMediaStreamContext[i]->WaitForFreeFragmentAvailable())
										{
											logprintf("%s %d Pushing encrypted header for %s", __FUNCTION__, __LINE__, getMediaTypeName(MediaType(i)));
											bool temp =  mMediaStreamContext[i]->CacheFragment(fragmentUrl, (eCURLINSTANCE_VIDEO + mMediaStreamContext[i]->mediaType), mMediaStreamContext[i]->fragmentTime, 0.0, NULL, true);
											if(!temp)
											{
												AAMPLOG_WARN("%s:%d: Error at  pthread_create", __FUNCTION__, __LINE__);  //CID:84438 - checked return
											}
										}
										delete fragmentDescriptor;
										encryptionFound = true;
									}
								}
							}
						}
					}
					else
					{
						AAMPLOG_WARN("%s:%d :  adaptationSet is null", __FUNCTION__, __LINE__);  //CID:84361 - Null Returns
					}
				}
			}
			else
			{
				AAMPLOG_WARN("%s:%d :  period    is null", __FUNCTION__, __LINE__);  //CID:86137 - Null Returns
			}
			iPeriod++;
		}
	}
}


/**
 * @brief Check if the given period is empty
 */
static bool IsEmptyPeriod(IPeriod *period)
{
	bool isEmptyPeriod = true;
	const std::vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();
	size_t numAdaptationSets = period->GetAdaptationSets().size();
	for (int iAdaptationSet = 0; iAdaptationSet < numAdaptationSets; iAdaptationSet++)
	{
		IAdaptationSet *adaptationSet = period->GetAdaptationSets().at(iAdaptationSet);
		IRepresentation *representation = NULL;
		ISegmentTemplate *segmentTemplate = adaptationSet->GetSegmentTemplate();
		if (segmentTemplate)
		{
			isEmptyPeriod = false;
		}
		else
		{
			if(adaptationSet->GetRepresentation().size() > 0)
			{
				//Get first representation in the adapatation set
				representation = adaptationSet->GetRepresentation().at(0);
			}
			if(representation)
			{
				segmentTemplate = representation->GetSegmentTemplate();
				if(segmentTemplate)
				{
					isEmptyPeriod = false;
				}
				else
				{
					ISegmentList *segmentList = representation->GetSegmentList();
					if(segmentList)
					{
						isEmptyPeriod = false;
					}
					else
					{
						ISegmentBase *segmentBase = representation->GetSegmentBase();
						if(segmentBase)
						{
							isEmptyPeriod = false;
						}
					}
				}
			}
		}

		if(!isEmptyPeriod)
		{
			break;
		}
	}
	return isEmptyPeriod;
}

/**
 * @brief Fetches and caches audio fragment parallelly for video fragment.
 */
void StreamAbstractionAAMP_MPD::AdvanceTrack(int trackIdx, bool trickPlay, double delta, bool *waitForFreeFrag, bool *exitFetchLoop, bool *bCacheFullState)
{
	class MediaStreamContext *pMediaStreamContext = mMediaStreamContext[trackIdx];
	bool isAllowNextFrag = true;
	int  maxCachedFragmentsPerTrack;
        GETCONFIGVALUE(eAAMPConfig_MaxFragmentCached,maxCachedFragmentsPerTrack); 
	int  vodTrickplayFPS;
	GETCONFIGVALUE(eAAMPConfig_VODTrickPlayFPS,vodTrickplayFPS); 

	if (waitForFreeFrag && *waitForFreeFrag && !trickPlay)
	{
		PrivAAMPState state;
		aamp->GetState(state);

		if(state == eSTATE_PLAYING)
		{
			*waitForFreeFrag = false;
		}
		else
		{
			isAllowNextFrag = pMediaStreamContext->WaitForFreeFragmentAvailable();
		}
	}

	if (isAllowNextFrag)
	{
		if (pMediaStreamContext->adaptationSet )
		{
			if((pMediaStreamContext->numberOfFragmentsCached != maxCachedFragmentsPerTrack) && !(pMediaStreamContext->profileChanged))
			{	// profile not changed and Cache not full scenario
				if (!pMediaStreamContext->eos)
				{
					if(trickPlay && pMediaStreamContext->mDownloadedFragment.ptr == NULL)
					{
						if((rate > 0 && delta <= 0) || (rate < 0 && delta >= 0))
						{
							delta = rate / vodTrickplayFPS;
						}
						double currFragTime = pMediaStreamContext->fragmentTime;
						delta = SkipFragments(pMediaStreamContext, delta);
						mBasePeriodOffset += (pMediaStreamContext->fragmentTime - currFragTime);
					}
					if (PushNextFragment(pMediaStreamContext, getCurlInstanceByMediaType(static_cast<MediaType>(trackIdx))))
					{
						if (mIsLiveManifest)
						{
							pMediaStreamContext->GetContext()->CheckForPlaybackStall(true);
						}
						if((!pMediaStreamContext->GetContext()->trickplayMode) && (eMEDIATYPE_VIDEO == trackIdx))
						{
							if (aamp->CheckABREnabled())
							{
								pMediaStreamContext->GetContext()->CheckForProfileChange();
							}
						}
					}
					else if (pMediaStreamContext->eos == true && mIsLiveManifest && trackIdx == eMEDIATYPE_VIDEO)
					{
						pMediaStreamContext->GetContext()->CheckForPlaybackStall(false);
					}

					if (AdState::IN_ADBREAK_AD_PLAYING == mCdaiObject->mAdState && rate > 0 && !(pMediaStreamContext->eos)
							&& mCdaiObject->CheckForAdTerminate(pMediaStreamContext->fragmentTime - pMediaStreamContext->periodStartOffset))
					{
						//Ensuring that Ad playback doesn't go beyond Adbreak
						AAMPLOG_WARN("%s:%d: [CDAI] Adbreak ended early. Terminating Ad playback. fragmentTime[%lf] periodStartOffset[%lf]",
											__FUNCTION__, __LINE__, pMediaStreamContext->fragmentTime, pMediaStreamContext->periodStartOffset);
						pMediaStreamContext->eos = true;
					}
				}
			}
			// Fetch init header for both audio and video ,after mpd refresh(stream selection) , profileChanged = true for both tracks .
			// Need to reset profileChanged flag which is done inside FetchAndInjectInitialization
			// Without resetting profileChanged flag , fetch of audio was stopped causing audio drop
			// DELIA-32017
			else if(pMediaStreamContext->profileChanged)
			{	// Profile changed case
				FetchAndInjectInitialization();
			}

			if(pMediaStreamContext->numberOfFragmentsCached != maxCachedFragmentsPerTrack && bCacheFullState)
			{
				*bCacheFullState = false;
			}

		}
	}
	if (!aamp->DownloadsAreEnabled() && exitFetchLoop && bCacheFullState)
	{
		*exitFetchLoop = true;
		*bCacheFullState = false;
	}
}

/**
 * @brief Fetches and caches fragments in a loop
 */
void StreamAbstractionAAMP_MPD::FetcherLoop()
{
	aamp_pthread_setname(pthread_self(), "aampMPDFetcher");

	bool exitFetchLoop = false;
	bool trickPlay = (AAMP_NORMAL_PLAY_RATE != rate);
	bool waitForFreeFrag = true;
	bool mpdChanged = false;
	double delta = 0;
	bool lastLiveFlag = false;  //CID:96059 - Removed the  placeNextAd variable which is initialized but not used
	int direction = 1;
	bool hasEventStream = false;

	if(rate < 0)
		direction = -1;
	bool adStateChanged = false;

	IPeriod *currPeriod = mCurrentPeriod;
	if(!currPeriod)
	{
		AAMPLOG_WARN("%s:%d :  currPeriod is null", __FUNCTION__, __LINE__);  //CID:80891 - Null Returns
	}
	std::string currentPeriodId = currPeriod->GetId();
	mPrevAdaptationSetCount = currPeriod->GetAdaptationSets().size();
	logprintf("aamp: ready to collect fragments. mpd %p", mpd);
	do
	{
		bool liveMPDRefresh = false;
			bool waitForAdBreakCatchup= false;
		if (mpd)
		{
			size_t numPeriods = mpd->GetPeriods().size();
			unsigned iPeriod = mCurrentPeriodIdx;
			AAMPLOG_INFO("MPD has %zu periods current period index %u", numPeriods, mCurrentPeriodIdx);
			std::vector<IPeriod*> availablePeriods = mpd->GetPeriods();
			unsigned upperBoundary = numPeriods - 1;
			unsigned lowerBoundary = 0;
			// Calculate lower boundary of playable periods, discard empty periods at the start
			for(auto temp : availablePeriods)
			{
				if(IsEmptyPeriod(temp))
				{
					lowerBoundary++;
					continue;
				}
				break;
			}
			// Calculate upper boundary of playable periods, discard empty periods at the end
			for(auto iter = availablePeriods.rbegin() ; iter != availablePeriods.rend(); iter++ )
			{
				if(IsEmptyPeriod(*iter))
				{
					upperBoundary--;
					continue;
				}
				break;
			}

			while(iPeriod < numPeriods && !exitFetchLoop)  //CID:95090 - No effect
			{
				bool periodChanged = (iPeriod != mCurrentPeriodIdx) | (mBasePeriodId != mpd->GetPeriods().at(mCurrentPeriodIdx)->GetId());
				if (periodChanged || mpdChanged || adStateChanged)
				{
					bool discontinuity = false;
					bool requireStreamSelection = false;
					uint64_t nextSegmentTime = mMediaStreamContext[eMEDIATYPE_VIDEO]->fragmentDescriptor.Time;

					if(mpdChanged)
					{
#ifdef AAMP_MPD_DRM
						if(aamp->mIsVSS)
						{
							std::vector<IPeriod*> vssPeriods;
							// Collect only new vss periods from manifest
							GetAvailableVSSPeriods(vssPeriods);
							for (auto tempPeriod : vssPeriods)
							{
								if (NULL != tempPeriod)
								{
									// Save new period ID and create DRM helper for that
									mEarlyAvailablePeriodIds.push_back(tempPeriod->GetId());
									std::shared_ptr<AampDrmHelper> drmHelper = CreateDrmHelper(tempPeriod->GetAdaptationSets().at(0), eMEDIATYPE_VIDEO);
									if (drmHelper){
									// Identify key ID from parsed PSSH data
									std::vector<uint8_t> keyIdArray;
									drmHelper->getKey(keyIdArray);

									if (!keyIdArray.empty())
									{
										// Save individual VSS stream information
										EarlyAvailablePeriodInfo vssKeyPeriodInfo;
										vssKeyPeriodInfo.periodId = tempPeriod->GetId();
										vssKeyPeriodInfo.helper = drmHelper;
										std::string keyIdDebugStr = AampLogManager::getHexDebugStr(keyIdArray);
										AAMPLOG_INFO("%s:%d New VSS Period : %s Key ID: %s", __FUNCTION__, __LINE__, tempPeriod->GetId().c_str(), keyIdDebugStr.c_str());
										// Check whether key ID is already marked as failure.
										if (!aamp->mDRMSessionManager->IsKeyIdUsable(keyIdArray))
										{
											vssKeyPeriodInfo.isLicenseFailed = true;
										}

										// Insert VSS period information into map if key is not processed
										std::pair<std::map<std::string,EarlyAvailablePeriodInfo>::iterator,bool> retVal;
										retVal = mEarlyAvailableKeyIDMap.insert(std::pair<std::string, EarlyAvailablePeriodInfo>(keyIdDebugStr, vssKeyPeriodInfo));
										if ((retVal.second) && (!vssKeyPeriodInfo.isLicenseFailed))
										{
											// FIFO queue for processing license request
											mPendingKeyIDs.push(keyIdDebugStr);
										}
										else
										{
											AAMPLOG_TRACE("%s:%d Skipping license request for keyID : %s", __FUNCTION__, __LINE__, keyIdDebugStr.c_str() );
										}
									}
									else
									{
										AAMPLOG_WARN("%s:%d Failed to get keyID for vss common key EAP", __FUNCTION__, __LINE__);
									}
									}
									else
									{
										AAMPLOG_ERR("%s:%d Failed to Create DRM Helper", __FUNCTION__, __LINE__);	
									}
								}
							}
							// Proces EAP License request if there is pending keyIDs
							if(!mPendingKeyIDs.empty())
							{
								// Check Deferred License thread status, and process license request
								ProcessEAPLicenseRequest();
							}
						}
#endif
						mpdChanged = false;
					}


					if (periodChanged)
					{
						IPeriod *newPeriod = mpd->GetPeriods().at(iPeriod);

						//for VOD and cDVR
						logprintf("%s:%d Period(%s - %d/%zu) Offset[%lf] IsLive(%d) IsCdvr(%d) ",__FUNCTION__,__LINE__,
							mBasePeriodId.c_str(), mCurrentPeriodIdx, numPeriods, mBasePeriodOffset, mIsLiveStream, aamp->IsInProgressCDVR());

						vector <IAdaptationSet*> adapatationSets = newPeriod->GetAdaptationSets();
						int adaptationSetCount = adapatationSets.size();
						if(0 == adaptationSetCount || IsEmptyPeriod(newPeriod))
						{
							/*To Handle non fog scenarios where empty periods are
							* present after mpd update causing issues (DELIA-29879)
							*/
							iPeriod += direction;
							continue;
						}


						if(mBasePeriodId != newPeriod->GetId() && AdState::OUTSIDE_ADBREAK == mCdaiObject->mAdState)
						{
							mBasePeriodOffset = 0;		//Not considering the delta from previous period's duration.
						}
						if(rate > 0)
						{
							if(AdState::OUTSIDE_ADBREAK != mCdaiObject->mAdState)	//If Adbreak (somehow) goes beyond the designated periods, period outside adbreak will have +ve duration. Avoiding catastrophic cases.
							{
								mBasePeriodOffset -= ((double)mCdaiObject->mPeriodMap[mBasePeriodId].duration)/1000.00;
							}
						}
						else
						{
							mBasePeriodOffset += (GetPeriodDuration(mpd, iPeriod)/1000.00);	//Already reached -ve. Subtracting from current period duration
						}
						mCurrentPeriodIdx = iPeriod;
						mBasePeriodId = newPeriod->GetId();
						periodChanged = false; //If the playing period changes, it will be detected below [if(currentPeriodId != mCurrentPeriod->GetId())]
					}
					adStateChanged = onAdEvent(AdEvent::DEFAULT);		//TODO: Vinod, We can optimize here.

					if(AdState::IN_ADBREAK_WAIT2CATCHUP == mCdaiObject->mAdState)
					{
						waitForAdBreakCatchup= true;
						break;
					}
					if(adStateChanged && AdState::OUTSIDE_ADBREAK == mCdaiObject->mAdState)
					{
						//Just came out from the Adbreak. Need to search the right period
						for(iPeriod=0;iPeriod < numPeriods;  iPeriod++)
						{
							if(mBasePeriodId == mpd->GetPeriods().at(iPeriod)->GetId())
							{
								mCurrentPeriodIdx = iPeriod;
								AAMPLOG_INFO("%s:%d [CDAI] Landed at the periodId[%d] ",__FUNCTION__,__LINE__,mCurrentPeriodIdx);
								break;
							}
						}
					}
					if(AdState::IN_ADBREAK_AD_PLAYING != mCdaiObject->mAdState)
					{
						mCurrentPeriod = mpd->GetPeriods().at(mCurrentPeriodIdx);
					}

					vector <IAdaptationSet*> adapatationSets = mCurrentPeriod->GetAdaptationSets();
					int adaptationSetCount = adapatationSets.size();
					if(currentPeriodId != mCurrentPeriod->GetId())
					{
						/*DELIA-47914:  If next period is empty, period ID change is  not processing.
						Will check the period change for the same period in the next iteration.*/
						if(adaptationSetCount > 0 || !IsEmptyPeriod(mCurrentPeriod))
						{
							logprintf("Period ID changed from \'%s\' to \'%s\' [BasePeriodId=\'%s\']", currentPeriodId.c_str(),mCurrentPeriod->GetId().c_str(), mBasePeriodId.c_str());
							currentPeriodId = mCurrentPeriod->GetId();
							mPrevAdaptationSetCount = adaptationSetCount;
							periodChanged = true;
							requireStreamSelection = true;
							logprintf("playing period %d/%d", iPeriod, (int)numPeriods);
						}
						else
						{
							for (int i = 0; i < mNumberOfTracks; i++)
							{
								mMediaStreamContext[i]->enabled = false;
							}
							logprintf("Period ID not changed from \'%s\' to \'%s\',since period is empty [BasePeriodId=\'%s\']", currentPeriodId.c_str(),mCurrentPeriod->GetId().c_str(), mBasePeriodId.c_str());
						}

						//We are moving to new period so reset the lastsegment time
						for (int i = 0; i < mNumberOfTracks; i++)
						{
							mMediaStreamContext[i]->lastSegmentTime = 0;
							mMediaStreamContext[i]->lastSegmentDuration = 0;
						}
					}
					else if(mPrevAdaptationSetCount != adaptationSetCount)
					{
						logprintf("Change in AdaptationSet count; adaptationSetCount %d  mPrevAdaptationSetCount %d,updating stream selection", adaptationSetCount, mPrevAdaptationSetCount);
						mPrevAdaptationSetCount = adaptationSetCount;
						requireStreamSelection = true;
					}
					else
					{
						for (int i = 0; i < mNumberOfTracks; i++)
						{
							if(mMediaStreamContext[i]->adaptationSetId != adapatationSets.at(mMediaStreamContext[i]->adaptationSetIdx)->GetId())
							{
								logprintf("AdaptationSet index changed; updating stream selection");
								requireStreamSelection = true;
							}
						}
					}
					adStateChanged = false;

					if(requireStreamSelection)
					{
						StreamSelection();
					}

					// IsLive = 1 , resetTimeLineIndex = 1
					// InProgressCdvr (IsLive=1) , resetTimeLineIndex = 1
					// Vod/CDVR for PeriodChange , resetTimeLineIndex = 1
					if(AdState::IN_ADBREAK_AD_PLAYING != mCdaiObject->mAdState || (AdState::IN_ADBREAK_AD_PLAYING == mCdaiObject->mAdState && periodChanged))
					{
						bool resetTimeLineIndex = (mIsLiveStream || lastLiveFlag|| periodChanged);
						UpdateTrackInfo(true, periodChanged, resetTimeLineIndex);
					}

					if(mIsLiveStream || lastLiveFlag)
					{
						double culled = 0;
						if(mMediaStreamContext[eMEDIATYPE_VIDEO]->enabled)
						{
							culled = GetCulledSeconds();
						}
						if(culled > 0)
						{
							AAMPLOG_INFO("%s:%d Culled seconds = %f", __FUNCTION__, __LINE__, culled);
							aamp->UpdateCullingState(culled);
							mCulledSeconds += culled;
						}
						if(ISCONFIGSET(eAAMPConfig_UseAbsoluteTimeline) && mIsLiveStream && !mIsFogTSB)
						{
							UpdateCulledAndDurationFromPeriodInfo();
						}
						
						double durMs = 0;
						mPeriodEndTime = GetPeriodEndTime(mpd, mCurrentPeriodIdx, mLastPlaylistDownloadTimeMs);
						mPeriodStartTime = GetPeriodStartTime(mpd, mCurrentPeriodIdx);
						mPeriodDuration = GetPeriodDuration(mpd, mCurrentPeriodIdx);
						for(int periodIter = 0; periodIter < mpd->GetPeriods().size(); periodIter++)
						{
							if(!IsEmptyPeriod(mpd->GetPeriods().at(periodIter)))
							{
								durMs += GetPeriodDuration(mpd, periodIter);
							}
						}

						double duration = (double)durMs / 1000;
						aamp->UpdateDuration(duration);
						mLiveEndPosition = duration + mCulledSeconds;
						if(mCdaiObject->mContentSeekOffset)
						{
							AAMPLOG_INFO("%s:%d [CDAI]: Resuming channel playback at PeriodID[%s] at Position[%lf]",	__FUNCTION__, __LINE__, currentPeriodId.c_str(), mCdaiObject->mContentSeekOffset);
							//This seek should not be reflected in the fragmentTime, since we have already cached
							//same duration of ad content; So keep a copy of current fragmentTime so that it can be
							//updated back when seek is done
							double fragmentTime[AAMP_TRACK_COUNT];
							for (int i = 0; i < mNumberOfTracks; i++)
							{
								fragmentTime[i] = mMediaStreamContext[i]->fragmentTime;
							}
							SeekInPeriod(mCdaiObject->mContentSeekOffset);
							mCdaiObject->mContentSeekOffset = 0;
							for (int i = 0; i < mNumberOfTracks; i++)
							{
								mMediaStreamContext[i]->fragmentTime = fragmentTime[i];
							}
						}
					}

					lastLiveFlag = mIsLiveStream;
					/*Discontinuity handling on period change*/
					if (periodChanged && ISCONFIGSET(eAAMPConfig_MPDDiscontinuityHandling) && mMediaStreamContext[eMEDIATYPE_VIDEO]->enabled &&
							(ISCONFIGSET(eAAMPConfig_MPDDiscontinuityHandlingCdvr) || (!aamp->IsInProgressCDVR())))
					{
						MediaStreamContext *pMediaStreamContext = mMediaStreamContext[eMEDIATYPE_VIDEO];
						SegmentTemplates segmentTemplates(pMediaStreamContext->representation->GetSegmentTemplate(),
										pMediaStreamContext->adaptationSet->GetSegmentTemplate() );
						bool ignoreDiscontinuity = false;
						if (!trickPlay)
						{
							ignoreDiscontinuity = (mMediaStreamContext[eMEDIATYPE_AUDIO] && !mMediaStreamContext[eMEDIATYPE_AUDIO]->enabled && mMediaStreamContext[eMEDIATYPE_AUDIO]->isFragmentInjectorThreadStarted());
						}

						if(ignoreDiscontinuity)
						{
							logprintf("%s:%d Error! Audio or Video track missing in period, ignoring discontinuity",	__FUNCTION__, __LINE__);
						}
						else
						{
				
							if( segmentTemplates.HasSegmentTemplate() )
							{
								uint64_t segmentStartTime = GetFirstSegmentStartTime(mCurrentPeriod);
								/* Process the discontinuity,
								 * 1. If the next segment time is not matching with the next period segment start time.
								 * 2. To reconfigure the pipeline, if there is a change in the Audio Codec even if there is no change in segment start time in multi period content.
								 */
								if((segmentTemplates.GetSegmentTimeline() != NULL && nextSegmentTime != segmentStartTime) || GetESChangeStatus())
								{
									logprintf("StreamAbstractionAAMP_MPD::%s:%d discontinuity detected nextSegmentTime %" PRIu64 " FirstSegmentStartTime %" PRIu64 " ", __FUNCTION__, __LINE__, nextSegmentTime, segmentStartTime);
									discontinuity = true;
									mFirstPTS = (double)segmentStartTime/(double)segmentTemplates.GetTimescale();
									double startTime = (GetPeriodStartTime(mpd, mCurrentPeriodIdx) - mAvailabilityStartTime);
									if((startTime != 0) && !mIsFogTSB)
									{
										if (ISCONFIGSET(eAAMPConfig_UseAbsoluteTimeline) && aamp->IsLiveStream())
										{
											// Save period start time as first PTS for live stream for absolute progress reporting.
											mStartTimeOfFirstPTS = startTime * 1000;
										}
										else
										{
											mStartTimeOfFirstPTS = ((aamp->culledSeconds + startTime - (GetPeriodStartTime(mpd, 0) - mAvailabilityStartTime)) * 1000);
										}
									}
								}
								else
								{
									logprintf("StreamAbstractionAAMP_MPD::%s:%d No discontinuity detected nextSegmentTime %" PRIu64 " FirstSegmentStartTime %" PRIu64 " ", __FUNCTION__, __LINE__, nextSegmentTime, segmentStartTime);
								}
							}
							else
							{
								traceprintf("StreamAbstractionAAMP_MPD::%s:%d Segment template not available", __FUNCTION__, __LINE__);
							}
						}
					}
					FetchAndInjectInitialization(discontinuity);
					if(mCdaiObject->mAdFailed)
					{
						adStateChanged = onAdEvent(AdEvent::AD_FAILED);
						mCdaiObject->mAdFailed = false;
						continue;
					}
					if(rate < 0 && periodChanged)
					{
						SkipToEnd(mMediaStreamContext[eMEDIATYPE_VIDEO]);
					}
				}

				double lastPrdOffset = mBasePeriodOffset;
				bool parallelDnld = ISCONFIGSET(eAAMPConfig_DashParallelFragDownload) ;
				// playback
				while (!exitFetchLoop && !liveMPDRefresh)
				{
					bool bCacheFullState = true;
					std::thread *parallelDownload[AAMP_TRACK_COUNT];

					for (int trackIdx = (mNumberOfTracks - 1); trackIdx >= 0; trackIdx--)
					{
						if (parallelDnld && trackIdx > 0) // (trackIdx > 0) indicates video/iframe/audio-only has to be downloaded in sync mode from this FetcherLoop().
						{
							// Download the audio & subtitle fragments in a separate parallel thread.
							parallelDownload[trackIdx] = new std::thread(
												&StreamAbstractionAAMP_MPD::AdvanceTrack,
												this,
												trackIdx,
												trickPlay,
												delta,
												&waitForFreeFrag,
												&exitFetchLoop,
												&bCacheFullState);
						}
						else
						{
							AdvanceTrack(trackIdx, trickPlay, delta, &waitForFreeFrag, &exitFetchLoop, &bCacheFullState);
							parallelDownload[trackIdx] = NULL;
						}
					}

					for (int trackIdx = (mNumberOfTracks - 1); (parallelDnld && trackIdx >= 0); trackIdx--)
					{
						// Join the parallel threads.
						if (parallelDownload[trackIdx])
						{
							parallelDownload[trackIdx]->join();
							delete parallelDownload[trackIdx];
							parallelDownload[trackIdx] = NULL;
						}
					}

					// BCOM-2959  -- Exit from fetch loop for period to be done only after audio and video fetch
					// While playing CDVR with EAC3 audio , durations doesnt match and only video downloads are seen leaving audio behind
					// Audio cache is always full and need for data is not received for more fetch.
					// So after video downloads loop was exiting without audio fetch causing audio drop .
					// Now wait for both video and audio to reach EOS before moving to next period or exit.
					bool vEos = mMediaStreamContext[eMEDIATYPE_VIDEO]->eos;
					bool audioEnabled = (mMediaStreamContext[eMEDIATYPE_AUDIO] && mMediaStreamContext[eMEDIATYPE_AUDIO]->enabled);
					bool aEos = (audioEnabled && mMediaStreamContext[eMEDIATYPE_AUDIO]->eos);
					if (vEos || aEos)
					{
						bool eosOutSideAd = (AdState::IN_ADBREAK_AD_PLAYING != mCdaiObject->mAdState &&
								((rate > 0 && mCurrentPeriodIdx >= upperBoundary) || (rate < 0 && lowerBoundary == mCurrentPeriodIdx)));

						bool eosAdPlayback = (AdState::IN_ADBREAK_AD_PLAYING == mCdaiObject->mAdState &&
								((rate > 0 && mMediaStreamContext[eMEDIATYPE_VIDEO]->fragmentTime >= mLiveEndPosition)
								||(rate < 0 && mMediaStreamContext[eMEDIATYPE_VIDEO]->fragmentTime <= 0)));

						if((!mIsLiveManifest || (rate != AAMP_NORMAL_PLAY_RATE))
							&& (eosOutSideAd || eosAdPlayback))
						{
							if(vEos)
							{
								mMediaStreamContext[eMEDIATYPE_VIDEO]->eosReached = true;
								mMediaStreamContext[eMEDIATYPE_VIDEO]->AbortWaitForCachedAndFreeFragment(false);
							}
							if(audioEnabled)
							{
								if(mMediaStreamContext[eMEDIATYPE_AUDIO]->eos)
								{
									mMediaStreamContext[eMEDIATYPE_AUDIO]->eosReached = true;
									mMediaStreamContext[eMEDIATYPE_AUDIO]->AbortWaitForCachedAndFreeFragment(false);
								}
							}
							else
							{	// No Audio enabled , fake the flag to true
								aEos = true;
							}
						}
						else
						{
							if(!audioEnabled)
							{
								aEos = true;
							}
						}
						// If audio and video reached EOS then only break the fetch loop .
						if(vEos && aEos)
						{
							AAMPLOG_INFO("%s:%d EOS - Exit fetch loop ", __FUNCTION__, __LINE__);
							if(AdState::IN_ADBREAK_AD_PLAYING == mCdaiObject->mAdState)
							{
								adStateChanged = onAdEvent(AdEvent::AD_FINISHED);
							}
							break;
						}
					}
					if (AdState::OUTSIDE_ADBREAK != mCdaiObject->mAdState)
					{
						Period2AdData &curPeriod = mCdaiObject->mPeriodMap[mBasePeriodId];
						if((rate < 0 && mBasePeriodOffset <= 0 ) ||
								(rate > 0 && curPeriod.filled && curPeriod.duration <= (uint64_t)(mBasePeriodOffset * 1000)))
						{
							AAMPLOG_INFO("%s:%d [CDAI]: BasePeriod[%s] completed @%lf. Changing to next ",__FUNCTION__,__LINE__, mBasePeriodId.c_str(),mBasePeriodOffset);
							break;
						}
						else if(lastPrdOffset != mBasePeriodOffset && AdState::IN_ADBREAK_AD_NOT_PLAYING == mCdaiObject->mAdState)
						{
							//In adbreak, but somehow Ad is not playing. Need to check whether the position reached the next Ad start.
							adStateChanged = onAdEvent(AdEvent::BASE_OFFSET_CHANGE);
							if(adStateChanged)
								break;
						}
						lastPrdOffset = mBasePeriodOffset;
					}

					double refreshInterval = MAX_DELAY_BETWEEN_MPD_UPDATE_MS;
                                        std::vector<IPeriod*> availablePeriods = mpd->GetPeriods();
                                        for(auto temp : availablePeriods)
                                        {
						//DELIA-38846: refresh T5 Linear CDAI more frequently to avoid race condition
                                                auto eventStream = temp->GetEventStreams();
                                                if( !(eventStream.empty()) )
                                                {
                                                        hasEventStream = true;
                                                        refreshInterval = mMinUpdateDurationMs;
                                                        break;
                                                }

                                        }
					int timeoutMs = refreshInterval - (int)(aamp_GetCurrentTimeMS() - mLastPlaylistDownloadTimeMs);
					if(timeoutMs <= 0 && mIsLiveManifest && rate > 0)
					{
						liveMPDRefresh = true;
						break;
					}
					else if(bCacheFullState)
					{
						// play cache is full , wait until cache is available to inject next, max wait of 1sec
						int timeoutMs = 200;
						AAMPLOG_TRACE("%s:%d Cache full state,no download until(%d) Time(%lld)",__FUNCTION__, __LINE__,timeoutMs,aamp_GetCurrentTimeMS());
						bool temp =  mMediaStreamContext[eMEDIATYPE_VIDEO]->WaitForFreeFragmentAvailable(timeoutMs);
						if(temp == false)
						{
							AAMPLOG_TRACE("%s:%d:  Waiting for FreeFragmentAvailable", __FUNCTION__, __LINE__);  //CID:82355 - checked return
						}
					}
					else
					{	// This sleep will hit when there is no content to download and cache is not full
						// and refresh interval timeout not reached . To Avoid tight loop adding a min delay
						aamp->InterruptableMsSleep(50);
					}
				} // Loop 3: end of while loop (!exitFetchLoop && !liveMPDRefresh)
				if(liveMPDRefresh)
				{
					break;
				}
				if(AdState::IN_ADBREAK_WAIT2CATCHUP == mCdaiObject->mAdState)
				{
					continue; //Need to finish all the ads in current before period change
				}
				if(rate > 0)
				{
					iPeriod++;
				}
				else
				{
					iPeriod--;
				}
			} //Loop 2: End of Period while loop
			if (exitFetchLoop || (rate < AAMP_NORMAL_PLAY_RATE && iPeriod < 0) || (rate > 1 && iPeriod >= numPeriods) || (!mIsLiveManifest && waitForAdBreakCatchup != true))
			{
				break;
			}
		}
		else
		{
			logprintf("StreamAbstractionAAMP_MPD::%s:%d - null mpd", __FUNCTION__, __LINE__);
		}


		// If it comes here , two reason a) Reached eos b) Need livempdUpdate
		// If liveMPDRefresh is true , that means it already reached 6 sec timeout .
		// 		No more further delay required for mpd update .
		// If liveMPDRefresh is false, then it hit eos . Here the timeout is calculated based
		// on the buffer availability.
		if (!liveMPDRefresh && mLastPlaylistDownloadTimeMs)
		{
			int minDelayBetweenPlaylistUpdates = (int)mMinUpdateDurationMs;
			int timeSinceLastPlaylistDownload = (int)(aamp_GetCurrentTimeMS() - mLastPlaylistDownloadTimeMs);
			long long currentPlayPosition = aamp->GetPositionMilliseconds();
			long long endPositionAvailable = (aamp->culledSeconds + aamp->durationSeconds)*1000;
			// playTarget value will vary if TSB is full and trickplay is attempted. Cant use for buffer calculation
			// So using the endposition in playlist - Current playing position to get the buffer availability
			long bufferAvailable = (endPositionAvailable - currentPlayPosition);

			// If buffer Available is > 2*mMinUpdateDurationMs
			if(bufferAvailable  > (mMinUpdateDurationMs*2) )
			{
				// may be 1.0 times also can be set ???
				minDelayBetweenPlaylistUpdates = (int)(1.5 * mMinUpdateDurationMs);
			}
			// if buffer is between 2*target & mMinUpdateDurationMs
			else if(bufferAvailable  > mMinUpdateDurationMs)
			{
				minDelayBetweenPlaylistUpdates = (int)(0.5 * mMinUpdateDurationMs);
			}
			// This is to handle the case where target duration is high value(>Max delay)  but buffer is available just above the max update inteval
			else if(bufferAvailable > (2*MAX_DELAY_BETWEEN_MPD_UPDATE_MS))
			{
				minDelayBetweenPlaylistUpdates = MAX_DELAY_BETWEEN_MPD_UPDATE_MS;
			}
			// if buffer < targetDuration && buffer < MaxDelayInterval
			else
			{
				// if bufferAvailable is less than targetDuration ,its in RED alert . Close to freeze
				// need to refresh soon ..
				if(bufferAvailable)
				{
					minDelayBetweenPlaylistUpdates = (int)(bufferAvailable / 3) ;
				}
				else
				{
					minDelayBetweenPlaylistUpdates = MIN_DELAY_BETWEEN_MPD_UPDATE_MS; // 500mSec
				}
				// limit the logs when buffer is low
				{
					static int bufferlowCnt;
					if((bufferlowCnt++ & 5) == 0)
					{
						logprintf("Buffer is running low(%ld).Refreshing playlist(%d).PlayPosition(%lld) End(%lld)",
							bufferAvailable,minDelayBetweenPlaylistUpdates,currentPlayPosition,endPositionAvailable);
					}
				}

			}

			// First cap max limit ..
			// remove already consumed time from last update
			// if time interval goes negative, limit to min value

			// restrict to Max delay interval
			if ( hasEventStream && (minDelayBetweenPlaylistUpdates > mMinUpdateDurationMs) )
			{
				minDelayBetweenPlaylistUpdates = (int)mMinUpdateDurationMs;
			}
			if (minDelayBetweenPlaylistUpdates > MAX_DELAY_BETWEEN_MPD_UPDATE_MS)
			{
				minDelayBetweenPlaylistUpdates = MAX_DELAY_BETWEEN_MPD_UPDATE_MS;
			}

			// adjust with last refreshed time interval
			minDelayBetweenPlaylistUpdates -= timeSinceLastPlaylistDownload;

			if(minDelayBetweenPlaylistUpdates < MIN_DELAY_BETWEEN_MPD_UPDATE_MS)
			{
				// minimum of 500 mSec needed to avoid too frequent download.
				minDelayBetweenPlaylistUpdates = MIN_DELAY_BETWEEN_MPD_UPDATE_MS;
			}

			AAMPLOG_INFO("aamp playlist end refresh bufferMs(%ld) delay(%d) delta(%d) End(%lld) PlayPosition(%lld)",
				bufferAvailable,minDelayBetweenPlaylistUpdates,timeSinceLastPlaylistDownload,endPositionAvailable,currentPlayPosition);

			// sleep before next manifest update
			aamp->InterruptableMsSleep(minDelayBetweenPlaylistUpdates);
		}
		if (!aamp->DownloadsAreEnabled() || UpdateMPD() != eAAMPSTATUS_OK)
		{
			break;
		}

		if(mIsLiveStream)
		{
			//Periods could be added or removed, So select period based on periodID
			//If period ID not found in MPD that means it got culled, in that case select
			// first period
			vector<IPeriod *> periods = mpd->GetPeriods();
			int iter = periods.size() - 1;
			mCurrentPeriodIdx = 0;
			while(iter > 0)
			{
				if(mBasePeriodId == periods.at(iter)->GetId())
				{
					mCurrentPeriodIdx = iter;
					break;
				}
				iter--;
			}
			mFirstPeriodStartTime = GetPeriodStartTime(mpd,0);
			AAMPLOG_INFO("Updating period index after mpd refresh, mFirstPeriodStartTime: %lf", mFirstPeriodStartTime);
		}
		else
		{
			// DELIA-31750 - looping of cdvr video - Issue happens with multiperiod content only
			// When playback is near live position (last period) or after eos in period
			// mCurrentPeriodIdx was resetted to 0 . This caused fetch loop to continue from Period 0/fragement 1
			// Reset of mCurrentPeriodIdx to be done to max period if Period count changes after mpd refresh
			size_t newPeriods = mpd->GetPeriods().size();
			if(mCurrentPeriodIdx > (newPeriods - 1))
			{
				logprintf("MPD Fragment Collector detected reset in Period(New Size:%zu)(currentIdx:%d->%zu)",
					newPeriods,mCurrentPeriodIdx,newPeriods - 1);
				mCurrentPeriodIdx = newPeriods - 1;
			}
		}
		mpdChanged = true;
	}		//Loop 1
	while (!exitFetchLoop);
	logprintf("MPD fragment collector done");
}


/**
 * @brief Check new early available periods
 * @param vector of new Early Available Perids
 */
void StreamAbstractionAAMP_MPD::GetAvailableVSSPeriods(std::vector<IPeriod*>& PeriodIds)
{
	for(IPeriod* tempPeriod : mpd->GetPeriods())
	{
		if (STARTS_WITH_IGNORE_CASE(tempPeriod->GetId().c_str(), VSS_DASH_EARLY_AVAILABLE_PERIOD_PREFIX))
		{
			if(1 == tempPeriod->GetAdaptationSets().size() && IsEmptyPeriod(tempPeriod))
			{
				if(std::find(mEarlyAvailablePeriodIds.begin(), mEarlyAvailablePeriodIds.end(), tempPeriod->GetId()) == mEarlyAvailablePeriodIds.end())
				{
					PeriodIds.push_back(tempPeriod);
				}
			}
		}
	}
}


/**
 * @brief Check for VSS tags
 * @retval bool true if found, false otherwise
 */
bool StreamAbstractionAAMP_MPD::CheckForVssTags()
{
	bool isVss = false;
	IMPDElement* nodePtr = mpd;

	if (!nodePtr)
	{
		AAMPLOG_ERR("%s:%d > API Failed due to Invalid Arguments", __FUNCTION__, __LINE__);
	}
	else
	{
		for (INode* childNode : nodePtr->GetAdditionalSubNodes())
		{
			const std::string& name = childNode->GetName();
			if (name == SUPPLEMENTAL_PROPERTY_TAG)
			{
				if (childNode->HasAttribute("schemeIdUri"))
				{
#ifdef AAMP_RFC_ENABLED
					const std::string& schemeIdUri = childNode->GetAttributeValue("schemeIdUri");
					const std::string& schemeIdUriVss = RFCSettings::getSchemeIdUriVssStream();
					if (schemeIdUri == schemeIdUriVss)
					{
						if (childNode->HasAttribute("value"))
						{
							std::string value = childNode->GetAttributeValue("value");
							mCommonKeyDuration = std::stoi(value);
							AAMPLOG_INFO("%s:%d Recieved Common Key Duration : %d of VSS stream", __FUNCTION__, __LINE__, mCommonKeyDuration);
							isVss = true;
						}
					}
#endif
				}
			}
		}
	}

	return isVss;
}


/**
 * @brief GetVssVirtualStreamID from manifest
 * @retval return Virtual stream ID string
 */
std::string StreamAbstractionAAMP_MPD::GetVssVirtualStreamID()
{
	std::string ret;
	IMPDElement* nodePtr = mpd;

	if (!nodePtr)
	{
		AAMPLOG_ERR("%s:%d > API Failed due to Invalid Arguments", __FUNCTION__, __LINE__);
	}
	else
	{
		for (auto* childNode : mpd->GetProgramInformations())
		{
				for (auto infoNode : childNode->GetAdditionalSubNodes())
				{
					std::string subNodeName;
					std::string ns;
					ParseXmlNS(infoNode->GetName(), ns, subNodeName);
					const std::string& infoNodeType = infoNode->GetAttributeValue("type");
					if ((subNodeName == "ContentIdentifier") && (infoNodeType == "URI" || infoNodeType == "URN"))
					{
						if (infoNode->HasAttribute("value"))
						{
							std::string value = infoNode->GetAttributeValue("value");
							if (value.find(VSS_VIRTUAL_STREAM_ID_PREFIX) != std::string::npos)
							{
								ret = value.substr(sizeof(VSS_VIRTUAL_STREAM_ID_PREFIX)-1);
								AAMPLOG_INFO("%s:%d Parsed Virtual Stream ID from manifest:%s", __FUNCTION__, __LINE__, ret.c_str());
								break;
							}
						}
					}
				}
			if (!ret.empty())
			{
				break;
			}
		}
	}
	return ret;
}

/**
 * @brief StreamAbstractionAAMP_MPD Destructor
 */
StreamAbstractionAAMP_MPD::~StreamAbstractionAAMP_MPD()
{
	for (int iTrack = 0; iTrack < mMaxTracks; iTrack++)
	{
		MediaStreamContext *track = mMediaStreamContext[iTrack];
		if(track)
		{
			delete track;
		}
	}

	aamp->SyncBegin();
	if (mpd)
	{
		delete mpd;
		mpd = NULL;
	}

	if(mStreamInfo)
	{
		delete[] mStreamInfo;
	}

	if(!indexedTileInfo.empty())
	{
		deIndexTileInfo(indexedTileInfo);
	}

	if(!thumbnailtrack.empty())
	{
		int size = thumbnailtrack.size();
		for(int i = 0; i < size ; i++)
		{
			StreamInfo *tmp = thumbnailtrack[i];
			if(tmp)
			{
				delete tmp;
			}
		}
	}

	aamp->CurlTerm(eCURLINSTANCE_VIDEO, AAMP_TRACK_COUNT);

	aamp->SyncEnd();
}

/**
 *   @brief  Starts streaming.
 */
void StreamAbstractionAAMP_MPD::Start(void)
{
#ifdef AAMP_MPD_DRM
	aamp->mDRMSessionManager->setSessionMgrState(SessionMgrState::eSESSIONMGR_ACTIVE);
#endif
	fragmentCollectorThreadID = new std::thread(&StreamAbstractionAAMP_MPD::FetcherLoop, this);
	fragmentCollectorThreadStarted = true;
	for (int i = 0; i < mNumberOfTracks; i++)
	{
		if(aamp->IsPlayEnabled())
		{
			mMediaStreamContext[i]->StartInjectLoop();
		}
	}
}

/**
*   @brief  Stops streaming.
*
*   @param  clearChannelData - ignored.
*/
void StreamAbstractionAAMP_MPD::Stop(bool clearChannelData)
{
	aamp->DisableDownloads();
	ReassessAndResumeAudioTrack(true);
	AbortWaitForAudioTrackCatchup(false);
	// DELIA-45035: Change order of stopping threads. Collector thread has to be stopped at the earliest
	// There is a chance fragment collector is processing StreamSelection() which can change the mNumberOfTracks
	// and Enabled() status of MediaTrack momentarily.
	// Call AbortWaitForCachedAndFreeFragment() to unblock collector thread from WaitForFreeFragmentAvailable
	for (int iTrack = 0; iTrack < mMaxTracks; iTrack++)
	{
		MediaStreamContext *track = mMediaStreamContext[iTrack];
		if(track)
		{
			track->AbortWaitForCachedAndFreeFragment(true);
		}
	}

	if(fragmentCollectorThreadStarted)
	{
		fragmentCollectorThreadID->join();
		fragmentCollectorThreadStarted = false;
	}

	for (int iTrack = 0; iTrack < mMaxTracks; iTrack++)
	{
		MediaStreamContext *track = mMediaStreamContext[iTrack];
		if(track && track->Enabled())
		{
			track->StopInjectLoop();
			if (iTrack == eMEDIATYPE_SUBTITLE && track->mSubtitleParser)
			{
				track->mSubtitleParser->reset();
			}
		}
	}

	if(drmSessionThreadStarted)
	{
		AAMPLOG_INFO("Waiting to join CreateDRMSession thread");
		int rc = pthread_join(createDRMSessionThreadID, NULL);
		if (rc != 0)
		{
			logprintf("pthread_join returned %d for createDRMSession Thread", rc);
		}
		AAMPLOG_INFO("Joined CreateDRMSession thread");
		drmSessionThreadStarted = false;
	}

	if(deferredDRMRequestThreadStarted)
	{
		if((deferredDRMRequestThread) && (deferredDRMRequestThread->joinable()))
		{
			mAbortDeferredLicenseLoop = true;
			deferredDRMRequestThread->join();
			delete deferredDRMRequestThread;
			deferredDRMRequestThread = NULL;
		}
	}

	aamp->mStreamSink->ClearProtectionEvent();
 #ifdef AAMP_MPD_DRM
	aamp->mDRMSessionManager->setSessionMgrState(SessionMgrState::eSESSIONMGR_INACTIVE);
  #endif

	aamp->EnableDownloads();
}

/**
 * @brief Stub implementation
 */
void StreamAbstractionAAMP_MPD::DumpProfiles(void)
{ // STUB
}

/**
 * @brief Get output format of stream.
 *
 * @param[out]  primaryOutputFormat - format of primary track
 * @param[out]  audioOutputFormat - format of audio track
 */
void StreamAbstractionAAMP_MPD::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxOutputFormat)
{
	if(mMediaStreamContext[eMEDIATYPE_VIDEO] && mMediaStreamContext[eMEDIATYPE_VIDEO]->enabled )
	{
		primaryOutputFormat = FORMAT_ISO_BMFF;
	}
	else
	{
		primaryOutputFormat = FORMAT_INVALID;
	}
	if(mMediaStreamContext[eMEDIATYPE_AUDIO] && mMediaStreamContext[eMEDIATYPE_AUDIO]->enabled )
	{
		audioOutputFormat = FORMAT_ISO_BMFF;
	}
	else
	{
		audioOutputFormat = FORMAT_INVALID;
	}
	//RDK-27796, if subtitle is disabled, but aux is enabled, then its status is saved in place of eMEDIATYPE_SUBTITLE
	if ((mMediaStreamContext[eMEDIATYPE_AUX_AUDIO] && mMediaStreamContext[eMEDIATYPE_AUX_AUDIO]->enabled) ||
		(mMediaStreamContext[eMEDIATYPE_SUBTITLE] && mMediaStreamContext[eMEDIATYPE_SUBTITLE]->enabled && mMediaStreamContext[eMEDIATYPE_SUBTITLE]->type == eTRACK_AUX_AUDIO))
	{
		auxOutputFormat = FORMAT_ISO_BMFF;
	}
	else
	{
		auxOutputFormat = FORMAT_INVALID;
	}
}

/**
 *   @brief Return MediaTrack of requested type
 *
 *   @param[in]  type - track type
 *   @retval MediaTrack pointer.
 */
MediaTrack* StreamAbstractionAAMP_MPD::GetMediaTrack(TrackType type)
{
	return mMediaStreamContext[type];
}

double StreamAbstractionAAMP_MPD::GetBufferedDuration()
{
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	double retval = -1.0;
	if (video && video->enabled)
	{
		retval = video->GetBufferedDuration();
	}
	return retval;
}


/**
 * @brief Get current stream position.
 *
 * @retval current position of stream.
 */
double StreamAbstractionAAMP_MPD::GetStreamPosition()
{
	return seekPosition;
}

/**
 * @brief Get Period Start Time.
 *
 * @retval Period Start Time.
 */
double StreamAbstractionAAMP_MPD::GetFirstPeriodStartTime(void)
{
	return mFirstPeriodStartTime;
}

/**
 * @brief Gets number of profiles
 * @retval number of profiles
 */
int StreamAbstractionAAMP_MPD::GetProfileCount()
{
	int ret = 0;
	bool isFogTsb = mIsFogTSB && !mAdPlayingFromCDN;

	if(isFogTsb)
	{
		ret = mBitrateIndexVector.size();
	}
	else
	{
		ret = GetABRManager().getProfileCount();
	}
	return ret;
}


/**
 * @brief Get profile index for TsbBandwidth
 * @param bandwidth - bandwidth to identify profile index from list
 * @retval profile index of the current bandwidth
 */
int StreamAbstractionAAMP_MPD::GetProfileIndexForBandwidth(long mTsbBandwidth)
{
	int profileIndex = 0;
	bool isFogTsb = mIsFogTSB && !mAdPlayingFromCDN;

	if(isFogTsb)
	{
			std::vector<long>::iterator it = std::find(mBitrateIndexVector.begin(), mBitrateIndexVector.end(), mTsbBandwidth);

			if (it != mBitrateIndexVector.end())
			{
					// Get index of element from iterator
					profileIndex = std::distance(mBitrateIndexVector.begin(), it);
			}
	}
	else
	{
			profileIndex = GetABRManager().getBestMatchedProfileIndexByBandWidth(mTsbBandwidth);
	}
	return profileIndex;
}

/**
 *   @brief Get stream information of a profile from subclass.
 *
 *   @param[in]  idx - profile index.
 *   @retval stream information corresponding to index.
 */
StreamInfo* StreamAbstractionAAMP_MPD::GetStreamInfo(int idx)
{
	bool isFogTsb = mIsFogTSB && !mAdPlayingFromCDN;
	assert(idx < GetProfileCount());
	if (isFogTsb)
	{
		return &mStreamInfo[idx];
	}
	else
	{
		int userData = 0;

		if (GetProfileCount() && !aamp->IsTSBSupported()) // avoid calling getUserDataOfProfile() for playlist only URL playback.
		{
			userData = GetABRManager().getUserDataOfProfile(idx);
		}
		return &mStreamInfo[userData];
	}
}


/**
 *   @brief  Get PTS of first sample.
 *
 *   @retval PTS of first sample
 */
double StreamAbstractionAAMP_MPD::GetFirstPTS()
{
	return mFirstPTS;
}

/**
 *   @brief  Get Start time PTS of first sample.
 *
 *   @retval start time of first sample
 */
double StreamAbstractionAAMP_MPD::GetStartTimeOfFirstPTS()
{
	return mStartTimeOfFirstPTS;
}

/**
 * @brief Get index of profile corresponds to bandwidth
 * @param[in] bitrate Bitrate to lookup profile
 * @retval profile index
 */
int StreamAbstractionAAMP_MPD::GetBWIndex(long bitrate)
{
	int topBWIndex = 0;
	int profileCount = GetProfileCount();
	if (profileCount)
	{
		for (int i = 0; i < profileCount; i++)
		{
			StreamInfo *streamInfo = &mStreamInfo[i];
			if (!streamInfo->isIframeTrack && streamInfo->enabled && streamInfo->bandwidthBitsPerSecond > bitrate)
			{
				--topBWIndex;
			}
		}
	}
	return topBWIndex;
}

/**
 * @brief To get the available video bitrates.
 * @ret available video bitrates
 */
std::vector<long> StreamAbstractionAAMP_MPD::GetVideoBitrates(void)
{
	std::vector<long> bitrates;
	int profileCount = GetProfileCount();
	bitrates.reserve(profileCount);
	if (profileCount)
	{
		for (int i = 0; i < profileCount; i++)
		{
			StreamInfo *streamInfo = &mStreamInfo[i];
			if (!streamInfo->isIframeTrack)
			{
				bitrates.push_back(streamInfo->bandwidthBitsPerSecond);
			}
		}
	}
	return bitrates;
}

/*
* @brief Gets Max Bitrate avialable for current playback.
* @ret long MAX video bitrates
*/
long StreamAbstractionAAMP_MPD::GetMaxBitrate()
{
	long maxBitrate = 0;
	if( mIsFogTSB )
	{
		maxBitrate = mMaxTSBBandwidth;
	}
	else
	{

		maxBitrate = StreamAbstractionAAMP::GetMaxBitrate();
	}

	return maxBitrate;
}


/**
 * @brief To get the available audio bitrates.
 * @ret available audio bitrates
 */
std::vector<long> StreamAbstractionAAMP_MPD::GetAudioBitrates(void)
{
	std::vector<long> audioBitrate;
	int trackSize = mAudioTracks.size();
	if(trackSize)
	{
		audioBitrate.reserve(trackSize);
		std::vector<AudioTrackInfo>::iterator itr;

		for(itr = mAudioTracks.begin(); itr != mAudioTracks.end(); itr++)
		{
			audioBitrate.push_back(itr->bandwidth);
		}
	}
	return audioBitrate;
}

static void indexThumbnails(dash::mpd::IMPD *mpd, int thumbIndexValue, std::vector<TileInfo> &indexedTileInfo,std::vector<StreamInfo*> &thumbnailtrack)
{
	bool ret = false;
	logprintf("Entering  %s.",__FUNCTION__);
	bool trackEmpty = thumbnailtrack.empty();
	if(trackEmpty || indexedTileInfo.empty())
	{
		int w, h, bandwidth = 0, periodIndex = 0, idx = 0;
		bool isAdPeriod = true, done = false;
		double adDuration = 0;
		long int prevStartNumber = -1;
		{
			for(IPeriod* tempPeriod : mpd->GetPeriods())
			{
				const std::vector<IAdaptationSet *> adaptationSets = tempPeriod->GetAdaptationSets();
				int adSize = adaptationSets.size();
				for(int j =0; j < adSize; j++)
				{
					if( IsContentType(adaptationSets.at(j), eMEDIATYPE_IMAGE) )
					{
						isAdPeriod = false;
						const std::vector<IRepresentation *> representation = adaptationSets.at(j)->GetRepresentation();
						for (int repIndex = 0; repIndex < representation.size(); repIndex++)
						{
							const dash::mpd::IRepresentation *rep = representation.at(repIndex);
							const std::vector<INode *> subnodes = rep->GetAdditionalSubNodes();
							for (unsigned i = 0; i < subnodes.size() && !done; i++)
							{
								INode *xml = subnodes[i];
								if(xml != NULL)
								{
									if (xml->GetName() == "EssentialProperty")
									{
										if (xml->HasAttribute("schemeIdUri"))
										{
											const std::string& schemeUri = xml->GetAttributeValue("schemeIdUri");
											if (schemeUri == "http://dashif.org/guidelines/thumbnail_tile")
											{
												traceprintf("schemeuri = thumbnail_tile");
											}
											else
											{
												logprintf("%s:%d - skipping schemeUri %s", __FUNCTION__, __LINE__, schemeUri.c_str());
											}
										}
										if(xml->HasAttribute("value"))
										{
											const std::string& value = xml->GetAttributeValue("value");
											if(!value.empty())
											{
												sscanf(value.c_str(), "%dx%d",&w,&h);
												logprintf("%s:%d - value=%dx%d", __FUNCTION__, __LINE__,w,h);
												done = true;
											}
										}
									}
									else
									{
										logprintf("%s:%d - skipping name %s", __FUNCTION__, __LINE__, xml->GetName().c_str());
									}
								}
								else
								{
									AAMPLOG_WARN("%s:%d :  xml is null", __FUNCTION__, __LINE__);  //CID:81118 - Null Returns
								}
							}	// end of sub node loop
							bandwidth = rep->GetBandwidth();
							if(thumbIndexValue < 0 || trackEmpty)
							{
								std::string mimeType = rep->GetMimeType();
								StreamInfo *tmp = new StreamInfo;
								tmp->bandwidthBitsPerSecond = (long) bandwidth;
								tmp->resolution.width = rep->GetWidth()/w;
								tmp->resolution.height = rep->GetHeight()/h;
								thumbnailtrack.push_back(tmp);
								traceprintf("In %s thumbnailtrack bandwidth=%d width=%d height=%d",__FUNCTION__, tmp->bandwidthBitsPerSecond, tmp->resolution.width, tmp->resolution.height);
							}
							if((thumbnailtrack.size() > thumbIndexValue) && thumbnailtrack[thumbIndexValue]->bandwidthBitsPerSecond == (long)bandwidth)
							{
								const ISegmentTemplate *segRep = NULL;
								const ISegmentTemplate *segAdap = NULL;
								segAdap = adaptationSets.at(j)->GetSegmentTemplate();
								segRep = representation.at(repIndex)->GetSegmentTemplate();
								SegmentTemplates segmentTemplates(segRep, segAdap);
								if( segmentTemplates.HasSegmentTemplate() )
								{
									const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
									uint32_t timeScale = segmentTemplates.GetTimescale();
									long int startNumber = segmentTemplates.GetStartNumber();
									std::string media = segmentTemplates.Getmedia();
									if (segmentTimeline)
									{
										traceprintf("In %s - segment timeline",__FUNCTION__);
										std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
										int timeLineIndex = 0;
										uint64_t durationMs = 0;
										while (timeLineIndex < timelines.size())
										{
											if( prevStartNumber == startNumber )
											{
												/* TODO: This is temporary workaround for MT Ad streams which has redunant tile information
												   This entire condition has to be removed once the manifest is fixed. */
												timeLineIndex++;
												startNumber++;
												continue;
											}
											ITimeline *timeline = timelines.at(timeLineIndex);
											double startTime = timeline->GetStartTime() / timeScale;
											int repeatCount = timeline->GetRepeatCount();
											uint32_t timelineDurationMs = ComputeFragmentDuration(timeline->GetDuration(),timeScale);
											while( repeatCount-- >= 0 )
											{
												std::string tmedia = media;
												TileInfo tileInfo;
												memset( &tileInfo,0,sizeof(tileInfo) );
												tileInfo.startTime = startTime + ( adDuration / timeScale) ;
												traceprintf("In %s timeLineIndex[%d] size [%lu] updated durationMs[%" PRIu64 "] startTime:%f adDuration:%f repeatCount:%d", __FUNCTION__, timeLineIndex, timelines.size(), durationMs, startTime, adDuration, repeatCount);
												startTime += ( timelineDurationMs );
												replace(tmedia, "Number", startNumber);
												char *ptr = strndup(tmedia.c_str(), tmedia.size());
												tileInfo.url = ptr;
												traceprintf("tileInfo.url%s:%p",tileInfo.url, ptr);
												tileInfo.posterDuration = ((double)segmentTemplates.GetDuration()) / (timeScale * w * h);
												tileInfo.tileSetDuration = ComputeFragmentDuration(timeline->GetDuration(), timeScale);
												tileInfo.numRows = h;
												tileInfo.numCols = w;
												traceprintf("TileInfo - StartTime:%f posterDuration:%d tileSetDuration:%f numRows:%d numCols:%d",tileInfo.startTime,tileInfo.posterDuration,tileInfo.tileSetDuration,tileInfo.numRows,tileInfo.numCols);
												indexedTileInfo.push_back(tileInfo);
												startNumber++;
											}
											timeLineIndex++;
										}	// emd of timeLine loop
										prevStartNumber = startNumber - 1;
									}
									else
									{
										// Segment base.
									}
								}
							}
						}	// end of representation loop
					}	// if content type is IMAGE
				}	// end of adaptation set loop
				if((thumbIndexValue < 0) && done)
				{
					break;
				}
				if ( isAdPeriod )
				{
					adDuration += aamp_GetPeriodDuration(mpd, periodIndex, 0);
				}
				isAdPeriod = true;
				periodIndex++;
			}	// end of Period loop
		}	// end of thumbnail track size
	}
	logprintf("Exiting %s.",__FUNCTION__);
}

/**
 * @brief To get the available thumbnail tracks.
 * @ret available thumbnail tracks.
 */
std::vector<StreamInfo*> StreamAbstractionAAMP_MPD::GetAvailableThumbnailTracks(void)
{
	if(thumbnailtrack.empty())
	{
		indexThumbnails(mpd, -1, indexedTileInfo, thumbnailtrack);
	}
	return thumbnailtrack;
}

/**
 * @fn SetThumbnailTrack
 * @brief Function to set thumbnail track for processing
 *
 * @param thumbnail index value indicating the track to select
 * @return bool true on success.
 */
bool StreamAbstractionAAMP_MPD::SetThumbnailTrack(int thumbnailIndex)
{
	bool ret = false;
	if(aamp->mthumbIndexValue != thumbnailIndex)
	{
		if(thumbnailIndex < thumbnailtrack.size() || thumbnailtrack.empty())
		{
			deIndexTileInfo(indexedTileInfo);
			indexThumbnails(mpd, thumbnailIndex, indexedTileInfo, thumbnailtrack);
			if(!indexedTileInfo.empty())
			{
				aamp->mthumbIndexValue = thumbnailIndex;
				ret = true;
			}
		}
	}
	else
	{
		ret = true;
	}
	return ret;
}

/**
 * @fn GetThumbnailRangeData
 * @brief Function to fetch the thumbnail data.
 *
 * @param tStart start duration of thumbnail data.
 * @param tEnd end duration of thumbnail data.
 * @param *baseurl base url of thumbnail images.
 * @param *raw_w absolute width of the thumbnail spritesheet.
 * @param *raw_h absolute height of the thumbnail spritesheet.
 * @param *width width of each thumbnail tile.
 * @param *height height of each thumbnail tile.
 * @return Updated vector of available thumbnail data.
 */
std::vector<ThumbnailData> StreamAbstractionAAMP_MPD::GetThumbnailRangeData(double tStart, double tEnd, std::string *baseurl, int *raw_w, int *raw_h, int *width, int *height)
{
        std::vector<ThumbnailData> data;
	if(indexedTileInfo.empty())
	{
		if(aamp->mthumbIndexValue >= 0)
		{
			logprintf("In %s calling indexthumbnail",__FUNCTION__);
			deIndexTileInfo(indexedTileInfo);
			indexThumbnails(mpd, aamp->mthumbIndexValue, indexedTileInfo, thumbnailtrack);
		}
		else
		{
			logprintf("Exiting %s. Thumbnail track not configured!!!.",__FUNCTION__);
			return data;
		}
	}

	ThumbnailData tmpdata;
	double totalSetDuration = 0;
	bool updateBaseParam = true;
	for(int i = 0; i< indexedTileInfo.size(); i++)
	{
		TileInfo &tileInfo = indexedTileInfo[i];
		tmpdata.t = tileInfo.startTime;
		if( tmpdata.t > tEnd )
		{
			break;
		}
		double tileSetEndTime = tmpdata.t + tileInfo.tileSetDuration;
		totalSetDuration += tileInfo.tileSetDuration;
		if( tileSetEndTime < tStart )
		{
			continue;
		}
		tmpdata.url = tileInfo.url;
		tmpdata.d = tileInfo.posterDuration;
		bool done = false;
		for( int row=0; row<tileInfo.numRows && !done; row++ )
		{
			for( int col=0; col<tileInfo.numCols && !done; col++ )
			{
				double tNext = tmpdata.t+tileInfo.posterDuration;
				if( tNext >= tileSetEndTime )
				{
					tmpdata.d = tileSetEndTime - tmpdata.t;
					done = true;
				}
				if( tEnd >= tmpdata.t && tStart < tNext  )
				{
					tmpdata.x = col * thumbnailtrack[aamp->mthumbIndexValue]->resolution.width;
					tmpdata.y = row * thumbnailtrack[aamp->mthumbIndexValue]->resolution.height;
					data.push_back(tmpdata);
				}
				tmpdata.t = tNext;
			}
		}
		if(updateBaseParam)
		{
			updateBaseParam = false;
			std::string url;
			if( url.compare(0, 7, "http://")==0 || url.compare(0, 8, "https://")==0 )
			{
				url = tmpdata.url;
				*baseurl = url.substr(0,url.find_last_of("/\\")+1);
			}
			else
			{
				const std::vector<IBaseUrl *>*baseUrls = &mpd->GetBaseUrls();
				if ( baseUrls->size() > 0 )
				{
					*baseurl = baseUrls->at(0)->GetUrl();
				}
				else
				{
					url = aamp->GetManifestUrl();
					*baseurl = url.substr(0,url.find_last_of("/\\")+1);
				}
			}
			*width = thumbnailtrack[aamp->mthumbIndexValue]->resolution.width;
			*height = thumbnailtrack[aamp->mthumbIndexValue]->resolution.height;
			*raw_w = thumbnailtrack[aamp->mthumbIndexValue]->resolution.width * tileInfo.numCols;
			*raw_h = thumbnailtrack[aamp->mthumbIndexValue]->resolution.height * tileInfo.numRows;
		}
	}
	return data;
}

/**
*   @brief  Stops injecting fragments to StreamSink.
*/
void StreamAbstractionAAMP_MPD::StopInjection(void)
{
	//invoked at times of discontinuity. Audio injection loop might have already exited here
	ReassessAndResumeAudioTrack(true);
	for (int iTrack = 0; iTrack < mNumberOfTracks; iTrack++)
	{
		MediaStreamContext *track = mMediaStreamContext[iTrack];
		if(track && track->Enabled())
		{
			track->AbortWaitForCachedFragment();
			aamp->StopTrackInjection((MediaType) iTrack);
			track->StopInjectLoop();
		}
	}
}
/**
*   @brief  Start injecting fragments to StreamSink.
*/
void StreamAbstractionAAMP_MPD::StartInjection(void)
{
	mTrackState = eDISCONTIUITY_FREE;
	for (int iTrack = 0; iTrack < mNumberOfTracks; iTrack++)
	{
		MediaStreamContext *track = mMediaStreamContext[iTrack];
		if(track && track->Enabled())
		{
			aamp->ResumeTrackInjection((MediaType) iTrack);
			track->StartInjectLoop();
		}
	}
}


void StreamAbstractionAAMP_MPD::SetCDAIObject(CDAIObject *cdaiObj)
{
	if(cdaiObj)
	{
		CDAIObjectMPD *cdaiObjMpd = static_cast<CDAIObjectMPD *>(cdaiObj);
		mCdaiObject = cdaiObjMpd->GetPrivateCDAIObjectMPD();
	}
}
/**
 *   @brief Check whether the period has any valid ad.
 *
 *   @param[in] Period instance.
 *   @param[in] Break start time in milli seconds.
 *   @param[in] vector of pairs of SCTE35 binary object and corresponding duration.
 */
bool StreamAbstractionAAMP_MPD::isAdbreakStart(IPeriod *period, uint64_t &startMS, std::vector<std::pair<std::string, uint32_t>> &scte35Vec)
{
	const std::vector<IEventStream *> &eventStreams = period->GetEventStreams();
	bool ret = false;
	uint32_t duration = 0;
	for(auto &eventStream: eventStreams)
	{
		for(auto &event: eventStream->GetEvents())
		{
			//Currently for linear assets only the SCTE35 events having 'duration' tag present are considered for generating timedMetadat Events.
			//For VOD assets the events are generated irrespective of the 'duration' tag present or not
			if(event && (!mIsLiveManifest || 0 != event->GetDuration()))
			{
				for(auto &evtChild: event->GetAdditionalSubNodes())
				{
					std::string prefix = "scte35:";
					if(evtChild != NULL)
					{

						if(evtChild->HasAttribute("xmlns") && "http://www.scte.org/schemas/35/2016" == evtChild->GetAttributeValue("xmlns"))
						{
							//scte35 namespace defined here. Hence, this & children don't need the prefix 'scte35'
							prefix = "";
						}

						if(prefix+"Signal" == evtChild->GetName())
						{
							for(auto &signalChild: evtChild->GetNodes())
							{
								if(signalChild && prefix+"Binary" == signalChild->GetName())
								{
									uint32_t timeScale = 1;
									if(eventStream->GetTimescale() > 1)
									{
										timeScale = eventStream->GetTimescale();
									}
									//With the current implementation, ComputeFragmentDuration returns 2000 when the event->GetDuration() returns '0'
									//This is not desirable for VOD assets (for linear event->GetDuration() with 0 value will not bring the control to this point)
									if(0 != event->GetDuration())
									{
										duration = ComputeFragmentDuration(event->GetDuration(), timeScale) * 1000; //milliseconds
									}
									else
									{
										//Control gets here only for VOD with no duration data for the event, here set 0 as duration intead of the default 2000
										duration = 0;
									}
									std::string scte35 = signalChild->GetText();
									if(0 != scte35.length())
									{
										scte35Vec.push_back({scte35, duration});
										if(mIsLiveManifest)
										{
											return true;
										}
										else
										{
											ret = true;
											continue;
										}
									}
									else
									{
										AAMPLOG_WARN("%s:%d [CDAI]: Found a scte35:Binary in manifest with empty binary data!!",__FUNCTION__,__LINE__);
									}
								}
								else
								{
									AAMPLOG_WARN("%s:%d [CDAI]: Found a scte35:Signal in manifest without scte35:Binary!!",__FUNCTION__,__LINE__);
								}
							}
						}
					}
					else
					{
						AAMPLOG_WARN("%s:%d :  evtChild is null", __FUNCTION__, __LINE__);  //CID:85816 - Null Return
					}
				}
			}
		}
	}
	return ret;
}
bool StreamAbstractionAAMP_MPD::onAdEvent(AdEvent evt)
{
	double adOffset  = 0.0;   //CID:89257 - Intialization
	return onAdEvent(evt, adOffset);
}

bool StreamAbstractionAAMP_MPD::onAdEvent(AdEvent evt, double &adOffset)
{
	if(!ISCONFIGSET(eAAMPConfig_EnableClientDai))
	{
		return false;
	}
	std::lock_guard<std::mutex> lock(mCdaiObject->mDaiMtx);
	bool stateChanged = false;
	AdState oldState = mCdaiObject->mAdState;
	AAMPEventType reservationEvt2Send = AAMP_MAX_NUM_EVENTS; //None
	std::string adbreakId2Send("");
	AAMPEventType placementEvt2Send = AAMP_MAX_NUM_EVENTS; //None
	std::string adId2Send("");
	uint32_t adPos2Send = 0;
	bool sendImmediate = false;
	switch(mCdaiObject->mAdState)
	{
		case AdState::OUTSIDE_ADBREAK:
			if(AdEvent::DEFAULT == evt || AdEvent::INIT == evt)
			{
				std::string brkId = "";
				int adIdx = mCdaiObject->CheckForAdStart(rate, (AdEvent::INIT == evt), mBasePeriodId, mBasePeriodOffset, brkId, adOffset);
				if(!brkId.empty())
				{
					AAMPLOG_INFO("%s:%d [CDAI] CheckForAdStart found Adbreak. adIdx[%d] mBasePeriodOffset[%lf] adOffset[%lf].",__FUNCTION__,__LINE__, adIdx, mBasePeriodOffset, adOffset);

					mCdaiObject->mCurPlayingBreakId = brkId;
					if(-1 != adIdx && mCdaiObject->mAdBreaks[brkId].ads)
					{
						if(!(mCdaiObject->mAdBreaks[brkId].ads->at(adIdx).invalid))
						{
							AAMPLOG_WARN("%s:%d [CDAI]: STARTING ADBREAK[%s] AdIdx[%d] Found at Period[%s].",__FUNCTION__,__LINE__, brkId.c_str(), adIdx, mBasePeriodId.c_str());
							mCdaiObject->mCurAds = mCdaiObject->mAdBreaks[brkId].ads;

							mCdaiObject->mCurAdIdx = adIdx;
							mCdaiObject->mAdState = AdState::IN_ADBREAK_AD_PLAYING;

							for(int i=0; i<adIdx; i++)
								adPos2Send += mCdaiObject->mCurAds->at(i).duration;
						}
						else
						{
							AAMPLOG_WARN("%s:%d [CDAI]: AdIdx[%d] in the AdBreak[%s] is invalid. Skipping.",__FUNCTION__,__LINE__, adIdx, brkId.c_str());
						}
						reservationEvt2Send = AAMP_EVENT_AD_RESERVATION_START;
						adbreakId2Send = brkId;
						if(AdEvent::INIT == evt) sendImmediate = true;
					}

					if(AdState::IN_ADBREAK_AD_PLAYING != mCdaiObject->mAdState)
					{
						AAMPLOG_WARN("%s:%d [CDAI]: BasePeriodId in Adbreak. But Ad not available. BasePeriodId[%s],Adbreak[%s]",__FUNCTION__,__LINE__, mBasePeriodId.c_str(), brkId.c_str());
						mCdaiObject->mAdState = AdState::IN_ADBREAK_AD_NOT_PLAYING;
					}
					stateChanged = true;
				}
			}
			break;
		case AdState::IN_ADBREAK_AD_NOT_PLAYING:
			if(AdEvent::BASE_OFFSET_CHANGE == evt || AdEvent::PERIOD_CHANGE == evt)
			{
				std::string brkId = "";
				int adIdx = mCdaiObject->CheckForAdStart(rate, false, mBasePeriodId, mBasePeriodOffset, brkId, adOffset);
				if(-1 != adIdx && mCdaiObject->mAdBreaks[brkId].ads)
				{
					if(0 == adIdx && 0 != mBasePeriodOffset)
					{
						//Ad is ready; but it is late. Invalidate.
						mCdaiObject->mAdBreaks[brkId].ads->at(0).invalid = true;
					}
					if(!(mCdaiObject->mAdBreaks[brkId].ads->at(adIdx).invalid))
					{
						AAMPLOG_WARN("%s:%d [CDAI]: AdIdx[%d] Found at Period[%s].",__FUNCTION__,__LINE__, adIdx, mBasePeriodId.c_str());
						mCdaiObject->mCurAds = mCdaiObject->mAdBreaks[brkId].ads;

						mCdaiObject->mCurAdIdx = adIdx;
						mCdaiObject->mAdState = AdState::IN_ADBREAK_AD_PLAYING;

						for(int i=0; i<adIdx; i++)
							adPos2Send += mCdaiObject->mCurAds->at(i).duration;
						stateChanged = true;
					}
					if(adIdx == (mCdaiObject->mAdBreaks[brkId].ads->size() -1))	//Rewind case only.
					{
						reservationEvt2Send = AAMP_EVENT_AD_RESERVATION_START;
						adbreakId2Send = brkId;
					}
				}
				else if(brkId.empty())
				{
					AAMPLOG_WARN("%s:%d [CDAI]: ADBREAK[%s] ENDED. Playing the basePeriod[%s].",__FUNCTION__,__LINE__, mCdaiObject->mCurPlayingBreakId.c_str(), mBasePeriodId.c_str());
					mCdaiObject->mCurPlayingBreakId = "";
					mCdaiObject->mCurAds = nullptr;
					mCdaiObject->mCurAdIdx = -1;
					//Base content playing already. No need to jump to offset again.
					mCdaiObject->mAdState = AdState::OUTSIDE_ADBREAK;
					stateChanged = true;
				}
			}
			break;
		case AdState::IN_ADBREAK_AD_PLAYING:
			if(AdEvent::AD_FINISHED == evt)
			{
				AAMPLOG_WARN("%s:%d [CDAI]: Ad finished at Period. Waiting to catchup the base offset.[idx=%d] [period=%s]",__FUNCTION__,__LINE__, mCdaiObject->mCurAdIdx, mBasePeriodId.c_str());
				mCdaiObject->mAdState = AdState::IN_ADBREAK_WAIT2CATCHUP;

				placementEvt2Send = AAMP_EVENT_AD_PLACEMENT_END;
				AdNode &adNode =  mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx);
				adId2Send = adNode.adId;
				for(int i=0; i <= mCdaiObject->mCurAdIdx; i++)
					adPos2Send += mCdaiObject->mCurAds->at(i).duration;
				stateChanged = true;
			}
			else if(AdEvent::AD_FAILED == evt)
			{
				mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx).invalid = true;
				logprintf("%s:%d [CDAI]: Ad Playback failed. Going to the base period[%s] at offset[%lf].Ad[idx=%d]",__FUNCTION__,__LINE__, mBasePeriodId.c_str(), mBasePeriodOffset,mCdaiObject->mCurAdIdx);
				mCdaiObject->mAdState = AdState::IN_ADBREAK_AD_NOT_PLAYING; //TODO: Vinod, It should be IN_ADBREAK_WAIT2CATCHUP, But you need to fix the catchup check logic.

				placementEvt2Send = AAMP_EVENT_AD_PLACEMENT_ERROR;	//Followed by AAMP_EVENT_AD_PLACEMENT_END
				AdNode &adNode =  mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx);
				adId2Send = adNode.adId;
				sendImmediate = true;
				adPos2Send = 0; //TODO: Vinod, Fix it
				stateChanged = true;
			}

			if(stateChanged)
			{
				for (int i = 0; i < mNumberOfTracks; i++)
				{
					//Resetting the manifest Url in track contexts
					mMediaStreamContext[i]->fragmentDescriptor.manifestUrl = aamp->GetManifestUrl();
				}
			}
			break;
		case AdState::IN_ADBREAK_WAIT2CATCHUP:
			if(-1 == mCdaiObject->mCurAdIdx)
			{
				logprintf("%s:%d [CDAI]: BUG! BUG!! BUG!!! We should not come here.AdIdx[-1].",__FUNCTION__,__LINE__);
				mCdaiObject->mCurPlayingBreakId = "";
				mCdaiObject->mCurAds = nullptr;
				mCdaiObject->mCurAdIdx = -1;
				mCdaiObject->mContentSeekOffset = mBasePeriodOffset;
				mCdaiObject->mAdState = AdState::OUTSIDE_ADBREAK;
				stateChanged = true;
				break;
			}
			//In every event, we need to check this.But do it only on the begining of the fetcher loop. Hence it is the default event
			if(AdEvent::DEFAULT == evt)
			{
				if(!(mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx).placed)) //TODO: Vinod, Need to wait till the base period offset is available. 'placed' won't help in case of rewind.
				{
					break;
				}
				//Wait till placement of current ad is completed
				AAMPLOG_WARN("%s:%d [CDAI]: Current Ad placement Completed. Ready to play next Ad.",__FUNCTION__,__LINE__);
				mCdaiObject->mAdState = AdState::IN_ADBREAK_AD_READY2PLAY;
			}
		case AdState::IN_ADBREAK_AD_READY2PLAY:
			if(AdEvent::DEFAULT == evt)
			{
				bool curAdFailed = mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx).invalid;	//TODO: Vinod, may need to check boundary.

				if(rate >= AAMP_NORMAL_PLAY_RATE)
					mCdaiObject->mCurAdIdx++;
				else
					mCdaiObject->mCurAdIdx--;
				if(mCdaiObject->mCurAdIdx >= 0 && mCdaiObject->mCurAdIdx < mCdaiObject->mCurAds->size())
				{
					if(mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx).invalid)
					{
						AAMPLOG_WARN("%s:%d [CDAI]: AdIdx is invalid. Skipping. AdIdx[%d].",__FUNCTION__,__LINE__, mCdaiObject->mCurAdIdx);
						mCdaiObject->mAdState = AdState::IN_ADBREAK_AD_NOT_PLAYING;
					}
					else
					{
						AAMPLOG_WARN("%s:%d [CDAI]: Next AdIdx[%d] Found at Period[%s].",__FUNCTION__,__LINE__, mCdaiObject->mCurAdIdx, mBasePeriodId.c_str());
						mCdaiObject->mAdState = AdState::IN_ADBREAK_AD_PLAYING;

						for(int i=0; i<mCdaiObject->mCurAdIdx; i++)
							adPos2Send += mCdaiObject->mCurAds->at(i).duration;
					}
					stateChanged = true;
				}
				else
				{
					if(rate > 0)
					{
						mBasePeriodId =	mCdaiObject->mAdBreaks[mCdaiObject->mCurPlayingBreakId].endPeriodId;
						mCdaiObject->mContentSeekOffset = (double)(mCdaiObject->mAdBreaks[mCdaiObject->mCurPlayingBreakId].endPeriodOffset)/ 1000;
					}
					else
					{
						// mCdaiObject->mCurPlayingBreakId is the first period in the Adbreak. Set the previous period as mBasePeriodId to play
						std::string prevPId = "";
						size_t numPeriods = mpd->GetPeriods().size();
						for(size_t iPeriod=0;iPeriod < numPeriods;  iPeriod++)
						{
							const std::string &pId = mpd->GetPeriods().at(iPeriod)->GetId();
							if(mCdaiObject->mCurPlayingBreakId == pId)
							{
								break;
							}
							prevPId = pId;
						}
						if(!prevPId.empty())
						{
							mBasePeriodId = prevPId;
						} //else, it should play the mBasePeriodId
						mCdaiObject->mContentSeekOffset = 0; //Should continue tricking from the end of the previous period.
					}
					AAMPLOG_WARN("%s:%d [CDAI]: All Ads in the ADBREAK[%s] FINISHED. Playing the basePeriod[%s] at Offset[%lf].",__FUNCTION__,__LINE__, mCdaiObject->mCurPlayingBreakId.c_str(), mBasePeriodId.c_str(), mCdaiObject->mContentSeekOffset);
					reservationEvt2Send = AAMP_EVENT_AD_RESERVATION_END;
					adbreakId2Send = mCdaiObject->mCurPlayingBreakId;
					sendImmediate = curAdFailed;	//Current Ad failed. Hence may not get discontinuity from gstreamer.
					mCdaiObject->mCurPlayingBreakId = "";
					mCdaiObject->mCurAds = nullptr;
					mCdaiObject->mCurAdIdx = -1;
					mCdaiObject->mAdState = AdState::OUTSIDE_ADBREAK;	//No more offset check needed. Hence, changing to OUTSIDE_ADBREAK
					stateChanged = true;
				}
			}
			break;
		default:
			break;
	}
	if(stateChanged)
	{
		mAdPlayingFromCDN = false;
		bool fogManifestFailed = false;
		if(AdState::IN_ADBREAK_AD_PLAYING == mCdaiObject->mAdState)
		{
			AdNode &adNode = mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx);
			if(NULL == adNode.mpd)
			{
				//Need to ensure that mpd is available, if not available, download it (mostly from FOG)
				bool finalManifest = false;
				adNode.mpd = mCdaiObject->GetAdMPD(adNode.url, finalManifest, false);

				if(NULL == adNode.mpd)
				{
					AAMPLOG_WARN("%s:%d [CDAI]: Ad playback failed. Not able to download Ad manifest from FOG.",__FUNCTION__,__LINE__);
					mCdaiObject->mAdState = AdState::IN_ADBREAK_AD_NOT_PLAYING;
					fogManifestFailed = true;
					if(AdState::IN_ADBREAK_AD_NOT_PLAYING == oldState)
					{
						stateChanged = false;
					}
				}
			}
			if(adNode.mpd)
			{
				mCurrentPeriod = adNode.mpd->GetPeriods().at(0);
				/* TODO: Fix redundancy from UpdateTrackInfo */
				for (int i = 0; i < mNumberOfTracks; i++)
				{
					mMediaStreamContext[i]->fragmentDescriptor.manifestUrl = adNode.url.c_str();
				}

				placementEvt2Send = AAMP_EVENT_AD_PLACEMENT_START;
				adId2Send = adNode.adId;

				map<string, string> mpdAttributes = adNode.mpd->GetRawAttributes();
				if(mpdAttributes.find("fogtsb") == mpdAttributes.end())
				{
					//No attribute 'fogtsb' in MPD. Hence, current ad is from CDN
					mAdPlayingFromCDN = true;
				}
			}
		}

		if(stateChanged)
		{
			AAMPLOG_WARN("%s:%d [CDAI]: State changed from [%s] => [%s].",__FUNCTION__,__LINE__, ADSTATE_STR[static_cast<int>(oldState)],ADSTATE_STR[static_cast<int>(mCdaiObject->mAdState)]);
		}

		if(AAMP_NORMAL_PLAY_RATE == rate)
		{
			//Sending Ad events
			uint64_t resPosMS = 0;
			if(AAMP_EVENT_AD_RESERVATION_START == reservationEvt2Send || AAMP_EVENT_AD_RESERVATION_END == reservationEvt2Send)
			{
				const std::string &startStr = mpd->GetPeriods().at(mCurrentPeriodIdx)->GetStart();
				if(!startStr.empty())
				{
					resPosMS = ParseISO8601Duration(startStr.c_str() );
				}
				resPosMS += (uint64_t)(mBasePeriodOffset * 1000);
			}

			if(AAMP_EVENT_AD_RESERVATION_START == reservationEvt2Send)
			{
				aamp->SendAdReservationEvent(reservationEvt2Send,adbreakId2Send, resPosMS, sendImmediate);
				aamp->SendAnomalyEvent(ANOMALY_TRACE, "[CDAI] Adbreak of duration=%u sec starts.", (mCdaiObject->mAdBreaks[mCdaiObject->mCurPlayingBreakId].brkDuration)/1000);
			}

			if(AAMP_EVENT_AD_PLACEMENT_START == placementEvt2Send || AAMP_EVENT_AD_PLACEMENT_END == placementEvt2Send || AAMP_EVENT_AD_PLACEMENT_ERROR == placementEvt2Send)
			{
				uint32_t adDuration = 30000;
				if(AAMP_EVENT_AD_PLACEMENT_START == placementEvt2Send)
				{
					adDuration = mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx).duration;
					adPos2Send += adOffset;
					aamp->SendAnomalyEvent(ANOMALY_TRACE, "[CDAI] AdId=%s starts. Duration=%u sec URL=%s",
							adId2Send.c_str(),(adDuration/1000), mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx).url.c_str());
				}

				aamp->SendAdPlacementEvent(placementEvt2Send,adId2Send, adPos2Send, adOffset, adDuration, sendImmediate);
				if(fogManifestFailed)
				{
					aamp->SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_ERROR,adId2Send, adPos2Send, adOffset, adDuration, true);
				}
				if(AAMP_EVENT_AD_PLACEMENT_ERROR == placementEvt2Send || fogManifestFailed)
				{
					aamp->SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_END,adId2Send, adPos2Send, adOffset, adDuration, true);	//Ad ended with error
					aamp->SendAnomalyEvent(ANOMALY_ERROR, "[CDAI] AdId=%s encountered error.", adId2Send.c_str());
				}
			}

			if(AAMP_EVENT_AD_RESERVATION_END == reservationEvt2Send)
			{
				aamp->SendAdReservationEvent(reservationEvt2Send,adbreakId2Send, resPosMS, sendImmediate);
				aamp->SendAnomalyEvent(ANOMALY_TRACE, "%s", "[CDAI] Adbreak ends.");
			}
		}
	}
	return stateChanged;
}

/**
 * @brief To set the audio tracks of current period
 *
 * @param[in] tracks - available audio tracks in period
 * @param[in] trackIndex - index of current audio track
 * @return void
 */
void StreamAbstractionAAMP_MPD::SetAudioTrackInfo(const std::vector<AudioTrackInfo> &tracks, const std::string &trackIndex)
{
	bool tracksChanged = false;
	if (!mAudioTracks.empty()
			&& tracks.size() != mAudioTracks.size())
		//TODO: compare pld of tracks too once requirements available
	{
		tracksChanged = true;
	}

	mAudioTracks = tracks;
	mAudioTrackIndex = trackIndex;

	if (tracksChanged)
	{
		aamp->NotifyAudioTracksChanged();
	}
}

/**
 * @brief To set the text tracks of current period
 *
 * @param[in] tracks - available text tracks in period
 * @param[in] trackIndex - index of current text track
 * @return void
 */
void StreamAbstractionAAMP_MPD::SetTextTrackInfo(const std::vector<TextTrackInfo> &tracks, const std::string &trackIndex)
{
	bool tracksChanged = false;
	if (!mTextTracks.empty()
			&& tracks.size() != mTextTracks.size())
		//TODO: compare pld of tracks too once requirements available
	{
		tracksChanged = true;
	}

	mTextTracks = tracks;
	mTextTrackIndex = trackIndex;

#ifdef AAMP_CC_ENABLED
		std::vector<TextTrackInfo> textTracksCopy;
		std::copy_if(begin(mTextTracks), end(mTextTracks), back_inserter(textTracksCopy), [](const TextTrackInfo& e){return e.isCC;});
		AampCCManager::GetInstance()->updateLastTextTracks(textTracksCopy);
#endif

	if (tracksChanged)
	{
		aamp->NotifyTextTracksChanged();
	}
}

/**
 * @brief To check if the adaptation set is having matching language and supported mime type
 *
 * @param[in] type - media type
 * @param[in] lang - language to be matched
 * @param[in] adaptationSet - adaptation to be checked for
 * @param[out] representionIndex - represention within adaptation with matching params
 * @return bool true if the params are matching
 */
bool StreamAbstractionAAMP_MPD::IsMatchingLanguageAndMimeType(MediaType type, std::string lang, IAdaptationSet *adaptationSet, int &representationIndex)
{
	   bool ret = false;
	   std::string adapLang = GetLanguageForAdaptationSet(adaptationSet);
	   AAMPLOG_INFO("%s:%d type %d inlang %s current lang %s", __FUNCTION__, __LINE__, type, lang.c_str(), adapLang.c_str());
	   if (adapLang == lang)
	   {
			   std::string adaptationMimeType = adaptationSet->GetMimeType();
			   if (!adaptationMimeType.empty())
			   {
					   if (IsCompatibleMimeType(adaptationMimeType, type))
					   {
							   ret = true;
							   representationIndex = 0;
					   }
			   }
			   else
			   {
					   const std::vector<IRepresentation *> representation = adaptationSet->GetRepresentation();
					   for (int repIndex = 0; repIndex < representation.size(); repIndex++)
					   {
							   const dash::mpd::IRepresentation *rep = representation.at(repIndex);
							   std::string mimeType = rep->GetMimeType();
							   if (!mimeType.empty() && (IsCompatibleMimeType(mimeType, type)))
							   {
									   ret = true;
									   representationIndex = repIndex;
							   }
					   }
			   }
			   if (ret != true)
			   {
					   //Even though language matched, mimeType is missing or not supported right now. Log for now
					   AAMPLOG_WARN("StreamAbstractionAAMP_MPD::%s %d > Found matching track[%d] with language:%s but not supported mimeType and thus disabled!!\n",
											   __FUNCTION__, __LINE__, type, lang.c_str());
			   }
	   }
	   return ret;
}
