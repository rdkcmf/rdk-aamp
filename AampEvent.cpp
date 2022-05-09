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
 * @file AampEvent.cpp
 * @brief Implementation of AAMPEventObject and derived class.
 */

#include "AampEvent.h"


AAMPEventObject::AAMPEventObject(AAMPEventType type): mType(type)
{

}


AAMPEventType AAMPEventObject::getType() const
{
	return mType;
}


MediaErrorEvent::MediaErrorEvent(AAMPTuneFailure failure, int code, const std::string &desc, bool shouldRetry):
		AAMPEventObject(AAMP_EVENT_TUNE_FAILED), mFailure(failure), mCode(code),
		mDescription(desc), mShouldRetry(shouldRetry)
{

}


AAMPTuneFailure MediaErrorEvent::getFailure() const
{
	return mFailure;
}


int MediaErrorEvent::getCode() const
{
	return mCode;
}


const std::string &MediaErrorEvent::getDescription() const
{
	return mDescription;
}


bool MediaErrorEvent::shouldRetry() const
{
	return mShouldRetry;
}


SpeedChangedEvent::SpeedChangedEvent(int rate):
		AAMPEventObject(AAMP_EVENT_SPEED_CHANGED), mRate(rate)
{

}


int SpeedChangedEvent::getRate() const
{
	return mRate;
}


ProgressEvent::ProgressEvent(double duration, double position, double start, double end, float speed, long long pts, double bufferedDuration, std::string seiTimecode):
		AAMPEventObject(AAMP_EVENT_PROGRESS), mDuration(duration),
		mPosition(position), mStart(start),
		mEnd(end), mSpeed(speed), mPTS(pts),
		mBufferedDuration(bufferedDuration),
		mSEITimecode(seiTimecode)
{

}


double ProgressEvent::getDuration() const
{
	return mDuration;
}


double ProgressEvent::getPosition() const
{
	return mPosition;
}


double ProgressEvent::getStart() const
{
	return mStart;
}


double ProgressEvent::getEnd() const
{
	return mEnd;
}


float ProgressEvent::getSpeed() const
{
	return mSpeed;
}


long long ProgressEvent::getPTS() const
{
	return mPTS;
}


double ProgressEvent::getBufferedDuration() const
{
	return mBufferedDuration;
}


const char* ProgressEvent::getSEITimeCode() const
{
	return mSEITimecode.c_str();
}


CCHandleEvent::CCHandleEvent(unsigned long handle):
		AAMPEventObject(AAMP_EVENT_CC_HANDLE_RECEIVED), mHandle(handle)
{

}


unsigned long CCHandleEvent::getCCHandle() const
{
	return mHandle;
}


MediaMetadataEvent::MediaMetadataEvent(long duration, int width, int height, bool hasDrm, bool isLive, const std::string &DrmType, double programStartTime):
		AAMPEventObject(AAMP_EVENT_MEDIA_METADATA), mDuration(duration),
		mLanguages(), mBitrates(), mWidth(width), mHeight(height),
		mHasDrm(hasDrm), mSupportedSpeeds(), mIsLive(isLive), mDrmType(DrmType), mProgramStartTime(programStartTime),
		mPCRating(),mSsi(-1),mFrameRate(0),mVideoScanType(eVIDEOSCAN_UNKNOWN),mAspectRatioWidth(0),mAspectRatioHeight(0),
		mVideoCodec(),mHdrType(),mAudioBitrates(),mAudioCodec(),mAudioMixType(),isAtmos(false)
{

}


long MediaMetadataEvent::getDuration() const
{
	return mDuration;
}


double MediaMetadataEvent::getProgramStartTime() const
{
	return mProgramStartTime;
}


void MediaMetadataEvent::addLanguage(const std::string &lang)
{
	return mLanguages.push_back(lang);
}


const std::vector<std::string> &MediaMetadataEvent::getLanguages() const
{
	return mLanguages;
}


int MediaMetadataEvent::getLanguagesCount() const
{
	return mLanguages.size();
}


void MediaMetadataEvent::addBitrate(long bitrate)
{
	return mBitrates.push_back(bitrate);
}


const std::vector<long> &MediaMetadataEvent::getBitrates() const
{
	return mBitrates;
}


int MediaMetadataEvent::getBitratesCount() const
{
	return mBitrates.size();
}


int MediaMetadataEvent::getWidth() const
{
	return mWidth;
}


int MediaMetadataEvent::getHeight() const
{
	return mHeight;
}


bool MediaMetadataEvent::hasDrm() const
{
	return mHasDrm;
}


void MediaMetadataEvent::addSupportedSpeed(int speed)
{
	return mSupportedSpeeds.push_back(speed);
}


const std::vector<int> &MediaMetadataEvent::getSupportedSpeeds() const
{
	return mSupportedSpeeds;
}


int MediaMetadataEvent::getSupportedSpeedCount() const
{
	return mSupportedSpeeds.size();
}


bool MediaMetadataEvent::isLive() const
{
	return mIsLive;
}


const std::string &MediaMetadataEvent::getDrmType() const
{
	return mDrmType;
}

void MediaMetadataEvent::SetVideoMetaData(float frameRate,VideoScanType videoScanType,int aspectRatioWidth,int  aspectRatioHeight, const std::string &  videoCodec, const std::string  & strHdrType, const std::string & strPCRating, int ssi)
{
	this->mFrameRate = frameRate;
	this->mVideoScanType = videoScanType;
	this->mAspectRatioWidth = aspectRatioWidth;
	this->mAspectRatioHeight = aspectRatioHeight;
	this->mVideoCodec = videoCodec;
	this->mHdrType = strHdrType;
	this->mSsi = ssi;
	this->mPCRating = strPCRating;
	return;
}

void MediaMetadataEvent::SetAudioMetaData(const std::string &audioCodec,const std::string &mixType,bool  isAtmos  )
{
	mAudioCodec = audioCodec;
	mAudioMixType = mixType;
	isAtmos = isAtmos;
	return;
}


BitrateChangeEvent::BitrateChangeEvent(int time, long bitrate, const std::string &desc, int width, int height, double frameRate, double position, bool cappedProfile, int displayWidth, int displayHeight, VideoScanType videoScanType, int aspectRatioWidth, int aspectRatioHeight):
		AAMPEventObject(AAMP_EVENT_BITRATE_CHANGED), mTime(time),
		mBitrate(bitrate), mDescription(desc), mWidth(width),
		mHeight(height), mFrameRate(frameRate), mPosition(position), mCappedProfile(cappedProfile), mDisplayWidth(displayWidth), mDisplayHeight(displayHeight), mVideoScanType(videoScanType), mAspectRatioWidth(aspectRatioWidth), mAspectRatioHeight(aspectRatioHeight)
{

}


int BitrateChangeEvent::getTime() const
{
	return mTime;
}


long BitrateChangeEvent::getBitrate() const
{
	return mBitrate;
}


const std::string &BitrateChangeEvent::getDescription() const
{
	return mDescription;
}


int BitrateChangeEvent::getWidth() const
{
	return mWidth;
}


int BitrateChangeEvent::getHeight() const
{
	return mHeight;
}


double BitrateChangeEvent::getFrameRate() const
{
	return mFrameRate;
}


double BitrateChangeEvent::getPosition() const
{
	return mPosition;
}


bool BitrateChangeEvent::getCappedProfileStatus() const
{
        return mCappedProfile;
}


int BitrateChangeEvent::getDisplayWidth() const
{
        return mDisplayWidth;
}


int BitrateChangeEvent::getDisplayHeight() const
{
        return mDisplayHeight;
}


VideoScanType BitrateChangeEvent::getScanType() const
{
        return mVideoScanType;
}


int BitrateChangeEvent::getAspectRatioWidth() const
{
        return mAspectRatioWidth;
}


int BitrateChangeEvent::getAspectRatioHeight() const
{
        return mAspectRatioHeight;
}


TimedMetadataEvent::TimedMetadataEvent(const std::string &name, const std::string &id, double time, double duration, const std::string &content):
		AAMPEventObject(AAMP_EVENT_TIMED_METADATA), mName(name), mId(id),
		mTime(time), mDuration(duration), mContent(content)
{

}


const std::string &TimedMetadataEvent::getName() const
{
	return mName;
}


const std::string &TimedMetadataEvent::getId() const
{
	return mId;
}


double TimedMetadataEvent::getTime() const
{
	return mTime;
}


double TimedMetadataEvent::getDuration() const
{
	return mDuration;
}


const std::string &TimedMetadataEvent::getContent() const
{
	return mContent;
}


BulkTimedMetadataEvent::BulkTimedMetadataEvent(const std::string &content):
		AAMPEventObject(AAMP_EVENT_BULK_TIMED_METADATA), mContent(content)
{

}


const std::string &BulkTimedMetadataEvent::getContent() const
{
	return mContent;
}


StateChangedEvent::StateChangedEvent(PrivAAMPState state):
		AAMPEventObject(AAMP_EVENT_STATE_CHANGED), mState(state)
{

}


PrivAAMPState StateChangedEvent::getState() const
{
	return mState;
}


SupportedSpeedsChangedEvent::SupportedSpeedsChangedEvent():
		AAMPEventObject(AAMP_EVENT_SPEEDS_CHANGED), mSupportedSpeeds()
{

}


void SupportedSpeedsChangedEvent::addSupportedSpeed(int speed)
{
	return mSupportedSpeeds.push_back(speed);
}


const std::vector<int> &SupportedSpeedsChangedEvent::getSupportedSpeeds() const
{
	return mSupportedSpeeds;
}


int SupportedSpeedsChangedEvent::getSupportedSpeedCount() const
{
	return mSupportedSpeeds.size();
}


SeekedEvent::SeekedEvent(double positionMS):
		AAMPEventObject(AAMP_EVENT_SEEKED), mPosition(positionMS)
{

}


double SeekedEvent::getPosition() const
{
	return mPosition;
}


TuneProfilingEvent::TuneProfilingEvent(std::string &profilingData):
		AAMPEventObject(AAMP_EVENT_TUNE_PROFILING), mProfilingData(profilingData)
{

}


const std::string &TuneProfilingEvent::getProfilingData() const
{
	return mProfilingData;
}


BufferingChangedEvent::BufferingChangedEvent(bool buffering):
		AAMPEventObject(AAMP_EVENT_BUFFERING_CHANGED), mBuffering(buffering)
{

}


bool BufferingChangedEvent::buffering() const
{
	return mBuffering;
}


DrmMetaDataEvent::DrmMetaDataEvent(AAMPTuneFailure failure, const std::string &accessStatus, int statusValue, long responseCode, bool secclientErr):
		AAMPEventObject(AAMP_EVENT_DRM_METADATA), mFailure(failure), mAccessStatus(accessStatus),
		mAccessStatusValue(statusValue), mResponseCode(responseCode), mSecclientError(secclientErr), mSecManagerReasonCode(0)
{

}


AAMPTuneFailure DrmMetaDataEvent::getFailure() const
{
	return mFailure;
}


void DrmMetaDataEvent::setFailure(AAMPTuneFailure failure)
{
	mFailure = failure;
}


const std::string &DrmMetaDataEvent::getAccessStatus() const
{
	return mAccessStatus;
}


void DrmMetaDataEvent::setAccessStatus(const std::string &status)
{
	mAccessStatus = status;
}


int DrmMetaDataEvent::getAccessStatusValue() const
{
	return mAccessStatusValue;
}


void DrmMetaDataEvent::setAccessStatusValue(int value)
{
	mAccessStatusValue = value;
}


long DrmMetaDataEvent::getResponseCode() const
{
	return mResponseCode;
}


long DrmMetaDataEvent::getSecManagerReasonCode() const
{
	return mSecManagerReasonCode;
}


void DrmMetaDataEvent::setResponseCode(long code)
{
	mResponseCode = code;
}


void DrmMetaDataEvent::setSecManagerReasonCode(long code)
{
	mSecManagerReasonCode = code;
}


bool DrmMetaDataEvent::getSecclientError() const
{
	return mSecclientError;
}


void DrmMetaDataEvent::setSecclientError(bool secClientError)
{
	mSecclientError = secClientError;
}


AnomalyReportEvent::AnomalyReportEvent(int severity, const std::string &msg):
		AAMPEventObject(AAMP_EVENT_REPORT_ANOMALY), mSeverity(severity), mMsg(msg)
{

}


int AnomalyReportEvent::getSeverity() const
{
	return mSeverity;
}


const std::string &AnomalyReportEvent::getMessage() const
{
	return mMsg;
}


WebVttCueEvent::WebVttCueEvent(VTTCue* cueData):
		AAMPEventObject(AAMP_EVENT_WEBVTT_CUE_DATA), mCueData(cueData)
{

}


VTTCue* WebVttCueEvent::getCueData() const
{
	return mCueData;
}


AdResolvedEvent::AdResolvedEvent(bool resolveStatus, const std::string &adId, uint64_t startMS, uint64_t durationMs):
		AAMPEventObject(AAMP_EVENT_AD_RESOLVED), mResolveStatus(resolveStatus), mAdId(adId),
		mStartMS(startMS), mDurationMs(durationMs)
{

}


bool AdResolvedEvent::getResolveStatus() const
{
	return mResolveStatus;
}


const std::string &AdResolvedEvent::getAdId() const
{
	return mAdId;
}


uint64_t AdResolvedEvent::getStart() const
{
	return mStartMS;
}


uint64_t AdResolvedEvent::getDuration() const
{
	return mDurationMs;
}


AdReservationEvent::AdReservationEvent(AAMPEventType evtType, const std::string &breakId, uint64_t position):
		AAMPEventObject(evtType), mAdBreakId(breakId), mPosition(position)
{

}


const std::string &AdReservationEvent::getAdBreakId() const
{
	return mAdBreakId;
}


uint64_t AdReservationEvent::getPosition() const
{
	return mPosition;
}


AdPlacementEvent::AdPlacementEvent(AAMPEventType evtType, const std::string &adId, uint32_t position, uint32_t offset, uint32_t duration, int errorCode):
		AAMPEventObject(evtType), mAdId(adId), mPosition(position),
		mOffset(offset), mDuration(duration), mErrorCode(errorCode)
{

}


const std::string &AdPlacementEvent::getAdId() const
{
	return mAdId;
}


uint32_t AdPlacementEvent::getPosition() const
{
	return mPosition;
}


uint32_t AdPlacementEvent::getOffset() const
{
	return mOffset;
}


uint32_t AdPlacementEvent::getDuration() const
{
	return mDuration;
}


int AdPlacementEvent::getErrorCode() const
{
	return mErrorCode;
}


MetricsDataEvent::MetricsDataEvent(MetricsDataType dataType, const std::string &uuid, const std::string &data):
		AAMPEventObject(AAMP_EVENT_REPORT_METRICS_DATA), mMetricsDataType(dataType),
		mMetricUUID(uuid), mMetricsData(data)
{

}


MetricsDataType MetricsDataEvent::getMetricsDataType() const
{
	return mMetricsDataType;
}


const std::string &MetricsDataEvent::getMetricUUID() const
{
	return mMetricUUID;
}


const std::string &MetricsDataEvent::getMetricsData() const
{
	return mMetricsData;
}


ID3MetadataEvent::ID3MetadataEvent(const std::vector<uint8_t> &metadata, const std::string &schIDUri, std::string &id3Value, uint32_t timeScale, uint64_t presentationTime, uint32_t eventDuration, uint32_t id, uint64_t timestampOffset):
		AAMPEventObject(AAMP_EVENT_ID3_METADATA), mMetadata(metadata), mId(id), mTimeScale(timeScale), mSchemeIdUri(schIDUri), mValue(id3Value), mEventDuration(eventDuration), mPresentationTime(presentationTime), mTimestampOffset(timestampOffset)
{

}


const std::vector<uint8_t> &ID3MetadataEvent::getMetadata() const
{
	return mMetadata;
}


int ID3MetadataEvent::getMetadataSize() const
{
	return mMetadata.size();
}


uint32_t ID3MetadataEvent::getTimeScale() const
{
	return mTimeScale;
}


uint32_t ID3MetadataEvent::getEventDuration() const
{
	return mEventDuration;
}


uint32_t ID3MetadataEvent::getId() const
{
	return mId;
}


uint64_t ID3MetadataEvent::getPresentationTime() const
{
	return mPresentationTime;
}


uint64_t ID3MetadataEvent::getTimestampOffset() const
{
        return mTimestampOffset;
}


const std::string& ID3MetadataEvent::getSchemeIdUri() const
{
	return mSchemeIdUri;
}


const std::string& ID3MetadataEvent::getValue() const
{
	return mValue;
}


DrmMessageEvent::DrmMessageEvent(const std::string &msg):
		AAMPEventObject(AAMP_EVENT_DRM_MESSAGE), mMessage(msg)
{

}


const std::string &DrmMessageEvent::getMessage() const
{
	return mMessage;
}


ContentGapEvent::ContentGapEvent(double time, double duration):
		AAMPEventObject(AAMP_EVENT_CONTENT_GAP)
		, mTime(time), mDuration(duration)
{

}


double ContentGapEvent::getTime() const
{
	return mTime;
}


double ContentGapEvent::getDuration() const
{
	return mDuration;
}


HTTPResponseHeaderEvent::HTTPResponseHeaderEvent(const std::string &header, const std::string &response):
		AAMPEventObject(AAMP_EVENT_HTTP_RESPONSE_HEADER)
		, mHeaderName(header), mHeaderResponse(response)
{

}


const std::string &HTTPResponseHeaderEvent::getHeader() const
{
	return mHeaderName;
}


const std::string &HTTPResponseHeaderEvent::getResponse() const
{
	return mHeaderResponse;
}
