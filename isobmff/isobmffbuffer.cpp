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
* @file isobmffbuffer.cpp
* @brief Source file for ISO Base Media File Format Buffer
*/

#include "isobmffbuffer.h"
#include "priv_aamp.h" //Required for AAMPLOG_WARN1
#include <string.h>

/**
 * @brief IsoBmffBuffer destructor
 */
IsoBmffBuffer::~IsoBmffBuffer()
{
	for (unsigned int i=boxes.size(); i>0;)
	{
		--i;
		SAFE_DELETE(boxes[i]);
		boxes.pop_back();
	}
	boxes.clear();
}

/**
 * @brief Set buffer
 *
 * @param[in] buf - buffer pointer
 * @param[in] sz - buffer size
 * @return void
 */
void IsoBmffBuffer::setBuffer(uint8_t *buf, size_t sz)
{
	buffer = buf;
	bufSize = sz;
}

/**
 * @brief Parse ISOBMFF boxes from buffer
 *
 * @return true if parse was successful. false otherwise
 */
bool IsoBmffBuffer::parseBuffer(bool correctBoxSize)
{
	size_t curOffset = 0;
	while (curOffset < bufSize)
	{
		Box *box = Box::constructBox(buffer+curOffset, bufSize - curOffset, mLogObj, correctBoxSize);
		if( ((bufSize - curOffset) < 4) || ( (bufSize - curOffset) < box->getSize()) )
		{
			chunkedBox = box;
		}
		box->setOffset(curOffset);
		boxes.push_back(box);
		curOffset += box->getSize();
	}
	return !!(boxes.size());
}

/**
 * @brief Get mdat buffer handle and size from parsed buffer
 * @param[out] uint8_t * - mdat buffer pointer
 * @param[out] size_t - size of mdat buffer
 * @return true if mdat buffer is available. false otherwise
 */
bool IsoBmffBuffer::parseMdatBox(uint8_t *buf, size_t &size)
{
	return parseBoxInternal(&boxes, Box::MDAT, buf, size);
}

#define BOX_HEADER_SIZE 8

/**
 * @brief parse ISOBMFF boxes of a type in a parsed buffer
 *
 * @param[in] boxes - ISOBMFF boxes
 * @param[in] const char * - box name to get
 * @param[out] uint8_t * - mdat buffer pointer
 * @param[out] size_t - size of mdat buffer
 * @return bool
 */
bool IsoBmffBuffer::parseBoxInternal(const std::vector<Box*> *boxes, const char *name, uint8_t *buf, size_t &size)
{
	for (size_t i = 0; i < boxes->size(); i++)
	{
		Box *box = boxes->at(i);
		AAMPLOG_TRACE("Offset[%u] Type[%s] Size[%u]", box->getOffset(), box->getType(), box->getSize());
		if (IS_TYPE(box->getType(), name))
		{
			size_t offset = box->getOffset() + BOX_HEADER_SIZE;
			size = box->getSize() - BOX_HEADER_SIZE;
			memcpy(buf, buffer + offset, size);
			return true;
		}
	}
	return false;
}

/**
 * @brief Get mdat buffer size
 * @param[out] size_t - size of mdat buffer
 * @return true if buffer size available. false otherwise
 */
bool IsoBmffBuffer::getMdatBoxSize(size_t &size)
{
	return getBoxSizeInternal(&boxes, Box::MDAT, size);
}

/**
 * @brief get ISOBMFF box size of a type
 *
 * @param[in] boxes - ISOBMFF boxes
 * @param[in] const char * - box name to get
 * @param[out] size_t - size of mdat buffer
 * @return bool
 */
bool IsoBmffBuffer::getBoxSizeInternal(const std::vector<Box*> *boxes, const char *name, size_t &size)
{
	for (size_t i = 0; i < boxes->size(); i++)
	{
		Box *box = boxes->at(i);
		if (IS_TYPE(box->getType(), name))
		{
			size = box->getSize();
			return true;
		}
	}
	return false;
}

/**
 * @brief Restamp PTS in a buffer
 *
 * @param[in] offset - pts offset
 * @param[in] basePts - base PTS
 * @param[in] segment - buffer pointer
 * @param[in] bufSz - buffer size
 * @return void
 */
void IsoBmffBuffer::restampPTS(uint64_t offset, uint64_t basePts, uint8_t *segment, uint32_t bufSz)
{
	// TODO: Untest code, not required for now
	uint32_t curOffset = 0;
	while (curOffset < bufSz)
	{
		uint8_t *buf = segment + curOffset;
		uint32_t size = READ_U32(buf);
		uint8_t type[5];
		READ_U8(type, buf, 4);
		type[4] = '\0';

		if (IS_TYPE(type, Box::MOOF) || IS_TYPE(type, Box::TRAF))
		{
			restampPTS(offset, basePts, buf, size);
		}
		else if (IS_TYPE(type, Box::TFDT))
		{
			uint8_t version = READ_VERSION(buf);
			uint32_t flags  = READ_FLAGS(buf);

			if (1 == version)
			{
				uint64_t pts = ReadUint64(buf);
				pts -= basePts;
				pts += offset;
				WriteUint64(buf, pts);
			}
			else
			{
				uint32_t pts = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
				pts -= (uint32_t)basePts;
				pts += (uint32_t)offset;
				WRITE_U32(buf, pts);
			}
		}
		curOffset += size;
	}
}

/**
 * @brief Release ISOBMFF boxes parsed
 *
 * @return void
 */
void IsoBmffBuffer::destroyBoxes()
{
	for (unsigned int i=boxes.size(); i>0;)
	{
		--i;
		SAFE_DELETE(boxes[i]);
		boxes.pop_back();
	}
	boxes.clear();
}

/**
 * @brief Get first PTS of buffer
 *
 * @param[in] boxes - ISOBMFF boxes
 * @param[out] pts - pts value
 * @return true if parse was successful. false otherwise
 */
bool IsoBmffBuffer::getFirstPTSInternal(const std::vector<Box*> *boxes, uint64_t &pts)
{
	bool ret = false;
	for (size_t i = 0; (false == ret) && i < boxes->size(); i++)
	{
		Box *box = boxes->at(i);
		if (IS_TYPE(box->getType(), Box::TFDT))
		{
			pts = dynamic_cast<TfdtBox *>(box)->getBaseMDT();
			ret = true;
			break;
		}
		if (box->hasChildren())
		{
			ret = getFirstPTSInternal(box->getChildren(), pts);
		}
	}
	return ret;
}

/**
 * @brief Get track_id of the stream by traversing the boxes recursively
 *
 * @param[in] boxes - ISOBMFF boxes
 * @param[out] track_id - trak_id value
 * @return true if parse was successful. false otherwise
 */
bool IsoBmffBuffer::getTrackIdInternal(const std::vector<Box*> *boxes, uint32_t &track_id)
{
	bool ret = false;
	for (size_t i = 0; (false == ret) && i < boxes->size(); i++)
	{
		Box *box = boxes->at(i);
		if (IS_TYPE(box->getType(), Box::TRAK))
		{
			try {
				track_id = dynamic_cast<TrakBox *>(box)->getTrack_Id();
				ret = true;
				break;
			} catch (std::bad_cast& bc){
				//do nothing
			}			
		}
		if (box->hasChildren())
		{
			ret = getTrackIdInternal(box->getChildren(), track_id);
		}
	}
	return ret;
}

/**
 * @brief Get first PTS of buffer
 *
 * @param[out] pts - pts value
 * @return true if parse was successful. false otherwise
 */
bool IsoBmffBuffer::getFirstPTS(uint64_t &pts)
{
	return getFirstPTSInternal(&boxes, pts);
}

/**
 * @brief Print PTS of buffer
 * @return tvoid
 */
void IsoBmffBuffer::PrintPTS(void)
{
    return printPTSInternal(&boxes);
}

/**
 * @brief Get track_d iof the stream
 *
 * @param[out] track_id - track id
 * @return true if parse was successful. false otherwise
 */
bool IsoBmffBuffer::getTrack_id(uint32_t &track_id)
{
	return getTrackIdInternal(&boxes, track_id);
}


/**
 * @brief Get TimeScale value of buffer
 *
 * @param[in] boxes - ISOBMFF boxes
 * @param[out] timeScale - TimeScale value
 * @param[out] foundMdhd - flag indicates if MDHD box was seen
 * @return true if parse was successful. false otherwise
 */
bool IsoBmffBuffer::getTimeScaleInternal(const std::vector<Box*> *boxes, uint32_t &timeScale, bool &foundMdhd)
{
	bool ret = false;
	for (size_t i = 0; (false == foundMdhd) && i < boxes->size(); i++)
	{
		Box *box = boxes->at(i);
		if (IS_TYPE(box->getType(), Box::MVHD))
		{
			timeScale = dynamic_cast<MvhdBox *>(box)->getTimeScale();
			ret = true;
		}
		else if (IS_TYPE(box->getType(), Box::MDHD))
		{
			timeScale = dynamic_cast<MdhdBox *>(box)->getTimeScale();
			ret = true;
			foundMdhd = true;
		}
		if (box->hasChildren())
		{
			ret = getTimeScaleInternal(box->getChildren(), timeScale, foundMdhd);
		}
	}
	return ret;
}

/**
 * @brief Get TimeScale value of buffer
 *
 * @param[out] timeScale - TimeScale value
 * @return true if parse was successful. false otherwise
 */
bool IsoBmffBuffer::getTimeScale(uint32_t &timeScale)
{
	bool foundMdhd = false;
	return getTimeScaleInternal(&boxes, timeScale, foundMdhd);
}

/**
 * @brief Print ISOBMFF boxes
 *
 * @param[in] boxes - ISOBMFF boxes
 * @return void
 */
void IsoBmffBuffer::printBoxesInternal(const std::vector<Box*> *boxes)
{
	for (size_t i = 0; i < boxes->size(); i++)
	{
		Box *box = boxes->at(i);
		AAMPLOG_WARN("Offset[%u] Type[%s] Size[%u]", box->getOffset(), box->getType(), box->getSize());
		if (IS_TYPE(box->getType(), Box::TFDT))
		{
			AAMPLOG_WARN("****Base Media Decode Time: %lld", dynamic_cast<TfdtBox *>(box)->getBaseMDT());
		}
		else if (IS_TYPE(box->getType(), Box::MVHD))
		{
			AAMPLOG_WARN("**** TimeScale from MVHD: %u", dynamic_cast<MvhdBox *>(box)->getTimeScale());
		}
		else if (IS_TYPE(box->getType(), Box::MDHD))
		{
			AAMPLOG_WARN("**** TimeScale from MDHD: %u", dynamic_cast<MdhdBox *>(box)->getTimeScale());
		}

		if (box->hasChildren())
		{
			printBoxesInternal(box->getChildren());
		}
	}
}



/**
 * @brief Print ISOBMFF boxes
 *
 * @return void
 */
void IsoBmffBuffer::printBoxes()
{
	printBoxesInternal(&boxes);
}
 
/**
 * @brief Check if buffer is an initialization segment
 *
 * @return true if buffer is an initialization segment. false otherwise
 */
bool IsoBmffBuffer::isInitSegment()
{
	bool foundFtypBox = false;
	for (size_t i = 0; i < boxes.size(); i++)
	{
		Box *box = boxes.at(i);
		if (IS_TYPE(box->getType(), Box::FTYP))
		{
			foundFtypBox = true;
			break;
		}
	}
	return foundFtypBox;
}

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
bool IsoBmffBuffer::getEMSGInfoInternal(const std::vector<Box*> *boxes, uint8_t* &message, uint32_t &messageLen, uint8_t* &schemeIdUri, uint8_t* &value, uint64_t &presTime, uint32_t &timeScale, uint32_t &eventDuration, uint32_t &id, bool &foundEmsg)
{
	bool ret = false;
	for (size_t i = 0; (false == foundEmsg) && i < boxes->size(); i++)
	{
		Box *box = boxes->at(i);
		if (IS_TYPE(box->getType(), Box::EMSG))
		{
			message = dynamic_cast<EmsgBox *>(box)->getMessage();
			messageLen = dynamic_cast<EmsgBox *>(box)->getMessageLen();
			presTime = dynamic_cast<EmsgBox *>(box)->getPresentationTime();
			timeScale = dynamic_cast<EmsgBox *>(box)->getTimeScale();
			eventDuration = dynamic_cast<EmsgBox *>(box)->getEventDuration();
			id = dynamic_cast<EmsgBox *>(box)->getId();
			schemeIdUri = dynamic_cast<EmsgBox *>(box)->getSchemeIdUri();
			value = dynamic_cast<EmsgBox *>(box)->getValue();
			ret = true;
			foundEmsg = true;
		}
	}
	return ret;
}


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
bool IsoBmffBuffer::getEMSGData(uint8_t* &message, uint32_t &messageLen, uint8_t* &schemeIdUri, uint8_t* &value, uint64_t &presTime, uint32_t &timeScale, uint32_t &eventDuration, uint32_t &id)
{
	bool foundEmsg = false;
	return getEMSGInfoInternal(&boxes, message, messageLen, schemeIdUri, value, presTime, timeScale, eventDuration, id, foundEmsg);
}

/**
 * @brief get ISOBMFF box list of a type in a parsed buffer
 *
 * @param[in] boxes - ISOBMFF boxes
 * @param[in] const char * - box name to get
 * @param[out] size_t - size of mdat buffer
 * @return bool
 */
bool IsoBmffBuffer::getBoxesInternal(const std::vector<Box*> *boxes, const char *name, std::vector<Box*> *pBoxes)
{
    size_t size =boxes->size();
    //Adjust size when chunked box is available
    if(chunkedBox)
    {
        size -= 1;
    }
    for (size_t i = 0; i < size; i++)
    {
        Box *box = boxes->at(i);

        if (IS_TYPE(box->getType(), name))
        {
            pBoxes->push_back(box);
        }
    }
    return !!(pBoxes->size());
}

/**
 * @brief Check mdat buffer count in parsed buffer
 * @param[out] size_t - mdat box count
 * @return true if mdat count available. false otherwise
 */
bool IsoBmffBuffer::getMdatBoxCount(size_t &count)
{
    std::vector<Box*> mdatBoxes;
    bool bParse = false;
    bParse = getBoxesInternal(&boxes ,Box::MDAT, &mdatBoxes);
    count = mdatBoxes.size();
    return bParse;
}

/**
 * @brief Print ISOBMFF mdat boxes in parsed buffer
 *
 * @return void
 */
void IsoBmffBuffer::printMdatBoxes()
{
    std::vector<Box*> mdatBoxes;
    bool bParse = false;
    bParse = getBoxesInternal(&boxes ,Box::MDAT, &mdatBoxes);
    printBoxesInternal(&mdatBoxes);
}

/**
 * @brief Get list of box handle in parsed bufferr using name
 * @param[in] const char * - box name to get
 * @param[out] std::vector<Box*> - List of box handles of a type in a parsed buffer
 * @return true if Box found. false otherwise
 */
bool IsoBmffBuffer::getTypeOfBoxes(const char *name, std::vector<Box*> &stBoxes)
{
    bool bParse = false;
    bParse = getBoxesInternal(&boxes ,name, &stBoxes);
    return bParse;
}

/**
 * @brief Get list of box handles in a parsed buffer
 *
 * @return Box handle if Chunk box found in a parsed buffer. NULL otherwise
 */
Box* IsoBmffBuffer::getChunkedfBox() const
{
    return this->chunkedBox;
}

/**
 * @brief Get list of box handles in a parsed buffer
 *
 * @return Box handle list if Box found at index given. NULL otherwise
 */
std::vector<Box*> *IsoBmffBuffer::getParsedBoxes()
{
    return &this->boxes;
}

/**
 * @brief Get box handle in parsed bufferr using name
 * @param[in] const char * - box name to get
 * @param[out] size_t - index of box in a parsed buffer
 * @return Box handle if Box found at index given. NULL otherwise
 */
Box*  IsoBmffBuffer::getBox(const char *name, size_t &index)
{
    Box *pBox = NULL;
    index = -1;
    for (size_t i = 0; i < boxes.size(); i++)
    {
        pBox = boxes.at(i);
        if (IS_TYPE(pBox->getType(), name))
        {
            index = i;
            break;
        }
        pBox = NULL;
    }
    return pBox;
}

/**
 * @brief Get box handle in parsed bufferr using index
 * @param[out] size_t - index of box in a parsed buffer
 * @return Box handle if Box found at index given. NULL otherwise
 */
Box* IsoBmffBuffer::getBoxAtIndex(size_t index)
{
    if(index != -1)
        return boxes.at(index);
    else
        return NULL;
}

/**
 * @brief Print ISOBMFF box PTS
 *
 * @param[in] boxes - ISOBMFF boxes
 * @return void
 */
void IsoBmffBuffer::printPTSInternal(const std::vector<Box*> *boxes)
{
    for (size_t i = 0; i < boxes->size(); i++)
    {
        Box *box = boxes->at(i);

        if (IS_TYPE(box->getType(), Box::TFDT))
        {
            AAMPLOG_WARN("****Base Media Decode Time: %lld", dynamic_cast<TfdtBox *>(box)->getBaseMDT());
        }

        if (box->hasChildren())
        {
            printBoxesInternal(box->getChildren());
        }
    }
}

/**
 * @brief Get ISOBMFF box Sample Duration
 *
 * @param[in] boxes - ISOBMFF boxes
 * @return uint64_t - duration  value
 */
uint64_t IsoBmffBuffer::getSampleDurationInernal(const std::vector<Box*> *boxes)
{
    if(!boxes) return 0;

    for (size_t i = boxes->size()-1; i >= 0; i--)
    {
        Box *box = boxes->at(i);
        uint64_t duration = 0;
        if (IS_TYPE(box->getType(), Box::TRUN))
        {
            //AAMPLOG_WARN("****TRUN BOX SIZE: %d \n", box->getSize());
            duration = dynamic_cast<TrunBox *>(box)->getSampleDuration();
            //AAMPLOG_WARN("****DURATION: %lld \n", duration);
            if(duration) return duration;
        }
        else if (IS_TYPE(box->getType(), Box::TFHD))
        {
            //AAMPLOG_WARN("****TFHD BOX SIZE: %d \n", box->getSize());
            duration = dynamic_cast<TfhdBox *>(box)->getSampleDuration();
            //AAMPLOG_WARN("****DURATION: %lld \n", duration);
            if(duration) return duration;
        }

        if (IS_TYPE(box->getType(), Box::TRAF) && box->hasChildren())
        {
            return getSampleDurationInernal(box->getChildren());
        }
    }
}

/**
 * @brief Get ISOBMFF box Sample Duration
 *
 * @param[in] box - ISOBMFF box
 * @param[in] uint64_t* -  duration to get
 * @return void
 */
void IsoBmffBuffer::getSampleDuration(Box *box, uint64_t &fduration)
{
    if (box->hasChildren())
    {
        fduration = getSampleDurationInernal(box->getChildren());
    }
}

/**
 * @brief Get ISOBMFF box PTS
 *
 * @param[in] boxes - ISOBMFF boxes
 * @return uint64_t - PTS value
 */
uint64_t IsoBmffBuffer::getPtsInternal(const std::vector<Box*> *boxes)
{
    for (size_t i = 0; i < boxes->size(); i++)
    {
        Box *box = boxes->at(i);

        if (IS_TYPE(box->getType(), Box::TFDT))
        {
            AAMPLOG_WARN("****Base Media Decode Time: %lld", dynamic_cast<TfdtBox *>(box)->getBaseMDT());
            return dynamic_cast<TfdtBox *>(box)->getBaseMDT();
        }

        if (box->hasChildren())
        {
            return getPtsInternal(box->getChildren());
        }
    }
}

/**
 * @brief Get ISOBMFF box PTS
 *
 * @param[in] boxe - ISOBMFF box
 * @param[in] uint64_t* -  PTS to get
 * @return void
 */
void IsoBmffBuffer::getPts(Box *box, uint64_t &fpts)
{
    if (box->hasChildren())
    {
        fpts = getPtsInternal(box->getChildren());
    }
}

