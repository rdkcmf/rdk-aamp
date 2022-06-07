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

/**
 * @file AampDrmData.h
 * @brief File holds DRM License data
 */ 

#ifndef AAMPDRMDATA_H
#define AAMPDRMDATA_H

/**
 * @class DrmData
 * @brief To hold DRM key, license request etc.
 */
class DrmData{

private:
 	std::string data;  /**< License Data */
public:
	/**
	 *  @fn DrmData
	 */
	DrmData();
	/**
	 *  @fn DrmData
	 *
	 *  @param[in]	data - pointer to data to be copied.
	 *  @param[in]	dataLength - length of data
	 */	
	DrmData(unsigned char *data, int dataLength);
	/**
         * @brief Copy constructor disabled
         *
         */
	DrmData(const DrmData&) = delete;
	/**
         * @brief assignment operator disabled
         *
         */
	DrmData& operator=(const DrmData&) = delete;
	/**
	 *  @fn	~DrmData
	 */
	~DrmData();
	/**
	 *  @fn getData
	 *
	 *  @return	Returns pointer to data.
	 */
    	const std::string  &getData();
	/**
	 *  @fn getDataLength
	 *
	 *  @return Returns dataLength.
	 */
	int getDataLength();
	/**
	 *  @fn		setData
	 *
	 *  @param[in]	data - Pointer to data to be set.
	 *  @param[in]	dataLength - length of data.
	 *  @return		void.
	 */
	void setData(unsigned char *data, int dataLength);
	/**
	 *  @fn 	addData
	 *
	 *  @param[in]  data - Pointer to data to be appended.
	 *  @param[in]  dataLength - length of data.
	 *  @return     void.
	 */
	void addData(unsigned char *data, int dataLength);
};


#endif /* AAMPDRMDATA_H */

