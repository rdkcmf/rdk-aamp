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

#include "SubtecPacket.hpp"
#include "PacketSender.hpp"

class SubtecChannelManager
{
public:
    static SubtecChannelManager *getInstance()
    {
        static SubtecChannelManager instance;
        return &instance;
    }
    int getNextChannelId() { return m_nextChannelId++; }
    PacketPtr generateResetAllPacket() { return make_unique<ResetAllPacket>(); }
protected:
    SubtecChannelManager() :  m_nextChannelId(0) {}
private:
    uint32_t m_nextChannelId;
};

class SubtecChannel
{

protected:
    template<typename PacketType, typename ...Args>
    void sendPacket(Args && ...args)
    {
        std::unique_lock<std::mutex> lock(mChannelMtx);
        PacketSender::Instance()->SendPacket(make_unique<PacketType>(m_channelId, m_counter++, std::forward<Args>(args)...));
    }
public:
    SubtecChannel() : m_counter(0), m_channelId(0), mChannelMtx()
    {
        m_channelId = SubtecChannelManager::getInstance()->getNextChannelId();
    }

    void SendResetAllPacket()
    {
        std::unique_lock<std::mutex> lock(mChannelMtx);
        m_counter = 1;
        PacketSender::Instance()->SendPacket(SubtecChannelManager::getInstance()->generateResetAllPacket());
    }
    void SendResetChannelPacket() {
        sendPacket<ResetChannelPacket>();
    }
    void SendPausePacket() {
        sendPacket<PausePacket>();
    }
    void SendResumePacket() {
        sendPacket<ResumePacket>();
    }
    void SendMutePacket() {
        sendPacket<MutePacket>();
    }
    void SendUnmutePacket() {
        sendPacket<UnmutePacket>();
    }
    void SendCCSetAttributePacket(std::uint32_t ccType, std::uint32_t attribType, const std::array<uint32_t, 14>& attributesValues) {
        sendPacket<CCSetAttributePacket>(ccType, attribType, attributesValues);
    }

protected:
    uint32_t m_channelId;
    uint32_t m_counter;
    std::mutex mChannelMtx;
};
