#include "opencdmsessionadapter.h"

class AAMPOCDMGSTSessionAdapter : public AAMPOCDMSessionAdapter
{
public:
	AAMPOCDMGSTSessionAdapter(std::shared_ptr<AampDrmHelper> drmHelper) : AAMPOCDMSessionAdapter(drmHelper) {};
	~AAMPOCDMGSTSessionAdapter() {};

	int decrypt(GstBuffer* keyIDBuffer, GstBuffer* ivBuffer, GstBuffer* buffer, unsigned subSampleCount, GstBuffer* subSamplesBuffer);
};
