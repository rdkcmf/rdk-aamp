#include <iostream>
#include <iomanip>
#include <algorithm>
#include <unordered_map>
#include <memory>

#include "AampDRMutils.h"
#include "AampConfig.h"
#include "priv_aamp.h"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

//Enable the define below to get AAMP logging out when running tests
//#define ENABLE_LOGGING
#define TEST_LOG_LEVEL eLOGLEVEL_TRACE

std::shared_ptr<AampConfig> gGlobalConfig;
AampConfig *gpGlobalConfig;

static std::unordered_map<std::string, std::vector<std::string>> fCustomHeaders;

void MockAampReset(void)
{
	MemoryLeakWarningPlugin::turnOffNewDeleteOverloads();
	gGlobalConfig = std::make_shared<AampConfig>();
	MemoryLeakWarningPlugin::turnOnNewDeleteOverloads();

	gpGlobalConfig = gGlobalConfig.get();
}

long long aamp_GetCurrentTimeMS(void)
{
	return 0;
}

PrivateInstanceAAMP::PrivateInstanceAAMP() {}
PrivateInstanceAAMP::~PrivateInstanceAAMP() {}

void PrivateInstanceAAMP::GetCustomLicenseHeaders(std::unordered_map<std::string, std::vector<std::string>>& customHeaders)
{
	customHeaders = fCustomHeaders;
}

void PrivateInstanceAAMP::SendDrmErrorEvent(DrmMetaDataEventPtr event, bool isRetryEnabled)
{
}

void PrivateInstanceAAMP::SendDRMMetaData(DrmMetaDataEventPtr e)
{
}

void PrivateInstanceAAMP::individualization(const std::string& payload)
{
	mock("Aamp").actualCall("individualization").withStringParameter("payload", payload.c_str());
}

#ifdef USE_SECCLIENT
void PrivateInstanceAAMP::GetMoneyTraceString(std::string &customHeader) const
{
}
#endif

void aamp_Free(struct GrowableBuffer *buffer)
{
	if (buffer && buffer->ptr)
	{
		g_free(buffer->ptr);
		buffer->ptr  = NULL;
	}
}

void aamp_AppendBytes(struct GrowableBuffer *buffer, const void *ptr, size_t len)
{
	size_t required = buffer->len + len;
	if (required > buffer->avail)
	{
		if(buffer->avail > (128*1024))
		{
			AAMPLOG_WARN("WARNING - realloc. buf %p avail %d required %d",buffer, (int)buffer->avail, (int)required);
		}
		buffer->avail = required * 2; // grow generously to minimize realloc overhead
		char *ptr = (char *)g_realloc(buffer->ptr, buffer->avail);
		assert(ptr);
		if (ptr)
		{
			if (buffer->ptr == NULL)
			{ // first alloc (not a realloc)
			}
			buffer->ptr = ptr;
		}
	}
	if (buffer->ptr)
	{
		memcpy(&buffer->ptr[buffer->len], ptr, len);
		buffer->len = required;
	}
}

bool AampLogManager::isLogLevelAllowed(AAMP_LogLevel chkLevel)
{
	return chkLevel >= TEST_LOG_LEVEL;
}

std::string AampLogManager::getHexDebugStr(const std::vector<uint8_t>& data)
{
	std::ostringstream hexSs;
	hexSs << "0x";
	hexSs << std::hex << std::uppercase << std::setfill('0');
	std::for_each(data.cbegin(), data.cend(), [&](int c) { hexSs << std::setw(2) << c; });
	return hexSs.str();
}

void logprintf(const char *format, ...)
{
#ifdef ENABLE_LOGGING
	int len = 0;
	va_list args;
	va_start(args, format);

	char gDebugPrintBuffer[MAX_DEBUG_LOG_BUFF_SIZE];
	len = sprintf(gDebugPrintBuffer, "[AAMP-PLAYER]");
	vsnprintf(gDebugPrintBuffer+len, MAX_DEBUG_LOG_BUFF_SIZE-len, format, args);
	gDebugPrintBuffer[(MAX_DEBUG_LOG_BUFF_SIZE-1)] = 0;

	std::cout << gDebugPrintBuffer << std::endl;

	va_end(args);
#endif
}

void DumpBlob(const unsigned char *ptr, size_t len)
{
}
