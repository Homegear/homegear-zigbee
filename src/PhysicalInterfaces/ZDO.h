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

#ifndef ZDO_H_
#define ZDO_H_

#include <cstdint>

#include "ZigbeeIds.h"
#include "IZigbeeInterface.h"
#include "MT.h"

namespace ZigbeeCommands
{

class ZDO_MTCmdRequest : public MTCmdRequest
{
public:
    explicit ZDO_MTCmdRequest(uint8_t cmdid) : MTCmdRequest(cmdid, ZDO) {}
};

class ZDO_MTCmdResponse : public MTCmdResponse
{
public:
    explicit ZDO_MTCmdResponse(uint8_t cmdid) : MTCmdResponse(cmdid, ZDO) {}
};

class ZDO_MTCmdStatusResponse : public MTCmdStatusResponse
{
public:
    explicit ZDO_MTCmdStatusResponse(uint8_t cmdid) : MTCmdStatusResponse(cmdid, ZDO) {}
};

class ZDO_MTCmdStatusUint8Response : public MTCmdStatusUint8Response
{
public:
    explicit ZDO_MTCmdStatusUint8Response(uint8_t cmdid) : MTCmdStatusUint8Response(cmdid, ZDO) {}
};


class ZDO_MTCmdNotification : public MTCmdNotification
{
public:
    explicit ZDO_MTCmdNotification(uint8_t cmdid) : MTCmdNotification(cmdid, ZDO) {}
};

// ***************************************************************************************************************
// ZDO classes - Zigbee Device Object

class ZDONwkAddrRequest : public ZDO_MTCmdRequest
{
public:
    ZDONwkAddrRequest() : ZDO_MTCmdRequest(0x00), addrIEEE(0), reqType(0), startIndex(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(10);

        WriteUint64(4, addrIEEE, res);
        res[12] = reqType;
        res[13] = startIndex;

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        addrIEEE = ReadUint64(4, packet);
        reqType = packet[12];
        startIndex = packet[13];

        return 10 == length;
    }

    virtual uint8_t GetLength() const override { return 10; }

    virtual bool ExpectsResponse() const override { return true; }

    uint64_t addrIEEE;
    uint8_t reqType;
    uint8_t startIndex;
};

class ZDONwkAddrResponse : public ZDO_MTCmdStatusResponse
{
public:
    ZDONwkAddrResponse() : ZDO_MTCmdStatusResponse(0x00) {}
};


class ZDOIEEEAddrRequest : public ZDO_MTCmdRequest
{
public:
    ZDOIEEEAddrRequest() : ZDO_MTCmdRequest(0x01), shortAddr(0), reqType(0), startIndex(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(4);

        WriteUint16(4, shortAddr, res);
        res[6] = reqType;
        res[7] = startIndex;

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        shortAddr = ReadUint16(4, packet);
        reqType = packet[6];
        startIndex = packet[7];

        return 4 == length;
    }

    virtual uint8_t GetLength() const override { return 4; }

    virtual bool ExpectsResponse() const override { return true; }

    uint16_t shortAddr;
    uint8_t reqType;
    uint8_t startIndex;
};

class ZDOIEEEAddrResponse : public ZDO_MTCmdStatusResponse
{
public:
    ZDOIEEEAddrResponse() : ZDO_MTCmdStatusResponse(0x01) {}
};

class ZDONodeDescRequest : public ZDO_MTCmdRequest
{
public:
    ZDONodeDescRequest() : ZDO_MTCmdRequest(0x02), dstAddr(0), addrOfInterest(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(4);

        WriteUint16(4, dstAddr, res);
        WriteUint16(6, addrOfInterest, res);

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        dstAddr = ReadUint16(4, packet);
        addrOfInterest = ReadUint16(6, packet);

        return 4 == length;
    }

    virtual uint8_t GetLength() const override { return 4; }

    virtual bool ExpectsResponse() const override { return true; }

    uint16_t dstAddr;
    uint16_t addrOfInterest;
};

class ZDONodeDescResponse : public ZDO_MTCmdStatusResponse
{
public:
    ZDONodeDescResponse() : ZDO_MTCmdStatusResponse(0x02) {}
};


class ZDOPowerDescRequest : public ZDO_MTCmdRequest
{
public:
    ZDOPowerDescRequest() : ZDO_MTCmdRequest(0x03), dstAddr(0), addrOfInterest(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(4);

        WriteUint16(4, dstAddr, res);
        WriteUint16(6, addrOfInterest, res);

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        dstAddr = ReadUint16(4, packet);
        addrOfInterest = ReadUint16(6, packet);

        return 4 == length;
    }

    virtual uint8_t GetLength() const override { return 4; }

    virtual bool ExpectsResponse() const override { return true; }

    uint16_t dstAddr;
    uint16_t addrOfInterest;
};

class ZDOPowerDescResponse : public ZDO_MTCmdStatusResponse
{
public:
    ZDOPowerDescResponse() : ZDO_MTCmdStatusResponse(0x03) {}
};


class ZDOSimpleDescRequest : public ZDO_MTCmdRequest
{
public:
    ZDOSimpleDescRequest() : ZDO_MTCmdRequest(0x04), dstAddr(0), addrOfInterest(0), endPoint(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(5);

        WriteUint16(4, dstAddr, res);
        WriteUint16(6, addrOfInterest, res);
        res[8] = endPoint;

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        dstAddr = ReadUint16(4, packet);
        addrOfInterest = ReadUint16(6, packet);
        endPoint = packet[8];

        return 5 == length;
    }

    virtual uint8_t GetLength() const override { return 5; }

    virtual bool ExpectsResponse() const override { return true; }

    uint16_t dstAddr;
    uint16_t addrOfInterest;
    uint8_t endPoint;
};

class ZDOSimpleDescResponse : public ZDO_MTCmdStatusResponse
{
public:
    ZDOSimpleDescResponse() : ZDO_MTCmdStatusResponse(0x04) {}
};

class ZDOActiveEndPointRequest : public ZDO_MTCmdRequest
{
public:
    ZDOActiveEndPointRequest() : ZDO_MTCmdRequest(0x05), dstAddr(0), addrOfInterest(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(4);

        WriteUint16(4, dstAddr, res);
        WriteUint16(6, addrOfInterest, res);

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        dstAddr = ReadUint16(4, packet);
        addrOfInterest = ReadUint16(6, packet);

        return 4 == length;
    }

    virtual uint8_t GetLength() const override { return 4; }

    virtual bool ExpectsResponse() const override { return true; }

    uint16_t dstAddr; // specifies NWK address of the device generating the inquiry
    uint16_t addrOfInterest; // Specifies NWK address of the destination device being queried
};

class ZDOActiveEndPointResponse : public ZDO_MTCmdStatusResponse
{
public:
    ZDOActiveEndPointResponse() : ZDO_MTCmdStatusResponse(0x05) {}
};


class ZDOBindingRequest :  public ZDO_MTCmdRequest
{
public:
    explicit ZDOBindingRequest(uint8_t cmd) : ZDO_MTCmdRequest(cmd), DstAddr(0), SrcAddress(0), SrcEndpoint(0), ClusterId(0), DstAddrMode(0x3), DstAddressIEEE(0), DstAddressShort(0), DstEndpoint(0) {}


    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(GetLength());

        WriteUint16(4, DstAddr, res);
        WriteUint64(6, SrcAddress, res);
        res[14] = SrcEndpoint;
        WriteUint16(15, ClusterId, res);
        res[17] = DstAddrMode;

        if (0x03 == DstAddrMode)
        {
            WriteUint64(18, DstAddressIEEE, res);
            res[26] = DstEndpoint;
        }
        else
        {
            WriteUint16(18, DstAddressShort, res);
        }

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        DstAddr = ReadUint16(4, packet);
        SrcAddress = ReadUint64(6, packet);
        SrcEndpoint = packet[14];
        ClusterId = ReadUint16(15, packet);
        DstAddrMode = packet[17];

        if (0x03 == DstAddrMode)
        {
            DstAddressIEEE = ReadUint64(18, packet);
            DstEndpoint = packet[26];
        }
        else
        {
            DstAddressShort = ReadUint16(18, packet);
        }

        return GetLength() == length;
    }

    virtual uint8_t GetLength() const override { return 14 + (0x03 == DstAddrMode ? 8 : 2) + (0x03 == DstAddrMode ? 1 : 0); }

    virtual bool ExpectsResponse() const override { return true; }

    uint16_t DstAddr; // Specifies destination address of the device generating the bind request.
    uint64_t SrcAddress; // Specifies 64 bit Binding source IEEE address.
    uint8_t SrcEndpoint; // Specifies the binding source endpoint.
    uint16_t ClusterId; // Specifies cluster Id to match in messages.
    uint8_t DstAddrMode; // Specifies 64 bit Binding destination address mode
    /*
    ADDRESS_NOT_PRESENT 0x00
    GROUP_ADDRESS 0x01
    ADDRESS_16_BIT 0x02
    ADDRESS_64_BIT 0x03
    BROADCAST      0xFF
    */

    uint64_t DstAddressIEEE; // Specifies 64 bit Binding destination IEEE address. Not to be confused with DstAddr
    uint16_t DstAddressShort;

    uint8_t DstEndpoint; // Specifies the binding destination endpoint
};

class ZDOBindRequest : public ZDOBindingRequest
{
public:
    ZDOBindRequest() : ZDOBindingRequest(0x21) {}
};

class ZDOBindResponse : public ZDO_MTCmdStatusResponse
{
public:
    ZDOBindResponse() : ZDO_MTCmdStatusResponse(0x21) {}
};

class ZDOUnbindRequest : public ZDOBindingRequest
{
public:
    ZDOUnbindRequest() : ZDOBindingRequest(0x22) {}
};

class ZDOUnbindResponse : public ZDO_MTCmdStatusResponse
{
public:
    ZDOUnbindResponse() : ZDO_MTCmdStatusResponse(0x22) {}
};

// Request the binding table from device
class ZDOMgmtBindRequest : public ZDO_MTCmdRequest
{
public:
    ZDOMgmtBindRequest() : ZDO_MTCmdRequest(0x33), DstAddr(0), StartIndex(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(3);

        WriteUint16(4, DstAddr, res);
        res[6] = StartIndex;

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        DstAddr = ReadUint16(4, packet);
        StartIndex = packet[6];

        return 3 == length;
    }

    virtual uint8_t GetLength() const override { return 3; }

    virtual bool ExpectsResponse() const override { return true; }

    uint16_t DstAddr;
    uint8_t StartIndex;
};

class ZDOMgmtBindResponse : public ZDO_MTCmdStatusResponse
{
public:
    ZDOMgmtBindResponse() : ZDO_MTCmdStatusResponse(0x33) {}
};

// this is the one to be used for 'unpairing'
// ZDO_MGMT_LEAVE_REQ
class ZDOMgmtLeaveRequest : public ZDO_MTCmdRequest
{
public:
    ZDOMgmtLeaveRequest() : ZDO_MTCmdRequest(0x34), DstAddr(0), DeviceAddress(0), RemoveChildren_Rejoin(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(11);

        WriteUint16(4, DstAddr, res);
        WriteUint64(6, DeviceAddress, res);
        res[14] = RemoveChildren_Rejoin;

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        DstAddr = ReadUint16(4, packet);
        DeviceAddress = ReadUint64(6, packet);
        RemoveChildren_Rejoin = packet[14];

        return 11 == length;
    }

    virtual uint8_t GetLength() const override { return 11; }

    virtual bool ExpectsResponse() const override { return true; }

    bool RemoveChildren() const { return RemoveChildren_Rejoin & 0x2; }
    bool Rejoin() const { return RemoveChildren_Rejoin & 0x1; }

    uint16_t DstAddr; // Short address of the device that will process the mgmt leave (remote or self)
    uint64_t DeviceAddress; // The 64-bit IEEE address of the entity to be removed from the network or 0x0000000000000000 if the device removes itself from the network.
    uint8_t RemoveChildren_Rejoin; // Specifies actions to be performed by device when leaving the network: Bit 0: rejoin, Bit 1: remove children
};

class ZDOMgmtLeaveResponse : public ZDO_MTCmdStatusResponse
{
public:
    ZDOMgmtLeaveResponse() : ZDO_MTCmdStatusResponse(0x34) {}
};


// apparently this is the one to be used for 'pairing':

// ZDO_MGMT_PERMIT_JOIN_REQ
class ZDOMgmtPermitJoinRequest : public ZDO_MTCmdRequest
{
public:
    ZDOMgmtPermitJoinRequest() : ZDO_MTCmdRequest(0x36), AddrMode(0x02), DstAddr(0), Duration(60), TCSignificance(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(5);

        res[4] = AddrMode;
        WriteUint16(5, DstAddr, res);
        res[7] = Duration;
        res[8] = TCSignificance;

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        AddrMode = packet[4];
        DstAddr = ReadUint16(5, packet);
        Duration = packet[7];
        TCSignificance = packet[8];

        return 5 == length;
    }

    virtual uint8_t GetLength() const override { return 5; }

    virtual bool ExpectsResponse() const override { return true; }

    uint8_t AddrMode; // Destination address type: 0x02 – Address 16 bit, 0xFF – Broadcast
    uint16_t DstAddr; // Specifies the network address of the destination device whose Permit Join information is to be modified
    uint8_t Duration; // Specifies the duration to permit joining. 0 = join disabled. 0xff = join enabled. 0x01-0xfe = number of seconds to permit joining.
    uint8_t TCSignificance; // Trust Center Significance
};

class ZDOMgmtPermitJoinResponse : public ZDO_MTCmdStatusResponse
{
public:
    ZDOMgmtPermitJoinResponse() : ZDO_MTCmdStatusResponse(0x36) {}
};


class ZDOMsgCbRegisterRequest : public ZDO_MTCmdRequest
{
public:
    ZDOMsgCbRegisterRequest() : ZDO_MTCmdRequest(0x3E), ClusterId(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(2);

        WriteUint16(4, ClusterId, res);

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        ClusterId = ReadUint16(4, packet);

        return 2 == length;
    }

    virtual uint8_t GetLength() const override { return 2; }

    virtual bool ExpectsResponse() const override { return true; }

    uint16_t ClusterId;
};

class ZDOMsgCbRegisterResponse : public ZDO_MTCmdStatusResponse
{
public:
    ZDOMsgCbRegisterResponse() : ZDO_MTCmdStatusResponse(0x3E) {}
};

class ZDOMsgCbRemoveRequest : public ZDO_MTCmdRequest
{
public:
    ZDOMsgCbRemoveRequest() : ZDO_MTCmdRequest(0x3F), ClusterId(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(2);

        WriteUint16(4, ClusterId, res);

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        ClusterId = ReadUint16(4, packet);

        return 2 == length;
    }

    virtual uint8_t GetLength() const override { return 2; }

    virtual bool ExpectsResponse() const override { return true; }

    uint16_t ClusterId;
};

class ZDOMsgCbRemoveResponse : public ZDO_MTCmdStatusResponse
{
public:
    ZDOMsgCbRemoveResponse() : ZDO_MTCmdStatusResponse(0x3F) {}
};

class ZDOStartupFromAppRequest : public ZDO_MTCmdRequest
{
public:
    ZDOStartupFromAppRequest() : ZDO_MTCmdRequest(0x40), StartDelay(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(2);

        WriteUint16(4, StartDelay, res);

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        StartDelay = ReadUint16(4, packet);

        return 2 == length;
    }

    virtual uint8_t GetLength() const override { return 2; }

    virtual bool ExpectsResponse() const override { return true; }

    uint16_t StartDelay;
};

class ZDOStartupFromAppResponse : public ZDO_MTCmdStatusUint8Response
{
public:
    ZDOStartupFromAppResponse() : ZDO_MTCmdStatusUint8Response(0x40) {}
};


class ZDOAddrNotification : public ZDO_MTCmdNotification
{
public:
    explicit ZDOAddrNotification(uint8_t cmd) : ZDO_MTCmdNotification(cmd), status(0), IEEEaddr(0), addr(0), startIndex(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(GetLength());

        packet[4] = status;
        WriteUint64(5, IEEEaddr, packet);
        WriteUint16(13, addr, packet);
        packet[15] = startIndex;
        packet[16] = assocDevs.size();

        unsigned int offset = 17;
        for (const uint16_t val : assocDevs)
        {
            WriteUint16(offset, val, packet);
            offset += 2;
        }

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        status = packet[4];
        IEEEaddr = ReadUint64(5, packet);
        addr = ReadUint16(13, packet);
        startIndex = packet[15];
        const unsigned int len = packet[16];
        assocDevs.resize(len);

        if (GetLength() != length) return false;

        unsigned int offset = 17;

        for (auto& val : assocDevs)
        {
            val = ReadUint16(offset, packet);
            offset += 2;
        }

        return true;
    }

    virtual uint8_t GetLength() const override { return 13 + assocDevs.size() * 2; }

    uint8_t status;
    uint64_t IEEEaddr;
    uint16_t addr;
    uint8_t startIndex;
    std::vector<uint16_t> assocDevs;
};

class ZDONwkAddrNotification : public ZDOAddrNotification
{
public:
    ZDONwkAddrNotification() : ZDOAddrNotification(0x80) {}
};

class ZDOIEEEAddrNotification : public ZDOAddrNotification
{
public:
    ZDOIEEEAddrNotification() : ZDOAddrNotification(0x81) {}
};


class ZDONodeDescNotification : public ZDO_MTCmdNotification
{
public:
    ZDONodeDescNotification() : ZDO_MTCmdNotification(0x82), SrcAddr(0), Status(0), NWKAddr(0),
                                LogicalType_ComplexDescriptorAvailable_UserDescriptorAvailable(0), APSFlags_FrequencyBand(0), MacCapabilitiesFlags(0), ManufacturerCode(0),
                                MaxBufferSize(0), MaxInTransferSize(0), ServerMask(0), MaxOutTransferSize(0), DescriptorCapabilities(0)
     {
     }

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(18);

        WriteUint16(4, SrcAddr, packet);
        packet[6] = Status;
        WriteUint16(7, NWKAddr, packet);
        packet[9] = LogicalType_ComplexDescriptorAvailable_UserDescriptorAvailable;
        packet[10] = APSFlags_FrequencyBand;
        packet[11] = MacCapabilitiesFlags;
        WriteUint16(12, ManufacturerCode, packet);
        packet[14] = MaxBufferSize;
        WriteUint16(15, MaxInTransferSize, packet);
        WriteUint16(17, ServerMask, packet);
        WriteUint16(19, MaxOutTransferSize, packet);
        packet[21] = DescriptorCapabilities;

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        SrcAddr = ReadUint16(4, packet);
        Status = packet[6];
        NWKAddr = ReadUint16(7, packet);
        LogicalType_ComplexDescriptorAvailable_UserDescriptorAvailable = packet[9];
        APSFlags_FrequencyBand = packet[10];
        MacCapabilitiesFlags = packet[11];
        ManufacturerCode = ReadUint16(12, packet);
        MaxBufferSize = packet[14];
        MaxInTransferSize = ReadUint16(15, packet);
        ServerMask = ReadUint16(17, packet);
        MaxOutTransferSize = ReadUint16(19, packet);
        DescriptorCapabilities = packet[21];

        return 18 == length;
    }

    virtual uint8_t GetLength() const override { return 18; }

    uint8_t GetType() const { return LogicalType_ComplexDescriptorAvailable_UserDescriptorAvailable & 0x3; }
    bool IsRouter() const { return 0x1 == GetType(); }
    bool IsEndDevice() const { return 0x2 == GetType(); }

    bool GetComplexDescriptorAvailable() const { return LogicalType_ComplexDescriptorAvailable_UserDescriptorAvailable & 0x8; }
    bool GetUserDescriptorAvailable() const { return LogicalType_ComplexDescriptorAvailable_UserDescriptorAvailable & 0x10; }

    bool IsPANCoordinatorCapable() const { return 0x1 & MacCapabilitiesFlags; }
    bool IsFullFunctionDevice() const { return 0x2 & MacCapabilitiesFlags; }
    bool IsMainsPowerDevice() const { return 0x4 & MacCapabilitiesFlags; }
    bool IsReceiverWhenIdleDevice() const { return 0x8 & MacCapabilitiesFlags; }
    bool IsSecurityCapable() const { return 0x40 & MacCapabilitiesFlags; }
    bool AllocateAddress() const { return 0x80 & MacCapabilitiesFlags; }

    bool IsPrimaryTrustCenter() const { return 0x1 & ServerMask; }
    bool IsBackupTrustCenter() const { return 0x2 & ServerMask; }
    bool IsPrimaryBindingTableCacheCenter() const { return 0x4 & ServerMask; }
    bool IsBackupBindingTableCacheCenter() const { return 0x8 & ServerMask; }
    bool IsPrimaryDiscoveryCache() const { return 0x10 & ServerMask; }
    bool IsBackupDiscoveryCache() const { return 0x20 & ServerMask; }
    bool IsNetworkManager() const { return 0x40 & ServerMask; }

    bool IsExtendedActiveEndpointsListAvailable() const { return 0x1 & DescriptorCapabilities; }
    bool IsSimpleDescriptorListAvailable() const { return 0x2 & DescriptorCapabilities; }

    uint16_t SrcAddr;
    uint8_t Status;
    uint16_t NWKAddr;

    uint8_t LogicalType_ComplexDescriptorAvailable_UserDescriptorAvailable;
    uint8_t APSFlags_FrequencyBand;
    uint8_t MacCapabilitiesFlags;
    uint16_t ManufacturerCode;
    uint8_t MaxBufferSize;
    uint16_t MaxInTransferSize;
    uint16_t ServerMask;
    uint16_t MaxOutTransferSize;
    uint8_t DescriptorCapabilities;
};



class ZDOPowerDescNotification : public ZDO_MTCmdNotification
{
public:
    ZDOPowerDescNotification() : ZDO_MTCmdNotification(0x83), SrcAddr(0), Status(0), NWKAddr(0),
                                CurrentPowerMode_AvailablePowerSources(0), CurrentPowerSource_CurrentPowerSourceLevel(0)
     {
     }

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(7);

        WriteUint16(4, SrcAddr, packet);
        packet[6] = Status;
        WriteUint16(7, NWKAddr, packet);

        packet[9] = CurrentPowerMode_AvailablePowerSources;
        packet[10] = CurrentPowerSource_CurrentPowerSourceLevel;

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        SrcAddr = ReadUint16(4, packet);
        Status = packet[6];
        NWKAddr = ReadUint16(7, packet);

        CurrentPowerMode_AvailablePowerSources = packet[9];
        CurrentPowerSource_CurrentPowerSourceLevel = packet[10];

        return 7 == length;
    }

    virtual uint8_t GetLength() const override { return 7; }

    bool CurrentPowerConstantMains() const { return CurrentPowerSource_CurrentPowerSourceLevel & 0x1; }
    bool CurrentPowerRechargeableBattery() const { return CurrentPowerSource_CurrentPowerSourceLevel & 0x2; }
    bool CurrentPowerDisposableBattery() const { return CurrentPowerSource_CurrentPowerSourceLevel & 0x4; }

    bool CurrentPowerBattery() const { return 0 != (CurrentPowerSource_CurrentPowerSourceLevel & 0x6); }

    uint8_t GetChargeLevel() const { return CurrentPowerSource_CurrentPowerSourceLevel >> 12; }

    uint16_t SrcAddr;
    uint8_t Status;
    uint16_t NWKAddr;

    uint8_t CurrentPowerMode_AvailablePowerSources;
    uint8_t CurrentPowerSource_CurrentPowerSourceLevel;
};


class ZDOSimpleDescNotification : public ZDO_MTCmdNotification
{
public:
    ZDOSimpleDescNotification() : ZDO_MTCmdNotification(0x84), SrcAddr(0), Status(0), NWKAddr(0),
                Len(8), Endpoint(0), ProfileId(0), DeviceId(0), DeviceVersion(0)
     {
     }

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(GetLength());

        WriteUint16(4, SrcAddr, packet);
        packet[6] = Status;
        WriteUint16(7, NWKAddr, packet);

        packet[9] = Len;

        if (Len > 1)
        {
            packet[10] = Endpoint;
            if (Len > 2)
            {
                WriteUint16(11, ProfileId, packet);
                if (Len > 4)
                {
                    WriteUint16(13, DeviceId, packet);
                    if (Len > 5)
                    {
                        packet[15] = DeviceVersion;

                        if (Len > 6)
                        {
                            packet[16] = InClusterList.size();
                            if (Len > 6 + 2 * InClusterList.size())
                            {
                                for (unsigned int i = 0; i < InClusterList.size(); ++i)
                                    WriteUint16(17 + 2 * i, InClusterList[i], packet);

                                if (Len > 7 + 2 * InClusterList.size())
                                {
                                    packet[17 + InClusterList.size()] = OutClusterList.size();

                                    if (Len >= 7 + 2 * (InClusterList.size() + OutClusterList.size()))
                                    {
                                        for (unsigned int i = 0; i < OutClusterList.size(); ++i)
                                            WriteUint16(18 + 2 * (InClusterList.size()  + i), OutClusterList[i], packet);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        SrcAddr = ReadUint16(4, packet);
        Status = packet[6];
        NWKAddr = ReadUint16(7, packet);

        Len = packet[9];
        if (Len > 1)
        {
            Endpoint = packet[10];

            if (Len > 2)
            {
                ProfileId = ReadUint16(11, packet);
                if (Len > 4)
                {
                    DeviceId = ReadUint16(13, packet);
                    if (Len > 5)
                    {
                        DeviceVersion = packet[15];
                        if (Len > 6)
                        {
                            const int len = packet[16];
                            InClusterList.resize(len);
                            if (Len > 6 + 2 * len)
                            {
                                for (int i = 0; i < len; ++i)
                                    InClusterList[i] = ReadUint16(17 + 2 * i, packet);

                                if (Len > 7 + 2 * len)
                                {
                                    const int len2 = packet[17 + 2 * len];
                                    OutClusterList.resize(len2);
                                    if (Len >= 7 + 2 * len + 2 * len2)
                                    {
                                        for (int i = 0; i < len2; ++i)
                                            OutClusterList[i] = ReadUint16(18 + 2 * (len + i), packet);
                                    }
                                    else OutClusterList.clear();
                                }
                                else OutClusterList.clear();
                            }
                            else
                            {
                                InClusterList.clear();
                                OutClusterList.clear();
                            }
                        }
                        else
                        {
                            InClusterList.clear();
                            OutClusterList.clear();
                        }
                    }
                    else
                    {
                        DeviceVersion = 0;
                        InClusterList.clear();
                        OutClusterList.clear();
                    }
                }
                else
                {
                    DeviceId = 0;
                    DeviceVersion = 0;
                    InClusterList.clear();
                    OutClusterList.clear();
                }
            }
            else
            {
                ProfileId = 0;
                DeviceId = 0;
                DeviceVersion = 0;
                InClusterList.clear();
                OutClusterList.clear();
            }
        }
        else
        {
            Endpoint = 0;
            ProfileId = 0;
            DeviceId = 0;
            DeviceVersion = 0;
            InClusterList.clear();
            OutClusterList.clear();
        }

        return GetLength() == length;
    }

    virtual uint8_t GetLength() const override { return 6 + Len; }


    uint8_t GetDeviceGroup() const { return (DeviceId & 0xff00) >> 8; }

    bool IsHomeAutomation() const { return 0x0104 == ProfileId; }
    bool IsCommercialBuildingAutomation() const { return 0x0105 == ProfileId; }
    bool IsIndustrialPlantMonitoring() const { return 0x0101 == ProfileId; }
    bool IsTelecom() const { return 0x0107 == ProfileId; }
    bool IsPersonalHomeAndHospitalCare() const { return 0x0108 == ProfileId; }
    bool IsAdvancedMeteringInitiative() const { return 0x0109 == ProfileId; }

    bool IsGenericGroup() const { return GetDeviceGroup() == 0x0; }
    bool IsLightningGroup() const { return GetDeviceGroup() == 0x1; }
    bool IsClosuresGroup() const { return GetDeviceGroup() == 0x2; }
    bool IsHVACGroup() const { return GetDeviceGroup() == 0x3; }
    bool IsAlarmGroup() const { return GetDeviceGroup() == 0x4; }

    bool IsZCLSupported() const { return ProfileId < 0x8000 && DeviceId < 0xC000; }

    bool IsManufacturerSpecific() const { return ProfileId >= 0xBF00; }
    bool IsPublicProfile() const { return ProfileId <= 0x7FFF; }

    uint16_t SrcAddr;
    uint8_t Status;
    uint16_t NWKAddr;

    uint8_t Len;
    uint8_t Endpoint;
    uint16_t ProfileId;
    uint16_t DeviceId;
    uint8_t DeviceVersion;

    std::vector<uint16_t> InClusterList;
    std::vector<uint16_t> OutClusterList;
};



class ZDOActiveEndPointNotification : public ZDO_MTCmdNotification
{
public:
    ZDOActiveEndPointNotification() : ZDO_MTCmdNotification(0x85), SrcAddr(0), Status(0), NWKAddr(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(GetLength());

        WriteUint16(4, SrcAddr, packet);
        packet[6] = Status;
        WriteUint16(7, NWKAddr, packet);
        packet[9] = endPoints.size();

        unsigned int offset = 10;
        for (uint8_t ep : endPoints)
        {
            packet[offset++] = ep;
        }

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        SrcAddr = ReadUint16(4, packet);
        Status = packet[6];
        NWKAddr = ReadUint16(7, packet);
        const int len = packet[9];
        endPoints.resize(len);

        if (GetLength() != length)
            return false;

        int offset = 10;
        for (uint8_t& ep : endPoints)
            ep = packet[offset++];

        return true;
    }

    virtual uint8_t GetLength() const override { return 6 + endPoints.size(); }

    uint16_t SrcAddr;
    uint8_t Status;
    uint16_t NWKAddr;

    std::vector<uint8_t> endPoints;
};


class ZDOMgmtNotification : public ZDO_MTCmdNotification
{
public:
    explicit ZDOMgmtNotification(uint8_t cmd) : ZDO_MTCmdNotification(cmd), srcAddress(0), status(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(3);

        WriteUint16(4, srcAddress, packet);
        packet[6] = status;

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        srcAddress = ReadUint16(4, packet);
        status = packet[6];

        return 3 == length;
    }

    virtual uint8_t GetLength() const override { return 3; }

    uint16_t srcAddress;
    uint8_t status;
};


class ZDOEndDeviceBindNotification : public ZDOMgmtNotification
{
public:
    ZDOEndDeviceBindNotification() : ZDOMgmtNotification(0xA0) {}
};

class ZDOBindNotification : public ZDOMgmtNotification
{
public:
    ZDOBindNotification() : ZDOMgmtNotification(0xA1) {}
};

class ZDOUnbindNotification : public ZDOMgmtNotification
{
public:
    ZDOUnbindNotification() : ZDOMgmtNotification(0xA2) {}
};


class ZDOMgmtBindResponseNotification : public ZDO_MTCmdNotification
{
public:
    ZDOMgmtBindResponseNotification() : ZDO_MTCmdNotification(0xB3), srcAddress(0), status(0), BindTableEntries(0), StartIndex(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(GetLength());

        WriteUint16(4, srcAddress, packet);
        packet[6] = status;
        packet[7] = BindTableEntries;
        packet[8] = StartIndex;
        packet[9] = BindTableList.size();

        unsigned int offset = 10;

        for (const BindTableEntry& entry : BindTableList)
        {
            WriteUint64(offset, entry.SrcAddr, packet);
            offset += 8;
            packet[offset++] = entry.SrcEndpoint;
            WriteUint16(offset, entry.ClusterId, packet);
            offset += 2;
            packet[offset++] = entry.DstAddrMode;
            WriteUint64(offset, entry.DstAddr, packet);
            offset += 8;
            packet[offset++] = entry.DstEndpoint;
        }

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        srcAddress = ReadUint16(4, packet);
        status = packet[6];

        BindTableEntries = packet[7];
        StartIndex = packet[8];

        const int len = packet[9];
        BindTableList.resize(len);

        if (6 + len * 21 != length) return false;

        unsigned int offset = 10;
        for (BindTableEntry& entry : BindTableList)
        {
            entry.SrcAddr = ReadUint64(offset, packet);
            offset += 8;
            entry.SrcEndpoint = packet[offset++];
            entry.ClusterId = ReadUint16(offset, packet);
            offset += 2;
            entry.DstAddrMode = packet[offset++];
            entry.DstAddr = ReadUint64(offset, packet);
            offset += 8;
            entry.DstEndpoint = packet[offset++];
        }

        return true;
    }

    virtual uint8_t GetLength() const override { return 6 + BindTableList.size() * 21; }

    struct BindTableEntry
    {
        uint64_t SrcAddr = 0;
        uint8_t SrcEndpoint = 0;
        uint16_t ClusterId = 0;
        uint8_t DstAddrMode = 3;
        uint64_t DstAddr = 0;
        uint8_t DstEndpoint = 0;
    };

    uint16_t srcAddress;
    uint8_t status;

    uint8_t BindTableEntries;
    uint8_t StartIndex;

    std::vector<BindTableEntry> BindTableList;
};



class ZDOMgmtLeaveNotification : public ZDOMgmtNotification
{
public:
    ZDOMgmtLeaveNotification() : ZDOMgmtNotification(0xB4) {}
};

class ZDOMgmtDirectJoinNotification : public ZDOMgmtNotification
{
public:
    ZDOMgmtDirectJoinNotification() : ZDOMgmtNotification(0xB5) {}
};

class ZDOMgmtPermitJoinNotification : public ZDOMgmtNotification
{
public:
    ZDOMgmtPermitJoinNotification() : ZDOMgmtNotification(0xB6) {}
};


class ZDOStateChangeNotification : public ZDO_MTCmdNotification
{
public:
    ZDOStateChangeNotification() : ZDO_MTCmdNotification(0xC0), state(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(1);

        packet[4] = state;

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        state = packet[4];

        return 1 == length;
    }

    virtual uint8_t GetLength() const override { return 1; }

    uint8_t state;
};


class ZDOEndDeviceAnnceNotification : public ZDO_MTCmdNotification
{
public:
    ZDOEndDeviceAnnceNotification() : ZDO_MTCmdNotification(0xC1), srcAddr(0), dstAddr(0), srcIEEEAddr(0), capabilities(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(13);

        WriteUint16(4, srcAddr, packet);
        WriteUint16(6, dstAddr, packet);
        WriteUint64(8, srcIEEEAddr, packet);
        packet[16] = capabilities;

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        srcAddr = ReadUint16(4, packet);
        dstAddr = ReadUint16(6, packet);
        srcIEEEAddr = ReadUint64(8, packet);
        capabilities = packet[16];

        return 13 == length;
    }

    virtual uint8_t GetLength() const override { return 13; }

    uint16_t srcAddr;
    uint16_t dstAddr;
    uint64_t srcIEEEAddr;
    uint8_t capabilities;
};


class ZDOStatusErrorRspNotification : public ZDOMgmtNotification
{
public:
    ZDOStatusErrorRspNotification() : ZDOMgmtNotification(0xC3) {}
};


// TODO: for 'iluminize' this seem to come instead of 'announce' if the device is already added to the network
// maybe also deal with it in network admin mode
// seems to rejoin back after exclusion, although it wasn't supposed to
// a problem with this is that it doesn't carry the IEEE address and it could actually be for a device that is already paired
// but changed its short address
class ZDOSrcRtgIndNotification : public ZDOMgmtNotification
{
public:
    ZDOSrcRtgIndNotification() : ZDOMgmtNotification(0xC4), dstAddr(0), relayCount(0) {}


    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(3 + relayList.size() * 2);

        WriteUint16(4, dstAddr, packet);
        packet[6] = relayCount;

        unsigned int offset = 7;
        for (unsigned int i = 0; i < relayCount && i < relayList.size(); ++i)
        {
            WriteUint16(offset, relayList[i], packet);
            offset += 2;
        }

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        dstAddr = ReadUint16(4, packet);
        relayCount = packet[6];
        relayList.resize(relayCount);

        unsigned int offset = 7;
        for (unsigned int i = 0; i < relayCount && i < relayList.size(); ++i)
        {
            relayList[i] = ReadUint16(offset, packet);
            offset += 2;
        }

        return 3 + relayList.size() * 2 == length;
    }

    uint16_t dstAddr;
    uint8_t relayCount;
    std::vector<uint16_t> relayList;
};



class ZDOLeaveNotification : public ZDO_MTCmdNotification
{
public:
    ZDOLeaveNotification() : ZDO_MTCmdNotification(0xC9), srcAddr(0), extAddr(0), request(0), remove(0), rejoin(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(13);

        WriteUint16(4, srcAddr, packet);
        WriteUint64(6, extAddr, packet);
        packet[14] = request;
        packet[15] = remove;
        packet[16] = rejoin;

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        srcAddr = ReadUint16(4, packet);
        extAddr = ReadUint64(6, packet);
        request = packet[14];
        remove = packet[15];
        rejoin = packet[16];

        return 13 == length;
    }

    virtual uint8_t GetLength() const override { return 13; }

    uint16_t srcAddr;
    uint64_t extAddr;
    uint8_t request;
    uint8_t remove;
    uint8_t rejoin;
};


// ZDO_TC_DEV_IND or MT_ZDO_TC_DEVICE_IND, TC = trust center

class ZDOTCDeviceNotification : public ZDO_MTCmdNotification
{
public:
    ZDOTCDeviceNotification() : ZDO_MTCmdNotification(0xCA), srcAddr(0), srcIEEEAddr(0), parentAddr(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(12);

        WriteUint16(4, srcAddr, packet);
        WriteUint64(6, srcIEEEAddr, packet);
        WriteUint16(14, parentAddr, packet);

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        srcAddr = ReadUint16(4, packet);
        srcIEEEAddr = ReadUint64(6, packet);
        parentAddr = ReadUint16(14, packet);

        return 12 == length;
    }

    virtual uint8_t GetLength() const override { return 12; }

    uint16_t srcAddr;
    uint64_t srcIEEEAddr;
    uint16_t parentAddr;
};


class ZDOPermitJoinNotification : public ZDO_MTCmdNotification
{
public:
    ZDOPermitJoinNotification() : ZDO_MTCmdNotification(0xCB), duration(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(1);

        packet[4] = duration;

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        duration = packet[4];

        return 1 == length;
    }

    virtual uint8_t GetLength() const override { return 1; }

    uint8_t duration;
};


class ZDOMsgCbIncomingNotification : public ZDO_MTCmdNotification
{
public:
    ZDOMsgCbIncomingNotification() : ZDO_MTCmdNotification(0xFF), SrcAddr(0), WasBroadcast(0), ClusterId(0), SecurityUse(0), SeqNum(0), MacDstAddr(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(GetLength());

        WriteUint16(4, SrcAddr, packet);
        packet[6] = WasBroadcast;
        WriteUint16(7, ClusterId, packet);
        packet[9] = SecurityUse;
        packet[10] = SeqNum;
        WriteUint16(11, MacDstAddr, packet);

        std::copy(Data.begin(), Data.end(), packet.begin() + 13);

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        SrcAddr = ReadUint16(4, packet);
        WasBroadcast = packet[6];
        ClusterId = ReadUint16(7, packet);
        SecurityUse = packet[9];
        SeqNum = packet[10];
        MacDstAddr = ReadUint16(11, packet);

        const int sz = length - 9;
        Data.resize(length - 9);
        std::copy(packet.begin() + 13, packet.begin() + 13 + sz, Data.begin());

        return GetLength() == length;
    }

    virtual uint8_t GetLength() const override { return 9 + Data.size(); }

    uint16_t SrcAddr;
    uint8_t WasBroadcast;
    uint16_t ClusterId;
    uint8_t SecurityUse;
    uint8_t SeqNum;
    uint16_t MacDstAddr;

    std::vector<uint8_t> Data;
};

}

#endif
