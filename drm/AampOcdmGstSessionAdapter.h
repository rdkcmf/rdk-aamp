#include <dlfcn.h>
#include <mutex>
#include <gst/gst.h>
#include "opencdmsessionadapter.h"

class AAMPOCDMGSTSessionAdapter : public AAMPOCDMSessionAdapter
{
#ifdef AMLOGIC
        void ExtractSEI( GstBuffer *buffer);
#endif
public:
	AAMPOCDMGSTSessionAdapter(AampLogManager *logObj, std::shared_ptr<AampDrmHelper> drmHelper) : AAMPOCDMSessionAdapter(logObj, drmHelper)
#if (defined(AMLOGIC) || defined(REALTEKCE))
, AAMPOCDMGSTSessionDecrypt(nullptr)
	{
                const char* ocdmgstsessiondecrypt = "opencdm_gstreamer_session_decrypt_ex";
                AAMPOCDMGSTSessionDecrypt = (OpenCDMError(*)(struct OpenCDMSession*, GstBuffer*, GstBuffer*, const uint32_t, GstBuffer*, GstBuffer*, uint32_t, GstCaps*))dlsym(RTLD_DEFAULT, ocdmgstsessiondecrypt);
                if (AAMPOCDMGSTSessionDecrypt)
                        AAMPLOG_INFO("Has opencdm_gstreamer_session_decrypt_ex");
                else
                        AAMPLOG_WARN("No opencdm_gstreamer_session_decrypt_ex found");
#else
	{
#endif
	};
	~AAMPOCDMGSTSessionAdapter() {};

	int decrypt(GstBuffer* keyIDBuffer, GstBuffer* ivBuffer, GstBuffer* buffer, unsigned subSampleCount, GstBuffer* subSamplesBuffer, GstCaps* caps);
private:
#if (defined(AMLOGIC) || defined(REALTEKCE))
        OpenCDMError(*AAMPOCDMGSTSessionDecrypt)(struct OpenCDMSession*, GstBuffer*, GstBuffer*, const uint32_t, GstBuffer*, GstBuffer*, uint32_t, GstCaps*);
#endif
};
