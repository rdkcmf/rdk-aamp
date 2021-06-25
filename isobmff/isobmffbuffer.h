/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2019 RDK Management
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
* @file isobmffbuffer.h
* @brief Header file for ISO Base Media File Format Buffer
*/

#ifndef __ISOBMFFBUFFER_H__
#define __ISOBMFFBUFFER_H__

#include "isobmffbox.h"
#include <stddef.h>
#include <vector>
#include <cstdint>

/**
 * @brief Class for ISO BMFF Buffer
 */
class IsoBmffBuffer
{
private:
	std::vector<Box*> boxes;	//ISOBMFF boxes of associated buffer
	uint8_t *buffer;
	size_t bufSize;
	Box* chunkedBox; //will hold one element only
	size_t mdatCount;

	/**
	 * @brief Get first PTS of buffer
	 *
	 * @param[in] boxes - ISOBMFF boxes
	 * @param[out] pts - pts value
	 * @return true if parse was successful. false otherwise
	 */
	bool getFirstPTSInternal(const std::vector<Box*> *boxes, uint64_t &pts);

	/**
	 * @brief Get TimeScale value of buffer
	 *
	 * @param[in] boxes - ISOBMFF boxes
	 * @param[out] timeScale - TimeScale value
	 * @param[out] foundMdhd - flag indicates if MDHD box was seen
	 * @return true if parse was successful. false otherwise
	 */
	bool getTimeScaleInternal(const std::vector<Box*> *boxes, uint32_t &timeScale, bool &foundMdhd);

	/**
	 * @brief Print ISOBMFF boxes
	 *
	 * @param[in] boxes - ISOBMFF boxes
	 * @return void
	 */
	void printBoxesInternal(const std::vector<Box*> *boxes);

    /**
     * @brief parse ISOBMFF boxes of a type in a parsed buffer
     *
     * @param[in] boxes - ISOBMFF boxes
     * @param[in] const char * - box name to get
     * @param[out] uint8_t * - mdat buffer pointer
     * @param[out] size_t - size of mdat buffer
     * @return bool
     */
	bool parseBoxInternal(const std::vector<Box*> *boxes, const char *name, uint8_t *buf, size_t &size);

    /**
     * @brief get ISOBMFF box size of a type
     *
     * @param[in] boxes - ISOBMFF boxes
     * @param[in] const char * - box name to get
     * @param[out] size_t - size of mdat buffer
     * @return bool
     */
	bool getBoxSizeInternal(const std::vector<Box*> *boxes, const char *name, size_t &size);

    /**
     * @brief get ISOBMFF box list of a type in a parsed buffer
     *
     * @param[in] boxes - ISOBMFF boxes
     * @param[in] const char * - box name to get
     * @param[out] size_t - size of mdat buffer
     * @return bool
     */
    bool getBoxesInternal(const std::vector<Box*> *boxes, const char *name, std::vector<Box*> *pBoxes);

public:
	/**
	 * @brief IsoBmffBuffer constructor
	 */
	IsoBmffBuffer(): boxes(), buffer(NULL), bufSize(0), chunkedBox(NULL), mdatCount(0)
	{

	}

	/**
	 * @brief IsoBmffBuffer destructor
	 */
	~IsoBmffBuffer();

	IsoBmffBuffer(const IsoBmffBuffer&) = delete;
	IsoBmffBuffer& operator=(const IsoBmffBuffer&) = delete;

	/**
	 * @brief Set buffer
	 *
	 * @param[in] buf - buffer pointer
	 * @param[in] sz - buffer size
	 * @return void
	 */
	void setBuffer(uint8_t *buf, size_t sz);

	/**
	 * @brief Parse ISOBMFF boxes from buffer
	 *
	 * @return true if parse was successful. false otherwise
	 */
	bool parseBuffer(bool correctBoxSize = false);

	/**
	 * @brief Restamp PTS in a buffer
	 *
	 * @param[in] offset - pts offset
	 * @param[in] basePts - base pts
	 * @param[in] segment - buffer pointer
	 * @param[in] bufSz - buffer size
	 * @return void
	 */
	void restampPTS(uint64_t offset, uint64_t basePts, uint8_t *segment, uint32_t bufSz);

	/**
	 * @brief Get first PTS of buffer
	 *
	 * @param[out] pts - pts value
	 * @return true if parse was successful. false otherwise
	 */
	bool getFirstPTS(uint64_t &pts);

	/**
	 * @brief Print PTS of buffer
	 * @return tvoid
	 */
	void PrintPTS(void);

	/**
	 * @brief Get TimeScale value of buffer
	 *
	 * @param[out] timeScale - TimeScale value
	 * @return true if parse was successful. false otherwise
	 */
	bool getTimeScale(uint32_t &timeScale);

	/**
	 * @brief Release ISOBMFF boxes parsed
	 *
	 * @return void
	 */
	void destroyBoxes();

	/**
	 * @brief Print ISOBMFF boxes
	 *
	 * @return void
	 */
	void printBoxes();

	/**
	 * @brief Check if buffer is an initialization segment
	 *
	 * @return true if buffer is an initialization segment. false otherwise
	 */
	bool isInitSegment();

	/**
	* @brief Get mdat buffer handle and size from parsed buffer
	* @param[out] uint8_t * - mdat buffer pointer
	* @param[out] size_t - size of mdat buffer
	* @return true if mdat buffer is available. false otherwise
	*/
	bool parseMdatBox(uint8_t *buf, size_t &size);

	/**
	* @brief Get mdat buffer size
	* @param[out] size_t - size of mdat buffer
	* @return true if buffer size available. false otherwise
	*/
	bool getMdatBoxSize(size_t &size);

	/**
	 * @brief Get information from EMSG box
	 *
	 * @param[out] message - messageData from EMSG
	 * @param[out] messageLen - messageLen
	 * @param[out] schemeIdUri - schemeIdUri
	 * @param[out] value - value of Id3
	 * @param[out] presTime - Presentation time
	 * @param[out] timeScale - timeScale of ID3 metadata
	 * @param[out] eventDuration - eventDuration value
	 * @param[out] id - ID of metadata
	 * @return true if parse was successful. false otherwise
	 */
	bool getEMSGData(uint8_t* &message, uint32_t &messageLen, uint8_t* &schemeIdUri, uint8_t* &value, uint64_t &presTime, uint32_t &timeScale, uint32_t &eventDuration, uint32_t &id);

	/**
	 * @brief Get emsg informations
	 *
	 * @param[in] boxes - ISOBMFF boxes
	 * @param[out] message - messageData pointer
	 * @param[out] messageLen - messageLen value
	 * @param[out] schemeIdUri - schemeIdUri
	 * @param[out] value - value of Id3
	 * @param[out] presTime - Presentation time
	 * @param[out] timeScale - timeScale of ID3 metadata
	 * @param[out] eventDuration - eventDuration value
	 * @param[out] id - ID of metadata
	 * @param[out] foundEmsg - flag indicates if EMSG box was seen
	 * @return true if parse was successful. false otherwise
	 */
	bool getEMSGInfoInternal(const std::vector<Box*> *boxes, uint8_t* &message, uint32_t &messageLen, uint8_t* &schemeIdUri, uint8_t* &value, uint64_t &presTime, uint32_t &timeScale, uint32_t &eventDuration, uint32_t &id, bool &foundEmsg);
	
	/**
	* @brief Check mdat buffer count in parsed buffer
	* @param[out] size_t - mdat box count
	* @return true if mdat count available. false otherwise
	*/
	bool getMdatBoxCount(size_t &count);

	/**
	* @brief Print ISOBMFF mdat boxes in parsed buffer
	*
	* @return void
	*/
	void printMdatBoxes();

	/**
	* @brief Get list of box handle in parsed bufferr using name
	* @param[in] const char * - box name to get
	* @param[out] std::vector<Box*> - List of box handles of a type in a parsed buffer
	* @return true if Box found. false otherwise
	*/
	bool getTypeOfBoxes(const char *name, std::vector<Box*> &stBoxes);

	/**
	* @brief Get list of box handles in a parsed buffer
	*
	* @return Box handle if Chunk box found in a parsed buffer. NULL otherwise
	*/
	Box* getChunkedfBox() const;

	/**
	* @brief Get list of box handles in a parsed buffer
	*
	* @return Box handle list if Box found at index given. NULL otherwise
	*/
	std::vector<Box*> *getParsedBoxes();

	/**
	* @brief Get box handle in parsed bufferr using name
	* @param[in] const char * - box name to get
	* @param[out] size_t - index of box in a parsed buffer
	* @return Box handle if Box found at index given. NULL otherwise
	*/
	Box* getBox(const char *name, size_t &index);

	/**
	* @brief Get box handle in parsed bufferr using index
	* @param[out] size_t - index of box in a parsed buffer
	* @return Box handle if Box found at index given. NULL otherwise
	*/
	Box* getBoxAtIndex(size_t index);

	/**
	* @brief Print ISOBMFF box PTS
	*
	* @param[in] boxes - ISOBMFF boxes
	* @return void
	*/
	void printPTSInternal(const std::vector<Box*> *boxes);

	/**
	* @brief Get ISOBMFF box Sample Duration
	*
	* @param[in] box - ISOBMFF box
	* @param[in] uint64_t* -  duration to get
	* @return void
	*/
	void getSampleDuration(Box *box, uint64_t &fduration);

	/**
	* @brief Get ISOBMFF box Sample Duration
	*
	* @param[in] boxes - ISOBMFF boxes
	* @return uint64_t - duration  value
	*/
	uint64_t getSampleDurationInernal(const std::vector<Box*> *boxes);

	/**
	* @brief Get ISOBMFF box PTS
	*
	* @param[in] boxe - ISOBMFF box
	* @param[in] uint64_t* -  PTS to get
	* @return void
	*/
	void getPts(Box *box, uint64_t &fpts);

	/**
	* @brief Get ISOBMFF box PTS
	*
	* @param[in] boxes - ISOBMFF boxes
	* @return uint64_t - PTS value
	*/
	uint64_t getPtsInternal(const std::vector<Box*> *boxes);
};


#endif /* __ISOBMFFBUFFER_H__ */
