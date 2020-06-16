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

#include <chrono>

#include "SubtecPacket.hpp"
#include "PacketSender.hpp"

PacketSender *PacketSender::mInstance = nullptr;

void runWorkerTask(void *ctx)
{
    try {
        PacketSender *pkt = reinterpret_cast<PacketSender*>(ctx);
        pkt->senderTask();
    }
    catch (const std::exception& e) {
        logprintf("PacketSender: Error in run %s\n", e.what());
    }
}

PacketSender *PacketSender::Instance()
{
    if (!mInstance)
    {
        mInstance = new PacketSender;
    }
    
    return mInstance;
}

PacketSender::~PacketSender()
{
    PacketSender::Close();
}

void PacketSender::Close()
{
    closeSenderTask();
    if (mSubtecSocketHandle)
        ::close(mSubtecSocketHandle);
    mSubtecSocketHandle = 0;
}

void PacketSender::Flush()
{
    flushPacketQueue();
}

bool PacketSender::Init()
{
    return Init(SOCKET_PATH);
}

bool PacketSender::Init(const char *socket_path)
{
    bool ret = true;
    std::unique_lock<std::mutex> lock(mStartMutex);
    
    if (!running)
    {
        ret = initSocket(socket_path) && initSenderTask();
        if (mStartCv.wait_for(lock, std::chrono::milliseconds(100)) == std::cv_status::timeout)
        {
            logprintf("SenderTask timed out waiting to start\n");
            ret = false;
        }
        else
            logprintf("senderTask started\n");        
    }
    else
        logprintf("PacketSender::Init already running\n");
        
    return ret;
}

void PacketSender::AddPacket(PacketPtr packet)
{
    std::unique_lock<std::mutex> lock(mPktMutex);
    uint32_t type = packet->getType();
    std::string typeString = Packet::getTypeString(type);
    mPacketQueue.push(std::move(packet));
    logprintf("PacketSender: %s - queue size %lu type %s:%d\n", __FUNCTION__, mPacketQueue.size(), typeString.c_str(), type);
}

void PacketSender::senderTask()
{
    std::unique_lock<std::mutex> lock(mPktMutex);
    do {
        logprintf("PacketSender: Locking sender\n");
        mStartCv.notify_all();
        running = true;
        mCv.wait(lock);
        while (!mPacketQueue.empty())
        {
            sendPacket(std::move(mPacketQueue.front()));
            mPacketQueue.pop();
            logprintf("PacketSender: %s - queue size %lu\n", __FUNCTION__, mPacketQueue.size());
        }
    } while(running);
}
    
void PacketSender::SendPackets()
{
    std::unique_lock<std::mutex> lock(mPktMutex);
    mCv.notify_all();
}

bool PacketSender::IsRunning()
{
    std::unique_lock<std::mutex> lock(mPktMutex);
    return running.load();
}

void PacketSender::flushPacketQueue()
{
    std::queue<PacketPtr> empty;
    std::unique_lock<std::mutex> lock(mPktMutex);

    empty.swap(mPacketQueue);
}

void PacketSender::sendPacket(PacketPtr pkt)
{
    auto buffer = pkt->getBytes();
    size_t size =  static_cast<ssize_t>(buffer.size());
    auto written = ::write(mSubtecSocketHandle, &buffer[0], size);
    logprintf("PacketSender: Written %ld bytes with size %ld\n", written, size);
}

bool PacketSender::initSenderTask()
{
    try {
        mSendThread = std::thread(runWorkerTask, this);
    }
    catch (const std::exception& e) {
        logprintf("PacketSender: Error in initSenderTask: %s\n", e.what());
        return false;
    }
    
    return true;
}

void PacketSender::closeSenderTask()
{
    if (running)
    {
        running = false;
        mCv.notify_all();
        if (mSendThread.joinable())
        {
            mSendThread.join();        
        }
    }

}

bool PacketSender::initSocket(const char *socket_path)
{
    mSubtecSocketHandle = ::socket(AF_UNIX, SOCK_DGRAM, 0);
    if (mSubtecSocketHandle == -1)
    {
        logprintf("PacketSender: %s: Unable to init socket\n", __FUNCTION__);
        return false;
    }
    
    struct sockaddr_un addr;
    
    (void) std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    (void) std::strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path));
    addr.sun_path[sizeof(addr.sun_path) - 1] = 0;

    if (::connect(mSubtecSocketHandle, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0)
    {
        ::close(mSubtecSocketHandle);
        logprintf("PacketSender: %s - cannot connect to address \'%s\'\n", __func__, socket_path);
        return false;
    }
    printf("PacketSender: Initialised with socket_path %s\n", socket_path);

    return true;
}