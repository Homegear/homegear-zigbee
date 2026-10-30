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

#ifndef SAPI_H_
#define SAPI_H_

#include <cstdint>

#include "ZigbeeIds.h"
#include "IZigbeeInterface.h"
#include "MT.h"

namespace ZigbeeCommands
{


class SAPI_MTCmdRequest : public MTCmdRequest
{
public:
    explicit SAPI_MTCmdRequest(uint8_t cmdid) : MTCmdRequest(cmdid, SAPI) {}
};

class SAPI_MTCmdResponse : public MTCmdResponse
{
public:
    explicit SAPI_MTCmdResponse(uint8_t cmdid) : MTCmdResponse(cmdid, SAPI) {}
};

class SAPI_MTCmdStatusResponse : public MTCmdStatusResponse
{
public:
    explicit SAPI_MTCmdStatusResponse(uint8_t cmdid) : MTCmdStatusResponse(cmdid, SAPI) {}
};

class SAPI_MTCmdNotification : public MTCmdNotification
{
public:
    explicit SAPI_MTCmdNotification(uint8_t cmdid) : MTCmdNotification(cmdid, SAPI) {}
};

class SAPI_MTCmdStatusNotification : public MTCmdStatusNotification
{
public:
    explicit SAPI_MTCmdStatusNotification(uint8_t cmdid) : MTCmdStatusNotification(cmdid, SAPI) {}
};

// ***************************************************************************************************************
// SAPI classes - 'simple API'

class SAPIResetRequest : public SAPI_MTCmdRequest
{
public:
    SAPIResetRequest() : SAPI_MTCmdRequest(0x09) {}

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

    virtual bool ExpectsResponse() const override { return true; } // I'm not really sure about this, the documentation has no response, but there is a 'response' nevertheless
};


class SAPIStartRequest : public SAPI_MTCmdRequest
{
public:
    SAPIStartRequest() : SAPI_MTCmdRequest(0x0) {}

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

    virtual bool ExpectsResponse() const override { return true; } // I'm not really sure about this, the documentation has no response, but there is a 'response' nevertheless
};

class SAPIStartResponse : public SAPI_MTCmdStatusResponse
{
public:
    SAPIStartResponse() : SAPI_MTCmdStatusResponse(0x0) {}
};

class SAPIStartNotification : public SAPI_MTCmdStatusNotification
{
public:
    SAPIStartNotification() : SAPI_MTCmdStatusNotification(0x80) {}
};

}

#endif
