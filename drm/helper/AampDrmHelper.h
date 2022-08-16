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

#ifndef _AAMP_DRM_HELPER_H
#define _AAMP_DRM_HELPER_H

/**
 * @file AampDrmHelper.h
 * @brief Implented DRM helper functionalities
 */

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>

#include "AampDrmData.h"
#include "AampDrmInfo.h"
#include "AampDRMutils.h"
#include "AampMemorySystem.h"

#ifdef LIGHTTPD_AUTHSERVICE_DISABLE
#include <securityagent/SecurityTokenUtil.h>
#define SESSION_TOKEN_URL "http://127.0.0.1:9998/jsonrpc"
#define SESSION_TOKEN_POST_FIELD "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"org.rdk.AuthService.1.getSessionToken\"}"
#define AUTH_HEADER "Authorization: Bearer "
#define CONTENT_TYPE_HEADER "Content-Type: application/json"
#define MAX_LENGTH 1024
#else
#define SESSION_TOKEN_URL "http://localhost:50050/authService/getSessionToken"
#endif

/**
 * @struct AampChallengeInfo
 * @brief Aamp challenge info to get the License
 */

struct AampChallengeInfo
{
	AampChallengeInfo() : data(), url(), accessToken() {};
	std::shared_ptr<DrmData> data; /**< Challenge data returned from the DRM system */
	std::string url;               /**< Challenge URL returned from the DRM system */
	std::string accessToken;       /**< Access token required for the license request (if applicable) */
};

/**
 * @struct AampLicenseRequest
 * @brief Holds the data to get the License
 */

struct AampLicenseRequest
{
	AampLicenseRequest() : method(), url(), payload(), headers(),licenseAnonymousRequest(false)
	{}
	enum LicenseMethod
	{
		DRM_RETRIEVE,   /**< Don't fetch the license, it will be handled externally by the DRM */
		GET,		    /**< Fetch license via HTTP GET request */
		POST		    /**< Fetch license via HTTP POST request */
	};

	LicenseMethod method;
	bool licenseAnonymousRequest;
	std::string url;
	std::string payload;
	std::unordered_map<std::string, std::vector<std::string>> headers;
};

/**
 * @class AampDrmHelper
 * @brief AampDRM helper to handle DRM operations
 */
class AampDrmHelper
{
public:
	const uint32_t TIMEOUT_SECONDS;
	const std::string EMPTY_DRM_METADATA;
	
	const std::string EMPTY_STRING;
	AampLogManager *mLogObj;
	AampDrmHelper(const struct DrmInfo drmInfo, AampLogManager *logObj) : mLogObj(logObj), mDrmInfo(drmInfo), TIMEOUT_SECONDS(5000U), EMPTY_DRM_METADATA(), EMPTY_STRING() ,bOutputProtectionEnabled(false) {};
	AampDrmHelper(const AampDrmHelper&) = delete;
	AampDrmHelper& operator=(const AampDrmHelper&) = delete;
	
	/**
	 * @brief Returns the OCDM system ID of the helper
	 * @return the OCDM system ID
	 */
	virtual const std::string& ocdmSystemId() const = 0;

	/**
	 *
	 * @param initData the Init Data to send to the CDM
	 */
	virtual void createInitData(std::vector<uint8_t>& initData) const = 0;

	/**
	 * @brief Parse the optional PSSH data
	 * @param initData The init data from the PSSH
	 * @param initDataLen the length of initData
	 * @return
	 */
	virtual bool parsePssh(const uint8_t* initData, uint32_t initDataLen) = 0;

	/**
	 * @brief Determine if the DRM system needs to be in the clear or encrypted
	 * @return true if the data is clear, false if it should remain in the TEE
	 */
	virtual bool isClearDecrypt() const = 0;

	/**
	 * @brief Determine whether HDCP 2.2 protection is required to be active
	 * @return true if HDCP 2.2 protection is required, false otherwise
	 */
	virtual bool isHdcp22Required() const { return bOutputProtectionEnabled; };

	/**
	 * @brief Returns the content specific DRM metadata
	 * @return the DRM metadata
	 */
	virtual const std::string& getDrmMetaData() const {return EMPTY_DRM_METADATA;}

	/**
	 * @brief Sets the content specific DRM metadata
	 * @param the DRM metadata
	 */
	virtual void setDrmMetaData(const std::string& metaData) { }

	/**
	 * @brief Sets the defualt keyID
	 * @param the DRM cencData data
	 */
	virtual void setDefaultKeyID(const std::string& cencData) { }

	/**
	 * @brief Returns the DRM codec type for the helper, used in trace
	 * @return the DRM codec type
	 */
	virtual int getDrmCodecType() const { return 0; }

	/**
	 * @brief Get the amount of time in milliseconds to wait before aborting the wait
	 * for the license_challenge message to be received
	 * Default is TWO Seconds - 2000
	 * @return the time to wait in milliseconds
	 */
	virtual uint32_t licenseGenerateTimeout() const { return TIMEOUT_SECONDS; }

	/**
	 * @brief Get the amount of time in milliseconds to wait before aborting the wait
	 * for the key_updated message to be received
	 * Default is TWO Seconds - 2000
	 * @return the time to wait in milliseconds
	 */
	virtual uint32_t keyProcessTimeout() const { return TIMEOUT_SECONDS; }

	/**
	 * @brief Get the key ID
	 * @param keyID The key ID as a vector of binary data
	 */
	virtual void getKey(std::vector<uint8_t>& keyID) const = 0;

	/**
         * @brief Get the key IDs
         * @param keyIDs The map containing Key ID vector of binary data
         */
	virtual void getKeys(std::map<int, std::vector<uint8_t>>& keyIDs) const {};

	/**
	 * @brief Get the UUID
	 * @return the UUID
	 */
	virtual const std::string& getUuid() const { return mDrmInfo.systemUUID; };

	/**
	 * @brief Determines if the DRM itself fetches the license or if AAMP should use
	 * its own internal HTTP client to fetch the license
	 * Returning 'true' removes AAMP calling generateLicenseRequest() on the CDM
	 * Default is to return false
	 * @return true if the DRM acquires the license, false if AAMP should do it
	 */
	virtual bool isExternalLicense() const { return false; };

	/**
	 * @brief Generate the request details for the DRM license
	 * @param challengeInfo challenge information from the DRM system necessary to construct the license request
	 * @param licenseRequest license request data to populate
	 */
	virtual void generateLicenseRequest(const AampChallengeInfo& challengeInfo, AampLicenseRequest& licenseRequest) const = 0;

	/**
	 * @brief Transform the license response from the server into the necessary format for OCDM
	 * @param licenseResponse license response from the server to transform
	 */
	virtual void transformLicenseResponse(std::shared_ptr<DrmData> licenseResponse) const {};

	/**
	 * @brief Get the memory system used to transform data for transmission
	 * @return the memory system, or null if to send it as is to the ocdm wrapper
	 */
	virtual AAMPMemorySystem* getMemorySystem() { return nullptr; }

	/**
	 * @fn compare
	 * @return true if the two helpers can be considered the same, false otherwise
	 */
	virtual bool compare(std::shared_ptr<AampDrmHelper> other);

	/**
	 * @brief Cancels a DRM session
	 */
	virtual void cancelDrmSession() { };

	/**
	 * @brief Checks if the helper can cancel a session, or if the caller should do it
	 * @return true if the helper can cancel
	 */
	virtual bool canCancelDrmSession() { return false; }

	/**
	 * @brief Gets the friendly display name of the DRM
	 * @return friendly name
	 */
	virtual const std::string& friendlyName() const { return EMPTY_STRING; }

	/**
	 * @brief Set Output protection flag for the drmHelper
	 * @return None
	 */
	void setOutputProtectionFlag(bool bValue) { bOutputProtectionEnabled = bValue;}
public:
	virtual ~AampDrmHelper() {};

protected:
	const DrmInfo mDrmInfo;
	bool bOutputProtectionEnabled;
};

/**
 * @class AampDrmHelperFactory
 * @brief Helper class to  Maintain DRM data
 */

class AampDrmHelperFactory
{
public:
	/**
         * @brief Default weighting of a helper factory.
	 * Nominal scale of 0 to DEFAULT_WEIGHTING * 2
	 * Larger weightings have lower priority
	 */
	static const int DEFAULT_WEIGHTING = 50;

	/**
	 * @brief Determines if a helper class provides the identified DRM
	 * @param drmInfo DrmInfo built by the HLS manifest parser
	 * @return true if this helper provides that DRM
	 */
	virtual bool isDRM(const struct DrmInfo& drmInfo) const = 0;

	/**
	 * @brief Build a helper class to support the identified DRM
	 * @param drmInfo DrmInfo built by the HLS manifest parser
	 * @return the helper
	 */
	virtual std::shared_ptr<AampDrmHelper> createHelper(const struct DrmInfo& drmInfo, AampLogManager *logObj=NULL) const = 0;

	/**
	 * @brief Adds the system IDs supported by the DRM to a vector
	 * Used by the GStreamer plugins to advertise the DRM upstream to the pipeline
	 * @param systemIds the vector to use
	 */
	virtual void appendSystemId(std::vector<std::string>& systemIds) const = 0;

	/**
	 * @brief Get the weighting for this helper factory, which determines its priority
	 * @return weighting value
	 */
	int getWeighting() { return mWeighting; }

	virtual ~AampDrmHelperFactory() {};

protected:
	AampDrmHelperFactory(int weighting = DEFAULT_WEIGHTING);
	int mWeighting;
};


/**
 * @class AampDrmHelperEngine
 * @brief Helper Engine for Aamp DRM operations
 */
class AampDrmHelperEngine
{
private:
	std::vector<AampDrmHelperFactory* > factories;

public:
	/**
	 * @brief AampDrmHelperEngine constructor
	 */
	AampDrmHelperEngine() : factories() {};
	/**
	 * @fn hasDRM
	 * @param systemId the UUID from the PSSH or manifest
	 * @param drmInfo DrmInfo built by the HLS manifest parser
	 * @return true if a DRM was found, false otherwise
	 */
	bool hasDRM(const struct DrmInfo& drmInfo) const;

	/**
	 * @fn createHelper
	 * @param drmInfo DrmInfo built by the HLS manifest parser
	 * @return the helper
	 */
	std::shared_ptr<AampDrmHelper> createHelper(const struct DrmInfo& drmInfo, AampLogManager *logObj=NULL) const;

	/**
	 * @fn getSystemIds
	 * @param ids vector to populate with supported IDs
	 */
	void getSystemIds(std::vector<std::string>& ids) const;

	/**
	 * @fn getInstance
	 * @return DRM Helper Engine instance
	 */
	static AampDrmHelperEngine& getInstance();

	/**
	 * @fn registerFactory
	 * @param factory helper factory instance to register
	 */
	void registerFactory(AampDrmHelperFactory* factory);
};

#endif //_AAMP_DRM_HELPER_H
