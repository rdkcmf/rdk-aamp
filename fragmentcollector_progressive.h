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
 * @file fragmentcollector_progressive.h
 * @brief Streamer for progressive mp3/mp4 playback
 */

#ifndef FRAGMENTCOLLECTOR_PROGRESSIVE_H_
#define FRAGMENTCOLLECTOR_PROGRESSIVE_H_

#include "StreamAbstractionAAMP.h"
#include <string>
#include <stdint.h>
using namespace std;

/**
 * @class StreamAbstractionAAMP_PROGRESSIVE
 * @brief Streamer for progressive mp3/mp4 playback
 */
class StreamAbstractionAAMP_PROGRESSIVE : public StreamAbstractionAAMP
{
public:
    /**
     * @fn StreamAbstractionAAMP_PROGRESSIVE
     * @param aamp pointer to PrivateInstanceAAMP object associated with player
     * @param seekpos Seek position
     * @param rate playback rate
     */
    StreamAbstractionAAMP_PROGRESSIVE(AampLogManager *logObj, class PrivateInstanceAAMP *aamp,double seekpos, float rate);
    /**
     * @fn ~StreamAbstractionAAMP_PROGRESSIVE
     */
    ~StreamAbstractionAAMP_PROGRESSIVE();
    /**
     * @brief Copy constructor disabled
     *
     */
    StreamAbstractionAAMP_PROGRESSIVE(const StreamAbstractionAAMP_PROGRESSIVE&) = delete;
    /**
     * @brief assignment operator disabled
     *
     */
    StreamAbstractionAAMP_PROGRESSIVE& operator=(const StreamAbstractionAAMP_PROGRESSIVE&) = delete;
    double seekPosition;
    void DumpProfiles(void) override;
    /**
     *   @fn Start
     *   @return void
     */
    void Start() override;
    /**
     *   @fn Stop
     *   @return void
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
     *
     * @param[out]  primaryOutputFormat - format of primary track
     * @param[out]  audioOutputFormat - format of audio track
     * @param[out]  auxAudioOutputFormat - format of aux audio track
     */
    void GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat) override;
    /**
     * @fn GetStreamPosition 
     *
     * @retval current position of stream.
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
     *  @fn GetBufferedDuration
     *
     */
    double GetBufferedDuration() override;
    /**
     *  @fn IsInitialCachingSupported
     *
     */
    bool IsInitialCachingSupported() override;
    /**
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
     * @fn FetcherLoop
     * @return void
     */
    void FetcherLoop();
    /**
     * @fn GetAvailableVideoTracks
     * @return available video tracks.
     */
    std::vector<StreamInfo*> GetAvailableVideoTracks(void) override;
    /**
     * @fn GetAvailableThumbnailTracks
     * @return available thumbnail tracks.
     */
    std::vector<StreamInfo*> GetAvailableThumbnailTracks(void) override;
    /**
     * @fn SetThumbnailTrack
     *
     * @param thumbnailIndex thumbnail index value indicating the track to select
     * @return bool true on success.
     */
    bool SetThumbnailTrack(int) override;
    /**
     * @fn GetThumbnailRangeData
     * @return Updated vector of available thumbnail data.
     */
    std::vector<ThumbnailData> GetThumbnailRangeData(double,double, std::string*, int*, int*, int*, int*) override;
protected:
    /**
     *   @fn GetStreamInfo
     *
     *   @param[in]  idx - profile index.
     *   @retval stream information corresponding to index.
     */
    StreamInfo* GetStreamInfo(int idx) override;
private:
    void StreamFile( const char *uri, long *http_error );
    bool fragmentCollectorThreadStarted;
    pthread_t fragmentCollectorThreadID;
};

#endif //FRAGMENTCOLLECTOR_PROGRESSIVE_H_
/**
 * @}
 */
 


