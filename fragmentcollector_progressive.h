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
     * @brief StreamAbstractionAAMP_PROGRESSIVE Constructor
     * @param aamp pointer to PrivateInstanceAAMP object associated with player
     * @param seekpos Seek position
     * @param rate playback rate
     */
    StreamAbstractionAAMP_PROGRESSIVE(AampLogManager *logObj, class PrivateInstanceAAMP *aamp,double seekpos, float rate);
    /**
     * @brief StreamAbstractionAAMP_PROGRESSIVE Destructor
     */
    ~StreamAbstractionAAMP_PROGRESSIVE();
    
    StreamAbstractionAAMP_PROGRESSIVE(const StreamAbstractionAAMP_PROGRESSIVE&) = delete;
    StreamAbstractionAAMP_PROGRESSIVE& operator=(const StreamAbstractionAAMP_PROGRESSIVE&) = delete;
 
    double seekPosition;
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
     *  @broef Get the Buffered duration
     *
     */
    double GetBufferedDuration() override;
    /*
     *  @brief check whether initial caching data supported
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
    /**
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
    void SeekPosUpdate(double) { };
    /**
     * @brief harvest chunks from large mp3/mp4
     */
    void FetcherLoop();
    /**
     * @brief To get the available video tracks.
     * @return available video tracks.
     */
    std::vector<StreamInfo*> GetAvailableVideoTracks(void) override;
    /**
     * @brief To get the available thumbnail tracks.
     * @return available thumbnail tracks.
     */
    std::vector<StreamInfo*> GetAvailableThumbnailTracks(void) override;
    /**
     * @fn SetThumbnailTrack
     * @brief Function to set thumbnail track for processing
     *
     * @param thumbnailIndex thumbnail index value indicating the track to select
     * @return bool true on success.
     */
    bool SetThumbnailTrack(int) override;
    std::vector<ThumbnailData> GetThumbnailRangeData(double,double, std::string*, int*, int*, int*, int*) override;
protected:
    /**
     *   @brief Get stream information of a profile from subclass.
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
 


