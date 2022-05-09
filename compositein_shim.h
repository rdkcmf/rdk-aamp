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
 * @file compositein_shim.h
 * @brief shim for dispatching UVE Composite input playback
 */

#ifndef COMPOSITEIN_SHIM_H_
#define COMPOSITEIN_SHIM_H_

#include "videoin_shim.h"
#include <string>
#include <stdint.h>
#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
#include <core/core.h>
#include "ThunderAccess.h"
#endif
using namespace std;

/**
 * @class StreamAbstractionAAMP_COMPOSITEIN
 * @brief Fragment collector for MPEG DASH
 */
class StreamAbstractionAAMP_COMPOSITEIN : public StreamAbstractionAAMP_VIDEOIN
{
public:
    /**
     * @brief StreamAbstractionAAMP_COMPOSITEIN Constructor
     * @param aamp pointer to PrivateInstanceAAMP object associated with player
     * @param seekpos Seek position
     * @param rate playback rate
     */
    StreamAbstractionAAMP_COMPOSITEIN(AampLogManager *logObj, class PrivateInstanceAAMP *aamp,double seekpos, float rate);
    /**
     *   @brief StreamAbstractionAAMP_COMPOSITEIN Destructor
     */
    ~StreamAbstractionAAMP_COMPOSITEIN();
    /**
     * @brief Copy constructor disabled
     *
     */
    StreamAbstractionAAMP_COMPOSITEIN(const StreamAbstractionAAMP_COMPOSITEIN&) = delete;
    /**
     * @brief assignment operator disabled
     *
     */
    StreamAbstractionAAMP_COMPOSITEIN& operator=(const StreamAbstractionAAMP_COMPOSITEIN&) = delete;
    /**
     *   @brief  Initialize a newly created object.
     *   @param  tuneType to set type of object.
     *   @retval eAAMPSTATUS_OK
     */
    AAMPStatusType Init(TuneType tuneType) override;
    /**
     *   @brief  Starts streaming.
     */
    void Start() override;
    /**
     *   @brief  Stops streaming.
     */
    void Stop(bool clearChannelData) override;
    /**
     * @brief To get the available video tracks.
     * @return available video tracks.
     */
    std::vector<StreamInfo*> GetAvailableVideoTracks(void) override;
    /**
     * @brief To get the available thumbnail tracks.
     * @return available thunbnail tracks.
     */
    std::vector<StreamInfo*> GetAvailableThumbnailTracks(void) override;
    /**
     * @brief To set the available thumbnail tracks.
     * @return True/False to set.
     */
    bool SetThumbnailTrack(int) override;
    /***************************************************************************
     * @brief Function to fetch the thumbnail data.
     *
     * @param tStart start duration of thumbnail data.
     * @param tEnd end duration of thumbnail data.
     * @param baseurl base url of thumbnail images.
     * @param raw_w absolute width of the thumbnail spritesheet.
     * @param raw_h absolute height of the thumbnail spritesheet.
     * @param width width of each thumbnail tile.
     * @param height height of each thumbnail tile.
     * @return Updated vector of available thumbnail data.
     ***************************************************************************/
    std::vector<ThumbnailData> GetThumbnailRangeData(double tStart, double tEnd, std::string *baseurl, int *raw_w, int *raw_h, int *width, int *height) override;
};

#endif // COMPOSITEIN_SHIM_H_
/**
 * @}
 */

