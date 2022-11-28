/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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

#ifndef AAMP_MOCK_AAMP_PRIV_AAMP_H
#define AAMP_MOCK_AAMP_PRIV_AAMP_H

#include <gmock/gmock.h>
#include "priv_aamp.h"

class MockPrivateInstanceAAMP
{
public:

    MOCK_METHOD(void, StartPausePositionMonitoring, (long long pausePositionMilliseconds));

    MOCK_METHOD(void, StopPausePositionMonitoring, (std::string reason));

    MOCK_METHOD(void, GetState, (PrivAAMPState& state));

    MOCK_METHOD(void, SetState, (PrivAAMPState state));
};

extern MockPrivateInstanceAAMP *g_mockPrivateInstanceAAMP;

#endif /* AAMP_MOCK_AAMP_PRIV_AAMP_H */
