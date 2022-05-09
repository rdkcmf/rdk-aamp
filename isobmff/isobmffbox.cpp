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
#include "AampUtils.h"
#include <stddef.h>
#include <inttypes.h>

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


uint64_t ReadUint64(uint8_t *buf)
{
	uint64_t val = READ_U32(buf);
	val = (val<<32) | (uint32_t)READ_U32(buf);
	return val;
}


void WriteUint64(uint8_t *dst, uint64_t val)
{
	uint32_t msw = (uint32_t)(val>>32);
	WRITE_U32(dst, msw); dst+=4;
	WRITE_U32(dst, val);
}


Box::Box(uint32_t sz, const char btype[4]) : offset(0), size(sz), type{}
{
	memcpy(type,btype,4);
}


void Box::setOffset(uint32_t os)
{
	offset = os;
}


uint32_t Box::getOffset() const
{
	return offset;
}


bool Box::hasChildren()
{
	return false;
}


const std::vector<Box*> *Box::getChildren()
{
	return NULL;
}


uint32_t Box::getSize() const
{
	return size;
}


const char *Box::getType()
{
	return type;
}


const char* Box::getBoxType() const
{
    if ((!IS_TYPE(type, MOOV)) ||
        (!IS_TYPE(type, MDIA)) ||
        (!IS_TYPE(type, MOOF)) ||
        (!IS_TYPE(type, TRAF)) ||
        (!IS_TYPE(type, TFDT)) ||
        (!IS_TYPE(type, MVHD)) ||
        (!IS_TYPE(type, MDHD)) ||
        (!IS_TYPE(type, TFDT)) ||
        (!IS_TYPE(type, FTYP)) ||
        (!IS_TYPE(type, STYP)) ||
        (!IS_TYPE(type, SIDX)) ||
        (!IS_TYPE(type, PRFT)) ||
        (!IS_TYPE(type, MDAT)))
    {
        return "UKWN";
    }
    return type;
}


Box* Box::constructBox(uint8_t *hdr, uint32_t maxSz, AampLogManager *mLogObj, bool correctBoxSize)
{
	L_RESTART:
	uint8_t *hdr_start = hdr;
	uint32_t size = 0;
	uint8_t type[5];
	if(maxSz < 4)
	{
		AAMPLOG_WARN("Box data < 4 bytes. Can't determine Size & Type");
        	return new Box(maxSz, (const char *)"UKWN");
    	}
    	else if(maxSz >= 4 && maxSz < 8)
    	{
		AAMPLOG_WARN("Box Size between >4 but <8 bytes. Can't determine Type");
		//size = READ_U32(hdr);
		return new Box(maxSz, (const char *)"UKWN");
    	}
    	else
    	{
        	size = READ_U32(hdr);
        	READ_U8(type, hdr, 4);
        	type[4] = '\0';
    	}

	if (size > maxSz)
	{
                if(correctBoxSize)
                {
                        //Fix box size to handle cases like receiving whole file for HTTP range requests
			AAMPLOG_WARN("Box[%s] fixing size error:size[%u] > maxSz[%u]", type, size, maxSz);
                        hdr = hdr_start;
                        WRITE_U32(hdr,maxSz);
                        goto L_RESTART;
                }
                else
                {
#ifdef AAMP_DEBUG_BOX_CONSTRUCT
                    AAMPLOG_WARN("Box[%s] Size error:size[%u] > maxSz[%u]",type, size, maxSz);
#endif
                }
	}
	else if (IS_TYPE(type, MOOV))
	{
		return GenericContainerBox::constructContainer(size, MOOV, hdr);
	}
	else if (IS_TYPE(type, TRAK))
	{		
		return TrakBox::constructTrakBox(size,hdr);
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
	else if (IS_TYPE(type, TFHD))
	{
		return TfhdBox::constructTfhdBox(size,  hdr);
	}
	else if (IS_TYPE(type, TFDT))
	{
		return TfdtBox::constructTfdtBox(size,  hdr);
	}
	else if (IS_TYPE(type, TRUN))
	{
		return TrunBox::constructTrunBox(size,  hdr);
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
	else if (IS_TYPE(type, PRFT))
	{
		return PrftBox::constructPrftBox(size,  hdr);
	}

	return new Box(size, (const char *)type);
}


GenericContainerBox::GenericContainerBox(uint32_t sz, const char btype[4]) : Box(sz, btype), children()
{

}


GenericContainerBox::~GenericContainerBox()
{
	for (unsigned int i = children.size(); i>0;)
	{
		--i;
		SAFE_DELETE(children.at(i));
		children.pop_back();
	}
	children.clear();
}


void GenericContainerBox::addChildren(Box *box)
{
	children.push_back(box);
}


bool GenericContainerBox::hasChildren()
{
	return true;
}


const std::vector<Box*> *GenericContainerBox::getChildren()
{
	return &children;
}


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


FullBox::FullBox(uint32_t sz, const char btype[4], uint8_t ver, uint32_t f) : Box(sz, btype), version(ver), flags(f)
{

}


MvhdBox::MvhdBox(uint32_t sz, uint32_t tScale) : FullBox(sz, Box::MVHD, 0, 0), timeScale(tScale)
{

}


MvhdBox::MvhdBox(FullBox &fbox, uint32_t tScale) : FullBox(fbox), timeScale(tScale)
{

}


void MvhdBox::setTimeScale(uint32_t tScale)
{
	timeScale = tScale;
}


uint32_t MvhdBox::getTimeScale()
{
	return timeScale;
}


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


MdhdBox::MdhdBox(uint32_t sz, uint32_t tScale) : FullBox(sz, Box::MDHD, 0, 0), timeScale(tScale)
{

}


MdhdBox::MdhdBox(FullBox &fbox, uint32_t tScale) : FullBox(fbox), timeScale(tScale)
{

}


void MdhdBox::setTimeScale(uint32_t tScale)
{
	timeScale = tScale;
}


uint32_t MdhdBox::getTimeScale()
{
	return timeScale;
}


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


TfdtBox::TfdtBox(uint32_t sz, uint64_t mdt) : FullBox(sz, Box::TFDT, 0, 0), baseMDT(mdt)
{

}


TfdtBox::TfdtBox(FullBox &fbox, uint64_t mdt) : FullBox(fbox), baseMDT(mdt)
{

}


void TfdtBox::setBaseMDT(uint64_t mdt)
{
	baseMDT = mdt;
}


uint64_t TfdtBox::getBaseMDT()
{
	return baseMDT;
}


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


EmsgBox::EmsgBox(uint32_t sz, uint32_t tScale, uint32_t evtDur, uint32_t _id) : FullBox(sz, Box::EMSG, 0, 0)
	, timeScale(tScale), eventDuration(evtDur), id(_id)
	, presentationTimeDelta(0), presentationTime(0)
	, schemeIdUri(nullptr), value(nullptr), messageData(nullptr), messageLen(0)
{

}


EmsgBox::EmsgBox(FullBox &fbox, uint32_t tScale, uint32_t evtDur, uint32_t _id, uint64_t presTime, uint32_t presTimeDelta) : FullBox(fbox)
	, timeScale(tScale), eventDuration(evtDur), id(_id)
	, presentationTimeDelta(presTimeDelta), presentationTime(presTime)
	, schemeIdUri(nullptr), value(nullptr), messageData(nullptr), messageLen(0)
{

}


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


void EmsgBox::setTimeScale(uint32_t tScale)
{
	timeScale = tScale;
}


uint32_t EmsgBox::getTimeScale()
{
	return timeScale;
}


void EmsgBox::setEventDuration(uint32_t evtDur)
{
	eventDuration = evtDur;
}


uint32_t EmsgBox::getEventDuration()
{
	return eventDuration;
}


void EmsgBox::setId(uint32_t _id)
{
	id = _id;
}


uint32_t EmsgBox::getId()
{
	return id;
}


void EmsgBox::setPresentationTimeDelta(uint32_t presTimeDelta)
{
	presentationTimeDelta = presTimeDelta;
}


uint32_t EmsgBox::getPresentationTimeDelta()
{
	return presentationTimeDelta;
}


void EmsgBox::setPresentationTime(uint64_t presTime)
{
	presentationTime = presTime;
}


uint64_t EmsgBox::getPresentationTime()
{
	return presentationTime;
}


void EmsgBox::setSchemeIdUri(uint8_t* schemeIdURI)
{
	schemeIdUri = schemeIdURI;
}


uint8_t* EmsgBox::getSchemeIdUri()
{
	return schemeIdUri;
}


void EmsgBox::setValue(uint8_t* schemeIdValue)
{
	value = schemeIdValue;
}


uint8_t* EmsgBox::getValue()
{
	return value;
}


void EmsgBox::setMessage(uint8_t* message, uint32_t len)
{
	messageData = message;
	messageLen = len;
}


uint8_t* EmsgBox::getMessage()
{
	return messageData;
}


uint32_t EmsgBox::getMessageLen()
{
	return messageLen;
}


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
		presTime = READ_64(ptr);
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
		AAMPLOG_WARN("Unsupported emsg box version");
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


TrunBox::TrunBox(uint32_t sz, uint64_t sampleDuration) : FullBox(sz, Box::TRUN, 0, 0), duration(sampleDuration)
{
}


TrunBox::TrunBox(FullBox &fbox, uint64_t sampleDuration) : FullBox(fbox), duration(sampleDuration)
{
}


void TrunBox::setSampleDuration(uint64_t sampleDuration)
{
    duration = sampleDuration;
}


uint64_t TrunBox::getSampleDuration()
{
    return duration;
}


TrunBox* TrunBox::constructTrunBox(uint32_t sz, uint8_t *ptr)
{
	const uint32_t TRUN_FLAG_DATA_OFFSET_PRESENT                    = 0x0001;
	const uint32_t TRUN_FLAG_FIRST_SAMPLE_FLAGS_PRESENT             = 0x0004;
	const uint32_t TRUN_FLAG_SAMPLE_DURATION_PRESENT                = 0x0100;
	const uint32_t TRUN_FLAG_SAMPLE_SIZE_PRESENT                    = 0x0200;
	const uint32_t TRUN_FLAG_SAMPLE_FLAGS_PRESENT                   = 0x0400;
	const uint32_t TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT = 0x0800;

	uint8_t version = READ_VERSION(ptr);
	uint32_t flags  = READ_FLAGS(ptr);
	uint64_t sampleDuration = 0;//1001000; //fix-Me
	uint32_t sample_count = 0;
	uint32_t sample_duration = 0;
	uint32_t sample_size = 0;
	uint32_t sample_flags = 0;
	uint32_t sample_composition_time_offset = 0;
	uint32_t discard;
	uint32_t totalSampleDuration = 0;

	uint32_t record_fields_count = 0;

	// count the number of bits set to 1 in the second byte of the flags
	for (unsigned int i=0; i<8; i++)
	{
		if (flags & (1<<(i+8))) ++record_fields_count;
	}

	sample_count = READ_U32(ptr);

	discard = READ_U32(ptr);

	for (unsigned int i=0; i<sample_count; i++)
	{
		if (flags & TRUN_FLAG_SAMPLE_DURATION_PRESENT)
		{
			sample_duration = READ_U32(ptr);
			totalSampleDuration += sample_duration;
		}
		if (flags & TRUN_FLAG_SAMPLE_SIZE_PRESENT)
		{
			sample_size = READ_U32(ptr);
		}
		if (flags & TRUN_FLAG_SAMPLE_FLAGS_PRESENT)
		{
			sample_flags = READ_U32(ptr);
		}
		if (flags & TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT)
		{
			sample_composition_time_offset = READ_U32(ptr);
		}
	}
	FullBox fbox(sz, Box::TRUN, version, flags);
	return new TrunBox(fbox, totalSampleDuration);
}

TfhdBox::TfhdBox(uint32_t sz, uint64_t default_duration) : FullBox(sz, Box::TFHD, 0, 0), duration(default_duration)
{

}

TfhdBox::TfhdBox(FullBox &fbox, uint64_t default_duration) : FullBox(fbox), duration(default_duration)
{

}

void TfhdBox::setSampleDuration(uint64_t default_duration)
{
    duration = default_duration;
}

uint64_t TfhdBox::getSampleDuration()
{
    return duration;
}


TfhdBox* TfhdBox::constructTfhdBox(uint32_t sz, uint8_t *ptr)
{
    uint8_t version = READ_VERSION(ptr); //8
    uint32_t flags  = READ_FLAGS(ptr); //24

    const uint32_t TFHD_FLAG_BASE_DATA_OFFSET_PRESENT            = 0x00001;
    const uint32_t TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX_PRESENT    = 0x00002;
    const uint32_t TFHD_FLAG_DEFAULT_SAMPLE_DURATION_PRESENT     = 0x00008;
    const uint32_t TFHD_FLAG_DEFAULT_SAMPLE_SIZE_PRESENT         = 0x00010;
    const uint32_t TFHD_FLAG_DEFAULT_SAMPLE_FLAGS_PRESENT        = 0x00020;
    const uint32_t TFHD_FLAG_DURATION_IS_EMPTY                   = 0x10000;
    const uint32_t TFHD_FLAG_DEFAULT_BASE_IS_MOOF                = 0x20000;

    uint32_t TrackId;
    uint32_t BaseDataOffset;
    uint32_t SampleDescriptionIndex;
    uint32_t DefaultSampleDuration;
    uint32_t DefaultSampleSize;
    uint32_t DefaultSampleFlags;

    TrackId = READ_U32(ptr);
    if (flags & TFHD_FLAG_BASE_DATA_OFFSET_PRESENT) {
        BaseDataOffset = READ_64(ptr);
    } else {
        BaseDataOffset = 0;
    }
    if (flags & TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX_PRESENT) {
        SampleDescriptionIndex = READ_U32(ptr);
    } else {
        SampleDescriptionIndex = 1;
    }
    if (flags & TFHD_FLAG_DEFAULT_SAMPLE_DURATION_PRESENT) {
        DefaultSampleDuration = READ_U32(ptr);
    } else {
        DefaultSampleDuration = 0;
    }
    if (flags & TFHD_FLAG_DEFAULT_SAMPLE_SIZE_PRESENT) {
        DefaultSampleSize = READ_U32(ptr);
    } else {
        DefaultSampleSize = 0;
    }
    if (flags & TFHD_FLAG_DEFAULT_SAMPLE_FLAGS_PRESENT) {
        DefaultSampleFlags = READ_U32(ptr);
    } else {
        DefaultSampleFlags = 0;
    }

//    logprintf("TFHD constructTfhdBox TrackId: %d",TrackId );
//    logprintf("TFHD constructTfhdBox BaseDataOffset: %d",BaseDataOffset );
//    logprintf("TFHD constructTfhdBox SampleDescriptionIndex: %d",SampleDescriptionIndex);
//    logprintf("TFHD constructTfhdBox DefaultSampleDuration: %d",DefaultSampleDuration );
//    logprintf("TFHD constructTfhdBox DefaultSampleSize: %d",DefaultSampleSize );
//    logprintf("TFHD constructTfhdBox DefaultSampleFlags: %d",DefaultSampleFlags );

    FullBox fbox(sz, Box::TFHD, version, flags);
    return new TfhdBox(fbox, DefaultSampleDuration);
}

PrftBox::PrftBox(uint32_t sz, uint32_t trackId, uint64_t ntpTs, uint64_t mediaTime) : FullBox(sz, Box::PRFT, 0, 0), track_id(trackId), ntp_ts(ntpTs), media_time(mediaTime)
{

}

PrftBox::PrftBox(FullBox &fbox, uint32_t trackId, uint64_t ntpTs, uint64_t mediaTime) : FullBox(fbox), track_id(trackId), ntp_ts(ntpTs), media_time(mediaTime)
{

}

void PrftBox::setTrackId(uint32_t trackId)
{
    track_id = trackId;
}

uint32_t PrftBox::getTrackId()
{
    return track_id;
}

void PrftBox::setNtpTs(uint64_t ntpTs)
{
    ntp_ts = ntpTs;
}

uint64_t PrftBox::getNtpTs()
{
    return ntp_ts;
}

void PrftBox::setMediaTime(uint64_t mediaTime)
{
    media_time = mediaTime;
}

uint64_t PrftBox::getMediaTime()
{
    return media_time;
}


PrftBox* PrftBox::constructPrftBox(uint32_t sz, uint8_t *ptr)
{
    uint8_t version = READ_VERSION(ptr); //8
    uint32_t flags  = READ_FLAGS(ptr); //24

    uint32_t track_id = 0; // reference track ID
    track_id = READ_U32(ptr);
    uint64_t ntp_ts = 0; // NTP time stamp
    ntp_ts = READ_64(ptr);
    uint64_t pts = 0; //media time
    pts = READ_64(ptr);
//    logprintf("PRFT constructPrftBox TrackId: %d",track_id );
//    logprintf("PRFT constructPrftBox Ntp TS: %llu",ntp_ts );
//    logprintf("PRFT constructPrftBox Media Time: %ld",pts);
    FullBox fbox(sz, Box::PRFT, version, flags);

    return new PrftBox(fbox, track_id, ntp_ts, pts);
}


TrakBox* TrakBox::constructTrakBox(uint32_t sz, uint8_t *ptr)
{
	TrakBox *cbox = new TrakBox(sz);
	uint32_t curOffset = sizeof(uint32_t) + sizeof(uint32_t); //Sizes of size & type fields
	while (curOffset < sz)
	{
		Box *box = Box::constructBox(ptr, sz-curOffset);
		box->setOffset(curOffset);
		
		if (IS_TYPE(box->getType(),TKHD))
		{
			uint8_t *tkhd_start = ptr;
			ptr += curOffset;
			uint8_t version = READ_VERSION(ptr);
			uint32_t skip = 3; // size of flags
			if (1 == version)
			{
				skip += sizeof(uint64_t) * 2; //CreationTime + ModificationTime
			}
			else
			{
				skip += sizeof(uint32_t) * 2; //CreationTime + ModificationTime
			}
			ptr += skip;
			cbox->track_id = READ_U32(ptr);
			ptr = tkhd_start;
		}
		cbox->addChildren(box);
		curOffset += box->getSize();
		ptr += box->getSize();
	}
	return cbox;
}
