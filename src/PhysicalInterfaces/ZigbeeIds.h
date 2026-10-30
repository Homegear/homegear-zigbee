/* Copyright 2019 Homegear GmbH
 *
 * Homegear is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Homegear is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Homegear.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */

#ifndef IZIGBEEIDS_H_
#define IZIGBEEIDS_H_

#include <cstdint>

namespace Zigbee
{

#define SOF 0xFE // start of frame

// MT Command types:

//A POLL command is used to retrieve queued data. This
//command is only applicable to SPI transport. For a POLL command the
//subsystem and Id are set to zero and data length is zero

#define POLL 0x00 // this does not seem so useful, it seems that for cc2630 is not even supported

//synchronous request that requires an immediate response. For
//example, a function call with a return value would use an SREQ command.

#define SREQ 0x20

//An asynchronous request. For example, a callback event or a
//function call with no return value would use an AREQ command.

#define AREQ 0x40

//A synchronous response. This type of command is only sent in
//response to a SREQ command. For an SRSP command the subsystem and
//Id are set to the same values as the corresponding SREQ. The length of an
//SRSP is generally nonzero, so an SRSP with length=0 can be used to
//indicate an error

#define SRSP 0x60

// Subsystem:
// Info here: http://processors.wiki.ti.com/index.php/NPI_Type_SubSystem

// 2, 3 and perhaps 8 not so important at first, can be left for later? DEBUG is easy though and might prove useful
// also check the cmd1 = 60 cmd2 = 0 reply, it comes when the other end does not understand the sent command
// length = 3, 0x60, 0x00, Error Code, Req cmd0, req cmd1 : see 'z-stack znp interface specifications' pdf
// AF and ZDO feature the complete ZigBee interface - focus on those two first

// SYS is an api to low level
// UTIL is for info, callbacks and so on, seems to be needed, too
// APP_CFG is for setting the channel, commissioning methods, trust center configurations, install codes, so it's needed fast, it looks like without it cannot pair!

#define RPC_ERR 0x00
#define SYS     0x01
#define MAC     0x02
#define NWK     0x03 // this seems to be 'not supported'
#define AF      0x04
#define ZDO     0x05
#define SAPI    0x06 // can be avoided by using other ones
#define UTIL    0x07
#define ZB_DEBUG   0x08
#define APP_INT 0x09
#define APP_CFG 0x0F

// those grouped here might not be needed
#define RPC_SYS_RCAF       0x0A
#define RPC_SYS_RCN        0x0B
#define RPC_SYS_RCN_CLIENT 0x0C
#define RPC_SYS_BOOT       0x0D
#define RPC_SYS_ZIPTEST    0x0E // this could be useful, though
// until here

// again some possible useless defines
#define RPC_SYS_PERIPHERALS 0x10
#define RPC_SYS_NFC         0x11
#define RPC_SYS_PB_NWK_MGR  0x12
#define RPC_SYS_PB_GW       0x13
#define RPC_SYS_PB_OTA_MGR  0x14
#define RPC_SYS_BLE_HCI     0x16
#define RPC_SYS_SRV_CTR     0x1F
// until here

#define GREENPW 0x15 //also RPC_SYS_BLE_SPNP

#define MT_TYPE_MASK 0xE0
#define SUBSYS_MASK  0x1F


enum class ZigbeeReturnIds : uint8_t
{
    ZSuccess = 0x00,
    Zfailure = 0x01,
    ZinvalidParameter = 0x02,

    NV_ITEM_UNINIT = 0x09,
    NV_OPER_FAILED = 0x0a,
    NV_BAD_ITEM_LEN = 0x0c,

    ZmemError = 0x10,
    ZbufferFull = 0x11,
    ZunsupportedMode = 0x12,
    ZmacMemError = 0x13,

    MAC_UNSUPPORTED_NOT_SPOORT = 0x18,
    MAC_BAD_STATE = 0x19,
    MAC_NO_RESOURCES = 0x20,
    MAC_ACK_PENDING = 0x21,
    MAC_NO_TIME = 0x22,
    MAC_TX_ABORTED = 0x23,
    SAPI_IN_PROGRESS = 0x24,
    SAPI_TIMEOUT = 0x25,
    SAPI_INIT = 0x26,

    REQUEST_DENIED = 0x70,
    MULTIPLE_REQUEST_NOT_ALLOWED = 0x71,
    INDICATION_REDIRECTION_TO_AP = 0x72,
    PREFERENCE_DENIED = 0x73,
    PREFERENCE_IGNORED = 0x74,
    NOT_AUTHORIZED = 0x7E,

    zdoInvalidRequestType = 0x80,
    UNSUP_CLUSTER_CMD = 0x81, // also 'device not found'
    zdoInvalidEndpoint = 0x82,

    NOT_ACTIVE = 0x83,

    zdoUnsupported = 0x84,
    zdoTimeout = 0x85,
    zdoNoMatch = 0x86,
    zdoReserved = 0x87,
    zdoNoBindEntry = 0x88,
    zdoNoDescriptor = 0x89,
    zdoInsufficientSpace = 0x8A,
    zdoNotPermitter = 0x8B,
    zdoTableFull = 0x8C,
    zdoNotAuthorized = 0x8D,
    zdoDeviceBindingTableFull = 0x8E,

    OTA_ABORT = 0x95,
    OTA_IMAGE_INVALID = 0x96,
    OTA_WAIT_FOR_DATA = 0x97,
    OTA_NO_IMAGE_AVAILABLE = 0x98,
    OTA_REQUIRE_MORE_IMAGE = 0x99,

    ZsecNoKey = 0xa1,
    ZsecOldFrmCount = 0xa2,
    ZsecMaxFrmCount = 0xa3,
    ZsecCCMFail = 0xa4,

    ZapsFail = 0xb1,
    ZapsTableFull = 0xb2,
    ZapsIllegalRequest = 0xb3,
    ZapsInvalidBinding = 0xb4,
    ZapsUnsupportedAttrib = 0xb5,
    ZapsNotSupported = 0xb6,
    ZapsNoAck = 0xb7,
    ZapsDuplicateEntry = 0xb8,
    ZapsNoBoundDevice = 0xb9,
    ZapsNotAllowed = 0xba,
    ZapsNotAuthenticated = 0xbb,

    ZnwkInvalidParam = 0xc1,
    ZnwkInvalidRequest = 0xc2,
    ZnwkNotPermitted = 0xc3,
    ZnwkStartupFailure = 0xc4,
    ZnwkAlreadyPresent = 0xc5,
    ZnwkSyncFailure = 0xc6,
    ZnwkTableFull = 0xc7,
    ZnwkUnknownDevice = 0xc8,
    ZnwkUnsupportedAttribute = 0xc9,
    ZnwkNoNetworks = 0xca,
    ZnwkLeaveUnconfirmed = 0xcb,
    ZnwkNoAck = 0xcc,
    ZnwkNoRoute = 0xcd,

    MAC_BEACON_LOSS = 0xe0,
    MAC_CHANNEL_ACCESS_FAILURE = 0xe1,
    MAC_DENIED = 0xe2,
    MAC_DISABLE_TRX_FAILURE = 0xe3,
    MAC_FAILED_SECURITY_CHECK = 0xe4,
    MAC_FRAME_TOO_LONG = 0xe5,
    MAC_INVALID_GTS = 0xe6,
    MAC_INVALID_HANDLE = 0xe7,
    MAC_INVALID_PARAMETER = 0xe8,
    MAC_NO_ACK = 0xe9,
    MAC_NO_BEACON = 0xea,
    MAC_NO_DATA = 0xeb,
    MAC_NO_SHORT_ADDR = 0xec,
    MAC_OUT_OF_CAP = 0xed,
    MAC_PANID_CONFLICT = 0xee,
    MAC_REALIGNMENT = 0xef,
    MAC_TRANSACTION_EXPIRED = 0xf0,
    MAC_TRANSACTION_OVERFLOW = 0xf1,
    MAC_TX_ACTIVE = 0xf2,
    MAC_UNAVAILABLE_KEY = 0xf3,
    MAC_UNSUPPORTED_ATTRIBUTE = 0xf4,
    MAC_UNSUPPORTED = 0xf5,
    MAC_ON_TIME_TOO_LING = 0xf6,
    MAC_PAST_TIME = 0xf7,
    MAC_TRACKING_OFF = 0xf8,

    MAC_SCAN_IN_PROGRESS = 0xfc,

    MAC_SRC_MATCH_INVALID_INDEX = 0xff
};

enum class ZigbeeZCLReturnIds : uint8_t
{
    SUCCESS = 0x0,
    FAILURE = 0x01,

    REQUEST_DENIED = 0x70,
    MULTIPLE_REQUEST_NOT_ALLOWED = 0x71,
    INDICATION_REDIRECTION_TO_AP = 0x72,
    PREFERENCE_DENIED = 0x73,
    PREFERENCE_IGNORED = 0x74,
    NOT_AUTHORIZED = 0x7e,
    RESERVED_FIELD_NOT_ZERO = 0x7f,
    MALFORMED_COMMAND = 0x80,
    UNSUP_CLUSTER_COMMAND = 0x81,
    UNSUP_GENERAL_COMMAND = 0x82,
    UNSUP_MANUF_CLUSTER_COMMAND = 0x83,
    UNSUP_MANUF_GENERAL_COMMAND = 0x84,
    INVALID_FIELD = 0x85,
    UNSUPPORTED_ATTRIBUTE = 0x86,
    INVALID_VALUE = 0x87,
    READ_ONLY = 0x88,
    INSUFFICIENT_SPACE = 0x89,
    DUPLICATE_EXISTS = 0x8a,
    NOT_FOUND = 0x8b,
    UNREPORTABLE_ATTRIBUTE = 0x8c,
    INVALID_DATA_TYPE = 0x8d,
    INVALID_SELECTOR = 0x8e,
    WRITE_ONLY = 0x8f,
    INCONSISTENT_STARTUP_STATE = 0x90,
    DEFINED_OUT_OF_BAND = 0x91,
    INCONSISTENT = 0x92,
    ACTION_DENIED = 0x93,
    TIMEOUT = 0x94,
    ABORT = 0x95,
    INVALID_IMAGE = 0x96,
    WAIT_FOR_DATA = 0x97,
    NO_IMAGE_AVAILABLE = 0x98,
    REQUIRE_MORE_IMAGE = 0x99,
    NOTIFICATION_PENDING = 0x9a,
    HARDWARE_FAILURE = 0xc0,
    SOFTWARE_FAILURE = 0xc1,
    CALIBRATION_ERROR = 0xc2,
    UNSUPPORTED_CLUSTER = 0xc3
};

}

#endif

