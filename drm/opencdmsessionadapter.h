#ifndef OpenCDMSessionAdapter_h
#define OpenCDMSessionAdapter_h

/**
 * @file opencdmsessionadapter.h
 * @brief Handles operation with OCDM session to handle DRM License data
 */

#include "AampDrmSession.h"
#include "aampoutputprotection.h"
#include "AampDrmHelper.h"

#include <open_cdm.h>
#include <open_cdm_adapter.h>
#include "AampDrmCallbacks.h"

using namespace std;

/**
 * @class Event
 * @brief class to DRM Event handle
 */

class Event {
private:
	bool signalled; //TODO: added to handle the events fired before calling wait, need to recheck
	pthread_mutex_t lock;
	pthread_cond_t condition;
       pthread_condattr_t condAttr;
public:
	Event() : signalled(false), lock(PTHREAD_MUTEX_INITIALIZER), condition(PTHREAD_COND_INITIALIZER), condAttr() {
               pthread_mutex_init(&lock, NULL);
               pthread_condattr_init(&condAttr);
               pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC );
               pthread_cond_init(&condition, &condAttr);
	}
	virtual ~Event() {
		pthread_cond_destroy(&condition);
		pthread_mutex_destroy(&lock);
               pthread_condattr_destroy(&condAttr); 
	}

	inline bool wait(const uint32_t waitTime)
	{
		int ret = 0;
		pthread_mutex_lock(&lock);
		if (!signalled) {
			if (waitTime == 0) {
				ret = pthread_cond_wait(&condition, &lock);
			} else {
				struct timespec time;
				clock_gettime(CLOCK_MONOTONIC, &time);

				time.tv_nsec += ((waitTime % 1000) * 1000 * 1000);
				time.tv_sec += (waitTime / 1000) + (time.tv_nsec / 1000000000);
				time.tv_nsec = time.tv_nsec % 1000000000;

				ret = pthread_cond_timedwait(&condition, &lock, &time);

			}
		}

		signalled = false;
		pthread_mutex_unlock(&lock);

		return ((ret == 0)? true: false);
	}

	inline void signal()
        {
		pthread_mutex_lock(&lock);
		signalled = true;
		pthread_cond_broadcast(&condition);
	        pthread_mutex_unlock(&lock);
        }
};

/**
 * @class AAMPOCDMSessionAdapter
 * @brief Open CDM DRM session
 */ 

class AAMPOCDMSessionAdapter : public AampDrmSession
{
protected:
	pthread_mutex_t decryptMutex;

	KeyState m_eKeyState;

	OpenCDMSession* m_pOpenCDMSession;
#ifdef USE_THUNDER_OCDM_API_0_2
	struct OpenCDMSystem* m_pOpenCDMSystem;
#else
	struct OpenCDMAccessor* m_pOpenCDMSystem;
#endif
	OpenCDMSessionCallbacks m_OCDMSessionCallbacks;
	AampOutputProtection* m_pOutputProtection;

	std::string m_challenge;
	uint16_t m_challengeSize;

	std::string m_destUrl;
	KeyStatus m_keyStatus;
	bool m_keyStateIndeterminate;
	std::vector<uint8_t> m_keyStored;

	Event m_challengeReady;
	Event m_keyStatusReady;
	Event m_keyStatusWait;
	string m_sessionID;

	std::vector<uint8_t> m_keyId;

	std::shared_ptr<AampDrmHelper> m_drmHelper;
	AampDrmCallbacks *m_drmCallbacks;

	bool verifyOutputProtection();
public:
	void processOCDMChallenge(const char destUrl[], const uint8_t challenge[], const uint16_t challengeSize);
	void keysUpdatedOCDM();
	void keyUpdateOCDM(const uint8_t key[], const uint8_t keySize);
	long long timeBeforeCallback;

private:
	void initAampDRMSystem();

public:
    AAMPOCDMSessionAdapter(AampLogManager *logObj, std::shared_ptr<AampDrmHelper> drmHelper, AampDrmCallbacks *callbacks = nullptr);
	~AAMPOCDMSessionAdapter();
    AAMPOCDMSessionAdapter(const AAMPOCDMSessionAdapter&) = delete;
	AAMPOCDMSessionAdapter& operator=(const AAMPOCDMSessionAdapter&) = delete;
	void generateAampDRMSession(const uint8_t *f_pbInitData,
			uint32_t f_cbInitData, std::string &customData);
	DrmData * aampGenerateKeyRequest(string& destinationURL, uint32_t timeout);
	int aampDRMProcessKey(DrmData* key, uint32_t timeout);
	KeyState getState();
	void clearDecryptContext();
	void setKeyId(const std::vector<uint8_t>& keyId);
	bool waitForState(KeyState state, const uint32_t timeout) override;
};

#endif
