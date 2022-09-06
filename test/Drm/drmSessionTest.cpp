#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <iterator>
#include <memory>

#include "aampdrmsessionfactory.h"
#include "AampDRMSessionManager.h"
#include "AampVgdrmHelper.h"
#include "AampClearKeyHelper.h"
#include "AampHlsOcdmBridge.h"
#include "open_cdm.h"

#include "aampMocks.h"
#include "opencdmMocks.h"
#include "curlMocks.h"

#include "drmTestUtils.h"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

struct TestCurlResponse
{
	std::string response;
	MockCurlOpts opts;
	unsigned int callCount;

	TestCurlResponse(std::string response) :
		response(response),
		callCount(0) {}

	std::vector<std::string> getHeaders() const
	{
		std::vector<std::string> headerList;

		for (int i = 0; i < opts.headerCount; ++i)
		{
			headerList.push_back(std::string(opts.headers[i]));
		}
		std::sort(headerList.begin(), headerList.end());
		return headerList;
	}
};

struct MockChallengeData
{
	std::string url;
	std::string challenge;

	MockChallengeData(std::string url = "", std::string challenge = "") :
		url(url),
		challenge(challenge) {}
};

TEST_GROUP(AampDrmSessionTests)
{
	PrivateInstanceAAMP *mAamp;
	OpenCDMSession* mOcdmSession = (OpenCDMSession*)0x0CD12345;
	OpenCDMSystem* mOcdmSystem = (OpenCDMSystem*)0x0CDACC12345;
	std::map<std::string, TestCurlResponse> mCurlResponses;
	MockChallengeData mMockChallengeData;
	AampDRMSessionManager *sessionManager;
	AampLogManager mLogging;
	int mMaxDrmSessions = 2;

	// The URL AAMP uses to fetch the session token
	const std::string mSessionTokenUrl = "http://localhost:50050/authService/getSessionToken";
	// A fixed session token response - any tests which require a session token can use this,
	// since the manager is singleton and will only ever fetch it once. Test order is non-deterministic,
	// so it's not guaranteed which test will actually trigger the fetch
	const std::string mSessionTokenResponse = "{\"token\":\"SESSIONTOKEN\", \"status\":0}";

	AampDRMSessionManager* getSessionManager()
	{
		if (sessionManager == nullptr) {
			sessionManager = new AampDRMSessionManager(&mLogging, mMaxDrmSessions);
		}
		return sessionManager;
	}

	AampDrmSession* createDrmSessionForHelper(std::shared_ptr<AampDrmHelper> drmHelper, const char *keySystem)
	{
		AampDRMSessionManager *sessionManager = getSessionManager();
		DrmMetaDataEventPtr event = createDrmMetaDataEvent();

		mock("OpenCDM").expectOneCall("opencdm_create_system")
				.withParameter("keySystem", keySystem)
				.andReturnValue(mOcdmSystem);

		mock("OpenCDM").expectOneCall("opencdm_construct_session")
				.withOutputParameterReturning("session", &mOcdmSession, sizeof(mOcdmSession))
				.andReturnValue(0);
		AampDrmSession *drmSession = sessionManager->createDrmSession(drmHelper, event, mAamp, eMEDIATYPE_VIDEO);
		mock().checkExpectations();
		CHECK_TEXT(drmSession != NULL, "Session creation failed");

		return drmSession;
	}

	AampDrmSession* createDashDrmSession(const std::string testKeyData, const std::string psshStr, DrmMetaDataEventPtr& event)
	{
		std::vector<uint8_t> testKeyDataVec(testKeyData.begin(), testKeyData.end());
		return createDashDrmSession(testKeyDataVec, psshStr, event);
	}

	AampDrmSession* createDashDrmSession(const std::vector<uint8_t> testKeyData, const std::string psshStr, DrmMetaDataEventPtr& event)
	{
		AampDRMSessionManager *sessionManager = getSessionManager();

		mock("OpenCDM").expectOneCall("opencdm_create_system")
			.withParameter("keySystem", "com.microsoft.playready")
			.andReturnValue(mOcdmSystem);

		mock("OpenCDM").expectOneCall("opencdm_construct_session")
				.withOutputParameterReturning("session", &mOcdmSession, sizeof(mOcdmSession))
				.andReturnValue(0);

		mock("OpenCDM").expectOneCall("opencdm_session_update")
				.withMemoryBufferParameter("keyMessage", testKeyData.data(), testKeyData.size())
				.withIntParameter("keyLength", testKeyData.size())
				.andReturnValue(0);

		AampDrmSession *drmSession = sessionManager->createDrmSession("9a04f079-9840-4286-ab92-e65be0885f95", eMEDIAFORMAT_DASH,
				(const unsigned char*)psshStr.c_str(), psshStr.length(), eMEDIATYPE_VIDEO, mAamp, event);
		CHECK_TEXT(drmSession != NULL, "Session creation failed");

		return drmSession;
	}

	std::shared_ptr<AampHlsOcdmBridge> createBridgeForHelper(std::shared_ptr<AampVgdrmHelper> drmHelper)
	{
		return std::make_shared<AampHlsOcdmBridge>(&mLogging, createDrmSessionForHelper(drmHelper, "net.vgdrm"));
	}

	DrmMetaDataEventPtr createDrmMetaDataEvent()
	{
		return std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, false);
	}

	void setupCurlPerformResponse(std::string response)
	{
		static string responseStr = response;

		MockCurlSetPerformCallback([](CURL *curl, MockCurlWriteCallback writeCallback, void* writeData, void* userData) {
			writeCallback((char*)responseStr.c_str(), 1, responseStr.size(), writeData);
		}, this);
	}

	void setupCurlPerformResponses(const std::map<std::string, std::string>& responses)
	{
		for (auto& response : responses)
		{
			mCurlResponses.emplace(response.first, TestCurlResponse(response.second));
		}

		MockCurlSetPerformCallback([](CURL *curl, MockCurlWriteCallback writeCallback, void* writeData, void* userData) {
			const auto *responseMap = (const std::map<std::string, TestCurlResponse>*)userData;
			const MockCurlOpts *curlOpts = MockCurlGetOpts();

			// Check if there is a response setup for this URL
			auto iter = responseMap->find(curlOpts->url);
			if (iter != responseMap->end())
			{
				TestCurlResponse *curlResponse = const_cast<TestCurlResponse*>(&iter->second);
				curlResponse->opts = *curlOpts; // Taking a copy of the opts so we can check them later
				curlResponse->callCount++;
				// Issue the write callback with the user-provided response
				writeCallback((char*)curlResponse->response.c_str(), 1, curlResponse->response.size(), writeData);
			}
		}, &mCurlResponses);
	}

	const TestCurlResponse* getCurlPerformResponse(std::string url)
	{
		auto iter = mCurlResponses.find(url);
		if (iter != mCurlResponses.end())
		{
			return &iter->second;
		}

		return nullptr;
	}

	void checkHeaders()
	{
		CHECK_EQUAL(1, 0);
	}

	void setupChallengeCallbacks(const MockChallengeData& challengeData = MockChallengeData("challenge.example", "OCDM_CHALLENGE_DATA"))
	{
		mMockChallengeData = challengeData;
		MockOpenCdmCallbacks callbacks = {NULL, NULL};
		callbacks.constructSessionCallback = [](const MockOpenCdmSessionInfo *mockSessionInfo, void *mockUserData) {
			MockChallengeData *pChallengeData = (MockChallengeData*)mockUserData;
			// OpenCDM should come back to us with a URL + challenge payload.
			// The content of these shouldn't matter for us, since we use the request info from the DRM helper instead
			const char* url = pChallengeData->url.c_str();
			const std::string challenge = pChallengeData->challenge;
			mockSessionInfo->callbacks.process_challenge_callback((OpenCDMSession*)mockSessionInfo->session,
					mockSessionInfo->userData, url, (const uint8_t*)challenge.c_str(), challenge.size());
		};

		callbacks.sessionUpdateCallback = [](const MockOpenCdmSessionInfo *mockSessionInfo, const uint8_t keyMessage[], const uint16_t keyLength) {
			mockSessionInfo->callbacks.key_update_callback((OpenCDMSession*)mockSessionInfo->session, mockSessionInfo->userData, keyMessage, keyLength);
			mockSessionInfo->callbacks.keys_updated_callback((OpenCDMSession*)mockSessionInfo->session, mockSessionInfo->userData);
		};
		MockOpenCdmSetCallbacks(callbacks, &mMockChallengeData);
	}

	void setupChallengeCallbacksForExternalLicense()
	{
		MockOpenCdmCallbacks callbacks = {NULL, NULL};
		callbacks.constructSessionCallback = [](const MockOpenCdmSessionInfo *mockSessionInfo, void *mockUserData) {
			// OpenCDM should come back to us with a URL + challenge payload.
			// The content of these shouldn't matter for us, since we use the request info from the DRM helper instead
			const char* url = "test";
			uint8_t challenge[] = {'A'};
			mockSessionInfo->callbacks.process_challenge_callback((OpenCDMSession*)mockSessionInfo->session, mockSessionInfo->userData, url, challenge, 1);

			// For DRM's which perform license acquistion outside the AampDRMSessionManager context(AampLicenseRequest::DRM_RETRIEVE)
			// there wont be an opencdm_session_update call,hence trigger the keys_updated_callback as well within this callback
			mockSessionInfo->callbacks.key_update_callback((OpenCDMSession*)mockSessionInfo->session, mockSessionInfo->userData, nullptr, 0);
			mockSessionInfo->callbacks.keys_updated_callback((OpenCDMSession*)mockSessionInfo->session, mockSessionInfo->userData);
		};

		MockOpenCdmSetCallbacks(callbacks, NULL);
	}

	void setup()
	{
		MockAampReset();
		MockCurlReset();
		MockOpenCdmReset();
		mAamp = new PrivateInstanceAAMP(gpGlobalConfig);
	}

	void teardown()
	{
		if (sessionManager != nullptr) 
		{
			sessionManager->clearSessionData();
			SAFE_DELETE(sessionManager);
		}

		if (NULL != mAamp)
		{
			delete mAamp;
			mAamp = NULL;
		}

		MockAampReset();
		MockCurlReset();
		MockOpenCdmReset();
	}
};

#ifndef USE_SECCLIENT
TEST(AampDrmSessionTests, TestVgdrmSessionDecrypt)
{
	DrmInfo drmInfo;
	drmInfo.keyURI = "81701500000810367b131dd025ab0a7bd8d20c1314151600";
	std::shared_ptr<AampVgdrmHelper> drmHelper = std::make_shared<AampVgdrmHelper>(drmInfo, &mLogging);

	// The key used in the decrypt call should be picked up from the DRM helper
	// (URI to key conversion is tested elsewhere)
	std::vector<uint8_t> expectedKeyId;
	drmHelper->getKey(expectedKeyId);
	LONGS_EQUAL(16, expectedKeyId.size());

	setupChallengeCallbacksForExternalLicense();

	AampDrmSession *drmSession = createDrmSessionForHelper(drmHelper, "net.vgdrm");
	STRCMP_EQUAL("net.vgdrm", drmSession->getKeySystem().c_str());

	const uint8_t testIv[] = {'T', 'E', 'S', 'T', 'I', 'V'};
	const uint8_t payloadData[] = {'E', 'N', 'C', 'R', 'Y', 'P', 'T', 'E', 'D'};
	const uint8_t decryptedData[] = {'C', 'L', 'E', 'A', 'R', 'D', 'A', 'T', 'A', 'O', 'U', 'T'};
	uint8_t *pOpaqueData = NULL;

	// The actual data transfer happens in shared memory for VGDRM,
	// so data in/out is just expected to contain the size of the data
	uint32_t expectedDataIn = sizeof(payloadData);
	uint32_t expectedDataOut = sizeof(decryptedData);

	// Check the call to decrypt on the DrmSession leads to a opencdm_session_decrypt call
	// with the correct parameters (IV & payload provided here and key taken from the helper)
	mock("OpenCDM").expectOneCall("opencdm_session_decrypt")
			.withPointerParameter("session", mOcdmSession)
			.withMemoryBufferParameter("encrypted", (unsigned char*)&expectedDataIn, sizeof(expectedDataIn))
			.withOutputParameterReturning("encrypted", &expectedDataOut, sizeof(expectedDataOut))
			.withIntParameter("encryptedLength", sizeof(expectedDataIn))
			.withPointerParameter("iv", (void*)testIv)
			.withIntParameter("ivLength", sizeof(testIv))
			.withMemoryBufferParameter("keyId",  (unsigned char*)&expectedKeyId[0], expectedKeyId.size())
			.withIntParameter("keyIdLength", expectedKeyId.size())
			.andReturnValue(0);

	LONGS_EQUAL(0, drmSession->decrypt(testIv, sizeof(testIv), payloadData, sizeof(payloadData), &pOpaqueData));
	mock().checkExpectations();
}

TEST(AampDrmSessionTests, TestDecryptFromHlsOcdmBridgeNoKey)
{
	DrmInfo drmInfo;
	drmInfo.keyURI = "81701500000810367b131dd025ab0a7bd8d20c1314151600";
	std::shared_ptr<AampVgdrmHelper> drmHelper = std::make_shared<AampVgdrmHelper>(drmInfo, &mLogging);

	setupChallengeCallbacksForExternalLicense();

	shared_ptr<AampHlsOcdmBridge> hlsOcdmBridge = createBridgeForHelper(drmHelper);

	uint8_t payloadData[] = {'E', 'N', 'C', 'R', 'Y', 'P', 'T', 'E', 'D'};

	// Key not ready, (SetDecryptInfo not called on bridge)
	// so opencdm_session_decrypt should not be called and an error status should be returned
	mock("OpenCDM").expectNoCall("opencdm_session_decrypt");
	LONGS_EQUAL(eDRM_ERROR, hlsOcdmBridge->Decrypt(PROFILE_BUCKET_DECRYPT_VIDEO, payloadData, sizeof(payloadData)));
	mock().checkExpectations();
}

TEST(AampDrmSessionTests, TestDecryptFromHlsOcdmBridge)
{
	DrmInfo drmInfo;
	drmInfo.keyURI = "81701500000810367b131dd025ab0a7bd8d20c1314151600";
	std::string testIv = "TESTIV0123456789";
	drmInfo.iv = (unsigned char*)testIv.c_str();

	std::shared_ptr<AampVgdrmHelper> drmHelper = std::make_shared<AampVgdrmHelper>(drmInfo, &mLogging);
	std::vector<uint8_t> expectedKeyId;
	drmHelper->getKey(expectedKeyId);
	LONGS_EQUAL(16, expectedKeyId.size());

	setupChallengeCallbacksForExternalLicense();

	shared_ptr<AampHlsOcdmBridge> hlsOcdmBridge = createBridgeForHelper(drmHelper);

	uint8_t payloadData[] = {'E', 'N', 'C', 'R', 'Y', 'P', 'T', 'E', 'D'};
	const uint8_t decryptedData[] = {'C', 'L', 'E', 'A', 'R', 'D', 'A', 'T', 'A', 'O', 'U', 'T'};

	hlsOcdmBridge->SetDecryptInfo(mAamp, &drmInfo);

	// The actual data transfer happens in shared memory for VGDRM,
	// so data in/out is just expected to contain the size of the data
	uint32_t expectedDataIn = sizeof(payloadData);
	uint32_t expectedDataOut = sizeof(decryptedData);

	// Check the call to decrypt on the HlsOcdmBridge leads to a opencdm_session_decrypt call
	// with the correct parameters (IV from SetDecryptInfo, payload provided here and key taken from the helper)
	mock("OpenCDM").expectOneCall("opencdm_session_decrypt")
			.withPointerParameter("session", mOcdmSession)
			.withMemoryBufferParameter("encrypted", (unsigned char*)&expectedDataIn, sizeof(expectedDataIn))
			.withOutputParameterReturning("encrypted", &expectedDataOut, sizeof(expectedDataOut))
			.withIntParameter("encryptedLength", sizeof(expectedDataIn))
			.withPointerParameter("iv", (void*)drmInfo.iv)
			.withIntParameter("ivLength", testIv.size())
			.withMemoryBufferParameter("keyId",  (unsigned char*)&expectedKeyId[0], expectedKeyId.size())
			.withIntParameter("keyIdLength", expectedKeyId.size())
			.andReturnValue(0);

	LONGS_EQUAL(eDRM_SUCCESS, hlsOcdmBridge->Decrypt(PROFILE_BUCKET_DECRYPT_VIDEO, payloadData, sizeof(payloadData)));
	mock().checkExpectations();
}

TEST(AampDrmSessionTests, TestClearKeyLicenseAcquisition)
{
	// Setup Curl and OpenCDM mocks. We expect that curl_easy_perform will be called to fetch
	// the key and that OpenCDM will be called to construct the session and handle the fetched key.
	std::string testKeyData = "TESTKEYDATA";
	setupCurlPerformResponse(testKeyData);
	setupChallengeCallbacks();

	// Begin test
	DrmInfo drmInfo;
	drmInfo.method = eMETHOD_AES_128;
	drmInfo.manifestURL = "http://example.com/assets/test.m3u8";
	drmInfo.keyURI = "file.key";
	std::shared_ptr<AampClearKeyHelper> drmHelper = std::make_shared<AampClearKeyHelper>(drmInfo, &mLogging);

	// We expect the key data to be transformed by the helper before being passed to opencdm_session_update.
	// Thus we call the helper ourselves here (with the data our mock Curl will return) so we know what to expect.
	// Note: testing of the transformation is done in the helper tests, here we just want to ensure that whatever
	// the helper returns is what OpenCDM gets.
	const shared_ptr<DrmData> expectedDrmData = make_shared<DrmData>((unsigned char *)testKeyData.c_str(), testKeyData.size());
	drmHelper->transformLicenseResponse(expectedDrmData);

	mock("OpenCDM").expectOneCall("opencdm_session_update")
			.withMemoryBufferParameter("keyMessage", (const unsigned char*) expectedDrmData->getData().c_str(), (size_t)expectedDrmData->getDataLength())
			.withIntParameter("keyLength", expectedDrmData->getDataLength())
			.andReturnValue(0);

	AampDrmSession *drmSession = createDrmSessionForHelper(drmHelper, "org.w3.clearkey");
	STRCMP_EQUAL("org.w3.clearkey", drmSession->getKeySystem().c_str());

	const MockCurlOpts *curlOpts = MockCurlGetOpts();
	// Key URL should been constructed based on manifestURL and keyURI from the DrmInfo
	STRCMP_EQUAL("http://example.com/assets/file.key", curlOpts->url);
	// POST is used
	LONGS_EQUAL(0L, curlOpts->httpGet);
}

TEST(AampDrmSessionTests, TestOcdmCreateSystemFailure)
{
	DrmInfo drmInfo;
	drmInfo.keyURI = "81701500000810367b131dd025ab0a7bd8d20c1314151600";
	std::shared_ptr<AampVgdrmHelper> drmHelper = std::make_shared<AampVgdrmHelper>(drmInfo, &mLogging);
	DrmMetaDataEventPtr event = createDrmMetaDataEvent();

	// Causing opencdm_create_system to fail - we should not go on to call opencdm_construct_session
	mOcdmSystem = nullptr;
	mock("OpenCDM").expectOneCall("opencdm_create_system")
			.withParameter("keySystem", "net.vgdrm")
			.andReturnValue(mOcdmSystem);
	mock("OpenCDM").expectNoCall("opencdm_construct_session");
	AampDrmSession *drmSession = getSessionManager()->createDrmSession(drmHelper, event, mAamp, eMEDIATYPE_VIDEO);
	mock().checkExpectations();
}

TEST(AampDrmSessionTests, TestOcdmConstructSessionFailure)
{
	DrmInfo drmInfo;
	drmInfo.keyURI = "81701500000810367b131dd025ab0a7bd8d20c1314151600";
	std::shared_ptr<AampVgdrmHelper> drmHelper = std::make_shared<AampVgdrmHelper>(drmInfo, &mLogging);
	DrmMetaDataEventPtr event = createDrmMetaDataEvent();

	AampDRMSessionManager *sessionManager = getSessionManager();

	mock("OpenCDM").expectOneCall("opencdm_create_system")
			.withParameter("keySystem", "net.vgdrm")
			.andReturnValue(mOcdmSystem);

	// Causing opencdm_construct_session to fail - createDrmSession should return a NULL session
	mOcdmSession = nullptr;
	mock("OpenCDM").expectOneCall("opencdm_construct_session")
			.withOutputParameterReturning("session", &mOcdmSession, sizeof(mOcdmSession))
			.andReturnValue((int)ERROR_KEYSYSTEM_NOT_SUPPORTED);
	AampDrmSession *drmSession = sessionManager->createDrmSession(drmHelper, event, mAamp, eMEDIATYPE_VIDEO);
	mock().checkExpectations();
	CHECK_TEXT(drmSession == NULL, "Session creation unexpectedly succeeded");
}

TEST(AampDrmSessionTests, TestMultipleSessionsDifferentKey)
{
	std::string testKeyData = "TESTKEYDATA";
	setupCurlPerformResponse(testKeyData);
	setupChallengeCallbacks();

	DrmInfo drmInfo;
	drmInfo.keyURI = "81701500000810367b131dd025ab0a7bd8d20c1314151600";
	std::shared_ptr<AampVgdrmHelper> drmHelper1 = std::make_shared<AampVgdrmHelper>(drmInfo, &mLogging);

	const shared_ptr<DrmData> expectedDrmData = make_shared<DrmData>((unsigned char *)testKeyData.c_str(), testKeyData.size());
	drmHelper1->transformLicenseResponse(expectedDrmData);

	setupChallengeCallbacksForExternalLicense();

	// 1st time around - expecting standard session creation
	AampDrmSession *drmSession1 = createDrmSessionForHelper(drmHelper1, "net.vgdrm");
	STRCMP_EQUAL("net.vgdrm", drmSession1->getKeySystem().c_str());

	// 2nd time around - expecting another new session to be created
	drmInfo.keyURI = "81701500000811367b131dd025ab0a7bd8d20c1314151600";
	std::shared_ptr<AampVgdrmHelper> drmHelper2 = std::make_shared<AampVgdrmHelper>(drmInfo, &mLogging);
	AampDrmSession *drmSession2 = createDrmSessionForHelper(drmHelper2, "net.vgdrm");
	STRCMP_EQUAL("net.vgdrm", drmSession2->getKeySystem().c_str());
	CHECK_FALSE(drmSession1 == drmSession2);
}

TEST(AampDrmSessionTests, TestMultipleSessionsSameKey)
{
	std::string testKeyData = "TESTKEYDATA";
	setupCurlPerformResponse(testKeyData);
	setupChallengeCallbacks();

	DrmInfo drmInfo;
	drmInfo.method = eMETHOD_AES_128;
	drmInfo.manifestURL = "http://example.com/assets/test.m3u8";
	drmInfo.keyURI = "file.key";
	std::shared_ptr<AampClearKeyHelper> drmHelper = std::make_shared<AampClearKeyHelper>(drmInfo, &mLogging);

	const shared_ptr<DrmData> expectedDrmData = make_shared<DrmData>((unsigned char *)testKeyData.c_str(), testKeyData.size());
	drmHelper->transformLicenseResponse(expectedDrmData);

	// Only 1 session update called expected, since a single session should be shared
	mock("OpenCDM").expectOneCall("opencdm_session_update")
			.withMemoryBufferParameter("keyMessage", (const unsigned char *)expectedDrmData->getData().c_str(), (size_t)expectedDrmData->getDataLength())
			.withIntParameter("keyLength", expectedDrmData->getDataLength())
			.andReturnValue(0);

	// 1st time around - expecting standard session creation
	AampDrmSession *drmSession1 = createDrmSessionForHelper(drmHelper, "org.w3.clearkey");
	STRCMP_EQUAL("org.w3.clearkey", drmSession1->getKeySystem().c_str());

	// 2nd time around - expecting the existing session will be shared, so no OCDM session created
	AampDRMSessionManager *sessionManager = getSessionManager();
	DrmMetaDataEventPtr event = createDrmMetaDataEvent();
	mock("OpenCDM").expectNoCall("opencdm_create_system");
	mock("OpenCDM").expectNoCall("opencdm_construct_session");
	AampDrmSession *drmSession2 = sessionManager->createDrmSession(drmHelper, event, mAamp, eMEDIATYPE_VIDEO);
	CHECK_EQUAL(drmSession1, drmSession2);

	// Clear out the sessions. Now a new OCDM session is expected again
	sessionManager->clearSessionData();

	mock("OpenCDM").expectOneCall("opencdm_session_update")
			.withMemoryBufferParameter("keyMessage", (const unsigned char *)expectedDrmData->getData().c_str(), (size_t)expectedDrmData->getDataLength())
			.withIntParameter("keyLength", expectedDrmData->getDataLength())
			.andReturnValue(0);

	AampDrmSession *drmSession3 = createDrmSessionForHelper(drmHelper, "org.w3.clearkey");
	STRCMP_EQUAL("org.w3.clearkey", drmSession3->getKeySystem().c_str());
}

TEST(AampDrmSessionTests, TestDashPlayReadySession)
{
	string prLicenseServerURL = "http://licenseserver.example/license";
	const std::string testKeyData = "TESTKEYDATA";
	const std::string testChallengeData = "PLAYREADY_CHALLENGE_DATA";

	// Setting a PlayReady license server URL in the global config. This
	// should get used to request the license
	gpGlobalConfig->SetConfigValue<std::string>(AAMP_APPLICATION_SETTING, eAAMPConfig_PRLicenseServerUrl, prLicenseServerURL);

	// Setting up Curl callbacks for the session token and license requests
	setupCurlPerformResponses({
		{mSessionTokenUrl, mSessionTokenResponse},
		{prLicenseServerURL, testKeyData}
	});

	setupChallengeCallbacks(MockChallengeData("playready.example", testChallengeData));

	// PSSH string which will get passed to the helper for parsing, so needs to be in valid format
	const std::string psshStr =
			"<WRMHEADER xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" version=\"4.0.0.0\">"
			"<DATA>"
			"<KID>16bytebase64enckeydata==</KID>"
			"</DATA>"
            "</WRMHEADER>";

	DrmMetaDataEventPtr event = createDrmMetaDataEvent();
	AampDrmSession *drmSession = createDashDrmSession(testKeyData, psshStr, event);
	STRCMP_EQUAL("com.microsoft.playready", drmSession->getKeySystem().c_str());

	const TestCurlResponse *licenseResponse = getCurlPerformResponse(prLicenseServerURL);
	CHECK_EQUAL(1, licenseResponse->callCount);

	// Check the post data set on Curl, this should be the challenge data returned by OCDM
	CHECK_EQUAL(testChallengeData.size(), licenseResponse->opts.postFieldSize);
	CHECK_EQUAL(testChallengeData, licenseResponse->opts.postFields);
}

TEST(AampDrmSessionTests, TestDashPlayReadySessionNoCkmPolicy)
{
	string prLicenseServerURL = "http://licenseserver.example/license";
	std::string testKeyData = "TESTKEYDATA";

	// Setting a PlayReady license server URL in the global config. This
	// should get used to request the license
	gpGlobalConfig->SetConfigValue<std::string>(AAMP_APPLICATION_SETTING, eAAMPConfig_PRLicenseServerUrl, prLicenseServerURL);

	// Setting up Curl callbacks for the session token and license requests
	setupCurlPerformResponses({
		{mSessionTokenUrl, mSessionTokenResponse},
		{prLicenseServerURL, testKeyData}
	});

	setupChallengeCallbacks();

	// PSSH string which will get passed to the helper for parsing, so needs to be in valid format
	const std::string psshStr =
			"<WRMHEADER xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" version=\"4.0.0.0\">"
			"<DATA>"
			"<KID>16bytebase64enckeydata==</KID>"
			"</DATA>"
            "</WRMHEADER>";

	DrmMetaDataEventPtr event = createDrmMetaDataEvent();
	AampDrmSession *drmSession = createDashDrmSession(testKeyData, psshStr, event);
	CHECK_TEXT(drmSession != NULL, "Session creation failed");
	STRCMP_EQUAL("com.microsoft.playready", drmSession->getKeySystem().c_str());

	const MockCurlOpts *curlOpts = MockCurlGetOpts();

	// Check license URL from the global config was used
	std::string url;
	CHECK_TRUE(gpGlobalConfig->GetConfigValue(eAAMPConfig_PRLicenseServerUrl, url));
	STRCMP_EQUAL(url.c_str(), curlOpts->url);

	// Check the post data set on Curl. Since we didn't pass any metadata (ckm:policy) in the init data,
	// we expect the challenge data returned by OCDM to just be used as-is
	CHECK_TRUE(curlOpts->postFieldSize > 0);
	STRNCMP_EQUAL("OCDM_CHALLENGE_DATA", curlOpts->postFields, curlOpts->postFieldSize);
}

TEST(AampDrmSessionTests, TestSessionBadChallenge)
{
	DrmInfo drmInfo;
	drmInfo.method = eMETHOD_AES_128;
	drmInfo.manifestURL = "http://example.com/assets/test.m3u8";
	drmInfo.keyURI = "file.key";
	std::shared_ptr<AampClearKeyHelper> drmHelper = std::make_shared<AampClearKeyHelper>(drmInfo, &mLogging);

	// Cause OpenCDM to return an empty challenge. This should cause an error
	setupChallengeCallbacks(MockChallengeData("", ""));

	mock("OpenCDM").expectOneCall("opencdm_create_system")
			.withParameter("keySystem", "org.w3.clearkey")
			.andReturnValue(mOcdmSystem);

	mock("OpenCDM").expectOneCall("opencdm_construct_session")
			.withOutputParameterReturning("session", &mOcdmSession, sizeof(mOcdmSession))
			.andReturnValue(0);

	DrmMetaDataEventPtr event = createDrmMetaDataEvent();
	AampDrmSession *drmSession = getSessionManager()->createDrmSession(drmHelper, event, mAamp, eMEDIATYPE_VIDEO);
	POINTERS_EQUAL(NULL, drmSession);
	CHECK_EQUAL(AAMP_TUNE_DRM_CHALLENGE_FAILED, event->getFailure());
}

TEST(AampDrmSessionTests, TestSessionBadLicenseResponse)
{
	DrmInfo drmInfo;
	drmInfo.method = eMETHOD_AES_128;
	drmInfo.manifestURL = "http://example.com/assets/test.m3u8";
	drmInfo.keyURI = "file.key";
	std::shared_ptr<AampClearKeyHelper> drmHelper = std::make_shared<AampClearKeyHelper>(drmInfo, &mLogging);

	// Make curl return empty data for the key. This should cause an error
	setupCurlPerformResponses({
		{"http://example.com/assets/file.key", ""}
	});

	mock("OpenCDM").expectOneCall("opencdm_create_system")
			.withParameter("keySystem", "org.w3.clearkey")
			.andReturnValue(mOcdmSystem);

	mock("OpenCDM").expectOneCall("opencdm_construct_session")
			.withOutputParameterReturning("session", &mOcdmSession, sizeof(mOcdmSession))
			.andReturnValue(0);
	setupChallengeCallbacks();

	DrmMetaDataEventPtr event = createDrmMetaDataEvent();
	AampDrmSession *drmSession = getSessionManager()->createDrmSession(drmHelper, event, mAamp, eMEDIATYPE_VIDEO);
	POINTERS_EQUAL(NULL, drmSession);
	CHECK_EQUAL(AAMP_TUNE_LICENCE_REQUEST_FAILED, event->getFailure());
}

TEST(AampDrmSessionTests, TestDashSessionBadPssh)
{
	std::string testKeyData = "TESTKEYDATA";

	// Pass a bad PSSH string. This should cause an error
	const std::string psshStr = "bad data with no KID";

	DrmMetaDataEventPtr event = createDrmMetaDataEvent();
	AampDrmSession *drmSession = getSessionManager()->createDrmSession("9a04f079-9840-4286-ab92-e65be0885f95", eMEDIAFORMAT_DASH,
					(const unsigned char*)psshStr.c_str(), psshStr.length(), eMEDIATYPE_VIDEO, mAamp, event);
	POINTERS_EQUAL(NULL, drmSession);
	CHECK_EQUAL(AAMP_TUNE_CORRUPT_DRM_METADATA, event->getFailure());
}

TEST(AampDrmSessionTests, TestDrmMessageCallback)
{
	DrmInfo drmInfo;
	drmInfo.keyURI = "81701500000810367b131dd025ab0a7bd8d20c1314151600";
	std::shared_ptr<AampVgdrmHelper> drmHelper = std::make_shared<AampVgdrmHelper>(drmInfo, &mLogging);
	setupChallengeCallbacksForExternalLicense();
	AampDrmSession *drmSession = createDrmSessionForHelper(drmHelper, "net.vgdrm");
	STRCMP_EQUAL("net.vgdrm", drmSession->getKeySystem().c_str());

	struct DrmMessageTestData
	{
		std::string prefix;
		std::string payload;
		bool callbackExpected;
	};

	const std::vector<DrmMessageTestData> testData = {
		// 'individualization-request' or '3' should be accepted
		{"individualization-request:Type:", "{\"watermark\":{\"display\":false}}", true},
		{"3:Type:", "{\"watermark\":{\"display\":false}}", true},
		{"individualization-request:Type:", "payload is opaque so could be anything", true},

		// Wrong/no type - shouldn't trigger callback, shouldn't cause a problem
		{"4:Type:", "{\"watermark\":{\"display\":false}}", false},
		{"", "just a random string", false},
	};

	for (const auto& testCase : testData)
	{
		const std::string challengeData = testCase.prefix + testCase.payload;

		if (testCase.callbackExpected)
		{
			mock("Aamp").expectOneCall("individualization").withStringParameter("payload", testCase.payload.c_str());
		}
		else
		{
			mock("Aamp").expectNoCall("individualization");
		}

		MockOpenCdmSessionInfo *cdmInfo = MockOpenCdmGetSessionInfo();
		cdmInfo->callbacks.process_challenge_callback((OpenCDMSession*)cdmInfo->session,
				cdmInfo->userData,
				"", // Empty URL, not needed for this callback
				(const uint8_t*)challengeData.data(),
				(const uint16_t)challengeData.size());
	}
}

#else /* USE_SECCLIENT */
TEST(AampDrmSessionTests, TestDashPlayReadySessionSecClient)
{
	std::string prLicenseServerURL = "http://licenseserver.example/license";
    std::string expectedServiceHostUrl = "licenseserver.example";

	// Setting a PlayReady license server URL in the global config. This
	// should get used to request the license
	gpGlobalConfig->SetConfigValue<std::string>(AAMP_APPLICATION_SETTING, eAAMPConfig_PRLicenseServerUrl, prLicenseServerURL);

	// Setting up Curl repsonse for the session token
	setupCurlPerformResponses({
		{mSessionTokenUrl, mSessionTokenResponse}
	});

	setupChallengeCallbacks();

	// PSSH string which will get passed to the helper for parsing, so needs to be in valid format
	const std::string psshStr =
			"<WRMHEADER xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" version=\"4.0.0.0\">"
			"<DATA>"
			"<KID>16bytebase64enckeydata==</KID>"
			"<ckm:policy xmlns:ckm=\"urn:ccp:ckm\">policy</ckm:policy>"
			"</DATA>"
            "</WRMHEADER>";

	std::string expectedKeyData = "TESTSECKEYDATA";
	size_t respLength = expectedKeyData.length();
	const char *respPtr = expectedKeyData.c_str();

	// The license should be acquired using the secure client, rather than curl
	mock("SecClient").expectOneCall("AcquireLicense")
			.withStringParameter("serviceHostUrl", expectedServiceHostUrl.c_str())
			.withOutputParameterReturning("licenseResponse", &respPtr, sizeof(respPtr))
			.withOutputParameterReturning("licenseResponseLength", &respLength, sizeof(respLength));

	DrmMetaDataEventPtr event = createDrmMetaDataEvent();
	AampDrmSession *drmSession = createDashDrmSession(expectedKeyData, psshStr, event);
	CHECK_TEXT(drmSession != NULL, "Session creation failed");
	STRCMP_EQUAL("com.microsoft.playready", drmSession->getKeySystem().c_str());
}
#endif /* USE_SECCLIENT */
