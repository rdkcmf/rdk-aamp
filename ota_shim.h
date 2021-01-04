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
#include <core/core.h>
#include "ThunderAccess.h"

//moc parental control
#include <thread>
#include <chrono>

#endif
using namespace std;

#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
//moc parental control

/**
 * @brief Structure holding the pc config
 */
struct pcConfigItem
{
        long pgmDuration;      /**< Program duration based on config file*/
        std::string pgmRestrictions;     /**< Restriction set for the program*/
};


class cTimer{
    private:
        bool clear;
        int interval;
        void (*callBack_function)();
    public:
        /***
         * @brief    : Constructor.
         * @return   : nil.
         */
        cTimer();

        /***
         * @brief    : Destructor.
         * @return   : nil.
         */
        ~cTimer();

        /***
         * @brief    : start timer thread.
         * @return   : <bool> False if timer thread couldn't be started.
         */
        bool start ();

        /***
         * @brief   : stop timer thread.
         * @return   : nil
         */
        void stop();

        /***
         * @brief        : Set Callback function
         * @param1[in]   : function which has to be invoked on timed intervals
         * @return       : nil
         */
        void setCallback(void (*function)());
	/***
	 * @brief     : Set interval in which the given function should be invoked.
	 * @param2[in]  : timer interval val.
	 * @return     : nil
	 */
	void setInterval(int val);

};

#endif

/**
 * @class StreamAbstractionAAMP_OTA
 * @brief Fragment collector for OTA
 */
class StreamAbstractionAAMP_OTA : public StreamAbstractionAAMP
{
public:
    StreamAbstractionAAMP_OTA(class PrivateInstanceAAMP *aamp,double seekpos, float rate);
    ~StreamAbstractionAAMP_OTA();
    StreamAbstractionAAMP_OTA(const StreamAbstractionAAMP_OTA&) = delete;
    StreamAbstractionAAMP_OTA& operator=(const StreamAbstractionAAMP_OTA&) = delete;

#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
    /*Event Handler*/
    void onPlayerStatusHandler(const JsonObject& parameters);
    void onContentRestrictedHandler(bool locked);
#endif
    void DumpProfiles(void) override;
    void Start() override;
    void Stop(bool clearChannelData) override;
    AAMPStatusType Init(TuneType tuneType) override;
    void GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat) override;
    double GetStreamPosition() override;
    MediaTrack* GetMediaTrack(TrackType type) override;
    double GetFirstPTS() override;
    double GetBufferedDuration() override;
    bool IsInitialCachingSupported() override;
    int GetBWIndex(long bitrate) override;
    std::vector<long> GetVideoBitrates(void) override;
    std::vector<long> GetAudioBitrates(void) override;
    long GetMaxBitrate(void) override;
    void StopInjection(void) override;
    void StartInjection(void) override;
    void SeekPosUpdate(double) { };
    void NotifyFirstVideoPTS(unsigned long long pts) { };
    void SetVideoRectangle(int x, int y, int w, int h) override;
    void SetAudioTrack(int index) override;
    void SetAudioTrackByLanguage(const char* lang) override;
    std::vector<AudioTrackInfo> &GetAvailableAudioTracks() override;
    int GetAudioTrack() override;
    std::vector<TextTrackInfo> &GetAvailableTextTracks() override;
    void ApplyContentRestrictions(std::vector<std::string> restrictions) override;
    std::vector<std::string> GetContentRestrictions() override;
    void DisableContentRestrictions(long secondsRelativeToCurrentTime) override;
    void DisableContentRestrictions(bool untilProgramChange) override;
    void EnableContentRestrictions() override;
    std::vector<StreamInfo*> GetAvailableThumbnailTracks(void) override;
    bool SetThumbnailTrack(int) override;
    std::vector<ThumbnailData> GetThumbnailRangeData(double,double, std::string*, int*, int*, int*, int*) override;
private:

#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
    ThunderAccessAAMP thunderAccessObj;
    ThunderAccessAAMP mediaSettingsObj;
    std::string prevState;
    bool tuned;
    bool mEventSubscribed;

    ThunderAccessAAMP thunderRDKShellObj;
    bool GetScreenResolution(int & screenWidth, int & screenHeight);

//moc parental control
    /*Temporarily saving restrictions at ota till the PC APIs are ready*/
    std::vector<std::string> restrictionsOta;
    static std::vector<std::string> currentPgmRestrictions;
    bool m_unlocktimeout, m_unlockpc, m_unlockfull;
    static cTimer m_lockTimer;
    static void mocLockCallback();
    static cTimer m_pcTimer;
    static void mocProgramChangeCallback();
    static bool m_isPcConfigRead;
    static std::vector<pcConfigItem> m_pcProgramList;
    static int m_pcProgramCount;
    void ProcessPcConfigEntry(std::string cfg);
    int ReadConfigStringHelper(std::string buf, const char *prefixPtr, const char **valueCopyPtr);
    int ReadConfigNumericHelper(std::string buf, const char* prefixPtr, long& value);
#endif
    void GetAudioTracks();
    void SetPreferredAudioLanguage();
    int GetAudioTrackInternal();
    void NotifyAudioTrackChange(const std::vector<AudioTrackInfo> &tracks);
    void GetTextTracks();
protected:
    StreamInfo* GetStreamInfo(int idx) override;

#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
//moc parental control
public:
    static StreamAbstractionAAMP_OTA* _instance;
#endif

};

#endif //OTA_SHIM_H_
/**
 * @}
 */
 


