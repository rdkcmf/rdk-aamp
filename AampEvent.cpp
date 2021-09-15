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
 * @file aamp_events.cpp
 * @brief Implementation of AAMPEventObject and derived class.
 */

#include "AampEvent.h"

/**
 * @brief AAMPEventObject Constructor
 */
AAMPEventObject::AAMPEventObject(AAMPEventType type): mType(type)
{

}

/**
 * @brief Get Event Type
 *
 * @return Event Type
 */
AAMPEventType AAMPEventObject::getType() const
{
	return mType;
}

/*
 * @brief MediaErrorEvent Constructor
 *
 * @param[in] evtType     - Event type
 * @param[in] failure     - Failure type
 * @param[in] code        - Error code
 * @param[in] desc        - Error description
 * @param[in] shouldRetry - Retry or not
 */
MediaErrorEvent::MediaErrorEvent(AAMPTuneFailure failure, int code, const std::string &desc, bool shouldRetry):
		AAMPEventObject(AAMP_EVENT_TUNE_FAILED), mFailure(failure), mCode(code),
		mDescription(desc), mShouldRetry(shouldRetry)
{

}

/**
 * @brief Get Failure Type
 *
 * @return Tune failure type
 */
AAMPTuneFailure MediaErrorEvent::getFailure() const
{
	return mFailure;
}

/**
 * @brief Get Error Code
 *
 * @return Tune error code
 */
int MediaErrorEvent::getCode() const
{
	return mCode;
}

/**
 * @brief Get Description
 *
 * @return Error description
 */
const std::string &MediaErrorEvent::getDescription() const
{
	return mDescription;
}

/**
 * @brief Retry or not
 *
 * @return Retry or don't retry
 */
bool MediaErrorEvent::shouldRetry() const
{
	return mShouldRetry;
}

/*
 * @brief SpeedChangedEvent Constructor
 *
 * @param[in]  rate - New speed
 */
SpeedChangedEvent::SpeedChangedEvent(int rate):
		AAMPEventObject(AAMP_EVENT_SPEED_CHANGED), mRate(rate)
{

}

/**
 * @brief Get Rate
 *
 * @return New speed
 */
int SpeedChangedEvent::getRate() const
{
	return mRate;
}

/*
 * @brief ProgressEvent Constructor
 *
 * @param[in]  duration - Duration of Asset
 * @param[in]  position - Current Position
 * @param[in]  start    - Start Position
 * @param[in]  end      - End Position
 * @param[in]  speed    - Current Speed
 * @param[in]  pts      - Video PTS
 * @param[in]  bufferedDuration - buffered duration
 */
ProgressEvent::ProgressEvent(double duration, double position, double start, double end, float speed, long long pts, double bufferedDuration, std::string seiTimecode):
		AAMPEventObject(AAMP_EVENT_PROGRESS), mDuration(duration),
		mPosition(position), mStart(start),
		mEnd(end), mSpeed(speed), mPTS(pts),
		mBufferedDuration(bufferedDuration),
		mSEITimecode(seiTimecode)
{

}

/**
 * @brief Get Duration of Asset
 *
 * @return Asset duration in MS
 */
double ProgressEvent::getDuration() const
{
	return mDuration;
}

/**
 * @brief Get Current Position
 *
 * @return Current position in MS
 */
double ProgressEvent::getPosition() const
{
	return mPosition;
}

/**
 * @brief Get Start Position
 *
 * @return Start position in MS
 */
double ProgressEvent::getStart() const
{
	return mStart;
}

/**
 * @brief Get End Position
 *
 * @return End position in MS
 */
double ProgressEvent::getEnd() const
{
	return mEnd;
}

/**
 * @brief Get Speed
 *
 * @return Current speed
 */
float ProgressEvent::getSpeed() const
{
	return mSpeed;
}

/**
 * @brief Get Video PTS
 *
 * @return Video PTS
 */
long long ProgressEvent::getPTS() const
{
	return mPTS;
}

/**
 * @brief Get Buffered Duration
 *
 * @return Buffered duration
 */
double ProgressEvent::getBufferedDuration() const
{
	return mBufferedDuration;
}

/**
* @brief Get SEI Timecode Information
*
* @return SEI Timecode
*/
const char* ProgressEvent::getSEITimeCode() const
{
	return mSEITimecode.c_str();
}

/*
 * @brief CCHandleEvent Constructor
 *
 * @param[in] handle - Handle to close caption
 */
CCHandleEvent::CCHandleEvent(unsigned long handle):
		AAMPEventObject(AAMP_EVENT_CC_HANDLE_RECEIVED), mHandle(handle)
{

}

/**
 * @brief Get Closed Caption Handle
 *
 * @return Handle to the closed caption
 */
unsigned long CCHandleEvent::getCCHandle() const
{
	return mHandle;
}

/*
 * @brief MediaMetadataEvent Constructor
 *
 * @param[in] duration   - Duration of Media Metadata
 * @param[in] width      - Video width
 * @param[in] height     - Video height
 * @param[in] hasDrm     - Drm enablement status
 * @param[in] isLive     - Is Live
 * @param[in] DrmType    - Current DRM Type
 */
MediaMetadataEvent::MediaMetadataEvent(long duration, int width, int height, bool hasDrm, bool isLive, const std::string &DrmType, double programStartTime):
		AAMPEventObject(AAMP_EVENT_MEDIA_METADATA), mDuration(duration),
		mLanguages(), mBitrates(), mWidth(width), mHeight(height),
		mHasDrm(hasDrm), mSupportedSpeeds(), mIsLive(isLive), mDrmType(DrmType), mProgramStartTime(programStartTime)
{

}

/**
 * @brief Get Duration
 *
 * @return Asset duration
 */
long MediaMetadataEvent::getDuration() const
{
	return mDuration;
}

/**
 * @brief Get Program/Availability Start Time.
 *
 * @return Program/Availability Start Time.
 */
double MediaMetadataEvent::getProgramStartTime() const
{
	return mProgramStartTime;
}

/**
 * @brief Add a supported language
 *
 * @param[in] lang - Supported language
 * @return void
 */
void MediaMetadataEvent::addLanguage(const std::string &lang)
{
	return mLanguages.push_back(lang);
}

/**
 * @brief Get Languages
 *
 * @return Vector of supported languages
 */
const std::vector<std::string> &MediaMetadataEvent::getLanguages() const
{
	return mLanguages;
}

/**
 * @brief Get Language Count
 *
 * @return Supported language count
 */
int MediaMetadataEvent::getLanguagesCount() const
{
	return mLanguages.size();
}

/**
 * @brief Add a supported bitrate
 *
 * @param[in] bitrate - Supported bitrate
 * @return void
 */
void MediaMetadataEvent::addBitrate(long bitrate)
{
	return mBitrates.push_back(bitrate);
}

/**
 * @brief Get Bitrates
 *
 * @return Vector of supported bitrates
 */
const std::vector<long> &MediaMetadataEvent::getBitrates() const
{
	return mBitrates;
}

/**
 * @brief Get Bitrate Count
 *
 * @return Supported bitrate count
 */
int MediaMetadataEvent::getBitratesCount() const
{
	return mBitrates.size();
}

/**
 * @brief Get Width
 *
 * @return Video width
 */
int MediaMetadataEvent::getWidth() const
{
	return mWidth;
}

/**
 * @brief Get Height
 *
 * @return Video height
 */
int MediaMetadataEvent::getHeight() const
{
	return mHeight;
}

/**
 * @brief Supports DRM or not
 *
 * @return DRM enablement status
 */
bool MediaMetadataEvent::hasDrm() const
{
	return mHasDrm;
}

/**
 * @brief Add a supported speed
 *
 * @param[in] speed - Supported speed
 * @return void
 */
void MediaMetadataEvent::addSupportedSpeed(int speed)
{
	return mSupportedSpeeds.push_back(speed);
}

/**
 * @brief Get Supported Speeds
 *
 * @return Vector of supported speeds
 */
const std::vector<int> &MediaMetadataEvent::getSupportedSpeeds() const
{
	return mSupportedSpeeds;
}

/**
 * @brief Get Supported Speed count
 *
 * @return Supported speeds count
 */
int MediaMetadataEvent::getSupportedSpeedCount() const
{
	return mSupportedSpeeds.size();
}

/**
 * @brief Check for Live content or VOD
 *
 * @return isLive
 */
bool MediaMetadataEvent::isLive() const
{
	return mIsLive;
}

/**
 * @brief Get Current DRM Type
 *
 * @return Current DRM
 */
const std::string &MediaMetadataEvent::getDrmType() const
{
	return mDrmType;
}

/*
 * @brief BitrateChangeEvent Constructor
 *
 * @param[in] time       - Time of bitrate change
 * @param[in] bitrate    - New bitrate
 * @param[in] desc       - Reason of change
 * @param[in] width      - Video width
 * @param[in] height     - Video height
 * @param[in] frameRate  - Framerate
 * @param[in] position   - Position
 * @param[in] cappedProfile - Restricted profile status
 * @param[in] displayWidth - Output display width
 * @param[in] displayHeight - Output display height
 * @param[in] videoScanType - Video Scan Type
 * @param[in] aspectRatioWidth - Aspect Ratio Width
 * @param[in] aspectRatioHeight - Aspect Ratio Height
 */
BitrateChangeEvent::BitrateChangeEvent(int time, long bitrate, const std::string &desc, int width, int height, double frameRate, double position, bool cappedProfile, int displayWidth, int displayHeight, VideoScanType videoScanType, int aspectRatioWidth, int aspectRatioHeight):
		AAMPEventObject(AAMP_EVENT_BITRATE_CHANGED), mTime(time),
		mBitrate(bitrate), mDescription(desc), mWidth(width),
		mHeight(height), mFrameRate(frameRate), mPosition(position), mCappedProfile(cappedProfile), mDisplayWidth(displayWidth), mDisplayHeight(displayHeight), mVideoScanType(videoScanType), mAspectRatioWidth(aspectRatioWidth), mAspectRatioHeight(aspectRatioHeight)
{

}

/**
 * @brief Get Time
 *
 * @return Playback time
 */
int BitrateChangeEvent::getTime() const
{
	return mTime;
}

/**
 * @brief Get Bitrate
 *
 * @return Current bitrate
 */
long BitrateChangeEvent::getBitrate() const
{
	return mBitrate;
}

/**
 * @brief Get Description
 *
 * @return Reason of bitrate change
 */
const std::string &BitrateChangeEvent::getDescription() const
{
	return mDescription;
}

/**
 * @brief Get Width
 *
 * @return Video width
 */
int BitrateChangeEvent::getWidth() const
{
	return mWidth;
}

/**
 * @brief Get Height
 *
 * @return Video height
 */
int BitrateChangeEvent::getHeight() const
{
	return mHeight;
}

/**
 * @brief Get Frame Rate
 *
 * @return Frame Rate
 */
double BitrateChangeEvent::getFrameRate() const
{
	return mFrameRate;
}

/**
 * @brief Get Position
 *
 * @return Position
 */
double BitrateChangeEvent::getPosition() const
{
	return mPosition;
}

/**
 * @brief Get Profile capped status
 *
 * @return true or false
 */
bool BitrateChangeEvent::getCappedProfileStatus() const
{
        return mCappedProfile;
}

/**
 * @brief Get output tv display Width
 *
 * @return Display width
 */
int BitrateChangeEvent::getDisplayWidth() const
{
        return mDisplayWidth;
}

/**
 * @brief Get output tv display Height
 *
 * @return Displat height
 */
int BitrateChangeEvent::getDisplayHeight() const
{
        return mDisplayHeight;
}

/**
 * @brief Get Video Scan Type
 *
 * @return Video Scan Type
 */
VideoScanType BitrateChangeEvent::getScanType() const
{
        return mVideoScanType;
}

/**
 * @brief Get Aspect Ratio Width
 *
 * @return Aspect Ratio Width
 */
int BitrateChangeEvent::getAspectRatioWidth() const
{
        return mAspectRatioWidth;
}

/**
 * @brief Get Aspect Ratio Height
 *
 * @return Aspect Ratio Height
 */
int BitrateChangeEvent::getAspectRatioHeight() const
{
        return mAspectRatioHeight;
}

/*
 * @brief TimedMetadataEvent Constructor
 *
 * @param[in] name      - TimedMetadata name
 * @param[in] id        - TimedMetadata id
 * @param[in] time      - Time of event
 * @param[in] duration   - Duration of event
 * @param[in] content   - Content field of the TimedMetadata
 */
TimedMetadataEvent::TimedMetadataEvent(const std::string &name, const std::string &id, double time, double duration, const std::string &content):
		AAMPEventObject(AAMP_EVENT_TIMED_METADATA), mName(name), mId(id),
		mTime(time), mDuration(duration), mContent(content)
{

}

/**
 * @brief Get Timed Metadata Name
 *
 * @return TimedMetadata name string
 */
const std::string &TimedMetadataEvent::getName() const
{
	return mName;
}

/**
 * @brief Get Timed Metadata Id
 *
 * @return TimedMetadata id string
 */
const std::string &TimedMetadataEvent::getId() const
{
	return mId;
}

/**
 * @brief Get Time
 *
 * @return Time of the Timed Metadata
 */
double TimedMetadataEvent::getTime() const
{
	return mTime;
}

/**
 * @brief Get Duration
 *
 * @return Duration (in MS) of the TimedMetadata
 */
double TimedMetadataEvent::getDuration() const
{
	return mDuration;
}

/**
 * @brief Get Content
 *
 * @return Content field of the TimedMetadata
 */
const std::string &TimedMetadataEvent::getContent() const
{
	return mContent;
}

/*
 * @brief BulkTimedMetadataEvent Constructor
 *
 * @param[in] content - metadata serialized in JSON format
 */
BulkTimedMetadataEvent::BulkTimedMetadataEvent(const std::string &content):
		AAMPEventObject(AAMP_EVENT_BULK_TIMED_METADATA), mContent(content)
{

}

/**
 * @brief Get metadata content
 *
 * @return metadata content
 */
const std::string &BulkTimedMetadataEvent::getContent() const
{
	return mContent;
}

/*
 * @brief StateChangedEvent Constructor
 *
 * @param[in] state - New player state
 */
StateChangedEvent::StateChangedEvent(PrivAAMPState state):
		AAMPEventObject(AAMP_EVENT_STATE_CHANGED), mState(state)
{

}

/**
 * @brief Get Current Player State
 *
 * @return Player state
 */
PrivAAMPState StateChangedEvent::getState() const
{
	return mState;
}

/*
 * @brief SupportedSpeedsChangedEvent Constructor
 */
SupportedSpeedsChangedEvent::SupportedSpeedsChangedEvent():
		AAMPEventObject(AAMP_EVENT_SPEEDS_CHANGED), mSupportedSpeeds()
{

}

/**
 * @brief Add a Supported Speed
 *
 * @param[in] speed - Speed
 * @return void
 */
void SupportedSpeedsChangedEvent::addSupportedSpeed(int speed)
{
	return mSupportedSpeeds.push_back(speed);
}

/**
 * @brief Get Supported Speeds
 *
 * @return Vector of supported speeds
 */
const std::vector<int> &SupportedSpeedsChangedEvent::getSupportedSpeeds() const
{
	return mSupportedSpeeds;
}

/**
 * @brief Get Supported Speeds Count
 *
 * @return Supported speeds count
 */
int SupportedSpeedsChangedEvent::getSupportedSpeedCount() const
{
	return mSupportedSpeeds.size();
}

/*
 * @brief SeekedEvent Constructor
 *
 * @param[in] positionMS - seeked position in milliseconds
 */
SeekedEvent::SeekedEvent(double positionMS):
		AAMPEventObject(AAMP_EVENT_SEEKED), mPosition(positionMS)
{

}

/**
 * @brief Get position
 *
 * @return Seeked position
 */
double SeekedEvent::getPosition() const
{
	return mPosition;
}

/*
 * @brief TuneProfilingEvent Constructor
 *
 * @param[in] profilingData - tune profiling data
 */
TuneProfilingEvent::TuneProfilingEvent(std::string &profilingData):
		AAMPEventObject(AAMP_EVENT_TUNE_PROFILING), mProfilingData(profilingData)
{

}

/**
 * @brief Get Tune profiling data
 *
 * @return Tune profiling data
 */
const std::string &TuneProfilingEvent::getProfilingData() const
{
	return mProfilingData;
}

/*
 * @brief BufferingChangedEvent Constructor
 *
 * @param[in] buffering - Buffering status
 */
BufferingChangedEvent::BufferingChangedEvent(bool buffering):
		AAMPEventObject(AAMP_EVENT_BUFFERING_CHANGED), mBuffering(buffering)
{

}

/**
 * @brief Get Buffering Status
 *
 * @return Buffering status (true/false)
 */
bool BufferingChangedEvent::buffering() const
{
	return mBuffering;
}

/*
 * @brief DrmMetaDataEvent Constructor
 *
 * @param[in] failure      - Failure type
 * @param[in] accessStatus - Access status
 * @param[in] statusValue  - Access status value
 * @param[in] responseCode - Response code
 */
DrmMetaDataEvent::DrmMetaDataEvent(AAMPTuneFailure failure, const std::string &accessStatus, int statusValue, long responseCode, bool secclientErr):
		AAMPEventObject(AAMP_EVENT_DRM_METADATA), mFailure(failure), mAccessStatus(accessStatus),
		mAccessStatusValue(statusValue), mResponseCode(responseCode), mSecclientError(secclientErr)
{

}

/**
 * @brief Get Failure type
 *
 * @return Tune failure type
 */
AAMPTuneFailure DrmMetaDataEvent::getFailure() const
{
	return mFailure;
}

/**
 * @brief Set Failure type
 *
 * @param[in] failure - Failure type
 * @return void
 */
void DrmMetaDataEvent::setFailure(AAMPTuneFailure failure)
{
	mFailure = failure;
}

/**
 * @brief Get Access Status
 *
 * @return Access status string
 */
const std::string &DrmMetaDataEvent::getAccessStatus() const
{
	return mAccessStatus;
}

/**
 * @brief Set Access Status
 *
 * @param[in] status - Access status
 * @return void
 */
void DrmMetaDataEvent::setAccessStatus(const std::string &status)
{
	mAccessStatus = status;
}

/**
 * @brief Get Access Status
 *
 * @return Access status value
 */
int DrmMetaDataEvent::getAccessStatusValue() const
{
	return mAccessStatusValue;
}

/**
 * @brief Set Access Status Value
 *
 * @param[in] value - Access status value
 * @return void
 */
void DrmMetaDataEvent::setAccessStatusValue(int value)
{
	mAccessStatusValue = value;
}

/**
 * @brief Get Response Code
 *
 * @return Response code
 */
long DrmMetaDataEvent::getResponseCode() const
{
	return mResponseCode;
}

/**
 * @brief Set Response Code
 *
 * @param[in] code - Response code
 * @return void
 */
void DrmMetaDataEvent::setResponseCode(long code)
{
	mResponseCode = code;
}

/**
 * @brief get secclient error
 *
 * @return get secclient error (true/false)
 */
bool DrmMetaDataEvent::getSecclientError() const
{
	return mSecclientError;
}

/**
 * @brief Update secclient error
 *
 * @param[in] secClientError status (true/false)
 * @return void
 */
void DrmMetaDataEvent::setSecclientError(bool secClientError)
{
	mSecclientError = secClientError;
}

/**
 * @brief AnomalyReportEvent Constructor
 *
 * @param[in]  severity - Severity of message
 * @param[in]  msg      - Anomaly message
 */
AnomalyReportEvent::AnomalyReportEvent(int severity, const std::string &msg):
		AAMPEventObject(AAMP_EVENT_REPORT_ANOMALY), mSeverity(severity), mMsg(msg)
{

}

/**
 * @brief Get Severity
 *
 * @return Severity value
 */
int AnomalyReportEvent::getSeverity() const
{
	return mSeverity;
}

/**
 * @brief Get Anomaly Message
 *
 * @return Anomaly message string
 */
const std::string &AnomalyReportEvent::getMessage() const
{
	return mMsg;
}

/*
 * @brief WebVttCueEvent Constructor
 *
 * @param[in] cueData - Pointer to VTT cue data
 */
WebVttCueEvent::WebVttCueEvent(VTTCue* cueData):
		AAMPEventObject(AAMP_EVENT_WEBVTT_CUE_DATA), mCueData(cueData)
{

}

/**
 * @brief Get VTT Cue Data
 *
 * @return Pointer to VTT cue data
 */
VTTCue* WebVttCueEvent::getCueData() const
{
	return mCueData;
}

/*
 * @brief AdResolvedEvent Constructor
 *
 * @param[in] resolveStatus - Ad resolve status
 * @param[in] adId          - Identifier of the Ad
 * @param[in] startMS       - Start position of Ad (relative to reservation start)
 * @param[in] durationMs    - Duration of the Ad in MS
 */
AdResolvedEvent::AdResolvedEvent(bool resolveStatus, const std::string &adId, uint64_t startMS, uint64_t durationMs):
		AAMPEventObject(AAMP_EVENT_AD_RESOLVED), mResolveStatus(resolveStatus), mAdId(adId),
		mStartMS(startMS), mDurationMs(durationMs)
{

}

/**
 * @brief Get Resolve Status
 *
 * @return Ad resolve status
 */
bool AdResolvedEvent::getResolveStatus() const
{
	return mResolveStatus;
}

/**
 * @brief Get Ad Identifier
 *
 * @return Ad's identifier
 */
const std::string &AdResolvedEvent::getAdId() const
{
	return mAdId;
}

/**
 * @brief Get Start Positon
 *
 * @return Start position (in MS), relative to Adbreak
 */
uint64_t AdResolvedEvent::getStart() const
{
	return mStartMS;
}

/**
 * @brief Get Duration
 *
 * @return Ad's duration in MS
 */
uint64_t AdResolvedEvent::getDuration() const
{
	return mDurationMs;
}

/*
 * @brief AdReservationEvent Constructor
 *
 * @param[in] evtType  - Event Type
 * @param[in] breakId  - Unique identifier of Ad reservation.
 * @param[in] position - Postion of reservation in content's PTS
 */
AdReservationEvent::AdReservationEvent(AAMPEventType evtType, const std::string &breakId, uint64_t position):
		AAMPEventObject(evtType), mAdBreakId(breakId), mPosition(position)
{

}

/**
 * @brief Get Adbreak Identifier
 *
 * @return Adbreak's id
 */
const std::string &AdReservationEvent::getAdBreakId() const
{
	return mAdBreakId;
}

/**
 * @brief Get Ad's Position
 *
 * @return Ad's position (in channel's PTS)
 */
uint64_t AdReservationEvent::getPosition() const
{
	return mPosition;
}

/*
 * @brief AdPlacementEvent Constructor
 *
 * @param[in] evtType   - Event Type
 * @param[in] adId      - Ad Id
 * @param[in] position  - Ad's position (in channel's PTS)
 * @param[in] offset    - Ad's start offset
 * @param[in] duration  - Ad's duration in MS
 * @param[in] errorCode - Error code, in case of placement error
 */
AdPlacementEvent::AdPlacementEvent(AAMPEventType evtType, const std::string &adId, uint32_t position, uint32_t offset, uint32_t duration, int errorCode):
		AAMPEventObject(evtType), mAdId(adId), mPosition(position),
		mOffset(offset), mDuration(duration), mErrorCode(errorCode)
{

}

/**
 * @brief Get Ad's Identifier
 *
 * @return Ad's id
 */
const std::string &AdPlacementEvent::getAdId() const
{
	return mAdId;
}

/**
 * @brief Get Ad's Position
 *
 * @return Ad's position (in channel's PTS)
 */
uint32_t AdPlacementEvent::getPosition() const
{
	return mPosition;
}

/**
 * @brief Get Ad's Offset
 *
 * @return Ad's start offset
 */
uint32_t AdPlacementEvent::getOffset() const
{
	return mOffset;
}

/**
 * @brief Get Ad's Duration
 *
 * @return Ad's duration in MS
 */
uint32_t AdPlacementEvent::getDuration() const
{
	return mDuration;
}

/**
 * @brief Get Error Code
 *
 * @return Error code
 */
int AdPlacementEvent::getErrorCode() const
{
	return mErrorCode;
}

/**
 * @brief MetricsDataEvent Constructor
 *
 * @param[in]  dataType - Data type
 * @param[in]  uuid     - unique identifier
 * @param[in]  data     - Metrics data
 */
MetricsDataEvent::MetricsDataEvent(MetricsDataType dataType, const std::string &uuid, const std::string &data):
		AAMPEventObject(AAMP_EVENT_REPORT_METRICS_DATA), mMetricsDataType(dataType),
		mMetricUUID(uuid), mMetricsData(data)
{

}

/**
 * @brief Get Metrics Data Type
 *
 * @return Metrics data type
 */
MetricsDataType MetricsDataEvent::getMetricsDataType() const
{
	return mMetricsDataType;
}

/**
 * @brief Get Metric UUID
 *
 * @return Uuid string
 */
const std::string &MetricsDataEvent::getMetricUUID() const
{
	return mMetricUUID;
}

/**
 * @brief Get Metrics Data
 *
 * @return Metrics data string
 */
const std::string &MetricsDataEvent::getMetricsData() const
{
	return mMetricsData;
}

/*
 * @brief ID3MetadataEvent Constructor
 *
 * @param[in] metadata - ID3 metadata
 * @param[in] timeScale - timeScale od ID3 data
 * @param[in] presentationTime - PTS value
 * @param[in] eventDuration - eventDuration
 * @param[in] id - id of ID3 data
 */
ID3MetadataEvent::ID3MetadataEvent(const std::vector<uint8_t> &metadata, const std::string &schIDUri, std::string &id3Value, uint32_t timeScale, uint64_t presentationTime, uint32_t eventDuration, uint32_t id, uint64_t timestampOffset):
		AAMPEventObject(AAMP_EVENT_ID3_METADATA), mMetadata(metadata), mId(id), mTimeScale(timeScale), mSchemeIdUri(schIDUri), mValue(id3Value), mEventDuration(eventDuration), mPresentationTime(presentationTime), mTimestampOffset(timestampOffset)
{

}

/**
 * @brief Get ID3 metdata
 *
 * @return ID3 metadata content
 */
const std::vector<uint8_t> &ID3MetadataEvent::getMetadata() const
{
	return mMetadata;
}

/**
 * @brief Get ID3 metdata size
 *
 * @return ID3 metadata size
 */
int ID3MetadataEvent::getMetadataSize() const
{
	return mMetadata.size();
}

/**
 * @brief Get TimeScale value
 *
 * @return TimeScale value
 */
uint32_t ID3MetadataEvent::getTimeScale() const
{
	return mTimeScale;
}

/**
 * @brief Get eventDuration
 *
 * @return eventDuration value
 */
uint32_t ID3MetadataEvent::getEventDuration() const
{
	return mEventDuration;
}

/**
 * @brief Get id
 *
 * @return id value
 */
uint32_t ID3MetadataEvent::getId() const
{
	return mId;
}

/**
 * @brief Get presentationTime
 *
 * @return presentationTime value
 */
uint64_t ID3MetadataEvent::getPresentationTime() const
{
	return mPresentationTime;
}

/**
 * @brief Get timestampOffset
 *
 * @return timestampOffset value
 */
uint64_t ID3MetadataEvent::getTimestampOffset() const
{
        return mTimestampOffset;
}

/**
 * @brief Get schemeIdUri
 *
 * @return schemeIdUri value
 */
const std::string& ID3MetadataEvent::getSchemeIdUri() const
{
	return mSchemeIdUri;
}

/**
 * @brief Get value
 *
 * @return schemeIdUri value
 */
const std::string& ID3MetadataEvent::getValue() const
{
	return mValue;
}

/*
* @brief DrmMessageEvent Constructor
*
* @param[in] msg - DRM message
*/
DrmMessageEvent::DrmMessageEvent(const std::string &msg):
		AAMPEventObject(AAMP_EVENT_DRM_MESSAGE), mMessage(msg)
{

}

/**
 * @brief Get DRM Message
 *
 * @return DRM message
 */
const std::string &DrmMessageEvent::getMessage() const
{
	return mMessage;
}


/*
 * @brief ContentGapEvent Constructor
 * @param[in] time      - Time of event
 * @param[in] duration   - Duration of event
 */
ContentGapEvent::ContentGapEvent(double time, double duration):
		AAMPEventObject(AAMP_EVENT_CONTENT_GAP)
		, mTime(time), mDuration(duration)
{

}

/**
 * @brief Get Time
 *
 * @return Time of the ContentGap
 */
double ContentGapEvent::getTime() const
{
	return mTime;
}

/**
 * @brief Get Duration
 *
 * @return Duration (in MS) of the ContentGap
 */
double ContentGapEvent::getDuration() const
{
	return mDuration;
}