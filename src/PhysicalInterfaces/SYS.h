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

#ifndef SYS_H_
#define SYS_H_

#include <cstdint>

#include "ZigbeeIds.h"
#include "IZigbeeInterface.h"
#include "MT.h"

namespace ZigbeeCommands
{

class SYS_MTCmdRequest : public MTCmdRequest
{
public:
    explicit SYS_MTCmdRequest(uint8_t cmdid) : MTCmdRequest(cmdid, SYS) {}
};

class SYS_MTCmdResponse : public MTCmdResponse
{
public:
    explicit SYS_MTCmdResponse(uint8_t cmdid) : MTCmdResponse(cmdid, SYS) {}
};

class SYS_MTCmdStatusResponse : public MTCmdStatusResponse
{
public:
    explicit SYS_MTCmdStatusResponse(uint8_t cmdid) : MTCmdStatusResponse(cmdid, SYS) {}
};

class SYS_MTCmdNotification : public MTCmdNotification
{
public:
    explicit SYS_MTCmdNotification(uint8_t cmdid) : MTCmdNotification(cmdid, SYS) {}
};


// **************************************************************************************************************
// SYS classes

class SysPingSend : public SYS_MTCmdRequest
{
public:
    SysPingSend() : SYS_MTCmdRequest(1) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(0);
        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        return 0 == length;
    }

    virtual bool ExpectsResponse() const override { return true; }
};

class SysPingResp : public SYS_MTCmdResponse
{
public:
    SysPingResp() : SYS_MTCmdResponse(1), capabilities(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(2);

        WriteUint16(4, capabilities, packet);

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        capabilities = ReadUint16(4, packet);

        return 2 == length;
    }

    virtual uint8_t GetLength() const override { return 2; }

    bool SYSCapable() const { return capabilities & 0x0001; }
    bool MACCapable() const { return capabilities & 0x0002; }
    bool NWKCapable() const { return capabilities & 0x0004; }
    bool AFCapable() const { return capabilities & 0x0008; }
    bool ZDOCapable() const { return capabilities & 0x0010; }
    bool SAPICapable() const { return capabilities & 0x0020; }
    bool UTILCapable() const { return capabilities & 0x0040; }
    bool DEBUGCapable() const { return capabilities & 0x0080; }
    bool APPCapable() const { return capabilities & 0x0100; }
    bool ZOADCapable() const { return capabilities & 0x1000; }

    uint16_t capabilities;
};


class SysOsalNVItemInitRequest : public SYS_MTCmdRequest
{
public:
    SysOsalNVItemInitRequest() : SYS_MTCmdRequest(0x7), id(0), itemLen(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(GetLength());

        WriteUint16(4, id, packet);
        WriteUint16(6, itemLen, packet);
        packet[8] = value.size();

        std::copy(value.begin(), value.end(), packet.begin() + 9);

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        id = ReadUint16(4, packet);
        itemLen = ReadUint16(6, packet);

        const uint8_t len = packet[8];

        value.resize(len);
        if (GetLength() != length) return false;
        std::copy(packet.begin() + 9, packet.begin() + 9 + len, value.begin());

        return true;
    }

    virtual uint8_t GetLength() const override { return 5 + value.size(); }

    virtual bool ExpectsResponse() const override { return true; }

    //0x3 - ZCD_NV_STARTUP_OPTION
    //0x87 - ZCD_NV_LOGICAL_TYPE
    //0x8F - ZCD_NV_ZDO_DIRECT_CB
    //0x83 - ZCD_NV_PANID - two bytes, default 0xFFFF meaning 'don't care' - assign something between 0 and 0x3FFF

    uint16_t id;
    uint16_t itemLen;

    std::vector<uint8_t> value;
};


class  SysOsalNVItemInitResponse : public SYS_MTCmdStatusResponse
{
public:
    SysOsalNVItemInitResponse() : SYS_MTCmdStatusResponse(0x7) {}

    /*
    0x00 = Item already exists, no action taken
    0x09 = Success, item created and initialized
    0x0A = Initialization failed, item not created
    */
};


class SysOsalNVReadRequest : public SYS_MTCmdRequest
{
public:
    SysOsalNVReadRequest() : SYS_MTCmdRequest(0x8), id(0), offset(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(3);

        WriteUint16(4, id, packet);
        packet[6] = offset;

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        id = ReadUint16(4, packet);
        offset = packet[6];

        return 3 == length;
    }

    virtual uint8_t GetLength() const override { return 3; }

    virtual bool ExpectsResponse() const override { return true; }

    uint16_t id;
    uint8_t offset;
};


class  SysOsalNVReadResponse : public SYS_MTCmdResponse
{
public:
    SysOsalNVReadResponse() : SYS_MTCmdResponse(0x8), status(Zigbee::ZigbeeReturnIds::ZSuccess) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(GetLength());

        packet[4] = static_cast<uint8_t>(status);
        packet[5] = value.size();
        std::copy(value.begin(), value.end(), packet.begin() + 6);

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        status = static_cast<Zigbee::ZigbeeReturnIds>(packet[4]);
        const uint8_t len = packet[5];
        value.resize(len);

        if (GetLength() != length) return false;
        std::copy(packet.begin() + 6, packet.begin() + 6 + len, value.begin());

        return true;
    }

    virtual uint8_t GetLength() const override { return 2 + value.size(); }

    Zigbee::ZigbeeReturnIds status;
    std::vector<uint8_t> value;
};


class SysOsalNVWriteRequest : public SYS_MTCmdRequest
{
public:
    SysOsalNVWriteRequest() : SYS_MTCmdRequest(0x9), id(0), offset(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(GetLength());

        WriteUint16(4, id, packet);
        packet[6] = offset;
        packet[7] = value.size();

        std::copy(value.begin(), value.end(), packet.begin() + 8);

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        id = ReadUint16(4, packet);
        offset = packet[6];
        const uint8_t len = packet[7];

        value.resize(len);
        if (GetLength() != length) return false;
        std::copy(packet.begin() + 8, packet.begin() + 8 + len, value.begin());

        return true;
    }

    virtual uint8_t GetLength() const override { return 4 + value.size(); }

    virtual bool ExpectsResponse() const override { return true; }

    //0x3 - ZCD_NV_STARTUP_OPTION
    //0x21 - ZCD_NV_NIB
    //0x87 - ZCD_NV_LOGICAL_TYPE
    //0x8F - ZCD_NV_ZDO_DIRECT_CB
    //0x83 - ZCD_NV_PANID - two bytes, default 0xFFFF meaning 'don't care' - assign something between 0 and 0x3FFF
    //0x89 - ZCD_NV_NWKMGR_ADDR
    //0x003A ZCD_NV_NWK_ACTIVE_KEY_INFO Active Network Key
    //0x003B ZCD_NV_NWK_ALTERN_KEY_INFO Alternate network key
    //0x0101 ZCD_NV_TCLK_TABLE_START First element of TCLK table
    //0x01FF ZCD_NV_TCLK_TABLE_END Last element of TCLK table
    //0x0201 ZCD_NV_APS_LINK_KEY_DATA_START First element of APS Link Key table
    //0x02FF ZCD_NV_APS_LINK_KEY_DATA_END Last element of APS Link Key table
    //0x0301 ZCD_NV_MASTER_KEY_DATA_START First element of Master Key table
    //0x03FF ZCD_NV_MASTER_KEY_DATA_END Last element of Master Key table

    uint16_t id;

    uint8_t offset;
    std::vector<uint8_t> value;
};


class  SysOsalNVWriteResponse : public SYS_MTCmdStatusResponse
{
public:
    SysOsalNVWriteResponse() : SYS_MTCmdStatusResponse(0x9) {}
};



class SysResetRequest : public SYS_MTCmdRequest
{
public:
    SysResetRequest() : SYS_MTCmdRequest(0), type(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(1);

        res[4] = type;

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        type = packet[4];

        return 1 == length;
    }

    virtual uint8_t GetLength() const override { return 1; }

    virtual bool ExpectsResponse() const override { return true; } // I'm not really sure about this, the documentation has no response, but there is a 'response' nevertheless

    uint8_t type; // 0 - hard reset, otherwise soft reset
};

class SysResetNotification : public SYS_MTCmdNotification
{
public:
    SysResetNotification() : SYS_MTCmdNotification(0x80), reason(0), transportRev(0), product(0), majorRel(0), minorRel(0), hwRev(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(6);

        packet[4] = reason;
        packet[5] = transportRev;
        packet[6] = product;
        packet[7] = majorRel;
        packet[8] = minorRel;
        packet[9] = hwRev;

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        reason = packet[4];
        transportRev = packet[5];
        product = packet[6];
        majorRel = packet[7];
        minorRel = packet[8];
        hwRev = packet[9];

        return 6 == length;
    }

    virtual uint8_t GetLength() const override { return 6; }

    uint8_t reason;
    uint8_t transportRev;
    uint8_t product;
    uint8_t majorRel;
    uint8_t minorRel;
    uint8_t hwRev;
};


}

#endif
