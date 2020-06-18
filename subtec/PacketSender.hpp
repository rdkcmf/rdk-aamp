/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

#pragma once

#include <memory>
#include <queue>
#include <thread>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <cstring>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "SubtecPacket.hpp"

#define PACKET_DEBUG 1

#ifdef PACKET_DEBUG
#define logprintf printf
#else
#define logprintf(FORMAT, ...)
#endif

const constexpr char *SOCKET_PATH = "/run/subttx/pes_data_main";

void runWorkerTask(void *ctx);

class PacketSender
{
public:    
    ~PacketSender();
    
    void Close();
    void Flush();
    bool Init();
    bool Init(const char *socket_path);
    void AddPacket(PacketPtr packet);
    void senderTask();
    void SendPackets();
    bool IsRunning();
    static PacketSender *Instance();
private:
    void closeSenderTask();
    void flushPacketQueue();
    void sendPacket(PacketPtr pkt);
    bool initSenderTask();
    bool initSocket(const char *socket_path);

    std::thread mSendThread;
    int mSubtecSocketHandle;
    std::atomic_bool running;
    std::queue<PacketPtr> mPacketQueue;
    std::mutex mPktMutex;
    std::condition_variable mCv;
    std::mutex mStartMutex;
    std::condition_variable mStartCv;
    static PacketSender *mInstance;
protected:
    PacketSender() : 
        mSendThread(), 
        mSubtecSocketHandle(-1), 
        running(false), 
        mPacketQueue(), 
        mPktMutex(), 
        mCv(),
        mStartMutex(),
        mStartCv() 
        {}
};