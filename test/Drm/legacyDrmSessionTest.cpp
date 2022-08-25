#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <iterator>
#include <memory>
#include<cjson/cJSON.h>

#include "aampdrmsessionfactory.h"
#include "AampDRMSessionManager.h"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "aampMocks.h"
#include "curlMocks.h"

TEST_GROUP(AampLegacyDrmSessionTests)
{
	PrivateInstanceAAMP *mAamp;
	AampDRMSessionManager *sessionManager;
	AampLogManager mLogging;
	int mMaxDrmSessions = 1;

	AampDRMSessionManager* getSessionManager()
	{
		if (sessionManager == nullptr) 
		{
			sessionManager = new AampDRMSessionManager(&mLogging, mMaxDrmSessions);
		}

		return sessionManager;
	}

	void setupCurlPerformResponse(std::string response)
	{
		static string responseStr = response;

		MockCurlSetPerformCallback([](CURL *curl, MockCurlWriteCallback writeCallback, void* writeData, void* userData) {
			writeCallback((char*)responseStr.c_str(), 1, responseStr.size(), writeData);
		}, this);
	}

	void setup()
	{
		MockAampReset();
		MockCurlReset();
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
	}
};

TEST(AampLegacyDrmSessionTests, TestCreateClearkeySession)
{
	AampDRMSessionManager *sessionManager = getSessionManager();

	cJSON *keysObj = cJSON_CreateObject();
	cJSON *keyInstanceObj = cJSON_CreateObject();
	cJSON_AddStringToObject(keyInstanceObj, "alg", "cbc");
	cJSON_AddStringToObject(keyInstanceObj, "k", "_u3wDe7erb7v8Lqt8A3QDQ");
	cJSON_AddStringToObject(keyInstanceObj, "kid", "_u3wDe7erb7v8Lqt8A3QDQ");
	cJSON *keysArr = cJSON_AddArrayToObject(keysObj, "keys");
	cJSON_AddItemToArray(keysArr, keyInstanceObj);

	char *keyResponse = cJSON_PrintUnformatted(keysObj);
	setupCurlPerformResponse(keyResponse);
	cJSON_free(keyResponse);
	cJSON_Delete(keysObj);

	const unsigned char initData[] = {
		0x00, 0x00, 0x00, 0x34, 0x70, 0x73, 0x73, 0x68, 0x01, 0x00, 0x00, 0x00, 0x10, 0x77, 0xef, 0xec,
		0xc0, 0xb2, 0x4d, 0x02, 0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb, 0x4b, 0x00, 0x00, 0x00, 0x01,
		0xfe, 0xed, 0xf0, 0x0d, 0xee, 0xde, 0xad, 0xbe, 0xef, 0xf0, 0xba, 0xad, 0xf0, 0x0d, 0xd0, 0x0d,
		0x00, 0x00, 0x00, 0x00};

	DrmMetaDataEventPtr aampEvent = std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, false);
	AampDrmSession *drmSession = sessionManager->createDrmSession(
			"1077efec-c0b2-4d02-ace3-3c1e52e2fb4b",
			eMEDIAFORMAT_DASH,
			initData,
			sizeof(initData),
			eMEDIATYPE_VIDEO,
			mAamp,
			aampEvent,
			NULL,
			true);
	CHECK_TEXT(drmSession != NULL, "Session creation failed");
}
