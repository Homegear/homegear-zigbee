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

#ifndef UTIL_H_
#define UTIL_H_

#include <cstdint>

#include "ZigbeeIds.h"
#include "IZigbeeInterface.h"
#include "MT.h"

namespace ZigbeeCommands
{


class UTIL_MTCmdRequest : public MTCmdRequest
{
public:
    explicit UTIL_MTCmdRequest(uint8_t cmdid) : MTCmdRequest(cmdid, UTIL) {}
};

class UTIL_MTCmdResponse : public MTCmdResponse
{
public:
    explicit UTIL_MTCmdResponse(uint8_t cmdid) : MTCmdResponse(cmdid, UTIL) {}
};

class UTIL_MTCmdStatusResponse : public MTCmdStatusResponse
{
public:
    explicit UTIL_MTCmdStatusResponse(uint8_t cmdid) : MTCmdStatusResponse(cmdid, UTIL) {}
};

class UTIL_MTCmdNotification : public MTCmdNotification
{
public:
    explicit UTIL_MTCmdNotification(uint8_t cmdid) : MTCmdNotification(cmdid, UTIL) {}
};

// **************************************************************************************************************
// UTIL classes


class UtilGetDeviceInfoRequest : public UTIL_MTCmdRequest
{
public:
    UtilGetDeviceInfoRequest() : UTIL_MTCmdRequest(0) {}

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

class UtilGetDeviceInfoResponse : public UTIL_MTCmdResponse
{
public:
    UtilGetDeviceInfoResponse() : UTIL_MTCmdResponse(0), status(Zigbee::ZigbeeReturnIds::ZSuccess), IEEEaddress(0), shortAddress(0), deviceType(0), deviceState(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(GetLength());

        packet[4] = static_cast<uint8_t>(status);
        WriteUint64(5, IEEEaddress, packet);
        WriteUint16(13, shortAddress, packet);

        packet[15] = deviceType;
        packet[16] = deviceState;

        packet[17] = assocDevices.size();

        int off = 18;
        for (uint16_t val : assocDevices)
        {
            WriteUint16(off, val, packet);
            off += 2;
        }

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        status = static_cast<Zigbee::ZigbeeReturnIds>(packet[4]);
        IEEEaddress = ReadUint64(5, packet);
        shortAddress = ReadUint16(13, packet);

        deviceType = packet[15];
        deviceState = packet[16];

        const int len = packet[17];
        assocDevices.resize(len);

        int off = 18;
        for (uint16_t& val : assocDevices)
        {
            val = ReadUint16(off, packet);
            off += 2;
        }

        return GetLength() == length;
    }

    virtual uint8_t GetLength() const override { return 14 + assocDevices.size() * 2; }

    Zigbee::ZigbeeReturnIds status; // 0 - success, 1 - fail (if the addr was not in valid range)
    uint64_t IEEEaddress;
    uint16_t shortAddress;
    uint8_t deviceType; // Indicates device type, where bits 0 to 2 indicate the capability for the device to operate as a coordinator, router, or end device, respectively.
    uint8_t deviceState; // See below:
    /*
    0x00: Initialized - not started automatically - DEV_HOLD state
    0x01: Initialized - not connected to anything

    0x02: Discovering PAN's to join
    0x03: Joining a PAN
    0x04: Rejoining a PAN, only for end devices
    0x05: Joined but not yet authenticated by trust center
    0x06: Started as device after authentication
    0x07: Device joined, authenticated and is a router

    0x08: Starting as ZigBee Coordinator
    0x09: Started as ZigBee Coordinator

    0x0A: Device has lost information about its parent
    */
    std::vector<uint16_t> assocDevices; // short addresses
};


class UtilSetPanIDRequest : public UTIL_MTCmdRequest
{
public:
    UtilSetPanIDRequest() : UTIL_MTCmdRequest(0x02), panID(0xFFFF) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(2);

        WriteUint16(4, panID, res);

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        panID = ReadUint16(4, packet);

        return 2 == length;
    }

    virtual uint8_t GetLength() const override { return 2; }

    virtual bool ExpectsResponse() const override { return true; }

    uint16_t panID;
};

class UtilSetPanIDResponse : public UTIL_MTCmdStatusResponse
{
public:
    UtilSetPanIDResponse() : UTIL_MTCmdStatusResponse(0x02) {}
};


class UtilSetChannelsRequest : public UTIL_MTCmdRequest
{
public:
    UtilSetChannelsRequest() : UTIL_MTCmdRequest(0x03), channels(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(4);

        WriteUint32(4, channels, res);

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        channels = ReadUint32(4, packet);

        return 4 == length;
    }

    virtual uint8_t GetLength() const override { return 4; }

    virtual bool ExpectsResponse() const override { return true; }

    uint32_t channels;
};

class UtilSetChannelsResponse : public UTIL_MTCmdStatusResponse
{
public:
    UtilSetChannelsResponse() : UTIL_MTCmdStatusResponse(0x03) {}
};

class UtilSetSecLevelRequest : public UTIL_MTCmdRequest
{
public:
    UtilSetSecLevelRequest() : UTIL_MTCmdRequest(0x04), SecLevel(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(1);

        res[4] = SecLevel;

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        SecLevel = packet[4];

        return 1 == length;
    }

    virtual uint8_t GetLength() const override { return 1; }

    virtual bool ExpectsResponse() const override { return true; }

    uint8_t SecLevel;
};

class UtilSetSecLevelResponse : public UTIL_MTCmdStatusResponse
{
public:
    UtilSetSecLevelResponse() : UTIL_MTCmdStatusResponse(0x04) {}
};

class UtilCallbackSubCmdRequest : public UTIL_MTCmdRequest
{
public:
    UtilCallbackSubCmdRequest() : UTIL_MTCmdRequest(0x06), SubsystemId(0xFFFF), Action(1) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> res = MTCmd::GetEncoded(3);

        WriteUint16(4, SubsystemId, res);
        res[6] = Action;

        Zigbee::IZigbeeInterface::addCrc8(res);

        return res;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet)) return false;

        SubsystemId = ReadUint16(4, packet);
        Action = packet[6];

        return 3 == length;
    }

    virtual uint8_t GetLength() const override { return 3; }

    virtual bool ExpectsResponse() const override { return true; }

    /*
    MT_SYS 0x0100
    MT_MAC 0x0200
    MT_NWK 0x0300
    MT_AF 0x0400
    MT_ZDO 0x0500
    MT_SAPI 0x0600
    MT_UTIL 0x0700
    MT_DEBUG 0x0800
    MT_APP 0x0900
    ALL SUBSYSTEM 0xFFFF
    */
    uint16_t SubsystemId;
    uint8_t Action; // 1 enable, 0 disable
};

class UtilCallbackSubCmdResponse : public UTIL_MTCmdStatusResponse
{
public:
    UtilCallbackSubCmdResponse() : UTIL_MTCmdStatusResponse(0x06) {}
};

}

#endif
