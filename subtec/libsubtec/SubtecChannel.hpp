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

#pragma once

#include <memory>
#include <vector>
#include <mutex>
#include "SubtecAttribute.hpp"

class SubtecChannelManager
{
public:
    static SubtecChannelManager *getInstance()
    {
        static SubtecChannelManager instance;
        return &instance;
    }
    int getNextChannelId() { return m_nextChannelId++; }
protected:
    SubtecChannelManager() :  m_nextChannelId(0) {}
private:
    uint32_t m_nextChannelId;
};

class SubtecChannel
{

protected:
    template<typename PacketType, typename ...Args>
    void sendPacket(Args && ...args);
public:
    enum class ChannelType
    {
        TTML,
        WEBVTT,
        CC
    };

    SubtecChannel() : m_counter(0), m_channelId(0), mChannelMtx()
    {
        m_channelId = SubtecChannelManager::getInstance()->getNextChannelId();
    }

    static std::unique_ptr<SubtecChannel> SubtecChannelFactory(ChannelType type);

    static bool InitComms();
    static bool InitComms(const char* socket_path);
    void SendResetAllPacket();
    void SendResetChannelPacket();
    void SendPausePacket();
    void SendResumePacket();
    void SendMutePacket();
    void SendUnmutePacket();
    void SendCCSetAttributePacket(std::uint32_t ccType, std::uint32_t attribType, const attributesType &attributesValues);

    virtual void SendSelectionPacket(uint32_t width, uint32_t height){};
    virtual void SendDataPacket(std::vector<uint8_t> &&data, std::int64_t time_offset_ms = 0){};
    virtual void SendTimestampPacket(uint64_t timestampMs){};

    virtual ~SubtecChannel() = 0;
    
protected:
    uint32_t m_channelId;
    uint32_t m_counter;
    std::mutex mChannelMtx;
};
