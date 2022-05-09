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
 * @file aampoutputprotection.cpp
 * @brief Output protection management for Aamp
 */


#include "aampoutputprotection.h"
#include "config.h"
#include "priv_aamp.h"


//#define DEBUG_FUNC_TRACE 1
#ifdef DEBUG_FUNC_TRACE
#define DEBUG_FUNC fprintf(stdout, "%s --> %d\n", __FUNCTION__, __LINE__);
#else
#define DEBUG_FUNC
#endif

/**< Static local variables */
AampOutputProtection* s_pAampOP = NULL;

#define DISPLAY_WIDTH_UNKNOWN       -1  /**< Parsing failed for getResolution().getName(); */
#define DISPLAY_HEIGHT_UNKNOWN      -1  /**< Parsing failed for getResolution().getName(); */
#define DISPLAY_RESOLUTION_NA        0  /**< Resolution not available yet or not connected to HDMI */



AampOutputProtection::AampOutputProtection()
: m_sourceWidth(0)
, m_sourceHeight(0)
, m_displayWidth(DISPLAY_RESOLUTION_NA)
, m_displayHeight(DISPLAY_RESOLUTION_NA)
, m_isHDCPEnabled(false)
, m_gstElement(NULL)
, m_hdcpCurrentProtocol(dsHDCP_VERSION_MAX)
, m_opProtectMutex()
{
    DEBUG_FUNC;
    pthread_mutex_init(&m_opProtectMutex,NULL);

#ifndef USE_OPENCDM
#if defined(USE_PLAYREADY)    
    memset(&m_minOPLevels, 0, sizeof(MinOPLevelsplayReady));
#endif    
#endif

    // Get initial HDCP status
    SetHDMIStatus();

#ifdef IARM_MGR
    // Register IARM callbacks
    IARM_Bus_RegisterEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG, HDMIEventHandler);
    IARM_Bus_RegisterEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_HDCP_STATUS, HDMIEventHandler);
    IARM_Bus_RegisterEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_RES_POSTCHANGE, ResolutionHandler);
#endif //IARM_MGR
}


AampOutputProtection::~AampOutputProtection()
{
    DEBUG_FUNC;

#ifdef IARM_MGR
    // Remove IARM callbacks
    IARM_Bus_RemoveEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG, HDMIEventHandler);
    IARM_Bus_RemoveEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_HDCP_STATUS, HDMIEventHandler);
    IARM_Bus_RemoveEventHandler(IARM_BUS_DSMGR_NAME,IARM_BUS_DSMGR_EVENT_RES_POSTCHANGE, ResolutionHandler);
#endif //IARM_MGR

    s_pAampOP = NULL;

    pthread_mutex_destroy(&m_opProtectMutex);
}



bool AampOutputProtection::IsSourceUHD()
{
    bool retVal = false;

//    DEBUG_FUNC;

#ifdef CONTENT_4K_SUPPORTED
    static gint     sourceHeight    = 0;
    static gint     sourceWidth     = 0;

    if(m_gstElement == NULL) {
        // Value not set, since there is no
        // decoder yet the output size can not
        // be determined
        return false;
    }

    g_object_get(m_gstElement, "video_height", &sourceHeight, NULL);
    g_object_get(m_gstElement, "video_width", &sourceWidth, NULL);

    if(sourceWidth != m_sourceWidth || sourceHeight != m_sourceHeight) {
        AAMPLOG_WARN("viddec (%p) --> says width %d, height %d", m_gstElement, sourceWidth, sourceHeight);
        m_sourceWidth   = sourceWidth;
        m_sourceHeight  = sourceHeight;
    }
    if(sourceWidth != 0 && sourceHeight != 0 &&
       (sourceWidth >= UHD_WITDH || sourceHeight >= UHD_HEIGHT) ) {
        // Source Material is UHD
        retVal = true;
    }
#endif
    return retVal;
}



void AampOutputProtection::SetHDMIStatus()
{
#ifdef IARM_MGR
    bool                    isConnected              = false;
    bool                    isHDCPCompliant          = false;
    bool                    isHDCPEnabled            = true;
    dsHdcpProtocolVersion_t hdcpProtocol             = dsHDCP_VERSION_MAX;
    dsHdcpProtocolVersion_t hdcpReceiverProtocol     = dsHDCP_VERSION_MAX;
    dsHdcpProtocolVersion_t hdcpCurrentProtocol      = dsHDCP_VERSION_MAX;

    DEBUG_FUNC;


    try {
        //Get the HDMI port
	device::Manager::Initialize();
        ::device::VideoOutputPort &vPort = ::device::Host::getInstance().getVideoOutputPort("HDMI0");
        isConnected        = vPort.isDisplayConnected();
        hdcpProtocol       = (dsHdcpProtocolVersion_t)vPort.getHDCPProtocol();
        if(isConnected) {
            isHDCPCompliant          = (vPort.getHDCPStatus() == dsHDCP_STATUS_AUTHENTICATED);
            isHDCPEnabled            = vPort.isContentProtected();
            hdcpReceiverProtocol     = (dsHdcpProtocolVersion_t)vPort.getHDCPReceiverProtocol();
            hdcpCurrentProtocol      = (dsHdcpProtocolVersion_t)vPort.getHDCPCurrentProtocol();
            //get the resolution of the TV
            int width,height;
            int iResID = vPort.getResolution().getPixelResolution().getId();
            if( device::PixelResolution::k720x480 == iResID )
            {
                width =  720;
                height = 480;
            }
            else if(  device::PixelResolution::k720x576 == iResID )
            {
                width = 720;
                height = 576;
            }
            else if(  device::PixelResolution::k1280x720 == iResID )
            {
                width =  1280;
                height = 720;
            }
            else if(  device::PixelResolution::k1920x1080 == iResID )
            {
                width =  1920;
                height = 1080;
            }
            else if(  device::PixelResolution::k3840x2160 == iResID )
            {
                width =  3840;
                height = 2160;
            }
            else if(  device::PixelResolution::k4096x2160 == iResID )
            {
                width =  4096;
                height = 2160;
            }
            else
            {
                width =  DISPLAY_WIDTH_UNKNOWN;
                height = DISPLAY_HEIGHT_UNKNOWN;
                std::string _res = vPort.getResolution().getName();
                AAMPLOG_ERR(" ERR parse failed for getResolution().getName():%s id:%d",(_res.empty() ? "NULL" : _res.c_str()),iResID);
            }

            SetResolution(width, height);
        }
        else {
            isHDCPCompliant = false;
            isHDCPEnabled = false;
            SetResolution(DISPLAY_RESOLUTION_NA,DISPLAY_RESOLUTION_NA);
        }

	device::Manager::DeInitialize();
    }
    catch (const std::exception e) {
        AAMPLOG_WARN("DeviceSettings exception caught in ");
    }

    m_isHDCPEnabled = isHDCPEnabled;

    if(m_isHDCPEnabled) {
        if(hdcpCurrentProtocol == dsHDCP_VERSION_2X) {
            m_hdcpCurrentProtocol = hdcpCurrentProtocol;
        }
        else {
            m_hdcpCurrentProtocol = dsHDCP_VERSION_1X;
        }
        AAMPLOG_WARN(" detected HDCP version %s", m_hdcpCurrentProtocol == dsHDCP_VERSION_2X ? "2.x" : "1.4");
    }
    else {
        AAMPLOG_WARN("DeviceSettings HDCP is not enabled");
    }

    if(!isConnected) {
        m_hdcpCurrentProtocol = dsHDCP_VERSION_1X;
        AAMPLOG_WARN(" GetHDCPVersion: Did not detect HDCP version defaulting to 1.4 (%d)", m_hdcpCurrentProtocol);
    }
#else
    // No video output on device mark HDCP protection as valid
    m_hdcpCurrentProtocol = dsHDCP_VERSION_1X;
    m_isHDCPEnabled = true;
#endif // IARM_MGR

    return;
}


void AampOutputProtection::SetResolution(int width, int height)
{
    DEBUG_FUNC;
    AAMPLOG_WARN(" Resolution : width %d height:%d",width,height);
    m_displayWidth   = width;
    m_displayHeight  = height;
}


void AampOutputProtection::GetDisplayResolution(int &width, int &height)
{
    width   = m_displayWidth;
    height  = m_displayHeight;
}

#ifndef USE_OPENCDM
#if defined(USE_PLAYREADY)    
// TODO: this PlayReady-specific logic should ideally be split out into a separate subclass.
// Note that it is only used by the (non-OCDM) PlayReadyDRMSession class.

// Pleayrady OP Callback

DRM_RESULT DRM_CALL AampOutputProtection::PR_OP_Callback(const DRM_VOID *f_pvOutputLevelsData,
                                                                DRM_POLICY_CALLBACK_TYPE f_dwCallbackType,
                                                                const DRM_VOID *data)
{
    DRM_RESULT res = DRM_SUCCESS;

    DEBUG_FUNC;

    AAMPLOG_WARN("outputLevelsCallback outputLevels=%p callbackType=%u data=%p",
            f_pvOutputLevelsData, static_cast<uint32_t>(f_dwCallbackType), data);

    AampOutputProtection *pInstance = AampOutputProtection::GetAampOutputProcectionInstance();

    // We only care about the play callback.
    if (f_dwCallbackType != DRM_PLAY_OPL_CALLBACK)
        return DRM_SUCCESS;

    // Pull out the protection levels.
    DRM_PLAY_OPL_EX* pr_Levels          = (DRM_PLAY_OPL_EX*)f_pvOutputLevelsData;
    MinOPLevelsplayReady* pm_Levels     = (MinOPLevelsplayReady*)data;

    if(pm_Levels != NULL) {
        pm_Levels->compressedDigitalVideo      = pr_Levels->minOPL.wCompressedDigitalVideo;
        pm_Levels->uncompressedDigitalVideo    = pr_Levels->minOPL.wUncompressedDigitalVideo;
        pm_Levels->analogVideo                 = pr_Levels->minOPL.wAnalogVideo;
        pm_Levels->compressedDigitalAudio      = pr_Levels->minOPL.wCompressedDigitalAudio;
        pm_Levels->uncompressedDigitalAudio    = pr_Levels->minOPL.wUncompressedDigitalAudio;

        // At actual device, enable/disable device output protection will be needed
        // upon receiving this protection information.
        AAMPLOG_WARN(" compressed digital %d, uncompressed digital %d, analog video %d",
                  pm_Levels->compressedDigitalVideo,
                  pm_Levels->uncompressedDigitalVideo,
                  pm_Levels->analogVideo);

        // HDCP needs to be turned on for levels 270 and higher
        if(pm_Levels->uncompressedDigitalVideo >= 270) {
            // Get current HDCP level.
            if(pInstance->m_isHDCPEnabled) {
                // We have an HDCP connection
                if(pInstance->m_hdcpCurrentProtocol == dsHDCP_VERSION_1X ||
                   pInstance->m_hdcpCurrentProtocol == dsHDCP_VERSION_2X) {
                    // We have an active HDCP connection
                    AAMPLOG_WARN(" HDCP is enabled version --> %d", pInstance->m_hdcpCurrentProtocol);
                    res = DRM_SUCCESS;
                }
            }
            else {
                AAMPLOG_WARN(" HDCP --> is not connected", pInstance->m_hdcpCurrentProtocol);
                res = DRM_E_FAIL;
            }
        }
        else {
            AAMPLOG_WARN(" HDCP --> is not required, current version %d,  uncompressedDigitalVideo = %d",
                     pInstance->m_hdcpCurrentProtocol, pm_Levels->uncompressedDigitalVideo);
            res = DRM_SUCCESS;
        }
    }

    pInstance->Release();

    // All done.
    return res;
}
#endif
#endif

#ifdef IARM_MGR

void AampOutputProtection::HDMIEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
    DEBUG_FUNC;

    AampOutputProtection *pInstance = AampOutputProtection::GetAampOutputProcectionInstance();

    switch (eventId) {
        case IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG :
        {
            IARM_Bus_DSMgr_EventData_t *eventData = (IARM_Bus_DSMgr_EventData_t *)data;
            int hdmi_hotplug_event = eventData->data.hdmi_hpd.event;

            const char *hdmihotplug = (hdmi_hotplug_event == dsDISPLAY_EVENT_CONNECTED) ? "connected" : "disconnected";
            AAMPLOG_WARN(" Received IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG  event data:%d status: %s",
                       hdmi_hotplug_event, hdmihotplug);

            pInstance->SetHDMIStatus();

            break;
        }
        case IARM_BUS_DSMGR_EVENT_HDCP_STATUS :
        {
            IARM_Bus_DSMgr_EventData_t *eventData = (IARM_Bus_DSMgr_EventData_t *)data;
            int hdcpStatus = eventData->data.hdmi_hdcp.hdcpStatus;
            const char *hdcpStatusStr = (hdcpStatus == dsHDCP_STATUS_AUTHENTICATED) ? "authenticated" : "authentication failure";
            AAMPLOG_WARN(" Received IARM_BUS_DSMGR_EVENT_HDCP_STATUS  event data:%d status:%s",
                      hdcpStatus, hdcpStatusStr);

            pInstance->SetHDMIStatus();
            break;
        }
        default:
            AAMPLOG_WARN(" Received unknown IARM bus event:%d", eventId);
            break;
    }

    pInstance->Release();
}


void AampOutputProtection::ResolutionHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
    DEBUG_FUNC;

    AampOutputProtection *pInstance = AampOutputProtection::GetAampOutputProcectionInstance();

    switch (eventId) {
        case IARM_BUS_DSMGR_EVENT_RES_PRECHANGE:
            AAMPLOG_WARN(" Received IARM_BUS_DSMGR_EVENT_RES_PRECHANGE ");
            break;
        case IARM_BUS_DSMGR_EVENT_RES_POSTCHANGE:
        {
            int width = 1280;
            int height = 720;

            IARM_Bus_DSMgr_EventData_t *eventData = (IARM_Bus_DSMgr_EventData_t *)data;
            width   = eventData->data.resn.width ;
            height  = eventData->data.resn.height ;

            AAMPLOG_WARN(" Received IARM_BUS_DSMGR_EVENT_RES_POSTCHANGE event width : %d height : %d", width, height);
            pInstance->SetResolution(width, height);
            break;
        }
        default:
            AAMPLOG_WARN(" Received unknown resolution event %d", eventId);
            break;
    }

    pInstance->Release();
}

#endif //IARM_MGR


bool AampOutputProtection::IsAampOutputProcectionInstanceActive()
{
    bool retval = false;

    if(s_pAampOP != NULL) {
        retval = true;
    }
    return retval;
}


AampOutputProtection * AampOutputProtection::GetAampOutputProcectionInstance()
{
    DEBUG_FUNC;
    if(s_pAampOP == NULL) {
        s_pAampOP = new AampOutputProtection();
    }
    s_pAampOP->AddRef();

    return s_pAampOP;
}
