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
 * @file ota_shim.h
 * @brief shim for dispatching UVE OTA ATSC playback
 */

#ifndef OTA_SHIM_H_
#define OTA_SHIM_H_

#include "StreamAbstractionAAMP.h"
#include <string>
#include <stdint.h>
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
#include "Module.h"
#include <core/core.h>
#include "ThunderAccess.h"
#endif
using namespace std;

/**
 * @brief Structure to save the ATSC settings
 */
typedef struct ATSCSettings
{
	std::string preferredLanguages;
	std::string preferredRendition;
}ATSCGlobalSettings;

/**
 * @class StreamAbstractionAAMP_OTA
 * @brief Fragment collector for OTA
 */
class StreamAbstractionAAMP_OTA : public StreamAbstractionAAMP
{
public:
    StreamAbstractionAAMP_OTA(AampLogManager *logObj, class PrivateInstanceAAMP *aamp,double seekpos, float rate);
    ~StreamAbstractionAAMP_OTA();
    StreamAbstractionAAMP_OTA(const StreamAbstractionAAMP_OTA&) = delete;
    StreamAbstractionAAMP_OTA& operator=(const StreamAbstractionAAMP_OTA&) = delete;

#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
    /*Event Handler*/
    void onPlayerStatusHandler(const JsonObject& parameters);
#endif
    void DumpProfiles(void) override;
    void Start() override;
    void Stop(bool clearChannelData) override;
    AAMPStatusType Init(TuneType tuneType) override;
    void GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat) override;
    double GetStreamPosition() override;
    MediaTrack* GetMediaTrack(TrackType type) override;
    double GetFirstPTS() override;
    double GetStartTimeOfFirstPTS() override;
    double GetBufferedDuration() override;
    bool IsInitialCachingSupported() override;
    int GetBWIndex(long bitrate) override;
    std::vector<long> GetVideoBitrates(void) override;
    std::vector<long> GetAudioBitrates(void) override;
    long GetMaxBitrate(void) override;
    void StopInjection(void) override;
    void StartInjection(void) override;
    void SeekPosUpdate(double) { };
    void SetVideoRectangle(int x, int y, int w, int h) override;
    void SetAudioTrack(int index) override;
    void SetAudioTrackByLanguage(const char* lang) override;
    std::vector<AudioTrackInfo> &GetAvailableAudioTracks(bool allTrack=false) override;
    int GetAudioTrack() override;
    bool GetCurrentAudioTrack(AudioTrackInfo &audioTrack) override;
    std::vector<TextTrackInfo> &GetAvailableTextTracks() override;
    void SetPreferredAudioLanguages() override;
    void DisableContentRestrictions(long grace, long time, bool eventChange) override;
    void EnableContentRestrictions() override;
    std::vector<StreamInfo*> GetAvailableThumbnailTracks(void) override;
    bool SetThumbnailTrack(int) override;
    std::vector<ThumbnailData> GetThumbnailRangeData(double,double, std::string*, int*, int*, int*, int*) override;
private:
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
    ThunderAccessAAMP thunderAccessObj;
    ThunderAccessAAMP mediaSettingsObj;
    std::string prevState;
    std::string prevBlockedReason;
    bool tuned;
    bool mEventSubscribed;

    ThunderAccessAAMP thunderRDKShellObj;
    bool GetScreenResolution(int & screenWidth, int & screenHeight);

	/* Additional data from ATSC playback  */
	std::string mPCRating; /**< Parental control rating json string object  */
	int mSsi;  /**<  Signal strength indicator 0-100 where 100 is a perfect signal. -1 indicates data not available  */
	/* Video info   */
	long mVideoBitrate;
	float mFrameRate;		/**< FrameRate */
	VideoScanType mVideoScanType;   /**< Video Scan Type progressive/interlaced */
	int mAspectRatioWidth;		/**< Aspect Ratio Width*/
	int mAspectRatioHeight;		/**< Aspect Ratio Height*/
	std::string mVideoCodec;	 /**<  VideoCodec - E.g MPEG2.*/
	std::string mHdrType; /**<  type of HDR being played, in example "DOLBY_VISION" */
	int miVideoWidth;       /**<  Video Width  */
	int miVideoHeight;       /**<  Video Height  */

	int miPrevmiVideoWidth;
	int miPrevmiVideoHeight;

	/* Audio Info   */
	long mAudioBitrate; 		/**<  int - Rate of the Audio stream in bps. Calculated based on transport stream rate. So will have some fluctuation. */
	std::string mAudioCodec; /**< AudioCodec E.g AC3.*/
	std::string mAudioMixType; /**<  AudioMixType(- E.g STEREO. */
	bool  mIsAtmos;  	 		/**<  Is Atmos : 1 - True if audio playing is Dolby Atmos, 0 false ,  -1 indicates data not available */

	bool PopulateMetaData(const JsonObject& parameters); /**< reads metadata properties from player status object and return true if any of data is changed  */
	void SendMediaMetadataEvent();
#endif
    void GetAudioTracks();
    int GetAudioTrackInternal();
    void NotifyAudioTrackChange(const std::vector<AudioTrackInfo> &tracks);
    void GetTextTracks();
protected:
    StreamInfo* GetStreamInfo(int idx) override;
};

#endif //OTA_SHIM_H_
/**
 * @}
 */
 


