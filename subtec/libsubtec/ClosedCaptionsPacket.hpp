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

#include "SubtecPacket.hpp"
#include "SubtecChannel.hpp"
#include "PacketSender.hpp"


class ClosedCaptionsPacket : public Packet
{
public:

    ClosedCaptionsPacket(uint32_t channelId, uint32_t counter, uint32_t ptsValue, uint8_t* data, size_t dataLen)
    {
        appendType(Packet::PacketType::CC_DATA);
        append32(counter);
        append32(CC_DATA_HEADER_LEN+dataLen);
        append32(channelId);
        append32(CC_CHANNEL_TYPE);
        append32(1); // pts presence
        append32(ptsValue);
        for(int i = 0; i<dataLen; ++i)
            getBuffer().push_back(data[i]);
    }

    ClosedCaptionsPacket(uint32_t channelId, uint32_t counter, uint8_t* data, size_t dataLen)
    {
        appendType(Packet::PacketType::CC_DATA);
        append32(counter);
        append32(CC_DATA_HEADER_LEN+dataLen);
        append32(channelId);
        append32(CC_CHANNEL_TYPE);
        append32(0); // pts presence
        append32(0); // no pts value
        for(int i = 0; i<dataLen; ++i)
            getBuffer().push_back(data[i]);
    }

private:
    static constexpr std::uint8_t CC_DATA_HEADER_LEN = 16;
    static constexpr std::uint8_t CC_CHANNEL_TYPE = 3;
};

class ClosedCaptionsActiveTypePacket: public Packet
{
public:
    enum class CEA : uint32_t
    {
        type_608 = 0,
        type_708 = 1
    };

    ClosedCaptionsActiveTypePacket(uint32_t channelId, uint32_t counter, CEA type, int service)
    {
        auto to_integral = [](CEA e) -> uint32_t
        {
            return static_cast<uint32_t>(e);
        };

        appendType(Packet::PacketType::SUBTITLE_SELECTION);
        append32(counter);
        append32(CC_SELECTION_LEN);
        append32(channelId);
        append32(CC_USERDATA_SUBTITLE_TYPE);
        append32(to_integral(type)); //aux id 1
        append32(service);//aux id 2


    }


private:
    static constexpr std::uint8_t CC_SELECTION_LEN = 16;
    static constexpr std::uint8_t CC_USERDATA_SUBTITLE_TYPE = 3;
};

class ClosedCaptionsChannel : public SubtecChannel
{
public:
    ClosedCaptionsChannel() {}
    
    void SendDataPacketWithPTS(uint32_t ptsValue, uint8_t* data, size_t dataLen)
    {
        std::unique_lock<std::mutex> lock(mChannelMtx);
        PacketSender::Instance()->SendPacket(std::unique_ptr<ClosedCaptionsPacket>(new ClosedCaptionsPacket(m_channelId, m_counter++, ptsValue, data, dataLen)));
    }

    void SendDataPacketNoPTS(uint8_t* data, size_t dataLen)
    {
        std::unique_lock<std::mutex> lock(mChannelMtx);
        PacketSender::Instance()->SendPacket(std::unique_ptr<ClosedCaptionsPacket>(new ClosedCaptionsPacket(m_channelId, m_counter++, data, dataLen)));
    }

    void SendActiveTypePacket(ClosedCaptionsActiveTypePacket::CEA type, unsigned int channel)
    {
        std::unique_lock<std::mutex> lock(mChannelMtx);
        PacketSender::Instance()->SendPacket(std::unique_ptr<ClosedCaptionsActiveTypePacket>(new ClosedCaptionsActiveTypePacket(m_channelId, m_counter++, type, channel)));
    }
};