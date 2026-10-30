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

#ifndef HOMEGEAR_ZIGBEE_HGDC_H
#define HOMEGEAR_ZIGBEE_HGDC_H

#include <cstdint>

#include <homegear-base/BaseLib.h>

namespace Zigbee
{

template<class Impl> class Serial;

class HgdcImpl
{
public:
    explicit HgdcImpl(Serial<HgdcImpl> *container);
    ~HgdcImpl();

    static bool IsLocal() { return false; }
    static bool IsGateway() { return false; }

    bool IsOpen() const
    {
        return !IsStopped() && _packetReceivedEventHandlerId != -1;
    }

    bool Open();

    void Reset();

    void Close();

    bool IsStopped() const;

    void SetStopped(bool stop = true);

    void EmptyReadBuffers(int tryCount = 30);
    void rawSend(const std::vector<uint8_t>& packet);
    void listen() {}; // doesn't do anything, for this it should not be called

protected:
    Serial<HgdcImpl>* m_container;

    int32_t _packetReceivedEventHandlerId = -1;

    void processPacket(int64_t familyId, const std::string& serialNumber, const std::vector<uint8_t>& data);
};

}

#endif //HOMEGEAR_ZIGBEE_HGDC_H
