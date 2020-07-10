#include "open_cdm.h"

#ifdef __cplusplus
extern "C" {
#endif
	typedef struct _MockSessionInfo
	{
		void *session;
		void *userData;	// User data from the client of OCDM
		OpenCDMSessionCallbacks callbacks;
	} MockOpenCdmSessionInfo;

	typedef void (*MockConstructSessionCallback)(const MockOpenCdmSessionInfo *session, void *mockUserData);
	typedef void (*MockSessionUpdateCallback)(const MockOpenCdmSessionInfo *session, const uint8_t keyMessage[], const uint16_t keyLength);

	typedef struct _MockOpenCdmCallbacks
	{
		MockConstructSessionCallback constructSessionCallback;
		MockSessionUpdateCallback	sessionUpdateCallback;
	} MockOpenCdmCallbacks;

	MockOpenCdmSessionInfo* MockOpenCdmGetSessionInfo();

	void MockOpenCdmSetCallbacks(MockOpenCdmCallbacks callbacks, void *mockUserData);

	void MockOpenCdmReset(void);

#ifdef __cplusplus
}
#endif
