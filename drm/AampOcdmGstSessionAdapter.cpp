/**
 * @file AampOcdmGstSessionAdapter.cpp
 * @brief File holds operations on OCDM gst sessions
 */

#include <sys/time.h>
#include "AampOcdmGstSessionAdapter.h"

#ifdef AMLOGIC
#include "gst/video/gstvideotimecode.h"
#include "gst/video/gstvideometa.h"
#endif

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
#define DECRYPT_AVG_TIME_THRESHOLD 10.0 //10 milliseconds
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

#ifdef AMLOGIC
class BitStreamState
{
	private:
		const guint8 *ptr;
		gsize bit_offset;
	public:
		BitStreamState(const BitStreamState &L):
		ptr(L.ptr),
		bit_offset(L.bit_offset) {}             // copy constructor
		BitStreamState & operator=(const BitStreamState &L)
		{
			ptr = L.ptr;
			bit_offset = L.bit_offset;
			return *this;
		} // assignment
		BitStreamState(const guint8 *ptr ):
		ptr(ptr),
		bit_offset(0)
		{
		}
		~BitStreamState()
		{
			this->ptr = NULL;
			this->bit_offset = 0;
		}

		int Read( int bit_count = 1 )
		{
			int rc = 0;
			while( bit_count-- && ptr)
			{
				rc <<= 1;
				int mask = 0x80>>(bit_offset&0x7);
				if( ptr[bit_offset/8] & mask )
				{
					rc |= 1;
				}
				bit_offset++;
			}
			return rc;
		}
};

/**
 * @brief Extract SEI
 */
void AAMPOCDMGSTSessionAdapter::ExtractSEI( GstBuffer *buffer)
{
	GstMapInfo info;
	const guint8 *ptr;
	gsize len;
	if (buffer)
	{
		gst_buffer_map( buffer, &info, (GstMapFlags)(GST_MAP_READ) );
		ptr = info.data;
		len = info.size;
	}
	else
	{
		logprintf("Invalid Buffer Input - NULL");
		gst_buffer_unmap(buffer, &info);
		return;
	}

	if (len > 2048)
		len = 2048;

	for( int i=0; i<len-4; i++ )
	{
		if( ptr[i+0] == 0x00 &&
			ptr[i+1] == 0x00 &&
			ptr[i+2] == 0x00 &&
			ptr[i+3] == 0x0b )
		{ // brute force for now
			BitStreamState bitstream(&ptr[i+4]);
			int forbidden_zero_bit = bitstream.Read();
			int NALUnitTpe = bitstream.Read(6);
			if( NALUnitTpe == 39 ) // NAL_SEI_PREFIX
			{
				int nuh_layer_id = bitstream.Read(6);
				int nuh_temporal_id_plus1 = bitstream.Read(3);
				int payload_type = bitstream.Read(8); // 0x88 (136)
				if( payload_type == 136 ) // SeiMessage::TIME_CODE
				{
					int payload_size = bitstream.Read(8);
					int num_clock_ts = bitstream.Read(2 );
					for( int j=0; j<num_clock_ts; j++ )
					{
						int clock_time_stamp_flag = bitstream.Read(1 );
						if( clock_time_stamp_flag )
						{
							int nuit_field_based_flag = bitstream.Read();
							int counting_type = bitstream.Read(5);
							int full_timestamp_flag = bitstream.Read();
							int discontinuity_flag = bitstream.Read();
							int cnt_dropped_flag = bitstream.Read();
							int n_frames = bitstream.Read(9);
							int seconds_value = 0;
							int minutes_value = 0;
							int hours_value = 0;
							if(full_timestamp_flag )
							{
								seconds_value = bitstream.Read(6);
								minutes_value = bitstream.Read(6);
								hours_value = bitstream.Read(5);
							}
							else
							{
								int seconds_flag = bitstream.Read();
								if( seconds_flag )
								{
									seconds_value = bitstream.Read(6);
									int minutes_flag = bitstream.Read();
									if( minutes_flag)
									{
										minutes_value = bitstream.Read(6);
										int hours_flag = bitstream.Read();
										if( hours_flag )
										{
											hours_value = bitstream.Read(5);
										}
									}
								}
							}
							AAMPLOG_TRACE( "SEI (HH:MM:SS)  %02d:%02d:%02d number of frames (%d)", hours_value, minutes_value, seconds_value, n_frames );
							gst_buffer_add_video_time_code_meta_full(
																	 buffer,
																	 0, // fps_n
																	 1, // fps_d
																	 NULL, // latest_daily_jam
																	 GST_VIDEO_TIME_CODE_FLAGS_NONE,
																	 hours_value,
																	 minutes_value,
																	 seconds_value,
																	 n_frames,
																	 0 // field_count
							);
							break;
						}
					}
				}
			}
		}
	}

	/** Unmap buffer after use **/
	gst_buffer_unmap(buffer, &info);
}
#endif

/**
 * @brief decrypt the data
 */
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

#ifdef AMLOGIC
		/**
		 * Extract the SEI timestamps from both clear and encrypted content. 
		 */
		AAMPLOG_TRACE("DEBUG: Extract the SEI timestamps from encrypted content.");
		ExtractSEI(buffer);
#endif
		pthread_mutex_lock(&decryptMutex);
		start_decrypt_time = GetCurrentTimeStampInMSec();

#if (defined(AMLOGIC) || defined(REALTEKCE))
		/* Added GST_IS_CAPS check also before passing gst caps to OCDM decrypt() as gst_caps_is_empty returns false when caps object is not of 
		   type GST_TYPE_CAPS. This will avoid crash when caps is not of type GST_TYPE_CAPS. */
		if (AAMPOCDMGSTSessionDecrypt && !gst_caps_is_empty(caps) && GST_IS_CAPS(caps))
		{
			AAMPLOG_TRACE("Caps is %s", gst_caps_to_string(caps));
			retValue = AAMPOCDMGSTSessionDecrypt(m_pOpenCDMSession, buffer, subSamplesBuffer, subSampleCount, ivBuffer, keyIDBuffer, 0, caps);
		}
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
				AAMPLOG_INFO("AAMPOCDMSessionAdapter: decrypt returned : %d key status is : %d", retValue, keyStatus);
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

int AAMPOCDMGSTSessionAdapter::decrypt(const uint8_t *f_pbIV, uint32_t f_cbIV, const uint8_t *payloadData, uint32_t payloadDataSize, uint8_t **ppOpaqueData)
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
		retValue = opencdm_session_decrypt(m_pOpenCDMSession, (uint8_t *)payloadData, payloadDataSize, f_pbIV, f_cbIV, NULL, 0, 0);
		end_decrypt_time = GetCurrentTimeStampInMSec();
		if (retValue != 0)
		{
#ifdef USE_THUNDER_OCDM_API_0_2
			KeyStatus keyStatus = opencdm_session_status(m_pOpenCDMSession, NULL, 0);
#else
			KeyStatus keyStatus = opencdm_session_status(m_pOpenCDMSession, NULL, 0);
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
		}

		pthread_mutex_unlock(&decryptMutex);
	}
	return retValue;
}

