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

#ifndef MT_H_
#define MT_H_

#include <cstdint>

#include "ZigbeeIds.h"
#include "IZigbeeInterface.h"

namespace ZigbeeCommands
{

class MTCmd
{
public:
    // default: COMMAND_CLASS_BASIC : BASIC_GET
    MTCmd(uint8_t cmdid, uint8_t subsys = SYS, uint8_t type = SREQ);
    virtual ~MTCmd();
    MTCmd(const MTCmd& other);
    MTCmd& operator=(const MTCmd& other);

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const;
    virtual bool Decode(const std::vector<uint8_t>& packet);
    virtual bool ForceDecode(const std::vector<uint8_t>& packet);

    bool DecodeHeader(const std::vector<uint8_t>& packet); // for a 'generic' packet, to find out what 'kind' it is, could be used for example for a 'factory method' to identify the packets and create the proper objects to parse them

    bool IsSynchronousRequest() const { return SREQ == (m_cmd1 & MT_TYPE_MASK); }
    bool IsSynchronousResponse() const { return SRSP == (m_cmd1 & MT_TYPE_MASK); }

    bool IsSynchronous() const { return IsSynchronousRequest() || IsSynchronousResponse(); }
    bool IsPoll() const { return POLL == (m_cmd1 & MT_TYPE_MASK); }
    bool IsAsynchronousRequest() const { return AREQ == (m_cmd1 & MT_TYPE_MASK); }

    uint8_t GetType() const { return m_cmd1 & MT_TYPE_MASK; }
    uint8_t GetSubsystem() const { return m_cmd1 & SUBSYS_MASK; }

    uint8_t GetCmdTypeAndSubsys() const { return m_cmd1; }

    uint8_t GetCommandId() const { return m_cmd2; }

    virtual uint8_t GetLength() const { return length; } // this has to be implemented in each derived class (if length is not zero)

    virtual bool ExpectsResponse() const { return false; }


    static std::string getHexCode(uint8_t val);
    std::string getDescription() const;

    // bytes are transmitted in LSB manner

// some util functions
    static uint16_t ReadUint16(int offset, const std::vector<uint8_t>& packet)
    {
        uint16_t value;

        value = packet[offset++];
        value |= static_cast<uint16_t>(packet[offset]) << 8;

        return value;
    }

    static void WriteUint16(int offset, uint16_t value, std::vector<uint8_t>& packet)
    {
        packet[offset++] = (value & 0xFF);
        packet[offset] = (value & 0xFF00) >> 8;
    }

    static uint32_t ReadUint24(int offset, const std::vector<uint8_t>& packet)
    {
        uint32_t value;

        value = packet[offset++];
        value |= static_cast<uint32_t>(packet[offset++]) << 8;
        value |= static_cast<uint32_t>(packet[offset]) << 16;

        return value;
    }

    static void WriteUint24(int offset, uint32_t value, std::vector<uint8_t>& packet)
    {
        packet[offset++] = (value & 0xFF);
        packet[offset++] = (value & 0xFF00) >> 8;
        packet[offset] = (value & 0xFF0000) >> 16;
    }


    static int32_t ReadInt24(int offset, const std::vector<uint8_t>& packet)
    {
        uint32_t value;

        value = packet[offset++];
        value |= static_cast<uint32_t>(packet[offset++]) << 8;
        value |= static_cast<uint32_t>(packet[offset]) << 16;
        if (packet[offset] & 0x80) // negative
            value |= 0xFF000000;

        return static_cast<int32_t>(value);
    }

    static uint32_t ReadUint32(int offset, const std::vector<uint8_t>& packet)
    {
        uint32_t value;

        value = packet[offset++];
        value |= static_cast<uint32_t>(packet[offset++]) << 8;
        value |= static_cast<uint32_t>(packet[offset++]) << 16;
        value |= static_cast<uint32_t>(packet[offset]) << 24;

        return value;
    }

    static void WriteUint32(int offset, uint32_t value, std::vector<uint8_t>& packet)
    {
        packet[offset++] = (value & 0xFF);
        packet[offset++] = (value & 0xFF00) >> 8;
        packet[offset++] = (value & 0xFF0000) >> 16;
        packet[offset] = (value & 0xFF000000) >> 24;
    }

    static uint64_t ReadUint40(int offset, const std::vector<uint8_t>& packet)
    {
        uint64_t value;

        value = packet[offset++];
        value |= static_cast<uint64_t>(packet[offset++]) << 8;
        value |= static_cast<uint64_t>(packet[offset++]) << 16;
        value |= static_cast<uint64_t>(packet[offset++]) << 24;
        value |= static_cast<uint64_t>(packet[offset]) << 32;

        return value;
    }

    static int64_t ReadInt40(int offset, const std::vector<uint8_t>& packet)
    {
        uint64_t value;

        value = packet[offset++];
        value |= static_cast<uint64_t>(packet[offset++]) << 8;
        value |= static_cast<uint64_t>(packet[offset++]) << 16;
        value |= static_cast<uint64_t>(packet[offset++]) << 24;
        value |= static_cast<uint64_t>(packet[offset]) << 32;

        if (packet[offset] & 0x80) // negative
            value |= 0xFFFFFF0000000000ul;

        return static_cast<int64_t>(value);
    }

    static uint64_t ReadUint48(int offset, const std::vector<uint8_t>& packet)
    {
        uint64_t value;

        value = packet[offset++];
        value |= static_cast<uint64_t>(packet[offset++]) << 8;
        value |= static_cast<uint64_t>(packet[offset++]) << 16;
        value |= static_cast<uint64_t>(packet[offset++]) << 24;
        value |= static_cast<uint64_t>(packet[offset++]) << 32;
        value |= static_cast<uint64_t>(packet[offset]) << 40;

        return value;
    }

    static int64_t ReadInt48(int offset, const std::vector<uint8_t>& packet)
    {
        uint64_t value;

        value = packet[offset++];
        value |= static_cast<uint64_t>(packet[offset++]) << 8;
        value |= static_cast<uint64_t>(packet[offset++]) << 16;
        value |= static_cast<uint64_t>(packet[offset++]) << 24;
        value |= static_cast<uint64_t>(packet[offset++]) << 32;
        value |= static_cast<uint64_t>(packet[offset]) << 40;

        if (packet[offset] & 0x80) // negative
            value |= 0xFFFF000000000000ul;

        return static_cast<int64_t>(value);
    }

    static uint64_t ReadUint56(int offset, const std::vector<uint8_t>& packet)
    {
        uint64_t value;

        value = packet[offset++];
        value |= static_cast<uint64_t>(packet[offset++]) << 8;
        value |= static_cast<uint64_t>(packet[offset++]) << 16;
        value |= static_cast<uint64_t>(packet[offset++]) << 24;
        value |= static_cast<uint64_t>(packet[offset++]) << 32;
        value |= static_cast<uint64_t>(packet[offset++]) << 40;
        value |= static_cast<uint64_t>(packet[offset]) << 48;

        return value;
    }

    static int64_t ReadInt56(int offset, const std::vector<uint8_t>& packet)
    {
        uint64_t value;

        value = packet[offset++];
        value |= static_cast<uint64_t>(packet[offset++]) << 8;
        value |= static_cast<uint64_t>(packet[offset++]) << 16;
        value |= static_cast<uint64_t>(packet[offset++]) << 24;
        value |= static_cast<uint64_t>(packet[offset++]) << 32;
        value |= static_cast<uint64_t>(packet[offset++]) << 40;
        value |= static_cast<uint64_t>(packet[offset]) << 48;

        if (packet[offset] & 0x80) // negative
            value |= 0xFF00000000000000ul;

        return static_cast<int64_t>(value);
    }


    static uint64_t ReadUint64(int offset, const std::vector<uint8_t>& packet)
    {
        uint64_t value;

        value = packet[offset++];
        value |= static_cast<uint64_t>(packet[offset++]) << 8;
        value |= static_cast<uint64_t>(packet[offset++]) << 16;
        value |= static_cast<uint64_t>(packet[offset++]) << 24;

        value |= static_cast<uint64_t>(packet[offset++]) << 32;
        value |= static_cast<uint64_t>(packet[offset++]) << 40;
        value |= static_cast<uint64_t>(packet[offset++]) << 48;
        value |= static_cast<uint64_t>(packet[offset]) << 56;

        return value;
    }

    static void WriteUint64(int offset, uint64_t value, std::vector<uint8_t>& packet)
    {
        packet[offset++] = (value & 0xFF);
        packet[offset++] = (value & 0xFF00) >> 8;
        packet[offset++] = (value & 0xFF0000) >> 16;
        packet[offset++] = (value & 0xFF000000) >> 24;
        packet[offset++] = (value & 0xFF00000000ul) >> 32;
        packet[offset++] = (value & 0xFF0000000000ul) >> 40;
        packet[offset++] = (value & 0xFF000000000000ul) >> 48;
        packet[offset] = (value & 0xFF00000000000000ul) >> 56;
    }

protected:
    uint8_t m_cmd1;
    uint8_t m_cmd2;
    uint8_t length;
};

// TODO: a request that has an async response (or several) typically has only a synchronous 'status response' (or no sync response at all)
// see if this gives some advantage on waiting for the response
// also the async response seems to have cmd2 = 0x80 + cm2_of_sync_request/response!!!! (check this!)

// synchronous request
class MTCmdRequest : public MTCmd
{
public:
    MTCmdRequest(uint8_t cmdid, uint8_t subsys = SYS) : MTCmd(cmdid, subsys, SREQ) {}
};

// a synchronous response to a request like above
class MTCmdResponse : public MTCmd
{
public:
    MTCmdResponse(uint8_t cmdid, uint8_t subsys = SYS) : MTCmd(cmdid, subsys, SRSP) {}
};


class MTCmdStatusResponse : public MTCmdResponse
{
public:
    MTCmdStatusResponse(uint8_t cmdid, uint8_t subsys = SYS) : MTCmdResponse(cmdid, subsys), status(Zigbee::ZigbeeReturnIds::ZSuccess) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(1);

        packet[4] = static_cast<uint8_t>(status);

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        status = static_cast<Zigbee::ZigbeeReturnIds>(packet[4]);

        return 1 == length;
    }

    virtual uint8_t GetLength() const override { return 1; }

    Zigbee::ZigbeeReturnIds status; // 0 - success, 1 - failure
};


class MTCmdStatusUint8Response : public MTCmdResponse
{
public:
    MTCmdStatusUint8Response(uint8_t cmdid, uint8_t subsys = SYS) : MTCmdResponse(cmdid, subsys), status(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(1);

        packet[4] = status;

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        status = packet[4];

        return 1 == length;
    }

    virtual uint8_t GetLength() const override { return 1; }

    uint8_t status; // 0 - some other encoding status than above
};


// an asynchronous request/notification
class MTCmdNotification : public MTCmd
{
public:
    MTCmdNotification(uint8_t cmdid, uint8_t subsys = SYS) : MTCmd(cmdid, subsys, AREQ) {}
};

class MTCmdStatusNotification : public MTCmdNotification
{
public:
    MTCmdStatusNotification(uint8_t cmdid, uint8_t subsys = SYS) : MTCmdNotification(cmdid, subsys), status(Zigbee::ZigbeeReturnIds::ZSuccess) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(1);

        packet[4] = static_cast<uint8_t>(status);

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        status = static_cast<Zigbee::ZigbeeReturnIds>(packet[4]);

        return 1 == length;
    }

    virtual uint8_t GetLength() const override { return 1; }

    Zigbee::ZigbeeReturnIds status; // 0 - success, 1 - failure
};


// an error response:

class ERROR_MTCmdResponse : public MTCmdResponse
{
public:
    ERROR_MTCmdResponse() : MTCmdResponse(0, RPC_ERR), errorCode(0), cmd1(0), cmd2(0) {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const override
    {
        std::vector<uint8_t> packet = MTCmd::GetEncoded(3);

        packet[4] = errorCode;
        packet[5] = cmd1;
        packet[6] = cmd2;

        Zigbee::IZigbeeInterface::addCrc8(packet);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet) override
    {
        if (!MTCmd::Decode(packet))
            return false;

        errorCode = packet[4];
        cmd1 = packet[5];
        cmd2 = packet[6];

        return 3 == length;
    }

    virtual uint8_t GetLength() const override { return 3; }

    uint8_t GetTypeForError() const { return cmd1 & MT_TYPE_MASK; }
    uint8_t GetSubsystemForError() const { return cmd1 & SUBSYS_MASK; }
    uint8_t GetCmdTypeAndSubsysForError() const { return cmd1; }

    uint8_t GetCommandIdForError() const { return cmd2; }

    /*
    0x01 - invalid subsystem
    0x02 - invalid command id
    0x03 - invalid parameter
    0x04 - invalid length
    */

    uint8_t errorCode;
    uint8_t cmd1;
    uint8_t cmd2;
};


}

#endif
