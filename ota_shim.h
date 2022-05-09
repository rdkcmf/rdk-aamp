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
    /**
     * @brief StreamAbstractionAAMP_OTA Constructor
     * @param aamp pointer to PrivateInstanceAAMP object associated with player
     * @param seek_pos Seek position
     * @param rate playback rate
     */
    StreamAbstractionAAMP_OTA(AampLogManager *logObj, class PrivateInstanceAAMP *aamp,double seekpos, float rate);
    /**
     * @brief StreamAbstractionAAMP_OTA Distructor
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
     * @brief Stub implementation
     */
    void DumpProfiles(void) override;
    /**
     *   @brief  Starts streaming.
     */
    void Start() override;
    /**
     *   @brief  Stops streaming.
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
     * @param[out]  auxAudioOutputFormat - format of aux audio track
     */
    void GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat) override;
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
     * @brief Get the Buffered time
     *
     */
    double GetBufferedDuration() override;
    /**
     * @brief Check if initial Caching is supports
     *
     */
    bool IsInitialCachingSupported() override;
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
    /*
     * @brief Gets Max Bitrate avialable for current playback.
     * @return long MAX video bitrates
     */
    long GetMaxBitrate(void) override;
    /**
     *   @brief  Stops injecting fragments to StreamSink.
     */
    void StopInjection(void) override;
    /**
     *   @brief  Start injecting fragments to StreamSink.
     */
    void StartInjection(void) override;
    /*
     *   @brief update the Seek position
     */
    void SeekPosUpdate(double) { };
    /**
     * @brief SetVideoRectangle sets the position coordinates (x,y) & size (w,h)
     *
     * @param[in] x,y - position coordinates of video rectangle
     * @param[in] wxh - width & height of video rectangle
     */
    void SetVideoRectangle(int x, int y, int w, int h) override;
    /**
     * @brief SetAudioTrack sets a specific audio track
     *
     * @param[in] Index of the audio track.
     */
    void SetAudioTrack(int index) override;
    /**
     * @brief SetAudioTrackByLanguage set the audio language
     *
     * @param[in] lang : Audio Language to be set
     */
    void SetAudioTrackByLanguage(const char* lang) override;
    /**
     *   @brief Get the list of available audio tracks
     *
     *   @return std::vector<AudioTrackInfo> List of available audio tracks
     */
    std::vector<AudioTrackInfo> &GetAvailableAudioTracks(bool allTrack=false) override;
    /**
     *   @brief Get current audio track
     *
     *   @return int - index of current audio track
     */
    int GetAudioTrack() override;
    /**
     *   @brief Get current audio track
     *
     *   @return int - index of current audio track
     */
    bool GetCurrentAudioTrack(AudioTrackInfo &audioTrack) override;
    /**
     *   @brief Get the list of available text tracks
     *
     *   @return std::vector<TextTrackInfo> List of available text tracks
     */
    std::vector<TextTrackInfo> &GetAvailableTextTracks() override;
    /**
     * @brief SetPreferredAudioLanguages set the preferred audio language list
     *
     */
    void SetPreferredAudioLanguages() override;
    /**
     * @brief Disable Restrictions (unlock) till seconds mentioned
     *
     * @param[in] grace - seconds from current time, grace period, grace = -1 will allow an unlimited grace period
     * @param[in] time - seconds from current time,time till which the channel need to be kept unlocked
     * @param[in] eventChange - disable restriction handling till next program event boundary
     */
    void DisableContentRestrictions(long grace, long time, bool eventChange) override;
    /**
     * @brief Enable Content Restriction (lock)
     *
     */
    void EnableContentRestrictions() override;
    /**
     * @brief To get the available video tracks.
     * @return available video tracks
     */
    std::vector<StreamInfo*> GetAvailableVideoTracks(void) override;
    /**
     * @brief To get the available thumbnail tracks.
     * @return available thumbnail tracks
     */
    std::vector<StreamInfo*> GetAvailableThumbnailTracks(void) override;
    /**
     * @fn SetThumbnailTrack
     * @brief Function to set thumbnail track for processing
     *
     * @param thumbnail index value indicating the track to select
     * @return bool true on success.
     */
    bool SetThumbnailTrack(int) override;
    /**
     * @brief To get the available thumbnail tracks.
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
	 *   @brief  reads metadata properties from player status object and return true if any of data is changed
	 */
	bool PopulateMetaData(const JsonObject& parameters); /**< reads metadata properties from player status object and return true if any of data is changed  */
	void SendMediaMetadataEvent();
#endif
    /**
     * @brief GetAudioTracks get the available audio tracks for the selected service / media
     *
     */
    void GetAudioTracks();
    /**
     * @brief GetAudioTrackInternal get the primary key for the selected audio
     *
     */
    int GetAudioTrackInternal();
    /**
     * @brief NotifyAudioTrackChange To notify audio track change.Currently not used
     * as mediaplayer does not have support yet.
     *
     * @param[in] tracks - updated audio track info
     * @param[in]
     */
    void NotifyAudioTrackChange(const std::vector<AudioTrackInfo> &tracks);
    /**
     * @brief GetTextTracks get the available text tracks for the selected service / media
     *
     */
    void GetTextTracks();
protected:
    /**
     *   @brief Get stream information of a profile from subclass.
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
 


