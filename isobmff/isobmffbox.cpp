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
* @file isobmffbox.cpp
* @brief Source file for ISO Base Media File Format Boxes
*/

#include "isobmffbox.h"
#include "AampConfig.h"
#include <stddef.h>
#include <inttypes.h>

/**
 * @brief Read a string from buffer and return it
 *
 * @param[in] buffer Buffer to read
 * @param[in] lenth String length
 * @param[in] Length of string
 */
uint32_t ReadCStringLen(const uint8_t* buffer, uint32_t bufferLen)
{
	int retLen = -1;
	if(buffer && bufferLen > 0)
	{
		for(int i=0; i < bufferLen; i++)
		{
			if(buffer[i] == '\0')
			{
				retLen = i+1;
				break;
			}
		}
	}
	return retLen;
}

/**
 * @brief Utility function to read 8 bytes from a buffer
 *
 * @param[in] buf - buffer pointer
 * @return bytes read from buffer
 */
uint64_t ReadUint64(uint8_t *buf)
{
	uint64_t val = READ_U32(buf);
	val = (val<<32) | (uint32_t)READ_U32(buf);
	return val;
}

/**
 * @brief Utility function to write 8 bytes to a buffer
 *
 * @param[in] dst - buffer pointer
 * @param[in] value - value to write
 * @return void
 */
void WriteUint64(uint8_t *dst, uint64_t val)
{
	uint32_t msw = (uint32_t)(val>>32);
	WRITE_U32(dst, msw); dst+=4;
	WRITE_U32(dst, val);
}

/**
 * @brief Box constructor
 *
 * @param[in] sz - box size
 * @param[in] btype - box type
 */
Box::Box(uint32_t sz, const char btype[4]) : offset(0), size(sz), type{}
{
	memcpy(type,btype,4);
}

/**
 * @brief Set box's offset from the beginning of the buffer
 *
 * @param[in] os - offset
 * @return void
 */
void Box::setOffset(uint32_t os)
{
	offset = os;
}

/**
 * @brief Get box offset
 *
 * @return offset of box
 */
uint32_t Box::getOffset()
{
	return offset;
}

/**
 * @brief To check if box has any child boxes
 *
 * @return true if this box has other boxes as children
 */
bool Box::hasChildren()
{
	return false;
}

/**
 * @brief Get children of this box
 *
 * @return array of child boxes
 */
const std::vector<Box*> *Box::getChildren()
{
	return NULL;
}

/**
 * @brief Get box size
 *
 * @return box size
 */
uint32_t Box::getSize()
{
	return size;
}

/**
 * @brief Get box type
 *
 * @return box type
 */
const char *Box::getType()
{
	return type;
}

/**
 * @brief Static function to construct a Box object
 *
 * @param[in] hdr - pointer to box
 * @param[in] maxSz - box size
 * @return newly constructed Box object
 */
Box* Box::constructBox(uint8_t *hdr, uint32_t maxSz, bool correctBoxSize)
{
	L_RESTART:
	uint8_t *hdr_start = hdr;
	uint32_t size = READ_U32(hdr);
	uint8_t type[5];
	READ_U8(type, hdr, 4);
	type[4] = '\0';

	if (size > maxSz)
	{
		if(correctBoxSize)
		{
			//Fix box size to handle cases like receiving whole file for HTTP range requests 
			AAMPLOG_WARN("%s:%d: Box[%s] fixing size error:size[%u] > maxSz[%u]\n",__FUNCTION__, __LINE__, type, size, maxSz);
			hdr = hdr_start;
			WRITE_U32(hdr,maxSz);
			goto L_RESTART;
		}
		else
		{
			AAMPLOG_WARN("Box[%s] Size error:size[%u] > maxSz[%u]\n",type, size, maxSz);
		}
	}
	else if (IS_TYPE(type, MOOV))
	{
		return GenericContainerBox::constructContainer(size, MOOV, hdr);
	}
	else if (IS_TYPE(type, TRAK))
	{
		return GenericContainerBox::constructContainer(size, TRAK, hdr);
	}
	else if (IS_TYPE(type, MDIA))
	{
		return GenericContainerBox::constructContainer(size, MDIA, hdr);
	}
	else if (IS_TYPE(type, MOOF))
	{
		return GenericContainerBox::constructContainer(size, MOOF, hdr);
	}
	else if (IS_TYPE(type, TRAF))
	{
		return GenericContainerBox::constructContainer(size, TRAF, hdr);
	}
	else if (IS_TYPE(type, TFDT))
	{
		return TfdtBox::constructTfdtBox(size,  hdr);
	}
	else if (IS_TYPE(type, MVHD))
	{
		return MvhdBox::constructMvhdBox(size,  hdr);
	}
	else if (IS_TYPE(type, MDHD))
	{
		return MdhdBox::constructMdhdBox(size,  hdr);
	}
	else if (IS_TYPE(type, EMSG))
	{
		return EmsgBox::constructEmsgBox(size, hdr);
	}

	return new Box(size, (const char *)type);
}

/**
 * @brief GenericContainerBox constructor
 *
 * @param[in] sz - box size
 * @param[in] btype - box type
 */
GenericContainerBox::GenericContainerBox(uint32_t sz, const char btype[4]) : Box(sz, btype), children()
{

}

/**
 * @brief GenericContainerBox destructor
 */
GenericContainerBox::~GenericContainerBox()
{
	for (size_t i = 0; i < children.size(); i++)
	{
		delete children.at(i);
	}
	children.clear();
}

/**
 * @brief Add a box as a child box
 *
 * @param[in] box - child box object
 * @return void
 */
void GenericContainerBox::addChildren(Box *box)
{
	children.push_back(box);
}

/**
 * @brief To check if box has any child boxes
 *
 * @return true if this box has other boxes as children
 */
bool GenericContainerBox::hasChildren()
{
	return true;
}

/**
 * @brief Get children of this box
 *
 * @return array of child boxes
 */
const std::vector<Box*> *GenericContainerBox::getChildren()
{
	return &children;
}

/**
 * @brief Static function to construct a GenericContainerBox object
 *
 * @param[in] sz - box size
 * @param[in] btype - box type
 * @param[in] ptr - pointer to box
 * @return newly constructed GenericContainerBox object
 */
GenericContainerBox* GenericContainerBox::constructContainer(uint32_t sz, const char btype[4], uint8_t *ptr)
{
	GenericContainerBox *cbox = new GenericContainerBox(sz, btype);
	uint32_t curOffset = sizeof(uint32_t) + sizeof(uint32_t); //Sizes of size & type fields
	while (curOffset < sz)
	{
		Box *box = Box::constructBox(ptr, sz-curOffset);
		box->setOffset(curOffset);
		cbox->addChildren(box);
		curOffset += box->getSize();
		ptr += box->getSize();
	}
	return cbox;
}

/**
 * @brief FullBox constructor
 *
 * @param[in] sz - box size
 * @param[in] btype - box type
 * @param[in] ver - version value
 * @param[in] f - flag value
 */
FullBox::FullBox(uint32_t sz, const char btype[4], uint8_t ver, uint32_t f) : Box(sz, btype), version(ver), flags(f)
{

}

/**
 * @brief MvhdBox constructor
 *
 * @param[in] sz - box size
 * @param[in] tScale - TimeScale value
 */
MvhdBox::MvhdBox(uint32_t sz, uint32_t tScale) : FullBox(sz, Box::MVHD, 0, 0), timeScale(tScale)
{

}

/**
 * @brief MvhdBox constructor
 *
 * @param[in] fbox - box object
 * @param[in] tScale - TimeScale value
 */
MvhdBox::MvhdBox(FullBox &fbox, uint32_t tScale) : FullBox(fbox), timeScale(tScale)
{

}

/**
 * @brief Set TimeScale value
 *
 * @param[in] tScale - TimeScale value
 * @return void
 */
void MvhdBox::setTimeScale(uint32_t tScale)
{
	timeScale = tScale;
}

/**
 * @brief Get TimeScale value
 *
 * @return TimeScale value
 */
uint32_t MvhdBox::getTimeScale()
{
	return timeScale;
}

/**
 * @brief Static function to construct a MvhdBox object
 *
 * @param[in] sz - box size
 * @param[in] ptr - pointer to box
 * @return newly constructed MvhdBox object
 */
MvhdBox* MvhdBox::constructMvhdBox(uint32_t sz, uint8_t *ptr)
{
	uint8_t version = READ_VERSION(ptr);
	uint32_t flags  = READ_FLAGS(ptr);
	uint64_t tScale;

	uint32_t skip = sizeof(uint32_t)*2;
	if (1 == version)
	{
		//Skipping creation_time &modification_time
		skip = sizeof(uint64_t)*2;
	}
	ptr += skip;

	tScale = READ_U32(ptr);

	FullBox fbox(sz, Box::MVHD, version, flags);
	return new MvhdBox(fbox, tScale);
}

/**
 * @brief MdhdBox constructor
 *
 * @param[in] sz - box size
 * @param[in] tScale - TimeScale value
 */
MdhdBox::MdhdBox(uint32_t sz, uint32_t tScale) : FullBox(sz, Box::MDHD, 0, 0), timeScale(tScale)
{

}

/**
 * @brief MdhdBox constructor
 *
 * @param[in] fbox - box object
 * @param[in] tScale - TimeScale value
 */
MdhdBox::MdhdBox(FullBox &fbox, uint32_t tScale) : FullBox(fbox), timeScale(tScale)
{

}

/**
 * @brief Set TimeScale value
 *
 * @param[in] tScale - TimeScale value
 * @return void
 */
void MdhdBox::setTimeScale(uint32_t tScale)
{
	timeScale = tScale;
}

/**
 * @brief Get TimeScale value
 *
 * @return TimeScale value
 */
uint32_t MdhdBox::getTimeScale()
{
	return timeScale;
}

/**
 * @brief Static function to construct a MdhdBox object
 *
 * @param[in] sz - box size
 * @param[in] ptr - pointer to box
 * @return newly constructed MdhdBox object
 */
MdhdBox* MdhdBox::constructMdhdBox(uint32_t sz, uint8_t *ptr)
{
	uint8_t version = READ_VERSION(ptr);
	uint32_t flags  = READ_FLAGS(ptr);
	uint64_t tScale;

	uint32_t skip = sizeof(uint32_t)*2;
	if (1 == version)
	{
		//Skipping creation_time &modification_time
		skip = sizeof(uint64_t)*2;
	}
	ptr += skip;

	tScale = READ_U32(ptr);

	FullBox fbox(sz, Box::MDHD, version, flags);
	return new MdhdBox(fbox, tScale);
}

/**
 * @brief TfdtBox constructor
 *
 * @param[in] sz - box size
 * @param[in] mdt - BaseMediaDecodeTime value
 */
TfdtBox::TfdtBox(uint32_t sz, uint64_t mdt) : FullBox(sz, Box::TFDT, 0, 0), baseMDT(mdt)
{

}

/**
 * @brief TfdtBox constructor
 *
 * @param[in] fbox - box object
 * @param[in] mdt - BaseMediaDecodeTime value
 */
TfdtBox::TfdtBox(FullBox &fbox, uint64_t mdt) : FullBox(fbox), baseMDT(mdt)
{

}

/**
 * @brief Set BaseMediaDecodeTime value
 *
 * @param[in] mdt - BaseMediaDecodeTime value
 * @return void
 */
void TfdtBox::setBaseMDT(uint64_t mdt)
{
	baseMDT = mdt;
}

/**
 * @brief Get BaseMediaDecodeTime value
 *
 * @return BaseMediaDecodeTime value
 */
uint64_t TfdtBox::getBaseMDT()
{
	return baseMDT;
}

/**
 * @brief Static function to construct a TfdtBox object
 *
 * @param[in] sz - box size
 * @param[in] ptr - pointer to box
 * @return newly constructed TfdtBox object
 */
TfdtBox* TfdtBox::constructTfdtBox(uint32_t sz, uint8_t *ptr)
{
	uint8_t version = READ_VERSION(ptr);
	uint32_t flags  = READ_FLAGS(ptr);
	uint64_t mdt;

	if (1 == version)
	{
		mdt = READ_BMDT64(ptr);
	}
	else
	{
		mdt = (uint32_t)READ_U32(ptr);
	}
	FullBox fbox(sz, Box::TFDT, version, flags);
	return new TfdtBox(fbox, mdt);
}


/**
 * @brief EmsgBox constructor
 *
 * @param[in] sz - box size
 * @param[in] tScale - TimeScale value
 */
EmsgBox::EmsgBox(uint32_t sz, uint32_t tScale, uint32_t evtDur, uint32_t _id) : FullBox(sz, Box::EMSG, 0, 0)
	, timeScale(tScale), eventDuration(evtDur), id(_id)
	, presentationTimeDelta(0), presentationTime(0)
	, schemeIdUri(nullptr), value(nullptr), messageData(nullptr), messageLen(0)
{

}

/**
 * @brief EmsgBox constructor
 *
 * @param[in] fbox - box object
 * @param[in] tScale - TimeScale value
 * @param[in] evtDur - eventDuration value
 * @param[in] _id - id value
 */
EmsgBox::EmsgBox(FullBox &fbox, uint32_t tScale, uint32_t evtDur, uint32_t _id, uint64_t presTime, uint32_t presTimeDelta) : FullBox(fbox)
	, timeScale(tScale), eventDuration(evtDur), id(_id)
	, presentationTimeDelta(presTimeDelta), presentationTime(presTime)
	, schemeIdUri(nullptr), value(nullptr), messageData(nullptr), messageLen(0)
{

}

/**
 * @brief EmsgBox dtor
 */
EmsgBox::~EmsgBox()
{
	if (messageData)
	{
		free(messageData);
	}

	if (schemeIdUri)
	{
		free(schemeIdUri);
	}

	if (value)
	{
		free(value);
	}
}

/**
 * @brief Set TimeScale value
 *
 * @param[in] tScale - TimeScale value
 * @return void
 */
void EmsgBox::setTimeScale(uint32_t tScale)
{
	timeScale = tScale;
}

/**
 * @brief Get TimeScale value
 *
 * @return TimeScale value
 */
uint32_t EmsgBox::getTimeScale()
{
	return timeScale;
}

/**
 * @brief Set eventDuration value
 *
 * @param[in] evtDur - eventDuration value
 * @return void
 */
void EmsgBox::setEventDuration(uint32_t evtDur)
{
	eventDuration = evtDur;
}

/**
 * @brief Get eventDuration
 *
 * @return eventDuration value
 */
uint32_t EmsgBox::getEventDuration()
{
	return eventDuration;
}

/**
 * @brief Set id
 *
 * @param[in] _id - id
 * @return void
 */
void EmsgBox::setId(uint32_t _id)
{
	id = _id;
}

/**
 * @brief Get id
 *
 * @return id value
 */
uint32_t EmsgBox::getId()
{
	return id;
}

/**
 * @brief Set presentationTimeDelta
 *
 * @param[in] presTimeDelta - presTimeDelta
 * @return void
 */
void EmsgBox::setPresentationTimeDelta(uint32_t presTimeDelta)
{
	presentationTimeDelta = presTimeDelta;
}

/**
 * @brief Get presentationTimeDelta
 *
 * @return presentationTimeDelta value
 */
uint32_t EmsgBox::getPresentationTimeDelta()
{
	return presentationTimeDelta;
}

/**
 * @brief Set presentationTime
 *
 * @param[in] presTime - presTime
 * @return void
 */
void EmsgBox::setPresentationTime(uint64_t presTime)
{
	presentationTime = presTime;
}

/**
 * @brief Get presentationTime
 *
 * @return presentationTime value
 */
uint64_t EmsgBox::getPresentationTime()
{
	return presentationTime;
}

/**
 * @brief Set schemeIdUri
 *
 * @param[in] schemeIdUri - schemeIdUri pointer
 * @return void
 */
void EmsgBox::setSchemeIdUri(uint8_t* schemeIdURI)
{
	schemeIdUri = schemeIdURI;
}

/**
 * @brief Get schemeIdUri
 *
 * @return schemeIdUri value
 */
uint8_t* EmsgBox::getSchemeIdUri()
{
	return schemeIdUri;
}

/**
 * @brief Set value
 *
 * @param[in] value - value pointer
 * @return void
 */
void EmsgBox::setValue(uint8_t* schemeIdValue)
{
	value = schemeIdValue;
}

/**
 * @brief Get value
 *
 * @return schemeIdUri value
 */
uint8_t* EmsgBox::getValue()
{
	return value;
}

/**
 * @brief Set Message
 *
 * @param[in] value - Message pointer
 * @return void
 */
void EmsgBox::setMessage(uint8_t* message, uint32_t len)
{
	messageData = message;
	messageLen = len;
}

/**
 * @brief Get Message
 *
 * @return messageData
 */
uint8_t* EmsgBox::getMessage()
{
	return messageData;
}

/**
 * @brief Get Message length
 *
 * @return messageLen
 */
uint32_t EmsgBox::getMessageLen()
{
	return messageLen;
}

/**
 * @brief Static function to construct a EmsgBox object
 *
 * @param[in] sz - box size
 * @param[in] ptr - pointer to box
 * @return newly constructed EmsgBox object
 */
EmsgBox* EmsgBox::constructEmsgBox(uint32_t sz, uint8_t *ptr)
{
	uint8_t version = READ_VERSION(ptr);
	uint32_t flags  = READ_FLAGS(ptr);
	// Calculationg remaining size,
	// flags(3bytes)+ version(1byte)+ box_header(type and size)(8bytes)
	uint32_t remainingSize = sz - ((sizeof(uint32_t))+(sizeof(uint64_t)));

	uint64_t presTime = 0;
	uint32_t presTimeDelta = 0;
	uint32_t tScale;
	uint32_t evtDur;
	uint32_t boxId;

	uint8_t* schemeId = nullptr;
	uint8_t* schemeIdValue = nullptr;

	uint8_t* message = nullptr;
	FullBox fbox(sz, Box::EMSG, version, flags);

	/*
	 * Extraction is done as per https://aomediacodec.github.io/id3-emsg/
	 */
	if (1 == version)
	{
		tScale = READ_U32(ptr);
		// Read 64 bit value
		presTime = READ_U32(ptr);
		presTime <<= 32;
		presTime |= READ_U32(ptr);
		evtDur = READ_U32(ptr);
		boxId = READ_U32(ptr);
		remainingSize -=  ((sizeof(uint32_t)*3) + sizeof(uint64_t));
		uint32_t schemeIdLen = ReadCStringLen(ptr, remainingSize);
		if(schemeIdLen > 0)
		{
			schemeId = (uint8_t*) malloc(sizeof(uint8_t)*schemeIdLen);
			READ_U8(schemeId, ptr, schemeIdLen);
			remainingSize -= (sizeof(uint8_t) * schemeIdLen);
			uint32_t schemeIdValueLen = ReadCStringLen(ptr, remainingSize);
			if (schemeIdValueLen > 0)
			{
				schemeIdValue = (uint8_t*) malloc(sizeof(uint8_t)*schemeIdValueLen);
				READ_U8(schemeIdValue, ptr, schemeIdValueLen);
				remainingSize -= (sizeof(uint8_t) * schemeIdValueLen);
			}
		}
	}
	else if(0 == version)
	{
		uint32_t schemeIdLen = ReadCStringLen(ptr, remainingSize);
		if(schemeIdLen)
		{
			schemeId = (uint8_t*) malloc(sizeof(uint8_t)*schemeIdLen);
			READ_U8(schemeId, ptr, schemeIdLen);
			remainingSize -= (sizeof(uint8_t) * schemeIdLen);
			uint32_t schemeIdValueLen = ReadCStringLen(ptr, remainingSize);
			if (schemeIdValueLen)
			{
				schemeIdValue = (uint8_t*) malloc(sizeof(uint8_t)*schemeIdValueLen);
				READ_U8(schemeIdValue, ptr, schemeIdValueLen);
				remainingSize -= (sizeof(uint8_t) * schemeIdValueLen);
			}
		}
		tScale = READ_U32(ptr);
		presTimeDelta = READ_U32(ptr);
		evtDur = READ_U32(ptr);
		boxId = READ_U32(ptr);
		remainingSize -= (sizeof(uint32_t)*4);
	}
	else
	{
		AAMPLOG_WARN("%s():%d Unsupported emsg box version", __FUNCTION__, __LINE__);
		return new EmsgBox(fbox, 0, 0, 0, 0, 0);
	}

	EmsgBox* retBox = new EmsgBox(fbox, tScale, evtDur, boxId, presTime, presTimeDelta);
	if(retBox)
	{
		// Extract remaining message
		if(remainingSize > 0)
		{
			message = (uint8_t*) malloc(sizeof(uint8_t)*remainingSize);
			READ_U8(message, ptr, remainingSize);
			if(message)
			{
				retBox->setMessage(message, remainingSize);
			}
		}

		// Save schemeIdUri and value if present
		if (schemeId)
		{
			retBox->setSchemeIdUri(schemeId);
			if(schemeIdValue)
			{
				retBox->setValue(schemeIdValue);
			}
		}
	}
	return retBox;
}
