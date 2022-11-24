/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2021 RDK Management
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
 * @file videoin_shim.h
 * @brief shim for dispatching UVE Video input playback
 */

#ifndef VIDEOIN_SHIM_H_
#define VIDEOIN_SHIM_H_

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
 * @class StreamAbstractionAAMP_VIDEOIN
 * @brief Fragment collector for MPEG DASH
 */
class StreamAbstractionAAMP_VIDEOIN : public StreamAbstractionAAMP
{
public:
    /**
     * @fn StreamAbstractionAAMP_VIDEOIN
     * @param aamp pointer to PrivateInstanceAAMP object associated with player
     * @param seekpos Seek position
     * @param rate playback rate
     */
    StreamAbstractionAAMP_VIDEOIN(const std::string name, const std::string callSign, AampLogManager *logObj, class PrivateInstanceAAMP *aamp,double seekpos, float rate);
    /**
     * @fn ~StreamAbstractionAAMP_VIDEOIN
     */
    ~StreamAbstractionAAMP_VIDEOIN();
    /**     
     * @brief Copy constructor disabled
     *
     */
    StreamAbstractionAAMP_VIDEOIN(const StreamAbstractionAAMP_VIDEOIN&) = delete;
    /**
     * @brief assignment operator disabled
     *
     */
    StreamAbstractionAAMP_VIDEOIN& operator=(const StreamAbstractionAAMP_VIDEOIN&) = delete;
    /**
     * @brief Stub implementation
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
     * @fn SetVideoRectangle
     * 
     * @param[in] x,y - position coordinates of video rectangle
     * @param[in] w,h - width & height of video rectangle
     */
    void SetVideoRectangle(int x, int y, int w, int h) override;
   /**
     *   @fn Init
     *   @note   To be implemented by sub classes
     *   @param  tuneType to set type of object.
     *   @retval eAAMPSTATUS_OK
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
     * @fn Is4KStream
     * @brief check if current stream have 4K content
     * @param height - resolution of 4K stream if found
     * @param bandwidth - bandwidth of 4K stream if foudd
     * @return true on success 
     */
    virtual bool Is4KStream(int &height, long &bandwidth) override {return false; };
    
    /**
     * @fn GetStreamPosition
     *
     * @retval current position of stream. 
     */
    double GetStreamPosition() override;
    /**
     *   @fn GetMediaTrack
     *   @param[in] type - track type
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
     */
    double GetBufferedDuration() override;
    /**
     * @fn IsInitialCachingSupported
     * @return true if yes 
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
    void SeekPosUpdate(double) { };

	/**
	 * @fn UpdateFailedDRMStatus
	 * @brief Function to update the failed DRM status to mark the adaptation sets to be omitted
	 * @param[in] object  - Prefetch object instance which failed
	 */
	void UpdateFailedDRMStatus(LicensePreFetchObject *object) override { }

protected:
    /**
     *   @fn GetStreamInfo
     *
     *   @param[in]  idx - profile index.
     *   @retval stream information corresponding to index.
     */
    StreamInfo* GetStreamInfo(int idx) override;
    AAMPStatusType InitHelper(TuneType tuneType);
    /**
     *   @fn StartHelper
     */
    void StartHelper(int port, const std::string & methodName);
    /**
     *   @fn StopHelper
     */
    void StopHelper(const std::string & methodName) ;
    bool mTuned;

#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
    /**
     *   @fn RegisterEvent
     *   @param[in] eventName : Event name
     *   @param[in] functionHandler : Event funciton pointer
     */
    void RegisterEvent (string eventName, std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> functionHandler);
    /**
     *   @fn RegisterAllEvents
     */
    void RegisterAllEvents ();
#endif

private:
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
    ThunderAccessAAMP thunderAccessObj;
    ThunderAccessAAMP thunderRDKShellObj;
    
    /**
     *  @fn OnInputStatusChanged
     *  @return void
     */
    void OnInputStatusChanged(const JsonObject& parameters);
    /** 
     *  @fn OnSignalChanged
     *  @param parameters Json object 
     *  @return void
     */
    void OnSignalChanged(const JsonObject& parameters);
#endif
    bool GetScreenResolution(int & screenWidth, int & screenHeight);
    int videoInputPort;
    std::string mName; // Used for logging
    std::list<std::string> mRegisteredEvents;
};

#endif // VIDEOIN_SHIM_H_
/**
 * @}
 */

