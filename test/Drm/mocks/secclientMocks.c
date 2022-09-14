#include <stddef.h>
#include "sec_client.h"

#include "CppUTestExt/MockSupport_c.h"

int32_t SecClient_AcquireLicense(const char *serviceHostUrl,
                                 uint8_t numberOfRequestMetadataKeys,
                                 const char *requestMetadata[][2],
                                 uint8_t numberOfAccessAttributes,
                                 const char *accessAttributes[][2],
                                 const char *contentMetadata, size_t contentMetadataLength,
                                 const char *licenseRequest, size_t licenseRequestLength,
                                 const char *keySystemId, const char *mediaUsage,
                                 const char *accessToken,
                                 char **licenseResponse, size_t *licenseResponseLength,
                                 uint32_t *refreshDurationSeconds,
                                 SecClient_ExtendedStatus *statusInfo)
{
    return mock_scope_c("SecClient")->actualCall("AcquireLicense")
            ->withStringParameters("serviceHostUrl", serviceHostUrl)
            ->withOutputParameter("licenseResponse", licenseResponse)
            ->withOutputParameter("licenseResponseLength", licenseResponseLength)
            ->returnIntValueOrDefault(0);
}

int32_t SecClient_FreeResource(const char *resource)
{
    return 0;
}
