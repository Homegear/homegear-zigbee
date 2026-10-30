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

#ifndef ZCL_H_
#define ZCL_H_

#include <cstdint>

#include "MT.h"

namespace ZigbeeCommands
{

class ZCLFrame
{
public:
    ZCLFrame() : FrameControl(0x10 /* default response disabled by default*/), ManufacturerCode(0), TransactionSeq(0), cmd(0) {}
    virtual ~ZCLFrame() {}

    virtual std::vector<uint8_t> GetEncoded(unsigned int reserveAdvance = 0) const
    {
        std::vector<uint8_t> packet(GetLength());

        packet[0] = FrameControl;

        int offset = 1;
        if (IsManufacturerSpecific())
        {
            MTCmd::WriteUint16(1, ManufacturerCode, packet);
            offset = 3;
        }

        packet[offset++] = TransactionSeq;
        packet[offset++] = cmd;

        std::copy(payload.begin(), payload.end(), packet.begin() + offset);

        return packet;
    }

    virtual bool Decode(const std::vector<uint8_t>& packet)
    {
        if (packet.size() < 3) return false;
        FrameControl = packet[0];

        int offset = 1;
        if (IsManufacturerSpecific())
        {
            if (packet.size() < 5) return false;
            ManufacturerCode = MTCmd::ReadUint16(1, packet);
            offset = 3;
        }
        else ManufacturerCode = 0;

        TransactionSeq = packet[offset++];
        cmd = packet[offset++];

        payload.resize(packet.size() - GetLengthNoPayload());
        std::copy(packet.begin() + GetLengthNoPayload(), packet.end(), payload.begin());

        return true;
    }

    uint8_t GetLengthNoPayload() const
    {
        uint8_t len = 3;

        if (IsManufacturerSpecific()) len += 2;

        return len;
    }

    virtual uint8_t GetLength() const
    {
        return GetLengthNoPayload() + payload.size();
    }

    uint8_t GetFrameType() const { return FrameControl & 0x3; }

    bool IsGlobal() const { return GetFrameType() == 0; }
    bool IsLocal() const { return GetFrameType() == 1; }

    void SetGlobal(bool global)
    {
        if (global) FrameControl &= 0xFC;
        else FrameControl |= 0x1;
    }

    bool IsManufacturerSpecific() const { return FrameControl & 0x4; }

    void SetManufacturerSpecific(bool specific)
    {
        if (specific) FrameControl |= 0x4;
        else FrameControl &= 0xFB;
    }

    bool DirectionFromServerToClient() const { return FrameControl & 0x8; }

    void SetDirection(bool FromServerToClient)
    {
        if (FromServerToClient) FrameControl |= 0x8;
        else FrameControl &= 0xF7;
    }

    // This field SHALL be set to 1, for all response frames generated as the immediate and direct effect of a previously received frame.
    bool IsDefaultResponseDisabled() const { return FrameControl & 0x10; }

    void SetDefaultResponseDisabled(bool disabled)
    {
        if (disabled) FrameControl |= 0x10;
        else FrameControl &= 0xEF;
    }

    bool ExpectsResponse() const
    {
        // TODO: if it's local it might be some cluster command
        // that sends or not a packet back
        // this should be checked
        // I looked a little and it's not straightforward
        // some commands send no 'response' back, some of them will send a response, some of them only in some circumstances, it's quite difficult to tell
        // TODO: should we ask for 'default response' no matter what for the cluster commands and consider that the response?

        // global attributes: 0xfffd cluster revision (mandatory, ro), 0xfffe - attribute reporting status (optional, ro)

        if (IsLocal()) return false;
        else if (0x00 == cmd || 0x02 == cmd || 0x03 == cmd || 0x06 == cmd || 0x08 == cmd || // Read Attributes, Write Attributes, Write Attributes Undivided, Configure Reporting, Read Reporting Configuration
                0x0c == cmd || 0x0e == cmd || 0x0f == cmd || // Discover Attributes, Read Attributes Structured, Write Attributes Structured
                0x11 == cmd || 0x13 == cmd || 0x15 == cmd)
                    return true;

        return false;
    }

    bool IsDefaultResponseError(uint8_t sentCmd) const
    {
        if (IsLocal()) return false;
        else if (0x0B == cmd && payload.size() >= 2 && sentCmd == payload[0] && payload[1] != 0)
            return true;

        return false;
    }

    bool IsExpectedResponseTo(uint8_t sentCmd) const
    {
        // TODO: deal with local, don't simply return false, see above
        if (IsLocal()) return false;
        else if (((0x0 == sentCmd || 0x0e == sentCmd) && (0x01 == cmd /*|| 0x0A == cmd*/)) || // read attrs or Read Attributes Structured - sometimes it replies with a 'report attributes', though, which I don't think it's ok
            ((0x02 == sentCmd || 0x03 == sentCmd) && 0x04 == cmd) || // write attrs, write attrs undivided
            (0x06 == sentCmd && 0x07 == cmd) || // config reporting
            (0x08 == sentCmd && 0x09 == cmd) || // read reporting config
            (0x0c == sentCmd && 0x0d == cmd) || // Discover Attributes
            (0x0f == sentCmd && 0x10 == cmd) || // Write Attributes Structured
            (0x11 == sentCmd && 0x12 == cmd) || // Discover Commands Received
            (0x13 == sentCmd && 0x14 == cmd) || // Discover Commands Generated
            (0x15 == sentCmd && 0x16 == cmd) // Discover Attributes Extended
            )
            return true;

        // default response, not really 'expected', but can be sent in case of some error
        return IsDefaultResponseError(sentCmd);
    }

    std::string GetDescription() const
    {
        if (IsLocal())
            return std::string("Local command: ") + std::to_string(int(cmd));
        else
        {
            switch(cmd)
            {
            case 0x00:
                return "Global command: read attr";
            case 0x01:
                return "Global command: read attr response";
            case 0x02:
                return "Global command: write attr";
            case 0x03:
                return "Global command: write attr undivided";
            case 0x04:
                return "Global command: write attr response";
            case 0x05:
                return "Global command: write attr no response";
            case 0x06:
                return "Global command: configure reporting";
            case 0x07:
                return "Global command: configure reporting response";
            case 0x08:
                return "Global command: read reporting configuration";
            case 0x09:
                return "Global command: read reporting configuration response";
            case 0x0A:
                return "Global command: report attr";
            case 0x0B:
                return "Global command: default response";
            case 0x0C:
                return "Global command: discover attr";
            case 0x0D:
                return "Global command: discover attr response";
            case 0x0E:
                return "Global command: read attr structured";
            case 0x0F:
                return "Global command: write attr structured";
            case 0x10:
                return "Global command: write attr structured response";
            case 0x11:
                return "Global command: discover commands received";
            case 0x12:
                return "Global command: discover commands received response";
            case 0x13:
                return "Global command: discover commands generated";
            case 0x14:
                return "Global command: discover commands generated response";
            case 0x15:
                return "Global command: discover attr extended";
            case 0x16:
                return "Global command: discover attr extended response";
            }
        }

        return "Unknown";
    }

    uint8_t FrameControl;
    uint16_t ManufacturerCode;
    uint8_t TransactionSeq;
    uint8_t cmd;
    // for cmd, if the command is 'global' (otherwise depends on cluster):
    /*
    0x00 Read Attributes
    0x01 Read Attributes Response
    0x02 Write Attributes
    0x03 Write Attributes Undivided
    0x04 Write Attributes Response
    0x05 Write Attributes No Response
    0x06 Configure Reporting
    0x07 Configure Reporting Response
    0x08 Read Reporting Configuration
    0x09 Read Reporting Configuration Response
    0x0a Report attributes
    0x0b Default Response
    0x0c Discover Attributes
    0x0d Discover Attributes Response
    0x0e Read Attributes Structured
    0x0f Write Attributes Structured
    0x10 Write Attributes Structured response
    0x11 Discover Commands Received
    0x12 Discover Commands Received Response
    0x13 Discover Commands Generated
    0x14 Discover Commands Generated Response
    0x15 Discover Attributes Extended
    0x16 Discover Attributes Extended Response
    */

    std::vector<uint8_t> payload;

    static std::string GetErrorString(uint8_t code);

protected:
    static const std::map<uint8_t, std::string> errorCodesMap;
};

}

#endif

