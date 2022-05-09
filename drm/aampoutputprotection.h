/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/**
 * @file aampoutputprotection.h
 * @brief Output protection management for Aamp
 */

#ifndef aampoutputprotection_h
#define aampoutputprotection_h

#include <pthread.h>

#ifdef IARM_MGR
// IARM
#include "manager.hpp"
#include "host.hpp"
#include "videoResolution.hpp"
#include "videoOutputPort.hpp"
#include "videoOutputPortType.hpp"
#include <libIARM.h>
#include <libIBus.h>
#include "libIBusDaemon.h"
#include "dsMgr.h"
#include "dsDisplay.h"
#include <iarmUtil.h>

#else
#include <stdint.h>
typedef int dsHdcpProtocolVersion_t;
#define dsHDCP_VERSION_MAX      30
#define dsHDCP_VERSION_2X       22
#define dsHDCP_VERSION_1X       14
#endif // IARM_MGR

#include <stdio.h>
#include <gst/gst.h>

#ifndef USE_OPENCDM
#ifdef USE_PLAYREADY

#include <drmbuild_oem.h>
#include <drmcommon.h>
#include <drmmanager.h>
#include <drmmathsafe.h>
#include <drmtypes.h>
#include <drmerr.h>
#endif
#endif

#undef __in
#undef __out
using namespace std;

#define UHD_WITDH   3840
#define UHD_HEIGHT  2160

/**
 * @class ReferenceCount
 * @brief Provides reference based memory management
 */
class ReferenceCount
{
public:

    ReferenceCount() : m_refCount(0), m_refCountMutex() {
        pthread_mutex_init(&m_refCountMutex, NULL);
    }

    virtual ~ReferenceCount() {
        pthread_mutex_destroy(&m_refCountMutex);
    }


    uint32_t AddRef() const {
        pthread_mutex_lock(&m_refCountMutex);
        m_refCount++;
        pthread_mutex_unlock(&m_refCountMutex);
        return m_refCount;
    }


    void Release() const {
        pthread_mutex_lock(&m_refCountMutex);
        m_refCount--;
        pthread_mutex_unlock(&m_refCountMutex);

        if(m_refCount == 0) {
		delete (ReferenceCount *)this;
        }
    }

private:
    mutable pthread_mutex_t     m_refCountMutex;
    mutable int                 m_refCount;
};

/**
 * @class AampOutputProtection
 * @brief Class to enforce HDCP authentication
 */
class AampOutputProtection : public ReferenceCount
{

private:
#ifndef USE_OPENCDM
#ifdef USE_PLAYREADY
    // Protection levels from CDM
    struct MinOPLevelsplayReady
    {
        DRM_WORD compressedDigitalVideo;
        DRM_WORD uncompressedDigitalVideo;
        DRM_WORD analogVideo;
        DRM_WORD compressedDigitalAudio;
        DRM_WORD uncompressedDigitalAudio;
    };
#endif
#endif

    pthread_mutex_t         m_opProtectMutex;

#ifndef USE_OPENCDM
#ifdef USE_PLAYREADY
    MinOPLevelsplayReady    m_minOPLevels;
#endif
#endif
    int                     m_sourceWidth;
    int                     m_sourceHeight;
    int                     m_displayWidth;
    int                     m_displayHeight;
    bool                    m_isHDCPEnabled;
    dsHdcpProtocolVersion_t m_hdcpCurrentProtocol;
    GstElement*             m_gstElement;

    /**
     * @brief Set the HDCP status using data from DeviceSettings
     */
    void SetHDMIStatus();
    /**
     * @brief Set values of resolution member variable
     * @param width display width
     * @param height display height
     */
    void SetResolution(int width, int height);

public:
    /**
     * @brief AampOutputProtection Constructor
     */
    AampOutputProtection();
    /**
     * @brief AampOutputProtection Destructor
     */
    virtual ~AampOutputProtection();
    /**     
     * @brief Copy constructor disabled
     *
     */
    AampOutputProtection(const AampOutputProtection&) = delete;
    /**
     * @brief assignment operator disabled
     *
     */
    AampOutputProtection& operator=(const AampOutputProtection&) = delete;

#ifndef USE_OPENCDM

    /**
     * @brief Get PlayRedy OP levels
     * @retval m_minOPLevels
     */
#ifdef USE_PLAYREADY

    MinOPLevelsplayReady* getPlayReadyLevels() { return & m_minOPLevels; }
   /**
    * @brief Pleayrady OP Callback to ensure HDCP compliance
    * @param f_pvOutputLevelsData : Pointer to licenses output restrictions information
    * @param f_dwCallbackType : Type of callback
    * @param data : Pointer passed from Drm_Reader_Bind, m_minOPLevels
    * @retval DRM_SUCCESS if no errors encountered
    */
    static DRM_RESULT DRM_CALL PR_OP_Callback(const DRM_VOID *f_pvOutputLevelsData,
                                              DRM_POLICY_CALLBACK_TYPE f_dwCallbackType,
                                              const DRM_VOID *data);
#endif
#endif
    
#ifdef IARM_MGR
    // IARM Callbacks
    /**
     * @brief IARM event handler for HDCP and HDMI hot plug events
     * @param owner Owner of the IARM mgr
     * @param eventId IARM Event ID
     * @param data HDMI data
     * @param len Length of the data
     */
    static void HDMIEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len); 
    /**
     * @brief IARM event handler for resolution changes
     * @param owner Owner of the IARM mgr
     * @param eventId IARM Event ID
     * @param data IARM data  
     * @param len Length of the data
     */
    static void ResolutionHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
#endif //IARM_MGR

    // State functions

    /**
     * @brief Check if HDCP is 2.2
     * @retval true if 2.2 false otherwise
     */
    bool isHDCPConnection2_2() { return m_hdcpCurrentProtocol == dsHDCP_VERSION_2X; }
    /** 
     * @brief Check if source is UHD using video decoder dimensions
     * @retval true, if source is UHD, otherwise false
     */
    bool IsSourceUHD();

    /**
     * @brief gets display resolution
     * @param[out] width : Display Width
     * @param[out] height : Display height
     */
    void GetDisplayResolution(int &width, int &height);

    /**
     * @brief Set GstElement
     * @param element
     */
    void setGstElement(GstElement *element) { m_gstElement = element;  }

    // Singleton for object creation
	
    /**
     * @brief Singleton for object creation
     * @retval AampOutputProtection object
     */	
    static AampOutputProtection * GetAampOutputProcectionInstance();
    /**
     * @brief Check if  AampOutputProcectionInstance active
     * @retval true or false
     */
    static bool IsAampOutputProcectionInstanceActive();

};

#endif // aampoutputprotection_h
