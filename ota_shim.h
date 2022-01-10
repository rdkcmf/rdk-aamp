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
 * @struct ATSCSettings
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
    /**
     * @fn StreamAbstractionAAMP_OTA
     * @param aamp pointer to PrivateInstanceAAMP object associated with player
     * @param seek_pos Seek position
     * @param rate playback rate
     */
    StreamAbstractionAAMP_OTA(AampLogManager *logObj, class PrivateInstanceAAMP *aamp,double seekpos, float rate);
    /**
     * @fn ~StreamAbstractionAAMP_OTA
     */
    ~StreamAbstractionAAMP_OTA();
    /**
     * @brief Copy constructor disabled
     *
     */
    StreamAbstractionAAMP_OTA(const StreamAbstractionAAMP_OTA&) = delete;
    /**
     * @brief assignment operator disabled
     *
     */
    StreamAbstractionAAMP_OTA& operator=(const StreamAbstractionAAMP_OTA&) = delete;

#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
    /*Event Handler*/
    void onPlayerStatusHandler(const JsonObject& parameters);
#endif
    /**
     *   @fn DumpProfiles
     */
    void DumpProfiles(void) override;
    /**
     *   @fn Start
     */
    void Start() override;
    /**
     *   @fn Stop
     */
    void Stop(bool clearChannelData) override;
    /**
     *   @fn Init
     *   @note   To be implemented by sub classes
     *   @param  tuneType to set type of object.
     *   @retval true on success
     *   @retval false on failure
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
     *   @fn GetStreamPosition
     *
     *   @retval current position of stream.
     */
    double GetStreamPosition() override;
   /**
     *   @fn GetMediaTrack
     *
     *   @param[in]  type - track type
     *   @retval MediaTrack pointer.
     */
    MediaTrack* GetMediaTrack(TrackType type) override;
    /**
     *   @fn GetFirstPTS
     *
     *   @retval PTS of first sample
     */
    double GetFirstPTS() override;
    /**
     *   @fn GetStartTimeOfFirstPTS
     *
     *   @retval start time of first sample
     */
    double GetStartTimeOfFirstPTS() override;
    /**
     * @fn GetBufferedDuration
     *
     */
    double GetBufferedDuration() override;
    /**
     * @fn IsInitialCachingSupported
     */
    bool IsInitialCachingSupported() override;
   /**
     * @fn GetBWIndex
     * @param[in] bitrate Bitrate to lookup profile
     * @retval profile index
     */
    int GetBWIndex(long bitrate) override;
    /**
     * @fn GetVideoBitrates
     * @return available video bitrates
     */
    std::vector<long> GetVideoBitrates(void) override;
    /**
     * @fn GetAudioBitrates
     * @return available audio bitrates
     */
    std::vector<long> GetAudioBitrates(void) override;
    /**
     * @fn GetMaxBitrate
     * @return long MAX video bitrates
     */
    long GetMaxBitrate(void) override;
    /**
     *   @fn StopInjection
     */
    void StopInjection(void) override;
    /**
     *   @fn StartInjection
     */
    void StartInjection(void) override;
    /**
     *   @brief update the Seek position
     */
    void SeekPosUpdate(double) { };
    /**
     * @fn SetVideoRectangle
     *
     * @param[in] x,y - position coordinates of video rectangle
     * @param[in] wxh - width & height of video rectangle
     */
    void SetVideoRectangle(int x, int y, int w, int h) override;
    /**
     * @fn SetAudioTrack
     *
     * @param[in] Index of the audio track.
     */
    void SetAudioTrack(int index) override;
   /**
     * @fn SetAudioTrackByLanguage
     *
     * @param[in] lang : Audio Language to be set
     */
    void SetAudioTrackByLanguage(const char* lang) override;
    /**
     *   @fn GetAvailableAudioTracks
     *
     *   @return std::vector<AudioTrackInfo> List of available audio tracks
     */
    std::vector<AudioTrackInfo> &GetAvailableAudioTracks(bool allTrack=false) override;
    /**
     *   @fn GetAudioTrack
     *
     *   @return int - index of current audio track
     */
    int GetAudioTrack() override;
    /**
     *   @fn GetCurrentAudioTrack
     *   @return int - index of current audio track
     */
    bool GetCurrentAudioTrack(AudioTrackInfo &audioTrack) override;
    /**
     *   @fn GetAvailableTextTracks
     *   @return std::vector<TextTrackInfo> List of available text tracks
     */
    std::vector<TextTrackInfo> &GetAvailableTextTracks(bool all=false) override;
    /**
     * @fn SetPreferredAudioLanguages
     *
     */
    void SetPreferredAudioLanguages() override;
    /**
     * @fn DisableContentRestrictions
     *
     * @param[in] grace - seconds from current time, grace period, grace = -1 will allow an unlimited grace period
     * @param[in] time - seconds from current time,time till which the channel need to be kept unlocked
     * @param[in] eventChange - disable restriction handling till next program event boundary
     */
    void DisableContentRestrictions(long grace, long time, bool eventChange) override;
    /**
     * @fn EnableContentRestrictions
     *
     */
    void EnableContentRestrictions() override;
    /**
     * @fn GetAvailableVideoTracks
     * @return available video tracks
     */
    std::vector<StreamInfo*> GetAvailableVideoTracks(void) override;
    /**
     * @fn GetAvailableThumbnailTracks
     * @return available thumbnail tracks
     */
    std::vector<StreamInfo*> GetAvailableThumbnailTracks(void) override;
    /**
     * @fn SetThumbnailTrack
     *
     * @param thumbnail index value indicating the track to select
     * @return bool true on success.
     */
    bool SetThumbnailTrack(int) override;
    /**
     * @fn GetThumbnailRangeData
     * @return available thumbnail tracks
     */
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
	std::string mPCRating; 		/**< Parental control rating json string object  */
	int mSsi;  			/**<  Signal strength indicator 0-100 where 100 is a perfect signal. -1 indicates data not available  */
	/* Video info   */
	long mVideoBitrate;
	float mFrameRate;		/**< FrameRate */
	VideoScanType mVideoScanType;   /**< Video Scan Type progressive/interlaced */
	int mAspectRatioWidth;		/**< Aspect Ratio Width*/
	int mAspectRatioHeight;		/**< Aspect Ratio Height*/
	std::string mVideoCodec;	/**<  VideoCodec - E.g MPEG2.*/
	std::string mHdrType; 		/**<  type of HDR being played, in example "DOLBY_VISION" */
	int miVideoWidth;       	/**<  Video Width  */
	int miVideoHeight;       	/**<  Video Height  */

	int miPrevmiVideoWidth;
	int miPrevmiVideoHeight;

	/* Audio Info   */
	long mAudioBitrate; 		/**<  int - Rate of the Audio stream in bps. Calculated based on transport stream rate. So will have some fluctuation. */
	std::string mAudioCodec; 	/**< AudioCodec E.g AC3.*/
	std::string mAudioMixType; 	/**<  AudioMixType(- E.g STEREO. */
	bool  mIsAtmos;  	 	/**<  Is Atmos : 1 - True if audio playing is Dolby Atmos, 0 false ,  -1 indicates data not available */

	/**
	 *   @fn PopulateMetaData
	 */
	bool PopulateMetaData(const JsonObject& parameters); /**< reads metadata properties from player status object and return true if any of data is changed  */
	void SendMediaMetadataEvent();
#endif
    /**
     * @fn GetAudioTracks
     * @return void
     */
    void GetAudioTracks();
    /**
     * @fn GetAudioTrackInternal
     *
     */
    int GetAudioTrackInternal();
    /**
     * @fn NotifyAudioTrackChange
     *
     * @param[in] tracks - updated audio track info
     */
    void NotifyAudioTrackChange(const std::vector<AudioTrackInfo> &tracks);
    /**
     * @fn GetTextTracks
     * @return voi @return void
     */
    void GetTextTracks();
protected:
    /**
     *   @fn GetStreamInfo
     *
     *   @param[in]  idx - profile index.
     *   @retval stream information corresponding to index.
     */
    StreamInfo* GetStreamInfo(int idx) override;
};

#endif //OTA_SHIM_H_
/**
 * @}
 */
 


