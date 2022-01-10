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
    StreamAbstractionAAMP_VIDEOIN(const std::string name, const std::string callSign, AampLogManager *logObj, class PrivateInstanceAAMP *aamp,double seekpos, float rate);
    ~StreamAbstractionAAMP_VIDEOIN();
    StreamAbstractionAAMP_VIDEOIN(const StreamAbstractionAAMP_VIDEOIN&) = delete;
    StreamAbstractionAAMP_VIDEOIN& operator=(const StreamAbstractionAAMP_VIDEOIN&) = delete;
    void DumpProfiles(void) override;
    void Start() override;
    void Stop(bool clearChannelData) override;
    void SetVideoRectangle(int x, int y, int w, int h) override;
    AAMPStatusType Init(TuneType tuneType) override;
    void GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat, StreamOutputFormat &subtitleOutputFormat) override;
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
protected:
    StreamInfo* GetStreamInfo(int idx) override;
    AAMPStatusType InitHelper(TuneType tuneType);
    void StartHelper(int port, const std::string & methodName);
    void StopHelper(const std::string & methodName) ;
    bool mTuned;

#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
    void RegisterEvent (string eventName, std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> functionHandler);
    void RegisterAllEvents ();
#endif

private:
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
    ThunderAccessAAMP thunderAccessObj;
    ThunderAccessAAMP thunderRDKShellObj;
    /*Event Handler*/
    void OnInputStatusChanged(const JsonObject& parameters);
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

