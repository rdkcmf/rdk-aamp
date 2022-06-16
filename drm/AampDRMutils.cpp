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
 * @file AampDRMutils.cpp
 * @brief DataStructures and methods for DRM license acquisition
 */

#include "AampDRMutils.h"
#include "AampConfig.h"
#include "_base64.h"

#include <uuid/uuid.h>
#include <regex>

#define KEYID_TAG_START "<KID>"
#define KEYID_TAG_END "</KID>"

/* Regex to detect if a string starts with a protocol definition e.g. http:// */
static const std::string PROTOCOL_REGEX = "^[a-zA-Z0-9\\+\\.-]+://";
#define KEY_ID_SZE_INDICATOR 0x12

/**
 *  @brief  Default constructor for DrmData.
 *          NULL initialize data and dataLength.
 */
DrmData::DrmData() : data("")
{
}

/**
 *  @brief Constructor for DrmData
 *         allocate memory and initialize data and
 *         dataLength with given params.
 *
 */
DrmData::DrmData(unsigned char *data, int dataLength) : data("")
{
		this->data.assign(reinterpret_cast<const char*>(data),dataLength);
}

/**
 *  @brief   Distructor for DrmData.
 *           Free memory (if any) allocated for data.
 */
DrmData::~DrmData()
{
	if(!data.empty())
	{
		data.clear();
	}
}

/**
 *  @brief Getter method for data.
 *
 */
const std::string &DrmData::getData()
{
	return data;
}

/**
 *  @brief  Getter method for dataLength.
 */
int DrmData::getDataLength()
{
	return (this->data.length());
}

/**
 *  @brief Updates DrmData with given data.
 */
void DrmData::setData(unsigned char *data, int dataLength)
{
	if(!this->data.empty())
	{
		this->data.clear();
	}
	this->data.assign(reinterpret_cast<const char*>(data),dataLength);
}

/**
 *  @brief  Appends DrmData with given data.
 */
void DrmData::addData(unsigned char *data, int dataLength)
{
	if(this->data.empty())
	{
		this->setData(data,dataLength);
	}
	else
	{
		std::string key;
		key.assign(reinterpret_cast<const char*>(data),dataLength);
		this->data       = this->data  + key;
	}
}

/**
 *  @brief	Swap the bytes at given positions.
 *
 *  @param[out]	bytes - Pointer to byte block where swapping is done.
 *  @param[in]	pos1, pos2 - Swap positions.
 *  @return		void.
 */
static void aamp_SwapBytes(unsigned char *bytes, int pos1, int pos2)
{
	unsigned char temp = bytes[pos1];
	bytes[pos1] = bytes[pos2];
	bytes[pos2] = temp;
}

/**
 * @brief Convert endianness of 16 byte block. 
 */
void aamp_ConvertEndianness(unsigned char *original, unsigned char *guidBytes)
{
	memcpy(guidBytes, original, 16);
	aamp_SwapBytes(guidBytes, 0, 3);
	aamp_SwapBytes(guidBytes, 1, 2);
	aamp_SwapBytes(guidBytes, 4, 5);
	aamp_SwapBytes(guidBytes, 6, 7);
}

/**
 *  @brief  Extract WideVine content meta data from DRM
 *          Agnostic PSSH header. Might not work with WideVine PSSH header
 *
 */
std::string aamp_ExtractWVContentMetadataFromPssh(const char* psshData, int dataLength)
{
	//WV PSSH format 4+4+4+16(system id)+4(data size)
	uint32_t header = 28;
	unsigned char* content_id = NULL;
	std::string metadata;
	uint32_t  content_id_size =
                    (uint32_t)((psshData[header] & 0x000000FFu) << 24 |
                               (psshData[header+1] & 0x000000FFu) << 16 |
                               (psshData[header+2] & 0x000000FFu) << 8 |
                               (psshData[header+3] & 0x000000FFu));

	AAMPLOG_INFO("content meta data length  : %d",content_id_size);
	if ((header + 4 + content_id_size) <= dataLength)
	{
		metadata = std::string(psshData + header + 4, content_id_size);
	}
	else
	{
		AAMPLOG_WARN("psshData : %d bytes in length, metadata would read past end of buffer", dataLength);
	}

	return metadata;
}
//End of special for Widevine

/**
 * @brief Get the base URI of a resource
 *
 * @param uri URI of a resource
 * @param originOnly if true, only the domain (and port, if present) is returned.
 *					 if false, the full parent path of the resource is returned
 * @return base URI
 */
static std::string aamp_getBaseUri(std::string uri, bool originOnly)
{
	std::smatch results;
	if (std::regex_match(uri, results, std::regex("(" + PROTOCOL_REGEX + "[^/]+)/.*")))
	{
		return originOnly ? results[1].str() : uri.substr(0, uri.rfind("/"));
	}

	return uri;
}

