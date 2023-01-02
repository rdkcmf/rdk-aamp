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
#include "MediaStreamContext.h"
#include "AampFnLogger.h"
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
#include <chrono>
//#define DEBUG_TIMELINE

#ifdef AAMP_CC_ENABLED
#include "AampCCManager.h"
#endif



/**
 * @addtogroup AAMP_COMMON_TYPES
 */
#define SEGMENT_COUNT_FOR_ABR_CHECK 5
#define DEFAULT_INTERVAL_BETWEEN_MPD_UPDATES_MS 3000
#define TIMELINE_START_RESET_DIFF 4000000000
#define MAX_DELAY_BETWEEN_MPD_UPDATE_MS (6000)
#define MIN_DELAY_BETWEEN_MPD_UPDATE_MS (500) // 500mSec
#define MIN_TSB_BUFFER_DEPTH 6 //6 seconds from 4.3.3.2.2 in https://dashif.org/docs/DASH-IF-IOP-v4.2-clean.htm
#define VSS_DASH_EARLY_AVAILABLE_PERIOD_PREFIX "vss-"
#define FOG_INSERTED_PERIOD_ID_PREFIX "FogPeriod"
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

// weights used for autio/subtitle track-selection heuristic
#define AAMP_LANGUAGE_SCORE 1000000000L  /**< Top priority:  matching language **/
#define AAMP_SCHEME_ID_SCORE 100000000L  /**< 2nd priority to scheme id matching **/
#define AAMP_LABEL_SCORE 10000000L       /**< 3rd priority to  label matching **/
#define AAMP_ROLE_SCORE 1000000L         /**< 4th priority to role/rendition matching **/
#define AAMP_TYPE_SCORE 100000L          /**< 5th priority to type matching **/
#define AAMP_CODEC_SCORE 1000L           /**< Lowest priority: matchng codec **/

static double ParseISO8601Duration(const char *ptr);

static double ComputeFragmentDuration( uint32_t duration, uint32_t timeScale )
{
        FN_TRACE_F_MPD( __FUNCTION__ );
	double newduration = 2.0;
	if( duration && timeScale )
	{
		newduration =  (double)duration / (double)timeScale;
		return newduration;
	}
	AAMPLOG_WARN( "bad fragment duration");
	return newduration;
}

/**
 * @class PeriodElement
 * @brief Consists Adaptation Set and representation-specific parts
 */
class PeriodElement
{ //  Common (Adaptation Set) and representation-specific parts
private:
	const IRepresentation *pRepresentation; // primary (representation)
	const IAdaptationSet *pAdaptationSet; // secondary (adaptation set)
	
public:
	PeriodElement(const PeriodElement &other) = delete;
	PeriodElement& operator=(const PeriodElement& other) = delete;
	
	PeriodElement(const IAdaptationSet *adaptationSet, const IRepresentation *representation ):
	pAdaptationSet(NULL),pRepresentation(NULL)
	{
		pRepresentation = representation;
		pAdaptationSet = adaptationSet;
	}
	~PeriodElement()
	{
	}
	
	std::string GetMimeType()
	{
				FN_TRACE_F_MPD( __FUNCTION__ );
		std::string mimeType;
		if( pAdaptationSet ) mimeType = pAdaptationSet->GetMimeType();
		if( mimeType.empty() && pRepresentation ) mimeType = pRepresentation->GetMimeType();
		return mimeType;
	}
};//PerioidElement

/**
 * @class SegmentTemplates
 * @brief Handles operation and information on segment template from manifest
 */
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
                FN_TRACE_F_MPD( __FUNCTION__ );
		return segmentTemplate1 || segmentTemplate2;
	}
	
	std::string Getmedia()
	{
                FN_TRACE_F_MPD( __FUNCTION__ );
		std::string media;
		if( segmentTemplate1 ) media = segmentTemplate1->Getmedia();
		if( media.empty() && segmentTemplate2 ) media = segmentTemplate2->Getmedia();
		return media;
	}
	
	const ISegmentTimeline *GetSegmentTimeline()
	{
                FN_TRACE_F_MPD( __FUNCTION__ );
		const ISegmentTimeline *segmentTimeline = NULL;
		if( segmentTemplate1 ) segmentTimeline = segmentTemplate1->GetSegmentTimeline();
		if( !segmentTimeline && segmentTemplate2 ) segmentTimeline = segmentTemplate2->GetSegmentTimeline();
		return segmentTimeline;
	}
	
	uint32_t GetTimescale()
	{
                //FN_TRACE_F_MPD( __FUNCTION__ );
		uint32_t timeScale = 0;
		if( segmentTemplate1 ) timeScale = segmentTemplate1->GetTimescale();
		// if timescale missing in template ,GetTimeScale returns 1
		if((timeScale==1 || timeScale==0) && segmentTemplate2 ) timeScale = segmentTemplate2->GetTimescale();
		return timeScale;
	}

	uint32_t GetDuration()
	{
		//FN_TRACE_F_MPD( __FUNCTION__ );
		uint32_t duration = 0;
		if( segmentTemplate1 ) duration = segmentTemplate1->GetDuration();
		if( duration==0 && segmentTemplate2 ) duration = segmentTemplate2->GetDuration();
		return duration;
	}
	
	long GetStartNumber()
	{
		FN_TRACE_F_MPD( __FUNCTION__ );
		long startNumber = 0;
		if( segmentTemplate1 ) startNumber = segmentTemplate1->GetStartNumber();
		if( startNumber==0 && segmentTemplate2 ) startNumber = segmentTemplate2->GetStartNumber();
		return startNumber;
	}

	uint64_t GetPresentationTimeOffset()
	{
		FN_TRACE_F_MPD( __FUNCTION__ );
		uint64_t presentationOffset = 0;
		if(segmentTemplate1 ) presentationOffset = segmentTemplate1->GetPresentationTimeOffset();
		if( presentationOffset==0 && segmentTemplate2) presentationOffset = segmentTemplate2->GetPresentationTimeOffset();
		return presentationOffset;
	}
	
	std::string Getinitialization()
	{
		FN_TRACE_F_MPD( __FUNCTION__ );
		std::string initialization;
		if( segmentTemplate1 ) initialization = segmentTemplate1->Getinitialization();
		if( initialization.empty() && segmentTemplate2 ) initialization = segmentTemplate2->Getinitialization();
		return initialization;
	}
}; // SegmentTemplates

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

/**
 * @struct EarlyAvailablePeriodInfo
 * @brief Period Information available at early
 */
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
 */
StreamAbstractionAAMP_MPD::StreamAbstractionAAMP_MPD(AampLogManager *logObj, class PrivateInstanceAAMP *aamp,double seek_pos, float rate): StreamAbstractionAAMP(logObj, aamp),
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
	,mMaxTracks(0)
	,mServerUtcTime(0)
	,mDeltaTime(0)
	,mHasServerUtcTime(0)
	,latencyMonitorThreadStarted(false),prevLatencyStatus(LATENCY_STATUS_UNKNOWN),latencyStatus(LATENCY_STATUS_UNKNOWN),latencyMonitorThreadID(0)
	,mStreamLock()
	,mProfileCount(0)
	,mSubtitleParser()
{
        FN_TRACE_F_MPD( __FUNCTION__ );
	this->aamp = aamp;
	memset(&mMediaStreamContext, 0, sizeof(mMediaStreamContext));
	for (int i=0; i<AAMP_TRACK_COUNT; i++)
	{
		mFirstFragPTS[i] = 0.0;
	}
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
        //FN_TRACE_F_MPD( __FUNCTION__ );
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
 * @param nodePtr Pointer to MPD child node, Tage Name , Property Name,
 * 	  SchemeIdUri (if the propery mapped against scheme Id , default value is empty)
 * @retval return the property name if found, if not found return empty string
 */
static bool IsAtmosAudio(const IMPDElement *nodePtr)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	bool isAtmos = false;

	if (!nodePtr){
		AAMPLOG_ERR("API Failed due to Invalid Arguments");
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
							AAMPLOG_INFO("Recieved %s tag property value as %s ",
							SUPPLEMENTAL_PROPERTY_TAG, value.c_str());
							if (value == EC3_EXT_VALUE_AUDIO_ATMOS){
								isAtmos = true;
								break;
							}

						}
					}
					else
					{
						AAMPLOG_WARN("schemeIdUri is not equals to  SCHEME_ID_URI_EC3_EXT_CODEC ");  //CID:84346 - Null Returns
					}
				}
			}
		}
	}

	return isAtmos;
}

/**
 * @brief Get codec value from representation level
 * @param[out] codecValue - string value of codec as per manifest
 * @param[in] rep - representation node for atmos audio check
 * @retval audio type as per aamp code from string value
 */
static AudioType getCodecType(string & codecValue, const IMPDElement *rep)
{
	AudioType audioType = eAUDIO_UNSUPPORTED;
	std::string ac4 = "ac-4";
	if (codecValue == "ec+3")
	{
#ifndef __APPLE__
		audioType = eAUDIO_ATMOS;
#endif
	}
	else if (!codecValue.compare(0, ac4.size(), ac4))
	{
		audioType = eAUDIO_DOLBYAC4;
	}
	else if ((codecValue == "ac-3"))
	{
		audioType = eAUDIO_DOLBYAC3;
	}
	else if ((codecValue == "ec-3"))
	{
		audioType = eAUDIO_DDPLUS;
		/*
		* check whether ATMOS Flag is set as per ETSI TS 103 420
		*/
		if (IsAtmosAudio(rep))
		{
			AAMPLOG_INFO("Setting audio codec as eAUDIO_ATMOS as per ETSI TS 103 420");
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

	return audioType;
}

/**
 * @brief Get representation index from preferred codec list
 * @retval whether track selected or not
 */
bool StreamAbstractionAAMP_MPD::GetPreferredCodecIndex(IAdaptationSet *adaptationSet, int &selectedRepIdx, AudioType &selectedCodecType, 
	uint32_t &selectedRepBandwidth, uint32_t &bestScore, bool disableEC3, bool disableATMOS, bool disableAC4, bool disableAC3, bool& disabled)
{
    FN_TRACE_F_MPD( __FUNCTION__ );
	bool isTrackSelected = false;
	if( aamp->preferredCodecList.size() > 0 )
	{
		selectedRepIdx = -1;
		if(adaptationSet != NULL)
		{
			uint32_t score = 0;
			const std::vector<IRepresentation *> representation = adaptationSet->GetRepresentation();
			/* check for codec defined in Adaptation Set */
			const std::vector<string> adapCodecs = adaptationSet->GetCodecs();
			for (int representationIndex = 0; representationIndex < representation.size(); representationIndex++)
			{
				score = 0;
				const dash::mpd::IRepresentation *rep = representation.at(representationIndex);
				uint32_t bandwidth = rep->GetBandwidth();
				const std::vector<string> codecs = rep->GetCodecs();
				AudioType audioType = eAUDIO_UNKNOWN;
				string codecValue="";

				/* check if Representation includec codec */
				if(codecs.size())
				{
					codecValue=codecs.at(0);
				}
				else if(adapCodecs.size()) /* else check if Adaptation has codec defined */
				{
					codecValue = adapCodecs.at(0);
				}
				auto iter = std::find(aamp->preferredCodecList.begin(), aamp->preferredCodecList.end(), codecValue);
				if(iter != aamp->preferredCodecList.end())
				{  /* track is in preferred codec list */
					int distance = std::distance(aamp->preferredCodecList.begin(),iter);
					score = ((aamp->preferredCodecList.size()-distance))*AAMP_CODEC_SCORE; /* bonus for codec match */
				}
				AudioType codecType = getCodecType(codecValue, rep);
				score += (uint32_t)codecType;
				if (((codecType == eAUDIO_ATMOS) && (disableATMOS || disableEC3)) || /*ATMOS audio desable by config */
					((codecType == eAUDIO_DDPLUS) && disableEC3) || /* EC3 disable neglact it that case */
					((codecType == eAUDIO_DOLBYAC4) && disableAC4) || /** Disable AC4 **/
					((codecType == eAUDIO_DOLBYAC3) && disableAC3) ) /**< Disable AC3 **/
				{
					//Reduce score to 0 since ATMOS and/or DDPLUS is disabled;
					score = 0; 
				}

				if(( score > bestScore ) || /* better matching codec */
				((score == bestScore ) && (bandwidth > selectedRepBandwidth) && isTrackSelected )) /* Same codec as selected track but better quality */
				{
					bestScore = score;
					selectedRepIdx = representationIndex;
					selectedRepBandwidth = bandwidth;
					selectedCodecType = codecType;
					isTrackSelected = true;
				}
			} //representation Loop
			if (score == 0)
			{
				/**< No valid representation found here */
				disabled = true;
			}
		} //If valid adaptation
	} // If preferred Codec Set
	return isTrackSelected;
}

/**
 * @brief Get representation index from preferred codec list
 * @retval whether track selected or not
 */
void StreamAbstractionAAMP_MPD::GetPreferredTextRepresentation(IAdaptationSet *adaptationSet, int &selectedRepIdx, uint32_t &selectedRepBandwidth, unsigned long long &score, std::string &name, std::string &codec)
{
	if(adaptationSet != NULL)
	{
		selectedRepBandwidth = 0;
		selectedRepIdx = 0;
		/* check for codec defined in Adaptation Set */
		const std::vector<string> adapCodecs = adaptationSet->GetCodecs();
		const std::vector<IRepresentation *> representation = adaptationSet->GetRepresentation();
		for (int representationIndex = 0; representationIndex < representation.size(); representationIndex++)
		{
			const dash::mpd::IRepresentation *rep = representation.at(representationIndex);
			uint32_t bandwidth = rep->GetBandwidth();
			if (bandwidth > selectedRepBandwidth) /**< Get Best Rep based on bandwidth **/
			{
				selectedRepIdx  = representationIndex;
				selectedRepBandwidth = bandwidth;
				score += 2; /**< Increase score by 2 if multiple track present*/
			}
			name = rep->GetId();
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
				PeriodElement periodElement(adaptationSet, rep);
				codec = periodElement.GetMimeType();
			}
		}
	}
	AAMPLOG_INFO("StreamAbstractionAAMP_MPD: SelectedRepIndex : %d selectedRepBandwidth: %d", selectedRepIdx, selectedRepBandwidth);
}

static int GetDesiredCodecIndex(IAdaptationSet *adaptationSet, AudioType &selectedCodecType, uint32_t &selectedRepBandwidth,
				bool disableEC3,bool disableATMOS, bool disableAC4,bool disableAC3,  bool &disabled)
{
        FN_TRACE_F_MPD( __FUNCTION__ );
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
#if defined(RPI)
			if((codecValue == "ec+3") || (codecValue == "ec-3"))
				continue;
#endif
			audioType = getCodecType(codecValue, rep);

			/*
			* By default the audio profile selection priority is set as ATMOS then DD+ then AAC
			* Note that this check comes after the check of selected language.
			* disableATMOS: avoid use of ATMOS track
			* disableEC3: avoid use of DDPLUS and ATMOS tracks
			* disableAC4: avoid use if ATMOS AC4 tracks
			*/
			if ((selectedCodecType == eAUDIO_UNKNOWN && (audioType != eAUDIO_UNSUPPORTED || selectedRepBandwidth == 0)) || // Select any profile for the first time, reject unsupported streams then
				(selectedCodecType == audioType && bandwidth>selectedRepBandwidth) || // same type but better quality
				(selectedCodecType < eAUDIO_DOLBYAC4 && audioType == eAUDIO_DOLBYAC4 && !disableAC4 ) || // promote to AC4
				(selectedCodecType < eAUDIO_ATMOS && audioType == eAUDIO_ATMOS && !disableATMOS && !disableEC3) || // promote to atmos
				(selectedCodecType < eAUDIO_DDPLUS && audioType == eAUDIO_DDPLUS && !disableEC3) || // promote to ddplus
				(selectedCodecType != eAUDIO_AAC && audioType == eAUDIO_AAC && disableEC3) || // force AAC
				(selectedCodecType == eAUDIO_UNSUPPORTED) // anything better than nothing
				)
			{
				selectedRepIdx = representationIndex;
				selectedCodecType = audioType;
				selectedRepBandwidth = bandwidth;
				AAMPLOG_INFO("StreamAbstractionAAMP_MPD: SelectedRepIndex : %d ,selectedCodecType : %d, selectedRepBandwidth: %d", selectedRepIdx, selectedCodecType, selectedRepBandwidth);
			}
		}
	}
	else
	{
		AAMPLOG_WARN("adaptationSet  is null");  //CID:85233 - Null Returns
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
	FN_TRACE_F_MPD( __FUNCTION__ );
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
	//FN_TRACE_F_MPD( __FUNCTION__ );
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
static bool IsContentType(const IAdaptationSet *adaptationSet, MediaType mediaType )
{
	//FN_TRACE_F_MPD( __FUNCTION__ );
	const char *name = getMediaTypeName(mediaType);
	if(name != NULL)
	{
		if (adaptationSet->GetContentType() == name)
		{
			return true;
		}
		else if (adaptationSet->GetContentType() == "muxed")
		{
			AAMPLOG_WARN("excluding muxed content");
		}
		else
		{
			PeriodElement periodElement(adaptationSet, NULL);
			if (IsCompatibleMimeType(periodElement.GetMimeType(), mediaType) )
			{
				return true;
			}
			const std::vector<IRepresentation *> &representation = adaptationSet->GetRepresentation();
			for (int i = 0; i < representation.size(); i++)
			{
				const IRepresentation * rep = representation.at(i);
				PeriodElement periodElement(adaptationSet, rep);
				if (IsCompatibleMimeType(periodElement.GetMimeType(), mediaType) )
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
		AAMPLOG_WARN("name is null");  //CID:86093 - Null Returns
	}
	return false;
}

/**
 * @brief read unsigned 32 bit value and update buffer pointer
 * @param[in] pptr buffer
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
	FN_TRACE_F_MPD( __FUNCTION__ );

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
	//FN_TRACE_F_MPD( __FUNCTION__ );
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
				AAMPLOG_WARN("next is not found ");  //CID:81252 - checked return
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
	FN_TRACE_F_MPD( __FUNCTION__ );
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
				AAMPLOG_WARN("Error at  next");  //CID:81346 - checked return
				break;
			}
		}

		if (done) break;
	}

	return rc;
}


/**
 * @brief Generates fragment url from media information
 */
void StreamAbstractionAAMP_MPD::GetFragmentUrl( std::string& fragmentUrl, const FragmentDescriptor *fragmentDescriptor, std::string media)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	std::string constructedUri = fragmentDescriptor->GetMatchingBaseUrl();
	if( media.empty() )
	{
	}
	else if( aamp_IsAbsoluteURL(media) )
	{ // don't pre-pend baseurl if media starts with http:// or https://
		constructedUri.clear();
	}
	else if (!constructedUri.empty())
	{
		if(ISCONFIGSET(eAAMPConfig_DASHIgnoreBaseURLIfSlash))
		{
			if (constructedUri == "/")
			{
				AAMPLOG_WARN("ignoring baseurl /");
				constructedUri.clear();
			}
		}

		// append '/' suffix to BaseURL if not already present
		if( aamp_IsAbsoluteURL(constructedUri) )
		{
			if( constructedUri.back() != '/' )
			{
				constructedUri += '/';
			}
		}
	}
	else
	{
		AAMPLOG_TRACE("BaseURL not available");
	}
	constructedUri += media;
	replace(constructedUri, "Bandwidth", fragmentDescriptor->Bandwidth);
	replace(constructedUri, "RepresentationID", fragmentDescriptor->RepresentationID);
	replace(constructedUri, "Number", fragmentDescriptor->Number);
	replace(constructedUri, "Time", fragmentDescriptor->Time );
	aamp_ResolveURL(fragmentUrl, fragmentDescriptor->manifestUrl, constructedUri.c_str(),ISCONFIGSET(eAAMPConfig_PropogateURIParam));
	//As a part of RDK-35897 to fetch the url of next next fragment and bandwidth corresponding to each fragment fetch
	if(ISCONFIGSET(eAAMPConfig_EnableCMCD))
	{
		std::string CMCDfragmentUrl;
		std::string CMCDUri = constructedUri;
		int num = fragmentDescriptor->Number;
		++num;
		replace(CMCDUri, "Bandwidth", fragmentDescriptor->Bandwidth);
		replace(CMCDUri, "RepresentationID", fragmentDescriptor->RepresentationID);
		replace(CMCDUri, "Number", num);
		replace(CMCDUri, "Time", fragmentDescriptor->Time );
		aamp->mCMCDBandwidth = fragmentDescriptor->Bandwidth;
		aamp_ResolveURL(CMCDfragmentUrl, fragmentDescriptor->manifestUrl, CMCDUri.c_str(),ISCONFIGSET(eAAMPConfig_PropogateURIParam));
		aamp->mCMCDNextObjectRequest = CMCDfragmentUrl;
		AAMPLOG_INFO("Next fragment url %s",CMCDfragmentUrl.c_str());
	}
}

/**
 * @brief Gets a curlInstance index for a given MediaType
 * @param type the stream MediaType
 * @retval AampCurlInstance index to curl_easy_perform session
 */
static AampCurlInstance getCurlInstanceByMediaType(MediaType type)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
	FN_TRACE_F_MPD( __FUNCTION__ );
	AAMPLOG_WARN("indexedTileInfo size=%d",indexedTileInfo.size());
	for(int i=0;i<indexedTileInfo.size();i++)
	{
		if( indexedTileInfo[i].url )
		{
			free( (char *)indexedTileInfo[i].url );
			indexedTileInfo[i].url = NULL;
		}
	}
	indexedTileInfo.clear();
}
/**
 * @brief Fetch and cache a fragment
 *
 * @retval true on fetch success
 */
bool StreamAbstractionAAMP_MPD::FetchFragment(MediaStreamContext *pMediaStreamContext, std::string media, double fragmentDuration, bool isInitializationSegment, unsigned int curlInstance, bool discontinuity, double pto , uint32_t scale)
{ // given url, synchronously download and transmit associated fragment
	FN_TRACE_F_MPD( __FUNCTION__ );
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
		AAMPLOG_INFO("StreamAbstractionAAMP_MPD: rate %f pMediaStreamContext->fragmentTime %f updated position %f",
				rate, pMediaStreamContext->fragmentTime, position);
		int  vodTrickplayFPS;
		GETCONFIGVALUE(eAAMPConfig_VODTrickPlayFPS,vodTrickplayFPS); 
		duration = duration/rate * vodTrickplayFPS;
		//aamp->disContinuity();
	}
//	AAMPLOG_WARN("[%s] mFirstFragPTS %f  position %f -> %f ", pMediaStreamContext->name, mFirstFragPTS[pMediaStreamContext->mediaType], position, mFirstFragPTS[pMediaStreamContext->mediaType]+position);
	position += mFirstFragPTS[pMediaStreamContext->mediaType];

	bool fragmentCached = pMediaStreamContext->CacheFragment(fragmentUrl, curlInstance, position, duration, NULL, isInitializationSegment, discontinuity
		,(mCdaiObject->mAdState == AdState::IN_ADBREAK_AD_PLAYING), pto, scale);
	// Check if we have downloaded the fragment and waiting for init fragment download on
	// bitrate switching before caching it.
	bool fragmentSaved = (NULL != pMediaStreamContext->mDownloadedFragment.ptr);

	if (!fragmentCached)
	{
		if(!fragmentSaved)
		{
		//AAMPLOG_WARN("StreamAbstractionAAMP_MPD: failed. fragmentUrl %s fragmentTime %f", fragmentUrl.c_str(), pMediaStreamContext->fragmentTime);
			if(mCdaiObject->mAdState == AdState::IN_ADBREAK_AD_PLAYING && (isInitializationSegment || pMediaStreamContext->segDLFailCount >= MAX_AD_SEG_DOWNLOAD_FAIL_COUNT))
			{
				AAMPLOG_WARN("StreamAbstractionAAMP_MPD: [CDAI] Ad fragment not available. Playback failed.");
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
	}
	return retval;
}

/**
 * @brief Fetch and push next fragment
 * @retval true if push is done successfully
 */
bool StreamAbstractionAAMP_MPD::PushNextFragment( class MediaStreamContext *pMediaStreamContext, unsigned int curlInstance)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	bool retval=false;
	static bool FCS_content=false;
	static bool FCS_rep=false;
	FailoverContent failovercontent;
	static std::vector<IFCS *>failovercontents;
	bool isLowLatencyMode = aamp->GetLLDashServiceData()->lowLatencyMode;
	SegmentTemplates segmentTemplates(pMediaStreamContext->representation->GetSegmentTemplate(),
					pMediaStreamContext->adaptationSet->GetSegmentTemplate() );
#ifdef DEBUG_TIMELINE
	AAMPLOG_WARN("Type[%d] timeLineIndex %d fragmentRepeatCount %u PeriodDuration:%f mCurrentPeriodIdx:%d mPeriodStartTime %f",pMediaStreamContext->type,
				pMediaStreamContext->timeLineIndex, pMediaStreamContext->fragmentRepeatCount,mPeriodDuration,mCurrentPeriodIdx,mPeriodStartTime );
#endif
//To fill the failover Content Vector for only once on every representation ,if it presents
        static int OldRepresentation = -1;
	pMediaStreamContext->failAdjacentSegment = false;
	if(OldRepresentation != pMediaStreamContext->representationIndex)
	{
		FCS_rep=false;
		failovercontents.clear();
		FCS_content = false;
		ISegmentTemplate *segmentTemplate = pMediaStreamContext->representation->GetSegmentTemplate();
		if (segmentTemplate)
		{
			const IFailoverContent *failoverContent = segmentTemplate->GetFailoverContent();
			if(failoverContent)
			{
				failovercontents = failoverContent->GetFCS();
				bool valid = failoverContent->IsValid();
				//to indicate this representation contain failover tag or not
				FCS_rep = valid ? false : true;
			}
		}
	}
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
				AAMPLOG_WARN("Type[%d] timelineCnt=%d timeLineIndex:%d FDTime=%f L=%" PRIu64 " [fragmentTime = %f,  mLiveEndPosition = %f]",
					pMediaStreamContext->type ,timelines.size(),pMediaStreamContext->timeLineIndex,pMediaStreamContext->fragmentDescriptor.Time,pMediaStreamContext->lastSegmentTime
					, pMediaStreamContext->fragmentTime, mLiveEndPosition);
#endif
				if ((pMediaStreamContext->timeLineIndex >= timelines.size()) || (pMediaStreamContext->timeLineIndex < 0)
						||(AdState::IN_ADBREAK_AD_PLAYING == mCdaiObject->mAdState &&
							((rate > AAMP_NORMAL_PLAY_RATE && pMediaStreamContext->fragmentTime >= mLiveEndPosition)
							 ||(rate < 0 && pMediaStreamContext->fragmentTime <= 0))))
				{
					AAMPLOG_INFO("Type[%d] EOS. timeLineIndex[%d] size [%lu]",pMediaStreamContext->type, pMediaStreamContext->timeLineIndex, timelines.size());
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

					pMediaStreamContext->timeStampOffset = 0;

					if(!startTimeStr.empty())
					{
						periodStart = (uint64_t)(ParseISO8601Duration(startTimeStr.c_str()) / 1000);
						int64_t timeStampOffset = (int64_t)(periodStart - (uint64_t)(presentationTimeOffset/tScale));

						if (timeStampOffset > 0)
						{
							pMediaStreamContext->timeStampOffset = timeStampOffset;
						}
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
								AAMPLOG_INFO("Type[%d] Setting start time with PTSOffset:%" PRIu64 "",pMediaStreamContext->type,presentationTimeOffset);
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
									AAMPLOG_INFO("Type[%d] skipping fragments[%d] to Index:%d FNum=%d Repeat:%d", pMediaStreamContext->type,offsetNumber,index,pMediaStreamContext->fragmentDescriptor.Number,pMediaStreamContext->fragmentRepeatCount);
								}
							}
						}
						// Modify the descriptor time to start download
						pMediaStreamContext->fragmentDescriptor.Time = startTime;
#ifdef DEBUG_TIMELINE
					AAMPLOG_WARN("Type[%d] timelineCnt=%d timeLineIndex:%d FDTime=%f L=%" PRIu64 " [fragmentTime = %f,  mLiveEndPosition = %f]",
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
							if((0 == pMediaStreamContext->fragmentDescriptor.Time) || rate > AAMP_NORMAL_PLAY_RATE)
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
									AAMPLOG_WARN("Type[%d] Boundary Condition !!! Index(%d) reached Max.Start=%" PRIu64 " Last=%" PRIu64 " ",
										pMediaStreamContext->type,index,startTime,pMediaStreamContext->lastSegmentTime);
									index--;
									startTime = pMediaStreamContext->lastSegmentTime;
									pMediaStreamContext->fragmentRepeatCount = repeatCount+1;
								}

#ifdef DEBUG_TIMELINE
								AAMPLOG_WARN("Type[%d] t=%" PRIu64 " L=%" PRIu64 " d=%d r=%d Index=%d Num=%" PRIu64 " FTime=%f", pMediaStreamContext->type,
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
								AAMPLOG_WARN("Type[%d] t=%" PRIu64 " L=%" PRIu64 " d=%d r=%d fragRep=%d Index=%d Num=%" PRIu64 " FTime=%f", pMediaStreamContext->type,
								startTime,pMediaStreamContext->lastSegmentTime, duration, repeatCount,pMediaStreamContext->fragmentRepeatCount,pMediaStreamContext->timeLineIndex,
								pMediaStreamContext->fragmentDescriptor.Number,pMediaStreamContext->fragmentTime);
#endif
							}
						}// if starttime
						if(0 == pMediaStreamContext->timeLineIndex)
						{
							AAMPLOG_INFO("Type[%d] update startTime to %" PRIu64 ,pMediaStreamContext->type, startTime);
						}
						pMediaStreamContext->fragmentDescriptor.Time = startTime;
#ifdef DEBUG_TIMELINE
						AAMPLOG_WARN("Type[%d] Setting startTime to %" PRIu64 ,pMediaStreamContext->type, startTime);
#endif
					}// if fragRepeat == 0

					ITimeline *timeline = timelines.at(pMediaStreamContext->timeLineIndex);
					uint32_t repeatCount = timeline->GetRepeatCount();
					uint32_t duration = timeline->GetDuration();
#ifdef DEBUG_TIMELINE
					AAMPLOG_WARN("Type[%d] FDt=%f L=%" PRIu64 " d=%d r=%d fragrep=%d x=%d num=%lld",
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
						uint64_t ret = pMediaStreamContext->lastSegmentDuration;
						if(((firstTimeline->GetRawAttributes().find("t")) != (firstTimeline->GetRawAttributes().end())) && (ret > 0))
						{
							// 't' in first timeline is expected.
							positionInPeriod = (pMediaStreamContext->lastSegmentDuration - firstTimeline->GetStartTime()) / timeScale;
						}
#ifdef DEBUG_TIMELINE
						AAMPLOG_WARN("Type[%d] presenting FDt%f Number(%lld) Last=%" PRIu64 " Duration(%d) FTime(%f) endTime:%f",
							pMediaStreamContext->type,pMediaStreamContext->fragmentDescriptor.Time,pMediaStreamContext->fragmentDescriptor.Number,pMediaStreamContext->lastSegmentTime,duration,pMediaStreamContext->fragmentTime,endTime);
#endif
						retval = true;
						if(FCS_rep)
                                                {
							FCS_content = false;
							for(int i =0;i< failovercontents.size();i++)
                                                        {
								uint64_t starttime = failovercontents.at(i)->GetStartTime();
								uint64_t duration  =  failovercontents.at(i)->GetDuration();
								// Logic  to handle the duration option missing case
								if(!duration)
								{
									//If not present, the alternative content section lasts until the start of the next FCS element
									if(i+1 < failovercontents.size())
									{
										duration =  failovercontents.at(i+1)->GetStartTime();
									}
									// until the end of the Period
									else if(mPeriodEndTime)
									{
										duration = mPeriodEndTime;
									}
									// or until the end of MPD duration
                                                                        else
									{
										std::string durationStr =  mpd->GetMediaPresentationDuration();
										if(!durationStr.empty())
										{
											duration = ParseISO8601Duration( durationStr.c_str());
										}
									}
								}
								// the value of this attribute minus the value of the @presentationTimeOffset specifies the MPD start time,
								uint64_t fcscontent_range = (starttime  + duration);
								if((starttime <= pMediaStreamContext->fragmentDescriptor.Time)&&(fcscontent_range > pMediaStreamContext->fragmentDescriptor.Time))
								{
									FCS_content = true;
								}
							}
						}

						int finalPeriodIndex = (mpd->GetPeriods().size() - 1);
						if((mIsFogTSB || (mPeriodDuration !=0 && (mPeriodStartTime + positionInPeriod) < endTime))&& !FCS_content)
						{
							retval = FetchFragment( pMediaStreamContext, media, fragmentDuration, false, curlInstance);
						}
						else
						{
							AAMPLOG_WARN("Type[%d] Skipping Fetchfragment, Number(%lld) fragment beyond duration. fragmentPosition: %lf periodEndTime : %lf", pMediaStreamContext->type, pMediaStreamContext->fragmentDescriptor.Number, positionInPeriod , endTime);
						}
						if(FCS_content)
						{
							long http_code = 404;
							retval = false;
							if (pMediaStreamContext->mediaType == eMEDIATYPE_VIDEO)
							{
								// Attempt rampdown
								if (CheckForRampDownProfile(http_code))
								{
									AAMPLOG_WARN("RampDownProfile Due to failover Content %lf Number %lf FDT",pMediaStreamContext->fragmentDescriptor.Number,pMediaStreamContext->fragmentDescriptor.Time);
									mCheckForRampdown = true;
									// Rampdown attempt success, download same segment from lower profile.
									pMediaStreamContext->mSkipSegmentOnError = false;
									return retval;
								}
								else
								{
									AAMPLOG_WARN("Already at the lowest profile, skipping segment due to failover");
									mRampDownCount = 0;
								}
							}
						}

						if(retval)
						{
							pMediaStreamContext->lastSegmentTime = pMediaStreamContext->fragmentDescriptor.Time;
							pMediaStreamContext->lastSegmentDuration = pMediaStreamContext->fragmentDescriptor.Time + duration;
							// pMediaStreamContext->downloadedDuration is introduced to calculate the bufferedduration value.
							// Update position in period after fragment download
							positionInPeriod += fragmentDuration;
							if(aamp->IsUninterruptedTSB() || (mIsLiveStream && !mIsLiveManifest && !ISCONFIGSET(eAAMPConfig_UseAbsoluteTimeline)))
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
							FetchAndInjectInitFragments();
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
						else if(mIsFogTSB && ISCONFIGSET(eAAMPConfig_InterruptHandling))
						{
							// Mark fragment fetched and save lasr segment time to avoid reattempt.
							pMediaStreamContext->lastSegmentTime = pMediaStreamContext->fragmentDescriptor.Time;
							pMediaStreamContext->lastSegmentDuration = pMediaStreamContext->fragmentDescriptor.Time + duration;
						}
					}
					else if (rate < 0)
					{
#ifdef DEBUG_TIMELINE
						AAMPLOG_WARN("Type[%d] presenting %f" ,pMediaStreamContext->type,pMediaStreamContext->fragmentDescriptor.Time);
#endif
						pMediaStreamContext->lastSegmentTime = pMediaStreamContext->fragmentDescriptor.Time;
						pMediaStreamContext->lastSegmentDuration = pMediaStreamContext->fragmentDescriptor.Time + duration;
						double fragmentDuration = ComputeFragmentDuration(duration,timeScale);
						retval = FetchFragment( pMediaStreamContext, media, fragmentDuration, false, curlInstance);
						if (!retval && ((mIsFogTSB && !mAdPlayingFromCDN) && pMediaStreamContext->mDownloadedFragment.ptr))
						{
							pMediaStreamContext->profileChanged = true;
							profileIdxForBandwidthNotification = GetProfileIdxForBandwidthNotification(pMediaStreamContext->fragmentDescriptor.Bandwidth);
							FetchAndInjectInitFragments();
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
						AAMPLOG_WARN("Calling ScheduleRetune to handle start-time reset lastSegmentTime=%" PRIu64 " start-time=%f" , pMediaStreamContext->lastSegmentTime, pMediaStreamContext->fragmentDescriptor.Time);
						aamp->ScheduleRetune(eDASH_ERROR_STARTTIME_RESET, pMediaStreamContext->mediaType);
					}
					else
					{
#ifdef DEBUG_TIMELINE
						AAMPLOG_WARN("Type[%d] Before skipping. fragmentDescriptor.Time %f lastSegmentTime %" PRIu64 " Index=%d fragRep=%d,repMax=%d Number=%lld",pMediaStreamContext->type,
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
						AAMPLOG_WARN("Type[%d] After skipping. fragmentDescriptor.Time %f lastSegmentTime %" PRIu64 " Index=%d Number=%lld",pMediaStreamContext->type,
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
							AAMPLOG_WARN("Type[%d] After Incr. fragmentDescriptor.Time %f lastSegmentTime %" PRIu64 " Index=%d fragRep=%d,repMax=%d Number=%lld",pMediaStreamContext->type,
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
				 //< FailoverContent> can span more than one adjacent segment on a profile - in this case, client shall remain ramped down for the duration of the marked fragments (without re-injecting extra init header in-between)
				if((OldRepresentation != pMediaStreamContext->representationIndex) && ( pMediaStreamContext->mediaType == eMEDIATYPE_VIDEO))
				{
					std::vector<IFCS *>failovercontents;
					if(OldRepresentation != -1)
					{
						if(pMediaStreamContext->adaptationSet!=NULL){
							const std::vector<IRepresentation *> representation = pMediaStreamContext->adaptationSet ->GetRepresentation();
							if(OldRepresentation < (representation.size()-1)){
								const dash::mpd::IRepresentation *rep = representation.at(OldRepresentation);
								if(rep){
									ISegmentTemplate *segmentTemplate = rep->GetSegmentTemplate();
									if (segmentTemplate)
									{
										const IFailoverContent *failoverContent = segmentTemplate->GetFailoverContent();
										if(failoverContent)
										{
											failovercontents = failoverContent->GetFCS();
											bool valid = failoverContent->IsValid();
											for(int i =0;i< failovercontents.size() && !valid;i++)
											{
												uint64_t starttime = failovercontents.at(i)->GetStartTime();
												uint64_t duration  =  failovercontents.at(i)->GetDuration();
												uint64_t fcscontent_range = starttime + duration ;
												if((starttime <= pMediaStreamContext->fragmentDescriptor.Time)&&(fcscontent_range > pMediaStreamContext->fragmentDescriptor.Time))
													pMediaStreamContext->failAdjacentSegment = true;
											}
										}
									}
								}}
						}}
					if(!pMediaStreamContext->failAdjacentSegment)
					{
						OldRepresentation = pMediaStreamContext->representationIndex;
					}
				}
			}
			else
			{
				AAMPLOG_WARN("timelines is null");  //CID:81702 ,82851 - Null Returns
			}
		}
		else
		{
#ifdef DEBUG_TIMELINE
			AAMPLOG_WARN("segmentTimeline not available");
#endif

			double currentTimeSeconds = (double)aamp_GetCurrentTimeMS() / 1000;

			uint32_t duration = segmentTemplates.GetDuration();
			double fragmentDuration =  ComputeFragmentDuration(duration,timeScale);
			long startNumber = segmentTemplates.GetStartNumber();
			uint32_t scale = segmentTemplates.GetTimescale();
			double pto =  (double) segmentTemplates.GetPresentationTimeOffset();
			AAMPLOG_TRACE("Type[%d] currentTimeSeconds:%f duration:%d fragmentDuration:%f startNumber:%ld", pMediaStreamContext->type, currentTimeSeconds,duration,fragmentDuration,startNumber);
			if (0 == pMediaStreamContext->lastSegmentNumber)
			{
				if (mIsLiveStream)
				{
					if(mHasServerUtcTime)
					{
						currentTimeSeconds+=mDeltaTime;
					}
					double liveTime = currentTimeSeconds - aamp->mLiveOffset;
					if(liveTime < mPeriodStartTime)
					{
						// Not to go beyond the period , as that is checked in skipfragments
						liveTime = mPeriodStartTime;
					}
						
					pMediaStreamContext->lastSegmentNumber = (long long)((liveTime - mPeriodStartTime) / fragmentDuration) + startNumber;
					pMediaStreamContext->fragmentDescriptor.Time = liveTime;
					AAMPLOG_INFO("Type[%d] Printing fragmentDescriptor.Number %" PRIu64 " Time=%f  ", pMediaStreamContext->type, pMediaStreamContext->lastSegmentNumber, pMediaStreamContext->fragmentDescriptor.Time);
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
					if(!aamp->IsLive()) pMediaStreamContext->lastSegmentNumber =  pMediaStreamContext->fragmentDescriptor.Number;
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
			double fragmentRequestTime = 0.0f;
			double availabilityTimeOffset = 0.0f;
			if(isLowLatencyMode)
			{
				// Low Latency Mode will be pointing to edge of the fragment based on avilablitystarttimeoffset,
				// and fragmentDescriptor time itself will be pointing to edge time when live offset is 0. 
				// So adding the duration will cause the latency of fragment duartion and sometime repeat the same content.
				availabilityTimeOffset =  aamp->GetLLDashServiceData()->availabilityTimeOffset;
				fragmentRequestTime = pMediaStreamContext->fragmentDescriptor.Time+(fragmentDuration-availabilityTimeOffset);
			}
			else
			{
				fragmentRequestTime = pMediaStreamContext->fragmentDescriptor.Time + fragmentDuration;
			}

			AAMPLOG_TRACE("fDesc.Time= %lf utcTime=%lf delta=%lf CTSeconds=%lf,FreqTime=%lf",pMediaStreamContext->fragmentDescriptor.Time,
					mServerUtcTime,mDeltaTime,currentTimeSeconds,fragmentRequestTime);

			bool bProcessFrgment = true;
			if(!mIsLiveStream)
			{
				if(ISCONFIGSET(eAAMPConfig_EnableIgnoreEosSmallFragment))
				{
					double fractionDuration = 1.0;
					if(fragmentRequestTime >= mPeriodEndTime)
					{
						double fractionDuration = (mPeriodEndTime-pMediaStreamContext->fragmentDescriptor.Time)/fragmentDuration;
						bProcessFrgment = (fractionDuration < MIN_SEG_DURTION_THREASHOLD)? false:true;
						AAMPLOG_TRACE("Type[%d] DIFF=%f Process Fragment=%d", pMediaStreamContext->type, fractionDuration, bProcessFrgment);
					}
				}
				else
				{
					bProcessFrgment = (pMediaStreamContext->fragmentDescriptor.Time >= mPeriodEndTime)? false: true;
				}
			}
			if ((!mIsLiveStream && ((!bProcessFrgment) || (rate < 0 )))
			|| (mIsLiveStream && (
				(isLowLatencyMode? pMediaStreamContext->fragmentDescriptor.Time>mPeriodEndTime+availabilityTimeOffset:pMediaStreamContext->fragmentDescriptor.Time >= mPeriodEndTime)
			|| (pMediaStreamContext->fragmentDescriptor.Time < mPeriodStartTime))))  //CID:93022 - No effect
			{
				AAMPLOG_INFO("Type[%d] EOS. pMediaStreamContext->lastSegmentNumber %" PRIu64 " fragmentDescriptor.Time=%f mPeriodEndTime=%f mPeriodStartTime %f  currentTimeSeconds %f FTime=%f", pMediaStreamContext->type, pMediaStreamContext->lastSegmentNumber, pMediaStreamContext->fragmentDescriptor.Time, mPeriodEndTime, mPeriodStartTime, currentTimeSeconds, pMediaStreamContext->fragmentTime);
				pMediaStreamContext->lastSegmentNumber =0; // looks like change in period may happen now. hence reset lastSegmentNumber
				pMediaStreamContext->eos = true;
			}
			else if( mIsLiveStream &&  mHasServerUtcTime &&  
					( isLowLatencyMode? fragmentRequestTime >= mServerUtcTime+mDeltaTime : fragmentRequestTime >= mServerUtcTime)) 
			{
				int sleepTime = mMinUpdateDurationMs;
				sleepTime = (sleepTime > MAX_DELAY_BETWEEN_MPD_UPDATE_MS) ? MAX_DELAY_BETWEEN_MPD_UPDATE_MS : sleepTime;
				sleepTime = (sleepTime < 200) ? 200 : sleepTime;

				AAMPLOG_TRACE("With ServerUTCTime. Next fragment Not Available yet: fragmentDescriptor.Time %f fragmentDuration:%f currentTimeSeconds %f Server  UTCTime %f sleepTime %d ", pMediaStreamContext->fragmentDescriptor.Time, fragmentDuration, currentTimeSeconds, mServerUtcTime, sleepTime);	
				aamp->InterruptableMsSleep(sleepTime);
				retval = false;
            }
			else if(mIsLiveStream && !mHasServerUtcTime && 
					(isLowLatencyMode?(fragmentRequestTime>=currentTimeSeconds):(fragmentRequestTime >= (currentTimeSeconds-mPresentationOffsetDelay))))
			{
				int sleepTime = mMinUpdateDurationMs;
				sleepTime = (sleepTime > MAX_DELAY_BETWEEN_MPD_UPDATE_MS) ? MAX_DELAY_BETWEEN_MPD_UPDATE_MS : sleepTime;
				sleepTime = (sleepTime < 200) ? 200 : sleepTime;

				AAMPLOG_TRACE("Without ServerUTCTime. Next fragment Not Available yet: fragmentDescriptor.Time %f fragmentDuration:%f currentTimeSeconds %f Server  UTCTime %f sleepTime %d ", pMediaStreamContext->fragmentDescriptor.Time, fragmentDuration, currentTimeSeconds, mServerUtcTime, sleepTime);	
				aamp->InterruptableMsSleep(sleepTime);
				retval = false;
			}
			else
			{
				if (mIsLiveStream)
				{
					pMediaStreamContext->fragmentDescriptor.Number = pMediaStreamContext->lastSegmentNumber;
				}
				retval = FetchFragment(pMediaStreamContext, media, fragmentDuration, false, curlInstance, false, pto, scale);
				double positionInPeriod = 0;
				if(pMediaStreamContext->lastSegmentNumber > startNumber)
				{
					positionInPeriod = (pMediaStreamContext->lastSegmentNumber - startNumber) * fragmentDuration;
				}

				string startTimeStringValue = mpd->GetPeriods().at(mCurrentPeriodIdx)->GetStart();
				double periodstartValue = 0;
				if(mIsLiveStream && ISCONFIGSET(eAAMPConfig_UseAbsoluteTimeline))
				{
					// DELIA-52320: Non-fog Linear with absolute position reporting
					if(!startTimeStringValue.empty())
					{
						periodstartValue = (ParseISO8601Duration(startTimeStringValue.c_str()))/1000;
						pMediaStreamContext->downloadedDuration = periodstartValue + positionInPeriod;
					}
					else
					{
						pMediaStreamContext->downloadedDuration = (GetPeriodStartTime(mpd, mCurrentPeriodIdx) - mAvailabilityStartTime) + positionInPeriod;
					}
				}
				else
				{
					// For VOD and non-fog linear without Absolute timeline
					// calculate relative position in manifest
					// For VOD, culledSeconds will be 0, and for linear it is added to first period start
					pMediaStreamContext->downloadedDuration = aamp->culledSeconds + GetPeriodStartTime(mpd, mCurrentPeriodIdx) - GetPeriodStartTime(mpd, 0) + positionInPeriod;
				}
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
				AAMPLOG_TRACE("Type[%d] Printing fragmentDescriptor.Number %" PRIu64 " Time=%f  ", pMediaStreamContext->type, pMediaStreamContext->lastSegmentNumber, pMediaStreamContext->fragmentDescriptor.Time);
			}
			//if pipeline is currently clear and if encrypted period is found in the last updated mpd, then do an internal retune
			//to configure the pipeline the for encrypted content
			if(mIsLiveStream && aamp->mEncryptedPeriodFound && aamp->mPipelineIsClear)
			{
				AAMPLOG_WARN("Retuning as encrypted pipeline found when pipeline is configured as clear");
				aamp->ScheduleRetune(eDASH_RECONFIGURE_FOR_ENC_PERIOD,(MediaType) pMediaStreamContext->type);
				AAMPLOG_INFO("Retune (due to enc pipeline) done");
				aamp->mEncryptedPeriodFound = false;
				aamp->mPipelineIsClear = false;
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
					AAMPLOG_INFO("current fragmentIndex = %d", pMediaStreamContext->fragmentIndex);
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
					AAMPLOG_INFO("%s [%s]",getMediaTypeName(pMediaStreamContext->mediaType), range);
					if(pMediaStreamContext->CacheFragment(fragmentUrl, curlInstance, pMediaStreamContext->fragmentTime, fragmentDuration, range ))
					{
						pMediaStreamContext->fragmentTime += fragmentDuration;
						pMediaStreamContext->fragmentOffset += referenced_size;
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
								AAMPLOG_TRACE("StreamAbstractionAAMP_MPD: did not cache fragmentUrl %s fragmentTime %f", fragmentUrl.c_str(), pMediaStreamContext->fragmentTime);
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
										FetchAndInjectInitFragments();
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
								AAMPLOG_WARN("START-TIME RESET in TSB period, lastSegmentTime=%" PRIu64 " start-time=%lld duration=%lld", pMediaStreamContext->lastSegmentTime, startTime, duration);
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
								AAMPLOG_TRACE("PushNextFragment Exit : startTime %lld lastSegmentTime %lu index = %d", startTime, pMediaStreamContext->lastSegmentTime, pMediaStreamContext->fragmentIndex);
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
						AAMPLOG_WARN("segmentURL    is null");  //CID:82493 ,86180 - Null Returns
					}
				}
				else
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_MPD: SegmentUrl is empty");
				}
			}
			else
			{
				AAMPLOG_ERR(" not-yet-supported mpd format");
			}
		}
	}
	return retval;
}


/**
 * @brief Seek current period by a given time
 */
void StreamAbstractionAAMP_MPD::SeekInPeriod( double seekPositionSeconds, bool skipToEnd)
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
			SkipFragments(mMediaStreamContext[i], seekPositionSeconds, true, skipToEnd);
		}
	}
}

/**
 * @brief Find the fragment based on the system clock after the SAP.
 */
void StreamAbstractionAAMP_MPD::ApplyLiveOffsetWorkaroundForSAP( double seekPositionSeconds)
{
	for(int i = 0; i < mNumberOfTracks; i++)
	{
		SegmentTemplates segmentTemplates(mMediaStreamContext[i]->representation->GetSegmentTemplate(),
		mMediaStreamContext[i]->adaptationSet->GetSegmentTemplate() );
		if( segmentTemplates.HasSegmentTemplate() )
		{
			const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
			if(segmentTimeline)
			{
				SeekInPeriod(seekPositionSeconds);
				break;
			}
			else
			{
				double currentplaybacktime = aamp->mOffsetFromTunetimeForSAPWorkaround;
				long startNumber = segmentTemplates.GetStartNumber();
				uint32_t duration = segmentTemplates.GetDuration();
				uint32_t timeScale = segmentTemplates.GetTimescale();
				double fragmentDuration =  ComputeFragmentDuration(duration,timeScale);
				if(currentplaybacktime < mPeriodStartTime)
				{
					currentplaybacktime = mPeriodStartTime;
				}
				mMediaStreamContext[i]->fragmentDescriptor.Number = (long long)((currentplaybacktime - mPeriodStartTime) / fragmentDuration) + startNumber - 1;
				mMediaStreamContext[i]->fragmentDescriptor.Time = currentplaybacktime - fragmentDuration;
				mMediaStreamContext[i]->fragmentTime = seekPositionSeconds/fragmentDuration - fragmentDuration;
				mMediaStreamContext[i]->lastSegmentNumber= mMediaStreamContext[i]->fragmentDescriptor.Number;
				AAMPLOG_INFO("moffsetFromStart:%f startNumber:%ld mPeriodStartTime:%f fragmentDescriptor.Number:%lld >fragmentDescriptor.Time:%f  mLiveOffset:%f seekPositionSeconds:%f"
				,aamp->mOffsetFromTunetimeForSAPWorkaround,startNumber,mPeriodStartTime, mMediaStreamContext[i]->fragmentDescriptor.Number,mMediaStreamContext[i]->fragmentDescriptor.Time,aamp->mLiveOffset,seekPositionSeconds);
			}
		}
		else
		{
			SeekInPeriod(seekPositionSeconds);
			break;
		}
	}
}

/**
 * @brief Skip to end of track
 */
void StreamAbstractionAAMP_MPD::SkipToEnd( MediaStreamContext *pMediaStreamContext)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
				AAMPLOG_WARN("timelines is null");  //CID:82016,84031 - Null Returns
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
			AAMPLOG_ERR("not-yet-supported mpd format");
		}
	}
}


/**
 * @brief Skip fragments by given time
 */
double StreamAbstractionAAMP_MPD::SkipFragments( MediaStreamContext *pMediaStreamContext, double skipTime, bool updateFirstPTS, bool skipToEnd)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	if( !pMediaStreamContext->representation )
	{ // avoid crash with video-only content
		return 0.0;
	}
	SegmentTemplates segmentTemplates(pMediaStreamContext->representation->GetSegmentTemplate(),
					pMediaStreamContext->adaptationSet->GetSegmentTemplate() );
	if( segmentTemplates.HasSegmentTemplate() )
	{
		 AAMPLOG_INFO("Enter : Type[%d] timeLineIndex %d fragmentRepeatCount %d fragmentTime %f skipTime %f segNumber %lu",pMediaStreamContext->type,
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
					AAMPLOG_INFO("Type[%d] EOS. timeLineIndex[%d] size [%lu]",pMediaStreamContext->type, pMediaStreamContext->timeLineIndex, timelines.size());
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
//					AAMPLOG_TRACE("[%s] firstPTS %f nextPTS %f duration %f skipTime %f", pMediaStreamContext->name, firstPTS, nextPTS, fragmentDuration, skipTime);
					if (firstFrag && updateFirstPTS)
					{
						if (skipToEnd)
						{
							AAMPLOG_INFO("Processing skipToEnd for track type %d", pMediaStreamContext->type);
						}
						else
						{
							//When player started with rewind mode during trickplay, make sure that the skip time calcualtion would not spill over the duration of the content
							if(aamp->playerStartedWithTrickPlay && aamp->rate < 0 && skipTime > fragmentDuration )
							{
								AAMPLOG_INFO("Player switched in rewind mode, adjusted skptime from %f to %f ", skipTime, skipTime - fragmentDuration);
								skipTime -= fragmentDuration;
							}
							if (pMediaStreamContext->type == eTRACK_AUDIO && (mFirstFragPTS[eTRACK_VIDEO] || mFirstPTS || mVideoPosRemainder)){
								
								/* need to adjust audio skipTime/seekPosition so 1st audio fragment sent matches start of 1st video fragment being sent */
								double newSkipTime = skipTime + (mFirstFragPTS[eTRACK_VIDEO] - firstPTS); /* adjust for audio/video frag start PTS differences */
								newSkipTime -= mVideoPosRemainder;   /* adjust for mVideoPosRemainder, which is (video seekposition/skipTime - mFirstPTS) */
								newSkipTime += fragmentDuration/4.0; /* adjust for case where video start is near end of current audio fragment by adding to the audio skipTime, pushing it into the next fragment, if close(within this adjustment) */
								//							AAMPLOG_INFO("newSkiptime %f, skipTime %f  mFirstFragPTS[vid] %f  firstPTS %f  mFirstFragPTS[vid]-firstPTS %f mVideoPosRemainder %f  fragmentDuration/4.0 %f", newSkipTime, skipTime, mFirstFragPTS[eTRACK_VIDEO], firstPTS, mFirstFragPTS[eTRACK_VIDEO]-firstPTS, mVideoPosRemainder,  fragmentDuration/4.0);
								skipTime = newSkipTime;
							}
						}
						firstFrag = false;
						mFirstFragPTS[pMediaStreamContext->mediaType] = firstPTS;
					}

					if (skipToEnd)
					{
						if ((pMediaStreamContext->fragmentRepeatCount == repeatCount) &&
							(pMediaStreamContext->timeLineIndex + 1 == timelines.size()))
						{
							skipTime = 0; // We have reached the last fragment
						}
						else
						{
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
					}
					else if (skipTime >= fragmentDuration)
					{
						skipTime -= fragmentDuration;
						pMediaStreamContext->fragmentTime += fragmentDuration;
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
						AAMPLOG_INFO("[%s] firstPTS %f, nextPTS %f  skipTime %f  fragmentDuration %f ", pMediaStreamContext->name, firstPTS, nextPTS, skipTime, fragmentDuration);
						if (updateFirstPTS)
						{
							/*Keep the lower PTS */
							if ( ((mFirstPTS == 0) || (firstPTS < mFirstPTS)) && (pMediaStreamContext->type == eTRACK_VIDEO))
							{
								AAMPLOG_INFO("[%s] mFirstPTS %f -> %f ", pMediaStreamContext->name, mFirstPTS, firstPTS);
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
								AAMPLOG_INFO("[%s] mFirstPTS %f  mVideoPosRemainder %f", pMediaStreamContext->name, mFirstPTS, mVideoPosRemainder);
							}
						}
						skipTime = 0;
						if (pMediaStreamContext->type == eTRACK_AUDIO){
//							AAMPLOG_TRACE("audio/video PTS offset %f  audio %f video %f", firstPTS-mFirstPTS, firstPTS, mFirstPTS);
							if (abs(firstPTS - mFirstPTS)> 1.00){
								AAMPLOG_WARN("audio/video PTS offset Large %f  audio %f video %f",  firstPTS-mFirstPTS, firstPTS, mFirstPTS);
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
					AAMPLOG_INFO("Type[%d] EOS. fragmentDescriptor.Time=%f",pMediaStreamContext->type, pMediaStreamContext->fragmentDescriptor.Time);
					pMediaStreamContext->eos = true;
					break;
				}
				else
				{
					uint32_t timeScale = segmentTemplates.GetTimescale();
					double segmentDuration = ComputeFragmentDuration( segmentTemplates.GetDuration(), timeScale );

					if (skipToEnd)
					{
						skipTime = mPeriodEndTime - pMediaStreamContext->fragmentDescriptor.Time;
						if ( skipTime > segmentDuration )
						{
							skipTime -= segmentDuration;
						}
						else
						{
							skipTime = 0;
						}
					}
					
					if(!aamp->IsLive())
					{
						if( timeScale )
						{
							mFirstPTS = (double)segmentTemplates.GetPresentationTimeOffset() / (double)timeScale;
						}
						if( updateFirstPTS )
						{
							mFirstPTS += skipTime;
							AAMPLOG_TRACE("Type[%d] updateFirstPTS: %f SkipTime: %f",mFirstPTS, skipTime);
						}
					}
					if(mMinUpdateDurationMs > MAX_DELAY_BETWEEN_MPD_UPDATE_MS)
					{
						AAMPLOG_INFO("Minimum Update Period is larger  than Max mpd update delay");
						break;
					}					
					else if (skipTime >= segmentDuration)
					{ // seeking past more than one segment
						uint64_t number = skipTime / segmentDuration;
						double fragmentTimeFromNumber = segmentDuration * number;
						
						pMediaStreamContext->fragmentDescriptor.Number += number;
						pMediaStreamContext->fragmentDescriptor.Time += fragmentTimeFromNumber;
						pMediaStreamContext->fragmentTime = fragmentTimeFromNumber;

						pMediaStreamContext->lastSegmentNumber = pMediaStreamContext->fragmentDescriptor.Number;
						skipTime -= fragmentTimeFromNumber;
						break;
					}
					else if (-(skipTime) >= segmentDuration)
					{
						pMediaStreamContext->fragmentDescriptor.Number--;
						pMediaStreamContext->fragmentTime -= segmentDuration;
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

		AAMPLOG_INFO("Exit :Type[%d] timeLineIndex %d fragmentRepeatCount %d fragmentDescriptor.Number %" PRIu64 " fragmentTime %f FTime:%f",pMediaStreamContext->type,
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

				unsigned int lastReferencedSize = 0;
				float lastFragmentDuration = 0.00;

				while ((fragmentTime < skipTime) || skipToEnd)
				{
					if (ParseSegmentIndexBox(pMediaStreamContext->index_ptr, pMediaStreamContext->index_len, fragmentIndex++, &referenced_size, &fragmentDuration, NULL))
					{
						lastFragmentDuration = fragmentDuration;
						lastReferencedSize = referenced_size;

						fragmentTime += fragmentDuration;
						pMediaStreamContext->fragmentOffset += referenced_size;
					}
					else if (skipToEnd)
					{
						fragmentTime -= lastFragmentDuration;
						pMediaStreamContext->fragmentOffset -= lastReferencedSize;
						fragmentIndex--;
						break;
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
			AAMPLOG_INFO("Enter : fragmentIndex %d skipTime %f",
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
							AAMPLOG_WARN("Printing diff value for adjusting %lld",diff);
							if(diff > 0)
							{
								double diffSeconds = double(diff) / timescale;
								skipTime -= diffSeconds;
							}
						}
						else
						{
							AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Video SegmentUrl is empty");
						}
					}
					else
					{
						AAMPLOG_WARN("videoContext  is null");  //CID:82361 - Null Returns
					}
				}

				while ((skipTime != 0) || skipToEnd)
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
						if (skipToEnd)
						{
							if ((pMediaStreamContext->fragmentIndex + 1) >= segmentURLs.size())
							{
								break;
							}

							pMediaStreamContext->fragmentIndex++;
							pMediaStreamContext->fragmentTime += segmentDuration;
						}
						else if (skipTime >= segmentDuration)
						{
							pMediaStreamContext->fragmentIndex++;
							skipTime -= segmentDuration;
							pMediaStreamContext->fragmentTime += segmentDuration;
						}
						else if (-(skipTime) >= segmentDuration)
						{
							pMediaStreamContext->fragmentIndex--;
							skipTime += segmentDuration;
							pMediaStreamContext->fragmentTime -= segmentDuration;
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
				AAMPLOG_WARN("StreamAbstractionAAMP_MPD: SegmentUrl is empty");
			}

			AAMPLOG_INFO("Exit : fragmentIndex %d segmentDuration %f",
					pMediaStreamContext->fragmentIndex, segmentDuration);
		}
		else
		{
			AAMPLOG_ERR("not-yet-supported mpd format");
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
	//FN_TRACE_F_MPD( __FUNCTION__ );
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
				AAMPLOG_WARN("key   is null");  //CID:85916 - Null Returns
			}
		}
	}
}



/**
 * @brief Get mpd object of manifest
 * @retval AAMPStatusType indicates if success or fail
*/
AAMPStatusType StreamAbstractionAAMP_MPD::GetMpdFromManfiest(const GrowableBuffer &manifest, MPD * &mpd, std::string manifestUrl, bool init)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
					if(mIsLiveManifest)
					{
						mHasServerUtcTime = FindServerUTCTime(root);
					}
					if(mIsFogTSB && ISCONFIGSET(eAAMPConfig_InterruptHandling))
					{
						FindPeriodGapsAndReport();
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
				SAFE_DELETE(root);
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
 * @retval xml node
 */
Node* aamp_ProcessNode(xmlTextReaderPtr *reader, std::string url, bool isAd)
{
	//FN_TRACE_F_MPD( __FUNCTION__ );
	int type = xmlTextReaderNodeType(*reader);

	if (type != WhiteSpace && type != Text)
	{
		while (type == Comment || type == WhiteSpace)
		{
			if(!xmlTextReaderRead(*reader))
			{
				AAMPLOG_WARN("xmlTextReaderRead  failed");
			}
			type = xmlTextReaderNodeType(*reader);
		}

		Node *node = new Node();
		node->SetType(type);
		node->SetMPDPath(Path::GetDirectoryPath(url));

		const char *name = (const char *)xmlTextReaderConstName(*reader);
		if (name == NULL)
		{
			SAFE_DELETE(node);
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
		int subnodeType = xmlTextReaderNodeType(*reader);

		while (ret == 1)
		{
			if (!strcmp(name, (const char *)xmlTextReaderConstName(*reader)))
			{
				return node;
			}

			if(subnodeType != Comment && subnodeType != WhiteSpace)
			{
				subnode = aamp_ProcessNode(reader, url, isAd);
				if (subnode != NULL)
					node->AddSubNode(subnode);
			}

			ret = xmlTextReaderRead(*reader);
			subnodeType = xmlTextReaderNodeType(*reader);
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
inline double safeMultiply(const  unsigned int first, const unsigned int second)
{
    return static_cast<double>(first * second);
}
/**
 * @brief Parse duration from ISO8601 string
 * @param ptr ISO8601 string
 * @return durationMs duration in milliseconds
 */
static double ParseISO8601Duration(const char *ptr)
{
	int years = 0;
	int months = 0;
	int days = 0;
	int hour = 0;
	int minute = 0;
	double seconds = 0.0;
	double returnValue = 0.0;
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
				AAMPLOG_WARN("years %d", years);
				ptr = temp + 1;
			}
			temp = strchr(ptr, 'M');
			if (temp && ( indexforM < indexforT ) )
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
			if (temp)
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
		AAMPLOG_WARN("Invalid input %s", ptr);
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
	FN_TRACE_F_MPD( __FUNCTION__ );
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
 * @return The preference level for the DRM type.
 */
int StreamAbstractionAAMP_MPD::GetDrmPrefs(const std::string& uuid)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	auto iter = mDrmPrefs.find(uuid);

	if (iter != mDrmPrefs.end())
	{
		return iter->second;
	}

	return 0; // Unknown DRM
}

/**
 * @brief Get the UUID of preferred DRM.
 * @return The UUID of preferred DRM
 */
std::string StreamAbstractionAAMP_MPD::GetPreferredDrmUUID()
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
 * @retval shared_ptr of AampDrmHelper
 */
std::shared_ptr<AampDrmHelper> StreamAbstractionAAMP_MPD::CreateDrmHelper(IAdaptationSet * adaptationSet,MediaType mediaType)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	const vector<IDescriptor*> contentProt = GetContentProtection(adaptationSet, mediaType); 
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

	AAMPLOG_TRACE("[HHH] contentProt.size= %d", contentProt.size());
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
				AAMPLOG_INFO("cencDefaultData= %s", cencDefaultData.c_str());
			}
		}
	
		// Look for UUID in schemeIdUri by matching any UUID to maintian backwards compatibility
		std::regex rgx(".*([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[1-5][0-9a-fA-F]{3}-[89abAB][0-9a-fA-F]{3}-[0-9a-fA-F]{12}).*");
		std::smatch uuid;
		if (!std::regex_search(schemeIdUri, uuid, rgx))
		{
			AAMPLOG_WARN("(%s) got schemeID empty at ContentProtection node-%d", getMediaTypeName(mediaType), iContentProt);
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
				/**< PSSH Data can be represented in <mspr:pro> tag also in PR 
				 * reference : https://docs.microsoft.com/en-us/playready/specifications/mpeg-dash-playready#2-playready-dash-content-protection-scheme
				*/
				/*** TODO: Enable the condition after OCDM support added  */
				/** if ((tagName.find("pssh") != std::string::npos) || (tagName.find("mspr:pro") != std::string::npos))*/
				if (tagName.find("pssh") != std::string::npos) 
				{
					string psshData = node.at(i)->GetText();
					data = base64_Decode(psshData.c_str(), &dataLength);
					if (0 == dataLength)
					{
						AAMPLOG_WARN("base64_Decode of pssh resulted in 0 length");
						if (data)
						{
							free(data);
							data = NULL;
						}
					}
					else
					{
						/**< Time being giving priority to cenc:ppsh if it is present */
						break;
					}
				}
				else if (tagName.find("mspr:pro") != std::string::npos)
				{
					AAMPLOG_WARN("Unsupported  PSSH data format - MSPR found in manifest");
				}
			}
		}

		// A special PSSH is used to signal data to append to the widevine challenge request
		if (drmInfo.systemUUID == CONSEC_AGNOSTIC_UUID)
		{
			if (data)
			{
				contentMetadata = aamp_ExtractWVContentMetadataFromPssh((const char*)data, dataLength);
				free(data);
                                data = NULL;
			}
			else
			{
				AAMPLOG_WARN("data is NULL");
			}
			continue;
		}

		if (aamp->mIsWVKIDWorkaround && drmInfo.systemUUID == CLEARKEY_UUID ){
			/* WideVine KeyID workaround present , UUID change from clear key to widevine **/
			AAMPLOG_INFO("WideVine KeyID workaround present, processing KeyID from clear key to WV Data");
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
			AAMPLOG_WARN("(%s) Failed to locate DRM helper for UUID %s", getMediaTypeName(mediaType), drmInfo.systemUUID.c_str());
			/** Preferred DRM configured and it is failed hhen exit here */
			if(aamp->isPreferredDRMConfigured && (GetPreferredDrmUUID() == drmInfo.systemUUID) && !aamp->mIsWVKIDWorkaround){
				AAMPLOG_ERR("(%s) Preffered DRM Failed to locate with UUID %s", getMediaTypeName(mediaType), drmInfo.systemUUID.c_str());
				if (data)
				{
					free(data);
					data = NULL;
				}
				break;
			}
		}
		else if (data && dataLength)
		{
			tmpDrmHelper = AampDrmHelperEngine::getInstance().createHelper(drmInfo, mLogObj);

			if (!tmpDrmHelper->parsePssh(data, dataLength))
			{
				AAMPLOG_WARN("(%s) Failed to Parse PSSH from the DRM InitData", getMediaTypeName(mediaType));
			}
			else
			{
				if (forceSelectDRM){
					AAMPLOG_INFO("(%s) If Widevine DRM Selected due to Widevine KeyID workaround",
						getMediaTypeName(mediaType));
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
					AAMPLOG_WARN("(%s) Created DRM helper for UUID %s and best to use", getMediaTypeName(mediaType), drmInfo.systemUUID.c_str());
					drmHelper = tmpDrmHelper;
				}
			}
		}
		else
		{
			AAMPLOG_WARN("(%s) No PSSH data available from the stream for UUID %s", getMediaTypeName(mediaType), drmInfo.systemUUID.c_str());
			/** Preferred DRM configured and it is failed then exit here */
			if(aamp->isPreferredDRMConfigured && (GetPreferredDrmUUID() == drmInfo.systemUUID)&& !aamp->mIsWVKIDWorkaround){
				AAMPLOG_ERR("(%s) No PSSH data available for Preffered DRM with UUID  %s", getMediaTypeName(mediaType), drmInfo.systemUUID.c_str());
				if (data)
				{
					free(data);
					data = NULL;
				}
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
 */
void StreamAbstractionAAMP_MPD::ProcessVssContentProtection(std::shared_ptr<AampDrmHelper> drmHelper, MediaType mediaType)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
					AAMPLOG_ERR("(%s) pthread_join returned %d for createDRMSession Thread", getMediaTypeName(mediaType), rc);
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
				AAMPLOG_INFO("Thread created");
				drmSessionThreadStarted = true;
				mLastDrmHelper = drmHelper;
				aamp->setCurrentDrm(drmHelper);
			}
			else
			{
				AAMPLOG_ERR("(%s) pthread_create failed for CreateDRMSession : error code %d, %s", getMediaTypeName(mediaType), errno, strerror(errno));
			}
		}
		else
		{
			AAMPLOG_WARN("sessionParams  is null");  //CID:85612 - Null Returns
		}
	}
	else
	{
		AAMPLOG_WARN("(%s) Skipping creation of session for duplicate helper", getMediaTypeName(mediaType));
	}
}


/**
 * @brief Process content protection of adaptation
 */
void StreamAbstractionAAMP_MPD::ProcessContentProtection(IAdaptationSet * adaptationSet, MediaType mediaType, std::shared_ptr<AampDrmHelper> drmHelper)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
				AAMPLOG_ERR("(%s) pthread_join returned %d for createDRMSession Thread", getMediaTypeName(mediaType), rc);
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
			AAMPLOG_ERR("(%s) pthread_create failed for CreateDRMSession : error code %d, %s", getMediaTypeName(mediaType), errno, strerror(errno));
		}
		AAMPLOG_INFO("Current DRM Selected is %s", drmHelper->friendlyName().c_str());
	}
	else if (!drmHelper)
	{
		AAMPLOG_ERR("(%s) Failed to create DRM helper", getMediaTypeName(mediaType));
	}
	else
	{
		AAMPLOG_WARN("(%s) Skipping creation of session for duplicate helper", getMediaTypeName(mediaType));
	}
}

#else

/**
 * @brief Process content protection of adaptation
 */
void StreamAbstractionAAMP_MPD::ProcessContentProtection(IAdaptationSet * adaptationSet,MediaType mediaType, std::shared_ptr<AampDrmHelper> drmHelper)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	AAMPLOG_WARN("MPD DRM not enabled");
}
#endif


/**
 *   @brief  GetFirstSegment start time from period
 *   @param  period
 *   @retval start time
 */
uint64_t GetFirstSegmentStartTime(IPeriod * period)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
 *   @param  period Segment period
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


double aamp_GetPeriodNewContentDuration(dash::mpd::IMPD *mpd, IPeriod * period, uint64_t &curEndNumber)
{
	FN_TRACE_F_MPD( __FUNCTION__ );

	double durationMs = 0;
	bool found = false;
	std::string tempString = period->GetDuration();
        if(!tempString.empty())
        {
                durationMs = ParseISO8601Duration( tempString.c_str());
		found = true;
        	AAMPLOG_INFO("periodDuration %f", durationMs);
	}
	size_t numPeriods = mpd->GetPeriods().size();
        if(0 == durationMs && mpd->GetType() == "static" && numPeriods == 1)
        {
                std::string durationStr =  mpd->GetMediaPresentationDuration();
                if(!durationStr.empty())
                {
                        durationMs = ParseISO8601Duration( durationStr.c_str());
			found = true;
			AAMPLOG_INFO("mediaPresentationDuration based periodDuration %f", durationMs);
                }
                else
                {
                        AAMPLOG_INFO("mediaPresentationDuration missing in period %s", period->GetId().c_str());
                }
        }
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
					double timelineDurationMs = ComputeFragmentDuration(timeline->GetDuration(),timeScale) * 1000;
					if(found)
					{
						curEndNumber = startNumber + segmentCount - 1;
						startNumber = curEndNumber++;
					}
					else
					{
						for(int i=0;i<segmentCount;i++)
						{
							if(startNumber > curEndNumber)
							{
								durationMs += timelineDurationMs;
								curEndNumber = startNumber;
							}
							startNumber++;
						}
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
				AAMPLOG_TRACE("tscale: %" PRIu32 " offset : %" PRIu64 "", timeScale, presentationTimeOffset);
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
						AAMPLOG_TRACE("timeline start : %" PRIu64 " offset delta : %lf", timelineStart,duration);
					}
					duration = (double) deltaBwFirstSegmentAndOffset / timeScale;
					if(duration != 0.0f)
						AAMPLOG_INFO("offset delta : %lf",  duration);

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
	FN_TRACE_F_MPD( __FUNCTION__ );
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
 * @retval current period's start time
 */
double StreamAbstractionAAMP_MPD::GetPeriodStartTime(IMPD *mpd, int periodIndex)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	double periodStart = 0;
	double  periodStartMs = 0;
	if( periodIndex<0 )
	{
		AAMPLOG_WARN( "periodIndex<0" );
	}
	else if(mpd != NULL )
	{
		int periodCnt= mpd->GetPeriods().size();
		if(periodIndex < periodCnt)
		{
			string startTimeStr = mpd->GetPeriods().at(periodIndex)->GetStart();
			if(!startTimeStr.empty())
			{
				periodStartMs = ParseISO8601Duration(startTimeStr.c_str()) + (aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset(mpd->GetPeriods().at(periodIndex)) * 1000);
				periodStart =  mAvailabilityStartTime + (periodStartMs / 1000);
				AAMPLOG_INFO("StreamAbstractionAAMP_MPD: - MPD periodIndex %d AvailStartTime %f periodStart %f %s", periodIndex, mAvailabilityStartTime, periodStart,startTimeStr.c_str());
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
				periodStart =  ((double)durationTotal / (double)1000);
				if(aamp->IsLiveStream() && (periodStart > 0))
				{
					periodStart += mAvailabilityStartTime;
				}

				AAMPLOG_INFO("StreamAbstractionAAMP_MPD: - MPD periodIndex %d periodStart %f", periodIndex, periodStart);
			}
		}
	}
	else
	{
		AAMPLOG_WARN("mpd is null");  //CID:83436 Null Returns
	}
	return periodStart;
}


/**
 * @brief Get duration of current period
 * @retval current period's duration
 */
double StreamAbstractionAAMP_MPD::GetPeriodDuration(IMPD *mpd, int periodIndex)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
				AAMPLOG_INFO("StreamAbstractionAAMP_MPD: MPD periodIndex:%d periodDuration %f", periodIndex, periodDuration);
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
					AAMPLOG_INFO("StreamAbstractionAAMP_MPD: [MediaPresentation] - MPD periodIndex:%d periodDuration %f", periodIndex, periodDuration);
				}
				else
				{
					string curStartStr = mpd->GetPeriods().at(periodIndex)->GetStart();
					string nextStartStr = "";
					if(periodIndex+1 < periodCnt)
					{
						nextStartStr = mpd->GetPeriods().at(periodIndex+1)->GetStart();
					}
					if(!curStartStr.empty() && (!nextStartStr.empty()) && !aamp->IsUninterruptedTSB())
					{
						double  curPeriodStartMs = 0;
						double  nextPeriodStartMs = 0;
						curPeriodStartMs = ParseISO8601Duration(curStartStr.c_str()) + (aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset(mpd->GetPeriods().at(periodIndex)) * 1000);
						nextPeriodStartMs = ParseISO8601Duration(nextStartStr.c_str()) + (aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset(mpd->GetPeriods().at(periodIndex+1)) * 1000);
						periodDurationMs = nextPeriodStartMs - curPeriodStartMs;
						periodDuration = ((double)periodDurationMs / (double)1000);
						if(periodDuration != 0.0f)
							AAMPLOG_INFO("StreamAbstractionAAMP_MPD: [StartTime based] - MPD periodIndex:%d periodDuration %f", periodIndex, periodDuration);
					}
					else
					{
						if(IsEmptyPeriod(mpd->GetPeriods().at(periodIndex), mIsFogTSB))
						{
							// Final empty period, return duration as 0 incase if GetPeriodDuration is called for this.
							periodDurationMs = 0;
							periodDuration = 0;
						}
						else
						{
							periodDurationMs = aamp_GetPeriodDuration(mpd, periodIndex, mLastPlaylistDownloadTimeMs);
							periodDuration = ((double)periodDurationMs / (double)1000);
						}
						AAMPLOG_INFO("StreamAbstractionAAMP_MPD: [Segments based] - MPD periodIndex:%d periodDuration %f", periodIndex, periodDuration);
					}
				}
			}
		}
	}
	else
	{
		AAMPLOG_WARN("mpd is null");  //CID:83436 Null Returns
	}
	return periodDurationMs;
}


/**
 * @brief Get end time of current period
 * @retval current period's end time
 */
double StreamAbstractionAAMP_MPD::GetPeriodEndTime(IMPD *mpd, int periodIndex, uint64_t mpdRefreshTime)
{
    FN_TRACE_F_MPD( __FUNCTION__ );
	double periodStartMs = 0;
	double periodDurationMs = 0;
	double periodEndTime = 0;
	double availablilityStart = 0;
	IPeriod *period = NULL;
	if(mpd != NULL)
	{
		int periodCnt= mpd->GetPeriods().size();
		if(periodIndex < periodCnt)
		{
			period = mpd->GetPeriods().at(periodIndex);
		}
	}
	else
	{
		AAMPLOG_WARN("mpd is null");  //CID:80459 , 80529- Null returns
	}
	if(period != NULL)
	{
		string startTimeStr = period->GetStart();
		periodDurationMs = GetPeriodDuration(mpd, periodIndex);

		if((0 > mAvailabilityStartTime) && !(mpd->GetType() == "static"))
		{
			AAMPLOG_WARN("availabilityStartTime required to calculate period duration not present in MPD");
		}
		else if(0 == periodDurationMs)
		{
			AAMPLOG_WARN("Could not get valid period duration to calculate end time");
		}
		else
		{
			if(startTimeStr.empty())
			{
				AAMPLOG_WARN("Period startTime is not present in MPD, so calculating start time with previous period durations");
				periodStartMs = GetPeriodStartTime(mpd, periodIndex) * 1000;
			}
			else
			{
				periodStartMs = ParseISO8601Duration(startTimeStr.c_str()) + (aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset(period)* 1000);
			}
			periodEndTime = ((double)(periodStartMs + periodDurationMs) /1000);
			if(aamp->IsLiveStream())
			{
				periodEndTime +=  mAvailabilityStartTime;
			}
		}
		AAMPLOG_INFO("StreamAbstractionAAMP_MPD: MPD periodIndex:%d periodEndTime %f", periodIndex, periodEndTime);
	}
	else
	{
		AAMPLOG_WARN("period is null");  //CID:85519- Null returns
	}
	return periodEndTime;
}

/**
 *   @brief  Get Period Duration
 *   @retval period duration in milli seconds
  */
double aamp_GetPeriodDuration(dash::mpd::IMPD *mpd, int periodIndex, uint64_t mpdDownloadTime)
{
    FN_TRACE_F_MPD( __FUNCTION__ );
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
			AAMPLOG_WARN("mediaPresentationDuration missing in period %s", period->GetId().c_str());
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
							AAMPLOG_TRACE("timeLineIndex[%d] size [%lu] updated durationMs[%lf]", timeLineIndex, timelines.size(), durationMs);
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
									AAMPLOG_WARN("mpdDownloadTime required to calculate period duration not provided");
								}
								else if(minimumUpdatePeriodStr.empty())
								{
									AAMPLOG_WARN("minimumUpdatePeriod required to calculate period duration not present in MPD");
								}
								else if(availabilityStartStr.empty())
								{
									AAMPLOG_WARN("availabilityStartTime required to calculate period duration not present in MPD");
								}
								else
								{
									double periodStart = 0;
									double availablilityStart = 0;
									double minUpdatePeriod = 0;
									periodStart = ParseISO8601Duration( periodStartStr.c_str() );
									availablilityStart = ISO8601DateTimeToUTCSeconds(availabilityStartStr.c_str()) * 1000;
									minUpdatePeriod = ParseISO8601Duration( minimumUpdatePeriodStr.c_str() );
									AAMPLOG_INFO("periodStart %lf availabilityStartTime %lf minUpdatePeriod %lf mpdDownloadTime %lf", periodStart, availablilityStart, minUpdatePeriod, mpdDownloadTime);
									double periodEndTime = mpdDownloadTime + minUpdatePeriod;
									double periodStartTime = availablilityStart + periodStart;
									durationMs = periodEndTime - periodStartTime;
									if(durationMs <= 0)
									{
										AAMPLOG_WARN("Invalid period duration periodStartTime %lf periodEndTime %lf durationMs %lf", periodStartTime, periodEndTime, durationMs);
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
										AAMPLOG_WARN("Invalid period duration periodStartTime %lf nextPeriodStart %lf durationMs %lf", periodStart, nextPeriodStart, durationMs);
										durationMs = 0;
									}
								}
								else
								{
									AAMPLOG_WARN("Next period startTime missing periodIndex %d", periodIndex);
								}
							}
						}
						else
						{
							AAMPLOG_WARN("Start time and duration missing in period %s", period->GetId().c_str());
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
								AAMPLOG_WARN("segmentURLs  is null");  //CID:82729 - Null Returns
							}
						}
						else
						{
							AAMPLOG_ERR("not-yet-supported mpd format");
						}
					}
				}
			}
			else
			{
				AAMPLOG_WARN("firstAdaptation is null");  //CID:84261 - Null Returns
			}
		}
	}
	return durationMs;
}

/**
 *   @brief  Initialize a newly created object.
 *   @note   To be implemented by sub classes
 *   @retval true on success
 *   @retval false on failure
 */
AAMPStatusType StreamAbstractionAAMP_MPD::Init(TuneType tuneType)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
	pushEncInitFragment = newTune || (eTUNETYPE_RETUNE == tuneType) || aamp->mbDetached;
#endif
        if(aamp->mbDetached){
                /* No more needed reset it **/
                aamp->mbDetached = false;
        }

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
			AAMPLOG_WARN("mpd is null");  //CID:81139 , 81645 ,82315.83556- Null Returns
			return ret;
		}
		if(!tempString.empty())
		{
			durationMs = ParseISO8601Duration( tempString.c_str() );
			mpdDurationAvailable = true;
			AAMPLOG_WARN("StreamAbstractionAAMP_MPD: MPD duration str %s val %" PRIu64 " seconds", tempString.c_str(), durationMs/1000);
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

		// Validate tune type
		// (need to find a better way to do this)
		if (tuneType == eTUNETYPE_NEW_NORMAL)
		{
			if(!mIsLiveStream && !aamp->mIsDefaultOffset)
			{
				tuneType = eTUNETYPE_NEW_END;
			}
		}


		if(mIsLiveStream)
		{
			/*LL DASH VERIFICATION START*/
			ret = EnableAndSetLiveOffsetForLLDashPlayback((MPD*)this->mpd);
			if(eAAMPSTATUS_OK != ret && ret == eAAMPSTATUS_MANIFEST_PARSE_ERROR)
			{
				aamp->SendErrorEvent(AAMP_TUNE_INVALID_MANIFEST_FAILURE);
				return ret;	
			}	
			if (aamp->mIsVSS)
			{
				std::string vssVirtualStreamId = GetVssVirtualStreamID();
				
				if (!vssVirtualStreamId.empty())
				{
					AAMPLOG_INFO("Virtual stream ID :%s", vssVirtualStreamId.c_str());
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
			AAMPLOG_WARN("StreamAbstractionAAMP_MPD: AvailabilityStartTime=%f", mAvailabilityStartTime);

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
					segmentDurationSeconds = ParseISO8601Duration( tempStr.c_str() )/1000;
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

			AAMPLOG_WARN("StreamAbstractionAAMP_MPD: MPD minupdateduration val %" PRIu64 " seconds mTSBDepth %f mPresentationOffsetDelay :%f StartTimeFirstPeriod: %lf offsetStartTime: %lf",  mMinUpdateDurationMs/1000, mTSBDepth, mPresentationOffsetDelay, mFirstPeriodStartTime, aamp->mProgressReportOffset);
		}

		for (int i = 0; i < mMaxTracks; i++)
		{
			mMediaStreamContext[i] = new MediaStreamContext(mLogObj, (TrackType)i, this, aamp, getMediaTypeName(MediaType(i)));
			mMediaStreamContext[i]->fragmentDescriptor.manifestUrl = manifestUrl;
			mMediaStreamContext[i]->mediaType = (MediaType)i;
			mMediaStreamContext[i]->representationIndex = -1;
		}

		uint64_t nextPeriodStart = 0;
		double currentPeriodStart = 0;
		double prevPeriodEndMs = 0; // used to find gaps between periods
		size_t numPeriods = mpd->GetPeriods().size();
		bool seekPeriods = true;

		if (ISCONFIGSET(eAAMPConfig_UseAbsoluteTimeline) && mIsLiveStream && !aamp->IsUninterruptedTSB())
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
					AAMPLOG_WARN("Resetting offset from start to 0");
				}
			}
		}
		for (unsigned iPeriod = 0; iPeriod < numPeriods; iPeriod++)
		{//TODO -  test with streams having multiple periods.
			IPeriod *period = mpd->GetPeriods().at(iPeriod);
			if(IsEmptyPeriod(period, mIsFogTSB))
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
				AAMPLOG_INFO("Updated duration %lf seconds", (durationMs/1000));
			}

			if(offsetFromStart >= 0 && seekPeriods)
			{
				tempString = period->GetStart();
				if(!tempString.empty() && !aamp->IsUninterruptedTSB())
				{
					periodStartMs = ParseISO8601Duration( tempString.c_str() );
				}
				else if (periodDurationMs)
				{
					periodStartMs = nextPeriodStart;
				}

				if(ISCONFIGSET(eAAMPConfig_UseAbsoluteTimeline) && aamp->IsLiveStream() && !aamp->IsUninterruptedTSB() && iPeriod == 0)
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
							AAMPLOG_WARN("GAP betwen period found :GAP:%f  mCurrentPeriodIdx %d currentPeriodStart %f offsetFromStart %f",
								periodGap, mCurrentPeriodIdx, periodStartSeconds, offsetFromStart);
						}
						if(!mIsLiveStream && periodGap > 0 )
						{
							//increment period gaps to notify partner apps during manifest parsing for VOD assets
							aamp->IncrementGaps();
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
						AAMPLOG_WARN("Skipping period %d seekPosition %f periodEnd %f offsetFromStart %f", iPeriod, seekPosition, periodEnd, offsetFromStart);
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
			AAMPLOG_WARN("Duration after GetDurationFromRepresentation %" PRIu64 " seconds", durationMs/1000);
		}

		if(0 == durationMs)
		{
			segmentTagsPresent = false;
			for(int iPeriod = 0; iPeriod < mpd->GetPeriods().size(); iPeriod++)
			{
				if(IsEmptyPeriod(mpd->GetPeriods().at(iPeriod), mIsFogTSB))
				{
					continue;
				}
				durationMs += GetPeriodDuration(mpd, iPeriod);
			}
			AAMPLOG_WARN("Duration after adding up Period Duration %" PRIu64 " seconds", durationMs/1000);
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
				AAMPLOG_WARN("StreamAbstractionAAMP_MPD: eTUNETYPE_SEEKTOLIVE");
				liveAdjust = true;
				notifyEnteringLive = true;
			}
			else if (((eTUNETYPE_SEEK == tuneType) || (eTUNETYPE_RETUNE == tuneType || eTUNETYPE_NEW_SEEK == tuneType)) && (rate > 0))
			{
				double seekWindowEnd = duration - aamp->mLiveOffset;
				// check if seek beyond live point
				if (seekPosition > seekWindowEnd)
				{
					AAMPLOG_WARN( "StreamAbstractionAAMP_MPD: offSetFromStart[%f] seekWindowEnd[%f]",
							seekPosition, seekWindowEnd);
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
					if( !IsEmptyPeriod(period, mIsFogTSB) )
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
							AAMPLOG_INFO("Adjusting to live offset offsetFromStart %f, mCurrentPeriodIdx %d", offsetFromStart, mCurrentPeriodIdx);
							mCurrentPeriodIdx--;
							while((IsEmptyPeriod(mpd->GetPeriods().at(mCurrentPeriodIdx), mIsFogTSB) && mCurrentPeriodIdx > 0))
							{
								mCurrentPeriodIdx--;
							}
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
							while((IsEmptyPeriod(mpd->GetPeriods().at(mCurrentPeriodIdx), mIsFogTSB) && mCurrentPeriodIdx > 0))
							{
								mCurrentPeriodIdx--;
							}
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
					AAMPLOG_WARN("duration %f durationMs %f mCurrentPeriodIdx %d currentPeriodStart %f offsetFromStart %f",
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
				AAMPLOG_WARN( "StreamAbstractionAAMP_MPD: liveAdjust - Updated offSetFromStart[%f] duration [%f] currentPeriodStart[%f] MaxPeriodIdx[%d]",
						offsetFromStart, duration, currentPeriodStart,mCurrentPeriodIdx);
			}

		}
		else
		{
			// Non-live - VOD/CDVR(Completed) - DELIA-30266
			if(durationMs == INVALID_VOD_DURATION)
			{
				AAMPLOG_WARN("Duration of VOD content is 0");
				return eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
			}

			double seekWindowEnd = (double) durationMs / 1000;
			if(seekPosition > seekWindowEnd)
			{
				for (int i = 0; i < mNumberOfTracks; i++)
				{
					mMediaStreamContext[i]->eosReached=true;
				}
				AAMPLOG_WARN("seek target out of range, mark EOS. playTarget:%f End:%f. ",
					seekPosition, seekWindowEnd);
				return eAAMPSTATUS_SEEK_RANGE_ERROR;
			}
		}
		mPeriodStartTime =  GetPeriodStartTime(mpd, mCurrentPeriodIdx);
		mPeriodDuration =  GetPeriodDuration(mpd, mCurrentPeriodIdx);
		mPeriodEndTime = GetPeriodEndTime(mpd, mCurrentPeriodIdx, mLastPlaylistDownloadTimeMs);
		int periodCnt= mpd->GetPeriods().size();
		if(mCurrentPeriodIdx < periodCnt)
		{
			mCurrentPeriod = mpd->GetPeriods().at(mCurrentPeriodIdx);
		}
		if(ISCONFIGSET(eAAMPConfig_UseAbsoluteTimeline) && !aamp->IsUninterruptedTSB() && mIsLiveStream)
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
			AAMPLOG_WARN("mCurrentPeriod  is null");  //CID:84770 - Null Return
		}
		mBasePeriodOffset = offsetFromStart;

		onAdEvent(AdEvent::INIT, offsetFromStart);

		UpdateLanguageList();

		if ((eTUNETYPE_SEEK == tuneType) ||
		    (eTUNETYPE_SEEKTOEND == tuneType))
		{
			forceSpeedsChangedEvent = true; // Send speed change event if seek done from non-iframe period to iframe available period to inform XRE to allow trick operations.
		}

		StreamSelection(true, forceSpeedsChangedEvent);

		if(aamp->mIsFakeTune)
		{
			// Aborting init here after stream and DRM initialization.
			return eAAMPSTATUS_FAKE_TUNE_COMPLETE;
		}
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
			aamp->SendEvent(std::make_shared<AAMPEventObject>(AAMP_EVENT_PLAYLIST_INDEXED),AAMP_EVENT_ASYNC_MODE);
			if (eTUNED_EVENT_ON_PLAYLIST_INDEXED == aamp->GetTuneEventConfig(mIsLiveStream))
			{
				if (aamp->SendTunedEvent())
				{
					AAMPLOG_WARN("aamp: mpd - sent tune event after indexing playlist");
				}
			}
			ret = UpdateTrackInfo(!newTune, true, true);

			if(eAAMPSTATUS_OK != ret)
			{
				if (ret == eAAMPSTATUS_MANIFEST_CONTENT_ERROR)
				{
					AAMPLOG_ERR("ERROR: No playable profiles found");
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
					AAMPLOG_INFO("Culled seconds = %f, Adjusting seekPos after considering new culled value", culled);
					aamp->UpdateCullingState(culled);
				}

				double durMs = 0;
				for(int periodIter = 0; periodIter < mpd->GetPeriods().size(); periodIter++)
				{
					if(!IsEmptyPeriod(mpd->GetPeriods().at(periodIter), mIsFogTSB))
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
			if(mIsLiveStream && aamp->mLanguageChangeInProgress)
			{
				aamp->mLanguageChangeInProgress = false;
				ApplyLiveOffsetWorkaroundForSAP(offsetFromStart);
			}
			else if ((tuneType == eTUNETYPE_SEEKTOEND) ||
				 (tuneType == eTUNETYPE_NEW_END))
			{
				SeekInPeriod( 0, true );
				seekPosition = mMediaStreamContext[eMEDIATYPE_VIDEO]->fragmentTime;
			}
			else
			{
				if( (aamp->GetLLDashServiceData()->lowLatencyMode ) && 
					(!aamp->GetLLDashServiceData()->isSegTimeLineBased) )
				{
					offsetFromStart = offsetFromStart+(aamp->GetLLDashServiceData()->fragmentDuration - aamp->GetLLDashServiceData()->availabilityTimeOffset);
				}
				SeekInPeriod( offsetFromStart);
			}

			if(!ISCONFIGSET(eAAMPConfig_MidFragmentSeek))
			{
				seekPosition = mMediaStreamContext[eMEDIATYPE_VIDEO]->fragmentTime;
				if((0 != mCurrentPeriodIdx && !ISCONFIGSET(eAAMPConfig_UseAbsoluteTimeline)) || aamp->IsUninterruptedTSB())
				{
					// Avoid adding period start time for Absolute progress reporting,
					// position is adjusted in TuneHelper based on current period start,
					// So just save fragmentTime.
					seekPosition += currentPeriodStart;
				}
			}
			else if (!seekPosition)
			{
				seekPosition = offsetFromStart;
			}
			for (int i = 0; i < mNumberOfTracks; i++)
			{
				if(0 != mCurrentPeriodIdx)
				{
					mMediaStreamContext[i]->fragmentTime = seekPosition;
				}
				mMediaStreamContext[i]->periodStartOffset = currentPeriodStart;
			}
			if(aamp->GetLLDashServiceData()->lowLatencyMode)
			{
				aamp->mLLActualOffset = seekPosition;
			}	
			AAMPLOG_INFO("offsetFromStart(%f) seekPosition(%f) currentPeriodStart(%f)", offsetFromStart,seekPosition, currentPeriodStart);

			if (newTune )
			{
				aamp->SetState(eSTATE_PREPARING);

				// send the http response header values if available
				if(!aamp->httpHeaderResponses.empty()) {
					aamp->SendHTTPHeaderResponse();
				}

				double durationSecond = (((double)durationMs)/1000);
				aamp->UpdateDuration(durationSecond);
				GetCulledSeconds();
				aamp->UpdateRefreshPlaylistInterval((float)mMinUpdateDurationMs / 1000);
				mProgramStartTime = mAvailabilityStartTime;
			}
		}
		else
		{
			AAMPLOG_WARN("No adaptation sets could be selected");
			retval = eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
		}
	}
	else
	{
		AAMPLOG_ERR("StreamAbstractionAAMP_MPD: corrupt/invalid manifest");
		retval = eAAMPSTATUS_MANIFEST_PARSE_ERROR;
	}
	if (ret == eAAMPSTATUS_OK)
	{
#ifdef AAMP_MPD_DRM
		//CheckForInitalClearPeriod() check if the current period is clear or encrypted
		if (pushEncInitFragment && CheckForInitalClearPeriod())
		{
			AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Pushing EncryptedHeaders");
			if(PushEncryptedHeaders())
			{
				aamp->mPipelineIsClear = false;
				aamp->mEncryptedPeriodFound = false;
			}
			else if (mIsLiveStream)
			{
				AAMPLOG_INFO("Pipeline set as clear since no enc perid found");
				//If no encrypted period is found, then update the pipeline status
				aamp->mPipelineIsClear = true;
			}
		}
#endif

		AAMPLOG_WARN("StreamAbstractionAAMP_MPD: fetch initialization fragments");
		FetchAndInjectInitFragments();
	}

	return retval;
}

/**
 * @brief Get duration though representation iteration
 * @retval duration in milliseconds
 */
uint64_t aamp_GetDurationFromRepresentation(dash::mpd::IMPD *mpd)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	uint64_t durationMs = 0;
	if(mpd == NULL) {
		AAMPLOG_WARN("mpd is null");  //CID:82158 - Null Returns
		return durationMs;
	}
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
			AAMPLOG_WARN("mpd is null");  //CID:82158 - Null Returns
		}
		
		if(period != NULL)
		{
			const std::vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();
			if (adaptationSets.size() > 0)
			{
				IAdaptationSet * firstAdaptation = adaptationSets.at(0);
				ISegmentTemplate *AdapSegmentTemplate = NULL;
				ISegmentTemplate *RepSegmentTemplate = NULL;
				if (firstAdaptation == NULL)
				{
					AAMPLOG_WARN("firstAdaptation is null");  //CID:82158 - Null Returns
					return durationMs;
				}
				AdapSegmentTemplate = firstAdaptation->GetSegmentTemplate();
				const std::vector<IRepresentation *> representations = firstAdaptation->GetRepresentation();
				if (representations.size() > 0)
				{
					RepSegmentTemplate  = representations.at(0)->GetSegmentTemplate();
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
								AAMPLOG_TRACE("period[%d] timeLineIndex[%d] size [%lu] updated durationMs[%" PRIu64 "]", iPeriod, timeLineIndex, timelines.size(), durationMs);
								timeLineIndex++;
							}
						}
					}
					else
					{
						AAMPLOG_WARN("media is null");  //CID:83185 - Null Returns
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
								AAMPLOG_WARN("segmentURLs is null");  //CID:82113 - Null Returns
							}
							durationMs += ComputeFragmentDuration(segmentList->GetDuration(), segmentList->GetTimescale()) * 1000;
						}
						else
						{
							AAMPLOG_ERR("not-yet-supported mpd format");
						}
					}
				}
			}
		}
		else
		{
			AAMPLOG_WARN("period is null");  //CID:81482 - Null Returns
		}
	}
	return durationMs;
}


/**
 * @fn IsPeriodEncrypted
 * @param[in] period - current period
 * @brief check if current period is encrypted
 * @retval true on success
 */
bool StreamAbstractionAAMP_MPD::IsPeriodEncrypted(IPeriod *period)
{
	for(int i = mNumberOfTracks - 1; i >= 0; i--)
	{
		if(period != NULL)
		{
			size_t numAdaptationSets = period->GetAdaptationSets().size();
			for(unsigned iAdaptationSet = 0; iAdaptationSet < numAdaptationSets; iAdaptationSet++)
			{
				const IAdaptationSet *adaptationSet = period->GetAdaptationSets().at(iAdaptationSet);
				if(adaptationSet != NULL)
				{
					if (IsContentType(adaptationSet, (MediaType)i))
					{
						if(0 != GetContentProtection(adaptationSet, (MediaType) i).size())
						{
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

/**
 * @brief Update MPD manifest
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
		AAMPLOG_WARN("StreamAbstractionAAMP_MPD: manifest retrieved from cache");
		retrievedPlaylistFromCache = true;
	}
	double downloadTime;
	bool updateVideoEndMetrics = false;
	long http_error = 0;
	if (!retrievedPlaylistFromCache)
	{
		memset(&manifest, 0, sizeof(manifest));
		aamp->profiler.ProfileBegin(PROFILE_BUCKET_MANIFEST);
		aamp->SetCurlTimeout(aamp->mManifestTimeoutMs,eCURLINSTANCE_VIDEO);
		gotManifest = aamp->GetFile(manifestUrl, &manifest, manifestUrl, &http_error, &downloadTime, NULL, eCURLINSTANCE_VIDEO, true, eMEDIATYPE_MANIFEST);
		aamp->SetCurlTimeout(aamp->mNetworkTimeoutMs,eCURLINSTANCE_VIDEO);
		//update videoend info
		updateVideoEndMetrics = true;
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
			aamp->profiler.ProfileEnd(PROFILE_BUCKET_MANIFEST);
			if (this->mpd != NULL && (CURLE_OPERATION_TIMEDOUT == http_error || CURLE_COULDNT_CONNECT == http_error))
			{
				//Skip this for first ever update mpd request
				mNetworkDownDetected = true;
				AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Ignore curl timeout");
				ret = AAMPStatusType::eAAMPSTATUS_OK;
			}
			else
			{
				aamp->UpdateDuration(0);
				aamp->SendDownloadErrorEvent(AAMP_TUNE_MANIFEST_REQ_FAILED, http_error);
				AAMPLOG_WARN("StreamAbstractionAAMP_MPD: manifest download failed");
				ret = AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR;
			}
		}
		else // if downloads disabled
		{
			aamp->UpdateDuration(0);
			AAMPLOG_ERR("StreamAbstractionAAMP_MPD: manifest download failed");
			ret = AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR;
		}
	}
	else
	{
		gotManifest = true;
		aamp->mManifestUrl = manifestUrl;
	}
	long parseTimeMs = 0;
	if (gotManifest)
	{
		MPD* mpd = nullptr;
		vector<std::string> locationUrl;		
		long long tStartTime = NOW_STEADY_TS_MS;
		ret = GetMpdFromManfiest(manifest, mpd, manifestUrl, init);
		// get parse time for playback stats, if required, parse error can be considered for a later point for statistics
		parseTimeMs = NOW_STEADY_TS_MS - tStartTime;
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
				SAFE_DELETE(this->mpd);
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
			AAMPLOG_WARN("Error while processing MPD, GetMpdFromManfiest returned %d", ret);
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
		AAMPLOG_WARN("aamp: error on manifest fetch");
	}
	if(updateVideoEndMetrics)
	{
		ManifestData manifestData(downloadTime * 1000, manifest.len, parseTimeMs, mpd ? mpd->GetPeriods().size() : 0);
		aamp->UpdateVideoEndMetrics(eMEDIATYPE_MANIFEST,0,http_error,manifestUrl,downloadTime, &manifestData);
	}

	if( ret == eAAMPSTATUS_MANIFEST_PARSE_ERROR || ret == eAAMPSTATUS_MANIFEST_CONTENT_ERROR)
	{
		if(NULL != manifest.ptr && !manifestUrl.empty())
		{
			int tempDataLen = (MANIFEST_TEMP_DATA_LENGTH - 1);
			char temp[MANIFEST_TEMP_DATA_LENGTH];
			strncpy(temp, manifest.ptr, tempDataLen);
			temp[tempDataLen] = 0x00;
			AAMPLOG_WARN("ERROR: Invalid Playlist URL: %s ret:%d", manifestUrl.c_str(),ret);
			AAMPLOG_WARN("ERROR: Invalid Playlist DATA: %s ", temp);
		}
		aamp->SendErrorEvent(AAMP_TUNE_INVALID_MANIFEST_FAILURE);
	}

	return ret;
}

/**
 * @brief Check if Period is empty or not
 * @retval Return true on empty Period
 */
bool StreamAbstractionAAMP_MPD::IsEmptyPeriod(IPeriod *period, bool isFogPeriod)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
			isEmptyPeriod = IsEmptyAdaptation(adaptationSet, isFogPeriod);

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
 * @brief Check if Period is empty or not
 * @retval Return true on empty Period
 */
bool StreamAbstractionAAMP_MPD::IsEmptyAdaptation(IAdaptationSet *adaptationSet, bool isFogPeriod)
{
	bool isEmptyAdaptation = true;
	IRepresentation *representation = NULL;
	ISegmentTemplate *segmentTemplate = adaptationSet->GetSegmentTemplate();
	if (segmentTemplate)
	{
		if(!isFogPeriod || (0 != segmentTemplate->GetDuration()))
		{
			isEmptyAdaptation = false;
		}
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
				if(!isFogPeriod || (0 != segmentTemplate->GetDuration()))
				{
					isEmptyAdaptation = false;
				}
			}
			else
			{
				ISegmentList *segmentList = representation->GetSegmentList();
				if(segmentList)
				{
					isEmptyAdaptation = false;
				}
				else
				{
					ISegmentBase *segmentBase = representation->GetSegmentBase();
					if(segmentBase)
					{
						isEmptyAdaptation = false;
					}
				}
			}
		}
	}
	return isEmptyAdaptation;
}

/**
 * @brief Check if Period is empty or not
 * @retval Return true on empty Period
 */
void StreamAbstractionAAMP_MPD::FindPeriodGapsAndReport()
{
	double prevPeriodEndMs = aamp->culledSeconds * 1000;
	double curPeriodStartMs = 0;
	for(int i = 0; i< mpd->GetPeriods().size(); i++)
	{
		auto tempPeriod = mpd->GetPeriods().at(i);
		std::string curPeriodStartStr = tempPeriod->GetStart();
		if(!curPeriodStartStr.empty())
		{
			curPeriodStartMs = ParseISO8601Duration(curPeriodStartStr.c_str());
		}
		if (STARTS_WITH_IGNORE_CASE(tempPeriod->GetId().c_str(), FOG_INSERTED_PERIOD_ID_PREFIX))
		{
			if(IsEmptyPeriod(tempPeriod, mIsFogTSB))
			{
				// Empty period indicates that the gap is growing, report event without duration
				aamp->ReportContentGap((long long)(prevPeriodEndMs - (aamp->mProgressReportOffset*1000)), tempPeriod->GetId());
			}
			else
			{
				// Non empty  event indicates that the gap is complete.
				if(curPeriodStartMs > 0 && prevPeriodEndMs > 0)
				{
					double periodGapMS = (curPeriodStartMs - prevPeriodEndMs);
					aamp->ReportContentGap((long long)(prevPeriodEndMs - (aamp->mProgressReportOffset*1000)), tempPeriod->GetId(), periodGapMS);
				}
			}
		}
		else if (prevPeriodEndMs > 0 && (std::round((curPeriodStartMs - prevPeriodEndMs) / 1000) > 0))
		{
			// Gap in between periods, but period ID changed after interrupt
			// Fog skips duplicate period and inserts fragments in new period.
			double periodGapMS = (curPeriodStartMs - prevPeriodEndMs);
			aamp->ReportContentGap((long long)(prevPeriodEndMs - (aamp->mProgressReportOffset*1000)), tempPeriod->GetId(), periodGapMS);
		}
		if(IsEmptyPeriod(tempPeriod, mIsFogTSB)) continue;
		double periodDuration = aamp_GetPeriodDuration(mpd, i, mLastPlaylistDownloadTimeMs);
		prevPeriodEndMs = curPeriodStartMs + periodDuration;
	}
}

/**
 * @brief Read UTCTiming element
 * @retval Return true if UTCTiming element is available in the manifest
 */
bool  StreamAbstractionAAMP_MPD::FindServerUTCTime(Node* root)
{
	bool hasServerUtcTime = false;
	if( root )
	{
		mServerUtcTime = 0;
		for ( auto node :  root->GetSubNodes() )
		{
			if(node)
			{
				if( "UTCTiming" == node->GetName() && node->HasAttribute("schemeIdUri"))
				{
					if ( SERVER_UTCTIME_DIRECT == node->GetAttributeValue("schemeIdUri") && node->HasAttribute("value"))
					{
						double currentTime = (double)aamp_GetCurrentTimeMS() / 1000;
						const std::string &value = node->GetAttributeValue("value");
						mServerUtcTime = ISO8601DateTimeToUTCSeconds(value.c_str() );
						mDeltaTime =  mServerUtcTime - currentTime;
						hasServerUtcTime = true;
						break;
					}
				}
			}
		}
	}
	return hasServerUtcTime;
}

/**
 * @brief Find timed metadata from mainifest
 */
void StreamAbstractionAAMP_MPD::FindTimedMetadata(MPD* mpd, Node* root, bool init, bool reportBulkMeta)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	std::vector<Node*> subNodes = root->GetSubNodes();


	if(!subNodes.empty())
	{
		uint64_t periodStartMS = 0;
		uint64_t periodDurationMS = 0;
		std::vector<std::string> newPeriods;
		int64_t firstSegmentStartTime = -1;

		// If we intend to use the PTS presentation time from the event to determine the event start time then
		// before parsing the events we will get the first segment time. If we do not have a valid first
		// segment time then we will get the event start time from the period start/duration
		if (ISCONFIGSET(eAAMPConfig_EnableSCTE35PresentationTime))
		{
			if (mpd != NULL)
			{
				int iPeriod = 0;
				while (iPeriod < mpd->GetPeriods().size())
				{
					IPeriod *period = mpd->GetPeriods().at(iPeriod);
					if (period == NULL)
					{
						break;
					}
					
					uint64_t segmentStartPTS = GetFirstSegmentStartTime(period);
					if (segmentStartPTS)
					{
						// Got a segment start time so convert it to ms and quit
						uint64_t timescale = GetPeriodSegmentTimeScale(period);
						if (timescale > 1)
						{
							// We have a first segment start time so we will use that
							firstSegmentStartTime = segmentStartPTS;

							firstSegmentStartTime *= 1000;
							firstSegmentStartTime /= timescale;
						}
						break;
					}
					iPeriod++;
				}
			}
			if (firstSegmentStartTime == -1)
			{
				AAMPLOG_ERR("SCTEDBG - failed to get firstSegmentStartTime");	
			}
			else
			{
				AAMPLOG_INFO("SCTEDBG - firstSegmentStartTime %lld", firstSegmentStartTime);	
			}
		}

		// Iterate through each of the MPD's Period nodes, and ProgrameInformation.
		int periodCnt = 0;
		for (size_t i=0; i < subNodes.size(); i++) {
			Node* node = subNodes.at(i);
			if(node == NULL)  //CID:163928 - forward null
			{
				AAMPLOG_WARN("node is null");  //CID:80723 - Null Returns
				return;
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
					AAMPLOG_WARN("mpd is null");  //CID:80941 - Null Returns
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
							if((name == "EventStream") && ("" != prdId) && !mCdaiObject->isPeriodExist(prdId))
							{
								bool processEventsInPeriod = ((!init || (1 < periodCnt && 0 == period->GetAdaptationSets().size())) //Take last & empty period at the MPD init AND all new periods in the MPD refresh. (No empty periods will come the middle)
								      						 || (!mIsLiveManifest && init)); //to enable VOD content to send the metadata

								bool modifySCTEProcessing = ISCONFIGSET(eAAMPConfig_EnableSCTE35PresentationTime);
								if (modifySCTEProcessing)
								{
									//LLAMA-8251
									// cdvr events that are currently recording are tagged as live - we still need to process
									// all the SCTE events for these manifests so we'll just rely on isPeriodExist() to prevent
									// repeated notifications and process all events in the manifest
									processEventsInPeriod = true;
								}

								if (processEventsInPeriod)
								{
									mCdaiObject->InsertToPeriodMap(period);	//Need to do it. Because the FulFill may finish quickly
									ProcessEventStream(periodStartMS, firstSegmentStartTime, period, reportBulkMeta);
									continue;
								}
							}
						}
						else
						{
							AAMPLOG_WARN("name is empty");  //CID:80526 - Null Returns
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
				if (schemeIdUri == aamp->schemeIdUriDai && node->HasAttribute("id")) {
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
				continue;
			}
		}
		mCdaiObject->PrunePeriodMaps(newPeriods);
	}
	else
	{
		AAMPLOG_WARN("SubNodes  is empty");  //CID:85449 - Null Returns
	}
}


/**
 * @brief Process supplemental property of a period
 */
void StreamAbstractionAAMP_MPD::ProcessPeriodSupplementalProperty(Node* node, std::string& AdID, uint64_t startMS, uint64_t durationMS, bool isInit, bool reportBulkMeta)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	if (node->HasAttribute("schemeIdUri")) {
		const std::string& schemeIdUri = node->GetAttributeValue("schemeIdUri");
		
		if(!schemeIdUri.empty())
		{
			if ((schemeIdUri == aamp->schemeIdUriDai) && node->HasAttribute("id")) {
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
			AAMPLOG_WARN("schemeIdUri  is empty");  //CID:83796 - Null Returns
		}
	}
}


/**
 * @brief Process Period AssetIdentifier
 */
void StreamAbstractionAAMP_MPD::ProcessPeriodAssetIdentifier(Node* node, uint64_t startMS, uint64_t durationMS, std::string& AssetID, std::string& ProviderID, bool isInit, bool reportBulkMeta)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
 *   @brief Process event stream.
 */
bool StreamAbstractionAAMP_MPD::ProcessEventStream(uint64_t startMS, int64_t startOffsetMS, IPeriod * period, bool reportBulkMeta)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	bool ret = false;

	const std::string &prdId = period->GetId();
	if(!prdId.empty())
	{
		uint64_t startMS1 = 0;
		//Vector of pair of scte35 binary data and correspoding duration
		std::vector<EventBreakInfo> eventBreakVec;
		if(isAdbreakStart(period, startMS1, eventBreakVec))
		{
			#define MAX_EVENT_STARTIME (24*60*60*1000)
			uint64_t maxPTSTime = ((uint64_t)0x1FFFFFFFF / (uint64_t)90); // 33 bit pts max converted to ms

			AAMPLOG_WARN("Found CDAI events for period %s ", prdId.c_str());
			for(EventBreakInfo &eventInfo : eventBreakVec)
			{
				uint64_t eventStartTime = startMS; // by default, use the time derived from the period start/duration

				// If we have a presentation time and a valid start time for the stream, then we will use the presentation 
				// time to set / adjust the event start and duration realtive to the start time of the stream
				if (eventInfo.presentationTime && (startOffsetMS > -1))
				{
					// Adjust for stream start offset and check for pts wrap
					eventStartTime = eventInfo.presentationTime;
					if (eventStartTime < startOffsetMS)
					{
						eventStartTime += (maxPTSTime - startOffsetMS);

						// If the difference is too large (>24hrs), assume this is not a pts wrap and the event is timed 
						// to occur before the start - set it to start immediately and adjust the duration accordingly
						if (eventStartTime > MAX_EVENT_STARTIME)
						{
							uint64_t diff = startOffsetMS - eventInfo.presentationTime;
							if (eventInfo.duration > diff)
							{
								eventInfo.duration -= diff;
							}
							else
							{
								eventInfo.duration = 0;
							}
							eventStartTime = 0;
							
						}
					}
					else
					{
						eventStartTime -= startOffsetMS;
					}
					AAMPLOG_INFO("SCTEDBG adjust start time %lld -> %lld (duration %d)", eventInfo.presentationTime, eventStartTime, eventInfo.duration);
				}

				//for livestream send the timedMetadata only., because at init, control does not come here
				if(mIsLiveManifest)
				{
					//LLAMA-8251
					// The current process relies on enabling eAAMPConfig_EnableClientDai and that may not be desirable
					// for our requirements. We'll just skip this and use the VOD process to send events
					bool modifySCTEProcessing = ISCONFIGSET(eAAMPConfig_EnableSCTE35PresentationTime);
					if (modifySCTEProcessing)
					{
						aamp->SaveNewTimedMetadata(eventStartTime, eventInfo.name.c_str(), eventInfo.payload.c_str(), eventInfo.payload.size(), prdId.c_str(), eventInfo.duration);
					}
					else
					{
						aamp->FoundEventBreak(prdId, eventStartTime, eventInfo);
					}
				}
				else
				{
					//for vod, send TimedMetadata only when bulkmetadata is not enabled 
					if(reportBulkMeta)
					{
						AAMPLOG_INFO("Saving timedMetadata for VOD %s event for the period, %s", eventInfo.name.c_str(), prdId.c_str());
						aamp->SaveTimedMetadata(eventStartTime, eventInfo.name.c_str() , eventInfo.payload.c_str(), eventInfo.payload.size(), prdId.c_str(), eventInfo.duration);
					}
					else
					{
						aamp->SaveNewTimedMetadata(eventStartTime, eventInfo.name.c_str(), eventInfo.payload.c_str(), eventInfo.payload.size(), prdId.c_str(), eventInfo.duration);
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
 */
void StreamAbstractionAAMP_MPD::ProcessStreamRestrictionList(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
		AAMPLOG_WARN("node is null");  //CID:84081 - Null Returns
	}
}


/**
 * @brief Process stream restriction
 */
void StreamAbstractionAAMP_MPD::ProcessStreamRestriction(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
			AAMPLOG_WARN("child is null");  //CID:84810 - Null Returns
		}
	}
}


/**
 * @brief Process stream restriction extension
 */
void StreamAbstractionAAMP_MPD::ProcessStreamRestrictionExt(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
 */
void StreamAbstractionAAMP_MPD::ProcessTrickModeRestriction(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	double start = startMS / 1000.0f;

	std::string trickMode;
	if (node->HasAttribute("trickMode")) {
		trickMode = node->GetAttributeValue("trickMode");
		if(!trickMode.length())
		{
			AAMPLOG_WARN("trickMode is null");  //CID:86122 - Null Returns
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
	FN_TRACE_F_MPD( __FUNCTION__ );
	class HeaderFetchParams* fetchParms = (class HeaderFetchParams*)arg;
	if(aamp_pthread_setname(pthread_self(), "aampMPDTrackDL"))
	{
		AAMPLOG_WARN("aamp_pthread_setname failed");
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
	FN_TRACE_F_MPD( __FUNCTION__ );
	class FragmentDownloadParams* downloadParams = (class FragmentDownloadParams*) arg;
	if(aamp_pthread_setname(pthread_self(), "aampMPDFragDL"))
	{
		AAMPLOG_WARN(" aamp_pthread_setname failed");
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
					AAMPLOG_INFO(" %s EOS - Exit fetch loop", downloadParams->pMediaStreamContext->name);
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
		AAMPLOG_WARN("NULL adaptationSet");
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
	FN_TRACE_F_MPD( __FUNCTION__ );
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
						AAMPLOG_WARN("skipping schemeUri %s", schemeUri.c_str());
					}
				}
			}
			else
			{
				AAMPLOG_TRACE("skipping name %s", xml->GetName().c_str());
			}
		}
		else
		{
			AAMPLOG_WARN("xml is null");  //CID:81118 - Null Returns
		}
	}
	return false;
}


/**
 * @brief Get the language for an adaptation set
 */
std::string StreamAbstractionAAMP_MPD::GetLanguageForAdaptationSet(IAdaptationSet *adaptationSet)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	std::string lang = adaptationSet->GetLang();
	// If language from adaptation is undefined , retain the current player language
	// Not to change the language .
	
//	if(lang == "und")
//	{
//		lang = aamp->language;
//	}
	if(!lang.empty())
	{
		lang = Getiso639map_NormalizeLanguageCode(lang,aamp->GetLangCodePreference());
	}
	else
	{
		// set und+id as lang, this is required because sometimes lang is not present and stream has multiple audio track.
		// this unique lang string will help app to SetAudioTrack by index.
		// Attempt is made to make lang unique by picking ID of first representation under current adaptation
		IRepresentation* representation = adaptationSet->GetRepresentation().at(0);
		if(NULL != representation)
		{
			lang = "und_" + representation->GetId();
			if( lang.size() > (MAX_LANGUAGE_TAG_LENGTH-1))
			{
				// Lang string len  should not be more than "MAX_LANGUAGE_TAG_LENGTH" so trim from end
				// lang is sent to metadata event where len of char array  is limited to MAX_LANGUAGE_TAG_LENGTH
				lang = lang.substr(0,(MAX_LANGUAGE_TAG_LENGTH-1));
			}
		}
	}
	return lang;
}

/**
 * @brief Get Adaptation Set at given index for the current period
 *
 * @retval Adaptation Set at given Index
 */
const IAdaptationSet* StreamAbstractionAAMP_MPD::GetAdaptationSetAtIndex(int idx)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	assert(idx < mCurrentPeriod->GetAdaptationSets().size());
	return mCurrentPeriod->GetAdaptationSets().at(idx);
}

/**
 * @brief Get Adaptation Set and Representation Index for given profile
 *
 * @retval Adaptation Set and Representation Index pair for given profile
 */
struct ProfileInfo StreamAbstractionAAMP_MPD::GetAdaptationSetAndRepresetationIndicesForProfile(int profileIndex)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	assert(profileIndex < GetProfileCount());
	return mProfileMaps.at(profileIndex);
}

/**
 * @brief Update language list state variables
 */
void StreamAbstractionAAMP_MPD::UpdateLanguageList()
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
					AAMPLOG_WARN("adaptationSet is null");  //CID:86317 - Null Returns
				}
			}
		}
		else
		{
			AAMPLOG_WARN("period is null");  //CID:83754 - Null Returns
		}
	}
	aamp->StoreLanguageList(mLangList);
}

#ifdef AAMP_MPD_DRM
/**
 * @brief Process Early Available License Request
 */
void StreamAbstractionAAMP_MPD::ProcessEAPLicenseRequest()
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	AAMPLOG_TRACE("Processing License request for pending KeyIDs: %d", mPendingKeyIDs.size());
	if(!deferredDRMRequestThreadStarted)
	{
		// wait for thread to complete and create a new thread
		if ((deferredDRMRequestThread!= NULL) && (deferredDRMRequestThread->joinable()))
		{
			deferredDRMRequestThread->join();
			SAFE_DELETE(deferredDRMRequestThread);
		}

		if(NULL == deferredDRMRequestThread)
		{
			AAMPLOG_INFO("New Deferred DRM License thread starting");
			mAbortDeferredLicenseLoop = false;
			deferredDRMRequestThread = new std::thread(&StreamAbstractionAAMP_MPD::StartDeferredDRMRequestThread, this, eMEDIATYPE_VIDEO);
			deferredDRMRequestThreadStarted = true;
		}
	}
	else
	{
		AAMPLOG_TRACE("Diferred License Request Thread already running");
	}
}


/**
 * @brief Start Deferred License Request
 */
void StreamAbstractionAAMP_MPD::StartDeferredDRMRequestThread(MediaType mediaType)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
				AAMPLOG_ERR("aborted");
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
				AAMPLOG_TRACE("sleep over for deferred time:%d", deferTime);
			}

			if((aamp->DownloadsAreEnabled()) && (!mEarlyAvailableKeyIDMap[keyID].isLicenseFailed))
			{
				AAMPLOG_TRACE("Processing License request after sleep");
				// Process content protection with early created helper
				ProcessVssContentProtection(mEarlyAvailableKeyIDMap[keyID].helper, mediaType);
				mEarlyAvailableKeyIDMap[keyID].isLicenseProcessed = true;
			}
			else
			{
				AAMPLOG_ERR("Aborted");
				exitLoop = true;
				break;
			}
			// Remove processed keyID from FIFO queue
			mPendingKeyIDs.pop();
		}
	}
	while(!exitLoop);
	deferredDRMRequestThreadStarted = false;
}
#endif

/**
 * @brief Get the best Audio track by Language, role, and/or codec type
 * @return int selected representation index
 */
int StreamAbstractionAAMP_MPD::GetBestAudioTrackByLanguage( int &desiredRepIdx, AudioType &CodecType, 
std::vector<AudioTrackInfo> &ac4Tracks, std::string &audioTrackIndex)
{
    FN_TRACE_F_MPD( __FUNCTION__ );
	int bestTrack = -1;
	unsigned long long bestScore = 0;
	AudioTrackInfo selectedAudioTrack; /**< Selected Audio track information */
	class MediaStreamContext *pMediaStreamContext = mMediaStreamContext[eMEDIATYPE_AUDIO];
	IPeriod *period = mCurrentPeriod;
	bool isMuxedAudio = false; /** Flag to inidcate muxed audio track , used by AC4 */
	if(!period)
	{
		AAMPLOG_WARN("period is null");
		return bestTrack;
	}
	
	size_t numAdaptationSets = period->GetAdaptationSets().size();
	for( int iAdaptationSet = 0; iAdaptationSet < numAdaptationSets; iAdaptationSet++)
	{
		IAdaptationSet *adaptationSet = period->GetAdaptationSets().at(iAdaptationSet);
		if( IsContentType(adaptationSet, eMEDIATYPE_AUDIO) )
		{
			unsigned long long score = 0;
			std::string trackLabel = adaptationSet->GetLabel();
			std::string trackLanguage = GetLanguageForAdaptationSet(adaptationSet);
			if(trackLanguage.empty())
			{
				isMuxedAudio = true;
				AAMPLOG_INFO("Found Audio Track with no language, look like muxed stream");
			}
			else if( aamp->preferredLanguagesList.size() > 0 )
			{
				auto iter = std::find(aamp->preferredLanguagesList.begin(), aamp->preferredLanguagesList.end(), trackLanguage);
				if(iter != aamp->preferredLanguagesList.end())
				{ // track is in preferred language list
					int distance = std::distance(aamp->preferredLanguagesList.begin(),iter);
					score += ((aamp->preferredLanguagesList.size()-distance))*AAMP_LANGUAGE_SCORE; // big bonus for language match
				}
			}

			if( aamp->preferredLabelList.size() > 0 )
			{
				auto iter = std::find(aamp->preferredLabelList.begin(), aamp->preferredLabelList.end(), trackLabel);
				if(iter != aamp->preferredLabelList.end())
				{ // track is in preferred language list
					int distance = std::distance(aamp->preferredLabelList.begin(),iter);
					score += ((aamp->preferredLabelList.size()-distance))*AAMP_LABEL_SCORE; // big bonus for language match
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
							score += AAMP_ROLE_SCORE;
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
							score += AAMP_TYPE_SCORE;
						}
					}
				}
			}

			Accessibility accessibilityNode = StreamAbstractionAAMP_MPD::getAccessibilityNode((void* )adaptationSet);
			if (accessibilityNode == aamp->preferredAudioAccessibilityNode)
			{
				score += AAMP_SCHEME_ID_SCORE;
			}
			
			AudioType selectedCodecType = eAUDIO_UNKNOWN;
			unsigned int selRepBandwidth = 0;
			bool disableATMOS = ISCONFIGSET(eAAMPConfig_DisableATMOS);
			bool disableEC3 = ISCONFIGSET(eAAMPConfig_DisableEC3);
			bool disableAC4 = ISCONFIGSET(eAAMPConfig_DisableAC4); 
			bool disableAC3 = ISCONFIGSET(eAAMPConfig_DisableAC3); 

			int audioRepresentationIndex = -1;
			bool audioCodecSelected = false;
			unsigned int codecScore = 0;
			bool disabled = false;
			if(!GetPreferredCodecIndex(adaptationSet, audioRepresentationIndex, selectedCodecType, selRepBandwidth, codecScore, disableEC3 , disableATMOS, disableAC4, disableAC3, disabled))
			{
				audioRepresentationIndex = GetDesiredCodecIndex(adaptationSet, selectedCodecType, selRepBandwidth, disableEC3 , disableATMOS, disableAC4, disableAC3, disabled);
				switch( selectedCodecType )
				{
					case eAUDIO_DOLBYAC4:
						if( !disableAC4 )
						{
							score += 10;
						}
						break;

					case eAUDIO_ATMOS:
						if( !disableATMOS )
						{
							score += 8;
						}
						break;
						
					case eAUDIO_DDPLUS:
						if( !disableEC3 )
						{
							score += 6;
						}
						break;

					case eAUDIO_DOLBYAC3:
						if( !disableAC3 )
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
			}
			else
			{
				score += codecScore;
			}

			/**Add score for language , role and type matching from preselection node for AC4 tracks */
			if ((selectedCodecType == eAUDIO_DOLBYAC4) && isMuxedAudio)
			{
				AAMPLOG_INFO("AC4 Track Selected, get the priority based on language and role" );
				unsigned long long ac4CurrentScore = 0;
				unsigned long long ac4SelectedScore = 0;
				
				
				for(auto ac4Track:ac4Tracks)
				{
					if (ac4Track.codec.find("ac-4") != std::string::npos)
					{
						//TODO: Non AC4 codec in preselection node as per Dash spec 
						//https://dashif.org/docs/DASH-IF-IOP-for-ATSC3-0-v1.0.pdf#page=63&zoom=100,92,113
						continue;
					}
					if( aamp->preferredLanguagesList.size() > 0 )
					{
						auto iter = std::find(aamp->preferredLanguagesList.begin(), aamp->preferredLanguagesList.end(), ac4Track.language);
						if(iter != aamp->preferredLanguagesList.end())
						{ // track is in preferred language list
							int distance = std::distance(aamp->preferredLanguagesList.begin(),iter);
							ac4CurrentScore += ((aamp->preferredLanguagesList.size()-distance))*AAMP_LANGUAGE_SCORE; // big bonus for language match
						}
					}
					if( !aamp->preferredRenditionString.empty() )
					{
						if( aamp->preferredRenditionString.compare(ac4Track.rendition)==0 )
						{
							ac4CurrentScore += AAMP_ROLE_SCORE;
						}
					}
					if( !aamp->preferredTypeString.empty() )
					{
						if (aamp->preferredTypeString.compare(ac4Track.contentType)==0 )
						{
							ac4CurrentScore += AAMP_TYPE_SCORE;
						}
					}
					if ((ac4CurrentScore > ac4SelectedScore) || 
					((ac4SelectedScore == ac4CurrentScore ) && (ac4Track.bandwidth > selectedAudioTrack.bandwidth)) /** Same track with best quality */
					)
					{
						selectedAudioTrack = ac4Track;
						audioTrackIndex = ac4Track.index; /**< for future reference **/
						ac4SelectedScore = ac4CurrentScore;
						
					}
				}
				score += ac4SelectedScore;
				//KC
				//PrasePreselection and found language match and add points for language role match type match
			}
			if (disabled || (audioRepresentationIndex < 0))
			{
				/**< No valid representation found in this Adaptation, discard this adaptation */
				score = 0;
			}
			AAMPLOG_INFO( "track#%d::%d -  score = %d", iAdaptationSet, audioRepresentationIndex,  score );
			if( score > bestScore )
			{
				bestScore = score;
				bestTrack = iAdaptationSet;
				desiredRepIdx = audioRepresentationIndex;
				CodecType = selectedCodecType;
			}
		} // IsContentType(adaptationSet, eMEDIATYPE_AUDIO)
	} // next iAdaptationSet
	if (CodecType == eAUDIO_DOLBYAC4)
	{
		/* TODO: Cuurently current Audio track is updating only for AC4 need mechanism to update for other tracks also */
		AAMPLOG_INFO( "Audio Track selected index - %s - language : %s rendition : %s - codec - %s  score = %d", selectedAudioTrack.index.c_str(),
		selectedAudioTrack.language.c_str(), selectedAudioTrack.rendition.c_str(), selectedAudioTrack.codec.c_str(),  bestScore );
	}
	return bestTrack;
}


/**
 * @fn GetBestTextTrackByLanguage
 * 
 * @brief Get the best Text track by Language, role, and schemeId
 * @return int selected representation index
 */
bool StreamAbstractionAAMP_MPD::GetBestTextTrackByLanguage( TextTrackInfo &selectedTextTrack)
{
    FN_TRACE_F_MPD( __FUNCTION__ );
	bool bestTrack = false;
	unsigned long long bestScore = 0;
	class MediaStreamContext *pMediaStreamContext = mMediaStreamContext[eMEDIATYPE_SUBTITLE];
	IPeriod *period = mCurrentPeriod;
	if(!period)
	{
		AAMPLOG_WARN("period is null");
		return bestTrack;
	}
	std::string trackLanguage = "";
	std::string trackRendition = "";
	std::string trackLabel = "";
	std::string trackType = "";
	Accessibility accessibilityNode;
	
	size_t numAdaptationSets = period->GetAdaptationSets().size();
	for( int iAdaptationSet = 0; iAdaptationSet < numAdaptationSets; iAdaptationSet++)
	{
		bool labelFound = false;
		bool accessibilityFound = false;
		IAdaptationSet *adaptationSet = period->GetAdaptationSets().at(iAdaptationSet);
		if( IsContentType(adaptationSet, eMEDIATYPE_SUBTITLE) )
		{
			unsigned long long score = 1; /** Select first track by default **/
			trackLabel = adaptationSet->GetLabel();
			if (!trackLabel.empty())
			{
				labelFound = true;
			}
			trackLanguage = GetLanguageForAdaptationSet(adaptationSet);
			if( aamp->preferredTextLanguagesList.size() > 0 )
			{
				auto iter = std::find(aamp->preferredTextLanguagesList.begin(), aamp->preferredTextLanguagesList.end(), trackLanguage);
				if(iter != aamp->preferredTextLanguagesList.end())
				{ // track is in preferred language list
					int dist = std::distance(aamp->preferredTextLanguagesList.begin(),iter);
					score += (aamp->preferredTextLanguagesList.size()-dist)*AAMP_LANGUAGE_SCORE; // big bonus for language match
				}
			}
			
			if( !aamp->preferredTextRenditionString.empty() )
			{
				std::vector<IDescriptor *> rendition = adaptationSet->GetRole();
				for (unsigned iRendition = 0; iRendition < rendition.size(); iRendition++)
				{
					if (rendition.at(iRendition)->GetSchemeIdUri().find("urn:mpeg:dash:role:2011") != string::npos)
					{
						if (!trackRendition.empty())
						{
							trackRendition =+ ",";
						}
						/**< Save for future use **/
						trackRendition += rendition.at(iRendition)->GetValue();
						/**< If any rendition matched add the score **/
						std::string rend = rendition.at(iRendition)->GetValue();
						if( aamp->preferredTextRenditionString.compare(rend)==0 )
						{
							score += AAMP_ROLE_SCORE;
						}
					}
				}
			}

			if( !aamp->preferredTextTypeString.empty() )
			{
				for( auto iter : adaptationSet->GetAccessibility() )
				{
					if (iter->GetSchemeIdUri().find("urn:mpeg:dash:role:2011") != string::npos)
					{
						if (!trackType.empty())
						{
							trackType += ",";
						}
						trackType += iter->GetValue();
						/**< If any type matched add the score **/
						std::string type = iter->GetValue();
						if (aamp->preferredTextTypeString.compare(type)==0 )
						{
							score += AAMP_TYPE_SCORE;
						}
					}
				}
			}

			accessibilityNode = StreamAbstractionAAMP_MPD::getAccessibilityNode((void *)adaptationSet);
			if (accessibilityNode == aamp->preferredTextAccessibilityNode)
			{
				if (!accessibilityNode.getSchemeId().empty())
				{
					accessibilityFound = true;					
				}
				score += AAMP_SCHEME_ID_SCORE;
			}
			
			uint32_t selRepBandwidth = 0;
			int textRepresentationIndex = -1;
			std::string name; 
			std::string codec;
			std::string empty;
			std::string type;
			if (accessibilityFound)
			{
				type = "captions";
			}
			else if (labelFound)
			{
				type = "subtitle_" + trackLabel;
			}
			else
			{
				type = "subtitle";
			}

			GetPreferredTextRepresentation(adaptationSet, textRepresentationIndex, selRepBandwidth, score, name, codec);
			AAMPLOG_INFO( "track#%d::%d - trackLanguage = %s bandwidth = %d score = %llu currentBestScore = %llu ", iAdaptationSet, textRepresentationIndex, trackLanguage.c_str(),
			selRepBandwidth, score, bestScore);
			if( score > bestScore )
			{
				bestTrack = true;
				bestScore = score;
				std::string index =  std::to_string(iAdaptationSet) + "-" + std::to_string(textRepresentationIndex); //KC
				selectedTextTrack.set(index, trackLanguage, false, trackRendition, name, codec, empty, trackType, trackLabel, type, accessibilityNode);
			}
		} // IsContentType(adaptationSet, eMEDIATYPE_SUBTITLE)
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
		AAMPLOG_INFO("sending init %.3f", seekPosition);
		subtitle->mSubtitleParser->init(seekPosition, 0);
		subtitle->mSubtitleParser->mute(aamp->subtitles_muted);
		subtitle->mSubtitleParser->isLinear(mIsLiveStream);
	}
}

void StreamAbstractionAAMP_MPD::PauseSubtitleParser(bool pause)
{
	struct MediaStreamContext *subtitle = mMediaStreamContext[eMEDIATYPE_SUBTITLE];
	if (subtitle && subtitle->enabled && subtitle->mSubtitleParser)
	{
		AAMPLOG_INFO("setting subtitles pause state = %d", pause);
		subtitle->mSubtitleParser->pause(pause);
	}
}

/** Static function Do not use aamp log function **/
Accessibility StreamAbstractionAAMP_MPD::getAccessibilityNode(AampJsonObject &accessNode)
{
	std::string strSchemeId;
	std::string strValue;
	int intValue = -1;
	Accessibility accessibilityNode;
	if (accessNode.get(std::string("scheme"), strSchemeId))
	{
		if (accessNode.get(std::string("string_value"), strValue))
		{
			accessibilityNode.setAccessibilityData(strSchemeId, strValue);
		}
		else if(accessNode.get(std::string("int_value"), intValue))
		{
			accessibilityNode.setAccessibilityData(strSchemeId, intValue);
		}
	}
	return accessibilityNode;
}

/** Static function Do not use aamp log function **/
Accessibility StreamAbstractionAAMP_MPD::getAccessibilityNode(void* adaptationSetShadow)
{
	IAdaptationSet *adaptationSet = (IAdaptationSet*) adaptationSetShadow;
	Accessibility accessibilityNode;
	if (adaptationSet)
	{
		for( auto iter : adaptationSet->GetAccessibility() )
		{
			std::string schemeId  = iter->GetSchemeIdUri();
			std::string strValue = iter->GetValue();
			accessibilityNode.setAccessibilityData(schemeId, strValue);
			if (!accessibilityNode.getSchemeId().empty())
			{
				break; /**< Only one valid accessibility node is processing, so break here */
			}
		}
	}
	return accessibilityNode;
}

/**
 * @fn ParseTrackInformation
 * 
 * @brief get the Label value from adaptation in Dash
 * @return void
 */
void StreamAbstractionAAMP_MPD::ParseTrackInformation(IAdaptationSet *adaptationSet, uint32_t iAdaptationIndex, MediaType media, std::vector<AudioTrackInfo> &aTracks, std::vector<TextTrackInfo> &tTracks)
{
	if(adaptationSet)
	{
		bool labelFound = false;
		bool schemeIdFound = false;
		std::string type = "";
		/** Populate Common for all tracks */

		Accessibility accessibilityNode = StreamAbstractionAAMP_MPD::getAccessibilityNode((void*)adaptationSet);
		if (!accessibilityNode.getSchemeId().empty())
		{
			schemeIdFound = true;
		}
		
		/**< Audio or Subtitle representations **/
		if (eMEDIATYPE_AUDIO == media || eMEDIATYPE_SUBTITLE == media)
		{
			std::string lang = GetLanguageForAdaptationSet(adaptationSet);
			const std::vector<IRepresentation *> representation = adaptationSet->GetRepresentation();
			std::string codec;
			std::string group; // value of <Role>, empty if <Role> not present
			std::string accessibilityType; // value of <Accessibility> //KC
			std::string empty;
			std::string label = adaptationSet->GetLabel();
			if(!label.empty())
			{
				labelFound = true;
			}
			if (eMEDIATYPE_AUDIO == media)
			{
				if (schemeIdFound)
				{
					type = "audio_" + accessibilityNode.getStrValue();
				}
				else if (labelFound)
				{
					type = "audio_" + label;
				}
				else
				{
					type = "audio";
				}
			}
			else if (eMEDIATYPE_SUBTITLE == media)
			{
				if (schemeIdFound)
				{
					type = "captions";
				}
				else if (labelFound)
				{
					type = "subtitle_" + label;
				}
				else
				{
					type = "subtitle";
				}
			}

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
			for( auto iter : adaptationSet->GetAccessibility() )
			{
				if (iter->GetSchemeIdUri().find("urn:mpeg:dash:role:2011") != string::npos)
				{
					if (!accessibilityType.empty())
					{
						accessibilityType += ",";
					}
					accessibilityType += iter->GetValue();
				}
			}
			// check for codec defined in Adaptation Set
			const std::vector<string> adapCodecs = adaptationSet->GetCodecs();
			for (int representationIndex = 0; representationIndex < representation.size(); representationIndex++)
			{
				std::string index = std::to_string(iAdaptationIndex) + "-" + std::to_string(representationIndex);
				const dash::mpd::IRepresentation *rep = representation.at(representationIndex);
				std::string name = rep->GetId();
				long bandwidth = rep->GetBandwidth();
				const std::vector<std::string> repCodecs = rep->GetCodecs();
				bool isAvailable = !IsEmptyAdaptation(adaptationSet, mIsFogTSB);
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
					PeriodElement periodElement(adaptationSet, rep);
					codec = periodElement.GetMimeType();
				}

				if (eMEDIATYPE_AUDIO == media)
				{
					if ((codec.find("ac-4") != std::string::npos) && lang.empty())
					{
						/* Incase of AC4 muxed audio track, track information is already provided by preselection node*/
						AAMPLOG_WARN("AC4 muxed audio track detected.. Skipping..");
					}
					else
					{
						AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Audio Track - lang:%s, group:%s, name:%s, codec:%s, bandwidth:%d, AccessibilityType:%s label:%s type:%s availability:%d",
						lang.c_str(), group.c_str(), name.c_str(), codec.c_str(), bandwidth, accessibilityType.c_str(), label.c_str(), type.c_str(), isAvailable);
						aTracks.push_back(AudioTrackInfo(index, lang, group, name, codec, bandwidth, accessibilityType, false, label, type, accessibilityNode, isAvailable));
					}
				}
				else
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Text Track - lang:%s, isCC:0, group:%s, name:%s, codec:%s, accessibilityType:%s label:%s type:%s availability:%d",
						lang.c_str(), group.c_str(), name.c_str(), codec.c_str(), accessibilityType.c_str(), label.c_str(), type.c_str(), isAvailable);
					tTracks.push_back(TextTrackInfo(index, lang, false, group, name, codec, empty, accessibilityType, label, type, accessibilityNode, isAvailable));
				}
			}
		}
		// Look in VIDEO adaptation for inband CC track related info
		else if (eMEDIATYPE_VIDEO == media)
		{
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
							AAMPLOG_WARN("StreamAbstractionAAMP_MPD: CC Track - lang:%s, isCC:1, group:%s, id:%s",
								lang.c_str(), schemeId.c_str(), id.c_str());
							tTracks.push_back(TextTrackInfo(empty, lang, true, schemeId, empty, id, empty, empty, empty, empty, accessibilityNode, true));
							value = value.substr(delim + 1);
							delim = value.find(';');
						}
						ParseCCStreamIDAndLang(value, id, lang);
						lang = Getiso639map_NormalizeLanguageCode(lang,aamp->GetLangCodePreference());
						AAMPLOG_WARN("StreamAbstractionAAMP_MPD: CC Track - lang:%s, isCC:1, group:%s, id:%s",
							lang.c_str(), schemeId.c_str(), id.c_str());
						tTracks.push_back(TextTrackInfo(empty, lang, true, schemeId, empty, id, empty, empty, empty, empty, accessibilityNode, true));
					}
					else
					{
						// value = empty is highly discouraged as per spec, just added as fallback
						AAMPLOG_WARN("StreamAbstractionAAMP_MPD: CC Track - group:%s, isCC:1", schemeId.c_str());
						tTracks.push_back(TextTrackInfo(empty, empty, true, schemeId, empty, empty, empty, empty, empty, empty, accessibilityNode, true));
					}
				}
			}
		}
	}//NuULL Check
}

/**
 * @brief Does stream selection
 */
void StreamAbstractionAAMP_MPD::StreamSelection( bool newTune, bool forceSpeedsChangedEvent)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	std::vector<AudioTrackInfo> aTracks;
	std::vector<TextTrackInfo> tTracks;
	TextTrackInfo selectedTextTrack;
	TextTrackInfo preferredTextTrack;
	std::string aTrackIdx;
	std::string tTrackIdx;
	mNumberOfTracks = 0;
	IPeriod *period = mCurrentPeriod;
	if(!period)
	{
		AAMPLOG_WARN("period is null");  //CID:84742 - Null Returns
		return;
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
	int textAdaptationSetIndex = -1;
	int textRepresentationIndex = -1;

	bool disableAC4 = ISCONFIGSET(eAAMPConfig_DisableAC4); 

	if (rate == AAMP_NORMAL_PLAY_RATE)
	{
		/** Time being preselection based track selection only did for ac4 tracks **/
		if (!disableAC4) 
		{
			std::vector<AudioTrackInfo> ac4Tracks;
			ParseAvailablePreselections(period, ac4Tracks);
			aTracks.insert(ac4Tracks.end(), ac4Tracks.begin(), ac4Tracks.end());
		}
		audioAdaptationSetIndex = GetBestAudioTrackByLanguage(desiredRepIdx,selectedCodecType, aTracks, aTrackIdx);
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
			AAMPLOG_WARN("StreamAbstractionAAMP_MPD: lang[%s] AudioType[%d]", lang.c_str(), selectedCodecType);
		}
		else
		{
			AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Unable to get audioAdaptationSet.");
		}

		GetBestTextTrackByLanguage(preferredTextTrack);
	}

	for (int i = 0; i < mMaxTracks; i++)
	{
		class MediaStreamContext *pMediaStreamContext = mMediaStreamContext[i];
		size_t numAdaptationSets = period->GetAdaptationSets().size();
		int  selAdaptationSetIndex = -1;
		int selRepresentationIndex = -1;
		bool isIframeAdaptationAvailable = false;
		bool encryptedIframeTrackPresent = false;
		bool isFrstAvailableTxtTrackSelected = false;
		int videoRepresentationIdx;   //CID:118900 - selRepBandwidth variable locally declared but not reflected
		for (unsigned iAdaptationSet = 0; iAdaptationSet < numAdaptationSets; iAdaptationSet++)
		{
			IAdaptationSet *adaptationSet = period->GetAdaptationSets().at(iAdaptationSet);	
			AAMPLOG_TRACE("StreamAbstractionAAMP_MPD: Content type [%s] AdapSet [%d] ",
					adaptationSet->GetContentType().c_str(), iAdaptationSet);
			if (IsContentType(adaptationSet, (MediaType)i ))
			{
				ParseTrackInformation(adaptationSet, iAdaptationSet, (MediaType)i,  aTracks, tTracks);
				if (AAMP_NORMAL_PLAY_RATE == rate)
				{
					//if isFrstAvailableTxtTrackSelected is true, we should look for the best option (aamp->mSubLanguage) among all the tracks
					if (eMEDIATYPE_SUBTITLE == i && (selAdaptationSetIndex == -1 || isFrstAvailableTxtTrackSelected))
					{
						AAMPLOG_INFO("Checking subs - mime %s lang %s selAdaptationSetIndex %d",
										adaptationSet->GetMimeType().c_str(), GetLanguageForAdaptationSet(adaptationSet).c_str(), selAdaptationSetIndex);
						
						TextTrackInfo *firstAvailTextTrack = nullptr;
						if(aamp->GetPreferredTextTrack().index.empty() && !isFrstAvailableTxtTrackSelected)
						{
							//If no subtitles are selected, opt for the first subtitle as default, and
							for(int j =0 ; j < tTracks.size(); j++)
							{
								if(!tTracks[j].isCC)
								{
									if(nullptr == firstAvailTextTrack)
									{
										firstAvailTextTrack = &tTracks[j];
									}
								}
							}
						}
						if(nullptr != firstAvailTextTrack)
						{
							isFrstAvailableTxtTrackSelected = true;
							AAMPLOG_INFO("Selected first subtitle track, lang:%s, index:%s",
								firstAvailTextTrack->language.c_str(), firstAvailTextTrack->index.c_str());
						}
						if (!preferredTextTrack.index.empty())
						{
							/**< Available preferred**/
							selectedTextTrack = preferredTextTrack;
						}else
						{
							// TTML selection as follows:
							// 1. Text track as set from SetTextTrack API (this is confusingly named preferredTextTrack, even though it's explicitly set)
							// 2. The *actual* preferred text track, as set through the SetPreferredSubtitleLanguage API
							// 3. First text track and keep it
							// 3. Not set
							selectedTextTrack = (nullptr != firstAvailTextTrack) ? *firstAvailTextTrack : aamp->GetPreferredTextTrack();
						}
						

						if (!selectedTextTrack.index.empty())
						{
							if (IsMatchingLanguageAndMimeType((MediaType)i, selectedTextTrack.language, adaptationSet, selRepresentationIndex))
							{
								auto adapSetName = (adaptationSet->GetRepresentation().at(selRepresentationIndex))->GetId();
								AAMPLOG_INFO("adapSet Id %s selName %s", adapSetName.c_str(), selectedTextTrack.name.c_str());
								if (adapSetName.empty() || adapSetName == selectedTextTrack.name)
								{
									selAdaptationSetIndex = iAdaptationSet;
								}
							}
						}
						else if (IsMatchingLanguageAndMimeType((MediaType)i, aamp->mSubLanguage, adaptationSet, selRepresentationIndex) == true)
						{
							AAMPLOG_INFO("matched default sub language %s [%d]", aamp->mSubLanguage.c_str(), iAdaptationSet);
							selAdaptationSetIndex = iAdaptationSet;
						}

					}
					else if (eMEDIATYPE_AUX_AUDIO == i && aamp->IsAuxiliaryAudioEnabled())
					{
						if (aamp->GetAuxiliaryAudioLanguage() == aamp->mAudioTuple.language)
						{
							AAMPLOG_WARN("PrivateStreamAbstractionMPD: auxiliary audio same as primary audio, set forward audio flag");
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
						AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Got TrickMode track");
						pMediaStreamContext->enabled = true;
						pMediaStreamContext->profileChanged = true;
						pMediaStreamContext->adaptationSetIdx = iAdaptationSet;
						mNumberOfTracks = 1;
						isIframeAdaptationAvailable = true;
						if(!GetContentProtection(adaptationSet,(MediaType)i).empty())
						{
							encryptedIframeTrackPresent = true;
							AAMPLOG_WARN("PrivateStreamAbstractionMPD: Detected encrypted iframe track");
						}
						break;
					}
				}
			}
		} // next iAdaptationSet
		
		if (eMEDIATYPE_SUBTITLE == i && selAdaptationSetIndex != -1)
		{
			AAMPLOG_WARN("SDW config set %d", ISCONFIGSET(eAAMPConfig_GstSubtecEnabled));
			if(!ISCONFIGSET(eAAMPConfig_GstSubtecEnabled))
			{
				const IAdaptationSet *pAdaptationSet = period->GetAdaptationSets().at(selAdaptationSetIndex);
				PeriodElement periodElement(pAdaptationSet, pAdaptationSet->GetRepresentation().at(selRepresentationIndex));
				std::string mimeType = periodElement.GetMimeType();
				if (mimeType.empty())
				{
					if( !pMediaStreamContext->mSubtitleParser )
					{
						AAMPLOG_WARN("mSubtitleParser is NULL" );
					}
					else if (pMediaStreamContext->mSubtitleParser->init(0.0, 0))
					{
						pMediaStreamContext->mSubtitleParser->mute(aamp->subtitles_muted);
					}
					else
					{
						pMediaStreamContext->mSubtitleParser.reset(nullptr);
						pMediaStreamContext->mSubtitleParser = NULL;
					}
				}
				pMediaStreamContext->mSubtitleParser = SubtecFactory::createSubtitleParser(mLogObj, aamp, mimeType);
				if (!pMediaStreamContext->mSubtitleParser)
				{
					pMediaStreamContext->enabled = false;
					selAdaptationSetIndex = -1;
				}
			}

			if (-1 != selAdaptationSetIndex)
				tTrackIdx = std::to_string(selAdaptationSetIndex) + "-" + std::to_string(selRepresentationIndex);

			aamp->StopTrackDownloads(eMEDIATYPE_SUBTITLE);
		}

		if ((eAUDIO_UNKNOWN == mAudioType) && (AAMP_NORMAL_PLAY_RATE == rate) && (eMEDIATYPE_VIDEO != i) && selAdaptationSetIndex >= 0)
		{
			AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Selected Audio Track codec is unknown");
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
				AAMPLOG_WARN("StreamAbstractionAAMP_MPD: AudioType Changed %d -> %d",
						aamp->previousAudioType, selectedCodecType);
				aamp->previousAudioType = selectedCodecType;
				SetESChangeStatus();
			}
			AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Media[%s] Adaptation set[%d] RepIdx[%d] TrackCnt[%d]",
				getMediaTypeName(MediaType(i)),selAdaptationSetIndex,selRepresentationIndex,(mNumberOfTracks+1) );

			ProcessContentProtection(period->GetAdaptationSets().at(selAdaptationSetIndex),(MediaType)i);
			mNumberOfTracks++;
		}
		else if (encryptedIframeTrackPresent) //Process content protection for encyrpted Iframe
		{
			ProcessContentProtection(period->GetAdaptationSets().at(pMediaStreamContext->adaptationSetIdx),(MediaType)i);
		}

		if(selAdaptationSetIndex < 0 && rate == 1)
		{
			AAMPLOG_WARN("StreamAbstractionAAMP_MPD: No valid adaptation set found for Media[%s]",
				getMediaTypeName(MediaType(i)));
		}

		AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Media[%s] %s",
			getMediaTypeName(MediaType(i)), pMediaStreamContext->enabled?"enabled":"disabled");

		//RDK-27796, we need this hack for cases where subtitle is not enabled, but auxiliary audio track is enabled
		if (eMEDIATYPE_AUX_AUDIO == i && pMediaStreamContext->enabled && !mMediaStreamContext[eMEDIATYPE_SUBTITLE]->enabled)
		{
			AAMPLOG_WARN("PrivateStreamAbstractionMPD: Auxiliary enabled, but subtitle disabled, swap MediaStreamContext of both");
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
			AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Audio only period");
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

	/**< for Traiging Log selected track informations **/
	printSelectedTrack(aTrackIdx, eMEDIATYPE_AUDIO);
	printSelectedTrack(tTrackIdx, eMEDIATYPE_SUBTITLE);

}

/**
 * @brief Extract bitrate info from custom mpd
 * @note Caller function should delete the vector elements after use.
 * @param adaptationSet : AdaptaionSet from which bitrate info is to be extracted
 * @param[out] representations : Representation vector gets updated with Available bit rates.
 */
static void GetBitrateInfoFromCustomMpd( const IAdaptationSet *adaptationSet, std::vector<Representation *>& representations )
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
						AAMPLOG_WARN("reprNode  is null");  //CID:85171 - Null Returns
					}
				}
				break;
			}
		}
		else
		{
			AAMPLOG_WARN("node is null");  //CID:81731 - Null Returns
		}
	}
}

/**
 * @brief Get profile index for bandwidth notification
 * @retval profile index of the current bandwidth
 */
int StreamAbstractionAAMP_MPD::GetProfileIdxForBandwidthNotification(uint32_t bandwidth)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
 * @brief GetCurrentMimeType
 * @param MediaType type of media
 * @retval mimeType
 */
std::string StreamAbstractionAAMP_MPD::GetCurrentMimeType(MediaType mediaType)
{
	if (mediaType >= mNumberOfTracks) return std::string{};

	class MediaStreamContext *pMediaStreamContext = mMediaStreamContext[mediaType];
	auto adaptationSet = pMediaStreamContext->adaptationSet;

	std::string mimeType = adaptationSet->GetMimeType();
	AAMPLOG_TRACE("mimeType is %s", mimeType.c_str());

	return mimeType;
}

/**
 * @brief Updates track information based on current state
 */
AAMPStatusType StreamAbstractionAAMP_MPD::UpdateTrackInfo(bool modifyDefaultBW, bool periodChanged, bool resetTimeLineIndex)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	AAMPStatusType ret = eAAMPSTATUS_OK;
	long defaultBitrate = aamp->GetDefaultBitrate();
	long iframeBitrate = aamp->GetIframeBitrate();
	bool isFogTsb = mIsFogTSB && !mAdPlayingFromCDN;	/*Conveys whether the current playback from FOG or not.*/
	long minBitrate = aamp->GetMinimumBitrate();
	long maxBitrate = aamp->GetMaximumBitrate();
	mProfileCount = 0;
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
			AAMPLOG_WARN("pMediaStreamContext  is null");  //CID:82464,84186 - Null Returns
			return eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
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
						SAFE_DELETE_ARRAY(mStreamInfo);
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
							mStreamInfo[idx].validity = false;
							//Get video codec details
							if (representation->GetCodecs().size())
							{
								mStreamInfo[idx].codecs = representation->GetCodecs().at(0).c_str();
							}
							else if (pMediaStreamContext->adaptationSet->GetCodecs().size())
							{
								mStreamInfo[idx].codecs = pMediaStreamContext->adaptationSet->GetCodecs().at(0).c_str();
							}
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

							SAFE_DELETE(representation);
							// Skip profile by resolution, if profile capping already applied in Fog
							if(ISCONFIGSET(eAAMPConfig_LimitResolution) && aamp->mProfileCappedStatus &&  aamp->mDisplayWidth > 0 && mStreamInfo[idx].resolution.width > aamp->mDisplayWidth)
							{
								AAMPLOG_INFO ("Video Profile Ignoring resolution=%d:%d display=%d:%d Bw=%ld", mStreamInfo[idx].resolution.width, mStreamInfo[idx].resolution.height, aamp->mDisplayWidth, aamp->mDisplayHeight, mStreamInfo[idx].bandwidthBitsPerSecond);
								continue;
							}
							AAMPLOG_INFO("Added Video Profile to ABR BW=%ld to bitrate vector index:%d", mStreamInfo[idx].bandwidthBitsPerSecond, idx);
							mBitrateIndexVector.push_back(mStreamInfo[idx].bandwidthBitsPerSecond);
							if(mStreamInfo[idx].bandwidthBitsPerSecond > mMaxTSBBandwidth)
							{
								mMaxTSBBandwidth = mStreamInfo[idx].bandwidthBitsPerSecond;
								mTsbMaxBitrateProfileIndex = idx;
							}
						}
						else
						{
							AAMPLOG_WARN("representation  is null");  //CID:82489 - Null Returns
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
						SAFE_DELETE_ARRAY(mStreamInfo);

						// reset representationIndex to -1 to allow updating the currentProfileIndex for period change.
						pMediaStreamContext->representationIndex = -1;
						AAMPLOG_WARN("representationIndex set to (-1) to find currentProfileIndex");
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
								// Get codec details for video profile
								if (representation->GetCodecs().size())
								{
									mStreamInfo[idx].codecs = representation->GetCodecs().at(0).c_str();
								}
								else if (adaptationSet->GetCodecs().size())
								{
									mStreamInfo[idx].codecs = adaptationSet->GetCodecs().at(0).c_str();
								}

								mStreamInfo[idx].enabled = false;
								mStreamInfo[idx].validity = false;
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
					// To select profiles bitrates nearer with user configured bitrate list
					if (aamp->userProfileStatus)
					{
						for (int i = 0; i < aamp->bitrateList.size(); i++)
						{
							int curIdx = 0;
							long curValue, diff;
							// traverse all profiles and select closer to user bitrate
							for (int pidx = 0; pidx < idx; pidx++)
							{
								diff = abs(mStreamInfo[pidx].bandwidthBitsPerSecond - aamp->bitrateList.at(i));
								if ((0 == pidx) || (diff < curValue))
								{
									curValue = diff;
									curIdx = pidx;
								}
							}

							mStreamInfo[curIdx].validity = true;
						}
					}
					mProfileCount = idx;
					for (int pidx = 0; pidx < idx; pidx++)
					{
						if (false == aamp->userProfileStatus && resolutionCheckEnabled && (mStreamInfo[pidx].resolution.width > aamp->mDisplayWidth) &&
							((mStreamInfo[pidx].isIframeTrack && bIframeCapped) || 
							(!mStreamInfo[pidx].isIframeTrack && bVideoCapped)))
						{
							AAMPLOG_INFO("Video Profile ignoring for resolution= %d:%d display= %d:%d BW=%ld", mStreamInfo[pidx].resolution.width, mStreamInfo[pidx].resolution.height, aamp->mDisplayWidth, aamp->mDisplayHeight, mStreamInfo[pidx].bandwidthBitsPerSecond);
						}
						else
						{
							if (aamp->userProfileStatus || ((mStreamInfo[pidx].bandwidthBitsPerSecond > minBitrate) && (mStreamInfo[pidx].bandwidthBitsPerSecond < maxBitrate)))
							{
								if (aamp->userProfileStatus && false ==  mStreamInfo[pidx].validity)
								{
									AAMPLOG_INFO("Video Profile ignoring user profile range BW=%ld", mStreamInfo[pidx].bandwidthBitsPerSecond);
									continue;
								}
								else if (false == aamp->userProfileStatus && ISCONFIGSET(eAAMPConfig_Disable4K) &&
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
								AAMPLOG_INFO("Added Video Profile to ABR BW= %ld res= %d:%d display= %d:%d pc:%d", mStreamInfo[pidx].bandwidthBitsPerSecond, mStreamInfo[pidx].resolution.width, mStreamInfo[pidx].resolution.height, aamp->mDisplayWidth, aamp->mDisplayHeight, aamp->mProfileCappedStatus);
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
						AAMPLOG_WARN("No profiles found, minBitrate : %ld maxBitrate: %ld", minBitrate, maxBitrate);
						return ret;
					}
					if (modifyDefaultBW)
					{	// Not for new tune ( for Seek / Trickplay)
						long persistedBandwidth = aamp->GetPersistedBandwidth();
						// XIONE-2039 If Bitrate persisted over trickplay is true, set persisted BW as default init BW
						if (persistedBandwidth > 0 && (persistedBandwidth < defaultBitrate || aamp->IsBitRatePersistedOverSeek()))
						{
							defaultBitrate = persistedBandwidth;
						}
					}
					else
					{
						// For NewTune
						// Set Default init bitrate according to last PersistBandwidth
						if((ISCONFIGSET(eAAMPConfig_PersistLowNetworkBandwidth)|| ISCONFIGSET(eAAMPConfig_PersistHighNetworkBandwidth)) && !aamp->IsTSBSupported())
						{
							long persistbandwidth = aamp->mhAbrManager.getPersistBandwidth();
							long TimeGap   =  aamp_GetCurrentTimeMS() - ABRManager::mPersistBandwidthUpdatedTime;
							//If current Network bandwidth is lower than current default bitrate ,use persistbw as default bandwidth when peristLowNetworkConfig exist
							if(ISCONFIGSET(eAAMPConfig_PersistLowNetworkBandwidth) && TimeGap < 10000 &&  persistbandwidth < aamp->GetDefaultBitrate() && persistbandwidth > 0)
							{
								AAMPLOG_WARN("PersistBitrate used as defaultBitrate. PersistBandwidth : %ld TimeGap : %ld",persistbandwidth,TimeGap);
								aamp->mhAbrManager.setDefaultInitBitrate(persistbandwidth);
							}
							//If current Network bandwidth is higher than current default bitrate and if config for PersistHighBandwidth is true , then network bandwidth will be applied as default bitrate for tune 
							else if(ISCONFIGSET(eAAMPConfig_PersistHighNetworkBandwidth) && TimeGap < 10000 && persistbandwidth > 0)
							{
								AAMPLOG_WARN("PersistBitrate used as defaultBitrate. PersistBandwidth : %ld TimeGap : %ld",persistbandwidth,TimeGap);
								aamp->mhAbrManager.setDefaultInitBitrate(persistbandwidth);
							}
							// Set default init bitrate 
							else
							{
								AAMPLOG_WARN("Using defaultBitrate %ld . PersistBandwidth : %ld TimeGap : %ld",aamp->GetDefaultBitrate(),persistbandwidth,TimeGap);
								aamp->mhAbrManager.setDefaultInitBitrate(aamp->GetDefaultBitrate());

							}
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
					AAMPLOG_WARN("[WARN] !! representationIndex is '-1' override with '0' since Custom MPD has single representation");
					pMediaStreamContext->representationIndex = 0; //Fog custom mpd has single representation
				}
			}
			pMediaStreamContext->representation = pMediaStreamContext->adaptationSet->GetRepresentation().at(pMediaStreamContext->representationIndex);

			pMediaStreamContext->fragmentDescriptor.ClearMatchingBaseUrl();
			pMediaStreamContext->fragmentDescriptor.AppendMatchingBaseUrl( &mpd->GetBaseUrls() );
			pMediaStreamContext->fragmentDescriptor.AppendMatchingBaseUrl( &period->GetBaseURLs() );
			pMediaStreamContext->fragmentDescriptor.AppendMatchingBaseUrl( &pMediaStreamContext->adaptationSet->GetBaseURLs() );
			pMediaStreamContext->fragmentDescriptor.AppendMatchingBaseUrl( &pMediaStreamContext->representation->GetBaseURLs() );

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
				aamp->mNextPeriodDuration = mPeriodDuration;
				aamp->mNextPeriodStartTime = mPeriodStartTime;
			}

			SegmentTemplates segmentTemplates(pMediaStreamContext->representation->GetSegmentTemplate(),pMediaStreamContext->adaptationSet->GetSegmentTemplate());
			if( segmentTemplates.HasSegmentTemplate())
			{
				// In SegmentTemplate case, configure mFirstPTS as per PTO
				// mFirstPTS is used during Flush() for configuring gst_element_seek start position
				const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
				long int startNumber = segmentTemplates.GetStartNumber();
				if(NULL == segmentTimeline)
				{
                                        uint32_t timeScale = segmentTemplates.GetTimescale();
                                        uint32_t duration = segmentTemplates.GetDuration();
                                        double fragmentDuration =  ComputeFragmentDuration(duration,timeScale);

                                        if( timeScale )
                                        {
                                                pMediaStreamContext->scaledPTO = (double)segmentTemplates.GetPresentationTimeOffset() / (double)timeScale;
                                        }
					if(!aamp->IsLive())
					{
						if(segmentTemplates.GetPresentationTimeOffset())
						{
							uint64_t ptoFragmenNumber = 0;
							ptoFragmenNumber = (pMediaStreamContext->scaledPTO / fragmentDuration) + 1;
							AAMPLOG_TRACE("StreamAbstractionAAMP_MPD: Track %d startnumber:%ld PTO specific fragment Number : %lld", i,startNumber, ptoFragmenNumber);
							if(ptoFragmenNumber > startNumber)
							{
								startNumber = ptoFragmenNumber;
							}
						}

						if(i == eMEDIATYPE_VIDEO)
						{
							mFirstPTS = pMediaStreamContext->scaledPTO;

							if(periodChanged)
							{
								aamp->mNextPeriodScaledPtoStartTime = pMediaStreamContext->scaledPTO;
								AAMPLOG_TRACE("StreamAbstractionAAMP_MPD: Track %d Set mNextPeriodScaledPtoStartTime:%lf",i,aamp->mNextPeriodScaledPtoStartTime);
							}
							AAMPLOG_TRACE("StreamAbstractionAAMP_MPD: Track %d Set mFirstPTS:%lf",i,mFirstPTS);
							AAMPLOG_TRACE("PTO= %lld tScale= %ld", segmentTemplates.GetPresentationTimeOffset(), timeScale );
						}
					}
				}
				pMediaStreamContext->fragmentDescriptor.Number = startNumber;
				AAMPLOG_INFO("StreamAbstractionAAMP_MPD: Track %d timeLineIndex %d fragmentDescriptor.Number %lld mFirstPTS:%lf", i, pMediaStreamContext->timeLineIndex, pMediaStreamContext->fragmentDescriptor.Number, mFirstPTS);
			}
		}
	}
	return ret;
}

/**
 *   @brief  Get timescale from current period
 *   @retval timescale
 */
uint32_t StreamAbstractionAAMP_MPD::GetCurrPeriodTimeScale()
{
	IPeriod *currPeriod = mCurrentPeriod;
	if(!currPeriod)
	{
		AAMPLOG_WARN("currPeriod is null");  //CID:80891 - Null Returns
		return 0;
	}

	uint32_t timeScale = 0;
	timeScale = GetPeriodSegmentTimeScale(currPeriod);
	return timeScale;
}

dash::mpd::IMPD *StreamAbstractionAAMP_MPD::GetMPD( void )
{
	return mpd;
}

IPeriod *StreamAbstractionAAMP_MPD::GetPeriod( void )
{
	return mpd->GetPeriods().at(mCurrentPeriodIdx);
}

/**
 * @brief Update culling state for live manifests
 */
double StreamAbstractionAAMP_MPD::GetCulledSeconds()
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	double newStartTimeSeconds = 0;
	double culled = 0;
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
							AAMPLOG_INFO("PeriodId %s, prevStart %" PRIu64 " currStart %" PRIu64 " culled %f",
												prevPeriodInfo.periodId.c_str(), prevPeriodInfo.startTime, currFirstPeriodInfo.startTime, culled);
						}
						break;
					}
					else
					{
						culled += prevPeriodInfo.duration;
						iter1++;
						AAMPLOG_WARN("PeriodId %s , with last known duration %f seems to have got culled",
										prevPeriodInfo.periodId.c_str(), prevPeriodInfo.duration);
					}
				}
				aamp->mMPDPeriodsInfo = currMPDPeriodDetails;
			}
			else
			{
				AAMPLOG_INFO("StreamAbstractionAAMP_MPD: NULL segmentTimeline. Hence modifying culling logic based on MPD availabilityStartTime, periodStartTime, fragment number and current time");
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
					if(segmentTemplates.GetTimescale() != 0)
					{
						double fragmentDuration = ((double)segmentTemplates.GetDuration()) / segmentTemplates.GetTimescale();
						if (newStartSegment && mPrevStartTimeSeconds)
						{
							culled = (newStartSegment - mPrevStartTimeSeconds) * fragmentDuration;
							AAMPLOG_TRACE("StreamAbstractionAAMP_MPD:: post-refresh %fs before %f (%f)",  newStartTimeSeconds, mPrevStartTimeSeconds, culled);
						}
						else
						{
							AAMPLOG_WARN("StreamAbstractionAAMP_MPD: newStartTimeSeconds %f mPrevStartTimeSeconds %F", newStartSegment, mPrevStartTimeSeconds);
						}
					}  //CID:163916 - divide by zero
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
						if(adaptationSet == NULL)
						{
							AAMPLOG_WARN("adaptationSet is null");  //CID:80968 - Null Returns
							return culled;
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
					AAMPLOG_INFO("StreamAbstractionAAMP_MPD: PrevOffset %ld CurrentOffset %ld culled (%f)", mPrevLastSegurlOffset, currOffset, culled);
					mPrevLastSegurlOffset = duration - newOffset;
					mPrevLastSegurlMedia = newMedia;
				}
			}
			else
			{
				AAMPLOG_INFO("StreamAbstractionAAMP_MPD: NULL segmentTemplate and segmentList");
			}
		}
	}
	else
	{
		AAMPLOG_WARN("StreamAbstractionAAMP_MPD: NULL adaptationset");
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
			AAMPLOG_WARN("Duration is less than culled seconds, updating it wrt actual fragment duration");
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

		AAMPLOG_INFO("Culled seconds = %f, Updated culledSeconds: %lf duration: %lf", culled, mCulledSeconds, aamp->mAbsoluteEndPosition);
	}
}

/**
 * @brief Fetch and inject initialization fragment for all available tracks
 */
void StreamAbstractionAAMP_MPD::FetchAndInjectInitFragments(bool discontinuity)
{
	for( int i = 0; i < mNumberOfTracks; i++)
	{
		FetchAndInjectInitialization(i,discontinuity);
	}
}

/**
 * @brief Fetch and inject initialization fragment for media type
 */
void StreamAbstractionAAMP_MPD::FetchAndInjectInitialization(int trackIdx, bool discontinuity)
{
		FN_TRACE_F_MPD( __FUNCTION__ );
		pthread_t trackDownloadThreadID;
		HeaderFetchParams *fetchParams = NULL;
		bool dlThreadCreated = false;
		class MediaStreamContext *pMediaStreamContext = mMediaStreamContext[trackIdx];
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
								AAMPLOG_WARN("StreamAbstractionAAMP_MPD: pthread_create failed for TrackDownloader with errno = %d, %s", errno, strerror(errno));
								SAFE_DELETE(fetchParams);
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

								FetchFragment(pMediaStreamContext, initialization, fragmentDuration, true, getCurlInstanceByMediaType(static_cast<MediaType>(trackIdx)), pMediaStreamContext->discontinuity);
								pMediaStreamContext->discontinuity = false;
							}
						}
					}
					else
					{
						AAMPLOG_WARN("initialization  is null");  //CID:84853 ,86291- Null Return
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
						std::string range;
						const IURLType *urlType = segmentBase->GetInitialization();
						if (urlType)
						{
							range = urlType->GetRange();
						}
						else
						{
							range = segmentBase->GetIndexRange();
							uint64_t s1,s2;
							sscanf(range.c_str(), "%" PRIu64 "-%" PRIu64 "", &s1,&s2);
							char temp[128];
							sprintf( temp, "%lu", s1-1 );
							range = "0-";
							range += temp;
						}
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
								AAMPLOG_TRACE("StreamAbstractionAAMP_MPD: did not cache fragmentUrl %s fragmentTime %f", fragmentUrl.c_str(), pMediaStreamContext->fragmentTime);
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
									AAMPLOG_WARN("initialization is null");
									return;
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
										AAMPLOG_WARN("StreamAbstractionAAMP_MPD: pthread_create failed for TrackDownloader with errno = %d, %s", errno, strerror(errno));
										SAFE_DELETE(fetchParams);
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
										FetchFragment(pMediaStreamContext, initialization, fragmentDuration, true, getCurlInstanceByMediaType(static_cast<MediaType>(trackIdx)));
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
												AAMPLOG_WARN("StreamAbstractionAAMP_MPD: segmentList - cannot determine range for Initialization - first segment start %d",
														start);
											}
										}
									}
									else
									{
										AAMPLOG_WARN("firstSegmentURL is null");  //CID:80649 - Null Returns
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
											AAMPLOG_TRACE("StreamAbstractionAAMP_MPD: did not cache fragmentUrl %s fragmentTime %f", fragmentUrl.c_str(), pMediaStreamContext->fragmentTime);
										}
									}
								}
								else
								{
									AAMPLOG_WARN("StreamAbstractionAAMP_MPD: segmentList - empty range string for Initialization"
											);
								}
							}
						}
						else
						{
							AAMPLOG_ERR("not-yet-supported mpd format");
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
		SAFE_DELETE(fetchParams);
	}
}


/**
 * @brief Check if current period is clear
 * @retval true if clear period
 */
bool StreamAbstractionAAMP_MPD::CheckForInitalClearPeriod()
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	bool ret = true;
	for(int i = 0; i < mNumberOfTracks; i++)
	{
		vector<IDescriptor*> contentProt = GetContentProtection(mMediaStreamContext[i]->adaptationSet, (MediaType)i );
		if(0 != contentProt.size())
		{
			ret = false;
			break;
		}
	}
	if(ret)
	{
		AAMPLOG_WARN("Initial period is clear period, trying work around");
	}
	return ret;
}

/**
 * @brief Push encrypted headers if available
 * return true for a successful encypted init fragment push
 */
bool StreamAbstractionAAMP_MPD::PushEncryptedHeaders()
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	bool ret = false;
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
							vector<IDescriptor*> contentProt = GetContentProtection(adaptationSet, (MediaType)i );
							if(0 == contentProt.size())
							{
								continue;
							}
							else
							{
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
									bool disableAC4 = ISCONFIGSET(eAAMPConfig_DisableAC4);
									bool disableAC3 = ISCONFIGSET(eAAMPConfig_DisableAC3);
									bool disabled = false;
									representionIndex = GetDesiredCodecIndex(adaptationSet, selectedAudioType, selectedRepBandwidth,disableEC3 , disableATMOS, disableAC4, disableAC3, disabled);
									if(selectedAudioType != mAudioType)
									{
										continue;
									}
									AAMPLOG_WARN("Audio type %d", selectedAudioType);
								}
								else
								{
									AAMPLOG_WARN("Audio type eAUDIO_UNKNOWN");
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

										fragmentDescriptor->ClearMatchingBaseUrl();
										fragmentDescriptor->AppendMatchingBaseUrl(&mpd->GetBaseUrls());
										fragmentDescriptor->AppendMatchingBaseUrl(&period->GetBaseURLs());
										fragmentDescriptor->AppendMatchingBaseUrl(&adaptationSet->GetBaseURLs());
										fragmentDescriptor->AppendMatchingBaseUrl(&representation->GetBaseURLs());

										fragmentDescriptor->RepresentationID.assign(representation->GetId());
										GetFragmentUrl(fragmentUrl,fragmentDescriptor , initialization);
										if (mMediaStreamContext[i]->WaitForFreeFragmentAvailable())
										{
											AAMPLOG_WARN("Pushing encrypted header for %s", getMediaTypeName(MediaType(i)));
											//Set the last parameter (overWriteTrackId) true to overwrite the track id if ad and content has different track ids
											bool temp =  mMediaStreamContext[i]->CacheFragment(fragmentUrl, (eCURLINSTANCE_VIDEO + mMediaStreamContext[i]->mediaType), mMediaStreamContext[i]->fragmentTime, 0.0, NULL, true, false, false, 0, 0, true);
											if(!temp)
											{
												AAMPLOG_TRACE("StreamAbstractionAAMP_MPD: did not cache fragmentUrl %s fragmentTime %f", fragmentUrl.c_str(), mMediaStreamContext[i]->fragmentTime); //CID:84438 - checked return
											}
										}
										SAFE_DELETE(fragmentDescriptor);
										ret = true;
										encryptionFound = true;
									}
								}
							}
						}
					}
					else
					{
						AAMPLOG_WARN("adaptationSet is null");  //CID:84361 - Null Returns
					}
				}
			}
			else
			{
				AAMPLOG_WARN("period    is null");  //CID:86137 - Null Returns
			}
			iPeriod++;
		}
	}
	return ret;
}


/**
 * @brief Fetches and caches audio fragment parallelly for video fragment.
 */
void StreamAbstractionAAMP_MPD::AdvanceTrack(int trackIdx, bool trickPlay, double delta, bool *waitForFreeFrag, bool *exitFetchLoop, bool *bCacheFullState)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
		if(ISCONFIGSET(eAAMPConfig_SuppressDecode))
		{
			state = eSTATE_PLAYING;
		}
		if(state == eSTATE_PLAYING)
		{
			*waitForFreeFrag = false;
		}
		else
		{
			int timeoutMs = -1;
			if(bCacheFullState && *bCacheFullState &&
				(pMediaStreamContext->numberOfFragmentsCached == maxCachedFragmentsPerTrack))
			{
				timeoutMs = MAX_WAIT_TIMEOUT_MS;
			}
			isAllowNextFrag = pMediaStreamContext->WaitForFreeFragmentAvailable(timeoutMs);
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
						//When player started in trickplay rate during player swithcing, make sure that we are showing atleast one frame (mainly to avoid cases where trickplay rate is so high that an ad could get skipped completely)
						if(aamp->playerStartedWithTrickPlay)
						{
							AAMPLOG_WARN("Played switched in trickplay, delta set to zero");
							delta = 0;
							aamp->playerStartedWithTrickPlay = false;
						}
						else if((rate > 0 && delta <= 0) || (rate < 0 && delta >= 0))
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
						if((!pMediaStreamContext->GetContext()->trickplayMode) && (eMEDIATYPE_VIDEO == trackIdx)&& !pMediaStreamContext->failAdjacentSegment)
						{
							if (aamp->CheckABREnabled())
							{
								pMediaStreamContext->GetContext()->CheckForProfileChange();
							}
						}
					}
					else if (pMediaStreamContext->eos == true && mIsLiveManifest && trackIdx == eMEDIATYPE_VIDEO && !(ISCONFIGSET(eAAMPConfig_InterruptHandling) && mIsFogTSB))
					{
						pMediaStreamContext->GetContext()->CheckForPlaybackStall(false);
					}

					if (AdState::IN_ADBREAK_AD_PLAYING == mCdaiObject->mAdState && rate > 0 && !(pMediaStreamContext->eos)
							&& mCdaiObject->CheckForAdTerminate(pMediaStreamContext->fragmentTime - pMediaStreamContext->periodStartOffset))
					{
						//Ensuring that Ad playback doesn't go beyond Adbreak
						AAMPLOG_WARN("[CDAI] Adbreak ended early. Terminating Ad playback. fragmentTime[%lf] periodStartOffset[%lf]",
											pMediaStreamContext->fragmentTime, pMediaStreamContext->periodStartOffset);
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
				FetchAndInjectInitialization(trackIdx);
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
	FN_TRACE_F_MPD( __FUNCTION__ );
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
	if(currPeriod == NULL)
	{
		AAMPLOG_WARN("currPeriod is null");  //CID:80891 - Null Returns
		return;
	}
	std::string currentPeriodId = currPeriod->GetId();
	mPrevAdaptationSetCount = currPeriod->GetAdaptationSets().size();
	AAMPLOG_WARN("aamp: ready to collect fragments. mpd %p", mpd);
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
				if(IsEmptyPeriod(temp, mIsFogTSB))
				{
					lowerBoundary++;
					continue;
				}
				break;
			}
			// Calculate upper boundary of playable periods, discard empty periods at the end
			for(auto iter = availablePeriods.rbegin() ; iter != availablePeriods.rend(); iter++ )
			{
				if(IsEmptyPeriod(*iter, mIsFogTSB))
				{
					upperBoundary--;
					continue;
				}
				break;
			}

			while(iPeriod < numPeriods && !exitFetchLoop)  //CID:95090 - No effect
			{
				bool periodChanged = (iPeriod != mCurrentPeriodIdx) || (mBasePeriodId != mpd->GetPeriods().at(mCurrentPeriodIdx)->GetId());
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
										AAMPLOG_INFO("New VSS Period : %s Key ID: %s", tempPeriod->GetId().c_str(), keyIdDebugStr.c_str());
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
											AAMPLOG_TRACE("Skipping license request for keyID : %s", keyIdDebugStr.c_str() );
										}
									}
									else
									{
										AAMPLOG_WARN("Failed to get keyID for vss common key EAP");
									}
									}
									else
									{
										AAMPLOG_ERR("Failed to Create DRM Helper");	
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
						AAMPLOG_WARN("Period(%s - %d/%zu) Offset[%lf] IsLive(%d) IsCdvr(%d) ",
							mBasePeriodId.c_str(), mCurrentPeriodIdx, numPeriods, mBasePeriodOffset, mIsLiveStream, aamp->IsInProgressCDVR());

						vector <IAdaptationSet*> adapatationSets = newPeriod->GetAdaptationSets();
						int adaptationSetCount = adapatationSets.size();
						if(0 == adaptationSetCount || IsEmptyPeriod(newPeriod, mIsFogTSB))
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
						//Update period gaps for playback stats
						if(mIsLiveStream)
						{
							double periodGap = (GetPeriodEndTime(mpd, mCurrentPeriodIdx, mLastPlaylistDownloadTimeMs) -GetPeriodStartTime(mpd, iPeriod)) * 1000;
							if(periodGap > 0)
							{
								//for livestream, period gaps are updated as playback progresses throug the periods
								aamp->IncrementGaps();
							}
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
								AAMPLOG_INFO("[CDAI] Landed at the periodId[%d] ",mCurrentPeriodIdx);
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
						if (aamp->mIsPeriodChangeMarked)
						{
							AAMPLOG_WARN("Discontinuity process is yet to complete, going to wait until it is done");
							aamp->WaitForDiscontinuityProcessToComplete();
						}

						/*DELIA-47914:  If next period is empty, period ID change is  not processing.
						Will check the period change for the same period in the next iteration.*/
						if(adaptationSetCount > 0 || !IsEmptyPeriod(mCurrentPeriod , mIsFogTSB))
						{
							AAMPLOG_WARN("Period ID changed from \'%s\' to \'%s\' [BasePeriodId=\'%s\']", currentPeriodId.c_str(),mCurrentPeriod->GetId().c_str(), mBasePeriodId.c_str());
							currentPeriodId = mCurrentPeriod->GetId();
							mPrevAdaptationSetCount = adaptationSetCount;
							periodChanged = true;
							if (!trickPlay)
                                                        {
								aamp->mIsPeriodChangeMarked = true;
                                                        }
							requireStreamSelection = true;

							AAMPLOG_WARN("playing period %d/%d", iPeriod, (int)numPeriods);
						}
						else
						{
							for (int i = 0; i < mNumberOfTracks; i++)
							{
								mMediaStreamContext[i]->enabled = false;
							}
							AAMPLOG_WARN("Period ID not changed from \'%s\' to \'%s\',since period is empty [BasePeriodId=\'%s\']", currentPeriodId.c_str(),mCurrentPeriod->GetId().c_str(), mBasePeriodId.c_str());
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
						AAMPLOG_WARN("Change in AdaptationSet count; adaptationSetCount %d  mPrevAdaptationSetCount %d,updating stream selection", adaptationSetCount, mPrevAdaptationSetCount);
						mPrevAdaptationSetCount = adaptationSetCount;
						requireStreamSelection = true;
					}
					else
					{
						for (int i = 0; i < mNumberOfTracks; i++)
						{
							if(mMediaStreamContext[i]->adaptationSetId != adapatationSets.at(mMediaStreamContext[i]->adaptationSetIdx)->GetId())
							{
								AAMPLOG_WARN("AdaptationSet index changed; updating stream selection");
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
							AAMPLOG_INFO("Culled seconds = %f", culled);
							aamp->UpdateCullingState(culled);
							mCulledSeconds += culled;
						}
						if(ISCONFIGSET(eAAMPConfig_UseAbsoluteTimeline) && mIsLiveStream && !aamp->IsUninterruptedTSB())
						{
							UpdateCulledAndDurationFromPeriodInfo();
						}
						
						double durMs = 0;
						mPeriodEndTime = GetPeriodEndTime(mpd, mCurrentPeriodIdx, mLastPlaylistDownloadTimeMs);
						mPeriodStartTime = GetPeriodStartTime(mpd, mCurrentPeriodIdx);
						mPeriodDuration = GetPeriodDuration(mpd, mCurrentPeriodIdx);

						for(int periodIter = 0; periodIter < mpd->GetPeriods().size(); periodIter++)
						{
							if(!IsEmptyPeriod(mpd->GetPeriods().at(periodIter), mIsFogTSB))
							{
								durMs += GetPeriodDuration(mpd, periodIter);
							}
						}

						double duration = (double)durMs / 1000;
						aamp->UpdateDuration(duration);
						mLiveEndPosition = duration + mCulledSeconds;
						if(mCdaiObject->mContentSeekOffset)
						{
							AAMPLOG_INFO("[CDAI]: Resuming channel playback at PeriodID[%s] at Position[%lf]", currentPeriodId.c_str(), mCdaiObject->mContentSeekOffset);
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
							AAMPLOG_WARN("Error! Audio or Video track missing in period, ignoring discontinuity");
							aamp->mIsPeriodChangeMarked = false;
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
									AAMPLOG_WARN("StreamAbstractionAAMP_MPD: discontinuity detected nextSegmentTime %" PRIu64 " FirstSegmentStartTime %" PRIu64 " ", nextSegmentTime, segmentStartTime);
									discontinuity = true;
									if(segmentTemplates.GetTimescale() != 0)
									{
										mFirstPTS = (double)segmentStartTime/(double)segmentTemplates.GetTimescale();
									}  //CID:186900 - divide by zero
									double startTime = (GetPeriodStartTime(mpd, mCurrentPeriodIdx) - mAvailabilityStartTime);
									if((startTime != 0) && !aamp->IsUninterruptedTSB())
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
								else if(nextSegmentTime != segmentStartTime)
								{
									discontinuity = true;
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
									AAMPLOG_WARN("StreamAbstractionAAMP_MPD: discontinuity detected");
								}
								else
								{
									AAMPLOG_WARN("StreamAbstractionAAMP_MPD: No discontinuity detected nextSegmentTime %" PRIu64 " FirstSegmentStartTime %" PRIu64 " ", nextSegmentTime, segmentStartTime);
									aamp->mIsPeriodChangeMarked = false;
								}
							}
							else
							{
								AAMPLOG_TRACE("StreamAbstractionAAMP_MPD:: Segment template not available");
								aamp->mIsPeriodChangeMarked = false;
							}
						}
					}
					FetchAndInjectInitFragments(discontinuity);
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
							SAFE_DELETE(parallelDownload[trackIdx]);
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
							AAMPLOG_INFO("EOS - Exit fetch loop ");
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
							AAMPLOG_INFO("[CDAI]: BasePeriod[%s] completed @%lf. Changing to next ", mBasePeriodId.c_str(),mBasePeriodOffset);
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
						int timeoutMs = MAX_WAIT_TIMEOUT_MS;
						AAMPLOG_TRACE("Cache full state,no download until(%d) Time(%lld)",timeoutMs,aamp_GetCurrentTimeMS());
						bool temp =  mMediaStreamContext[eMEDIATYPE_VIDEO]->WaitForFreeFragmentAvailable(timeoutMs);
						if(temp == false)
						{
							AAMPLOG_TRACE("Waiting for FreeFragmentAvailable");  //CID:82355 - checked return
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
			AAMPLOG_WARN("StreamAbstractionAAMP_MPD: null mpd");
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
						AAMPLOG_WARN("Buffer is running low(%ld).Refreshing playlist(%d).PlayPosition(%lld) End(%lld)",
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
				if(aamp->mPipelineIsClear && IsPeriodEncrypted(periods.at(iter)))
				{
					AAMPLOG_WARN("Encrypted period found while pipeline is currently configured for clear streams");
					aamp->mEncryptedPeriodFound = true;
				}

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
				AAMPLOG_WARN("MPD Fragment Collector detected reset in Period(New Size:%zu)(currentIdx:%d->%zu)",
					newPeriods,mCurrentPeriodIdx,newPeriods - 1);
				mCurrentPeriodIdx = newPeriods - 1;
			}
		}
		mpdChanged = true;
	}		//Loop 1
	while (!exitFetchLoop);
	AAMPLOG_WARN("MPD fragment collector done");
}


/**
 * @brief Check new early available periods
 */
void StreamAbstractionAAMP_MPD::GetAvailableVSSPeriods(std::vector<IPeriod*>& PeriodIds)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
	FN_TRACE_F_MPD( __FUNCTION__ );
	bool isVss = false;
	IMPDElement* nodePtr = mpd;

	if (!nodePtr)
	{
		AAMPLOG_ERR("API Failed due to Invalid Arguments");
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
							AAMPLOG_INFO("Recieved Common Key Duration : %d of VSS stream", mCommonKeyDuration);
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
	FN_TRACE_F_MPD( __FUNCTION__ );
	std::string ret;
	IMPDElement* nodePtr = mpd;

	if (!nodePtr)
	{
		AAMPLOG_ERR("API Failed due to Invalid Arguments");
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
								AAMPLOG_INFO("Parsed Virtual Stream ID from manifest:%s", ret.c_str());
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
 * @brief Init webvtt subtitle parser for sidecar caption support
 * @param webvtt data
 */
void StreamAbstractionAAMP_MPD::InitSubtitleParser(char *data)
{
	mSubtitleParser = SubtecFactory::createSubtitleParser(mLogObj, aamp, eSUB_TYPE_WEBVTT);
	if (mSubtitleParser)
	{
		AAMPLOG_WARN("sending init to webvtt parser %.3f", seekPosition);
		mSubtitleParser->init(seekPosition,0);
		mSubtitleParser->mute(false);
		AAMPLOG_INFO("sending data");
		if (data != NULL)
			mSubtitleParser->processData(data,strlen(data),0,0);
	}

}

/**
 * @brief Reset subtitle parser created for sidecar caption support
 */
void StreamAbstractionAAMP_MPD::ResetSubtitle()
{
	if (mSubtitleParser)
	{
		mSubtitleParser->reset();
		mSubtitleParser = NULL;
	}
}

/**
 * @brief Mute subtitles on puase
 */
void StreamAbstractionAAMP_MPD::MuteSubtitleOnPause()
{
	if (mSubtitleParser)
	{
		mSubtitleParser->pause(true);
		mSubtitleParser->mute(true);
	}
}

/**
 * @brief Resume subtitle on play
 * @param mute status
 * @param webvtt data
 */
void StreamAbstractionAAMP_MPD::ResumeSubtitleOnPlay(bool mute, char *data)
{
	if (mSubtitleParser)
	{
		mSubtitleParser->pause(false);
		mSubtitleParser->mute(mute);
		if (data != NULL)
			mSubtitleParser->processData(data,strlen(data),0,0);
	}
}

/**
 * @brief Mute/unmute subtitles
 * @param mute/unmute
 */
void StreamAbstractionAAMP_MPD::MuteSidecarSubtitles(bool mute)
{
	if (mSubtitleParser)
		mSubtitleParser->mute(mute);
}

/**
 * @brief Resume subtitle after seek
 * @param mute status
 * @param webvtt data
 */
void  StreamAbstractionAAMP_MPD::ResumeSubtitleAfterSeek(bool mute, char *data)
{
	mSubtitleParser = SubtecFactory::createSubtitleParser(mLogObj, aamp, eSUB_TYPE_WEBVTT);
	if (mSubtitleParser)
	{
		mSubtitleParser->updateTimestamp(seekPosition*1000);
		mSubtitleParser->mute(mute);
		if (data != NULL)
			mSubtitleParser->processData(data,strlen(data),0,0);
	}

}

/**
 * @brief StreamAbstractionAAMP_MPD Destructor
 */
StreamAbstractionAAMP_MPD::~StreamAbstractionAAMP_MPD()
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	for (int iTrack = 0; iTrack < mMaxTracks; iTrack++)
	{
		MediaStreamContext *track = mMediaStreamContext[iTrack];
		SAFE_DELETE(track);
	}

	aamp->SyncBegin();
	SAFE_DELETE(mpd);

	SAFE_DELETE_ARRAY(mStreamInfo);

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
			SAFE_DELETE(tmp);
		}
	}

	aamp->CurlTerm(eCURLINSTANCE_VIDEO, AAMP_TRACK_COUNT);
	memset(aamp->GetLLDashServiceData(),0x00,sizeof(AampLLDashServiceData));
	aamp->SetLowLatencyServiceConfigured(false);
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

			if(aamp->GetLLDashServiceData()->lowLatencyMode)
			{
				mMediaStreamContext[i]->StartInjectChunkLoop();
			}
		}
	}
	TuneType tuneType = aamp->GetTuneType();
	if( (aamp->GetLLDashServiceData()->lowLatencyMode && ISCONFIGSET( eAAMPConfig_EnableLowLatencyCorrection ) ) )
	{
		StartLatencyMonitorThread();
	}
}

/**
 *   @brief  Stops streaming.
 */
void StreamAbstractionAAMP_MPD::Stop(bool clearChannelData)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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

	if(latencyMonitorThreadStarted)
	{
		AAMPLOG_INFO("Waiting to join StartLatencyMonitorThread");
		int rc = pthread_join(latencyMonitorThreadID, NULL);
		if (rc != 0)
		{
			AAMPLOG_WARN("pthread_join returned %d for StartLatencyMonitorThread", rc);
		}
		AAMPLOG_INFO("Joined StartLatencyMonitorThread");
		latencyMonitorThreadStarted = false;
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
			if(!ISCONFIGSET(eAAMPConfig_GstSubtecEnabled))
			{
				if (iTrack == eMEDIATYPE_SUBTITLE && track->mSubtitleParser)
				{
					track->mSubtitleParser->reset();
				}
			}

			if(aamp->GetLLDashServiceData()->lowLatencyMode)
			{
				track->StopInjectChunkLoop();
			}
		}
	}

	if(drmSessionThreadStarted)
	{
		AAMPLOG_INFO("Waiting to join CreateDRMSession thread");
		int rc = pthread_join(createDRMSessionThreadID, NULL);
		if (rc != 0)
		{
			AAMPLOG_WARN("pthread_join returned %d for createDRMSession Thread", rc);
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
			SAFE_DELETE(deferredDRMRequestThread);
		}
	}

	aamp->mStreamSink->ClearProtectionEvent();
	if (clearChannelData)
	{
#ifdef AAMP_MPD_DRM
		if(ISCONFIGSET(eAAMPConfig_UseSecManager))
		{
			aamp->mDRMSessionManager->notifyCleanup();
		}
#endif
	}
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

StreamOutputFormat GetSubtitleFormat(std::string mimeType)
{
	StreamOutputFormat format = FORMAT_SUBTITLE_MP4; // Should I default to INVALID?

	if (!mimeType.compare("application/mp4"))
		format = FORMAT_SUBTITLE_MP4;
	else if (!mimeType.compare("application/ttml+xml"))
		format = FORMAT_SUBTITLE_TTML;
	else if (!mimeType.compare("text/vtt"))
		format = FORMAT_SUBTITLE_WEBVTT;
	else
		AAMPLOG_INFO("Not found mimeType %s", mimeType.c_str());

	AAMPLOG_TRACE("Returning format %d for mimeType %s", format, mimeType.c_str());

	return format;
}

/**
 * @brief Get output format of stream.
 *
 */
void StreamAbstractionAAMP_MPD::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxOutputFormat, StreamOutputFormat &subtitleOutputFormat)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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

	//Bleurgh - check whether the ugly hack above is in operation
	if (mMediaStreamContext[eMEDIATYPE_SUBTITLE] && mMediaStreamContext[eMEDIATYPE_SUBTITLE]->enabled && mMediaStreamContext[eMEDIATYPE_SUBTITLE]->type != eTRACK_AUX_AUDIO)
	{
		AAMPLOG_WARN("Entering GetCurrentMimeType");
		auto mimeType = GetCurrentMimeType(eMEDIATYPE_SUBTITLE);
		if (!mimeType.empty())
			subtitleOutputFormat = GetSubtitleFormat(mimeType);
		else
		{
			AAMPLOG_INFO("mimeType empty");
			subtitleOutputFormat = FORMAT_SUBTITLE_MP4;
		}
	}
	else
	{
		subtitleOutputFormat = FORMAT_INVALID;
	}
}

/**
 *   @brief Return MediaTrack of requested type
 *
 *   @retval MediaTrack pointer.
 */
MediaTrack* StreamAbstractionAAMP_MPD::GetMediaTrack(TrackType type)
{
	//FN_TRACE_F_MPD( __FUNCTION__ );
	return mMediaStreamContext[type];
}

double StreamAbstractionAAMP_MPD::GetBufferedDuration()
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
	FN_TRACE_F_MPD( __FUNCTION__ );
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
	//FN_TRACE_F_MPD( __FUNCTION__ );
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
 * @retval profile index of the current bandwidth
 */
int StreamAbstractionAAMP_MPD::GetProfileIndexForBandwidth(long mTsbBandwidth)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
 *   @retval stream information corresponding to index.
 */
StreamInfo* StreamAbstractionAAMP_MPD::GetStreamInfo(int idx)
{
	//FN_TRACE_F_MPD( __FUNCTION__ );
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
	FN_TRACE_F_MPD( __FUNCTION__ );
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
 * @retval profile index
 */
int StreamAbstractionAAMP_MPD::GetBWIndex(long bitrate)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
	FN_TRACE_F_MPD( __FUNCTION__ );
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
	FN_TRACE_F_MPD( __FUNCTION__ );
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
	FN_TRACE_F_MPD( __FUNCTION__ );
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
	FN_TRACE_F_MPD( __FUNCTION__ );
	bool ret = false;
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
							PeriodElement periodElement(adaptationSets.at(j), rep);
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
												AAMPLOG_TRACE("schemeuri = thumbnail_tile");
											}
											else
											{
												AAMPLOG_WARN("skipping schemeUri %s", schemeUri.c_str());
											}
										}
										if(xml->HasAttribute("value"))
										{
											const std::string& value = xml->GetAttributeValue("value");
											if(!value.empty())
											{
												sscanf(value.c_str(), "%dx%d",&w,&h);
												AAMPLOG_WARN("value=%dx%d",w,h);
												done = true;
											}
										}
									}
									else
									{
										AAMPLOG_WARN("skipping name %s", xml->GetName().c_str());
									}
								}
								else
								{
									AAMPLOG_WARN("xml is null");  //CID:81118 - Null Returns
								}
							}	// end of sub node loop
							bandwidth = rep->GetBandwidth();
							if(thumbIndexValue < 0 || trackEmpty)
							{
								std::string mimeType = periodElement.GetMimeType();
								StreamInfo *tmp = new StreamInfo;
								tmp->bandwidthBitsPerSecond = (long) bandwidth;
								tmp->resolution.width = rep->GetWidth()/w;
								tmp->resolution.height = rep->GetHeight()/h;
								thumbnailtrack.push_back(tmp);
								AAMPLOG_TRACE("thumbnailtrack bandwidth=%d width=%d height=%d", tmp->bandwidthBitsPerSecond, tmp->resolution.width, tmp->resolution.height);
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
										AAMPLOG_TRACE("segment timeline");
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
											if(timeScale != 0)
											{
												double startTime = double(timeline->GetStartTime() / timeScale);  //CID:170361 - Unintended integer division
												int repeatCount = timeline->GetRepeatCount();
												uint32_t timelineDurationMs = ComputeFragmentDuration(timeline->GetDuration(),timeScale);
												while( repeatCount-- >= 0 )
												{
													std::string tmedia = media;
													TileInfo tileInfo;
													memset( &tileInfo,0,sizeof(tileInfo) );
													tileInfo.startTime = startTime + ( adDuration / timeScale) ;
													AAMPLOG_TRACE("timeLineIndex[%d] size [%lu] updated durationMs[%" PRIu64 "] startTime:%f adDuration:%f repeatCount:%d",  timeLineIndex, timelines.size(), durationMs, startTime, adDuration, repeatCount);

													startTime += ( timelineDurationMs );
													replace(tmedia, "Number", startNumber);
													char *ptr = strndup(tmedia.c_str(), tmedia.size());
													tileInfo.url = ptr;
													AAMPLOG_TRACE("tileInfo.url%s:%p",tileInfo.url, ptr);
													tileInfo.posterDuration = ((double)segmentTemplates.GetDuration()) / (timeScale * w * h);
													tileInfo.tileSetDuration = ComputeFragmentDuration(timeline->GetDuration(), timeScale);
													tileInfo.numRows = h;
													tileInfo.numCols = w;
													AAMPLOG_TRACE("TileInfo - StartTime:%f posterDuration:%d tileSetDuration:%f numRows:%d numCols:%d",tileInfo.startTime,tileInfo.posterDuration,tileInfo.tileSetDuration,tileInfo.numRows,tileInfo.numCols);
													indexedTileInfo.push_back(tileInfo);
													startNumber++;
												}
												timeLineIndex++;
											}
										}		// emd of timeLine loop
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
	AAMPLOG_WARN("Exiting");
}

/**
 * @brief To get the available thumbnail tracks.
 * @ret available thumbnail tracks.
 */
std::vector<StreamInfo*> StreamAbstractionAAMP_MPD::GetAvailableThumbnailTracks(void)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
 * @return bool true on success.
 */
bool StreamAbstractionAAMP_MPD::SetThumbnailTrack(int thumbnailIndex)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
 * @return Updated vector of available thumbnail data.
 */
std::vector<ThumbnailData> StreamAbstractionAAMP_MPD::GetThumbnailRangeData(double tStart, double tEnd, std::string *baseurl, int *raw_w, int *raw_h, int *width, int *height)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
        std::vector<ThumbnailData> data;
	if(indexedTileInfo.empty())
	{
		if(aamp->mthumbIndexValue >= 0)
		{
			AAMPLOG_WARN("calling indexthumbnail");
			deIndexTileInfo(indexedTileInfo);
			indexThumbnails(mpd, aamp->mthumbIndexValue, indexedTileInfo, thumbnailtrack);
		}
		else
		{
			AAMPLOG_WARN("Exiting. Thumbnail track not configured!!!.");
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
        FN_TRACE_F_MPD( __FUNCTION__ );
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
			if(aamp->GetLLDashServiceData()->lowLatencyMode)
			{
				track->StopInjectChunkLoop();
			}

		}
	}
}

/**
 *   @brief  Start injecting fragments to StreamSink.
 */
void StreamAbstractionAAMP_MPD::StartInjection(void)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	mTrackState = eDISCONTIUITY_FREE;
	for (int iTrack = 0; iTrack < mNumberOfTracks; iTrack++)
	{
		MediaStreamContext *track = mMediaStreamContext[iTrack];
		if(track && track->Enabled())
		{
			aamp->ResumeTrackInjection((MediaType) iTrack);
			track->StartInjectLoop();

			if(aamp->GetLLDashServiceData()->lowLatencyMode)
			{
				track->StartInjectChunkLoop();
			}
		}
	}
}


void StreamAbstractionAAMP_MPD::SetCDAIObject(CDAIObject *cdaiObj)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	if(cdaiObj)
	{
		CDAIObjectMPD *cdaiObjMpd = static_cast<CDAIObjectMPD *>(cdaiObj);
		mCdaiObject = cdaiObjMpd->GetPrivateCDAIObjectMPD();
	}
}
/**
 *   @brief Check whether the period has any valid ad.
 *
 */
bool StreamAbstractionAAMP_MPD::isAdbreakStart(IPeriod *period, uint64_t &startMS, std::vector<EventBreakInfo> &eventBreakVec)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	const std::vector<IEventStream *> &eventStreams = period->GetEventStreams();
	bool ret = false;
	uint32_t duration = 0;
	bool isScteEvent = false;
	for(auto &eventStream: eventStreams)
	{
		cJSON* root = cJSON_CreateObject();
		if(root)
		{
			for(auto &attribute : eventStream->GetRawAttributes())
			{
				cJSON_AddStringToObject(root, attribute.first.c_str(), attribute.second.c_str());
			}
			cJSON *eventsRoot = cJSON_AddArrayToObject(root,"Event");
			for(auto &event: eventStream->GetEvents())
			{
				cJSON *item;
				cJSON_AddItemToArray(eventsRoot, item = cJSON_CreateObject() );
				//Currently for linear assets only the SCTE35 events having 'duration' tag present are considered for generating timedMetadat Events.
				//For VOD assets the events are generated irrespective of the 'duration' tag present or not
				if(event)
				{
					bool eventHasDuration = false;

					for(auto &attribute : event->GetRawAttributes())
					{
						cJSON_AddStringToObject(item, attribute.first.c_str(), attribute.second.c_str());
						if (attribute.first == "duration")
						{
							eventHasDuration = true;
						}
					}

					// Try and get the PTS presentation time from the event (and convert to ms)
					uint64_t presentationTime = event->GetPresentationTime();
					if (presentationTime)
					{
						presentationTime *= 1000;

						uint64_t ts = eventStream->GetTimescale();
						if (ts > 1)
						{
							presentationTime /= ts;
						}
					}

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
								isScteEvent = true;

								bool processEvent = (!mIsLiveManifest || ( mIsLiveManifest && (0 != event->GetDuration())));

								bool modifySCTEProcessing = ISCONFIGSET(eAAMPConfig_EnableSCTE35PresentationTime);
								if (modifySCTEProcessing)
								{
									//LLAMA-8251
									// Assuming the comment above is correct then we should really check for a duration tag
									// rather than duration=0. For LLAMA-8251 we will do this to send all the SCTE events in 
									// the manifest even ones with duration=0
									processEvent = (!mIsLiveManifest || eventHasDuration);
								}

								if(processEvent)
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
												EventBreakInfo scte35Event(scte35, "SCTE35", presentationTime, duration);
												eventBreakVec.push_back(scte35Event);

												//LLAMA-8251
												// This may not be necessary but for LLAMA-8251 will to send all the events we find, 
												// even if the manifest is flagged as live
												if(mIsLiveManifest && !modifySCTEProcessing)
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
												AAMPLOG_WARN("[CDAI]: Found a scte35:Binary in manifest with empty binary data!!");
											}
										}
										else
										{
											AAMPLOG_WARN("[CDAI]: Found a scte35:Signal in manifest without scte35:Binary!!");
										}
									}
								}
							}
							else
							{
								if(!evtChild->GetName().empty())
								{
									cJSON* childItem;
									cJSON_AddItemToObject(item, evtChild->GetName().c_str(), childItem = cJSON_CreateObject());
									for (auto &signalChild: evtChild->GetNodes())
									{
										if(signalChild && !signalChild->GetName().empty())
										{
											std::string text = signalChild->GetText();
											if(!text.empty())
											{
												cJSON_AddStringToObject(childItem, signalChild->GetName().c_str(), text.c_str());
											}
											for(auto &attributes : signalChild->GetAttributes())
											{
												cJSON_AddStringToObject(childItem, attributes.first.c_str(), attributes.second.c_str());
											}
										}
									}
								}
								else
								{
									cJSON_AddStringToObject(item, "Event", evtChild->GetText().c_str());
								}
							}
						}
						else
						{
							AAMPLOG_WARN("evtChild is null");  //CID:85816 - Null Return
						}
					}
				}
			}
			if(!isScteEvent)
			{
				char* finalData = cJSON_PrintUnformatted(root);
				if(finalData)
				{
					std::string eventStreamStr(finalData);
					cJSON_free(finalData);
					EventBreakInfo eventBreak(eventStreamStr, "EventStream", 0, duration);
					eventBreakVec.push_back(eventBreak);
					ret = true;
				}
			}
			cJSON_Delete(root);
		}
	}
	return ret;
}

/**
 * @brief Handlig Ad event
 */
bool StreamAbstractionAAMP_MPD::onAdEvent(AdEvent evt)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	double adOffset  = 0.0;   //CID:89257 - Intialization
	return onAdEvent(evt, adOffset);
}

bool StreamAbstractionAAMP_MPD::onAdEvent(AdEvent evt, double &adOffset)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
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
				if(!brkId.empty() && adIdx >= 0)
				{
					AAMPLOG_INFO("[CDAI] CheckForAdStart found Adbreak. adIdx[%d] mBasePeriodOffset[%lf] adOffset[%lf].", adIdx, mBasePeriodOffset, adOffset);

					mCdaiObject->mCurPlayingBreakId = brkId;
					if(-1 != adIdx && mCdaiObject->mAdBreaks[brkId].ads)
					{
						if(!(mCdaiObject->mAdBreaks[brkId].ads->at(adIdx).invalid))
						{
							AAMPLOG_WARN("[CDAI]: STARTING ADBREAK[%s] AdIdx[%d] Found at Period[%s].", brkId.c_str(), adIdx, mBasePeriodId.c_str());
							mCdaiObject->mCurAds = mCdaiObject->mAdBreaks[brkId].ads;

							mCdaiObject->mCurAdIdx = adIdx;
							mCdaiObject->mAdState = AdState::IN_ADBREAK_AD_PLAYING;

							for(int i=0; i<adIdx; i++)
								adPos2Send += mCdaiObject->mCurAds->at(i).duration;
						}
						else
						{
							AAMPLOG_WARN("[CDAI]: AdIdx[%d] in the AdBreak[%s] is invalid. Skipping.", adIdx, brkId.c_str());
						}
						reservationEvt2Send = AAMP_EVENT_AD_RESERVATION_START;
						adbreakId2Send = brkId;
						if(AdEvent::INIT == evt) sendImmediate = true;
					}

					if(AdState::IN_ADBREAK_AD_PLAYING != mCdaiObject->mAdState)
					{
						AAMPLOG_WARN("[CDAI]: BasePeriodId in Adbreak. But Ad not available. BasePeriodId[%s],Adbreak[%s]", mBasePeriodId.c_str(), brkId.c_str());
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
						AAMPLOG_WARN("[CDAI]: AdIdx[%d] Found at Period[%s].", adIdx, mBasePeriodId.c_str());
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
					AAMPLOG_WARN("[CDAI]: ADBREAK[%s] ENDED. Playing the basePeriod[%s].", mCdaiObject->mCurPlayingBreakId.c_str(), mBasePeriodId.c_str());
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
				AAMPLOG_WARN("[CDAI]: Ad finished at Period. Waiting to catchup the base offset.[idx=%d] [period=%s]", mCdaiObject->mCurAdIdx, mBasePeriodId.c_str());
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
				AAMPLOG_WARN("[CDAI]: Ad Playback failed. Going to the base period[%s] at offset[%lf].Ad[idx=%d]", mBasePeriodId.c_str(), mBasePeriodOffset,mCdaiObject->mCurAdIdx);
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
				AAMPLOG_WARN("[CDAI]: BUG! BUG!! BUG!!! We should not come here.AdIdx[-1].");
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
				AAMPLOG_WARN("[CDAI]: Current Ad placement Completed. Ready to play next Ad.");
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
						AAMPLOG_WARN("[CDAI]: AdIdx is invalid. Skipping. AdIdx[%d].", mCdaiObject->mCurAdIdx);
						mCdaiObject->mAdState = AdState::IN_ADBREAK_AD_NOT_PLAYING;
					}
					else
					{
						AAMPLOG_WARN("[CDAI]: Next AdIdx[%d] Found at Period[%s].", mCdaiObject->mCurAdIdx, mBasePeriodId.c_str());
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
					AAMPLOG_WARN("[CDAI]: All Ads in the ADBREAK[%s] FINISHED. Playing the basePeriod[%s] at Offset[%lf].", mCdaiObject->mCurPlayingBreakId.c_str(), mBasePeriodId.c_str(), mCdaiObject->mContentSeekOffset);
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
					AAMPLOG_WARN("[CDAI]: Ad playback failed. Not able to download Ad manifest from FOG.");
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
			AAMPLOG_WARN("[CDAI]: State changed from [%s] => [%s].", ADSTATE_STR[static_cast<int>(oldState)],ADSTATE_STR[static_cast<int>(mCdaiObject->mAdState)]);
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
 * @brief Print the current the track information
 *
 * @return void
 */
void StreamAbstractionAAMP_MPD::printSelectedTrack(const std::string &trackIndex, MediaType media)
{
	if (!trackIndex.empty())
	{
		if (media == eMEDIATYPE_AUDIO)
		{
			for (auto &audioTrack : mAudioTracks)
			{
				if (audioTrack.index == trackIndex)
				{
					AAMPLOG_INFO("Selected Audio Track: Index:%s language:%s rendition:%s name:%s label:%s type:%s codec:%s bandwidth:%ld Channel:%d Accessibility:%s ", 
					audioTrack.index.c_str(), audioTrack.language.c_str(), audioTrack.rendition.c_str(), audioTrack.name.c_str(), 
					audioTrack.label.c_str(), audioTrack.mType.c_str(), audioTrack.codec.c_str(), 
					audioTrack.bandwidth, audioTrack.channels, audioTrack.accessibilityItem.print().c_str());
					break;
				}
			}
		}
		else if (media == eMEDIATYPE_SUBTITLE)
		{
			for (auto &textTrack : mTextTracks)
			{
				if (textTrack.index == trackIndex)
				{
					AAMPLOG_INFO("Selected Text Track: Index:%s language:%s rendition:%s name:%s label:%s type:%s codec:%s isCC:%d Accessibility:%s ", 
					textTrack.index.c_str(), textTrack.language.c_str(), textTrack.rendition.c_str(), textTrack.name.c_str(), 
					textTrack.label.c_str(), textTrack.mType.c_str(), textTrack.codec.c_str(), textTrack.isCC, textTrack.accessibilityItem.print().c_str());
					break;
				}
			}
		}
	}
}

/**
 * @brief To set the audio tracks of current period
 *
 * @return void
 */
void StreamAbstractionAAMP_MPD::SetAudioTrackInfo(const std::vector<AudioTrackInfo> &tracks, const std::string &trackIndex)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	bool tracksChanged = false;
	int audioIndex = -1;

	mAudioTracks = tracks;
	mAudioTrackIndex = trackIndex;
	audioIndex = GetAudioTrack();
	if (-1 != aamp->mCurrentAudioTrackIndex
			&& aamp->mCurrentAudioTrackIndex != audioIndex)
	{
		tracksChanged = true;
	}
	aamp->mCurrentAudioTrackIndex = audioIndex;

	if (tracksChanged)
	{
		aamp->NotifyAudioTracksChanged();
	}
}

/** TBD : Move to Dash Utils */
#define PRESELECTION_PROPERTY_TAG "Preselection"
#define ACCESSIBILITY_PROPERTY_TAG "Accessibility"

#define CHANNEL_PROPERTY_TAG "AudioChannelConfiguration"
#define CHANNEL_SCHEME_ID_TAG "urn:mpeg:mpegB:cicp:ChannelConfiguration"

#define ROLE_PROPERTY_TAG "Role"
#define ROLE_SCHEME_ID_TAG "urn:mpeg:dash:role:2011"

/** TBD : Move to Dash Utils */
/**
 * @brief Get the cannel number from preselection node
 * @param preselection ndoe as nodePtr
 * @return channel number
 */
static int getChannel(INode *nodePtr)
{
	int channel = 0;
	std::vector<INode*> childNodeList = nodePtr->GetNodes();
	for (auto &childNode : childNodeList)
	{
		const std::string& name = childNode->GetName();
		if (name == CHANNEL_PROPERTY_TAG ) 
		{
			if (childNode->HasAttribute("schemeIdUri")) 
			{
				if (childNode->GetAttributeValue("schemeIdUri") == CHANNEL_SCHEME_ID_TAG )
				{
					if (childNode->HasAttribute("value")) 
					{
						channel = std::stoi(childNode->GetAttributeValue("value"));
					}
				}
			}
		}
	}

	return channel;
}

/**
 * @fn getRole
 * 
 * @brief Get the role from preselection node
 * @param preselection ndoe as nodePtr
 * @return role
 */
static std::string getRole(INode *nodePtr)
{
	std::string role = "";
	std::vector<INode*> childNodeList = nodePtr->GetNodes();
	for (auto &childNode : childNodeList)
	{
		const std::string& name = childNode->GetName();
		if (name == ROLE_PROPERTY_TAG ) 
		{
			if (childNode->HasAttribute("schemeIdUri")) 
			{
				if (childNode->GetAttributeValue("schemeIdUri") == ROLE_SCHEME_ID_TAG )
				{
					if (childNode->HasAttribute("value")) 
					{
						role = childNode->GetAttributeValue("value");
					}
				}
			}
		}
	}
	return role;
}

void StreamAbstractionAAMP_MPD::ParseAvailablePreselections(IMPDElement *period, std::vector<AudioTrackInfo> & audioAC4Tracks)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	std::vector<INode*> childNodeList = period->GetAdditionalSubNodes();
	if (childNodeList.size() > 0)
	{
		std::string id ;
		std::string codec;
		std::string lang;
		std::string tag ;
		long bandwidth = 0;
		std::string role;
		int channel = 0;
		std::string label;
		std::string type = "audio";
		for (auto &childNode : childNodeList)
		{
			const std::string& name = childNode->GetName();
			if (name == PRESELECTION_PROPERTY_TAG ) 
			{
				/**
				 * <Preselection id="100" preselectionComponents="11" 
				 * tag="10" codecs="ac-4.02.01.01" audioSamplingRate="48000" lang="en">
				 */
				if (childNode->HasAttribute("id")) {
					id = childNode->GetAttributeValue("id");
				}
				if (childNode->HasAttribute("tag")) {
					tag = childNode->GetAttributeValue("tag");
				}
				if (childNode->HasAttribute("lang")) {
					lang = childNode->GetAttributeValue("lang");
				}
				if (childNode->HasAttribute("codecs")) {
					codec = childNode->GetAttributeValue("codecs");
				}
				if (childNode->HasAttribute("audioSamplingRate")) {
					bandwidth = std::stol(childNode->GetAttributeValue("audioSamplingRate"));
				}

				role = getRole (childNode);
				channel = getChannel(childNode);
				/** Preselection node is used for representing muxed audio tracks **/
				AAMPLOG_INFO("Preselection node found with tag %s language %s role %s id %s codec %s bandwidth %ld Channel %d ", 
				tag.c_str(), lang.c_str(), role.c_str(), id.c_str(), codec.c_str(), bandwidth, channel);
				audioAC4Tracks.push_back(AudioTrackInfo(tag, lang, role, id, codec, bandwidth, channel, true, true));
			}
		}
	}
}

/**
 * @brief Get the audio track information from all period
 *        updated member variable mAudioTracksAll
 * @return void
 */
void StreamAbstractionAAMP_MPD::PopulateTrackInfo(MediaType media, bool reset)
{
	//Clear the current track info , if any
	if (reset && media == eMEDIATYPE_AUDIO)
	{
		mAudioTracksAll.clear();
	}
	/**< Subtitle can be muxed with video adaptation also **/
	else if (reset && ((media == eMEDIATYPE_SUBTITLE) || (media == eMEDIATYPE_VIDEO)))
	{
		mTextTracksAll.clear();
	}
	std::vector<dash::mpd::IPeriod*>  ptrPeriods =  mpd->GetPeriods();
	for (auto &period : ptrPeriods)
	{
		AAMPLOG_TRACE("Traversing Period [%s] ", period->GetId().c_str());

		std::vector<dash::mpd::IAdaptationSet*> adaptationSets = period->GetAdaptationSets();
		uint32_t adaptationIndex = 0;
		for (auto &adaptationSet : adaptationSets)
		{
			AAMPLOG_TRACE("Adaptation Set Content type [%s] ", adaptationSet->GetContentType().c_str());
			if (IsContentType(adaptationSet, (MediaType)media))
			{
				ParseTrackInformation(adaptationSet, adaptationIndex, (MediaType)media,  mAudioTracksAll, mTextTracksAll);
			} // Audio Adaptation
			adaptationIndex++;
		} // Adaptation Loop
		{
			std::vector<AudioTrackInfo> ac4Tracks;
			ParseAvailablePreselections(period, ac4Tracks);
			mAudioTracksAll.insert(ac4Tracks.end(), ac4Tracks.begin(), ac4Tracks.end());
		}
	}//Period loop
	if (media == eMEDIATYPE_AUDIO)
	{
		std::sort(mAudioTracksAll.begin(), mAudioTracksAll.end());
		auto last = std::unique(mAudioTracksAll.begin(), mAudioTracksAll.end());
		// Resizing the vector so as to remove the undefined terms
		mAudioTracksAll.resize(std::distance(mAudioTracksAll.begin(), last));
	}
	else
	{
		std::sort(mTextTracksAll.begin(), mTextTracksAll.end());
		auto last = std::unique(mTextTracksAll.begin(), mTextTracksAll.end());
		// Resizing the vector so as to remove the undefined terms
		mTextTracksAll.resize(std::distance(mTextTracksAll.begin(), last));
	}
}

/**
 * @brief To set the audio tracks of current period
 */
std::vector<AudioTrackInfo>& StreamAbstractionAAMP_MPD::GetAvailableAudioTracks(bool allTrack)
{
	if (!allTrack)
	{
		return mAudioTracks;
	}
	else
	{
		if (aamp->IsLive() || (mAudioTracksAll.size() == 0))
		{
			/** Audio Track not populated yet**/ 
			PopulateTrackInfo(eMEDIATYPE_AUDIO, true);
		}
		return mAudioTracksAll;
	} 
}

/**
 * @brief To set the text tracks of current period
 *
 * @param[in] tracks - available text tracks in period
 * @param[in] trackIndex - index of current text track
 */
std::vector<TextTrackInfo>& StreamAbstractionAAMP_MPD::GetAvailableTextTracks(bool allTrack)
{
	if (!allTrack)
	{
		return mTextTracks;
	}
	else
	{
		if (aamp->IsLive() || (mTextTracksAll.size() == 0))
		{
			/** Text Track not populated yet**/ 
			PopulateTrackInfo(eMEDIATYPE_SUBTITLE, true);
			PopulateTrackInfo(eMEDIATYPE_VIDEO, false);
		}
		return mTextTracksAll;
	} 
}

void StreamAbstractionAAMP_MPD::SetTextTrackInfo(const std::vector<TextTrackInfo> &tracks, const std::string &trackIndex)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	bool tracksChanged = false;
	int textTrack = -1;

	mTextTracks = tracks;
	mTextTrackIndex = trackIndex;

	textTrack = GetTextTrack();
	if (-1 != aamp->mCurrentTextTrackIndex
			&& aamp->mCurrentTextTrackIndex != textTrack)
	{
		tracksChanged = true;
	}

	aamp->mCurrentTextTrackIndex = textTrack;

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
 * @return bool true if the params are matching
 */
bool StreamAbstractionAAMP_MPD::IsMatchingLanguageAndMimeType(MediaType type, std::string lang, IAdaptationSet *adaptationSet, int &representationIndex)
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	   bool ret = false;
	   std::string adapLang = GetLanguageForAdaptationSet(adaptationSet);
	   AAMPLOG_INFO("type %d inlang %s current lang %s", type, lang.c_str(), adapLang.c_str());
	   if (adapLang == lang)
	   {
			   PeriodElement periodElement(adaptationSet, NULL);
			   std::string adaptationMimeType = periodElement.GetMimeType();
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
							   PeriodElement periodElement(adaptationSet, rep);
							   std::string mimeType = periodElement.GetMimeType();
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
					   AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Found matching track[%d] with language:%s but not supported mimeType and thus disabled!!",
											   type, lang.c_str());
			   }
	   }
	   return ret;
}

double StreamAbstractionAAMP_MPD::GetEncoderDisplayLatency()
{
	/*
	a. If the ProducerReferenceTime element is present as defined in clause 4.X.3.2, then the
	i. WCA is the value of the @wallClockTime
	ii. PTA is the value of the @presentationTime
	iii. If the @inband attribute is set to TRUE, then it should parse the segments to continuously
	update PTA and WCA accordingly
	b. Else
	i. WCA is the value of the PeriodStart
	ii. PTA is the value of the @presentationTimeOffset
	c. Then the presentation latency PL of a presentation time PT presented at wall clock time WC is
	determinedas PL => (WC – WCA) - (PT – PTA)

	A segment has a presentation time PT => @t / @timescale (BEST: @PTS/@timescale)
	*/

	double encoderDisplayLatency  = 0;
	double WCA = 0;
	double PTA = 0;
	double WC = 0;
	double PT = 0;

	struct tm *lt = NULL;
	struct tm *gmt = NULL;
	time_t tt = 0;
	time_t tt_local = 0;
	time_t tt_utc = 0;

	tt       = NOW_SYSTEM_TS_MS/1000;//WC - Need display clock position
	lt       = localtime(&tt);
	tt_local = mktime(lt);
	gmt      = gmtime(&tt);
	gmt->tm_isdst = 0;
	tt_utc   = mktime(gmt);

	IProducerReferenceTime *producerReferenceTime = NULL;
	double presentationOffset = 0;
	uint32_t timeScale = 0;

	AAMPLOG_INFO("Current Index: %d Total Period: %d",mCurrentPeriodIdx, mpd->GetPeriods().size());

	if( mpd->GetPeriods().size())
	{
		IPeriod* tempPeriod = NULL;
		try {
			tempPeriod = mpd->GetPeriods().at(mCurrentPeriodIdx);

			if(tempPeriod && tempPeriod->GetAdaptationSets().size())
			{
				const std::vector<IAdaptationSet *> adaptationSets = tempPeriod->GetAdaptationSets();

				for(int j = 0; j < adaptationSets.size(); j++)
				{
					if( IsContentType(adaptationSets.at(j), eMEDIATYPE_VIDEO) )
					{
						producerReferenceTime = GetProducerReferenceTimeForAdaptationSet(adaptationSets.at(j));
						break;
					}
				}

				const ISegmentTemplate *representation = NULL;
				const ISegmentTemplate *adaptationSet = NULL;

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

				if( segmentTemplates.HasSegmentTemplate() )
				{
					std::string media = segmentTemplates.Getmedia();
					timeScale = segmentTemplates.GetTimescale();
					if(!timeScale)
					{
						timeScale = aamp->GetVidTimeScale();
					}
					AAMPLOG_TRACE("timeScale: %" PRIu32 "", timeScale);

					presentationOffset = (double) segmentTemplates.GetPresentationTimeOffset();
					const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
					if (segmentTimeline)
					{
						std::vector<ITimeline*> vec  = segmentTimeline->GetTimelines();
						if (!vec.empty())
						{
							ITimeline* timeline = vec.back();
							uint64_t startTime = 0;
							uint32_t duration = 0;
							uint32_t repeatCount = 0;

							startTime = timeline->GetStartTime();
							duration = timeline->GetDuration();
							repeatCount = timeline->GetRepeatCount();

							AAMPLOG_TRACE("startTime: %" PRIu64 " duration: %" PRIu32 " repeatCount: %" PRIu32 "", timeScale,duration,repeatCount);

							if(timeScale)
								PT = (double)(startTime+((uint64_t)repeatCount*duration))/timeScale ;
							else
								AAMPLOG_WARN("Empty timeScale !!!");
						}
					}
				}

				if(producerReferenceTime)
				{
					std::string id = "";
					std::string type = "";
					std::string wallClockTime = "";
					std::string presentationTimeOffset = "";
					std::string inband = "";
					long wTime = 0;

					map<string, string> attributeMap = producerReferenceTime->GetRawAttributes();
					map<string, string>::iterator pos = attributeMap.begin();
					pos = attributeMap.find("id");
					if(pos != attributeMap.end())
					{
						id = pos->second;
						if(!id.empty())
						{
							AAMPLOG_TRACE("ProducerReferenceTime@id [%s]", id.c_str());
						}
					}
					pos = attributeMap.find("type");
					if(pos != attributeMap.end())
					{
						type = pos->second;
						if(!type.empty())
						{
							AAMPLOG_TRACE("ProducerReferenceTime@type [%s]", type.c_str());
						}
					}
					pos = attributeMap.find("wallClockTime");
					if(pos != attributeMap.end())
					{
						wallClockTime = pos->second;
						if(!wallClockTime.empty())
						{
							AAMPLOG_TRACE("ProducerReferenceTime@wallClockTime [%s]", wallClockTime.c_str());

							std::tm tmTime;
							const char* format = "%Y-%m-%dT%H:%M:%S.%f%Z";
							char out_buffer[ 80 ];
							memset(&tmTime, 0, sizeof(tmTime));
							strptime(wallClockTime.c_str(), format, &tmTime);
							wTime = mktime(&tmTime);

							AAMPLOG_TRACE("ProducerReferenceTime@wallClockTime [%ld] UTCTime [%ld]",wTime, aamp->GetUtcTime());

							/* Convert the time back to a string. */
							strftime( out_buffer, 80, "That's %D (a %A), at %T",localtime (&wTime) );
							AAMPLOG_TRACE( "%s", out_buffer );
							WCA = (double)wTime ;
						}
					}
					pos = attributeMap.find("presentationTime");
					if(pos != attributeMap.end())
					{
						presentationTimeOffset = pos->second;
						if(!presentationTimeOffset.empty())
						{
							if(timeScale != 0)
							{
								PTA = ((double) std::stoll(presentationTimeOffset))/timeScale;
								AAMPLOG_TRACE("ProducerReferenceTime@presentationTime [%s] PTA [%lf]", presentationTimeOffset.c_str(), PTA);
							}
						}
					}
					pos = attributeMap.find("inband");
					if(pos != attributeMap.end())
					{
						inband = pos->second;
						if(!inband.empty())
						{
							AAMPLOG_TRACE("ProducerReferenceTime@inband [%d]", atoi(inband.c_str()));
						}
					}
				}
				else
				{
					AAMPLOG_WARN("ProducerReferenceTime Not Found for mCurrentPeriodIdx = [%d]", mCurrentPeriodIdx);
#if 0 //FIX-ME - Handle when ProducerReferenceTime element is not available

					//Check more for behavior here
					double periodStartTime = 0;
					periodStartTime =  GetPeriodStartTime(mpd, mCurrentPeriodIdx);
					AAMPLOG_TRACE("mCurrentPeriodIdx=%d periodStartTime=%lf",mCurrentPeriodIdx,periodStartTime);
					WCA =  periodStartTime;
					PTA = presentationOffset;
#endif
				}

				double wc_diff = tt_utc-WCA;
				double pt_diff  = PT-PTA;
				encoderDisplayLatency = (wc_diff - pt_diff);

				AAMPLOG_INFO("tt_utc [%lf] WCA [%lf] PT [%lf] PTA [%lf] tt_utc-WCA [%lf] PT-PTA [%lf] encoderDisplayLatency [%lf]", (double)tt_utc, WCA, PT, PTA, wc_diff, pt_diff, encoderDisplayLatency);
			}
		} catch (const std::out_of_range& oor) {
			AAMPLOG_WARN("mCurrentPeriodIdx: %d mpd->GetPeriods().size(): %d Out of Range error: %s", mCurrentPeriodIdx, mpd->GetPeriods().size(), oor.what() );
		}
	}

	return encoderDisplayLatency;
}

/**
 * @brief Latency monitor thread
 * @param arg Pointer to FragmentCollector
 * @retval NULL
 */
static void *LatencyMonitor(void *arg)
{
    FN_TRACE_F_MPD( __FUNCTION__ );
    StreamAbstractionAAMP_MPD *stAbsAAMP_MPD = (StreamAbstractionAAMP_MPD *)arg;
    if(aamp_pthread_setname(pthread_self(), "aampLatencyMonitor"))
    {
        AAMPLOG_WARN("aamp_pthread_setname failed");
    }
    AAMPLOG_WARN("LatencyMonitor -> Invoke MonitorLatency() ");
    stAbsAAMP_MPD->MonitorLatency();
    return NULL;
}

/**
 * @brief Starts Latency monitor loop
 */
void StreamAbstractionAAMP_MPD::StartLatencyMonitorThread()
{
    FN_TRACE_F_MPD( __FUNCTION__ );
    assert(!latencyMonitorThreadStarted);
    if (0 == pthread_create(&latencyMonitorThreadID, NULL, &LatencyMonitor, this))
    {
        latencyMonitorThreadStarted = true;
        AAMPLOG_WARN("Latency monitor thread started");
    }
    else
    {
        AAMPLOG_WARN("Failed to create LatencyMonitor thread");
    }
}

/**
 * @brief Monitor Live End Latency and Encoder Display Latency
 */
void StreamAbstractionAAMP_MPD::MonitorLatency()
{
	FN_TRACE_F_MPD( __FUNCTION__ );
	int latencyMonitorDelay = 0;
	int latencyMonitorInterval = 0;
	GETCONFIGVALUE(eAAMPConfig_LatencyMonitorDelay,latencyMonitorDelay);
	GETCONFIGVALUE(eAAMPConfig_LatencyMonitorInterval,latencyMonitorInterval);

	assert(latencyMonitorDelay >= latencyMonitorInterval);

	AAMPLOG_TRACE("latencyMonitorDelay %d latencyMonitorInterval=%d", latencyMonitorDelay,latencyMonitorInterval );
	unsigned int latencyMontiorScheduleTime = latencyMonitorDelay - latencyMonitorInterval;
	bool keepRunning = false;
	if(aamp->DownloadsAreEnabled())
	{
		AAMPLOG_TRACE("latencyMontiorScheduleTime %d", latencyMontiorScheduleTime );
		aamp->InterruptableMsSleep(latencyMontiorScheduleTime *1000);
		keepRunning = true;
	}
	AAMPLOG_TRACE("keepRunning : %d", keepRunning);
	int monitorInterval = latencyMonitorInterval  * 1000;
	AAMPLOG_INFO( "Speed correction state:%d", aamp->GetLLDashAdjustSpeed());

	aamp->SetLLDashCurrentPlayBackRate(AAMP_NORMAL_PLAY_RATE);

	while(keepRunning)
	{
		aamp->InterruptableMsSleep(monitorInterval);
		if (aamp->DownloadsAreEnabled())
		{
	
			double playRate = aamp->GetLLDashCurrentPlayBackRate();
			if( aamp->GetPositionMs() > aamp->DurationFromStartOfPlaybackMs() )
			{
				AAMPLOG_WARN("current position[%lld] must be less than Duration From Start Of Playback[%lld]!!!!:",aamp->GetPositionMs(), aamp->DurationFromStartOfPlaybackMs());
			}
			else
			{
				monitorInterval = latencyMonitorInterval  * 1000;
				AampLLDashServiceData *pAampLLDashServiceData = NULL;
				pAampLLDashServiceData = aamp->GetLLDashServiceData();
				if( NULL != pAampLLDashServiceData )
				{
					assert(pAampLLDashServiceData->minLatency != 0 );
					assert(pAampLLDashServiceData->minLatency <= pAampLLDashServiceData->targetLatency);
					assert(pAampLLDashServiceData->targetLatency !=0 );
					assert(pAampLLDashServiceData->maxLatency !=0 );
					assert(pAampLLDashServiceData->maxLatency >= pAampLLDashServiceData->targetLatency);

					long InitialLatencyOffset =  ( aamp->GetDurationMs() - ( (long long) (aamp->mLLActualOffset*1000)));
					long PlayBackLatency = ((aamp->DurationFromStartOfPlaybackMs()) - aamp->GetPositionMs() );
					long TimeOffsetSeekLatency = (long)(((pAampLLDashServiceData->fragmentDuration - pAampLLDashServiceData->availabilityTimeOffset))*1000);
					long currentLatency = ((InitialLatencyOffset+PlayBackLatency)-TimeOffsetSeekLatency);

					AAMPLOG_INFO("currentDur = %lld actualOffset=%ld DurationFromStart=%lld Position=%lld,seekLLValue=%ld",aamp->GetDurationMs(),
									(long long) (aamp->mLLActualOffset*1000), aamp->DurationFromStartOfPlaybackMs(),aamp->GetPositionMs(), TimeOffsetSeekLatency);
					AAMPLOG_INFO("LiveLatency=%ld currentPlayRate=%lf",currentLatency, playRate);

#if 0
					long encoderDisplayLatency = 0;
					encoderDisplayLatency = (long)( GetEncoderDisplayLatency() * 1000)+currentLatency;
					AAMPLOG_INFO("Encoder Display Latency=%ld", encoderDisplayLatency);
#endif
					//The minPlayback rate should be only <= AAMP_NORMAL_PLAY_RATE-0.20??
					//The MaxPlayBack rate should be only >= AAMP_NORMAL_PLAY_RATE+0.20??
					//Do we need to validate above?
					if(ISCONFIGSET(eAAMPConfig_EnableLowLatencyCorrection) && 
						pAampLLDashServiceData->minPlaybackRate !=0 && 
						pAampLLDashServiceData->minPlaybackRate < pAampLLDashServiceData->maxPlaybackRate &&
						pAampLLDashServiceData->minPlaybackRate < AAMP_NORMAL_PLAY_RATE && 
						pAampLLDashServiceData->maxPlaybackRate !=0 && 
						pAampLLDashServiceData->maxPlaybackRate > pAampLLDashServiceData->minPlaybackRate &&
						pAampLLDashServiceData->maxPlaybackRate > AAMP_NORMAL_PLAY_RATE)
					{
						if (currentLatency < (long)pAampLLDashServiceData->minLatency)
						{
							//Yellow state(the latency is within range but less than mimium latency)
							latencyStatus = LATENCY_STATUS_MIN;
							AAMPLOG_INFO("latencyStatus = LATENCY_STATUS_MIN(%d)",latencyStatus);
							playRate = pAampLLDashServiceData->minPlaybackRate;
						}
						else if ( ( currentLatency >= (long) pAampLLDashServiceData->minLatency ) &&
							(  currentLatency <= (long)pAampLLDashServiceData->targetLatency) )
						{
							//Yellow state(the latency is within range but less than target latency but greater than minimum latency)
							latencyStatus = LATENCY_STATUS_THRESHOLD_MIN;
							AAMPLOG_INFO("latencyStatus = LATENCY_STATUS_THRESHOLD_MIN(%d)",latencyStatus);
							playRate = AAMP_NORMAL_LL_PLAY_RATE;
						}
						else if ( currentLatency == (long)pAampLLDashServiceData->targetLatency )
						{
							//green state(No correction is requried. set the playrate to normal, the latency is equal to given latency from mpd)
							latencyStatus = LATENCY_STATUS_THRESHOLD;
							AAMPLOG_INFO("latencyStatus = LATENCY_STATUS_THRESHOLD(%d)",latencyStatus);
							playRate = AAMP_NORMAL_LL_PLAY_RATE;
						}
						else if ( ( currentLatency >= (long)pAampLLDashServiceData->targetLatency ) &&
							( currentLatency <= (long)pAampLLDashServiceData->maxLatency )  )
						{
							//Red state(The latency is more that target latency but less than maximum latency)
							latencyStatus = LATENCY_STATUS_THRESHOLD_MAX;
							AAMPLOG_INFO("latencyStatus = LATENCY_STATUS_THRESHOLD_MAX(%d)",latencyStatus);
							playRate = AAMP_NORMAL_LL_PLAY_RATE;
						}
						else if (currentLatency > (long)pAampLLDashServiceData->maxLatency)
						{
							//Red state(The latency is more than maximum latency)
							latencyStatus = LATENCY_STATUS_MAX; //Red state
							AAMPLOG_INFO("latencyStatus = LATENCY_STATUS_MAX(%d)",latencyStatus);
							playRate = pAampLLDashServiceData->maxPlaybackRate;
						}
						else //must not hit here
						{
							latencyStatus = LATENCY_STATUS_UNKNOWN; //Red state
							AAMPLOG_WARN("latencyStatus = LATENCY_STATUS_UNKNOWN(%d)",latencyStatus);
						}

						if ( playRate != aamp->GetLLDashCurrentPlayBackRate() )
						{
							bool rateCorrected=false;

							switch(latencyStatus)
							{
								case LATENCY_STATUS_THRESHOLD_MIN:
								{
									if ( pAampLLDashServiceData->maxPlaybackRate == aamp->GetLLDashCurrentPlayBackRate() )
									{
										if(false == aamp->mStreamSink->SetPlayBackRate(playRate))
										{
											AAMPLOG_WARN("[LATENCY_STATUS_%d] SetPlayBackRate: failed, rate:%f", latencyStatus,playRate);
										}
										else
										{
											rateCorrected = true;
											AAMPLOG_TRACE("[LATENCY_STATUS_%d] SetPlayBackRate: success", latencyStatus);
										}
									}
								}
								break;
								case LATENCY_STATUS_THRESHOLD_MAX:
								{
									if ( pAampLLDashServiceData->minPlaybackRate == aamp->GetLLDashCurrentPlayBackRate() )
									{
										if(false == aamp->mStreamSink->SetPlayBackRate(playRate))
										{
											AAMPLOG_WARN("[LATENCY_STATUS_%d] SetPlayBackRate: failed, rate:%f", latencyStatus,playRate);
										}
										else
										{
											rateCorrected = true;
											AAMPLOG_TRACE("[LATENCY_STATUS_%d] SetPlayBackRate: success", latencyStatus);
										}
									}
								}
								break;
								case LATENCY_STATUS_MIN:
								case LATENCY_STATUS_MAX:
								{
									if(false == aamp->mStreamSink->SetPlayBackRate(playRate))
									{
										AAMPLOG_WARN("[LATENCY_STATUS_%d] SetPlayBackRate: failed, rate:%f", latencyStatus,playRate);
									}
									else
									{
										rateCorrected = true;
										AAMPLOG_TRACE("[LATENCY_STATUS_%d] SetPlayBackRate: success", latencyStatus);
									}
								}
								break;
								case LATENCY_STATUS_THRESHOLD:
								break;
								default:
								break;
							}

							if ( rateCorrected )
							{
								aamp->SetLLDashCurrentPlayBackRate(playRate);
							}
						}
					}
				}
				else
				{
					AAMPLOG_WARN("ServiceDescription Element is empty");
				}
			}
		}
		else
		{
			AAMPLOG_WARN("Stopping Thread");
			keepRunning = false;

		}
	}
	AAMPLOG_WARN("Thread Done");
}

/**
 * @brief Check if LLProfile is Available in MPD
 * @retval bool true if LL profile. Else false
 */
bool StreamAbstractionAAMP_MPD::CheckLLProfileAvailable(IMPD *mpd)
{
	std::vector<std::string> profiles;
	profiles = this->mpd->GetProfiles();
	size_t numOfProfiles = profiles.size();
	for (int iProfileCnt = 0; iProfileCnt < numOfProfiles; iProfileCnt++)
	{
		std::string profile = profiles.at(iProfileCnt);
		if(!strcmp(LL_DASH_SERVICE_PROFILE , profile.c_str()))
		{
			return true;
		}
	}
	return false;
}

/**
 * @brief Check if ProducerReferenceTime UTCTime type Matches with Other UTCtime type declaration
 * @retval bool true if Match exist. Else false
 */
bool StreamAbstractionAAMP_MPD::CheckProducerReferenceTimeUTCTimeMatch(IProducerReferenceTime *pRT)
{
    bool bMatch = false;
    //1. Check if UTC Time provider in <ProducerReferenceTime> element is same as stored for MPD already

    if(pRT->GetUTCTimings().size())
    {
        IUTCTiming *utcTiming= pRT->GetUTCTimings().at(0);

        // Some timeline may not have attribute for target latency , check it .
        map<string, string> attributeMapTiming = utcTiming->GetRawAttributes();

        if(attributeMapTiming.find("schemeIdUri") == attributeMapTiming.end())
        {
            AAMPLOG_WARN("UTCTiming@schemeIdUri attribute not available");
        }
        else
        {
            UtcTiming utcTimingType = eUTC_HTTP_INVALID;
            AAMPLOG_TRACE("UTCTiming@schemeIdUri: %s", utcTiming->GetSchemeIdUri().c_str());

            if(!strcmp(URN_UTC_HTTP_XSDATE , utcTiming->GetSchemeIdUri().c_str()))
            {
                utcTimingType = eUTC_HTTP_XSDATE;
            }
            else if(!strcmp(URN_UTC_HTTP_ISO , utcTiming->GetSchemeIdUri().c_str()))
            {
                utcTimingType = eUTC_HTTP_ISO;
            }
            else if(!strcmp(URN_UTC_HTTP_NTP , utcTiming->GetSchemeIdUri().c_str()))
            {
                utcTimingType = eUTC_HTTP_NTP;
            }
            else
            {
                AAMPLOG_WARN("UTCTiming@schemeIdUri Value not proper");
            }
            //Check if it matches with MPD provided UTC timing
            if(utcTimingType == aamp->GetLLDashServiceData()->utcTiming)
            {
                bMatch = true;
            }

            //Adaptation set timing didnt match
            if(!bMatch)
            {
                AAMPLOG_WARN("UTCTiming did not Match. !!");
            }
        }
    }
    return bMatch;
}

/**
 * @brief Print ProducerReferenceTime parsed data
 * @retval void
 */
void StreamAbstractionAAMP_MPD::PrintProducerReferenceTimeAtrributes(IProducerReferenceTime *pRT)
{
    AAMPLOG_TRACE("Id: %s", pRT->GetId().c_str());
    AAMPLOG_TRACE("Type: %s", pRT->GetType().c_str());
    AAMPLOG_TRACE("WallClockTime %s" , pRT->GetWallClockTime().c_str());
    AAMPLOG_TRACE("PresentationTime : %d" , pRT->GetPresentationTime());
    AAMPLOG_TRACE("Inband : %s" , pRT->GetInband()?"true":"false");
}

/**
 * @brief Check if ProducerReferenceTime available in AdaptationSet
 * @retval IProducerReferenceTime* Porinter to parsed ProducerReferenceTime data
 */
IProducerReferenceTime *StreamAbstractionAAMP_MPD::GetProducerReferenceTimeForAdaptationSet(IAdaptationSet *adaptationSet)
{
    IProducerReferenceTime *pRT = NULL;

    if(adaptationSet != NULL)
    {
        const std::vector<IProducerReferenceTime *> producerReferenceTime = adaptationSet->GetProducerReferenceTime();

        if(!producerReferenceTime.size())
            return pRT;

        pRT = producerReferenceTime.at(0);
    }
    else
    {
        AAMPLOG_WARN("adaptationSet  is null");  //CID:85233 - Null Returns
    }
    return pRT;
}

/**
 * @brief EnableAndSetLiveOffsetForLLDashPlayback based on playerconfig/LL-dash 
 * profile/availabilityTimeOffset and set the LiveOffset
 */
AAMPStatusType  StreamAbstractionAAMP_MPD::EnableAndSetLiveOffsetForLLDashPlayback(const MPD* mpd)
{
 	 AAMPStatusType ret = eAAMPSTATUS_OK;
	/*LL DASH VERIFICATION START*/
	//Check if LLD requested
	if (ISCONFIGSET(eAAMPConfig_EnableLowLatencyDash))
	{
		AampLLDashServiceData stLLServiceData;
		memset(&stLLServiceData,0,sizeof(AampLLDashServiceData));
		double currentOffset = 0;
		int maxLatency=0,minLatency=0,TargetLatency=0;

		TuneType tuneType = aamp->GetTuneType();

		if( ( eTUNETYPE_SEEK != tuneType   ) &&
			( eTUNETYPE_NEW_SEEK != tuneType ) &&
			( eTUNETYPE_NEW_END != tuneType ) &&
			( eTUNETYPE_SEEKTOEND != tuneType ) )
		{
			if( GetLowLatencyParams((MPD*)this->mpd,stLLServiceData) &&	stLLServiceData.availabilityTimeComplete == false )
			{
				stLLServiceData.lowLatencyMode = true;
				if( ISCONFIGSET( eAAMPConfig_EnableLowLatencyCorrection ) )
				{
						aamp->SetLLDashAdjustSpeed(true);
						AAMPLOG_WARN("LL Dash speed correction enabled");
				}
				else
				{
						aamp->SetLLDashAdjustSpeed(false);
						AAMPLOG_WARN("LL Dash speed correction disabled");
				}
				AAMPLOG_WARN("StreamAbstractionAAMP_MPD: LL-DASH playback enabled availabilityTimeOffset=%lf,fragmentDuration=%lf",
										stLLServiceData.availabilityTimeOffset,stLLServiceData.fragmentDuration);
			}
			else
			{
				stLLServiceData.lowLatencyMode = false;
				aamp->SetLLDashAdjustSpeed(false);
				AAMPLOG_TRACE("LL-DASH Mode Disabled. Not a LL-DASH Stream");
			}
		}
		else
		{
			stLLServiceData.lowLatencyMode = false;
			aamp->SetLLDashAdjustSpeed(false);
			AAMPLOG_INFO("StreamAbstractionAAMP_MPD: tune type %d not support LL-DASH",tuneType);
		}
		
		//If LLD enabled then check servicedescription requirements
		if( stLLServiceData.lowLatencyMode )
		{
			if(!ParseMPDLLData((MPD*)this->mpd, stLLServiceData))
			{
				ret = eAAMPSTATUS_MANIFEST_PARSE_ERROR;
				return ret;
			}

			if ( 0 == stLLServiceData.maxPlaybackRate )
			{
				stLLServiceData.maxPlaybackRate = DEFAULT_MAX_RATE_CORRECTION_SPEED;
			}

			if ( 0 == stLLServiceData.minPlaybackRate )
			{
				stLLServiceData.minPlaybackRate = DEFAULT_MIN_RATE_CORRECTION_SPEED;
			}

			GETCONFIGVALUE(eAAMPConfig_LLMinLatency,minLatency);
			GETCONFIGVALUE(eAAMPConfig_LLTargetLatency,TargetLatency);
			GETCONFIGVALUE(eAAMPConfig_LLMaxLatency,maxLatency);
			GETCONFIGVALUE(eAAMPConfig_LiveOffset,currentOffset);
			AAMPLOG_INFO("StreamAbstractionAAMP_MPD: Current Offset(s): %ld",(long)currentOffset);

			if(	stLLServiceData.minLatency <= 0)
			{
				if(minLatency <= 0 || minLatency > TargetLatency )
				{
					stLLServiceData.minLatency = DEFAULT_MIN_LOW_LATENCY*1000;
				}
				else
				{
					stLLServiceData.minLatency = minLatency*1000;
				}
			}
			if(	stLLServiceData.maxLatency <= 0 ||
				stLLServiceData.maxLatency < stLLServiceData.minLatency )
			{
				if( maxLatency <=0 || maxLatency < minLatency )
				{
					stLLServiceData.maxLatency = DEFAULT_MAX_LOW_LATENCY*1000;
					stLLServiceData.minLatency = DEFAULT_MIN_LOW_LATENCY*1000;
				}
				else
				{
					stLLServiceData.maxLatency = maxLatency*1000;
				}
			}
			if(	stLLServiceData.targetLatency <= 0 ||
				stLLServiceData.targetLatency < stLLServiceData.minLatency ||
				stLLServiceData.targetLatency > stLLServiceData.maxLatency )

			{
				if(TargetLatency <=0 || TargetLatency < minLatency || TargetLatency > maxLatency )
				{
					stLLServiceData.targetLatency = DEFAULT_TARGET_LOW_LATENCY*1000;
					stLLServiceData.maxLatency = DEFAULT_MAX_LOW_LATENCY*1000;
					stLLServiceData.minLatency = DEFAULT_MIN_LOW_LATENCY*1000;
				}
				else
				{
					stLLServiceData.targetLatency = TargetLatency*1000;
				}
			}
			double latencyOffsetMin = stLLServiceData.minLatency/1000;
			double latencyOffsetMax = stLLServiceData.maxLatency/1000;
			double TargetLatencyWrtWallClockTime = stLLServiceData.targetLatency/1000;
			AAMPLOG_WARN("StreamAbstractionAAMP_MPD:[LL-Dash] Min Latency: %ld Max Latency: %ld Target Latency: %ld",(long)latencyOffsetMin,(long)latencyOffsetMax,(long)TargetLatency);

			//Ignore Low latency setting
			if((AAMP_DEFAULT_SETTING != GETCONFIGOWNER(eAAMPConfig_LiveOffset)) && (currentOffset > latencyOffsetMax))
			{
				AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Switch off LL mode: App requested currentOffset > latencyOffsetMax");
				stLLServiceData.lowLatencyMode = false;
			}
			else
			{
				
				string tempStr = mpd->GetMaxSegmentDuration();
				double maxFragmentDuartion = 0;
				if(!tempStr.empty())
				{
						maxFragmentDuartion = ParseISO8601Duration( tempStr.c_str() )/1000;
				}
				double latencyOffset = 0;
				if(!aamp->GetLowLatencyServiceConfigured())
				{
					if(maxFragmentDuartion > 0 )
					{
						latencyOffset = maxFragmentDuartion+(maxFragmentDuartion-stLLServiceData.availabilityTimeOffset);
					}
					else if(stLLServiceData.fragmentDuration > 0)
					{
						latencyOffset = stLLServiceData.fragmentDuration+(stLLServiceData.fragmentDuration-stLLServiceData.availabilityTimeOffset);
					}
					else
					{
						latencyOffset =(double)((double) DEFAULT_MIN_LOW_LATENCY);
					}
					
					if(latencyOffset > DEFAULT_TARGET_LOW_LATENCY)
					{
						latencyOffset = DEFAULT_MIN_LOW_LATENCY;
					}
					
					//Override Latency offset with Min Value if config enabled
					AAMPLOG_WARN("StreamAbstractionAAMP_MPD: currentOffset:%lf LL-DASH offset(s): %lf",currentOffset,latencyOffset);
					SETCONFIGVALUE(AAMP_STREAM_SETTING,eAAMPConfig_LiveOffset,latencyOffset);
					aamp->UpdateLiveOffset();

					//Set LL Dash Service Configuration Data in Pvt AAMP instance
					aamp->SetLLDashServiceData(stLLServiceData);
					aamp->SetLowLatencyServiceConfigured(true);
				}
			}
		}
	}
	else
	{
		AAMPLOG_INFO("StreamAbstractionAAMP_MPD: LL-DASH playback disabled in config");
	}
	return ret;
}
/**
 * @brief get Low Latency Parameters from segement template and timeline
 */
bool StreamAbstractionAAMP_MPD::GetLowLatencyParams(const MPD* mpd,AampLLDashServiceData &LLDashData)
{
	bool isSuccess=false;
	if(mpd != NULL)
	{
		size_t numPeriods = mpd->GetPeriods().size();

		for (unsigned iPeriod = 0; iPeriod < numPeriods; iPeriod++)
		{
			IPeriod *period = mpd->GetPeriods().at(iPeriod);
			if(IsEmptyPeriod(period, mIsFogTSB))
			{
				// Empty Period . Ignore processing, continue to next.
				continue;
			}
			if(NULL != period )
			{
				const std::vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();
				if (adaptationSets.size() > 0)
				{
					IAdaptationSet * pFirstAdaptation = adaptationSets.at(0);
					if ( NULL != pFirstAdaptation )
					{
						ISegmentTemplate *pSegmentTemplate = NULL;
						pSegmentTemplate = pFirstAdaptation->GetSegmentTemplate();
						if( NULL != pSegmentTemplate )
						{
							map<string, string> attributeMap = pSegmentTemplate->GetRawAttributes();
							if(attributeMap.find("availabilityTimeOffset") == attributeMap.end())
    						{
        						AAMPLOG_WARN("Latency availabilityTimeOffset attribute not available");
    						}
							else
							{
								LLDashData.availabilityTimeOffset = pSegmentTemplate->GetAvailabilityTimeOffset();
								LLDashData.availabilityTimeComplete = pSegmentTemplate->GetAvailabilityTimeComplete();
								AAMPLOG_INFO("AvailabilityTimeOffset=%lf AvailabilityTimeComplete=%d",
												pSegmentTemplate->GetAvailabilityTimeOffset(),pSegmentTemplate->GetAvailabilityTimeComplete());
								isSuccess=true;
								if( isSuccess )
								{
									uint32_t timeScale=0;
									uint32_t duration =0;
									const ISegmentTimeline *segmentTimeline = pSegmentTemplate->GetSegmentTimeline();
									if (segmentTimeline)
									{
										timeScale = pSegmentTemplate->GetTimescale();
										std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
										ITimeline *timeline = timelines.at(0);
										duration = timeline->GetDuration();
										LLDashData.fragmentDuration = ComputeFragmentDuration(duration,timeScale);
										LLDashData.isSegTimeLineBased = true;
									}
									else
									{
										timeScale = pSegmentTemplate->GetTimescale();
										duration = pSegmentTemplate->GetDuration();
										LLDashData.fragmentDuration = ComputeFragmentDuration(duration,timeScale);
										LLDashData.isSegTimeLineBased = false;
									}
									AAMPLOG_INFO("timeScale=%u duration=%u fragmentDuration=%lf",
												timeScale,duration,LLDashData.fragmentDuration);
								}
								break;
							}
						}
						else
						{
							AAMPLOG_ERR("NULL segmenttemplate"); 	
						}
					}
					else
					{
						AAMPLOG_INFO("NULL adaptationSets");
					}
				}
				else
				{
					AAMPLOG_WARN("empty adaptationSets");
				}
			}
			else
			{
				AAMPLOG_WARN("empty period ");
			}
		}
	}
	else
	{
		AAMPLOG_WARN("NULL mpd");
	}
	return isSuccess;
}
/**
 * @brief Parse MPD LL elements
 */
bool StreamAbstractionAAMP_MPD::ParseMPDLLData(MPD* mpd, AampLLDashServiceData &stAampLLDashServiceData)
{
    bool ret = false;
	//check if <ServiceDescription> available->raise error if not
    if(!mpd->GetServiceDescriptions().size())
    {
       AAMPLOG_TRACE("GetServiceDescriptions not avaialble");
    }
	else
	{
    	//check if <scope> element is available in <ServiceDescription> element->raise error if not
    	if(!mpd->GetServiceDescriptions().at(0)->GetScopes().size())
    	{
        	AAMPLOG_TRACE("Scope element not available");
       	}
    	//check if <Latency> element is availablein <ServiceDescription> element->raise error if not
    	if(!mpd->GetServiceDescriptions().at(0)->GetLatencys().size())
    	{
        	AAMPLOG_TRACE("Latency element not available");
		}
		else
		{
    		//check if attribute @target is available in <latency> element->raise error if not
    		ILatency *latency= mpd->GetServiceDescriptions().at(0)->GetLatencys().at(0);

    		// Some timeline may not have attribute for target latency , check it .
    		map<string, string> attributeMap = latency->GetRawAttributes();

    		if(attributeMap.find("target") == attributeMap.end())
    		{
        		AAMPLOG_TRACE("target Latency attribute not available");
    		}
			else
			{
				stAampLLDashServiceData.targetLatency = latency->GetTarget();
				AAMPLOG_INFO("targetLatency: %d", stAampLLDashServiceData.targetLatency);
			}
			
    		//check if attribute @max or @min is available in <Latency> element->raise info if not
    		if(attributeMap.find("max") == attributeMap.end())
    		{
        		AAMPLOG_TRACE("Latency max attribute not available");
			}	
    		else
    		{
        		stAampLLDashServiceData.maxLatency = latency->GetMax();
				AAMPLOG_INFO("maxLatency: %d", stAampLLDashServiceData.maxLatency);
    		}
    		if(attributeMap.find("min") == attributeMap.end())
    		{
        		AAMPLOG_TRACE("Latency min attribute not available");
    		}
    		else
    		{
        		stAampLLDashServiceData.minLatency = latency->GetMin();
        		AAMPLOG_INFO("minLatency: %d", stAampLLDashServiceData.minLatency);
    		}
		}
	
    	if(!mpd->GetServiceDescriptions().at(0)->GetPlaybackRates().size())
    	{
        	AAMPLOG_TRACE("Play Rate element not available");
    	}
    	else
    	{
    		//check if attribute @max or @min is available in <PlaybackRate> element->raise info if not
        	IPlaybackRate *playbackRate= mpd->GetServiceDescriptions().at(0)->GetPlaybackRates().at(0);

			// Some timeline may not have attribute for target latency , check it .
			map<string, string> attributeMapRate = playbackRate->GetRawAttributes();

			if(attributeMapRate.find("max") == attributeMapRate.end())
			{
				AAMPLOG_TRACE("Latency max attribute not available");
				stAampLLDashServiceData.maxPlaybackRate = DEFAULT_MAX_RATE_CORRECTION_SPEED;
			}
			else
			{
				stAampLLDashServiceData.maxPlaybackRate = playbackRate->GetMax();
				AAMPLOG_INFO("maxPlaybackRate: %0.2f",stAampLLDashServiceData.maxPlaybackRate);
			}
			if(attributeMapRate.find("min") == attributeMapRate.end())
			{
				AAMPLOG_TRACE("Latency min attribute not available");
				stAampLLDashServiceData.minPlaybackRate = DEFAULT_MIN_RATE_CORRECTION_SPEED;
			}
			else
			{
				stAampLLDashServiceData.minPlaybackRate = playbackRate->GetMin();
				AAMPLOG_INFO("minPlatbackRate: %0.2f", stAampLLDashServiceData.minPlaybackRate);
			}
		}
	}
    //check if UTCTiming element available
    if(!mpd->GetUTCTimings().size())
    {
        AAMPLOG_WARN("UTCTiming element not available");
    }
	else
	{

		//check if attribute @max or @min is available in <PlaybackRate> element->raise info if not
		IUTCTiming *utcTiming= mpd->GetUTCTimings().at(0);

		// Some timeline may not have attribute for target latency , check it .
		map<string, string> attributeMapTiming = utcTiming->GetRawAttributes();

		if(attributeMapTiming.find("schemeIdUri") == attributeMapTiming.end())
		{
			AAMPLOG_WARN("UTCTiming@schemeIdUri attribute not available");
		}
		else
		{
			AAMPLOG_TRACE("UTCTiming@schemeIdUri: %s", utcTiming->GetSchemeIdUri().c_str());
			if(!strcmp(URN_UTC_HTTP_XSDATE , utcTiming->GetSchemeIdUri().c_str()))
			{
				stAampLLDashServiceData.utcTiming = eUTC_HTTP_XSDATE;
			}
			else if(!strcmp(URN_UTC_HTTP_ISO , utcTiming->GetSchemeIdUri().c_str()))
			{
				stAampLLDashServiceData.utcTiming = eUTC_HTTP_ISO;
			}
			else if(!strcmp(URN_UTC_HTTP_NTP , utcTiming->GetSchemeIdUri().c_str()))
			{
				stAampLLDashServiceData.utcTiming = eUTC_HTTP_NTP;
			}
			else
			{
				stAampLLDashServiceData.utcTiming = eUTC_HTTP_INVALID;
				AAMPLOG_WARN("UTCTiming@schemeIdUri Value not proper");
			}

			//need to chcek support for eUTC_HTTP_XSDATE,eUTC_HTTP_NTP
			if( stAampLLDashServiceData.utcTiming == eUTC_HTTP_XSDATE ||
			stAampLLDashServiceData.utcTiming == eUTC_HTTP_ISO ||
			stAampLLDashServiceData.utcTiming == eUTC_HTTP_NTP)
			{
				long http_error = -1;
				AAMPLOG_TRACE("UTCTiming(%d) Value: %s",stAampLLDashServiceData.utcTiming, utcTiming->GetValue().c_str());
				bool bFlag = aamp->GetNetworkTime(stAampLLDashServiceData.utcTiming, utcTiming->GetValue(), &http_error, eCURL_GET);
			}
		}
	}
    return true;
}

/**
 * @brief Get content protection from represetation/adaptation field
 * @retval content protections if present. Else NULL.
 */
vector<IDescriptor*> StreamAbstractionAAMP_MPD::GetContentProtection(const IAdaptationSet *adaptationSet,MediaType mediaType )
	{
		//Priority for representation.If the content protection not available in the representation, go with adaptation set
		if(adaptationSet->GetRepresentation().size() > 0)
		{
			int representaionSize = adaptationSet->GetRepresentation().size();
			for(int index=0; index < representaionSize ; index++ )
			{
				IRepresentation* representation = adaptationSet->GetRepresentation().at(index);
				if( representation->GetContentProtection().size() > 0 )
				{
					return( representation->GetContentProtection() );
				}
			}
		}
		return (adaptationSet->GetContentProtection());
	}
/***************************************************************************
 * @brief Function to get available video tracks
 *
 * @return vector of available video tracks.
 ***************************************************************************/
std::vector<StreamInfo*> StreamAbstractionAAMP_MPD::GetAvailableVideoTracks(void)
{
	std::vector<StreamInfo*> videoTracks;
	for( int i = 0; i < mProfileCount; i++ )
	{
		struct StreamInfo *streamInfo = &mStreamInfo[i];
		videoTracks.push_back(streamInfo);
	}
	return videoTracks;
}
