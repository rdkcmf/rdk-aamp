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


DrmData::DrmData() : data("")
{
}


DrmData::DrmData(unsigned char *data, int dataLength) : data("")
{
		this->data.assign(reinterpret_cast<const char*>(data),dataLength);
}


DrmData::~DrmData()
{
	if(!data.empty())
	{
		data.clear();
	}
}


const std::string &DrmData::getData()
{
	return data;
}



int DrmData::getDataLength()
{
	return (this->data.length());
}


void DrmData::setData(unsigned char *data, int dataLength)
{
	if(!this->data.empty())
	{
		this->data.clear();
	}
	this->data.assign(reinterpret_cast<const char*>(data),dataLength);
}


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
 *  @brief	Find the position of substring in cleaned PSSH data.
 *  		Cleaned PSSH data, is PSSH data from which empty bytes are removed.
 *  		Used while extracting keyId or content meta data from PSSH.
 *
 *  @param[in]	psshData - Pointer to cleaned PSSH data.
 *  @param[in]	dataLength - Length of cleaned PSSH data.
 *  @param[in]	pos - Search start position.
 *  @param[in]	substr - Pointer to substring to be searched.
 *  @param[out]	substrStartPos - Default NULL; If not NULL, gets updated with end
 *  			position of the substring in Cleaned PSSH; -1 if not found.
 *  @return		Start position of substring in cleaned PSSH; -1 if not found.
 */
static int aamp_FindSubstr(const char* psshData, int dataLength, int pos, const char* substr, int *substrStartPos = NULL)
{
	int subStrLen = strlen(substr);
	int psshIter = pos;
	int subStrIter = 0;
	bool strMatched = false;
	while (psshIter < dataLength)
	{
		if (psshData[psshIter] == substr[subStrIter])
		{
			if(substrStartPos && subStrIter == 0)
			{
				*substrStartPos = psshIter;
			}
			subStrIter++;
			if (subStrIter == subStrLen)
			{
				strMatched = true;
				break;
			}
		}
		else
		{
			subStrIter = 0;
		}
		psshIter++;
	}

	if(strMatched)
	{
		return psshIter;
	}
	else
	{
		if(substrStartPos)
		{
			*substrStartPos = -1;
		}
		return -1;
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


void aamp_ConvertEndianness(unsigned char *original, unsigned char *guidBytes)
{
	memcpy(guidBytes, original, 16);
	aamp_SwapBytes(guidBytes, 0, 3);
	aamp_SwapBytes(guidBytes, 1, 2);
	aamp_SwapBytes(guidBytes, 4, 5);
	aamp_SwapBytes(guidBytes, 6, 7);
}


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


unsigned char * aamp_ExtractDataFromPssh(const char* psshData, int dataLength,
						const char* startStr, const char* endStr, int *len, const char* verStr) {
	int endPos = -1;
	unsigned char* contentMetaData = NULL;
	const char* verTag = "version=\"";
	bool verError = true;

	//Clear the 00  bytes
	char* cleanedPssh = (char*) malloc(dataLength);
	int cleanedPsshLen = 0;
	for(int itr = 0; itr < dataLength; itr++)
	{
		if(psshData[itr] != 0)
		{
			//cout<<psshData[itr];
			cleanedPssh[cleanedPsshLen++] = psshData[itr];
		}
	}

	int verPos = aamp_FindSubstr(cleanedPssh, cleanedPsshLen, 0, verTag);
	if (verPos > 0)
	{
		//compare with version values, if pssh version is greater than supported version then update error
		char* psshVer = cleanedPssh + verPos + 1;
		int val = strncmp(verStr, psshVer,7);
		if (val < 0)
		{
			AAMPLOG_WARN ("Unsupported PSSH version AAMP[%s] MPD[%c.%c.%c.%c]", verStr, psshVer[0],psshVer[2],psshVer[4],psshVer[6]);
		}
		else
		{
			verError = false; // reset verError on pssh version supported case
		}
	}

	if (!verError)
	{
		int startPos = aamp_FindSubstr(cleanedPssh, cleanedPsshLen, 0, startStr);  //CID:108195 - UNUSED_VALUE - Providing a direct declaration

		if(startPos >= 0)
		{
			aamp_FindSubstr(cleanedPssh, cleanedPsshLen, startPos, endStr, &endPos);
			if(endPos > 0 && startPos < endPos)
			{
				*len = endPos - startPos - 1;
				contentMetaData = (unsigned char*)malloc(*len + 1);
				memset(contentMetaData, 0, *len + 1);
				strncpy(reinterpret_cast<char*>(contentMetaData),reinterpret_cast<char*>(cleanedPssh + startPos + 1), *len);
			}
		}
	}
	free(cleanedPssh);
	return contentMetaData;
}

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

