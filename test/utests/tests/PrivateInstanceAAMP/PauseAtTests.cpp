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
#include <chrono>

#include "priv_aamp.h"

#include "AampConfig.h"
#include "AampScheduler.h"
#include "AampLogManager.h"
#include "MockAampConfig.h"
#include "MockAampGstPlayer.h"
#include "MockAampScheduler.h"
#include "MockAampEventManager.h"
#include "MockStreamAbstractionAAMP.h"

using ::testing::_;
using ::testing::WithParamInterface;
using ::testing::An;
using ::testing::DoAll;
using ::testing::SetArgReferee;
using ::testing::Invoke;
using ::testing::Return;

#define WAIT_FOR_SCHEDUE_TASK_POLL_PERIOD_MS    (50)

AampConfig *gpGlobalConfig=NULL;
AampLogManager *mLogObj=NULL;

class PauseAtTests : public ::testing::Test
{
protected:
    PrivateInstanceAAMP *mPrivateInstanceAAMP;

    AampScheduler mScheduler;

    AsyncTask mScheduleAsyncTask;
    void * mScheduleAsyncData;
    int mScheduleAsyncId;
    std::string mScheduleAsyncTaskName;

    bool mScheduleTaskCalled;
    std::mutex mScheduleTaskCalledMutex;
    std::condition_variable mScheduleTaskCalledCV;

    void SetUp() override
    {
        mScheduleTaskCalled = false;

        if(gpGlobalConfig == nullptr)
        {
            gpGlobalConfig =  new AampConfig();
        }

        mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

        g_mockAampConfig = new MockAampConfig();

        g_mockAampScheduler = new MockAampScheduler();
        g_mockAampGstPlayer = new MockAAMPGstPlayer(mLogObj, mPrivateInstanceAAMP);
        g_mockAampEventManager = new MockAampEventManager();
        g_mockStreamAbstractionAAMP = new MockStreamAbstractionAAMP(mLogObj, mPrivateInstanceAAMP);

        mPrivateInstanceAAMP->SetScheduler(&mScheduler);
        mPrivateInstanceAAMP->mStreamSink = g_mockAampGstPlayer;
        mPrivateInstanceAAMP->mpStreamAbstractionAAMP = g_mockStreamAbstractionAAMP;

        // Called in destructor of PrivateInstanceAAMP
        // Done here because setting up the EXPECT_CALL in TearDown, conflicted with the mock
        // being called in the PausePosition thread.
        EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableCurlStore)).WillRepeatedly(Return(false));
    }

    void TearDown() override
    {
        delete mPrivateInstanceAAMP;
        mPrivateInstanceAAMP = nullptr;

        delete g_mockStreamAbstractionAAMP;
        g_mockStreamAbstractionAAMP = nullptr;

        delete g_mockAampEventManager;
        g_mockAampEventManager = nullptr;

        delete g_mockAampGstPlayer;
        g_mockAampGstPlayer = nullptr;

        delete g_mockAampScheduler;
        g_mockAampScheduler = nullptr;

        delete gpGlobalConfig;
        gpGlobalConfig = nullptr;

        delete g_mockAampConfig;
        g_mockAampConfig = nullptr;
    }

public:

    int ScheduleTask(AsyncTaskObj obj)
    {
        mScheduleAsyncTask = obj.mTask;
        mScheduleAsyncData = obj.mData;
        mScheduleAsyncId = obj.mId;
        mScheduleAsyncTaskName = obj.mTaskName;

        std::unique_lock<std::mutex> lock(mScheduleTaskCalledMutex);
        mScheduleTaskCalled = true;
        mScheduleTaskCalledCV.notify_one();

        return 1;
    }

    // Wait for either:
    //    - ScheduleTask to be called
    //    - mPausePositionMilliseconds to be set to -1 (i.e. cancelled)
    //    - timeout to avoid lockup of test
    // Returns true if ScheduleTask was called, otherwise false
    bool WaitForScheduleTask(int timeoutMs)
    {
        std::unique_lock<std::mutex> lock(mScheduleTaskCalledMutex);
        std::chrono::time_point<std::chrono::steady_clock> startTime = std::chrono::steady_clock::now();
        std::chrono::time_point<std::chrono::steady_clock> currentTime = startTime;

        while ((false == mScheduleTaskCalled) &&
               (mPrivateInstanceAAMP->mPausePositionMilliseconds != AAMP_PAUSE_POSITION_INVALID_POSITION) &&
               (std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() < timeoutMs))
        {
            mScheduleTaskCalledCV.wait_for(lock, std::chrono::milliseconds(WAIT_FOR_SCHEDUE_TASK_POLL_PERIOD_MS));
            currentTime = std::chrono::steady_clock::now();
        }

        return mScheduleTaskCalled;
    }
};

MATCHER_P(AnEventOfType, type, "") { return type == arg->getType(); }

// Testing calling StartPausePositionMonitoring when pipeline paused
// Don't expect ScheduleTask to be called to execute pause
TEST_F(PauseAtTests, StartPausePositionMonitoring_PipelinePaused)
{
    long long pauseAtMilliseconds = 100.0 * 1000;

    mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
    mPrivateInstanceAAMP->pipeline_paused = true;
    mPrivateInstanceAAMP->trickStartUTCMS = 0;
    mPrivateInstanceAAMP->mAudioOnlyPb = false;
    mPrivateInstanceAAMP->durationSeconds = 3600;

    EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_VODTrickPlayFPS, An<int &>())).Times(0);

    // Calls from PrivateInstanceAAMP::GetPositionMilliseconds
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableGstPositionQuery)).WillOnce(Return(true));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback)).WillOnce(Return(false));

    // Return a position that is already beyond pauseAtMilliseconds
    EXPECT_CALL(*g_mockAampGstPlayer, GetPositionMilliseconds()).WillRepeatedly(Return (pauseAtMilliseconds + 1));

    // Check that PrivateInstanceAAMP_PausePosition is not called
    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_)).Times(0);

    mPrivateInstanceAAMP->StartPausePositionMonitoring(pauseAtMilliseconds);

    // Wait for scheduler to check it is not called
    ASSERT_FALSE(WaitForScheduleTask(1000));
    EXPECT_EQ(mPrivateInstanceAAMP->mPausePositionMilliseconds, -1);

    mPrivateInstanceAAMP->StopPausePositionMonitoring();
}

// Testing calling StartPausePositionMonitoring when rate is pause
// (which the code checks for but really pause is done by setting pipeline_paused)
// Don't expect ScheduleTask to be called to execute pause
TEST_F(PauseAtTests, StartPausePositionMonitoring_RatePaused)
{
    long long pauseAtMilliseconds = 100.0 * 1000;

    mPrivateInstanceAAMP->rate = AAMP_RATE_PAUSE;
    mPrivateInstanceAAMP->pipeline_paused = true;
    mPrivateInstanceAAMP->trickStartUTCMS = 0;
    mPrivateInstanceAAMP->mAudioOnlyPb = false;
    mPrivateInstanceAAMP->durationSeconds = 3600;

    EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_VODTrickPlayFPS, An<int &>())).Times(0);

    // Calls from PrivateInstanceAAMP::GetPositionMilliseconds
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableGstPositionQuery)).WillOnce(Return(true));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback)).WillOnce(Return(false));

    // Return a position that is already beyond pauseAtMilliseconds
    EXPECT_CALL(*g_mockAampGstPlayer, GetPositionMilliseconds()).WillRepeatedly(Return (pauseAtMilliseconds + 1));

    // Check that PrivateInstanceAAMP_PausePosition is not called
    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_)).Times(0);

    mPrivateInstanceAAMP->StartPausePositionMonitoring(pauseAtMilliseconds);

    // Wait for scheduler to check it is not called
    ASSERT_FALSE(WaitForScheduleTask(1000));
    EXPECT_EQ(mPrivateInstanceAAMP->mPausePositionMilliseconds, -1);

    mPrivateInstanceAAMP->StopPausePositionMonitoring();
}

// Testing calling StartPausePositionMonitoring when already monitoring
// Expect the pause position to be updated with later position
TEST_F(PauseAtTests, StartPausePositionMonitoring_AlreadyStarted)
{
    long long pauseAtMilliseconds01 = 100.0 * 1000;
    long long pauseAtMilliseconds02 = 200.0 * 1000;

    mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
    mPrivateInstanceAAMP->pipeline_paused = false;
    mPrivateInstanceAAMP->trickStartUTCMS = 0;
    mPrivateInstanceAAMP->mAudioOnlyPb = false;
    mPrivateInstanceAAMP->durationSeconds = 3600;

    // Calls from PrivateInstanceAAMP::GetPositionMilliseconds
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableGstPositionQuery)).WillRepeatedly(Return(true));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback)).WillRepeatedly(Return(false));

    // Don't move position for this test
    EXPECT_CALL(*g_mockAampGstPlayer, GetPositionMilliseconds()).WillRepeatedly(Return (0));

    // Check that PrivateInstanceAAMP_PausePosition is not called
    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_)).Times(0);

    mPrivateInstanceAAMP->StartPausePositionMonitoring(pauseAtMilliseconds01);
    ASSERT_EQ(mPrivateInstanceAAMP->mPausePositionMilliseconds, pauseAtMilliseconds01);

    mPrivateInstanceAAMP->StartPausePositionMonitoring(pauseAtMilliseconds02);
    EXPECT_EQ(mPrivateInstanceAAMP->mPausePositionMilliseconds, pauseAtMilliseconds02);

    mPrivateInstanceAAMP->StopPausePositionMonitoring();
}

// Testing calling StartPausePositionMonitoring with an invalid position (i.e. negative)
// Don't expect ScheduleTask to be called to execute pause
// Expect the pause position to be set to AAMP_PAUSE_POSITION_INVALID_POSITION
TEST_F(PauseAtTests, StartPausePositionMonitoring_InvalidPosition)
{
    long long pauseAtMilliseconds = -100.0 * 1000;

    mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
    mPrivateInstanceAAMP->pipeline_paused = false;
    mPrivateInstanceAAMP->trickStartUTCMS = 0;
    mPrivateInstanceAAMP->mAudioOnlyPb = false;
    mPrivateInstanceAAMP->durationSeconds = 3600;

    // Calls from PrivateInstanceAAMP::GetPositionMilliseconds
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableGstPositionQuery)).Times(0);
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback)).Times(0);

    // Don't move position for this test
    EXPECT_CALL(*g_mockAampGstPlayer, GetPositionMilliseconds()).WillRepeatedly(Return (0));
    
    // Check that PrivateInstanceAAMP_PausePosition is not called
    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_)).Times(0);

    mPrivateInstanceAAMP->StartPausePositionMonitoring(pauseAtMilliseconds);
    ASSERT_EQ(mPrivateInstanceAAMP->mPausePositionMilliseconds, AAMP_PAUSE_POSITION_INVALID_POSITION);
}

// Testing calling StopPausePositionMonitoring whilst monitoring
// Don't expect ScheduleTask to be called to execute pause
// Expect the pause position to be set to AAMP_PAUSE_POSITION_INVALID_POSITION
TEST_F(PauseAtTests, StopPausePositionMonitoring_WhenMonitoring)
{
    long long pauseAtMilliseconds = 100.0 * 1000;

    mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
    mPrivateInstanceAAMP->pipeline_paused = false;
    mPrivateInstanceAAMP->trickStartUTCMS = 0;
    mPrivateInstanceAAMP->mAudioOnlyPb = false;
    mPrivateInstanceAAMP->durationSeconds = 3600;

    // Calls from PrivateInstanceAAMP::GetPositionMilliseconds
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableGstPositionQuery)).WillRepeatedly(Return(true));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback)).WillRepeatedly(Return(false));

    // Don't move position for this test
    EXPECT_CALL(*g_mockAampGstPlayer, GetPositionMilliseconds()).WillRepeatedly(Return (0));

    // Check that PrivateInstanceAAMP_PausePosition is not called
    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_)).Times(0);

    mPrivateInstanceAAMP->StartPausePositionMonitoring(pauseAtMilliseconds);
    ASSERT_EQ(mPrivateInstanceAAMP->mPausePositionMilliseconds, pauseAtMilliseconds);

    mPrivateInstanceAAMP->StopPausePositionMonitoring();
    EXPECT_EQ(mPrivateInstanceAAMP->mPausePositionMilliseconds, AAMP_PAUSE_POSITION_INVALID_POSITION);
}

// Testing calling StopPausePositionMonitoring whilst not monitoring
// Expect the pause position to be set to AAMP_PAUSE_POSITION_INVALID_POSITION
TEST_F(PauseAtTests, StopPausePositionMonitoring_WhenNotMonitoring)
{
    mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
    mPrivateInstanceAAMP->pipeline_paused = false;
    mPrivateInstanceAAMP->trickStartUTCMS = 0;
    mPrivateInstanceAAMP->mAudioOnlyPb = false;
    mPrivateInstanceAAMP->durationSeconds = 3600;

    ASSERT_EQ(mPrivateInstanceAAMP->mPausePositionMilliseconds, AAMP_PAUSE_POSITION_INVALID_POSITION);

    mPrivateInstanceAAMP->StopPausePositionMonitoring();
    EXPECT_EQ(mPrivateInstanceAAMP->mPausePositionMilliseconds, AAMP_PAUSE_POSITION_INVALID_POSITION);
}

// Testing call of PrivateInstanceAAMP_PausePosition when at playback speed
// Current position returned is beyond the requested pause position, to trigger 
// the call immediately to PrivateInstanceAAMP_PausePosition
// Expect:
//     gstreamer pause to be called,
//     notifications of AAMP_EVENT_STATE_CHANGED and AAMP_EVENT_SPEED_CHANGED
//     call to StreamAbstractionAAMP::NotifyPlaybackPaused
//     seek_pos_seconds should remain at initial value
//     trickStartUTCMS to be set to 0
TEST_F(PauseAtTests, PausePosition_Playback)
{
    long long pauseAtMilliseconds = 100.0 * 1000;
    int seek_pos_seconds = 123;

    mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
    mPrivateInstanceAAMP->pipeline_paused = false;
    mPrivateInstanceAAMP->seek_pos_seconds = seek_pos_seconds;
    mPrivateInstanceAAMP->trickStartUTCMS = 0;
    mPrivateInstanceAAMP->mAudioOnlyPb = false;
    mPrivateInstanceAAMP->durationSeconds = 3600;

    mPrivateInstanceAAMP->SetState(eSTATE_PLAYING);

    ASSERT_FALSE(mPrivateInstanceAAMP->mbDownloadsBlocked);

    // Calls from PrivateInstanceAAMP::GetPositionMilliseconds
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableGstPositionQuery)).WillRepeatedly(Return(true));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback)).WillRepeatedly(Return(false));

    // Already beyond position
    EXPECT_CALL(*g_mockAampGstPlayer, GetPositionMilliseconds())
        .WillRepeatedly(Return(pauseAtMilliseconds+ (mPrivateInstanceAAMP->rate * 1000)));

    // Check that PrivateInstanceAAMP_PausePosition is called
    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_))
        .WillOnce(Invoke(this, &PauseAtTests::ScheduleTask));

    mPrivateInstanceAAMP->StartPausePositionMonitoring(pauseAtMilliseconds);
    ASSERT_EQ(mPrivateInstanceAAMP->mPausePositionMilliseconds, pauseAtMilliseconds);

    // Wait for scheduler to be called, and assert it didn't timeout
    ASSERT_TRUE(WaitForScheduleTask(5000));
    ASSERT_EQ(mScheduleAsyncTaskName, "PrivateInstanceAAMP_PausePosition");
    ASSERT_EQ(mScheduleAsyncData, mPrivateInstanceAAMP);

    EXPECT_CALL(*g_mockAampGstPlayer, Pause(true, false)).WillOnce(Return(true));

    // Expected calls from PrivateInstanceAAMP::NotifySpeedChanged
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_NativeCCRendering)).WillRepeatedly(Return(false));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_RepairIframes)).WillRepeatedly(Return(false));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_UseSecManager)).WillRepeatedly(Return(false));
    EXPECT_CALL(*g_mockAampScheduler, SetState(eSTATE_PAUSED)).Times(1);

    // Expected calls from PrivateInstanceAAMP::SetState
    EXPECT_CALL(*g_mockAampEventManager, IsEventListenerAvailable(AAMP_EVENT_STATE_CHANGED)).WillOnce(Return(true));
    EXPECT_CALL(*g_mockAampEventManager, SendEvent(AnEventOfType(AAMP_EVENT_STATE_CHANGED),_)).Times(1);

    EXPECT_CALL(*g_mockAampEventManager, SendEvent(AnEventOfType(AAMP_EVENT_SPEED_CHANGED),_)).Times(1);

    EXPECT_CALL(*g_mockStreamAbstractionAAMP, NotifyPlaybackPaused(true)).Times(1);

    // Execute PrivateInstanceAAMP_PausePosition
    mScheduleAsyncTask(mScheduleAsyncData);

    EXPECT_TRUE(mPrivateInstanceAAMP->pipeline_paused);
    EXPECT_TRUE(mPrivateInstanceAAMP->mbDownloadsBlocked);
    EXPECT_EQ(mPrivateInstanceAAMP->seek_pos_seconds, seek_pos_seconds);
    EXPECT_EQ(mPrivateInstanceAAMP->trickStartUTCMS, 0);
}

// Testing call of PrivateInstanceAAMP_PausePosition when at trickmode speed
// Current position returned is beyond the requested pause position, to trigger 
// the call immediately to PrivateInstanceAAMP_PausePosition
// Expect:
//     gstreamer pause to be called,
//     notifications of AAMP_EVENT_STATE_CHANGED and AAMP_EVENT_SPEED_CHANGED
//     call to StreamAbstractionAAMP::NotifyPlaybackPaused
//     seek_pos_seconds to be set to the current position
//     trickStartUTCMS to be set to -1
TEST_F(PauseAtTests, PausePosition_Trickmode)
{
    long long pauseAtMilliseconds = 100.0 * 1000;
    // Current position is beyond the pause at position
    long long currentPosition = pauseAtMilliseconds + 2000;
    int trickplayFPS = 4;

    mPrivateInstanceAAMP->rate = 2;
    mPrivateInstanceAAMP->pipeline_paused = false;
    mPrivateInstanceAAMP->trickStartUTCMS = 0;
    mPrivateInstanceAAMP->mAudioOnlyPb = false;
    mPrivateInstanceAAMP->durationSeconds = 3600;

    mPrivateInstanceAAMP->SetState(eSTATE_PLAYING);

    ASSERT_FALSE(mPrivateInstanceAAMP->mbDownloadsBlocked);

    EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_VODTrickPlayFPS, An<int &>()))
        .WillRepeatedly(DoAll(SetArgReferee<1>(trickplayFPS), Return(true)));

    // Calls from PrivateInstanceAAMP::GetPositionMilliseconds
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableGstPositionQuery)).WillRepeatedly(Return(true));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback)).WillRepeatedly(Return(false));

    EXPECT_CALL(*g_mockAampGstPlayer, GetPositionMilliseconds())
        .WillRepeatedly(Return(currentPosition));

    // Check that PrivateInstanceAAMP_PausePosition is called
    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_))
        .WillOnce(Invoke(this, &PauseAtTests::ScheduleTask));

    mPrivateInstanceAAMP->StartPausePositionMonitoring(pauseAtMilliseconds);
    ASSERT_EQ(mPrivateInstanceAAMP->mPausePositionMilliseconds, pauseAtMilliseconds);

    // Wait for scheduler to be called, and assert it didn't timeout
    ASSERT_TRUE(WaitForScheduleTask(5000));
    ASSERT_EQ(mScheduleAsyncTaskName, "PrivateInstanceAAMP_PausePosition");
    ASSERT_EQ(mScheduleAsyncData, mPrivateInstanceAAMP);

    EXPECT_CALL(*g_mockAampGstPlayer, Pause(true, false)).WillOnce(Return(true));

    // Expected calls from PrivateInstanceAAMP::NotifySpeedChanged
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_NativeCCRendering)).WillRepeatedly(Return(false));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_RepairIframes)).WillRepeatedly(Return(false));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_UseSecManager)).WillRepeatedly(Return(false));
    EXPECT_CALL(*g_mockAampScheduler, SetState(eSTATE_PAUSED)).Times(1);

    // Expected calls from PrivateInstanceAAMP::SetState
    EXPECT_CALL(*g_mockAampEventManager, IsEventListenerAvailable(AAMP_EVENT_STATE_CHANGED)).WillOnce(Return(true));
    EXPECT_CALL(*g_mockAampEventManager, SendEvent(AnEventOfType(AAMP_EVENT_STATE_CHANGED),_)).Times(1);

    EXPECT_CALL(*g_mockAampEventManager, SendEvent(AnEventOfType(AAMP_EVENT_SPEED_CHANGED),_)).Times(1);

    EXPECT_CALL(*g_mockStreamAbstractionAAMP, NotifyPlaybackPaused(true)).Times(1);

    // Execute PrivateInstanceAAMP_PausePosition
    mScheduleAsyncTask(mScheduleAsyncData);

    EXPECT_TRUE(mPrivateInstanceAAMP->pipeline_paused);
    EXPECT_TRUE(mPrivateInstanceAAMP->mbDownloadsBlocked);
    EXPECT_EQ(mPrivateInstanceAAMP->seek_pos_seconds, pauseAtMilliseconds / 1000);
    EXPECT_EQ(mPrivateInstanceAAMP->trickStartUTCMS, -1);
}

// Parameter test class, for running same tests with different rates
class PlaybackSpeedTests : public PauseAtTests,
                           public testing::WithParamInterface<float>
{

};

// Testing calling StartPausePositionMonitoring with a valid position
TEST_P(PlaybackSpeedTests, StartPausePositionMonitoring)
{
    long long pauseAtMilliseconds = 100.0 * 1000;
    int trickplayFPS = 4;

    mPrivateInstanceAAMP->rate = GetParam();
    mPrivateInstanceAAMP->pipeline_paused = false;
    mPrivateInstanceAAMP->trickStartUTCMS = 0;
    mPrivateInstanceAAMP->mAudioOnlyPb = false;
    mPrivateInstanceAAMP->durationSeconds = 3600;

    EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_VODTrickPlayFPS, An<int &>()))
        .WillRepeatedly(DoAll(SetArgReferee<1>(trickplayFPS), Return(true)));

    // Calls from PrivateInstanceAAMP::GetPositionMilliseconds
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableGstPositionQuery)).WillRepeatedly(Return(true));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback)).WillRepeatedly(Return(false));

    if ((mPrivateInstanceAAMP->rate == AAMP_NORMAL_PLAY_RATE) ||
        (mPrivateInstanceAAMP->rate == AAMP_SLOWMOTION_RATE))
    {
        // Simulate position just beyond AAMP_PAUSE_POSITION_POLL_PERIOD_MS of position,
        // then within AAMP_PAUSE_POSITION_POLL_PERIOD_MS of position,
        // then at position
        EXPECT_CALL(*g_mockAampGstPlayer, GetPositionMilliseconds())
            .WillOnce(Return(pauseAtMilliseconds - AAMP_PAUSE_POSITION_POLL_PERIOD_MS - 100))
            .WillOnce(Return(pauseAtMilliseconds - AAMP_PAUSE_POSITION_POLL_PERIOD_MS + 100))
            .WillRepeatedly(Return(pauseAtMilliseconds));
    }
    else if (mPrivateInstanceAAMP->rate > 0)
    {
        // Simulate position more than 2 frames prior to position,
        // then more than 1 frames prior to position,,
        // then within one frame of position,
        EXPECT_CALL(*g_mockAampGstPlayer, GetPositionMilliseconds())
            .WillOnce(Return(pauseAtMilliseconds - (((mPrivateInstanceAAMP->rate * 1000) / trickplayFPS) * 2) - 50))
            .WillOnce(Return(pauseAtMilliseconds - (((mPrivateInstanceAAMP->rate * 1000) / trickplayFPS) * 1) - 50))
            .WillRepeatedly(Return(pauseAtMilliseconds - (((mPrivateInstanceAAMP->rate * 1000) / trickplayFPS) * 1) + 50));
    }
    else
    {
        // Simulate position more than 2 frames prior to position,
        // then more than 1 frames prior to position,,
        // then within one frame of position,
        EXPECT_CALL(*g_mockAampGstPlayer, GetPositionMilliseconds())
            .WillOnce(Return(pauseAtMilliseconds - (((mPrivateInstanceAAMP->rate * 1000) / trickplayFPS) * 2) + 50))
            .WillOnce(Return(pauseAtMilliseconds - (((mPrivateInstanceAAMP->rate * 1000) / trickplayFPS) * 1) + 50))
            .WillRepeatedly(Return(pauseAtMilliseconds - (((mPrivateInstanceAAMP->rate * 1000) / trickplayFPS) * 1) - 50));
    }

    // Check that PrivateInstanceAAMP_PausePosition is called
    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_))
        .WillOnce(Invoke(this, &PauseAtTests::ScheduleTask));

    mPrivateInstanceAAMP->StartPausePositionMonitoring(pauseAtMilliseconds);

    // Wait for scheduler to be called, and assert it didn't timeout
    ASSERT_TRUE(WaitForScheduleTask(5000));
    EXPECT_EQ(mScheduleAsyncTaskName, "PrivateInstanceAAMP_PausePosition");
}

// Testing calling StartPausePositionMonitoring with a position already in the past
TEST_P(PlaybackSpeedTests, StartPausePositionMonitoring_PositionAlreadyPassed)
{
    long long pauseAtMilliseconds = 100.0 * 1000;
    int trickplayFPS = 4;

    mPrivateInstanceAAMP->rate = GetParam();
    mPrivateInstanceAAMP->pipeline_paused = false;
    mPrivateInstanceAAMP->trickStartUTCMS = 0;
    mPrivateInstanceAAMP->mAudioOnlyPb = false;
    mPrivateInstanceAAMP->durationSeconds = 3600;

    EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_VODTrickPlayFPS, An<int &>()))
        .WillRepeatedly(DoAll(SetArgReferee<1>(trickplayFPS), Return(true)));

    // Calls from PrivateInstanceAAMP::GetPositionMilliseconds
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableGstPositionQuery)).WillRepeatedly(Return(true));
    EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback)).WillRepeatedly(Return(false));

    EXPECT_CALL(*g_mockAampGstPlayer, GetPositionMilliseconds()).WillRepeatedly(Return(pauseAtMilliseconds + 100));
    
    // Check that PrivateInstanceAAMP_PausePosition is called
    EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_))
        .WillOnce(Invoke(this, &PauseAtTests::ScheduleTask));

    mPrivateInstanceAAMP->StartPausePositionMonitoring(pauseAtMilliseconds);

    // Wait for scheduler to be called, and assert it didn't timeout
    ASSERT_TRUE(WaitForScheduleTask(5000));
    EXPECT_EQ(mScheduleAsyncTaskName, "PrivateInstanceAAMP_PausePosition");
}

// Run PlaybackSpeedTests tests at various speeds
INSTANTIATE_TEST_SUITE_P(TestPlaybackSpeeds,
                         PlaybackSpeedTests,
                         testing::Values(AAMP_NORMAL_PLAY_RATE,AAMP_SLOWMOTION_RATE, -30, -2, 2, 30));


