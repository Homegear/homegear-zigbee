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

#include "MT.h"

#include <string>
#include <sstream>
#include <iomanip>

namespace ZigbeeCommands
{

MTCmd::MTCmd(uint8_t cmdid, uint8_t subsys, uint8_t type)
    : m_cmd1((type & MT_TYPE_MASK) | (subsys & SUBSYS_MASK)), m_cmd2(cmdid), length(0)
{
    //ctor
}

MTCmd::~MTCmd()
{
    //dtor
}

MTCmd::MTCmd(const MTCmd& other)
: m_cmd1(other.m_cmd1), m_cmd2(other.m_cmd2), length(other.length)
{
    //copy ctor
}

MTCmd& MTCmd::operator=(const MTCmd& rhs)
{
    if (this == &rhs) return *this; // handle self assignment
    //assignment operator
    m_cmd1 = rhs.m_cmd1;
    m_cmd2 = rhs.m_cmd2;
    length = rhs.length;

    return *this;
}

std::vector<uint8_t> MTCmd::GetEncoded(unsigned int reserveAdvance) const
{
    std::vector<uint8_t> res;
    res.resize(5 + reserveAdvance); // 1 for SOF, 1 for length, 2 for command, 1 for crc

    res[0] = SOF;
    res[1] = GetLength();
    res[2] = m_cmd1;
    res[3] = m_cmd2;

    // will add crc at the end

    return res;
}

bool MTCmd::Decode(const std::vector<uint8_t>& packet)
{
    if (packet.size() < 5) return false;
    else if (SOF != packet[0] || m_cmd1 != packet[2] || m_cmd2 != packet[3]) return false;

    length = packet[1];

    if (packet.back() != Zigbee::IZigbeeInterface::getCrc8(packet)) return false;

    const size_t totalLen = 5 + length;
    if (totalLen != packet.size()) return false;

    return true;
}


bool MTCmd::ForceDecode(const std::vector<uint8_t>& packet)
{
    if (packet.size() < 5) return false;
    else if (SOF != packet[0]) return false;

    m_cmd1 = packet[2];
    m_cmd2 = packet[3];

    length = packet[1];

    if (packet.back() != Zigbee::IZigbeeInterface::getCrc8(packet)) return false;

    const size_t totalLen = 5 + length;
    if (totalLen != packet.size()) return false;

    return true;
}


bool MTCmd::DecodeHeader(const std::vector<uint8_t>& packet)
{
    if (packet.size() < 5) return false;
    else if (SOF != packet[0]) return false;

    length = packet[1];
    m_cmd1 = packet[2];
    m_cmd2 = packet[3];

    if (packet.back() != Zigbee::IZigbeeInterface::getCrc8(packet)) return false;

    const size_t totalLen = 5 + length;
    if (totalLen != packet.size()) return false;

    return true;
}

std::string MTCmd::getHexCode(uint8_t val)
{
    std::stringstream ss;
    ss << std::hex;
    ss << std::uppercase << std::setw(2) << std::setfill('0') << int(val);

    return "0x" + ss.str();
}


std::string MTCmd::getDescription() const
{
    std::string type;

    switch(m_cmd1 & MT_TYPE_MASK)
    {
    case SREQ:
        type="SREQ";
        break;
    case AREQ:
        type="AREQ";
        break;
    case SRSP:
        type="SRSP";
        break;
    case POLL:
        type="POLL";
        break;
    default:
        type="UNK " + getHexCode(m_cmd1 & MT_TYPE_MASK);
        break;
    }

    std::string subsys;
    switch(m_cmd1 & SUBSYS_MASK)
    {
    case RPC_ERR:
        subsys="RPC_ERR";
        break;
    case SYS:
        subsys="SYS";
        break;
    case MAC:
        subsys="MAC";
        break;
    case NWK:
        subsys="NWK";
        break;
    case AF:
        subsys="AF";
        break;
    case ZDO:
        subsys="ZDO";
        break;
    case SAPI:
        subsys="SAPI";
        break;
    case UTIL:
        subsys="UTIL";
        break;
    case ZB_DEBUG:
        subsys="DEBUG";
        break;
    case APP_INT:
        subsys="APP_INT";
        break;
    case APP_CFG:
        subsys="APP_CFG";
        break;
    case RPC_SYS_RCAF:
        subsys="RPC_SYS_RCAF";
        break;
    case RPC_SYS_RCN:
        subsys="RPC_SYS_RCN";
        break;
    case RPC_SYS_RCN_CLIENT:
        subsys="RPC_SYS_RCN_CLIENT";
        break;
    case RPC_SYS_BOOT:
        subsys="RPC_SYS_BOOT";
        break;
    case RPC_SYS_ZIPTEST:
        subsys="RPC_SYS_ZIPTEST";
        break;
    case RPC_SYS_PERIPHERALS:
        subsys="RPC_SYS_PERIPHERALS";
        break;
    case RPC_SYS_NFC:
        subsys="RPC_SYS_NFC";
        break;
    case RPC_SYS_PB_NWK_MGR:
        subsys="RPC_SYS_PB_NWK_MGR";
        break;
    case RPC_SYS_PB_GW:
        subsys="RPC_SYS_PB_GW";
        break;
    case RPC_SYS_PB_OTA_MGR:
        subsys="RPC_SYS_PB_OTA_MGR";
        break;
    case RPC_SYS_BLE_HCI:
        subsys="RPC_SYS_BLE_HCI";
        break;
    case RPC_SYS_SRV_CTR:
        subsys="RPC_SYS_SRV_CTR";
        break;
    default:
        subsys = "UNK";
        break;
    }

    return "MT frame, type: " + type + " subsys: " + subsys + " cmd: " + getHexCode(m_cmd2) + " len: " + std::to_string(length);
}

}


