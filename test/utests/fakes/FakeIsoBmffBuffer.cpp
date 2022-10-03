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

#include "isobmffbuffer.h"

IsoBmffBuffer::~IsoBmffBuffer()
{
}

bool IsoBmffBuffer::isInitSegment()
{
    return false;
}

void IsoBmffBuffer::setBuffer(uint8_t *buf, size_t sz)
{
}

bool IsoBmffBuffer::parseBuffer(bool correctBoxSize, int newTrackId)
{
    return false;
}

bool IsoBmffBuffer::getTimeScale(uint32_t &timeScale)
{
    return false;
}

void IsoBmffBuffer::destroyBoxes()
{
}

bool IsoBmffBuffer::getEMSGData(uint8_t* &message, uint32_t &messageLen, uint8_t* &schemeIdUri, uint8_t* &value, uint64_t &presTime, uint32_t &timeScale, uint32_t &eventDuration, uint32_t &id)
{
    return false;
}
