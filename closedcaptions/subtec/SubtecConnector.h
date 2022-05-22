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
 * @file SubtecConnector.h
 *
 * @brief Interface header for libsubtec_connector
 *
 */

#ifndef __SUBTEC_CONNECTOR_H__
#define __SUBTEC_CONNECTOR_H__


/* Closed Captioning Color definition */

#define GSW_CC_EMBEDDED_COLOR (0xff000000)
#define GSW_CC_COLOR(r,g,b)  ( (((r) & 0xFF) << 16) | (((g) & 0xFF) << 8) | ((b) & 0xFF) )

#define GSW_MAX_CC_COLOR_NAME_LENGTH 32
typedef struct gsw_CcColor {
    int rgb;
    char name[GSW_MAX_CC_COLOR_NAME_LENGTH];
} gsw_CcColor;
// Maximum number of CC color capability values
#define GSW_CC_COLOR_MAX 32


/**
 * @brief Closed Captioning type
 */
typedef enum gsw_CcType {
    GSW_CC_TYPE_ANALOG,
    GSW_CC_TYPE_DIGITAL,
    GSW_CC_TYPE_MAX
} gsw_CcType;


/**
 * @brief Closed Captioning Opacity
 */
typedef enum gsw_CcOpacity {
    GSW_CC_OPACITY_EMBEDDED = -1,
    GSW_CC_OPACITY_SOLID,
    GSW_CC_OPACITY_FLASHING,
    GSW_CC_OPACITY_TRANSLUCENT,
    GSW_CC_OPACITY_TRANSPARENT,
    GSW_CC_OPACITY_MAX
} gsw_CcOpacity;


typedef enum gsw_CcFontSize {
    GSW_CC_FONT_SIZE_EMBEDDED = -1,
    GSW_CC_FONT_SIZE_SMALL,
    GSW_CC_FONT_SIZE_STANDARD,
    GSW_CC_FONT_SIZE_LARGE,
    GSW_CC_FONT_SIZE_EXTRALARGE,
    GSW_CC_FONT_SIZE_MAX
} gsw_CcFontSize;


static const char *CCSupportedColors_strings[] = {
    "BLACK",                /* CCColor_BLACK,            */
    "WHITE",                /* CCColor_WHITE,            */
    "RED",                  /* CCColor_RED,              */
    "GREEN",                /* CCColor_GREEN,            */
    "BLUE",                 /* CCColor_BLUE,             */
    "YELLOW",               /* CCColor_YELLOW,           */
    "MAGENTA",              /* CCColor_MAGENTA,          */
    "CYAN",                 /* CCColor_CYAN,             */
    "AUTO",                 /* CCColor_AUTO,             */
};

static unsigned long CCSupportedColors[] = {
    0x00000000,             /* CCColor_BLACK,            */
    0x00ffffff,             /* CCColor_WHITE,            */
    0x00FF0000,             /* CCColor_RED,              */
    0x0000FF00,             /* CCColor_GREEN,            */
    0x000000FF,             /* CCColor_BLUE,             */
    0x00FFFF00,             /* CCColor_YELLOW,           */
    0x00FF00FF,             /* CCColor_MAGENTA,          */
    0x0000FFFF,             /* CCColor_CYAN,             */
    0xFF000000,             /* CCColor_AUTO,             */
};


/* Closed Captioning Font Style */
#define GSW_CC_MAX_FONT_NAME_LENGTH 128
//#define GSW_CC_FONT_STYLE_EMBEDDED "EMBEDDED"
typedef char gsw_CcFontStyle[GSW_CC_MAX_FONT_NAME_LENGTH];

#define GSW_CC_FONT_STYLE_EMBEDDED 		"embedded"  //"EMBEDDED"
#define GSW_CC_FONT_STYLE_DEFAULT 		"Default"   //"DEFAULT"
#define GSW_CC_FONT_STYLE_MONOSPACED_SERIF	"MonospacedSerif"   //"MONOSPACED_SERIF"
#define GSW_CC_FONT_STYLE_PROPORTIONAL_SERIF	"ProportionalSerif" //"PROPORTIONAL_SERIF"
#define GSW_CC_FONT_STYLE_MONOSPACED_SANSSERIF	"MonospacedSansSerif"   //"MONOSPACED_SANSSERIF"
#define GSW_CC_FONT_STYLE_PROPORTIONAL_SANSSERIF  "ProportionalSansSerif"   //"PROPORTIONAL_SANSSERIF"
#define GSW_CC_FONT_STYLE_CASUAL		"Casual"    //"CASUAL"
#define GSW_CC_FONT_STYLE_CURSIVE		"Cursive"   //"CURSIVE"
#define GSW_CC_FONT_STYLE_SMALL_CAPITALS	"SmallCapital"  //"SMALL_CAPITALS"


/**
 * @brief Closed captioning text styles.
 */
typedef enum gsw_CcTextStyle {
    GSW_CC_TEXT_STYLE_EMBEDDED_TEXT = -1,
    GSW_CC_TEXT_STYLE_FALSE,
    GSW_CC_TEXT_STYLE_TRUE,
    GSW_CC_TEXT_STYLE_MAX
} gsw_CcTextStyle;


/**
 * @brief Window Border type
 */
typedef enum gsw_CcBorderType {
    GSW_CC_BORDER_TYPE_EMBEDDED = -1,
    GSW_CC_BORDER_TYPE_NONE,
    GSW_CC_BORDER_TYPE_RAISED,
    GSW_CC_BORDER_TYPE_DEPRESSED,
    GSW_CC_BORDER_TYPE_UNIFORM,
    GSW_CC_BORDER_TYPE_SHADOW_LEFT,
    GSW_CC_BORDER_TYPE_SHADOW_RIGHT,
    GSW_CC_BORDER_TYPE_MAX
} gsw_CcBorderType;

/**
 * @brief Font Edge type
 */
typedef enum gsw_CcEdgeType {
    GSW_CC_EDGE_TYPE_EMBEDDED = -1,
    GSW_CC_EDGE_TYPE_NONE,
    GSW_CC_EDGE_TYPE_RAISED,
    GSW_CC_EDGE_TYPE_DEPRESSED,
    GSW_CC_EDGE_TYPE_UNIFORM,
    GSW_CC_EDGE_TYPE_SHADOW_LEFT,
    GSW_CC_EDGE_TYPE_SHADOW_RIGHT,
    GSW_CC_EDGE_TYPE_MAX
} gsw_CcEdgeType;

/**
 * @brief Closed Captioning Attributes
 */
typedef struct gsw_CcAttributes {
    gsw_CcColor charBgColor;    /* character background color */
    gsw_CcColor charFgColor;    /* character foreground color */
    gsw_CcColor winColor;       /* window color */
    gsw_CcOpacity charBgOpacity;    /* background opacity */
    gsw_CcOpacity charFgOpacity;    /* foreground opacity */
    gsw_CcOpacity winOpacity;   /* window opacity */
    gsw_CcFontSize fontSize;    /* font size */
    gsw_CcFontStyle fontStyle;  /* font style */
    gsw_CcTextStyle fontItalic; /* italicized font */
    gsw_CcTextStyle fontUnderline;  /* underlined font */
    gsw_CcBorderType borderType;    /* window border type */
    gsw_CcColor borderColor;    /* window border color */
    gsw_CcEdgeType edgeType;    /* font edge type */
    gsw_CcColor edgeColor;      /* font edge color */

} gsw_CcAttributes;

/**
 * @brief type of attributes
 */
typedef enum gsw_CcAttribType {
    GSW_CC_ATTRIB_FONT_COLOR = 0x0001,
    GSW_CC_ATTRIB_BACKGROUND_COLOR = 0x0002,
    GSW_CC_ATTRIB_FONT_OPACITY = 0x0004,
    GSW_CC_ATTRIB_BACKGROUND_OPACITY = 0x0008,
    GSW_CC_ATTRIB_FONT_STYLE = 0x0010,
    GSW_CC_ATTRIB_FONT_SIZE = 0x0020,
    GSW_CC_ATTRIB_FONT_ITALIC = 0x0040,
    GSW_CC_ATTRIB_FONT_UNDERLINE = 0x0080,
    GSW_CC_ATTRIB_BORDER_TYPE = 0x0100,
    GSW_CC_ATTRIB_BORDER_COLOR = 0x0200,
    GSW_CC_ATTRIB_WIN_COLOR = 0x0400,
    GSW_CC_ATTRIB_WIN_OPACITY = 0x0800,
    GSW_CC_ATTRIB_EDGE_TYPE = 0x1000,
    GSW_CC_ATTRIB_EDGE_COLOR = 0x2000,
    GSW_CC_ATTRIB_MAX
} gsw_CcAttribType;


typedef enum _CC_VL_OS_API_RESULT {
    CC_VL_OS_API_RESULT_SUCCESS = 0x0,
    CC_VL_OS_API_RESULT_FAILED = 0x1000000,
    CC_VL_OS_API_RESULT_CHECK_ERRNO = 0x1000001,
    CC_VL_OS_API_RESULT_UNSPECIFIED_ERROR = 0x1000002,
    CC_VL_OS_API_RESULT_ACCESS_DENIED = 0x1000003,
    CC_VL_OS_API_RESULT_NOT_IMPLEMENTED = 0x1000004,
    CC_VL_OS_API_RESULT_NOT_EXISTING = 0x1000005,
    CC_VL_OS_API_RESULT_NULL_PARAM = 0x1000006,
    CC_VL_OS_API_RESULT_INVALID_PARAM = 0x1000007,
    CC_VL_OS_API_RESULT_OUT_OF_RANGE = 0x1000008,
    CC_VL_OS_API_RESULT_OPEN_FAILED = 0x1000009,
    CC_VL_OS_API_RESULT_READ_FAILED = 0x1000010,
    CC_VL_OS_API_RESULT_WRITE_FAILED = 0x1000011,
    CC_VL_OS_API_RESULT_MALLOC_FAILED = 0x1000012,
    CC_VL_OS_API_RESULT_TIMEOUT = 0x1000013,
    CC_VL_OS_API_RESULT_INFINITE_LOOP = 0x1000014,
    CC_VL_OS_API_RESULT_BUFFER_OVERFLOW = 0x1000015,

} CC_VL_OS_API_RESULT;

/**
 * @brief Defines closed captioning analog channels.  Each of these channels defines
 * a different service.  For example, CC1 is the "Primary Synchronous
 * Caption Service", and CC2 is the "Secondary Synchronous Caption Service".
 */

typedef enum {

    CCChannel_INCLUSIVE_MINIMUM = 1000, /* Not set */

    CCChannel_CC1,          /* CC1 */

    CCChannel_CC2,          /* CC2 */

    CCChannel_CC3,          /* CC3 */

    CCChannel_CC4,          /* CC4 */

    CCChannel_TEXT1,        /* Text 1 */

    CCChannel_TEXT2,        /* Text 2 */

    CCChannel_TEXT3,        /* Text 3 */

    CCChannel_TEXT4,        /* Text 4 */

    CCChannel_NONE,         /* Used to Set Digital channel to none. (Used to Disable Analog CC ) */

    CCChannel_XDS,          /* XDS */

    CCChannel_EXCLUSIVE_MAXIMUM /* Exclusive bounds check */
} CCAnalogChannel_t;

typedef enum gsw_CcAnalogServices {
    GSW_CC_ANALOG_SERVICE_NONE = 0,
    GSW_CC_ANALOG_SERVICE_CC1 = 1000, /**< Primary Caption service*/
    GSW_CC_ANALOG_SERVICE_CC2 = 1001, /**< Secondary Caption service */
    GSW_CC_ANALOG_SERVICE_CC3 = 1002, /**< Caption 3 */
    GSW_CC_ANALOG_SERVICE_CC4 = 1003, /**< Caption 4 */
    GSW_CC_ANALOG_SERVICE_T1 = 1004, /**< Text 1 */
    GSW_CC_ANALOG_SERVICE_T2 = 1005, /**< Text 2 */
    GSW_CC_ANALOG_SERVICE_T3 = 1006, /**< Text 3 */
    GSW_CC_ANALOG_SERVICE_T4 = 1007 /**< Text 4 */
} gsw_CcAnalogServices;

/*!
* @brief Defines closed captioning digital channels.  Each of these channels defines
* a different service.
*/
typedef enum {
    CCDigitalChannel_INCLUSIVE_MINIMUM = 0, /* Not set */
    CCDigitalChannel_DS1,   /* DS1 */
    CCDigitalChannel_DS2,   /* DS2 */
    CCDigitalChannel_DS3,   /* DS3 */
    CCDigitalChannel_DS4,   /* DS4 */
    CCDigitalChannel_DS5,   /* DS5 */
    CCDigitalChannel_DS6,   /* DS6 */
    CCDigitalChannel_DS7,   /* DS7 */
    CCDigitalChannel_DS8,   /* DS8 */
    CCDigitalChannel_DS9,   /* DS9 */
    CCDigitalChannel_DS10,  /* DS10 */
    CCDigitalChannel_DS11,  /* DS11 */
    CCDigitalChannel_DS12,  /* DS12 */
    CCDigitalChannel_DS13,  /* DS13 */
    CCDigitalChannel_DS14,  /* DS14 */
    CCDigitalChannel_DS15,  /* DS15 */
    CCDigitalChannel_DS16,  /* DS16 */
    CCDigitalChannel_DS17,  /* DS17 */
    CCDigitalChannel_DS18,  /* DS18 */
    CCDigitalChannel_DS19,  /* DS19 */
    CCDigitalChannel_DS20,  /* DS20 */
    CCDigitalChannel_DS21,  /* DS21 */
    CCDigitalChannel_DS22,  /* DS22 */
    CCDigitalChannel_DS23,  /* DS23 */
    CCDigitalChannel_DS24,  /* DS24 */
    CCDigitalChannel_DS25,  /* DS25 */
    CCDigitalChannel_DS26,  /* DS26 */
    CCDigitalChannel_DS27,  /* DS27 */
    CCDigitalChannel_DS28,  /* DS28 */
    CCDigitalChannel_DS29,  /* DS29 */
    CCDigitalChannel_DS30,  /* DS30 */
    CCDigitalChannel_DS31,  /* DS31 */
    CCDigitalChannel_DS32,  /* DS32 */
    CCDigitalChannel_DS33,  /* DS33 */
    CCDigitalChannel_DS34,  /* DS34 */
    CCDigitalChannel_DS35,  /* DS35 */
    CCDigitalChannel_DS36,  /* DS36 */
    CCDigitalChannel_DS37,  /* DS37 */
    CCDigitalChannel_DS38,  /* DS38 */
    CCDigitalChannel_DS39,  /* DS39 */
    CCDigitalChannel_DS40,  /* DS40 */
    CCDigitalChannel_DS41,  /* DS41 */
    CCDigitalChannel_DS42,  /* DS42 */
    CCDigitalChannel_DS43,  /* DS43 */
    CCDigitalChannel_DS44,  /* DS44 */
    CCDigitalChannel_DS45,  /* DS45 */
    CCDigitalChannel_DS46,  /* DS46 */
    CCDigitalChannel_DS47,  /* DS47 */
    CCDigitalChannel_DS48,  /* DS48 */
    CCDigitalChannel_DS49,  /* DS49 */
    CCDigitalChannel_DS50,  /* DS50 */
    CCDigitalChannel_DS51,  /* DS51 */
    CCDigitalChannel_DS52,  /* DS52 */
    CCDigitalChannel_DS53,  /* DS53 */
    CCDigitalChannel_DS54,  /* DS54 */
    CCDigitalChannel_DS55,  /* DS55 */
    CCDigitalChannel_DS56,  /* DS56 */
    CCDigitalChannel_DS57,  /* DS57 */
    CCDigitalChannel_DS58,  /* DS58 */
    CCDigitalChannel_DS59,  /* DS59 */
    CCDigitalChannel_DS60,  /* DS60 */
    CCDigitalChannel_DS61,  /* DS61 */
    CCDigitalChannel_DS62,  /* DS62 */
    CCDigitalChannel_DS63,  /* DS63 */
    CCDigitalChannel_NONE,  /* Used to Set Digital channel to none. (Used to Disable Digital CC.) */
    CCDigitalChannel_EXCLUSIVE_MAXIMUM  /* Exclusive bounds check */
} CCDigitalChannel_t;

/**
 * @brief Closed Caption Status values as referred to by Applications
 */
typedef enum {
    CCStatus_OFF = 0,       /* Closed Caption Disabled */

    CCStatus_ON,            /* Closed Caption Enabled */

    CCStatus_ON_MUTE,       /* Closed Caption ON MUTE */

    CCStatus_EXCLUSIVE_MAXIMUM
} CCStatus_t;

typedef int mrcc_Error;
namespace subtecConnector
{
    void resetChannel();
    void close();

    mrcc_Error initHal();
    mrcc_Error initPacketSender();

namespace ccMgrAPI
{
    mrcc_Error ccShow(void);
    mrcc_Error ccHide(void);

    mrcc_Error ccSetDigitalChannel(unsigned int channel);

    mrcc_Error ccSetAnalogChannel(unsigned int channel);

    mrcc_Error ccSetAttributes(gsw_CcAttributes * attrib, short type, gsw_CcType ccType);
    mrcc_Error ccGetAttributes(gsw_CcAttributes * attrib, gsw_CcType ccType);

    mrcc_Error ccGetCapability(gsw_CcAttribType attribType, gsw_CcType ccType, void **value, unsigned int *size);

    // mrcc_Error ccSetCCState(CCStatus_t ccStatus, int /*not used*/);
    // mrcc_Error ccGetCCState(CCStatus_t * pCcStatus);

} // ccMgrAPI
} // subtecConnector

#endif //__SUBTEC_CONNECTOR_H__
