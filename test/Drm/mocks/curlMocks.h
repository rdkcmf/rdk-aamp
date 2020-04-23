

#ifdef __cplusplus
extern "C" {
#endif

typedef size_t (*MockCurlWriteCallback)(char *ptr, size_t size, size_t nmemb, void *userdata);
typedef void (*MockCurlPerformCallback)(CURL *curl, MockCurlWriteCallback writeFunction, void *writeData, void* userData);

#define MOCK_CURL_MAX_HEADERS (10u)

typedef struct _MockCurlOpts
{
	MockCurlWriteCallback	writeFunction;	/* CURLOPT_WRITEFUNCTION */
	void					*writeData;		/* CURLOPT_WRITEDATA */
	char					url[200];		/* CURLOPT_URL */
	long					httpGet;		/* CURLOPT_HTTPGET */
	long					postFieldSize;	/* CURLOPT_POSTFIELDSIZE */
	char					postFields[500];/* CURLOPT_POSTFIELDS */
	char					headers[MOCK_CURL_MAX_HEADERS][200]; /* CURLOPT_HTTPHEADERS */
	unsigned int				headerCount;
} MockCurlOpts;

void MockCurlSetPerformCallback(MockCurlPerformCallback mockPerformCallback, void* userData);

void MockCurlReset(void);

const MockCurlOpts* MockCurlGetOpts(void);

#ifdef __cplusplus
}
#endif
