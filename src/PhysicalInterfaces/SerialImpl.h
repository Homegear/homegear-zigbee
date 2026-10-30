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

#ifndef SERIAL_IMPL_H
#define SERIAL_IMPL_H

#include <cstdint>


namespace Zigbee
{

template<class Impl> class Serial;

class SerialImpl {
public:
    explicit SerialImpl(Serial<SerialImpl> *container);
    ~SerialImpl()
    {
        if (_serial) _serial->closeDevice();
    }

    static bool IsLocal() { return true; }
    static bool IsGateway() { return false; }

    bool IsOpen() const
    {
        return !IsStopped() && _serial && _serial->isOpen();
    }

    bool Open()
    {
        if (!_serial) Reset();
        _serial->openDevice(false, false, false);
        if(!_serial->isOpen())
        {
            m_container->_out.printError("Error: Could not open device.");
            SetStopped(); //just to be sure
            return false;
        }
        SetStopped(false);

        return true;
    }

    void Reset()
    {
        _serial.reset(new BaseLib::SerialReaderWriter(m_container->_bl, m_container->_settings->device, 115200, 0, true, -1));
    }

    void Close()
    {
        if (_serial) _serial->closeDevice();
        SetStopped();
    }

    bool IsStopped() const
    {
        return m_container->_stopped;
    }

    void SetStopped(bool stop = true)
    {
        m_container->_stopped = stop;
    }

    void EmptyReadBuffers(int tryCount = 30);
    void rawSend(const std::vector<uint8_t>& packet);
    void listen();
protected:
    Serial<SerialImpl>* m_container;

    std::unique_ptr<BaseLib::SerialReaderWriter> _serial;
    int64_t lastSOFtime;
};

}

#endif
