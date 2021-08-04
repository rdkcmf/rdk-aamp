
#include <sys/time.h>
#include "AampOcdmGstSessionAdapter.h"

#define USEC_PER_SEC   1000000
static inline uint64_t GetCurrentTimeStampInUSec()
{
	struct timeval 	timeStamp;
	uint64_t 		retVal = 0;

	gettimeofday(&timeStamp, NULL);

	// Convert timestamp to Micro Seconds
	retVal = (uint64_t)(((uint64_t) timeStamp.tv_sec * USEC_PER_SEC) + timeStamp.tv_usec);

	return retVal;
}

static inline uint64_t GetCurrentTimeStampInMSec()
{
	return GetCurrentTimeStampInUSec() / 1000;
}

#define LOG_DECRYPT_STATS 1
#define DECRYPT_AVG_TIME_THRESHOLD 5.0 //5 milliseconds
#ifdef LOG_DECRYPT_STATS
#define MAX_THREADS 10
#define INTERVAL 120

/**
 * @struct DecryptStats
 * @brief Holds decryption profile stats
 */
struct DecryptStats
{
	uint64_t nBytesInterval;
	uint64_t nTimeInterval;
	uint64_t nBytesTotal;
	uint64_t nTimeTotal;
	uint64_t nCallsTotal;
	pthread_t threadID;

};
#endif // LOG_DECRYPT_STATS
#define SEC_SIZE size_t
void LogPerformanceExt(const char *strFunc, uint64_t msStart, uint64_t msEnd, SEC_SIZE nDataSize)
{
	uint64_t delta = msEnd - msStart;
	//CID: 107327,26,25,24,23 - Removed the unused initialized variables : bThreshold,nDataMin,nRestart,nRateMin,nTimeMin

#ifdef LOG_DECRYPT_STATS
	{
		static DecryptStats stats[MAX_THREADS] = { 0 };
		int idx = 0;
		while (idx < MAX_THREADS)
		{
			if (stats[idx].threadID == pthread_self())
			{
				break;
			}
			idx++;
		}
		if (idx == MAX_THREADS)
		{
			// new thread
			idx = 0;
			while (idx < MAX_THREADS)
			{
				if (stats[idx].threadID == 0)
				{
					// empty slot
					stats[idx].threadID = pthread_self();
					break;
				}
				idx++;
			}
		}
		if (idx == MAX_THREADS)
		{
			printf("%s >>>>>>>> All slots allocated!!!, idx = %d, clearing the array.\n", __FUNCTION__, idx);
			memset(stats, 0, sizeof(DecryptStats) * MAX_THREADS);
			return;
		}

		if (nDataSize > 0)
		{
			stats[idx].nBytesInterval += (uint64_t) nDataSize;
			stats[idx].nTimeInterval += delta;
			stats[idx].nCallsTotal++;

			if (stats[idx].nCallsTotal % INTERVAL == 0)
			{
				stats[idx].nBytesTotal += stats[idx].nBytesInterval;
				stats[idx].nTimeTotal += stats[idx].nTimeInterval;
				double avgTime = (double) stats[idx].nTimeTotal / (double) stats[idx].nCallsTotal;
				if (avgTime >= DECRYPT_AVG_TIME_THRESHOLD)
				{
					logprintf("%s >>>>>>>> Thread ID %X (%d) Avg Time %0.2llf ms, Avg Bytes %llu  calls (%llu) Interval avg time %0.2llf, Interval avg bytes %llu",
							strFunc, stats[idx].threadID, idx, avgTime, stats[idx].nBytesTotal / stats[idx].nCallsTotal, stats[idx].nCallsTotal, (double) stats[idx].nTimeInterval / (double) INTERVAL, stats[idx].nBytesInterval / INTERVAL);
				}
				stats[idx].nBytesInterval = 0;
				stats[idx].nTimeInterval = 0;

			}
		}
	}
#endif //LOG_DECRYPT_STATS
}

int AAMPOCDMGSTSessionAdapter::decrypt(GstBuffer *keyIDBuffer, GstBuffer *ivBuffer, GstBuffer *buffer, unsigned subSampleCount, GstBuffer *subSamplesBuffer, GstCaps* caps)
{
	int retValue = -1;

	if (m_pOpenCDMSession)
	{
		uint64_t start_decrypt_time;
		uint64_t end_decrypt_time;

		if (!verifyOutputProtection())
		{
			return HDCP_COMPLIANCE_CHECK_FAILURE;
		}

		pthread_mutex_lock(&decryptMutex);
		start_decrypt_time = GetCurrentTimeStampInMSec();

#if defined(AMLOGIC)
		if (AAMPOCDMGSTSessionDecrypt && !gst_caps_is_empty(caps))
			retValue = AAMPOCDMGSTSessionDecrypt(m_pOpenCDMSession, buffer, subSamplesBuffer, subSampleCount, ivBuffer, keyIDBuffer, 0, caps);
		else
#endif
			retValue = opencdm_gstreamer_session_decrypt(m_pOpenCDMSession, buffer, subSamplesBuffer, subSampleCount, ivBuffer, keyIDBuffer, 0);
		end_decrypt_time = GetCurrentTimeStampInMSec();
		if (retValue != 0)
		{
			GstMapInfo keyIDMap;
			if (gst_buffer_map(keyIDBuffer, &keyIDMap, (GstMapFlags) GST_MAP_READ) == true)
			{
				uint8_t *mappedKeyID = reinterpret_cast<uint8_t*>(keyIDMap.data);
				uint32_t mappedKeyIDSize = static_cast<uint32_t>(keyIDMap.size);
#ifdef USE_THUNDER_OCDM_API_0_2
				KeyStatus keyStatus = opencdm_session_status(m_pOpenCDMSession, mappedKeyID, mappedKeyIDSize);
#else
				KeyStatus keyStatus = opencdm_session_status(m_pOpenCDMSession, mappedKeyID,mappedKeyIDSize );
#endif
				AAMPLOG_INFO("AAMPOCDMSessionAdapter:%s : decrypt returned : %d key status is : %d", __FUNCTION__, retValue, keyStatus);
#ifdef USE_THUNDER_OCDM_API_0_2
				if (keyStatus == OutputRestricted){
#else
				if(keyStatus == KeyStatus::OutputRestricted){
#endif
					retValue = HDCP_OUTPUT_PROTECTION_FAILURE;
				}
#ifdef USE_THUNDER_OCDM_API_0_2
				else if (keyStatus == OutputRestrictedHDCP22){
#else
				else if(keyStatus == KeyStatus::OutputRestrictedHDCP22){
#endif
					retValue = HDCP_COMPLIANCE_CHECK_FAILURE;
				}
				gst_buffer_unmap(keyIDBuffer, &keyIDMap);
			}
		}

		GstMapInfo mapInfo;
		if (gst_buffer_map(buffer, &mapInfo, GST_MAP_READ))
		{
			if (mapInfo.size > 0)
			{
				LogPerformanceExt(__FUNCTION__, start_decrypt_time, end_decrypt_time, mapInfo.size);
			}
			gst_buffer_unmap(buffer, &mapInfo);
		}

		pthread_mutex_unlock(&decryptMutex);
	}
	return retValue;
}
