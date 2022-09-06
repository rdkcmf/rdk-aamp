#include <vector>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iterator>

#include "AampConfig.h"

#include "AampDrmHelper.h"

#include "drmTestUtils.h"
#include "aampMocks.h"

#include "CppUTest/TestHarness.h"

TEST_GROUP(AampDrmHelperTests)
{
	struct CreateHelperTestData
	{
		DrmMethod method;
		MediaFormat mediaFormat;
		std::string uri;
		std::string keyFormat;
		std::string systemUUID;
		std::vector<uint8_t> expectedKeyPayload;
	};

	DrmInfo createDrmInfo(DrmMethod method,
						  MediaFormat mediaFormat,
						  const std::string& uri = "",
						  const std::string& keyFormat = "",
						  const std::string& systemUUID = "",
						  const std::string& initData = "")
	{
		DrmInfo drmInfo;

		drmInfo.method = method;
		drmInfo.mediaFormat = mediaFormat;
		drmInfo.keyURI = uri;
		drmInfo.keyFormat = keyFormat;
		drmInfo.systemUUID = systemUUID;
		drmInfo.initData = initData;

		return drmInfo;
	}

	void setup()
	{
		MockAampReset();
	}

	void teardown()
	{
		MockAampReset();
	}
};

TEST(AampDrmHelperTests, TestDrmIds)
{
	std::vector<std::string> expectedIds({
		"A68129D3-575B-4F1A-9CBA-3223846CF7C3", // VGDRM
		"1077efec-c0b2-4d02-ace3-3c1e52e2fb4b", // ClearKey
		"edef8ba9-79d6-4ace-a3c8-27dcd51d21ed", // Widevine
		"9a04f079-9840-4286-ab92-e65be0885f95"  // PlayReady
	});
	std::sort(expectedIds.begin(), expectedIds.end());

	std::vector<std::string> actualIds;
	AampDrmHelperEngine::getInstance().getSystemIds(actualIds);
	std::sort(actualIds.begin(), actualIds.end());

	CHECK_EQUAL(expectedIds, actualIds);
}

TEST(AampDrmHelperTests, TestCreateVgdrmHelper)
{
	const std::vector<CreateHelperTestData> testData = {
		// Invalid URI but valid KEYFORMAT
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS,
		 "91701500000810367b131dd025ab0a7bd8d20c1314151600",
		 "A68129D3-575B-4F1A-9CBA-3223846CF7C3",
		 "",
		 {}},

		// Invalid URI but valid KEYFORMAT
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS,
		 "91701500000810367b131dd025ab0a7bd8d20c1314151600",
		 "A68129D3-575B-4F1A-9CBA-3223846CF7C3",
		 "",
		 {}},

		// Valid 48 char URI and KEYFORMAT
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS,
		 "81701500000810367b131dd025ab0a7bd8d20c1314151600",
		 "A68129D3-575B-4F1A-9CBA-3223846CF7C3",
		 "",
		 {0x36, 0x7b, 0x13, 0x1d, 0xd0, 0x25, 0xab, 0x0a, 0x7b, 0xd8, 0xd2, 0x0c, 0x13, 0x14, 0x15, 0x16}},

		// Valid 48 char URI and KEYFORMAT. No METHOD (not required to pick up the VGDRM helper)
		{eMETHOD_NONE,
		 eMEDIAFORMAT_HLS,
		 "81701500000810367b131dd025ab0a7bd8d20c1314151600",
		 "A68129D3-575B-4F1A-9CBA-3223846CF7C3",
		 "",
		 {0x36, 0x7b, 0x13, 0x1d, 0xd0, 0x25, 0xab, 0x0a, 0x7b, 0xd8, 0xd2, 0x0c, 0x13, 0x14, 0x15, 0x16}},

		// Valid 48 char URI, no KEYFORMAT
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS,
		 "81701500000810367b131dd025ab0a7bd8d20c1314151600",
		 "",
		 "",
		 {0x36, 0x7b, 0x13, 0x1d, 0xd0, 0x25, 0xab, 0x0a, 0x7b, 0xd8, 0xd2, 0x0c, 0x13, 0x14, 0x15, 0x16}},

		// Valid 40 char URI, no KEYFORMAT
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS,
		 "8170110000080c367b131dd025ab0a7bd8d20c00",
		 "",
		 "",
		 {0x36, 0x7b, 0x13, 0x1d, 0xd0, 0x25, 0xab, 0x0a, 0x7b, 0xd8, 0xd2, 0x0c}},

		// Valid 48 char URI, uppercase
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS,
		 "81701500000810367B131DD025AB0A7BD8D20C1314151600",
		 "",
		 "",
		 {0x36, 0x7b, 0x13, 0x1d, 0xd0, 0x25, 0xab, 0x0a, 0x7b, 0xd8, 0xd2, 0x0c, 0x13, 0x14, 0x15, 0x16}},

		// Valid 40 char URI, uppercase
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS,
		 "8170110000080C367B131DD025AB0A7BD8D20C00",
		 "",
		 "",
		 {0x36, 0x7b, 0x13, 0x1d, 0xd0, 0x25, 0xab, 0x0a, 0x7b, 0xd8, 0xd2, 0x0c}},

		// 48 char URI specifies maximum key length possible without going off the end of the string
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS,
		 "81701500000811367b131dd025ab0a7bd8d20c1314151600",
		 "A68129D3-575B-4F1A-9CBA-3223846CF7C3",
		 "",
		 {0x36, 0x7b, 0x13, 0x1d, 0xd0, 0x25, 0xab, 0x0a, 0x7b, 0xd8, 0xd2, 0x0c, 0x13, 0x14, 0x15, 0x16, 0x0}},

		// 48 char URI specifies key length which will just take us off the end of the string.
		// Creation should pass but empty key returned
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS,
		 "81701500000812367b131dd025ab0a7bd8d20c1314151600",
		 "A68129D3-575B-4F1A-9CBA-3223846CF7C3",
		 "",
		 {}},

		// 40 char URI specifies maximum key length possible without going off the end of the string
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS,
		 "8170110000080d367b131dd025ab0a7bd8d20c00",
		 "A68129D3-575B-4F1A-9CBA-3223846CF7C3",
		 "",
		 {0x36, 0x7b, 0x13, 0x1d, 0xd0, 0x25, 0xab, 0x0a, 0x7b, 0xd8, 0xd2, 0x0c, 0x0}},

		// 40 char URI specifies key length which will just take us off the end of the string.
		// Creation should pass but empty key returned
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS,
		 "8170110000080e367b131dd025ab0a7bd8d20c00",
		 "A68129D3-575B-4F1A-9CBA-3223846CF7C3",
		 "",
		 {}},

		// Textual identifier
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS,
		 "",
		 "net.vgdrm",
		 "",
		 {}}
	};
	DrmInfo drmInfo;

	for (auto& test_data: testData)
	{
		std::vector<uint8_t> keyID;

		drmInfo = createDrmInfo(test_data.method, test_data.mediaFormat, test_data.uri, test_data.keyFormat, test_data.systemUUID);

		CHECK_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));

		std::shared_ptr<AampDrmHelper> vgdrmHelper = AampDrmHelperEngine::getInstance().createHelper(drmInfo);
		CHECK_TEXT(vgdrmHelper != NULL, "VGDRM helper creation failed");
		CHECK_EQUAL("net.vgdrm", vgdrmHelper->ocdmSystemId());
		CHECK_EQUAL(true, vgdrmHelper->isClearDecrypt());
		CHECK_EQUAL(true, vgdrmHelper->isHdcp22Required());
		CHECK_EQUAL(4, vgdrmHelper->getDrmCodecType());
		CHECK_EQUAL(true, vgdrmHelper->isExternalLicense());
		CHECK_EQUAL(10000U, vgdrmHelper->licenseGenerateTimeout());
		CHECK_EQUAL(10000U, vgdrmHelper->keyProcessTimeout());

		vgdrmHelper->getKey(keyID);

		if (test_data.expectedKeyPayload.size() != 0) {
			std::shared_ptr<std::vector<uint8_t>> keyData;
			CHECK_EQUAL(test_data.expectedKeyPayload , keyID);
		}
	}
}

TEST(AampDrmHelperTests, TestCreateVgdrmHelperNegative)
{
	// Note: using METHOD_NONE on each of the below to avoid picking up
	// the default helper for AES_128 (ClearKey). The positive test proves
	// that we can create a VGDRM helper with METHOD_NONE, providing the
	// other criteria are matched
	const std::vector<CreateHelperTestData> testData = {
		// Start of URI is valid, but payload has a non hex value at start
		{eMETHOD_NONE,
		 eMEDIAFORMAT_HLS,
		 "8170110000080CZ67B331DD025AB0A7BD8D20C00",
		 "",
		 "",
		 {}},

		// Start of URI is valid, but payload has a non hex value in middle
		{eMETHOD_NONE,
		 eMEDIAFORMAT_HLS,
		 "8170110000080C367B331DD025AZ0A7BD8D20C00",
		 "",
		 "",
		 {}},

		// Invalid URI and KEYFORMAT
		{eMETHOD_NONE,
		 eMEDIAFORMAT_HLS,
		 "BAD0110000080c367b131dd025ab0a7bd8d20c00",
		 "BAD129D3-575B-4F1A-9CBA-3223846CF7C3",
		 "",
		 {}}, /* Since the URI data is bad we won't get a payload */

		// Invalid URI and KEYFORMAT (lower-case)
		{eMETHOD_NONE,
		 eMEDIAFORMAT_HLS,
		 "BAD0110000080c367b131dd025ab0a7bd8d20c00",
		 "a68129d3-575b-4f1a-9cba-3223846cf7c3",
		 "",
		 {}}, /* Since the URI data is bad we won't get a payload */

		// Invalid URI, no KEYFORMAT
		{eMETHOD_NONE,
		 eMEDIAFORMAT_HLS,
		 "BAD0110000080c367b131dd025ab0a7bd8d20c00",
		 "",
		 "",
		 {}},

		// Start of URI is valid, but it's 1 character short
		{eMETHOD_NONE,
		 eMEDIAFORMAT_HLS,
		 "8170110000080c367b131dd025ab0a7bd8d20c0",
		 "",
		 "",
		 {}},

		// Start of URI is valid, but it's 1 character too long
		{eMETHOD_NONE,
		 eMEDIAFORMAT_HLS,
		 "8170110000080c367b131dd025ab0a7bd8d20c000",
		 "",
		 "",
		 {}}
	};
	DrmInfo drmInfo;

	for (auto& test_data: testData)
	{
		drmInfo = createDrmInfo(test_data.method, test_data.mediaFormat, test_data.uri, test_data.keyFormat, test_data.systemUUID);

		CHECK_FALSE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));

		std::shared_ptr<AampDrmHelper> vgdrmHelper = AampDrmHelperEngine::getInstance().createHelper(drmInfo);
		CHECK(vgdrmHelper == NULL);
	}
}

TEST(AampDrmHelperTests, TestVgdrmHelperInitDataCreation)
{
	std::vector<DrmInfo> drmInfoList;

	drmInfoList.push_back(createDrmInfo(eMETHOD_AES_128,
										eMEDIAFORMAT_HLS,
										"8170110000080c367b131dd025ab0a7bd8d20c00",
										"A68129D3-575B-4F1A-9CBA-3223846CF7C3",
										 "",
										 "somebase64initdata"));

	// Extra base 64 characters at start and end. Shouldn't cause any issue
	drmInfoList.push_back(createDrmInfo(eMETHOD_AES_128,
										eMEDIAFORMAT_HLS,
										"8170110000080c367b131dd025ab0a7bd8d20c00",
										"A68129D3-575B-4F1A-9CBA-3223846CF7C3",
										 "",
										 "+/=somebase64initdata+/="));

	for (const DrmInfo& drmInfo : drmInfoList)
	{
		std::shared_ptr<AampDrmHelper> vgdrmHelper = AampDrmHelperEngine::getInstance().createHelper(drmInfo);
		CHECK_TEXT(vgdrmHelper != NULL, "VGDRM helper creation failed");

		std::vector<uint8_t> initData;
		vgdrmHelper->createInitData(initData);

		TestUtilJsonWrapper jsonWrapper(initData);
		cJSON *initDataObj = jsonWrapper.getJsonObj();
		CHECK_TEXT(initDataObj != NULL, "Invalid JSON init data");

		CHECK_JSON_STR_VALUE(initDataObj, "initData", drmInfo.initData.c_str());
		CHECK_JSON_STR_VALUE(initDataObj, "uri", drmInfo.keyURI.c_str());

		// Currently pssh won't be present
		POINTERS_EQUAL(NULL, cJSON_GetObjectItem(initDataObj, "pssh"));
	}
}

TEST(AampDrmHelperTests, TestVgdrmHelperGenerateLicenseRequest)
{
	DrmInfo drmInfo = createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_HLS, "81701500000810367b131dd025ab0a7bd8d20c1314151600");
	drmInfo.manifestURL = "http://example.com/hls/playlist.m3u8";
	CHECK_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<AampDrmHelper> clearKeyHelper = AampDrmHelperEngine::getInstance().createHelper(drmInfo);

	AampChallengeInfo challengeInfo;
	AampLicenseRequest licenseRequest;
	clearKeyHelper->generateLicenseRequest(challengeInfo, licenseRequest);

	CHECK_EQUAL(AampLicenseRequest::DRM_RETRIEVE, licenseRequest.method);
	CHECK_EQUAL("", licenseRequest.url);
	CHECK_EQUAL("", licenseRequest.payload);
}

TEST(AampDrmHelperTests, TestCreateClearKeyHelper)
{
	const std::vector<CreateHelperTestData> testData = {
		// Valid KEYFORMAT, HLS
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS_MP4,
		 "file.key",
		 "urn:uuid:1077efec-c0b2-4d02-ace3-3c1e52e2fb4b",
		 "1077efec-c0b2-4d02-ace3-3c1e52e2fb4b",
		 {'1'}},

		// Valid KEYFORMAT, DASH
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_DASH,
		 "file.key",
		 "urn:uuid:1077efec-c0b2-4d02-ace3-3c1e52e2fb4b",
		 "1077efec-c0b2-4d02-ace3-3c1e52e2fb4b",
		 {}}, // For DASH, the key should come from the PSSH, so we won't check that here

		// Textual identifier rather than UUID
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_HLS_MP4,
		 "file.key",
		 "org.w3.clearkey",
		 "1077efec-c0b2-4d02-ace3-3c1e52e2fb4b",
		 {'1'}},

	};
	DrmInfo drmInfo;

	for (auto& test_data: testData)
	{
		drmInfo = createDrmInfo(eMETHOD_AES_128, test_data.mediaFormat, test_data.uri, test_data.keyFormat, test_data.systemUUID);

		CHECK_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));

		std::shared_ptr<AampDrmHelper> clearKeyHelper = AampDrmHelperEngine::getInstance().createHelper(drmInfo);
		CHECK_TEXT(clearKeyHelper != NULL, "ClearKey helper creation failed");
		CHECK_EQUAL("org.w3.clearkey", clearKeyHelper->ocdmSystemId());
		CHECK_EQUAL(true, clearKeyHelper->isClearDecrypt());
		CHECK_EQUAL(false, clearKeyHelper->isHdcp22Required());
		CHECK_EQUAL(0, clearKeyHelper->getDrmCodecType());
		CHECK_EQUAL(false, clearKeyHelper->isExternalLicense());
		CHECK_EQUAL(5000U, clearKeyHelper->licenseGenerateTimeout());
		CHECK_EQUAL(5000U, clearKeyHelper->keyProcessTimeout());

		if (test_data.expectedKeyPayload.size() != 0)
		{
			std::vector<uint8_t> keyID;
			clearKeyHelper->getKey(keyID);
			CHECK_EQUAL(test_data.expectedKeyPayload , keyID);
		}
	}
}

TEST(AampDrmHelperTests, TestClearKeyHelperHlsInitDataCreation)
{
	DrmInfo drmInfo = createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_HLS_MP4, "file.key", "urn:uuid:1077efec-c0b2-4d02-ace3-3c1e52e2fb4b");
	CHECK_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<AampDrmHelper> clearKeyHelper = AampDrmHelperEngine::getInstance().createHelper(drmInfo);

	std::vector<uint8_t> initData;
	clearKeyHelper->createInitData(initData);

	TestUtilJsonWrapper jsonWrapper(initData);
	cJSON *initDataObj = jsonWrapper.getJsonObj();
	CHECK_TEXT(initDataObj != NULL, "Invalid JSON init data");

	// kids
	cJSON *kidsArray = cJSON_GetObjectItem(initDataObj, "kids");
	CHECK_TEXT(kidsArray != NULL, "Missing kids in JSON init data");
	CHECK_EQUAL(1, cJSON_GetArraySize(kidsArray));
	cJSON *kids0 = cJSON_GetArrayItem(kidsArray, 0);
	STRCMP_EQUAL("1", cJSON_GetStringValue(kids0));
}

TEST(AampDrmHelperTests, TestClearKeyHelperParsePssh)
{
	DrmInfo drmInfo = createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_DASH, "file.key", "urn:uuid:1077efec-c0b2-4d02-ace3-3c1e52e2fb4b");
	CHECK_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<AampDrmHelper> clearKeyHelper = AampDrmHelperEngine::getInstance().createHelper(drmInfo);

	// For DASH the init data should have come from the PSSH, so when asked to create
	// the init data, the helper should just return that
	std::vector<uint8_t> psshData = {
		0x00, 0x00, 0x00, 0x34, 0x70, 0x73, 0x73, 0x68, 0x01, 0x00, 0x00, 0x00, 0x10, 0x77, 0xef, 0xec,
		0xc0, 0xb2, 0x4d, 0x02, 0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb, 0x4b, 0x00, 0x00, 0x00, 0x01,
		0xfe, 0xed, 0xf0, 0x0d, 0xee, 0xde, 0xad, 0xbe, 0xef, 0xf0, 0xba, 0xad, 0xf0, 0x0d, 0xd0, 0x0d,
		0x00, 0x00, 0x00, 0x00};

	CHECK_TRUE(clearKeyHelper->parsePssh(psshData.data(), psshData.size()));

	std::vector<uint8_t> initData;
	clearKeyHelper->createInitData(initData);
	CHECK_EQUAL_TEXT(psshData, initData, "Init data does not match PSSH data");

	// KeyId should have been extracted from the PSSH
	std::vector<uint8_t> keyID;
	const std::vector<uint8_t> expectedKeyID = {0xfe, 0xed, 0xf0, 0x0d, 0xee, 0xde, 0xad, 0xbe, 0xef, 0xf0, 0xba, 0xad, 0xf0, 0x0d, 0xd0, 0x0d};
	clearKeyHelper->getKey(keyID);
	CHECK_EQUAL(expectedKeyID, keyID);
}

TEST(AampDrmHelperTests, TestClearKeyHelperGenerateLicenseRequest)
{
	DrmInfo drmInfo = createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_HLS_MP4, "file.key", "urn:uuid:1077efec-c0b2-4d02-ace3-3c1e52e2fb4b");
	drmInfo.manifestURL = "http://stream.example/hls/playlist.m3u8";
	CHECK_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<AampDrmHelper> clearKeyHelper = AampDrmHelperEngine::getInstance().createHelper(drmInfo);

	AampChallengeInfo challengeInfo;
	challengeInfo.url = "http://challengeinfourl.example";
	AampLicenseRequest licenseRequest;

	// No ClearKey license URL in the license request, expect the URL to be
	// constructed from the information in the DrmInfo
	clearKeyHelper->generateLicenseRequest(challengeInfo, licenseRequest);
	CHECK_EQUAL(AampLicenseRequest::POST, licenseRequest.method);
	CHECK_EQUAL("http://stream.example/hls/file.key", licenseRequest.url);
	CHECK_EQUAL("", licenseRequest.payload);

	// Setting a ClearKey license URL in the license request, expect
	// that to take precedence
	const std::string fixedCkLicenseUrl = "http://cklicenseserver.example";
    licenseRequest.url = fixedCkLicenseUrl;
	clearKeyHelper->generateLicenseRequest(challengeInfo, licenseRequest);
	CHECK_EQUAL(fixedCkLicenseUrl, licenseRequest.url);

	// Clearing ClearKey license URL in the license request and creating a
	// helper with no key URI in the DrmInfo. Should use the URI from the challenge
    licenseRequest.url.clear();
	DrmInfo drmInfoNoKeyUri = createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_HLS_MP4, "", "urn:uuid:1077efec-c0b2-4d02-ace3-3c1e52e2fb4b");
	std::shared_ptr<AampDrmHelper> clearKeyHelperNoKeyUri = AampDrmHelperEngine::getInstance().createHelper(drmInfoNoKeyUri);
	clearKeyHelperNoKeyUri->generateLicenseRequest(challengeInfo, licenseRequest);
	CHECK_EQUAL(challengeInfo.url, licenseRequest.url);
}

TEST(AampDrmHelperTests, TestClearKeyHelperTransformHlsLicenseResponse)
{
	struct TransformLicenseResponseTestData
	{
		std::vector<uint8_t> keyResponse;
		std::string expectedEncodedKey;
	};

	DrmInfo drmInfo = createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_HLS_MP4, "file.key", "urn:uuid:1077efec-c0b2-4d02-ace3-3c1e52e2fb4b");
	CHECK_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<AampDrmHelper> clearKeyHelper = AampDrmHelperEngine::getInstance().createHelper(drmInfo);

	const std::vector<TransformLicenseResponseTestData> testData {
		// Empty response - should lead to empty string
		{{}, {""}},
		// Most basic case - 1 maps to AQ
		{{0x1}, {"AQ"}},
		// Should lead to a string containing every possible base64url character
		{{0x00, 0x10, 0x83, 0x10, 0x51, 0x87, 0x20, 0x92, 0x8b, 0x30, 0xd3, 0x8f, 0x41, 0x14,
		  0x93, 0x51, 0x55, 0x97, 0x61, 0x96, 0x9b, 0x71, 0xd7, 0x9f, 0x82, 0x18, 0xa3, 0x92,
		  0x59, 0xa7, 0xa2, 0x9a, 0xab, 0xb2, 0xdb, 0xaf, 0xc3, 0x1c, 0xb3, 0xd3, 0x5d, 0xb7,
		  0xe3, 0x9e, 0xbb, 0xf3, 0xdf, 0xbf}, {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"}}
	};

	for (auto& testCase : testData)
	{
		std::shared_ptr<DrmData> drmData = std::make_shared<DrmData>((unsigned char *)&testCase.keyResponse[0], testCase.keyResponse.size());
		clearKeyHelper->transformLicenseResponse(drmData);

		TestUtilJsonWrapper jsonWrapper((const char*)drmData->getData().c_str(), drmData->getDataLength());
		cJSON *responseObj = jsonWrapper.getJsonObj();
		CHECK_TEXT(responseObj != NULL, "Invalid license response data");

		// keys array
		cJSON *keysArray = cJSON_GetObjectItem(responseObj, "keys");
		CHECK_TEXT(keysArray != NULL, "Missing keys in JSON response data");
		CHECK_EQUAL(1, cJSON_GetArraySize(keysArray));
		cJSON *keys0Obj = cJSON_GetArrayItem(keysArray, 0);
		CHECK_JSON_STR_VALUE(keys0Obj, "alg", "cbc");
		CHECK_JSON_STR_VALUE(keys0Obj, "k", testCase.expectedEncodedKey.c_str());
		CHECK_JSON_STR_VALUE(keys0Obj, "kid", "MQ"); // Expecting the character '1' (0x31) base64url encoded
	}
}

TEST(AampDrmHelperTests, TestTransformDashLicenseResponse)
{
	// Unlike HLS (where we do expect a ClearKey license to be transformed),
	// for DASH we expect the response to be given back untouched
	std::vector<DrmInfo> drmInfoList;

	// ClearKey
	drmInfoList.push_back(createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_DASH, "file.key", "urn:uuid:1077efec-c0b2-4d02-ace3-3c1e52e2fb4b"));

	// PlayReady
	drmInfoList.push_back(createDrmInfo(eMETHOD_NONE, eMEDIAFORMAT_DASH, "", "", "9a04f079-9840-4286-ab92-e65be0885f95"));

	for (auto& drmInfo : drmInfoList)
	{
		CHECK_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));
		std::shared_ptr<AampDrmHelper> clearKeyHelper = AampDrmHelperEngine::getInstance().createHelper(drmInfo);
		unsigned char licenseResponse[] = {'D', 'A', 'S', 'H', 'L', 'I', 'C'};
		std::shared_ptr<DrmData> drmData = std::make_shared<DrmData>(licenseResponse, sizeof(licenseResponse));

		clearKeyHelper->transformLicenseResponse(drmData);
		CHECK_EQUAL_TEXT(sizeof(licenseResponse), drmData->getDataLength(), "Unexpected change in license response size");
		MEMCMP_EQUAL_TEXT(licenseResponse, drmData->getData().c_str(), sizeof(licenseResponse), "Unexpected transformation of license response");
	}
}

TEST(AampDrmHelperTests, TestCreatePlayReadyHelper)
{
	const std::vector<CreateHelperTestData> testData = {
		// Valid UUID
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_DASH, // Note: PlayReady helper currently supports DASH only
		 "file.key",
		 "",
		 "9a04f079-9840-4286-ab92-e65be0885f95",
		 {}}, // For DASH, the key should come from the PSSH, so we won't check that here

		// Valid UUID, no method (method not required)
		{eMETHOD_NONE,
		 eMEDIAFORMAT_DASH, // Note: PlayReady helper currently supports DASH only
		 "file.key",
		 "",
		 "9a04f079-9840-4286-ab92-e65be0885f95",
		 {}}, // For DASH, the key should come from the PSSH, so we won't check that here

		// Textual identifier rather than UUID
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_DASH, // Note: PlayReady helper currently supports DASH only
		 "file.key",
		 "com.microsoft.playready",
		 "",
		 {}} // For DASH, the key should come from the PSSH, so we won't check that here
	};
	DrmInfo drmInfo;

	for (auto& test_data: testData)
	{
		drmInfo = createDrmInfo(test_data.method, test_data.mediaFormat, test_data.uri, test_data.keyFormat, test_data.systemUUID);

		CHECK_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));

		std::shared_ptr<AampDrmHelper> playReadyHelper = AampDrmHelperEngine::getInstance().createHelper(drmInfo);
		CHECK_TEXT(playReadyHelper != NULL, "PlayReady helper creation failed");
		CHECK_EQUAL("com.microsoft.playready", playReadyHelper->ocdmSystemId());
		CHECK_EQUAL(false, playReadyHelper->isClearDecrypt());
		CHECK_EQUAL(eDRM_PlayReady, playReadyHelper->getDrmCodecType());
		CHECK_EQUAL(false, playReadyHelper->isExternalLicense());
		CHECK_EQUAL(5000U, playReadyHelper->licenseGenerateTimeout());
		CHECK_EQUAL(5000U, playReadyHelper->keyProcessTimeout());

		// TODO: HDCP checks
	}
}

TEST(AampDrmHelperTests, TestCreatePlayReadyHelperNegative)
{
	const std::vector<CreateHelperTestData> testData = {
		// Valid UUID but HLS media format, which isn't supported for the PlayReady helper
		{eMETHOD_NONE,
		 eMEDIAFORMAT_HLS,
		 "file.key",
		 "",
		 "9a04f079-9840-4286-ab92-e65be0885f95",
		 {}}
	};
	DrmInfo drmInfo;

	for (auto& test_data: testData)
	{
		drmInfo = createDrmInfo(test_data.method, test_data.mediaFormat, test_data.uri, test_data.keyFormat, test_data.systemUUID);

		CHECK_FALSE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));

		std::shared_ptr<AampDrmHelper> playReadyHelper = AampDrmHelperEngine::getInstance().createHelper(drmInfo);
		CHECK(playReadyHelper == NULL);
	}
}

TEST(AampDrmHelperTests, TestWidevineHelperParsePsshDrmMetaData)
{
	// For DASH the init data should have come from the PSSH, so when asked to create
	// the init data, the helper should just return that.
	std::vector<uint8_t> psshData = {
		0x00, 0x00, 0x00, 0x34, 0x70, 0x73, 0x73, 0x68, 0x00, 0x00, 0x00, 0x00, 0x10, 0x77, 0xef, 0xec,
		0xc0, 0xb2, 0x4d, 0x02, 0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb, 0x4b, 0x00, 0x00, 0x00, 0x12,
		0x12, 0x10, 0xfe, 0xed, 0xf0, 0x0d, 0xee, 0xde, 0xad, 0xbe, 0xef, 0xf0, 0xba, 0xad, 0xf0, 0x0d, 
		0xd0, 0x0d, 0x00, 0x00, 0x00, 0x00};
		
	DrmInfo drmInfo;

	drmInfo = createDrmInfo(eMETHOD_AES_128, eMEDIAFORMAT_DASH, "file.key", "", "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed");
	
	CHECK_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<AampDrmHelper> widevineHelper = AampDrmHelperEngine::getInstance().createHelper(drmInfo);
	CHECK_TEXT(widevineHelper != NULL, "Widevine helper creation failed");

	CHECK_TRUE(widevineHelper->parsePssh(psshData.data(), psshData.size()));

	std::vector<uint8_t> initData;
	widevineHelper->createInitData(initData);
	CHECK_EQUAL_TEXT(psshData, initData, "Init data does not match PSSH data");

	// KeyId should have been extracted from the PSSH
	std::vector<uint8_t> keyID;
	const std::vector<uint8_t> expectedKeyID = {0xfe, 0xed, 0xf0, 0x0d, 0xee, 0xde, 0xad, 0xbe, 0xef, 0xf0, 0xba, 0xad, 0xf0, 0x0d, 0xd0, 0x0d};
	widevineHelper->getKey(keyID);
	CHECK_EQUAL(expectedKeyID, keyID);

	std::string contentMetadata;
	contentMetadata = widevineHelper->getDrmMetaData();
	CHECK_EQUAL("", contentMetadata);

	std::string metaData("content meta data");
	widevineHelper->setDrmMetaData(metaData);
	contentMetadata = widevineHelper->getDrmMetaData();
	CHECK_EQUAL(metaData, contentMetadata);
}


TEST(AampDrmHelperTests, TestCreateWidevineHelper)
{
	const std::vector<CreateHelperTestData> testData = {
		{eMETHOD_NONE,
		 eMEDIAFORMAT_DASH,
		 "file.key",
		 "",
		 "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed",
		 {}},
		 
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_DASH,
		 "file.key",
		 "",
		 "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed",
		 {}},

		// Textual identifier rather than UUID
		{eMETHOD_AES_128,
		 eMEDIAFORMAT_DASH,
		 "file.key",
		 "com.widevine.alpha",
		 "",
		 {}}
	};
	DrmInfo drmInfo;

	for (auto& test_data: testData)
	{
		drmInfo = createDrmInfo(test_data.method, test_data.mediaFormat, test_data.uri, test_data.keyFormat, test_data.systemUUID);

		CHECK_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));

		std::shared_ptr<AampDrmHelper> widevineHelper = AampDrmHelperEngine::getInstance().createHelper(drmInfo);
		CHECK_TEXT(widevineHelper != NULL, "Widevine helper creation failed");
		CHECK_EQUAL("com.widevine.alpha", widevineHelper->ocdmSystemId());
		CHECK_EQUAL(false, widevineHelper->isClearDecrypt());
		CHECK_EQUAL(false, widevineHelper->isHdcp22Required());
		CHECK_EQUAL(eDRM_WideVine, widevineHelper->getDrmCodecType());
		CHECK_EQUAL(false, widevineHelper->isExternalLicense());
		CHECK_EQUAL(5000U, widevineHelper->licenseGenerateTimeout());
		CHECK_EQUAL(5000U, widevineHelper->keyProcessTimeout());
	}

}


TEST(AampDrmHelperTests, TestCreateWidevineHelperNegative)
{
	const std::vector<CreateHelperTestData> testData = {
		// Valid UUID but HLS media format, which isn't supported for the Widevine helper
		{eMETHOD_NONE,
		 eMEDIAFORMAT_HLS,
		 "file.key",
		 "",
		 "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed",
		 {}}
	};
	DrmInfo drmInfo;

	for (auto& test_data: testData)
	{
		drmInfo = createDrmInfo(test_data.method, test_data.mediaFormat, test_data.uri, test_data.keyFormat, test_data.systemUUID);

		CHECK_FALSE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));

		std::shared_ptr<AampDrmHelper> widevineHelper = AampDrmHelperEngine::getInstance().createHelper(drmInfo);
		CHECK(widevineHelper == NULL);
	}
}

TEST(AampDrmHelperTests, TestPlayReadyHelperParsePssh)
{
	DrmInfo drmInfo = createDrmInfo(eMETHOD_NONE, eMEDIAFORMAT_DASH, "", "", "9a04f079-9840-4286-ab92-e65be0885f95");
	CHECK_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<AampDrmHelper> playReadyHelper = AampDrmHelperEngine::getInstance().createHelper(drmInfo);

	const std::string expectedMetadata = "testpolicydata";

	std::ostringstream psshSs;
	psshSs  << "<WRMHEADER xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" version=\"4.0.0.0\">"
			<< "<DATA>"
			<< "<KID>16bytebase64enckeydata==</KID>"
			<< "<ckm:policy xmlns:ckm=\"urn:ccp:ckm\">" << expectedMetadata << "</ckm:policy>"
			<< "</DATA>"
            << "</WRMHEADER>";
	const std::string psshStr = psshSs.str();

	CHECK_TRUE(playReadyHelper->parsePssh((const unsigned char*)psshStr.data(), psshStr.size()));

	// Check keyId and metadata, both of which should be based on the PSSH
	std::vector<uint8_t> keyId;
	playReadyHelper->getKey(keyId);

	const std::string expectedKeyId = "b5f2a6d7-dae6-eeb1-b87a-77247b275ab5";
	const std::string actualKeyId = std::string(keyId.begin(), keyId.begin() + keyId.size());

	CHECK_EQUAL(expectedKeyId, actualKeyId);
	CHECK_EQUAL(expectedMetadata, playReadyHelper->getDrmMetaData());
	// Ensure the helper doesn't set the meta data
	playReadyHelper->setDrmMetaData("content meta data that should be ignored");
	CHECK_EQUAL(expectedMetadata, playReadyHelper->getDrmMetaData());

	// Dodgy PSSH data should lead to false return value
	const std::string badPssh = "somerandomdatawhichisntevenxml";
	CHECK_FALSE(playReadyHelper->parsePssh((const unsigned char*)badPssh.data(), badPssh.size()));
}

TEST(AampDrmHelperTests, TestPlayReadyHelperParsePsshNoPolicy)
{
	// As before but with no ckm:policy in the PSSH data.
	// Should be OK but lead to empty metadata
	DrmInfo drmInfo = createDrmInfo(eMETHOD_NONE, eMEDIAFORMAT_DASH, "", "", "9a04f079-9840-4286-ab92-e65be0885f95");
	CHECK_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<AampDrmHelper> playReadyHelper = AampDrmHelperEngine::getInstance().createHelper(drmInfo);

	const std::string psshStr =
			"<WRMHEADER xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" version=\"4.0.0.0\">"
			"<DATA>"
			"<KID>16bytebase64enckeydata==</KID>"
			"</DATA>"
            "</WRMHEADER>";

	CHECK_TRUE(playReadyHelper->parsePssh((const unsigned char*)psshStr.data(), psshStr.size()));

	// Check keyId and metadata, both of which should be based on the PSSH
	std::vector<uint8_t> keyId;
	playReadyHelper->getKey(keyId);

	const std::string expectedKeyId = "b5f2a6d7-dae6-eeb1-b87a-77247b275ab5";
	const std::string actualKeyId = std::string(keyId.begin(), keyId.begin() + keyId.size());

	CHECK_EQUAL(expectedKeyId, actualKeyId);
	// Not expecting any metadata
	CHECK_EQUAL("", playReadyHelper->getDrmMetaData());
}

TEST(AampDrmHelperTests, TestPlayReadyHelperGenerateLicenseRequest)
{
	DrmInfo drmInfo = createDrmInfo(eMETHOD_NONE, eMEDIAFORMAT_DASH, "", "", "9a04f079-9840-4286-ab92-e65be0885f95");
	CHECK_TRUE(AampDrmHelperEngine::getInstance().hasDRM(drmInfo));
	std::shared_ptr<AampDrmHelper> playReadyHelper = AampDrmHelperEngine::getInstance().createHelper(drmInfo);

	AampChallengeInfo challengeInfo;
	challengeInfo.url = "http://challengeinfourl.example";
	std::string challengeData = "OCDM_CHALLENGE_DATA";

	challengeInfo.data = std::make_shared<DrmData>((unsigned char*)challengeData.data(), challengeData.length());
	challengeInfo.accessToken = "ACCESS_TOKEN";

	// No PSSH parsed. Expecting data from the provided challenge to be given back in the request info
	AampLicenseRequest licenseRequest1;
	playReadyHelper->generateLicenseRequest(challengeInfo, licenseRequest1);
	CHECK_EQUAL(challengeInfo.url, licenseRequest1.url);
	MEMCMP_EQUAL(challengeInfo.data->getData().c_str(), licenseRequest1.payload.data(), challengeInfo.data->getDataLength());

	// Parse a PSSH with a ckm:policy. This should cause generateLicenseRequest to return a JSON payload
	AampLicenseRequest licenseRequest2;
	const std::string psshStr =
			"<WRMHEADER xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" version=\"4.0.0.0\">"
			"<DATA>"
			"<KID>16bytebase64enckeydata==</KID>"
			"<ckm:policy xmlns:ckm=\"urn:ccp:ckm\">policy</ckm:policy>"
			"</DATA>"
            "</WRMHEADER>";
	CHECK_TRUE(playReadyHelper->parsePssh((const unsigned char*)psshStr.data(), psshStr.size()));

	playReadyHelper->generateLicenseRequest(challengeInfo, licenseRequest2);
	CHECK_EQUAL(challengeInfo.url, licenseRequest2.url);

	TestUtilJsonWrapper jsonWrapper(licenseRequest2.payload);
	cJSON *postFieldObj = jsonWrapper.getJsonObj();
	CHECK_TEXT(postFieldObj != NULL, "Invalid JSON challenge data");
	CHECK_JSON_STR_VALUE(postFieldObj, "keySystem", "playReady");
	CHECK_JSON_STR_VALUE(postFieldObj, "mediaUsage", "stream");
	// For the licenseRequest we expect the base64 encoding of the string
	// we placed in the challenge data: 'OCDM_CHALLENGE_DATA'
	CHECK_JSON_STR_VALUE(postFieldObj, "licenseRequest", "T0NETV9DSEFMTEVOR0VfREFUQQ==");
	CHECK_JSON_STR_VALUE(postFieldObj, "contentMetadata", "cG9saWN5");
	CHECK_JSON_STR_VALUE(postFieldObj, "accessToken", challengeInfo.accessToken.c_str());

	// Finally, checking the license uri override works
	AampLicenseRequest licenseRequest3;
	const std::string fixedPrLicenseUrl = "http://prlicenseserver.example";
	licenseRequest3.url = fixedPrLicenseUrl;

	playReadyHelper->generateLicenseRequest(challengeInfo, licenseRequest3);
	CHECK_EQUAL(fixedPrLicenseUrl, licenseRequest3.url);
}

TEST(AampDrmHelperTests, TestCompareHelpers)
{
	std::shared_ptr<AampDrmHelper> vgdrmHelper = AampDrmHelperEngine::getInstance().createHelper(createDrmInfo(
		eMETHOD_AES_128,
		eMEDIAFORMAT_HLS,
		"91701500000810367b131dd025ab0a7bd8d20c1314151600",
		"A68129D3-575B-4F1A-9CBA-3223846CF7C3"));
	CHECK_TRUE(vgdrmHelper != nullptr);

	std::shared_ptr<AampDrmHelper> playreadyHelper = AampDrmHelperEngine::getInstance().createHelper(createDrmInfo(
		eMETHOD_AES_128,
		eMEDIAFORMAT_DASH,
		"file.key",
		"",
		"9a04f079-9840-4286-ab92-e65be0885f95"));
	CHECK_TRUE(playreadyHelper != nullptr);

	std::shared_ptr<AampDrmHelper> widevineHelper = AampDrmHelperEngine::getInstance().createHelper(createDrmInfo(
		 eMETHOD_AES_128,
		 eMEDIAFORMAT_DASH,
		 "file.key",
		 "",
		 "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed"));
	CHECK_TRUE(widevineHelper != nullptr);

	std::shared_ptr<AampDrmHelper> clearKeyHelperHls = AampDrmHelperEngine::getInstance().createHelper(createDrmInfo(
		eMETHOD_AES_128,
		eMEDIAFORMAT_HLS_MP4,
		"file.key",
		"",
		"1077efec-c0b2-4d02-ace3-3c1e52e2fb4b"));
	CHECK_TRUE(clearKeyHelperHls != nullptr);

	std::shared_ptr<AampDrmHelper> clearKeyHelperDash = AampDrmHelperEngine::getInstance().createHelper(createDrmInfo(
		eMETHOD_AES_128,
		eMEDIAFORMAT_DASH,
		"file.key",
		"",
		"1077efec-c0b2-4d02-ace3-3c1e52e2fb4b"));
	CHECK_TRUE(clearKeyHelperDash != nullptr);

	// All helpers should equal themselves
	CHECK_TRUE(vgdrmHelper->compare(vgdrmHelper));
	CHECK_TRUE(widevineHelper->compare(widevineHelper));
	CHECK_TRUE(playreadyHelper->compare(playreadyHelper));
	CHECK_TRUE(clearKeyHelperHls->compare(clearKeyHelperHls));

	// Different helper types, should not equal
	CHECK_FALSE(vgdrmHelper->compare(playreadyHelper) || vgdrmHelper->compare(widevineHelper) || vgdrmHelper->compare(clearKeyHelperHls));
	CHECK_FALSE(playreadyHelper->compare(vgdrmHelper) || playreadyHelper->compare(widevineHelper) || playreadyHelper->compare(clearKeyHelperHls));
	CHECK_FALSE(widevineHelper->compare(vgdrmHelper) || widevineHelper->compare(playreadyHelper) || widevineHelper->compare(clearKeyHelperHls));

	// Same helper type but one is HLS and the other is DASH, so should not equal
	CHECK_FALSE(clearKeyHelperHls->compare(clearKeyHelperDash));

	// Comparison against null helper, should not equal, should not cause a problem
	std::shared_ptr<AampDrmHelper> nullHelper;
	CHECK_FALSE(clearKeyHelperHls->compare(nullHelper));

	std::shared_ptr<AampDrmHelper> vgdrmHelper2 = AampDrmHelperEngine::getInstance().createHelper(createDrmInfo(
		eMETHOD_AES_128,
		eMEDIAFORMAT_HLS,
		"91701500000810387b131dd025ab0a7bd8d20c1314151600", // Different key
		"A68129D3-575B-4F1A-9CBA-3223846CF7C3"));
	CHECK_TRUE(vgdrmHelper != nullptr);

	// Different key, should not equal
	CHECK_FALSE(vgdrmHelper->compare(vgdrmHelper2));

	std::shared_ptr<AampDrmHelper> playreadyHelper2 = AampDrmHelperEngine::getInstance().createHelper(createDrmInfo(
		eMETHOD_AES_128,
		eMEDIAFORMAT_DASH,
		"file.key",
		"",
		"9a04f079-9840-4286-ab92-e65be0885f95"));
	CHECK_TRUE(playreadyHelper2 != nullptr);

	const std::string pssh1 =
			"<WRMHEADER xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" version=\"4.0.0.0\">"
			"<DATA>"
			"<KID>16bytebase64enckeydata==</KID>"
			"</DATA>"
            "</WRMHEADER>";
	playreadyHelper->parsePssh((const unsigned char*)pssh1.data(), pssh1.size());
	playreadyHelper2->parsePssh((const unsigned char*)pssh1.data(), pssh1.size());

	// Same key in the PSSH data, should equal
	CHECK_TRUE(playreadyHelper->compare(playreadyHelper2));

	const std::string pssh2 =
			"<WRMHEADER xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" version=\"4.0.0.0\">"
			"<DATA>"
			"<KID>differentbase64keydata==</KID>"
			"</DATA>"
            "</WRMHEADER>";
	playreadyHelper2->parsePssh((const unsigned char*)pssh2.data(), pssh2.size());

	// Different key in the PSSH data, should not equal
	CHECK_FALSE(playreadyHelper->compare(playreadyHelper2));

	// Create another PR helper, same details as PR helper 1
	std::shared_ptr<AampDrmHelper> playreadyHelper3 = AampDrmHelperEngine::getInstance().createHelper(createDrmInfo(
		eMETHOD_AES_128,
		eMEDIAFORMAT_DASH,
		"file.key",
		"",
		"9a04f079-9840-4286-ab92-e65be0885f95"));
	CHECK_TRUE(playreadyHelper3 != nullptr);

	// But no PSSH parsed for the 3rd PR helper, so shouldn't be equal
	CHECK_FALSE(playreadyHelper->compare(playreadyHelper3));

	// Parse the same PSSH as used for 1, now should be equal
	playreadyHelper3->parsePssh((const unsigned char*)pssh1.data(), pssh1.size());
	CHECK_TRUE(playreadyHelper->compare(playreadyHelper3));

	// Finally keep the same key but add in metadata. Now PR helpers 1 & 3 shouldn't be equal
	const std::string pssh3 =
			"<WRMHEADER xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" version=\"4.0.0.0\">"
			"<DATA>"
			"<KID>16bytebase64enckeydata==</KID>"
			"<ckm:policy xmlns:ckm=\"urn:ccp:ckm\">policy</ckm:policy>"
			"</DATA>"
            "</WRMHEADER>";
	playreadyHelper3->parsePssh((const unsigned char*)pssh3.data(), pssh3.size());
	CHECK_FALSE(playreadyHelper->compare(playreadyHelper3));
}
