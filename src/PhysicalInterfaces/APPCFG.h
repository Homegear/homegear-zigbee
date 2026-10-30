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

#ifndef APPCFG_H_
#define APPCFG_H_

#include <cstdint>

#include "ZigbeeIds.h"
#include "IZigbeeInterface.h"
#include "MT.h"

namespace ZigbeeCommands
{

class APPCFG_MTCmdRequest : public MTCmdRequest
{
public:
    explicit APPCFG_MTCmdRequest(uint8_t cmdid) : MTCmdRequest(cmdid, APP_CFG) {}
};

class APPCFG_MTCmdResponse : public MTCmdResponse
{
public:
    explicit APPCFG_MTCmdResponse(uint8_t cmdid) : MTCmdResponse(cmdid, APP_CFG) {}
};

class APPCFG_MTCmdStatusResponse : public MTCmdStatusResponse
{
public:
    explicit APPCFG_MTCmdStatusResponse(uint8_t cmdid) : MTCmdStatusResponse(cmdid, APP_CFG) {}
};

class APPCFG_MTCmdNotification : public MTCmdNotification
{
public:
    explicit APPCFG_MTCmdNotification(uint8_t cmdid) : MTCmdNotification(cmdid, APP_CFG) {}
};


// **************************************************************************************************************
// APP CFG classes - application configuration


class AppCnfBdbSetChannelRequest : public APPCFG_MTCmdRequest
{
public:
    AppCnfBdbSetChannelRequest() : APPCFG_MTCmdRequest(0x8), isPrimary(0), channel(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(5);

        res[4] = isPrimary;
        WriteUint32(5, channel, res);

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        isPrimary = packet[4];
        channel = ReadUint32(5, packet);

        return 5 == length;
    }

    virtual uint8_t GetLength() const override { return 5; }

    virtual bool ExpectsResponse() const override { return true; } // I'm not really sure about this, the documentation has no response, but there is a 'response' nevertheless

    uint8_t isPrimary;
    uint32_t channel; // a channel mask: 0x00000800 is 11, 0x00001000 is 12 ... 0x00080000 is 19
};

class AppCnfBdbSetChannelResponse : public APPCFG_MTCmdStatusResponse
{
public:
    AppCnfBdbSetChannelResponse() : APPCFG_MTCmdStatusResponse(0x8) {}
};

class AppCnfBdbStartCommissioningRequest : public APPCFG_MTCmdRequest
{
public:
    // default: network formation
    AppCnfBdbStartCommissioningRequest() : APPCFG_MTCmdRequest(0x5), mode(0x4) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(1);

        res[4] = mode;

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        mode = packet[4];

        return 1 == length;
    }

    virtual uint8_t GetLength() const override { return 1; }

    virtual bool ExpectsResponse() const override { return true; }
/*
0x00 Initialization
0x01 TouchLink - BDB_COMMISSIONING_MODE_INITIATOR_TL
0x02 Network Steering - BDB_COMMISSIONING_MODE_NWK_STEERING
0x04 Network Formation - BDB_COMMISSIONING_MODE_NWK_FORMATION
0x08 Finding and Binding - BDB_COMMISSIONING_MODE_FINDING_BINDING
*/
    uint8_t mode;
};

class AppCnfBdbStartCommissioningResponse : public APPCFG_MTCmdStatusResponse
{
public:
    AppCnfBdbStartCommissioningResponse() : APPCFG_MTCmdStatusResponse(0x5) {}
};

class AppCnfBdbSetTCRequireKeyExchangeRequest : public APPCFG_MTCmdRequest
{
public:
    AppCnfBdbSetTCRequireKeyExchangeRequest() : APPCFG_MTCmdRequest(0x6), bdbTrustCenterRequireKeyExchange(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(1);

        res[4] = bdbTrustCenterRequireKeyExchange;

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        bdbTrustCenterRequireKeyExchange = packet[4];

        return 1 == length;
    }

    virtual uint8_t GetLength() const override { return 1; }

    virtual bool ExpectsResponse() const override { return true; }

    uint8_t bdbTrustCenterRequireKeyExchange;
};

class AppCnfBdbSetTCRequireKeyExchangeResponse : public APPCFG_MTCmdStatusResponse
{
public:
    AppCnfBdbSetTCRequireKeyExchangeResponse() : APPCFG_MTCmdStatusResponse(0x6) {}
};


class AppCnfBdbCommissioningNotification : public APPCFG_MTCmdNotification
{
public:
    AppCnfBdbCommissioningNotification() : APPCFG_MTCmdNotification(0x80), status(0), commissioningMode(0), remainingCommissioningModes(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(3);

        packet[4] = status;
        packet[5] = commissioningMode;
        packet[6] = remainingCommissioningModes;

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        status = packet[4];
        commissioningMode = packet[5];
        remainingCommissioningModes = packet[6];

        return 3 == length;
    }

    virtual uint8_t GetLength() const override { return 3; }


    uint8_t status;
    /* 0x00 BDB_COMMISSIONING_SUCCESS
       0x01 BDB_COMMISSIONING_IN_PROGRESS
       0x02 BDB_COMMISSIONING_NO_NETWORK
       0x03 BDB_COMMISSIONING_TL_TARGET_FAILURE
       0x04 BDB_COMMISSIONING_TL_NOT_AA_CAPABLE
       0x05 BDB_COMMISSIONING_TL_NO_SCAN_RESPONSE
       0x06 BDB_COMMISSIONING_TL_NOT_PERMITTED
       0x07 BDB_COMMISSIONING_TCLK_EX_FAILURE
       0x08 BDB_COMMISSIONING_FORMATION_FAILURE
       0x09 BDB_COMMISSIONING_FB_TARGET_IN_PROGRESS
       0x0A BDB_COMMISSIONING_FB_INITIATOR_IN_PROGRESS
       0x0B BDB_COMMISSIONING_FB_NO_IDENTIFY_QUERY_RESPONSE
       0x0C BDB_COMMISSIONING_FB_BINDING_TABLE_FULL
       0x0D BDB_COMMISSIONING_NETWORK_RESTORED
       0x0E BDB_COMMISSIONING_FAILURE
       */

    uint8_t commissioningMode;
    /* 0x00 BDB_COMMISSIONING_INITIALIZATION
       0x01 BDB_COMMISSIONING_NWK_STEERING
       0x02 BDB_COMMISSIONING_FORMATION
       0x03 BDB_COMMISSIONING_FINDING_BINDING
       0x04 BDB_COMMISSIONING_TOUCHLINK
       0x05 BDB_COMMISSIONING_PARENT_LOST
       */

    uint8_t remainingCommissioningModes;
    /* bitmask of:
    0x01 BDB_COMMISSIONING_MODE_INITIATOR_TL
    0x02 BDB_COMMISSIONING_MODE_NWK_STEERING
    0x04 BDB_COMMISSIONING_MODE_NWK_FORMATION
    0x08 BDB_COMMISSIONING_MODE_FINDING_BINDING
    0x10 BDB_COMMISSIONING_MODE_INITIALIZATION
    0x20 BDB_COMMISSIONING_MODE_PARENT_LOST
    */
};




}

#endif
