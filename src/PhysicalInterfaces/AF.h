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

#ifndef AF_H_
#define AF_H_

#include <cstdint>

#include "ZigbeeIds.h"
#include "IZigbeeInterface.h"
#include "MT.h"

// TODO: if data is too big to fit in a packet, it must be 'split', see AF_DATA_REQUEST_EXT, AF_INCOMING_MSG_EXT, AF_DATA_STORE, AF_DATA_RETRIEVE - those should be implemented and handled later, 'as needed'

// TODO: AFDataRequest might get back an AFDataResponse with a failure pointing out that the short address is unknown (probably quite hard to happen, but still it should be expected)
// in such a case, the real short addr should be determined with ZDO_NWK_ADDR_REQ then sending should be retried with the proper address

namespace ZigbeeCommands
{

class AF_MTCmdRequest : public MTCmdRequest
{
public:
    explicit AF_MTCmdRequest(uint8_t cmdid) : MTCmdRequest(cmdid, AF) {}
};

class AF_MTCmdResponse : public MTCmdResponse
{
public:
    explicit AF_MTCmdResponse(uint8_t cmdid) : MTCmdResponse(cmdid, AF) {}
};

class AF_MTCmdStatusResponse : public MTCmdStatusResponse
{
public:
    explicit AF_MTCmdStatusResponse(uint8_t cmdid) : MTCmdStatusResponse(cmdid, AF) {}
};

class AF_MTCmdNotification : public MTCmdNotification
{
public:
    explicit AF_MTCmdNotification(uint8_t cmdid) : MTCmdNotification(cmdid, AF) {}
};


// ***************************************************************************************************************
// AF - application framework - classes

// use it to register an end point for the coordinator!
// this way devices could be bind to the coordinator, to get notifications
// some other way: with ZDO reports or something like that


class AFRegisterRequest : public AF_MTCmdRequest
{
public:
    AFRegisterRequest() : AF_MTCmdRequest(0), EndPoint(1), AppProfId(0x0104), AppDeviceId(0x0050), AppDevVer(1), LatencyReq(0) {}


    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(GetLength());

        res[4] = EndPoint;
        WriteUint16(5, AppProfId, res);
        WriteUint16(7, AppDeviceId, res);
        res[9] = AppDevVer;
        res[10] = LatencyReq;

        res[11] = AppInClusterList.size();

        int offset = 12;
        for (uint16_t cluster : AppInClusterList)
        {
            WriteUint16(offset, cluster, res);
            offset += 2;
        }

        res[offset++] = AppOutClusterList.size();
        for (uint16_t cluster : AppOutClusterList)
        {
            WriteUint16(offset, cluster, res);
            offset += 2;
        }

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        EndPoint = packet[4];
        AppProfId = ReadUint16(5, packet);
        AppDeviceId = ReadUint16(7, packet);
        AppDevVer = packet[9];
        LatencyReq = packet[10];
        int len = packet[11];

        AppOutClusterList.resize(0);
        AppInClusterList.resize(len);
        if (GetLength() > length) return false;

        int offset = 12;
        for (uint16_t& cluster : AppInClusterList)
        {
            cluster = ReadUint16(offset, packet);
            offset += 2;
        }

        len = packet[offset++];
        AppOutClusterList.resize(len);
        if (GetLength() != length) return false;
        for (uint16_t& cluster : AppOutClusterList)
        {
            cluster = ReadUint16(offset, packet);
            offset += 2;
        }

        return true;
    }

    virtual uint8_t GetLength() const override { return 9 + 2 * (AppInClusterList.size() + AppOutClusterList.size()); }

    virtual bool ExpectsResponse() const override { return true; }

    uint8_t EndPoint; // Specifies the endpoint of the device, 0 is for ZDO, don't use it

    // TODO: I'll have to find out more precisely what those mean, to set them correctly

    uint16_t AppProfId; // Specifies the profile Id of the application
    // some values: 0x0101 - Industrial Plant Monitoring, 0x0104 Home Automation (default), 0x0105 Commercial Building Automation, 0x0107 Telecom Applications, 0x0108 Personal Home and Hospital Care, 0x0109 Advanced Metering Initiative, 0x7F02 - gateway

    uint16_t AppDeviceId; // Specifies the device description Id for this endpoint
    // Example of device ids:
    // 0x0008 Range Extender, 0x0009 Mains Power Outlet, 0x0100 On/Off light, 0x0101 - dimmable light, 0x0103 on/off switch, 0x0104 dimmer switch, 0x0106 light sensor, 0x0200 - shade, 0x0201 shade controller, 0x0300 heating/cooling unit
    // 0x0301 thermostat, 0x0302 temperature sensor, 0x0050 Home Gateway (default)
    // profile ids and cluster ids are more important than device ids!!!!! The later are used by commissioning tools (for UI), but the zigbee network does not care much about it.

    // more, here: http://www.rfwireless-world.com/Terminology/Zigbee-Profile-ID-list.html
    // also in zcl_ha.h in ti zstack source

    uint8_t AppDevVer; // Specifies the device version number, 1 by default

    uint8_t LatencyReq; // Specifies latency - 0 no latency (the case for coordinator), 1 - fast beacons, 2 - slow beacons

    // probably for the coordinator this is more important, if we need incoming notifications through binding
    std::vector<uint16_t> AppInClusterList; // Specifies the list of Input Cluster Id’s
    std::vector<uint16_t> AppOutClusterList; // Specifies the list of Output Cluster Id’s - for 'inverse' notifications: from coordinator to device, that is, the coordinator sends the command, the device receives it (and executes something accordingly)
};

class AFRegisterResponse : public AF_MTCmdStatusResponse
{
public:
    AFRegisterResponse() : AF_MTCmdStatusResponse(0) {}
};





// there is an undocumented command (0x4) that seem to be able to delete the endpoint once configured
// I don't think it's needed, but just in case...

class AFUnregisterRequest : public AF_MTCmdRequest
{
public:
    AFUnregisterRequest() : AF_MTCmdRequest(0x4), EndPoint(1) {}


    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(1);

        res[4] = EndPoint;

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        EndPoint = packet[4];

        return 1 == GetLength();
    }

    virtual uint8_t GetLength() const override { return 1; }

    virtual bool ExpectsResponse() const override { return true; }

    uint8_t EndPoint;
};

class AFUnregisterResponse : public AF_MTCmdStatusResponse
{
public:
    AFUnregisterResponse() : AF_MTCmdStatusResponse(0x4) {}
};

class AFDataRequest : public AF_MTCmdRequest
{
public:
    // NOTE: don't set 'discover routing', it will happen automatically if needed, otherwise don't force it (apparently it's 'obsoleted' anyway)! It's done by broadcasting and it's slow!
    AFDataRequest() : AF_MTCmdRequest(0x01), DstAddr(0), DstEndpoint(0), SrcEndpoint(1), ClusterId(0), TransId(0),
    Options(/*0x10*/ /*with ACK*/ 0x0/*no ACK*/), Radius(30)
    // ZigBee 2006 and 2007 have max hops 10, ZigBee Pro has 30
    {
    }

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(GetLength());

        WriteUint16(4, DstAddr, res);
        res[6] = DstEndpoint;
        res[7] = SrcEndpoint;
        WriteUint16(8, ClusterId, res);
        res[10] = TransId;
        res[11] = Options;
        res[12] = Radius;
        res[13] = Data.size();
        std::copy(Data.begin(), Data.end(), res.begin() + 14);

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        DstAddr = ReadUint16(4, packet);
        DstEndpoint = packet[6];
        SrcEndpoint = packet[7];
        ClusterId = ReadUint16(8, packet);
        TransId = packet[10];
        Options = packet[11];
        Radius = packet[12];

        const uint8_t len = packet[13];

        Data.resize(len);
        if (GetLength() != length) return false;
        std::copy(packet.begin() + 14, packet.begin() + 14 + len, Data.begin());

        return true;
    }

    virtual uint8_t GetLength() const override { return 10 + Data.size(); }

    virtual bool ExpectsResponse() const override { return true; }

    // TODO: find out what it does - if set, commands do not work
    bool IsWildcardProfile() const { return Options & 0x2; }
    void SetWidcardProfile(bool wildcard)
    {
        if (wildcard) Options |= 0x2;
        else Options &= 0xFD;
    }

    //this can be useful, works only for singlecasts
    bool IsApsACK() const { return Options & 0x10; }

    void SetApsACK(bool reqACK)
    {
        if (reqACK) Options |= 0x10;
        else Options &= 0xEF;
    }

    // NOTE: Discover routing is obsoleted, ignore it
    bool IsDiscoverRoute() const { return Options & 0x20; }

    void SetDiscoverRoute(bool discover)
    {
        if (discover) Options |= 0x20;
        else Options &= 0xDF;
    }

    // NOTE: no need to set security, it's actually automatic
    bool IsApsSecurity() const { return Options & 0x40; }

    void SetApsSecurity(bool security)
    {
        if (security) Options |= 0x40;
        else Options &= 0xBF;
    }

    // Good for only direct (unicast) and broadcast messages.
    // will try to send directly instead of using the parent
    bool IsSkipRouting() const { return Options & 0x80; }

    void SetSkipRouting(bool skip)
    {
        if (skip) Options |= 0x80;
        else Options &= 0x7F;
    }

    uint16_t DstAddr; //Short address of the destination device
    uint8_t DstEndpoint; //Endpoint of the destination device
    uint8_t SrcEndpoint; //Endpoint of the source device

    uint16_t ClusterId; //Specifies the cluster ID

    uint8_t TransId; //Specifies the transaction sequence number of the message.
    uint8_t Options; //Transmit options bit mask according to the following defines from AF.h: bit 1: sets ‘Wildcard Profile ID’; bit 4: turns on/off ‘APS ACK’; bit 5 sets ‘discover route’; bit 6 sets ‘APS security’; bit 7 sets ‘skip routing’.
    uint8_t Radius; //Specifies the number of hops allowed delivering the message (see AF_DEFAULT_RADIUS.)

    std::vector<uint8_t> Data; //0-128 bytes data
};

class AFDataResponse : public AF_MTCmdStatusResponse
{
public:
    AFDataResponse() : AF_MTCmdStatusResponse(0x01) {}
};


// AF_DATA_CONFIRM
class AFDataNotification : public AF_MTCmdNotification
{
public:
    AFDataNotification() : AF_MTCmdNotification(0x80), status(0), EndPoint(0), TransId(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(3);

        packet[4] = status;
        packet[5] = EndPoint;
        packet[6] = TransId;

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        status = packet[4];
        EndPoint = packet[5];
        TransId = packet[6];

        return 3 == length;
    }

    virtual uint8_t GetLength() const override { return 3; }

    uint8_t status; // Status is either Success (0) or Failure (1)
    uint8_t EndPoint;
    uint8_t TransId; // Specified the transaction sequence number of the message
};

// AF_INCOMING_MSG
class AFIncomingMsgNotification : public AF_MTCmdNotification
{
public:
    AFIncomingMsgNotification() : AF_MTCmdNotification(0x81), GroupId(0), ClusterId(0), SrcAddr(0), SrcEndpoint(0), DstEndpoint(0), WasBroadcast(0), LinkQuality(0), SecurityUse(0), TimeStamp(0), TransSeqNumber(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(GetLength());

        WriteUint16(4, GroupId, packet);
        WriteUint16(6, ClusterId, packet);
        WriteUint16(8, SrcAddr, packet);
        packet[10] = SrcEndpoint;
        packet[11] = DstEndpoint;
        packet[12] = WasBroadcast;
        packet[13] = LinkQuality;
        packet[14] = SecurityUse;
        WriteUint32(15, TimeStamp, packet);
        packet[19] = TransSeqNumber;
        packet[20] = Data.size();
        std::copy(Data.begin(), Data.end(), packet.begin() + 21);

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        GroupId = ReadUint16(4, packet);
        ClusterId = ReadUint16(6, packet);
        SrcAddr = ReadUint16(8, packet);
        SrcEndpoint = packet[10];
        DstEndpoint = packet[11];
        WasBroadcast = packet[12];
        LinkQuality = packet[13];
        SecurityUse = packet[14];
        TimeStamp = ReadUint32(15, packet);
        TransSeqNumber = packet[19];

        const uint8_t len = packet[20];

        Data.resize(len);
        // instead of checking for difference, check only if it's bigger
        // apparently GetLength can return something smaller, because there is some data after 'Data' (see the example below)
        if (GetLength() > length) return false;
        std::copy(packet.begin() + 21, packet.begin() + 21 + len, Data.begin());

        return true;
    }

    virtual uint8_t GetLength() const override { return 17 + Data.size(); }

    uint16_t GroupId; //Specifies the group ID of the device
    uint16_t ClusterId; //Specifies the cluster Id (only the LSB is used in V1.0 networks.)
    uint16_t SrcAddr; //Specifies the ZigBee network address of the source device sending the message
    uint8_t SrcEndpoint; //Specifies the source endpoint of the message
    uint8_t DstEndpoint; //Specifies the destination endpoint of the message
    uint8_t WasBroadcast; //Specifies if the message was a broadcast or not
    uint8_t LinkQuality;  //Indicates the link quality measured during reception
    uint8_t SecurityUse; //Specifies if the security is used or not
    uint32_t TimeStamp; //Specifies the timestamp of the message
    uint8_t TransSeqNumber; //Specifies transaction sequence number of the message
    std::vector<uint8_t>Data;  //Contains 0 to 128 bytes of data.
};

}

#endif
