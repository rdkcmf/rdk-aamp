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
 * @file aampgstplayer.h
 * @brief Gstreamer based player for AAMP
 */

#ifndef AAMPGSTPLAYER_H
#define AAMPGSTPLAYER_H

#include <stddef.h>
#include <functional>
#include <gst/gst.h>
#include "priv_aamp.h"
#include <pthread.h>

/**
 * @struct AAMPGstPlayerPriv
 * @brief forward declaration of AAMPGstPlayerPriv
 */
struct AAMPGstPlayerPriv;

/**
 * @struct TaskControlData
 * @brief data for scheduling and handling asynchronous tasks
 */
struct TaskControlData
{
	guint       taskID;
	bool        taskIsPending;
	std::string taskName;
	TaskControlData(const char* taskIdent) : taskID(0), taskIsPending(false), taskName(taskIdent ? taskIdent : "undefined") {};
};

/**
 * @brief Function pointer for the idle task
 * @param[in] arg - Arguments
 * @return Idle task status
 */
typedef int(*BackgroundTask)(void* arg);


/**
 * @class AAMPGstPlayer
 * @brief Class declaration of Gstreamer based player
 */
class AAMPGstPlayer : public StreamSink
{
private:
	bool SendHelper(MediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double duration, bool copy, bool initFragment = 0);
	void SendGstEvents(MediaType mediaType, GstClockTime pts);
	void SendNewSegmentEvent(MediaType mediaType, GstClockTime startPts ,GstClockTime stopPts = 0);

public:
	class PrivateInstanceAAMP *aamp;
	void Configure(StreamOutputFormat format, StreamOutputFormat audioFormat, StreamOutputFormat auxFormat, bool bESChangeStatus, bool forwardAudioToAux, bool setReadyAfterPipelineCreation);
	void SendCopy(MediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double duration);
	void SendTransfer(MediaType mediaType, GrowableBuffer* buffer, double fpts, double fdts, double duration, bool initFragment);
	void EndOfStreamReached(MediaType type);
	void Stream(void);
	void Stop(bool keepLastFrame);
	void DumpStatus(void);
	void Flush(double position, int rate, bool shouldTearDown);
	bool Pause(bool pause, bool forceStopGstreamerPreBuffering);
	long GetPositionMilliseconds(void);
        long GetDurationMilliseconds(void);
	unsigned long getCCDecoderHandle(void);
	virtual long long GetVideoPTS(void);
	void SetVideoRectangle(int x, int y, int w, int h);
	bool Discontinuity( MediaType mediaType);
	void SetVideoZoom(VideoZoomMode zoom);
	void SetVideoMute(bool muted);
	void SetAudioVolume(int volume);
	void setVolumeOrMuteUnMute(void);
	bool IsCacheEmpty(MediaType mediaType);
	void ResetEOSSignalledFlag();
	bool CheckForPTSChangeWithTimeout(long timeout);
	void NotifyFragmentCachingComplete();
	void NotifyFragmentCachingOngoing();
	void GetVideoSize(int &w, int &h);
	void QueueProtectionEvent(const char *protSystemId, const void *ptr, size_t len, MediaType type);
	void ClearProtectionEvent();
	bool IdleTaskAdd(TaskControlData& taskDetails, BackgroundTask funcPtr);
	bool IdleTaskRemove(TaskControlData& taskDetails);
	void IdleTaskClearFlags(TaskControlData& taskDetails);
	void TimerAdd(GSourceFunc funcPtr, int repeatTimeout, guint& taskId, gpointer user_data, const char* timerName = nullptr);
	void TimerRemove(guint& taskId, const char* timerName = nullptr);
	bool TimerIsRunning(guint& taskId);
	void StopBuffering(bool forceStop);
	bool AdjustPlayBackRate(double position, double rate);
	
	bool PipelineSetToReady; //To indicate the pipeline is set to ready forcefully
	bool trickTeardown;
	struct AAMPGstPlayerPriv *privateContext;
	AAMPGstPlayer(AampLogManager *logObj, PrivateInstanceAAMP *aamp
#ifdef RENDER_FRAMES_IN_APP_CONTEXT
	, std::function< void(uint8_t *, int, int, int) > exportFrames = nullptr
#endif
	);
	AAMPGstPlayer(const AAMPGstPlayer&) = delete;
	AAMPGstPlayer& operator=(const AAMPGstPlayer&) = delete;
	~AAMPGstPlayer();
	static void InitializeAAMPGstreamerPlugins(AampLogManager *logObj=NULL);
	void NotifyEOS();
	void NotifyFirstFrame(MediaType type);
	void DumpDiagnostics();
	void SignalTrickModeDiscontinuity();
#ifdef RENDER_FRAMES_IN_APP_CONTEXT
	std::function< void(uint8_t *, int, int, int) > cbExportYUVFrame;
	static GstFlowReturn AAMPGstPlayer_OnVideoSample(GstElement* object, AAMPGstPlayer * _this);
#endif
	void SeekStreamSink(double position, double rate);
	std::string GetVideoRectangle();
private:
	void PauseAndFlush(bool playAfterFlush);
	void TearDownStream(MediaType mediaType);
	bool CreatePipeline();
	void DestroyPipeline();
	static bool initialized;
	void Flush(void);
	void FlushLastId3Data();

	bool WaitForSourceSetup(MediaType mediaType);
	void ForwardBuffersToAuxPipeline(GstBuffer *buffer);
	bool ForwardAudioBuffersToAux();
	pthread_mutex_t mBufferingLock;
	pthread_mutex_t mProtectionLock;
	AampLogManager *mLogObj;
};

#endif // AAMPGSTPLAYER_H
