/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
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
* @defgroup iarmmgrs
* @{
* @defgroup hal
* @{
**/


#ifndef _XFC_IR_KEYCODES_H_
#define _XFC_IR_KEYCODES_H_

#ifdef __cplusplus
extern "C" {
#endif

/*-------------------------------------------------------------------
   Defines/Macros
   @TODO: We are simply reusing VL's value for now
-------------------------------------------------------------------*/
#define DELAY_REPEAT_MASK       0x00000001
#define LANGUAGE_MASK           0x00000010
#define MISSED_KEY_TIMEOUT_MASK 0x00000100
#define REPEAT_KEY_ENABLED_MASK 0x00001000
#define REPEAT_FREQUENCY_MASK   0x00010000
#define REPORT_MODIFIERS_MASK   0x00100000
#define NUM_OF_DEVICES_MASK     0x01000000


#define KET_KEYDOWN 		0x00008000UL	   
#define KET_KEYUP   		0x00008100UL	         
#define KET_KEYREPEAT		0x00008200UL 

//Numeric keys (common in Remote and Key Board)
//Combination of Key + device type (HID_DEVICES) is unique.
#define KED_DIGIT0		    0x00000030UL
#define KED_DIGIT1		    0x00000031UL
#define KED_DIGIT2		    0x00000032UL
#define KED_DIGIT3		    0x00000033UL
#define KED_DIGIT4		    0x00000034UL
#define KED_DIGIT5		    0x00000035UL
#define KED_DIGIT6		    0x00000036UL
#define KED_DIGIT7		    0x00000037UL
#define KED_DIGIT8		    0x00000038UL
#define KED_DIGIT9		    0x00000039UL
#define KED_PERIOD		    0x00000040UL

#define KED_DISCRETE_POWER_ON         0x00000050UL
#define KED_DISCRETE_POWER_STANDBY    0x00000051UL

#define KED_SEARCH			0x000000CFUL
#define KED_SETUP			0x00000052UL

#define KED_CLOSED_CAPTIONING       0x00000060UL
#define KED_LANGUAGE                0x00000061UL
#define KED_VOICE_GUIDANCE          0x00000062UL
#define KED_HEARTBEAT               0x00000063UL
#define KED_PUSH_TO_TALK            0x00000064UL
#define KED_DESCRIPTIVE_AUDIO       0x00000065UL
#define KED_VOLUME_OPTIMIZE         0x00000066UL
#define KED_XR2V3                   0x00000067UL
#define KED_XR5V2                   0x00000068UL
#define KED_XR11V1                  0x00000069UL
#define KED_XR11V2                  0x0000006AUL
#define KED_XR13                    0x0000006BUL
#define KED_XR11_NOTIFY             0x0000006CUL

#define KED_XR15V1_NOTIFY           0x00000070UL
#define KED_XR15V1_SELECT           0x00000071UL
#define KED_XR15V1_PUSH_TO_TALK     0x00000072UL

#define KED_SCREEN_BIND_NOTIFY      0x00000073UL

#define KED_XR16V1_NOTIFY           0x00000074UL
#define KED_XR16V1_SELECT           0x00000075UL
#define KED_XR16V1_PUSH_TO_TALK     0x00000076UL

#define KED_RF_POWER	    0x0000007FUL
#define KED_POWER		    0x00000080UL
#define KED_FP_POWER		KED_POWER
#define KED_ARROWUP		    0x00000081UL
#define KED_ARROWDOWN		0x00000082UL
#define KED_ARROWLEFT		0x00000083UL
#define KED_ARROWRIGHT	  	0x00000084UL
#define KED_SELECT		    0x00000085UL
#define KED_ENTER		    0x00000086UL
#define KED_EXIT	  	    0x00000087UL
#define KED_CHANNELUP	  	0x00000088UL
#define KED_CHANNELDOWN	  	0x00000089UL
#define KED_VOLUMEUP	  	0x0000008AUL
#define KED_VOLUMEDOWN	  	0x0000008BUL
#define KED_MUTE	  	    0x0000008CUL
#define KED_GUIDE	  	    0x0000008DUL
#define KED_VIEWINGGUIDE  	KED_GUIDE
#define KED_INFO	  	    0x0000008EUL
#define KED_SETTINGS	  	0x0000008FUL
#define KED_PAGEUP	  	    0x00000090UL
#define KED_PAGEDOWN	  	0x00000091UL
#define KED_KEYA	  	    0x00000092UL
#define KED_KEYB	  	    0x00000093UL
#define KED_KEYC	  	    0x00000094UL
#define KED_KEYD	  	    0x0000009FUL	
#define KED_KEY_RED_CIRCLE      KED_KEYC //Host2.1 Sec 7.2
#define KED_KEY_GREEN_DIAMOND   KED_KEYD //Host2.1 Sec 7.2
#define KED_KEY_BLUE_SQUARE     KED_KEYB //Host2.1 Sec 7.2
#define KED_KEY_YELLOW_TRIANGLE KED_KEYA //Host2.1 Sec 7.2
#define KED_LAST	  	    0x00000095UL
#define KED_FAVORITE	  	0x00000096UL
#define KED_REWIND	  	    0x00000097UL
#define KED_FASTFORWARD	  	0x00000098UL
#define KED_PLAY	  	    0x00000099UL
#define KED_STOP	  	    0x0000009AUL
#define KED_PAUSE		    0x0000009BUL
#define KED_RECORD		    0x0000009CUL
#define KED_BYPASS		    0x0000009DUL
#define KED_TVVCR		    0x0000009EUL

#define KED_REPLAY          0x000000A0UL
#define KED_HELP            0x000000A1UL
#define KED_RECALL_FAVORITE_0    0x000000A2UL
#define KED_CLEAR                0x000000A3UL
#define KED_DELETE               0x000000A4UL
#define KED_START                0x000000A5UL
#define KED_POUND                0x000000A6UL
#define KED_FRONTPANEL1          0x000000A7UL
#define KED_FRONTPANEL2          0x000000A8UL
#define KED_OK   		         0x000000A9UL
#define KED_STAR                 0x000000AAUL
#define KED_PROGRAM              0x000000ABUL

#define KED_TVPOWER		    0x000000C1UL	// Alt remote
#define KED_PREVIOUS		0x000000C3UL	// Alt remote
#define KED_NEXT		    0x000000C4UL	// Alt remote
#define KED_MENU       		0x000000C0UL	// Alt remote
#define KED_INPUTKEY		0x000000D0UL	//
#define KED_LIVE		    0x000000D1UL	//
#define KED_MYDVR		    0x000000D2UL	//
#define KED_ONDEMAND		0x000000D3UL	//
#define KED_STB_MENU        0x000000D4UL    //
#define KED_AUDIO           0x000000D5UL    //
#define KED_FACTORY         0x000000D6UL    //
#define KED_RFENABLE        0x000000D7UL    //
#define KED_LIST		    0x000000D8UL
#define KED_RF_PAIR_GHOST	0x000000EFUL	// Ghost code to implement auto pairing in RF remotes
#define KED_WPS                 0x000000F0UL    // Key to initiate WiFi WPS pairing
#define KED_DEEPSLEEP_WAKEUP    0x000000F1UL    // Key to initiate deepsleep wakeup
#define KED_NEW_BATTERIES_INSERTED 0x000000F2UL // Signals a battery set was replaced
#define KED_GRACEFUL_SHUTDOWN      0x000000F3UL // Signals an external power supply device is shutting down
#define KED_UNDEFINEDKEY	0x000000FEUL	// Use for keys not defined here.  Pass raw code as well.


#define KED_BACK		    0x100000FEUL
#define KED_DISPLAY_SWAP	0x300000FEUL
#define KED_PINP_MOVE		0x400000FEUL
#define KED_PINP_TOGGLE		0x500000FEUL
#define KED_PINP_CHDOWN		0x600000FEUL
#define KED_PINP_CHUP		0x700000FEUL
#define KED_DMC_ACTIVATE	0x800000FEUL
#define KED_DMC_DEACTIVATE	0x900000FEUL
#define KED_DMC_QUERY		0xA00000FEUL
#define KED_OTR_START		0xB00000FEUL
#define KED_OTR_STOP		0xC00000FEUL

#ifdef __cplusplus
}
#endif

#endif /* _XFC_IR_KEYCODES_H_ */


/** @} */
/** @} */
