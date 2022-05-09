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
* @file AampDRMutils.h
* @brief Data structures to help with DRM sessions.
*/

#ifndef AampDRMutils_h
#define AampDRMutils_h

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>

#include "AampDrmMediaFormat.h"
#include "AampDrmData.h"
#include "AampDrmInfo.h"
#include "AampDrmSystems.h"
#include "AampUtils.h"

/**
 * @brief Macros to track the value of API success or failure
 */
#define DRM_API_SUCCESS (0)
#define DRM_API_FAILED  (-1)

/**
 * @brief start and end tags for DRM policy
 */
#define DRM_METADATA_TAG_START "<ckm:policy xmlns:ckm=\"urn:ccp:ckm\">"
#define DRM_METADATA_TAG_END "</ckm:policy>"

#ifdef USE_SECMANAGER
#define AAMP_SECMGR_INVALID_SESSION_ID (-1)
#endif

/**
 *  @brief	Convert endianness of 16 byte block.
 *
 *  @param[in]	original - Pointer to source byte block.
 *  @param[out]	guidBytes - Pointer to destination byte block.
 *  @return	void.
 */
void aamp_ConvertEndianness(unsigned char *original, unsigned char *guidBytes);
/**
 *  @brief	Extract content meta data or keyID from given PSSH data.
 *  		For example for content meta data,
 *  		When strings are given as "ckm:policy xmlns:ckm="urn:ccp:ckm"" and "ckm:policy"
 *  		<ckm:policy xmlns:ckm="urn:ccp:ckm">we need the contents from here</ckm:policy>
 *
 *  		PSSH is cleaned of empty bytes before extraction steps, since from manifest the
 *  		PSSH data is in 2 bytes. Data dump looking like below, so direct string comparison
 *  		would strstr fail.

 *		000003c0 (0x14d3c0): 3c 00 63 00 6b 00 6d 00 3a 00 70 00 6f 00 6c 00  <.c.k.m.:.p.o.l.
 *		000003d0 (0x14d3d0): 69 00 63 00 79 00 20 00 78 00 6d 00 6c 00 6e 00  i.c.y. .x.m.l.n.
 *		000003e0 (0x14d3e0): 73 00 3a 00 63 00 6b 00 6d 00 3d 00 22 00 75 00  s.:.c.k.m.=.".u.
 *		000003f0 (0x14d3f0): 72 00 6e 00 3a 00 63 00 63 00 70 00 3a 00 63 00  r.n.:.c.c.p.:.c.
 *		00000400 (0x14d400): 6b 00 6d 00 22 00 3e 00 65 00 79 00 4a 00 34 00  k.m.".>.e.y.J.4.
 *		00000410 (0x14d410): 4e 00 58 00 51 00 6a 00 55 00 7a 00 49 00 31 00  N.X.Q.j.U.z.I.1.
 *		00000420 (0x14d420): 4e 00 69 00 49 00 36 00 49 00 6c 00 64 00 51 00  N.i.I.6.I.l.d.Q.
 *
 *  @param[in]	psshData - Pointer to PSSH data.
 *  @param[in]	dataLength - Length of PSSH data.
 *  @param[in]	startStr, endStr - Pointer to delimiter strings.
 *  @param[in]  verStr - Pointer to version delimiter string.
 *  @param[out]	len - Gets updated with length of content meta data.
 *  @return	Extracted data between delimiters; NULL if not found.
 *  @note	Memory of returned data is dynamically allocated, deallocation
 *		should be handled at the caller side.
 */
unsigned char *aamp_ExtractDataFromPssh(const char* psshData, int dataLength, const char* startStr, const char* endStr, int *len, const char* verStr);
/**
 *  @brief	Extract WideVine content meta data from DRM
 *  		Agnostic PSSH header. Might not work with WideVine PSSH header
 *
 *  @param[in]	psshData - Pointer to PSSH data.
 *  @param[in]  dataLength - pssh data length
 *  @return	Extracted ContentMetaData.
 */
std::string aamp_ExtractWVContentMetadataFromPssh(const char* psshData, int dataLength);

unsigned char * aamp_ExtractKeyIdFromPssh(const char* psshData, int dataLength, int *len, DRMSystems drmSystem);

#endif
