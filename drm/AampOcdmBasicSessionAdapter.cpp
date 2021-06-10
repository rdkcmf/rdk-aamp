#include "AampOcdmBasicSessionAdapter.h"
#include "AampMutex.h"

int AAMPOCDMBasicSessionAdapter::decrypt(const uint8_t *f_pbIV, uint32_t f_cbIV, const uint8_t *payloadData,
										 uint32_t payloadDataSize, uint8_t **ppOpaqueData)
{
	if (!verifyOutputProtection())
	{
		return HDCP_COMPLIANCE_CHECK_FAILURE;
	}

	AampMutexHold decryptMutexHold(decryptMutex);

	uint8_t *dataToSend = const_cast<uint8_t *>(payloadData);
	uint32_t sizeToSend = payloadDataSize;
	std::vector<uint8_t> vdata;

	if (m_drmHelper->getMemorySystem() != nullptr)
	{
		if (!m_drmHelper->getMemorySystem()->encode(payloadData, payloadDataSize, vdata))
		{
			AAMPLOG_WARN("Failed to encode memory for transmission");
			return -1;
		}
		sizeToSend = vdata.size();
		dataToSend = vdata.data();
	}

	int retvalue = opencdm_session_decrypt(m_pOpenCDMSession,
										   dataToSend,
										   sizeToSend,
										   f_pbIV, f_cbIV,
										   m_keyId.data(), m_keyId.size());
	if (retvalue != 0)
	{
		if (m_drmHelper->getMemorySystem() != nullptr)
		{
			m_drmHelper->getMemorySystem()->terminateEarly();
		}
		AAMPLOG_INFO("decrypt returned : %d", retvalue);
	}
	else if (m_drmHelper->getMemorySystem() != nullptr)
	{
		if (!m_drmHelper->getMemorySystem()->decode(dataToSend, sizeToSend, const_cast<uint8_t*>(payloadData), payloadDataSize))
		{
			AAMPLOG_WARN("Failed to decode memory for transmission");
			return -1;
		}
	}	
	return retvalue;
}
