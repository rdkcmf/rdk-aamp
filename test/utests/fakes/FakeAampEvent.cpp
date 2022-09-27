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

#include "AampEvent.h"

AAMPEventObject::AAMPEventObject(AAMPEventType type) : mType(type)
{
}

const std::string &MediaErrorEvent::getDescription() const
{
	return mDescription;
}

AAMPEventType AAMPEventObject::getType() const
{
	return mType;
}

ID3MetadataEvent::ID3MetadataEvent(const std::vector<uint8_t> &metadata, const std::string &schIDUri, std::string &id3Value, uint32_t timeScale, uint64_t presentationTime, uint32_t eventDuration, uint32_t id, uint64_t timestampOffset):
		AAMPEventObject(AAMP_EVENT_ID3_METADATA)
{
}

const std::vector<uint8_t> &ID3MetadataEvent::getMetadata() const
{
	return mMetadata;
}

int ID3MetadataEvent::getMetadataSize() const
{
	return 0;
}

uint32_t ID3MetadataEvent::getTimeScale() const
{
	return 0;
}

uint32_t ID3MetadataEvent::getId() const
{
	return 0;
}

uint64_t ID3MetadataEvent::getPresentationTime() const
{
	return 0;
}

uint64_t ID3MetadataEvent::getTimestampOffset() const
{
        return 0;
}

uint32_t ID3MetadataEvent::getEventDuration() const
{
	return 0;
}

const std::string& ID3MetadataEvent::getValue() const
{
	return mValue;
}

const std::string& ID3MetadataEvent::getSchemeIdUri() const
{
	return mSchemeIdUri;
}

MediaMetadataEvent::MediaMetadataEvent(long duration, int width, int height, bool hasDrm, bool isLive, const std::string &DrmType, double programStartTime):
		AAMPEventObject(AAMP_EVENT_MEDIA_METADATA)
{
}

void MediaMetadataEvent::addSupportedSpeed(float speed)
{
}

void MediaMetadataEvent::addBitrate(long bitrate)
{
}

void MediaMetadataEvent::addLanguage(const std::string &lang)
{
}

DrmMetaDataEvent::DrmMetaDataEvent(AAMPTuneFailure failure, const std::string &accessStatus, int statusValue, long responseCode, bool secclientErr):
    AAMPEventObject(AAMP_EVENT_DRM_METADATA)
{
}

AAMPTuneFailure DrmMetaDataEvent::getFailure() const
{
	return AAMP_TUNE_INIT_FAILED;
}

long DrmMetaDataEvent::getResponseCode() const
{
    return 0;
}

const std::string &DrmMetaDataEvent::getAccessStatus() const
{
	return mAccessStatus;
}

void DrmMetaDataEvent::setAccessStatus(const std::string &status)
{
}

int DrmMetaDataEvent::getAccessStatusValue() const
{
	return 0;
}

bool DrmMetaDataEvent::getSecclientError() const
{
	return false;
}

int32_t DrmMetaDataEvent::getSecManagerReasonCode() const
{
	return 0;
}

int32_t DrmMetaDataEvent::getSecManagerClassCode() const
{
	return 0;
}

int32_t DrmMetaDataEvent::getBusinessStatus() const
{
	return 0;
}

DrmMessageEvent::DrmMessageEvent(const std::string &msg):
		AAMPEventObject(AAMP_EVENT_DRM_MESSAGE)
{
}

AnomalyReportEvent::AnomalyReportEvent(int severity, const std::string &msg):
		AAMPEventObject(AAMP_EVENT_REPORT_ANOMALY)
{
}

int AnomalyReportEvent::getSeverity() const
{
	return 0;
}

BufferingChangedEvent::BufferingChangedEvent(bool buffering):
		AAMPEventObject(AAMP_EVENT_BUFFERING_CHANGED)
{
}

bool BufferingChangedEvent::buffering() const
{
	return false;
}

ProgressEvent::ProgressEvent(double duration, double position, double start, double end, float speed, long long pts, double bufferedDuration, std::string seiTimecode):
		AAMPEventObject(AAMP_EVENT_PROGRESS)
{
}

SpeedChangedEvent::SpeedChangedEvent(float rate):
		AAMPEventObject(AAMP_EVENT_SPEED_CHANGED)
{
}

TimedMetadataEvent::TimedMetadataEvent(const std::string &name, const std::string &id, double time, double duration, const std::string &content):
		AAMPEventObject(AAMP_EVENT_TIMED_METADATA)
{
}

CCHandleEvent::CCHandleEvent(unsigned long handle):
		AAMPEventObject(AAMP_EVENT_CC_HANDLE_RECEIVED)
{
}

SupportedSpeedsChangedEvent::SupportedSpeedsChangedEvent():
		AAMPEventObject(AAMP_EVENT_SPEEDS_CHANGED)
{
}

void SupportedSpeedsChangedEvent::addSupportedSpeed(float speed)
{
}

const std::vector<float> &SupportedSpeedsChangedEvent::getSupportedSpeeds() const
{
	return mSupportedSpeeds;
}

int SupportedSpeedsChangedEvent::getSupportedSpeedCount() const
{
	return 0;
}

MediaErrorEvent::MediaErrorEvent(AAMPTuneFailure failure, int code, const std::string &desc, bool shouldRetry, int classCode, int reason, int businessStatus):
		AAMPEventObject(AAMP_EVENT_TUNE_FAILED)
{
}

BitrateChangeEvent::BitrateChangeEvent(int time, long bitrate, const std::string &desc, int width, int height, double frameRate, double position, bool cappedProfile, int displayWidth, int displayHeight, VideoScanType videoScanType, int aspectRatioWidth, int aspectRatioHeight):
		AAMPEventObject(AAMP_EVENT_BITRATE_CHANGED)
{
}

BulkTimedMetadataEvent::BulkTimedMetadataEvent(const std::string &content):
		AAMPEventObject(AAMP_EVENT_BULK_TIMED_METADATA)
{
}

StateChangedEvent::StateChangedEvent(PrivAAMPState state):
		AAMPEventObject(AAMP_EVENT_STATE_CHANGED)
{
}

SeekedEvent::SeekedEvent(double positionMS):
		AAMPEventObject(AAMP_EVENT_SEEKED)
{
}

TuneProfilingEvent::TuneProfilingEvent(std::string &profilingData):
		AAMPEventObject(AAMP_EVENT_TUNE_PROFILING)
{
}

AdResolvedEvent::AdResolvedEvent(bool resolveStatus, const std::string &adId, uint64_t startMS, uint64_t durationMs):
		AAMPEventObject(AAMP_EVENT_AD_RESOLVED)
{
}

AdReservationEvent::AdReservationEvent(AAMPEventType evtType, const std::string &breakId, uint64_t position):
		AAMPEventObject(evtType)
{
}

AdPlacementEvent::AdPlacementEvent(AAMPEventType evtType, const std::string &adId, uint32_t position, uint32_t offset, uint32_t duration, int errorCode):
		AAMPEventObject(evtType)
{
}

const std::string &AdPlacementEvent::getAdId() const
{
	return mAdId;
}

uint32_t AdPlacementEvent::getPosition() const
{
	return 0;
}

uint32_t AdPlacementEvent::getOffset() const
{
	return 0;
}

uint32_t AdPlacementEvent::getDuration() const
{
	return 0;
}

WebVttCueEvent::WebVttCueEvent(VTTCue* cueData):
		AAMPEventObject(AAMP_EVENT_WEBVTT_CUE_DATA)
{
}

ContentGapEvent::ContentGapEvent(double time, double duration):
		AAMPEventObject(AAMP_EVENT_CONTENT_GAP)
{
}

HTTPResponseHeaderEvent::HTTPResponseHeaderEvent(const std::string &header, const std::string &response):
		AAMPEventObject(AAMP_EVENT_HTTP_RESPONSE_HEADER)
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

ContentProtectionDataEvent::ContentProtectionDataEvent(const std::vector<uint8_t> &keyID, const std::string &streamType):
	AAMPEventObject(AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE)
{
}

/**
 * @brief MetricsDataEvent Constructor
 */
MetricsDataEvent::MetricsDataEvent(MetricsDataType dataType, const std::string &uuid, const std::string &data):
		AAMPEventObject(AAMP_EVENT_REPORT_METRICS_DATA)
{
}
