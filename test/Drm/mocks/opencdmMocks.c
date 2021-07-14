#include <string.h>

#include "open_cdm.h"
#include <gst/gst.h>

#include "opencdmMocks.h"

#include "CppUTestExt/MockSupport_c.h"

typedef struct _MockOpenCdmInstanceData
{
	MockOpenCdmSessionInfo sessionInfo;
	MockOpenCdmCallbacks callbacks;
	void *mockUserData; // User data from the client of the mock
} MockOpenCdmInstanceData;

static MockOpenCdmInstanceData f_mockInstance;

/* BEGIN - methods to access mock functionality */
MockOpenCdmSessionInfo* MockOpenCdmGetSessionInfo()
{
	return &f_mockInstance.sessionInfo;
}

void MockOpenCdmSetCallbacks(MockOpenCdmCallbacks callbacks, void *mockUserData)
{
	f_mockInstance.callbacks = callbacks;
	f_mockInstance.mockUserData = mockUserData;
}

void MockOpenCdmReset(void)
{
	memset(&f_mockInstance, 0, sizeof(f_mockInstance));
}
/* END - methods to access mock functionality */

struct OpenCDMAccessor* opencdm_create_system()
{
	return mock_scope_c("OpenCDM")->actualCall("opencdm_create_system")
			->returnPointerValueOrDefault(0);
}

OpenCDMError opencdm_construct_session(struct OpenCDMAccessor* system, const char keySystem[],
									   const LicenseType licenseType, const char initDataType[],
									   const uint8_t initData[], const uint16_t initDataLength,
									   const uint8_t CDMData[], const uint16_t CDMDataLength,
									   OpenCDMSessionCallbacks* callbacks, void* userData,
									   struct OpenCDMSession** session)
{
	OpenCDMError retValue = mock_scope_c("OpenCDM")->actualCall("opencdm_construct_session")
			->withOutputParameter("session", session)
			->returnIntValueOrDefault(ERROR_NONE);

	f_mockInstance.sessionInfo.session = *session;
	f_mockInstance.sessionInfo.callbacks = *callbacks;
	f_mockInstance.sessionInfo.userData = userData;

	if (f_mockInstance.callbacks.constructSessionCallback)
	{
		f_mockInstance.callbacks.constructSessionCallback(&f_mockInstance.sessionInfo, f_mockInstance.mockUserData);
	}

	return retValue;
}

OpenCDMError opencdm_destruct_system(struct OpenCDMAccessor* system)
{
	return ERROR_NONE;
}

KeyStatus opencdm_session_status(const struct OpenCDMSession* session,
								 const uint8_t keyId[], const uint8_t length)
{
	return Usable;
}

OpenCDMError opencdm_session_update(struct OpenCDMSession* session,
									const uint8_t keyMessage[],
									const uint16_t keyLength)
{
	OpenCDMError retValue = mock_scope_c("OpenCDM")->actualCall("opencdm_session_update")
			->withMemoryBufferParameter("keyMessage", keyMessage, (size_t)keyLength)
			->withIntParameters("keyLength", keyLength)
			->returnIntValueOrDefault(ERROR_NONE);

	if (f_mockInstance.callbacks.sessionUpdateCallback)
	{
		f_mockInstance.callbacks.sessionUpdateCallback(&f_mockInstance.sessionInfo, keyMessage, keyLength);
	}

	return retValue;
}

OpenCDMError opencdm_gstreamer_session_decrypt_ex(struct OpenCDMSession* session, GstBuffer* buffer,
											   GstBuffer* subSample, const uint32_t subSampleCount,
											   GstBuffer* IV, GstBuffer* keyID, uint32_t initWithLast15, GstCaps *caps = NULL)
{
	return ERROR_NONE;
}

OpenCDMError opencdm_session_decrypt(struct OpenCDMSession* session,
									 uint8_t encrypted[],
									 const uint32_t encryptedLength,
									 const uint8_t* IV, uint16_t IVLength,
									 const uint8_t* keyId, const uint16_t keyIdLength,
									 uint32_t initWithLast15)
{
	return mock_scope_c("OpenCDM")->actualCall("opencdm_session_decrypt")
			->withPointerParameters("session", session)
			->withMemoryBufferParameter("encrypted", encrypted, encryptedLength)
			->withOutputParameter("encrypted", encrypted)
			->withIntParameters("encryptedLength", encryptedLength)
			->withPointerParameters("iv", (void*)IV)
			->withIntParameters("ivLength", IVLength)
			->withMemoryBufferParameter("keyId", keyId, keyIdLength)
			->withIntParameters("keyIdLength", keyIdLength)
			->intReturnValue();
}

OpenCDMError opencdm_session_close(struct OpenCDMSession* session)
{
	return ERROR_NONE;
}

OpenCDMError opencdm_destruct_session(struct OpenCDMSession* session)
{
	return ERROR_NONE;
}
