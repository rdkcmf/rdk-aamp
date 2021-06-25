#include <dlfcn.h>
#include <mutex>
#include <gst/gst.h>
#include "opencdmsessionadapter.h"

class AAMPOCDMGSTSessionAdapter : public AAMPOCDMSessionAdapter
{
public:
	AAMPOCDMGSTSessionAdapter(std::shared_ptr<AampDrmHelper> drmHelper) : AAMPOCDMSessionAdapter(drmHelper), AAMPOCDMGSTSessionDecrypt(nullptr)
	{
                const char* ocdmgstsessiondecrypt = "opencdm_gstreamer_session_decrypt_ex";
                AAMPOCDMGSTSessionDecrypt = (OpenCDMError(*)(struct OpenCDMSession*, GstBuffer*, GstBuffer*, const uint32_t, GstBuffer*, GstBuffer*, uint32_t, GstCaps*))dlsym(RTLD_DEFAULT, ocdmgstsessiondecrypt);
                if (AAMPOCDMGSTSessionDecrypt)
                        logprintf("Has opencdm_gstreamer_session_decrypt_ex");
                else
                        logprintf("No opencdm_gstreamer_session_decrypt_ex found");
	};
	~AAMPOCDMGSTSessionAdapter() {};

	int decrypt(GstBuffer* keyIDBuffer, GstBuffer* ivBuffer, GstBuffer* buffer, unsigned subSampleCount, GstBuffer* subSamplesBuffer, GstCaps* caps);
private:
        OpenCDMError(*AAMPOCDMGSTSessionDecrypt)(struct OpenCDMSession*, GstBuffer*, GstBuffer*, const uint32_t, GstBuffer*, GstBuffer*, uint32_t, GstCaps*);
};
