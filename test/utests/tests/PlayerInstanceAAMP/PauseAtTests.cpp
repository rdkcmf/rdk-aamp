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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "MockAampConfig.h"
#include "MockAampScheduler.h"
#include "MockPrivateInstanceAAMP.h"
#include "main_aamp.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::AtLeast;

class PauseAtTests : public ::testing::Test
{
protected:
    PlayerInstanceAAMP *mPlayerInstance = nullptr;

    void SetUp() override 
    {
        mPlayerInstance = new PlayerInstanceAAMP();

        g_mockAampConfig = new MockAampConfig();
        g_mockAampScheduler = new MockAampScheduler();
        g_mockPrivateInstanceAAMP = new MockPrivateInstanceAAMP();
    }
    
    void TearDown() override 
    {
        delete g_mockPrivateInstanceAAMP;
        g_mockPrivateInstanceAAMP = nullptr;

        delete g_mockAampScheduler;
        g_mockAampScheduler = nullptr;

        delete g_mockAampConfig;
        g_mockAampConfig = nullptr;

        delete mPlayerInstance;
        mPlayerInstance =nullptr;
    }
};


// Testing calling PauseAt with position
// Expect to call stop pause position monitoring, followed by 
// start pause position monitoring with the requested position
TEST_F(PauseAtTests, PauseAt)
{
    double pauseAtSeconds = 100.0;
    long long pauseAtMilliseconds = pauseAtSeconds * 1000;

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_)).WillRepeatedly(SetArgReferee<0>(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring()).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(pauseAtMilliseconds)).Times(1);

    mPlayerInstance->PauseAt(pauseAtSeconds);
}

// Testing calling PauseAt with position 0
// Expect to call stop pause position monitoring, followed by 
// start pause position monitoring with the requested position
TEST_F(PauseAtTests, PauseAt_Position0)
{
    double pauseAtSeconds = 0;
    long long pauseAtMilliseconds = pauseAtSeconds * 1000;

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_)).WillRepeatedly(SetArgReferee<0>(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring()).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(pauseAtMilliseconds)).Times(1);

    mPlayerInstance->PauseAt(pauseAtSeconds);
}

// Testing calling PauseAt with negative value to cancel
// Expect to call stop pause position monitoring
TEST_F(PauseAtTests, PauseAt_Cancel)
{
    double pauseAtSeconds = -1.0;

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_)).WillRepeatedly(SetArgReferee<0>(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring()).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(_)).Times(0);

    mPlayerInstance->PauseAt(pauseAtSeconds);
}

// Testing calling PauseAt when already paused
// Don't expect to start pause position monitoring
TEST_F(PauseAtTests, PauseAt_AlreadyPaused)
{
    double pauseAtSeconds = 100.0;

    mPlayerInstance->aamp->pipeline_paused = true;

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_)).WillRepeatedly(SetArgReferee<0>(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring()).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(_)).Times(0);

    mPlayerInstance->PauseAt(pauseAtSeconds);
}

// Testing calling PauseAt whilst in error state
// Expect to neither call stop nor start pause position monitoring
TEST_F(PauseAtTests, PauseAt_InErrorState)
{
    double pauseAtSeconds = -1.0;

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_)).WillRepeatedly(SetArgReferee<0>(eSTATE_ERROR));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring()).Times(0);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(_)).Times(0);

    mPlayerInstance->PauseAt(pauseAtSeconds);
}

// Testing calling PauseAt when configured to run async API
// Expect the async scheduler to be called
TEST_F(PauseAtTests, PauseAtAsync)
{
    double pauseAtSeconds = 100.0;

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_)).WillRepeatedly(SetArgReferee<0>(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AsyncTune)).WillRepeatedly(Return(true));
    mPlayerInstance->AsyncStartStop();

    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_)).WillOnce(Return(1));

    mPlayerInstance->PauseAt(pauseAtSeconds);
}

// Testing calling Tune cancels any pause position monitoring
// Expect StopPausePositionMonitoring to be called at least once 
// (internally Tune can call Stop ao possible for multiple calls)
TEST_F(PauseAtTests, PauseAt_Tune)
{
    char mainManifestUrl[] = "";

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_)).WillRepeatedly(SetArgReferee<0>(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring()).Times(AtLeast(1));
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(_)).Times(0);

    mPlayerInstance->Tune(mainManifestUrl);
}

// Testing calling detach cancels any pause position monitoring
// Expect StopPausePositionMonitoring to be called at once 
TEST_F(PauseAtTests, PauseAt_detach)
{
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_)).WillRepeatedly(SetArgReferee<0>(eSTATE_PLAYING));
    
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring()).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(_)).Times(0);

    mPlayerInstance->detach();
}

// Testing calling SetRate cancels any pause position monitoring
// Expect StopPausePositionMonitoring to be called at once 
TEST_F(PauseAtTests, PauseAt_SetRate)
{
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_)).WillRepeatedly(SetArgReferee<0>(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring()).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(_)).Times(0);

    mPlayerInstance->SetRate(1);
}

// Testing calling Stop cancels any pause position monitoring
// Expect StopPausePositionMonitoring to be called at once 
TEST_F(PauseAtTests, PauseAt_Stop)
{
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_)).WillRepeatedly(SetArgReferee<0>(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring()).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(_)).Times(0);

    mPlayerInstance->Stop();
}

// Testing calling Seek cancels any pause position monitoring
// Expect StopPausePositionMonitoring to be called at once 
TEST_F(PauseAtTests, PauseAt_Seek)
{
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState(_)).WillRepeatedly(SetArgReferee<0>(eSTATE_PLAYING));

    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StopPausePositionMonitoring()).Times(1);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, StartPausePositionMonitoring(_)).Times(0);

    mPlayerInstance->Seek(1000);
}
