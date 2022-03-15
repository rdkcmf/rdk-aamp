/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2021 RDK Management
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
 * @file aamplogger.h
 * @brief AAMP Log unitility
 */

#ifndef AAMPFNLOGGER_H
#define AAMPFNLOGGER_H

#include <stddef.h>

extern void logprintf(const char *format, ...);
extern void logprintf_new(const char* name, int playerId,const char* levelstr,const char* file, int line,const char *format, ...);
class FnLogger {
public:
    FnLogger( std::string const & pMsg1,std::string const & pMsg2  ) : msg1(pMsg1),msg2(pMsg2)
   {   logprintf("%s Enter ===> %s(): ", msg1.c_str(), msg2.c_str()); }
   ~FnLogger()
    {   logprintf("%s Exit <=== %s(): ", msg1.c_str(), msg2.c_str()); }
    std::string msg1;
    std::string msg2;
};

#ifndef LOG_FN_TRACE
#define LOG_FN_TRACE 0
#endif

#ifndef LOG_FN_TRACE_P
#define LOG_FN_TRACE_P 0
#endif

#ifndef LOG_FN_TRACE_F_MPD
#define LOG_FN_TRACE_F_MPD 0
#endif

#ifndef LOG_FN_TRACE_G_CONF
#define LOG_FN_TRACE_G_CONF 0
#endif

#ifndef LOG_FN_TRACE_DMX
#define LOG_FN_TRACE_DMX 0
#endif

#ifndef LOG_FN_TRACE_STRM_ABS
#define LOG_FN_TRACE_STRM_ABS 0
#endif

#ifndef LOG_FN_TRACE_STRM_PROG
#define LOG_FN_TRACE_STRM_PROG 0
#endif

#ifndef LOG_FN_TRACE_PROFILER
#define LOG_FN_TRACE_PROFILER 0
#endif

#ifndef LOG_FN_TRACE_CDAI
#define LOG_FN_TRACE_CDAI 0
#endif

#ifndef LOG_FN_TRACE_DRM
#define LOG_FN_TRACE_DRM 0
#endif

#ifndef LOG_FN_TRACE_DRM_SMGR
#define LOG_FN_TRACE_DRM_SMGR 0
#endif

#ifndef LOG_FN_TRACE_ISOBMFF
#define LOG_FN_TRACE_ISOBMFF 0
#endif

//===========================================

#if LOG_FN_TRACE
#define FN_TRACE(x) FnLogger l_##x##_scope("[GST-PLAYER]",x);
#else
#define FN_TRACE(x)
#endif

#if LOG_FN_TRACE_P
#define FN_TRACE_P(x) FnLogger l_##x##_scope("[GST-PLAYER-PVT]",x);
#else
#define FN_TRACE_P(x)
#endif

#if LOG_FN_TRACE_F_MPD
#define FN_TRACE_F_MPD(x) FnLogger l_##x##_scope("[F-MPD]",x);
#else
#define FN_TRACE_F_MPD(x)
#endif


#if LOG_FN_TRACE_G_CONF
#define FN_TRACE_G_CONF(x) FnLogger l_##x##_scope("[G-CONF]",x);
#else
#define FN_TRACE_G_CONF(x)
#endif

#if LOG_FN_TRACE_DMX
#define FN_TRACE_DMX(x) FnLogger l_##x##_scope("[DMX]",x);
#else
#define FN_TRACE_DMX(x)
#endif

#if LOG_FN_TRACE_STRM_ABS
#define FN_TRACE_STRM_ABS(x) FnLogger l_##x##_scope("[STRM-ABS]",x);
#else
#define FN_TRACE_STRM_ABS(x)
#endif

#if LOG_FN_TRACE_STRM_PROG
#define FN_TRACE_STRM_PROG(x) FnLogger l_##x##_scope("[STRM-PROG]",x);
#else
#define FN_TRACE_STRM_PROG(x)
#endif

#if LOG_FN_TRACE_PROFILER
#define FN_TRACE_PROFILER(x) FnLogger l_##x##_scope("[PROFILER]",x);
#else
#define FN_TRACE_PROFILER(x)
#endif

#if LOG_FN_TRACE_CDAI
#define FN_TRACE_CDAI(x) FnLogger l_##x##_scope("[CDAI]",x);
#else
#define FN_TRACE_CDAI(x)
#endif

#if LOG_FN_TRACE_DRM
#define FN_TRACE_DRM(x) FnLogger l_##x##_scope("[DRM]",x);
#else
#define FN_TRACE_DRM(x)
#endif

#if LOG_FN_TRACE_DRM_SMGR
#define FN_TRACE_DRM_SMGR(x) FnLogger l_##x##_scope("[DRM-SMGR]",x);
#else
#define FN_TRACE_DRM_SMGR(x)
#endif

#if LOG_FN_TRACE_ISOBMFF
#define FN_TRACE_ISOBMFF(x) FnLogger l_##x##_scope("[ISOBMFF]",x);
#else
#define FN_TRACE_ISOBMFF(x)
#endif

#endif // AAMPFNLOGGER_H
