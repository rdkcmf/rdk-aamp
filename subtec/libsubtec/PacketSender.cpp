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

#define MAX_SNDBUF_SIZE (8*1024*1024)

void runWorkerTask(void *ctx)
{
    try {
        PacketSender *pkt = reinterpret_cast<PacketSender*>(ctx);
        pkt->senderTask();
    }
    catch (const std::exception& e) {
        AAMPLOG_WARN("PacketSender: Error in run %s", e.what());
    }
}

PacketSender *PacketSender::Instance()
{
    static PacketSender instance;
    return &instance;
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

#ifdef AAMP_SIMULATOR_BUILD
// in simulator build, create a socket to receive and dump messages that would
// otherwise go to subtec
#include <pthread.h>
static struct SubtecSimulatorState
{
	bool started;
	pthread_t threadId;
	int sockfd;
} mSubtecSimulatorState;

static bool read32(const unsigned char *ptr, size_t len, std::uint32_t &ret32)
{
    bool ret = false;
    //Load packet header
    if (len >= sizeof(std::uint32_t))
    {
        const std::uint32_t byte0 = static_cast<const uint32_t>(ptr[0]) & 0xFF;
        const std::uint32_t byte1 = static_cast<const uint32_t>(ptr[1]) & 0xFF;
        const std::uint32_t byte2 = static_cast<const uint32_t>(ptr[2]) & 0xFF;
        const std::uint32_t byte3 = static_cast<const uint32_t>(ptr[3]) & 0xFF;
        ret32 =  byte0 | (byte1 << 8) | (byte2 << 16) | (byte3 << 24);
        ret = true;
    }
    
    return ret;
}

static void DumpPacket(const unsigned char *ptr, size_t len)
{
    //Get type
    std::uint32_t type;
    if (read32(ptr, len, type))
    {
        AAMPLOG_INFO("Type:%s:%d", Packet::getTypeString(type).c_str(), type);
        ptr += 4;
        len -= 4;
    }
    else
    {
        AAMPLOG_ERR("Packet read failed on type - returning");
        return;
    }
    //Get Packet counter
    std::uint32_t counter;
    if (read32(ptr, len, counter))
    {
        AAMPLOG_INFO("Counter:%d", counter);
        ptr += 4;
        len -= 4;
    }
    else
    {
        AAMPLOG_ERR("Packet read failed on type - returning");
        return;
    }
    //Get size
    std::uint32_t size;
    if (read32(ptr, len, size))
    {
        AAMPLOG_INFO("Packet size:%d", size);
        ptr += 4;
        len -= 4;
    }
    else
    {
        AAMPLOG_ERR("Packet read failed on type - returning");
        return;
    }
    if (len > 0)
    {
        AAMPLOG_WARN("Packet data:");
        DumpBlob(ptr, len);
    }
}

static void *SubtecSimulatorThread( void *param )
{
	struct SubtecSimulatorState *state = (SubtecSimulatorState *)param;
	struct sockaddr cliaddr;
	socklen_t sockLen = sizeof(cliaddr);
	size_t maxBuf = 8*1024; // big enough?
	unsigned char *buffer = (unsigned char *)malloc(maxBuf);
	if( buffer )
	{
		AAMPLOG_WARN( "SubtecSimulatorThread - listening for packets" );
		for(;;)
		{
			int numBytes = recvfrom( state->sockfd, (void *)buffer, maxBuf, MSG_WAITALL, (struct sockaddr *) &cliaddr, &sockLen);
			AAMPLOG_INFO( "***SubtecSimulatorThread:\n" );
            DumpPacket( buffer, numBytes );
		}
		free( buffer );
	}
	close( state->sockfd );
	return 0;
}

static bool PrepareSubtecSimulator( const char *name )
{
	struct SubtecSimulatorState *state = &mSubtecSimulatorState;
	if( !state->started )
	{ // already started - ok
		unlink( name ); // close if left over from previous session to avoid bind failure
		state->sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
		if( state->sockfd>=0 )
		{
			struct sockaddr_un serverAddr;
			memset(&serverAddr, 0, sizeof(serverAddr));
			serverAddr.sun_family = AF_UNIX;
			strcpy(serverAddr.sun_path, name );
			socklen_t len = sizeof(serverAddr);
			if( bind( state->sockfd, (struct sockaddr*)&serverAddr, len ) == 0 )
			{
				state->started = true;
				pthread_create(&state->threadId, NULL, &SubtecSimulatorThread, (void *)state);
			}
			else
			{
				AAMPLOG_ERR( "SubtecSimulatorThread bind() error: %d", errno );
			}
		}
	}
	return state->started;
}
#endif // AAMP_SIMULATOR_BUILD

bool PacketSender::Init(const char *socket_path)
{
    bool ret = true;
    std::unique_lock<std::mutex> lock(mStartMutex);

    AAMPLOG_INFO("PacketSender::Init with %s", socket_path);


#ifdef AAMP_SIMULATOR_BUILD
	ret = PrepareSubtecSimulator(socket_path);
#endif

    if (!running)
    {
        ret = initSocket(socket_path) && initSenderTask();
        if (!ret) {
            AAMPLOG_WARN("SenderTask failed to init");
        }
        else
            AAMPLOG_WARN("senderTask started");
    }
    else
        AAMPLOG_WARN("PacketSender::Init already running");
        
    return ret;
}

void PacketSender::SendPacket(PacketPtr && packet)
{
    std::unique_lock<std::mutex> lock(mPktMutex);
    uint32_t type = packet->getType();
    std::string typeString = Packet::getTypeString(type);
    AAMPLOG_TRACE("PacketSender:  queue size %lu type %s:%d counter:%d",
        mPacketQueue.size(), typeString.c_str(), type, packet->getCounter());

    mPacketQueue.push(std::move(packet));
    mCv.notify_all();
}

void PacketSender::senderTask()
{
    std::unique_lock<std::mutex> lock(mPktMutex);
    do {
        running = true;
        mCv.wait(lock);
        while (!mPacketQueue.empty())
        {
            sendPacket(std::move(mPacketQueue.front()));
            mPacketQueue.pop();
            AAMPLOG_TRACE("PacketSender:  queue size %lu", mPacketQueue.size());
        }
    } while(running);
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

void PacketSender::sendPacket(PacketPtr && pkt)
{
    auto buffer = pkt->getBytes();
    size_t size =  static_cast<ssize_t>(buffer.size());
    if (size > mSockBufSize && size < MAX_SNDBUF_SIZE)
    {
	int newSize = (int)buffer.size();
	if (::setsockopt(mSubtecSocketHandle, SOL_SOCKET, SO_SNDBUF, &newSize, sizeof(newSize)) == -1)
	{
            AAMPLOG_WARN("::setsockopt() SO_SNDBUF failed\n");
	}
	else
	{
	    mSockBufSize = newSize;
	    AAMPLOG_INFO("new socket buffer size %d\n", mSockBufSize);
	}
    }
    auto written = ::write(mSubtecSocketHandle, &buffer[0], size);
    AAMPLOG_TRACE("PacketSender: Written %ld bytes with size %ld", written, size);
}

bool PacketSender::initSenderTask()
{
    try {
        mSendThread = std::thread(runWorkerTask, this);
    }
    catch (const std::exception& e) {
        AAMPLOG_WARN("PacketSender: Error in initSenderTask: %s", e.what());
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
        AAMPLOG_WARN("PacketSender: Unable to init socket");
        return false;
    }
    
    struct sockaddr_un addr;
    
    (void) std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    (void) std::strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path));
    addr.sun_path[sizeof(addr.sun_path) - 1] = 0;

    socklen_t optlen = sizeof(mSockBufSize);
    ::getsockopt(mSubtecSocketHandle, SOL_SOCKET, SO_SNDBUF, &mSockBufSize, &optlen);
    mSockBufSize = mSockBufSize / 2;  //kernel returns twice the value of actual buffer
    AAMPLOG_INFO("SockBuffer size : %d\n", mSockBufSize);

    if (::connect(mSubtecSocketHandle, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0)
    {
        ::close(mSubtecSocketHandle);
        AAMPLOG_WARN("PacketSender: cannot connect to address \'%s\'", socket_path);
        return false;
    }
    AAMPLOG_INFO("PacketSender: Initialised with socket_path %s", socket_path);

    return true;
}
