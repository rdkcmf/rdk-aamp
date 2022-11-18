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

#include <string>
#include <cstdint>
#include <memory>
#include <vector>
#include <array>
#include <limits>
#include <mutex>
#include "SubtecAttribute.hpp"

class Packet
{
public:
    Packet() : m_buffer(), m_counter(std::numeric_limits<std::uint32_t>::max()) {}
    Packet(std::uint32_t counter) : m_buffer(), m_counter(counter) {}

    const uint32_t getType()
    {
        uint32_t type = 0;

        if (getBuffer().size() >= 4)
        {
            std::vector<std::uint8_t> buffer = getBuffer();
            for (int i = 0; i < 4; i++)
            {
                type += (buffer[i] << (i*8)) & 0xFF;
            }
        }
        return type;
    }

    const std::vector<uint8_t>& getBytes()
    {
        return m_buffer;
    }
    
    const std::uint32_t getCounter()
    {
        return m_counter;
    }

    static std::string getTypeString(uint32_t type)
    {
        std::string ret;
        PacketType pktType = static_cast<PacketType>(type);

        switch(pktType)
        {
        case PacketType::PES_DATA:
            ret = "PES_DATA";
            break;
        case PacketType::TIMESTAMP:
            ret = "TIMESTAMP";
            break;
        case PacketType::RESET_ALL:
            ret = "RESET_ALL";
            break;
        case PacketType::RESET_CHANNEL:
            ret = "RESET_CHANNEL";
            break;
        case PacketType::SUBTITLE_SELECTION:
            ret = "SUBTITLE_SELECTION";
            break;
        case PacketType::TELETEXT_SELECTION:
            ret = "TELETEXT_SELECTION";
            break;
        case PacketType::TTML_SELECTION:
            ret = "TTML_SELECTION";
            break;
        case PacketType::TTML_DATA:
            ret = "TTML_DATA";
            break;
        case PacketType::TTML_TIMESTAMP:
            ret = "TTML_TIMESTAMP";
            break;
        case PacketType::WEBVTT_SELECTION:
            ret = "WEBVTT_SELECTION";
            break;
        case PacketType::WEBVTT_DATA:
            ret = "WEBVTT_DATA";
            break;
        case PacketType::WEBVTT_TIMESTAMP:
            ret = "WEBVTT_TIMESTAMP";
            break;
        case PacketType::CC_DATA :
            ret = "CC_DATA";
            break;
        case PacketType::PAUSE :
            ret = "PAUSE";
            break;
        case PacketType::RESUME :
            ret = "RESUME";
            break;
        case PacketType::MUTE :
            ret = "MUTE";
            break;
        case PacketType::UNMUTE :
            ret = "UNMUTE";
            break;
        case PacketType::CC_SET_ATTRIBUTE:
            ret = "CC_SET_ATTRIBUTE";
            break;
        case PacketType::INVALID:
            ret = "INVALID";
            break;
        default:
            ret = "UNKNOWN";
            break;
        }

        return ret;
    }


protected:
    std::vector<uint8_t>& getBuffer() { return m_buffer; }

    enum class PacketType : std::uint32_t
    {
        ZERO,
        PES_DATA,
        TIMESTAMP,
        RESET_ALL,
        RESET_CHANNEL,
        SUBTITLE_SELECTION,
        TELETEXT_SELECTION,
        TTML_SELECTION,
        TTML_DATA,
        TTML_TIMESTAMP,
        CC_DATA,
        PAUSE,
        RESUME,
        MUTE,
        UNMUTE,
        WEBVTT_SELECTION,
        WEBVTT_DATA,
        WEBVTT_TIMESTAMP,
        CC_SET_ATTRIBUTE,

        INVALID = 0xFFFFFFFF,
    };

    std::vector<uint8_t> m_buffer;
    std::uint32_t m_counter;

    void append32(std::uint32_t value)
    {
        m_buffer.push_back((static_cast<std::uint8_t>((value >> 0)) & 0xFF));
        m_buffer.push_back((static_cast<std::uint8_t>((value >> 8)) & 0xFF));
        m_buffer.push_back((static_cast<std::uint8_t>((value >> 16)) & 0xFF));
        m_buffer.push_back((static_cast<std::uint8_t>((value >> 24)) & 0xFF));
    }

    void append64(std::int64_t value)
    {
        append32((static_cast<std::int32_t>((value >> 0)) & 0xFFFFFFFF));
        append32((static_cast<std::int32_t>((value >> 32)) & 0xFFFFFFFF));
    }

    void appendType(PacketType type)
    {
        append32(static_cast<std::underlying_type<PacketType>::type>(type));
    }
};

using PacketPtr = std::unique_ptr<Packet>;

class DummyPacket : public Packet
{
public:
    DummyPacket() : Packet() {}
};

/**
 * Pause packet.
 */
class PausePacket : public Packet
{
public:

    /**
     * Constructor.
     *
     * @param counter
     *      Packet counter.
     */
    PausePacket(std::uint32_t channelId,
                std::uint32_t counter) : Packet(counter)
    {
        appendType(PacketType::PAUSE);
        append32(counter);
        append32(4);
        append32(channelId);
    }
};

/**
 * Resume packet.
 */
class ResumePacket : public Packet
{
public:

    /**
     * Constructor.
     *
     * @param counter
     *      Packet counter.
     */
    ResumePacket(std::uint32_t channelId,
                 std::uint32_t counter) : Packet(counter)
    {
        appendType(PacketType::RESUME);
        append32(counter);
        append32(4);
        append32(channelId);
    }
};

/**
 * Mute packet.
 */
class MutePacket : public Packet
{
public:

    /**
     * Constructor.
     *
     * @param counter
     *      Packet counter.
     */
    MutePacket(std::uint32_t channelId,
               std::uint32_t counter) : Packet(counter)
    {
        appendType(PacketType::MUTE);
        append32(counter);
        append32(4);
        append32(channelId);
    }
};

/**
 * Mute packet.
 */
class UnmutePacket : public Packet
{
public:

    /**
     * Constructor.
     *
     * @param counter
     *      Packet counter.
     */
    UnmutePacket(std::uint32_t channelId,
                 std::uint32_t counter) : Packet(counter)
    {
        appendType(PacketType::UNMUTE);
        append32(counter);
        append32(4);
        append32(channelId);
    }
};

/**
 * Reset all data packet.
 */

class ResetAllPacket : public Packet
{
public:

    /**
     * Constructor.
     *
     * @param counter
     *      Packet counter.
     */
    ResetAllPacket() : Packet(0)
    {
        appendType(PacketType::RESET_ALL);
        append32(0);
        append32(0);
    }
};

/**
 * Reset all data packet.
 */

class ResetChannelPacket : public Packet
{
public:

    /**
     * Constructor.
     *
     * @param counter
     *      Packet counter.
     */
    ResetChannelPacket(std::uint32_t channelId,
                       std::uint32_t counter) : Packet(counter)
    {
        appendType(PacketType::RESET_CHANNEL);
        append32(counter);
        append32(4);
        append32(channelId);
    }
};


/*

field           size    value       description
type            4       18          message type
counter         4       0..n
size            4       68          specifies size of transferred "data" that includes channelId, ccType, attribType and attributes payload

data:

channelId           4                   Specifies channel on which data for subtitles is transmitted.
ccType              4       {0,1}       0 - analog, 1 - digital
attribType          4                   bitmask specifying which attribs are set

attributes payload: -
1.  FONT_COLOR          4       0..n
2.  BACKGROUND_COLOR    4       0..n
3.  FONT_OPACITY        4       0..n
4.  BACKGROUND_OPACITY  4       0..n
5.  FONT_STYLE          4       0..n
6.  FONT_SIZE           4       0..n
7.  FONT_ITALIC         4       0..n
8.  FONT_UNDERLINE      4       0..n
9.  BORDER_TYPE         4       0..n
10. BORDER_COLOR        4       0..n
11. WIN_COLOR           4       0..n
12. WIN_OPACITY         4       0..n
13. EDGE_TYPE           4       0..n
14. EDGE_COLOR          4       0..n

When adding/removing an attribute to the above list, update the definition in the file SubtecAttribute.hpp

*/

class CCSetAttributePacket : public Packet
{
public:

    /**
     * Constructor.
     *
     * @param counter
     *      Packet counter.
     */
    CCSetAttributePacket(std::uint32_t channelId,
                         std::uint32_t counter,
                         std::uint32_t ccType,
                         std::uint32_t attribType,
                         const attributesType &attributesValues) : Packet(counter)
    {
        appendType(PacketType::CC_SET_ATTRIBUTE);
        append32(counter);
        append32((NUMBER_OF_ATTRIBUTES+3)*4);
        append32(channelId);
        append32(ccType);
        append32(attribType);

        for(const auto value : attributesValues)
            append32(value);
    }
};
