/**
 * @file AampOcdmGstSessionAdapter.h
 * @brief File holds operations on OCDM gst sessions
 */


#include <dlfcn.h>
#include <mutex>
#include <gst/gst.h>
#include "opencdmsessionadapter.h"


/**
 * @class AAMPOCDMGSTSessionAdapter
 * @brief OCDM Gstreamer session to decrypt
 */

class AAMPOCDMGSTSessionAdapter : public AAMPOCDMSessionAdapter
{
#ifdef AMLOGIC
        void ExtractSEI( GstBuffer *buffer);
#endif
public:
	AAMPOCDMGSTSessionAdapter(AampLogManager *logObj, std::shared_ptr<AampDrmHelper> drmHelper) : AAMPOCDMSessionAdapter(logObj, drmHelper)
#if defined(AMLOGIC)
, AAMPOCDMGSTSessionDecrypt(nullptr)
	{
                const char* ocdmgstsessiondecrypt = "opencdm_gstreamer_session_decrypt_ex";
                AAMPOCDMGSTSessionDecrypt = (OpenCDMError(*)(struct OpenCDMSession*, GstBuffer*, GstBuffer*, const uint32_t, GstBuffer*, GstBuffer*, uint32_t, GstCaps*))dlsym(RTLD_DEFAULT, ocdmgstsessiondecrypt);
                if (AAMPOCDMGSTSessionDecrypt)
                        logprintf("Has opencdm_gstreamer_session_decrypt_ex");
                else
                        logprintf("No opencdm_gstreamer_session_decrypt_ex found");
#else
	{
#endif
	};
	~AAMPOCDMGSTSessionAdapter() {};

	int decrypt(GstBuffer* keyIDBuffer, GstBuffer* ivBuffer, GstBuffer* buffer, unsigned subSampleCount, GstBuffer* subSamplesBuffer, GstCaps* caps);
	int decrypt(const uint8_t *f_pbIV, uint32_t f_cbIV, const uint8_t *payloadData, uint32_t payloadDataSize, uint8_t **ppOpaqueData);
private:
#if defined(AMLOGIC)
        OpenCDMError(*AAMPOCDMGSTSessionDecrypt)(struct OpenCDMSession*, GstBuffer*, GstBuffer*, const uint32_t, GstBuffer*, GstBuffer*, uint32_t, GstCaps*);
#endif
};
