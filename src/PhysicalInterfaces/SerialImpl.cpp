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


#include "../GD.h"

#include "ZigbeeIds.h"
#include "Serial.h"
#include "SerialImpl.h"

namespace Zigbee
{

SerialImpl::SerialImpl(Serial<SerialImpl> *container)
    : m_container(container), lastSOFtime(0)
{
}

void SerialImpl::EmptyReadBuffers(int tryCount)
{
    try
    {
        int32_t result = 0;
        char byte = 0;
        int cnt = 0;

        if (m_container->_stopCallbackThread)
            return;

        do
        {
            //Clear buffer, otherwise the address response cannot be sent by the module if the buffer is full.
            result = _serial->readChar(byte, 100000);
            ++cnt;
        }
        while(result == 0 && cnt < tryCount && !m_container->_stopCallbackThread);
    }
    catch(const std::exception& ex)
    {
        m_container->_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}


void SerialImpl::rawSend(const std::vector<uint8_t>& packet)
{
    try
    {
        if(!_serial || !_serial->isOpen())
        {
            m_container->_out.printInfo("Info: Trying to RAW Sending packet " + BaseLib::HelperFunctions::getHexString(packet) + " but serial is not open!");

            return;
        }

        m_container->_out.printInfo("Info: RAW Sending packet " + BaseLib::HelperFunctions::getHexString(packet));

        _serial->writeData(packet);
    }
    catch(const std::exception& ex)
    {
        m_container->_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}


void SerialImpl::listen()
{
    try
    {
        std::vector<uint8_t> data;
        data.reserve(128);
        char byte = 0;
        int32_t result = 0;
        uint32_t packetSize = 0;
        uint8_t crc8 = 0;
        int errorReadCount = 0;

        while(!m_container->_stopCallbackThread)
        {
            try
            {
                if(!IsOpen())
                {
                    if(m_container->_stopCallbackThread)
                    {
                        m_container->_out.printInfo("Listen thread stopped");
                        SetStopped();
                        return;
                    }
                    if(IsStopped())
                        m_container->_out.printWarning("Warning: Connection to device closed. Trying to reconnect...");
                    _serial->closeDevice();

                    std::this_thread::sleep_for(std::chrono::seconds(5));

                    if(!m_container->_stopCallbackThread)
                        m_container->reconnect();
                    else
                    {
                        m_container->_out.printInfo("Listen thread stopped");
                        SetStopped();
                        return;
                    }

                    continue;
                }

                byte = 0;
                // TODO: check both timeouts, find what is proper for zigbee
                result = _serial->readChar(byte, 100000); // 100 ms for timeout

                if(result == -1) // error
                {
                    m_container->_out.printError("Error reading from serial device.");

                    // some workaround I'm trying for some odd read error for a 'stick' somebody else is using
                    // there seem to be some 'usb' ones that when a reset is issued, a read error occurs (none of the setups I tested does this)
                    // this tries to prevent re-initializing the stick in such case

                    if (++errorReadCount > 5)
                    {
                        m_container->_out.printError("Couldn't recover from errors reading from serial device, closing it for reopen...");

                        SetStopped();
                        errorReadCount = 0;
                        packetSize = 0;
                        data.clear();
                    }
                    else
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));

                    continue;
                }
                else if(result == 1) // timeout
                {
                    const int64_t curTime = BaseLib::HelperFunctions::getTime();
                    if (curTime - lastSOFtime < 1500) // TODO: check especially this one, here is as in zwave, but for zigbee it's almost sure it should be different! LE: in ccznp timeouts seem to be 3000, check other sources as well
                        continue;

                    packetSize = 0;

                    if(!data.empty())
                    {
                        m_container->_out.printWarning("Warning: Incomplete packet received: " + BaseLib::HelperFunctions::getHexString(data));
                        data.clear();
                    }

                    continue;
                }

                errorReadCount = 0;

                if(data.empty())
                {
                    if(static_cast<uint8_t>(byte) != SOF)
                    {
                        m_container->_out.printWarning("Warning: Unknown start byte received: " + BaseLib::HelperFunctions::getHexString(byte));

                        data.clear();

                        continue;
                    }
                    // if here, a SOF has been received
                    lastSOFtime = BaseLib::HelperFunctions::getTime();
                }
                data.push_back(byte);

                if(0 == packetSize && 2 == data.size())
                {
                    packetSize = data[1]; // this is only data size
                    // a packet is like this: SOF LEN CMD0 CMD1 DATA CRC
                    packetSize += 5; // 4 for header and 1 for crc
                }

                if(packetSize > 0 && data.size() == packetSize)
                {
                    crc8 = m_container->getCrc8(data);
                    if(crc8 != data.back())
                    {
                        m_container->_out.printError("Error: CRC failed for packet: " + BaseLib::HelperFunctions::getHexString(data));
                        packetSize = 0;
                        data.clear();

                        continue;
                    }

                    m_container->_lastPacketReceived = BaseLib::HelperFunctions::getTime();
                    packetSize = 0;

                    m_container->AddPacketToProcessQueue(data);

                    data.clear();
                }
            }
            catch(const std::exception& ex)
            {
                m_container->_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
            }
        }
    }
    catch(const std::exception& ex)
    {
        m_container->_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    m_container->_out.printInfo("Listen thread stopped");
}

}
