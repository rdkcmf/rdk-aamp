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
    StreamAbstractionAAMP_COMPOSITEIN(AampLogManager *logObj, class PrivateInstanceAAMP *aamp,double seekpos, float rate);
    ~StreamAbstractionAAMP_COMPOSITEIN();
    StreamAbstractionAAMP_COMPOSITEIN(const StreamAbstractionAAMP_COMPOSITEIN&) = delete;
    StreamAbstractionAAMP_COMPOSITEIN& operator=(const StreamAbstractionAAMP_COMPOSITEIN&) = delete;
    AAMPStatusType Init(TuneType tuneType) override;
    void Start() override;
    void Stop(bool clearChannelData) override;
    std::vector<StreamInfo*> GetAvailableThumbnailTracks(void) override;
    bool SetThumbnailTrack(int) override;
    std::vector<ThumbnailData> GetThumbnailRangeData(double,double, std::string*, int*, int*, int*, int*) override;
};

#endif // COMPOSITEIN_SHIM_H_
/**
 * @}
 */

