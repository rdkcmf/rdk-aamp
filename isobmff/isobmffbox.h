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
* @file isobmffbox.h
* @brief Header file for ISO Base Media File Format Boxes
*/

#ifndef __ISOBMFFBOX_H__
#define __ISOBMFFBOX_H__

#include <cstdint>
#include <vector>
#include <string.h>

#define READ_U32(buf) \
	(buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3]; buf+=4;

#define WRITE_U64(buf, val) \
	buf[0]= val>>56; buf[1]= val>>48; buf[2]= val>>40; buf[3]= val>>32; buf[4]= val>>24; buf[5]= val>>16; buf[6]= val>>8; buf[7]= val;

#define WRITE_U32(buf, val) \
	buf[0]= val>>24; buf[1]= val>>16; buf[2]= val>>8; buf[3]= val;

#define READ_U8(dst, src, sz) \
	memcpy(dst, src, sz); src+=sz;

#define READ_VERSION(buf) \
		buf[0]; buf++;

#define READ_FLAGS(buf) \
		(buf[0] << 16) | (buf[1] << 8) | buf[2]; buf+=3;

uint64_t ReadUint64(uint8_t *buf);

void WriteUint64(uint8_t *dst, uint64_t val);

uint32_t ReadCStringLen(const uint8_t* buffer, uint32_t bufferLen);

#define READ_BMDT64(buf) \
		ReadUint64(buf); buf+=8;

#define READ_64(buf) \
                ReadUint64(buf); buf+=8;

#define IS_TYPE(value, type) \
		(value[0]==type[0] && value[1]==type[1] && value[2]==type[2] && value[3]==type[3])


/**
 * @brief Base Class for ISO BMFF Box
 */
class Box
{
private:
	uint32_t offset;	//Offset from the beginning of the segment
	uint32_t size;		//Box Size
	char type[5]; 		//Box Type Including \0

/*TODO: Handle special cases separately */
public:
	static constexpr const char *FTYP = "ftyp";
	static constexpr const char *MOOV = "moov";
	static constexpr const char *MVHD = "mvhd";
	static constexpr const char *TRAK = "trak";
	static constexpr const char *MDIA = "mdia";
	static constexpr const char *MDHD = "mdhd";
	static constexpr const char *EMSG = "emsg";

	static constexpr const char *MOOF = "moof";
	static constexpr const char *TFHD = "tfhd";
	static constexpr const char *TRAF = "traf";
	static constexpr const char *TFDT = "tfdt";
	static constexpr const char *TRUN = "trun";
	static constexpr const char *MDAT = "mdat";

	static constexpr const char *STYP = "styp";
	static constexpr const char *SIDX = "sidx";
	static constexpr const char *PRFT = "prft";

	/**
	 * @brief Box constructor
	 *
	 * @param[in] sz - box size
	 * @param[in] btype - box type
	 */
	Box(uint32_t sz, const char btype[4]);

	/**
	 * @brief Box destructor
	 */
	virtual ~Box()
	{

	}

	/**
	 * @brief Set box's offset from the beginning of the buffer
	 *
	 * @param[in] os - offset
	 * @return void
	 */
	void setOffset(uint32_t os);

	/**
	 * @brief Get box offset
	 *
	 * @return offset of box
	 */
	uint32_t getOffset() const;

	/**
	 * @brief To check if box has any child boxes
	 *
	 * @return true if this box has other boxes as children
	 */
	virtual bool hasChildren();

	/**
	 * @brief Get children of this box
	 *
	 * @return array of child boxes
	 */
	virtual const std::vector<Box*> *getChildren();

	/**
	 * @brief Get box size
	 *
	 * @return box size
	 */
	uint32_t getSize() const;

	/**
	 * @brief Get box type
	 *
	 * @return box type
	 */
	const char *getType();

    /**
    * @brief Get box type
    *
    * @return box type if parsed. "unknown" otherwise
    */
    const char *getBoxType() const;

	/**
	 * @brief Static function to construct a Box object
	 *
	 * @param[in] hdr - pointer to box
	 * @param[in] maxSz - box size
	 * @return newly constructed Box object
	 */
	static Box* constructBox(uint8_t *hdr, uint32_t maxSz, bool correctBoxSize = false); 
};


/**
 * @brief Class for ISO BMFF Box container
 * Eg: MOOV, MOOF, TRAK, MDIA
 */
class GenericContainerBox : public Box
{
private:
	std::vector<Box*> children;	// array of child boxes

public:
	/**
	 * @brief GenericContainerBox constructor
	 *
	 * @param[in] sz - box size
	 * @param[in] btype - box type
	 */
	GenericContainerBox(uint32_t sz, const char btype[4]);

	/**
	 * @brief GenericContainerBox destructor
	 */
	virtual ~GenericContainerBox();

	/**
	 * @brief Add a box as a child box
	 *
	 * @param[in] box - child box object
	 * @return void
	 */
	void addChildren(Box *box);

	/**
	 * @brief To check if box has any child boxes
	 *
	 * @return true if this box has other boxes as children
	 */
	bool hasChildren() override;

	/**
	 * @brief Get children of this box
	 *
	 * @return array of child boxes
	 */
	const std::vector<Box*> *getChildren() override;

	/**
	 * @brief Static function to construct a GenericContainerBox object
	 *
	 * @param[in] sz - box size
	 * @param[in] btype - box type
	 * @param[in] ptr - pointer to box
	 * @return newly constructed GenericContainerBox object
	 */
	static GenericContainerBox* constructContainer(uint32_t sz, const char btype[4], uint8_t *ptr);
};


/**
 * @brief Class for single ISO BMFF Box
 * Eg: FTYP, MDHD, MVHD, TFDT
 */
class FullBox : public Box
{
protected:
	uint8_t version;
	uint32_t flags;

public:
	/**
	 * @brief FullBox constructor
	 *
	 * @param[in] sz - box size
	 * @param[in] btype - box type
	 * @param[in] ver - version value
	 * @param[in] f - flag value
	 */
	FullBox(uint32_t sz, const char btype[4], uint8_t ver, uint32_t f);
};


/**
 * @brief Class for ISO BMFF MVHD Box
 */
class MvhdBox : public FullBox
{
private:
	uint32_t timeScale;

public:
	/**
	 * @brief MvhdBox constructor
	 *
	 * @param[in] sz - box size
	 * @param[in] tScale - TimeScale value
	 */
	MvhdBox(uint32_t sz, uint32_t tScale);

	/**
	 * @brief MvhdBox constructor
	 *
	 * @param[in] fbox - box object
	 * @param[in] tScale - TimeScale value
	 */
	MvhdBox(FullBox &fbox, uint32_t tScale);

	/**
	 * @brief Set TimeScale value
	 *
	 * @param[in] tScale - TimeScale value
	 * @return void
	 */
	void setTimeScale(uint32_t tScale);

	/**
	 * @brief Get TimeScale value
	 *
	 * @return TimeScale value
	 */
	uint32_t getTimeScale();

	/**
	 * @brief Static function to construct a MvhdBox object
	 *
	 * @param[in] sz - box size
	 * @param[in] ptr - pointer to box
	 * @return newly constructed MvhdBox object
	 */
	static MvhdBox* constructMvhdBox(uint32_t sz, uint8_t *ptr);
};


/**
 * @brief Class for ISO BMFF MDHD Box
 */
class MdhdBox : public FullBox
{
private:
	uint32_t timeScale;

public:
	/**
	 * @brief MdhdBox constructor
	 *
	 * @param[in] sz - box size
	 * @param[in] tScale - TimeScale value
	 */
	MdhdBox(uint32_t sz, uint32_t tScale);

	/**
	 * @brief MdhdBox constructor
	 *
	 * @param[in] fbox - box object
	 * @param[in] tScale - TimeScale value
	 */
	MdhdBox(FullBox &fbox, uint32_t tScale);

	/**
	 * @brief Set TimeScale value
	 *
	 * @param[in] tScale - TimeScale value
	 * @return void
	 */
	void setTimeScale(uint32_t tScale);

	/**
	 * @brief Get TimeScale value
	 *
	 * @return TimeScale value
	 */
	uint32_t getTimeScale();

	/**
	 * @brief Static function to construct a MdhdBox object
	 *
	 * @param[in] sz - box size
	 * @param[in] ptr - pointer to box
	 * @return newly constructed MdhdBox object
	 */
	static MdhdBox* constructMdhdBox(uint32_t sz, uint8_t *ptr);
};


/**
 * @brief Class for ISO BMFF TFDT Box
 */
class TfdtBox : public FullBox
{
private:
	uint64_t baseMDT;	//BaseMediaDecodeTime value

public:
	/**
	 * @brief TfdtBox constructor
	 *
	 * @param[in] sz - box size
	 * @param[in] mdt - BaseMediaDecodeTime value
	 */
	TfdtBox(uint32_t sz, uint64_t mdt);

	/**
	 * @brief TfdtBox constructor
	 *
	 * @param[in] fbox - box object
	 * @param[in] mdt - BaseMediaDecodeTime value
	 */
	TfdtBox(FullBox &fbox, uint64_t mdt);

	/**
	 * @brief Set BaseMediaDecodeTime value
	 *
	 * @param[in] mdt - BaseMediaDecodeTime value
	 * @return void
	 */
	void setBaseMDT(uint64_t mdt);

	/**
	 * @brief Get BaseMediaDecodeTime value
	 *
	 * @return BaseMediaDecodeTime value
	 */
	uint64_t getBaseMDT();

	/**
	 * @brief Static function to construct a TfdtBox object
	 *
	 * @param[in] sz - box size
	 * @param[in] ptr - pointer to box
	 * @return newly constructed TfdtBox object
	 */
	static TfdtBox* constructTfdtBox(uint32_t sz, uint8_t *ptr);
};

/**
 * @brief Class for ISO BMFF EMSG Box
 */
class EmsgBox : public FullBox
{
private:
	uint32_t timeScale;
	uint32_t eventDuration;
	uint32_t id;
	uint32_t presentationTimeDelta; // This is added in emsg box v1
	uint64_t presentationTime;	// This is included in emsg box v0
	uint8_t* schemeIdUri;
	uint8_t* value;
	// Message data
	uint8_t* messageData;
	uint32_t messageLen;

public:
	/**
	 * @brief EmsgBox constructor
	 *
	 * @param[in] sz - box size
	 * @param[in] tScale - TimeScale value
	 * @param[in] evtDur - eventDuration value
	 * @param[in] _id - id value
	 */
	EmsgBox(uint32_t sz, uint32_t tScale, uint32_t evtDur, uint32_t _id);

	/**
	 * @brief EmsgBox constructor
	 *
	 * @param[in] fbox - box object
	 * @param[in] tScale - TimeScale value
	 * @param[in] evtDur - eventDuration value
	 * @param[in] _id - id value
	 * @param[in] presTime - presentationTime value
	 * @param[in] presTimeDelta - presentationTimeDelta value
	 */
	EmsgBox(FullBox &fbox, uint32_t tScale, uint32_t evtDur, uint32_t _id, uint64_t presTime, uint32_t presTimeDelta);

	/**
	 * @brief EmsgBox dtor
	 */
	~EmsgBox();

	/**
	 * @brief EmsgBox copy constructor
	 */
	EmsgBox(const EmsgBox&) = delete;

	/**
	 * @brief EmsgBox =operator overloading
	 */
	EmsgBox& operator=(const EmsgBox&) = delete;

	/**
	 * @brief Set TimeScale value
	 *
	 * @param[in] tScale - TimeScale value
	 * @return void
	 */
	void setTimeScale(uint32_t tScale);

	/**
	 * @brief Get schemeIdUri
	 *
	 * @return TimeScale value
	 */
	uint32_t getTimeScale();

	/**
	 * @brief Set eventDuration value
	 *
	 * @param[in] evtDur - eventDuration value
	 * @return void
	 */
	void setEventDuration(uint32_t evtDur);

	/**
	 * @brief Get eventDuration
	 *
	 * @return eventDuration value
	 */
	uint32_t getEventDuration();

	/**
	 * @brief Set id
	 *
	 * @param[in] _id - id
	 * @return void
	 */
	void setId(uint32_t _id);

	/**
	 * @brief Get id
	 *
	 * @return id value
	 */
	uint32_t getId();

	/**
	 * @brief Set presentationTimeDelta
	 *
	 * @param[in] presTimeDelta - presTimeDelta
	 * @return void
	 */
	void setPresentationTimeDelta(uint32_t presTimeDelta);

	/**
	 * @brief Get presentationTimeDelta
	 *
	 * @return presentationTimeDelta value
	 */
	uint32_t getPresentationTimeDelta();

	/**
	 * @brief Set presentationTime
	 *
	 * @param[in] presTime - presTime
	 * @return void
	 */
	void setPresentationTime(uint64_t presTime);

	/**
	 * @brief Get presentationTime
	 *
	 * @return presentationTime value
	 */
	uint64_t getPresentationTime();

	/**
	 * @brief Set schemeIdUri
	 *
	 * @param[in] schemeIdUri - schemeIdUri pointer
	 * @return void
	 */
	void setSchemeIdUri(uint8_t* schemeIdURI);

	/**
	 * @brief Get schemeIdUri
	 *
	 * @return schemeIdUri value
	 */
	uint8_t* getSchemeIdUri();

	/**
	 * @brief Set value
	 *
	 * @param[in] value - value pointer
	 * @return void
	 */
	void setValue(uint8_t* schemeIdValue);

	/**
	 * @brief Get value
	 *
	 * @return schemeIdUri value
	 */
	uint8_t* getValue();

	/**
	 * @brief Set Message
	 *
	 * @param[in] message - Message pointer
	 * @param[in] len - Message length
	 * @return void
	 */
	void setMessage(uint8_t* message, uint32_t len);

	/**
	 * @brief Get Message
	 *
	 * @return messageData
	 */
	uint8_t* getMessage();

	/**
	 * @brief Get Message length
	 *
	 * @return messageLen
	 */
	uint32_t getMessageLen();

	/**
	 * @brief Static function to construct a EmsgBox object
	 *
	 * @param[in] sz - box size
	 * @param[in] ptr - pointer to box
	 * @return newly constructed EmsgBox object
	 */
	static EmsgBox* constructEmsgBox(uint32_t sz, uint8_t *ptr);
};

/**
 * @brief Class for ISO BMFF TRUN Box
 */
class TrunBox : public FullBox
{
private:
	uint64_t duration;    //Sample Duration value


public:
	struct Entry {
	Entry() : sample_duration(0), sample_size(0), sample_flags(0), sample_composition_time_offset(0) {}
	uint32_t sample_duration;
	uint32_t sample_size;
	uint32_t sample_flags;
	uint32_t sample_composition_time_offset;
	};
	/**
	* @brief TrunBox constructor
	*
	* @param[in] sz - box size
	* @param[in] mdt - sampleDuration value
	*/
	TrunBox(uint32_t sz, uint64_t sampleDuration);

	/**
	* @brief TrunBox constructor
	*
	* @param[in] fbox - box object
	* @param[in] mdt - BaseMediaDecodeTime value
	*/
	TrunBox(FullBox &fbox, uint64_t sampleDuration);

	/**
	* @brief Set SampleDuration value
	*
	* @param[in] sampleDuration - Sample Duration value
	* @return void
	*/
	void setSampleDuration(uint64_t sampleDuration);

	/**
	* @brief Get sampleDuration value
	*
	* @return sampleDuration value
	*/
	uint64_t getSampleDuration();

	/**
	* @brief Static function to construct a TrunBox object
	*
	* @param[in] sz - box size
	* @param[in] ptr - pointer to box
	* @return newly constructed TrunBox object
	*/
	static TrunBox* constructTrunBox(uint32_t sz, uint8_t *ptr);
};

/**
 * @brief Class for ISO BMFF TFHD Box
 */
class TfhdBox : public FullBox
{
private:
    uint64_t duration;

public:
    /**
     * @brief TfhdBox constructor
     *
     * @param[in] sz - box size
     * @param[in] sample_duration - Sample Duration value
     */
    TfhdBox(uint32_t sz, uint64_t sample_duration);

    /**
     * @brief TfhdBox constructor
     *
     * @param[in] fbox - box object
     * @param[in] sample_duration - Sample Duration value
     */
    TfhdBox(FullBox &fbox, uint64_t sample_duration);

    /**
     * @brief Set Sample Duration value
     *
     * @param[in] sample_duration - SampleDuration value
     * @return void
     */
    void setSampleDuration(uint64_t sample_duration);

    /**
     * @brief Get SampleDuration value
     *
     * @return SampleDuration value
     */
    uint64_t getSampleDuration();

    /**
     * @brief Static function to construct a TfdtBox object
     *
     * @param[in] sz - box size
     * @param[in] ptr - pointer to box
     * @return newly constructed TfhdBox object
     */
    static TfhdBox* constructTfhdBox(uint32_t sz, uint8_t *ptr);
};

/**
 * @brief Class for ISO BMFF TFHD Box
 */
class PrftBox : public FullBox
{
private:
    uint32_t track_id;
    uint64_t ntp_ts;
    uint64_t media_time;

public:
    /**
     * @brief PrftBox constructor
     *
     * @param[in] sz - box size
     * @param[in] trackId - media time
     * @param[in] ntpTs - media time
     * @param[in] mediaTime - media time
     */
    PrftBox(uint32_t sz, uint32_t trackId, uint64_t ntpTs, uint64_t mediaTime);

    /**
     * @brief PrftBox constructor
     *
     * @param[in] fbox - box object
     * @param[in] trackId - media time
     * @param[in] ntpTs - media time
     * @param[in] mediaTime - media time
     */
    PrftBox(FullBox &fbox, uint32_t trackId, uint64_t ntpTs, uint64_t mediaTime);

    /**
     * @brief Set Track Id value
     *
     * @param[in] trackId - Track Id value
     * @return void
     */
    void setTrackId(uint32_t trackId);

    /**
     * @brief Get Track id value
     *
     * @return track_id value
     */
    uint32_t getTrackId();

    /**
     * @brief Set NTP Ts value
     *
     * @param[in] ntpTs - ntp timestamp value
     * @return void
     */
    void setNtpTs(uint64_t ntpTs);

    /**
     * @brief Get ntp Timestamp value
     *
     * @return ntp_ts value
     */
    uint64_t getNtpTs();

    /**
     * @brief Set Sample Duration value
     *
     * @param[in] mediaTime - metia time value
     * @return void
     */
    void setMediaTime(uint64_t mediaTime);

    /**
     * @brief Get SampleDuration value
     *
     * @return media_time value
     */
    uint64_t getMediaTime();

    /**
     * @brief Static function to construct a PrftBox object
     *
     * @param[in] sz - box size
     * @param[in] ptr - pointer to box
     * @return newly constructed PrftBox object
     */
    static PrftBox* constructPrftBox(uint32_t sz, uint8_t *ptr);
};

#endif /* __ISOBMFFBOX_H__ */
