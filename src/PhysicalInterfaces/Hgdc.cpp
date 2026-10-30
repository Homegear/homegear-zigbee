/* Copyright 2013-2019 Homegear GmbH
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

#include "../GD.h"
#include "Hgdc.h"

#include "Serial.h"
#include "ZigbeeIds.h"

namespace Zigbee
{

HgdcImpl::HgdcImpl(Serial<HgdcImpl> *container)
 : m_container(container)
{
}

HgdcImpl::~HgdcImpl()
{
    GD::bl->hgdc->unregisterPacketReceivedEventHandler(_packetReceivedEventHandlerId);
    _packetReceivedEventHandlerId = -1;
}

void HgdcImpl::EmptyReadBuffers(int tryCount)
{
    try
    {
        if (m_container->_stopCallbackThread)
            return;

        // for now, do nothing, I'll see if there is anything better to do here
    }
    catch(const std::exception& ex)
    {
        m_container->_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HgdcImpl::Reset()
{
    try
    {
        GD::bl->hgdc->unregisterPacketReceivedEventHandler(_packetReceivedEventHandlerId);
        _packetReceivedEventHandlerId = GD::bl->hgdc->registerPacketReceivedEventHandler(ZIGBEE_FAMILY_ID, std::function<void(int64_t, const std::string&, const std::vector<uint8_t>&)>(std::bind(&HgdcImpl::processPacket, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)));

        m_container->reconnectNoClose();
    }
    catch(const std::exception& ex)
    {
        m_container->_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}


bool HgdcImpl::Open()
{
    if (_packetReceivedEventHandlerId == -1) Reset();
    if (_packetReceivedEventHandlerId == -1)
    {
        m_container->_out.printError("Error: Could not open device.");
        SetStopped(); //just to be sure
        return false;
    }
    SetStopped(false);

    return true;
}


bool HgdcImpl::IsStopped() const
{
    return m_container->_stopped;
}

void HgdcImpl::SetStopped(bool stop)
{
    m_container->_stopped = stop;
}

void HgdcImpl::Close()
{
    try
    {
        GD::bl->hgdc->unregisterPacketReceivedEventHandler(_packetReceivedEventHandlerId);
        _packetReceivedEventHandlerId = -1;
        SetStopped();
    }
    catch(const std::exception& ex)
    {
        m_container->_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}


void HgdcImpl::rawSend(const std::vector<uint8_t>& packet)
{
    try
    {
        if(!GD::bl->hgdc->sendPacket(m_container->_settings->serialNumber, packet))
        {
            m_container->_out.printError("Error sending packet " + BaseLib::HelperFunctions::getHexString(packet) + ".");
        }
    }
    catch(const std::exception& ex)
    {
        m_container->_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HgdcImpl::processPacket(int64_t familyId, const std::string& serialNumber, const std::vector<uint8_t>& data)
{
    try
    {
        if(serialNumber != m_container->_settings->serialNumber || data.empty()) return;

        const uint8_t byte = data[0];
        if (SOF != byte)
        {
            m_container->_out.printWarning("Warning: Unknown start byte received: 0x" + BaseLib::HelperFunctions::getHexString(byte));

            return;
        }

        if (data.size() < 2)
        {
            m_container->_out.printWarning("Warning: Too small packet received: 0x" + BaseLib::HelperFunctions::getHexString(data));

            return;
        }

        unsigned int packetSize = data[1]; // this is only data size
        // a packet is like this: SOF LEN CMD0 CMD1 DATA CRC
        packetSize += 5; // 4 for header and 1 for crc

        if (packetSize != data.size())
        {
            m_container->_out.printWarning("Warning: Packet with wrong size received: 0x" + BaseLib::HelperFunctions::getHexString(data));

            return;
        }

        uint8_t crc8 = m_container->getCrc8(data);
        if(crc8 != data.back())
        {
            m_container->_out.printError("Error: CRC failed for packet: 0x" + BaseLib::HelperFunctions::getHexString(data));
            return;
        }

        m_container->_lastPacketReceived = BaseLib::HelperFunctions::getTime();
        m_container->AddPacketToProcessQueue(data);
    }
    catch(const std::exception& ex)
    {
        m_container->_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

}
