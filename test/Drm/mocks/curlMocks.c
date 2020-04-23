#include <curl/curl.h>
#include <stdarg.h>
#include <string.h>

#include "curlMocks.h"

#undef curl_easy_setopt
#undef curl_easy_getinfo

typedef struct _MockCurlInstanceData
{
	/* Actual option values set by the client of curl */
	MockCurlOpts opts;

	/* A callback provided by the user of the mock, to be called on curl_easy_perform */
	MockCurlPerformCallback mockPerformCallback;
	void *mockPerformUserData;
} MockCurlInstanceData;

static MockCurlInstanceData f_mockInstance;

/* BEGIN - methods to access mock functionality */
void MockCurlSetPerformCallback(MockCurlPerformCallback mockPerformCallback, void* userData)
{
	f_mockInstance.mockPerformCallback = mockPerformCallback;
	f_mockInstance.mockPerformUserData = userData;
}

void MockCurlReset(void)
{
	memset(&f_mockInstance, 0, sizeof(f_mockInstance));
}

const MockCurlOpts* MockCurlGetOpts(void)
{
	return &f_mockInstance.opts;
}
/* END - methods to access mock functionality */

CURL *curl_easy_init(void)
{
	return NULL;
}

CURLcode curl_easy_setopt(CURL *curl, CURLoption option, ...)
{
	va_list arg;
	void *paramp;

	va_start(arg, option);
	paramp = va_arg(arg, void *);

	switch (option)
	{
		case CURLOPT_WRITEFUNCTION:
			f_mockInstance.opts.writeFunction = (MockCurlWriteCallback)paramp;
			break;

		case CURLOPT_WRITEDATA:
			f_mockInstance.opts.writeData = paramp;
			break;

		case CURLOPT_URL:
			strcpy(f_mockInstance.opts.url, paramp);
			break;

		case CURLOPT_HTTPGET:
			f_mockInstance.opts.httpGet = (long)paramp;
			break;

		case CURLOPT_POSTFIELDSIZE:
			f_mockInstance.opts.postFieldSize = (long)paramp;
			break;

		case CURLOPT_POSTFIELDS:
			strncpy(f_mockInstance.opts.postFields, paramp, f_mockInstance.opts.postFieldSize);
			f_mockInstance.opts.postFields[f_mockInstance.opts.postFieldSize] = '\0';
			break;
	}

	va_end(arg);
}

CURLcode curl_easy_perform(CURL *curl)
{
	if (f_mockInstance.mockPerformCallback)
	{
		f_mockInstance.mockPerformCallback(curl, f_mockInstance.opts.writeFunction, f_mockInstance.opts.writeData, f_mockInstance.mockPerformUserData);
	}
	return CURLE_OK;
}

CURLcode curl_easy_getinfo(CURL *curl, CURLINFO info, ...)
{
	va_list arg;
	void *paramp;

	va_start(arg, info);
	paramp = va_arg(arg, void *);

	switch (info)
	{
		case CURLINFO_RESPONSE_CODE:
			*((long*)paramp) = 200;
			break;
	}

	va_end(arg);
	return CURLE_OK;
}

const char *curl_easy_strerror(CURLcode code)
{
	return "";
}

void curl_easy_cleanup(CURL *curl)
{
}

struct curl_slist *curl_slist_append(struct curl_slist * list, const char * val)
{
	if (f_mockInstance.opts.headerCount < MOCK_CURL_MAX_HEADERS)
	{
		size_t dataSize = strlen(val);
		char *header = (char*)&f_mockInstance.opts.headers[f_mockInstance.opts.headerCount++];
		memcpy(header, val, dataSize);
	}
	return NULL;
}

void curl_slist_free_all(struct curl_slist * list)
{
	f_mockInstance.opts.headerCount = 0u;
}
