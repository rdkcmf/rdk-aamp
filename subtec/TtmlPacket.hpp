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

class TtmlSelectionPacket : public Packet
{
public:
    /**
     * Constructor.
     *
     * @param channelId
     *      Packet channelId.
     * @param counter
     *      Packet counter.
     * @param width
     *      Display width.
     * @param height
     *      Display height.
     */
    TtmlSelectionPacket(uint32_t channelId, uint32_t counter, uint32_t width, uint32_t height) : Packet(counter)
    {
        appendType(Packet::PacketType::TTML_SELECTION);
        append32(counter);
        append32(TTML_SELECTION_PACKET_SIZE);
        append32(channelId);
        append32(width);
        append32(height);
    }
    
private:
    static constexpr std::uint8_t TTML_SELECTION_PACKET_SIZE = 12;
};

class TtmlDataPacket : public Packet
{
public:

    /**
     * Constructor.
     *
     * @param channelId
     *      Packet channelId.
     * @param counter
     *      Packet counter.
     * @param dataOffset
     *      Data offset if needed
     * @param dataBuffer
     *      Packet data.
     */
    TtmlDataPacket(std::uint32_t channelId,
                   std::uint32_t counter,
                   std::int64_t dataOffset,
                   std::vector<std::uint8_t> &&dataBuffer) : Packet(counter)
    {
        auto& buffer = getBuffer();
        uint32_t size = 8 + 4 + dataBuffer.size();

        appendType(PacketType::TTML_DATA);
        append32(counter);
        append32(size);
        append32(channelId);
        append64(dataOffset);

        buffer.insert(buffer.end(), dataBuffer.begin(), dataBuffer.end());
    }
};


class TtmlTimestampPacket : public Packet
{
public:

    /**
     * Constructor.
     *
     * @param counter
     *      Packet counter.
     */
    TtmlTimestampPacket(std::uint32_t channelId,
                    std::uint32_t counter,
                    std::uint64_t timestamp) : Packet(counter)
    {
        appendType(PacketType::TTML_TIMESTAMP);
        append32(counter);
        append32(TTML_TIMESTAMP_PACKET_SIZE);
        append32(channelId);
        append64(timestamp);
    }

private:

    static constexpr std::uint8_t TTML_TIMESTAMP_PACKET_SIZE = 12;
};


class TtmlChannel : public SubtecChannel
{
public:
    TtmlChannel() : SubtecChannel() {}

    void SendSelectionPacket(uint32_t width, uint32_t height) {
        sendPacket<TtmlSelectionPacket>(width, height);
    }
    void SendDataPacket(std::vector<uint8_t> &&data, std::int64_t time_offset_ms = 0) {
        sendPacket<TtmlDataPacket>(time_offset_ms, std::move(data));
    }
    void SendTimestampPacket(uint64_t timestampMs) {
        sendPacket<TtmlTimestampPacket>(timestampMs);
    }
};