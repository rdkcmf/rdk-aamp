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
 * @file admanager_mpd.h
 * @brief Client side DAI manger for MPEG DASH
 */

#ifndef ADMANAGER_MPD_H_
#define ADMANAGER_MPD_H_

#include "AdManagerBase.h"
#include <string>
#include "libdash/INode.h"
#include "libdash/IDASHManager.h"
#include "libdash/xml/Node.h"
#include "libdash/IMPD.h"

using namespace dash;
using namespace std;
using namespace dash::mpd;

/**
 * @class CDAIObjectMPD
 * @brief Client Side DAI object implementation for DASH
 */
class CDAIObjectMPD: public CDAIObject
{
	class PrivateCDAIObjectMPD* mPrivObj;
public:
	CDAIObjectMPD(PrivateInstanceAAMP* aamp);
	virtual ~CDAIObjectMPD();
	CDAIObjectMPD(const CDAIObjectMPD&) = delete;
	CDAIObjectMPD& operator= (const CDAIObjectMPD&) = delete;

	PrivateCDAIObjectMPD* GetPrivateCDAIObjectMPD()
	{
		return mPrivObj;
	}

	virtual void SetAlternateContents(const std::string &periodId, const std::string &adId, const std::string &url, uint64_t startMS=0, uint32_t breakdur=0) override;
};


/**
 * @brief Ad event types
 */
enum class AdEvent
{
	INIT,
	BASE_OFFSET_CHANGE,
	AD_FINISHED,
	AD_FAILED,
	PERIOD_CHANGE,
	DEFAULT = PERIOD_CHANGE
};

#define OFFSET_ALIGN_FACTOR 2000 //Observed minor slacks in the ad durations. Align factor used to place the ads correctly.

/**
 * @struct AdNode
 * @brief Individual Ad's meta info
 */
struct AdNode {
	bool invalid;		//Failed to play first time.
	bool placed;  		//Ad completely placed on the period
	std::string adId;
	std::string url;
	uint64_t duration;
	std::string basePeriodId;
	int basePeriodOffset;
	MPD* mpd;

	AdNode() : invalid(false), placed(false), adId(), url(), duration(0), basePeriodId(), basePeriodOffset(0), mpd(nullptr)
	{

	}

	AdNode(bool invalid, bool placed, std::string adId, std::string url, uint64_t duration,
									std::string basePeriodId, int basePeriodOffset, MPD* mpd)
	: invalid(invalid), placed(placed), adId(adId), url(url), duration(duration), basePeriodId(basePeriodId),
		basePeriodOffset(basePeriodOffset), mpd(mpd)
	{

	}

	AdNode(const AdNode& adNode) : invalid(adNode.invalid), placed(adNode.placed), adId(adNode.adId),
									url(adNode.url), duration(adNode.duration), basePeriodId(adNode.basePeriodId),
									basePeriodOffset(adNode.basePeriodOffset), mpd(adNode.mpd)
	{
	}

	AdNode& operator=(const AdNode&) = delete;
};

/**
 * @struct AdBreakObject
 * @brief AdBreak's metadata
 */
struct AdBreakObject{
	uint32_t brkDuration;
	std::shared_ptr<std::vector<AdNode>> ads;
	std::string endPeriodId;
	uint64_t endPeriodOffset;	//Last ad's end position stretches till here
	uint32_t adsDuration;

	AdBreakObject() : brkDuration(0), ads(), endPeriodId(), endPeriodOffset(0), adsDuration(0)
	{
	}

	AdBreakObject(uint32_t _duration, std::shared_ptr<std::vector<AdNode>> _ads, std::string _endPeriodId,
	uint64_t _endPeriodOffset, uint32_t _adsDuration)
	: brkDuration(_duration), ads(_ads), endPeriodId(_endPeriodId), endPeriodOffset(_endPeriodOffset), adsDuration(_adsDuration)
	{
	}
};

/**
 * @struct AdOnPeriod
 * @brief Individual Ad's object falling in each period
 */
struct AdOnPeriod
{
	int32_t adIdx;
	uint32_t adStartOffset;
};

/**
 * @struct Period2AdData
 * @brief Meta info corresponding to each period.
 */
struct Period2AdData {
	bool filled;				//Period filled with ads or not
	std::string adBreakId;		//Parent Adbreak
	uint64_t duration;			//Period Duration
	std::map<int, AdOnPeriod> offset2Ad;

	Period2AdData() : filled(false), adBreakId(), duration(0), offset2Ad()
	{
	}
};

/**
 * @struct AdFulfillObj
 * @brief Temporary object representing current fulfilling ad.
 */
struct AdFulfillObj {
	std::string periodId;
	std::string adId;
	std::string url;

	AdFulfillObj() : periodId(), adId(), url()
	{

	}
};

/**
 * @struct PlacementObj
 * @brief Current placing Ad's object
 */
struct PlacementObj {
	std::string pendingAdbrkId;			//Only one Adbreak will be pending for replacement
	std::string openPeriodId;			//The period in the adbreak that is progressing
	uint64_t 	curEndNumber;			//Current periods last fragment number
	int			curAdIdx;				//Currently placing ad, during MPD progression
	uint32_t 	adNextOffset;			//Current periods last fragment number

	PlacementObj(): pendingAdbrkId(), openPeriodId(), curEndNumber(0), curAdIdx(-1), adNextOffset(0)
	{

	}
};	//Keeping current placement status. Not used for playback


/**
 * @class PrivateCDAIObjectMPD
 * @brief Private Client Side DAI object for DASH
 */
class PrivateCDAIObjectMPD
{
public:
	PrivateInstanceAAMP* mAamp;
	std::mutex mDaiMtx;				/**< Mutex protecting DAI critical section */
	bool 	mIsFogTSB;				/**< Channel playing from TSB or not */
	std::unordered_map<std::string, AdBreakObject> mAdBreaks;
	std::unordered_map<std::string, Period2AdData> mPeriodMap;	//periodId -> (periodClosed, vector<<Ad, FragStartIdx, FragEndIdx>>)
	std::string mCurPlayingBreakId;
	pthread_t mAdObjThreadID;
	bool mAdFailed;
	std::shared_ptr<std::vector<AdNode>> mCurAds;
	int mCurAdIdx;
	AdFulfillObj mAdFulfillObj;
	PlacementObj mPlacementObj;
	double mContentSeekOffset;
	AdState mAdState;

	PrivateCDAIObjectMPD(PrivateInstanceAAMP* aamp);	//Ctor
	~PrivateCDAIObjectMPD();	//Dtor

	PrivateCDAIObjectMPD(const PrivateCDAIObjectMPD&) = delete;
	PrivateCDAIObjectMPD& operator= (const PrivateCDAIObjectMPD&) = delete;

	void SetAlternateContents(const std::string &periodId, const std::string &adId, const std::string &url,  uint64_t startMS, uint32_t breakdur=0);
	void FulFillAdObject();
	MPD*  GetAdMPD(std::string &url, bool &finalManifest, bool tryFog = false);
	void InsertToPeriodMap(IPeriod *period);
	bool isPeriodExist(const std::string &periodId);
	inline bool isAdBreakObjectExist(const std::string &adBrkId);
	void PrunePeriodMaps(std::vector<std::string> &newPeriodIds);
	void ResetState();
	void ClearMaps();
	void  PlaceAds(dash::mpd::IMPD *mpd);
	int CheckForAdStart(const float &rate, bool init, const std::string &periodId, double offSet, std::string &breakId, double &adOffset);
	bool CheckForAdTerminate(double fragmentTime);
	inline bool isPeriodInAdbreak(const std::string &periodId);
};


#endif /* ADMANAGER_MPD_H_ */
