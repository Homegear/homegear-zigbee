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

#ifndef IZIGBEEINTERFACE_H_
#define IZIGBEEINTERFACE_H_

#include <cstdint>

#include <limits>

#include "../../config.h"
#include <homegear-base/BaseLib.h>

namespace Zigbee
{


class IZigbeeInterface : public BaseLib::Systems::IPhysicalInterface
{
public:
    explicit IZigbeeInterface(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings);
    virtual ~IZigbeeInterface();

    virtual void startListening() override
    {
        BaseLib::Systems::IPhysicalInterface::startListening();
    }
    virtual void stopListening() override
    {
        BaseLib::Systems::IPhysicalInterface::stopListening();
    }

    virtual void Close() {}

    virtual bool PairOn(int seconds) { return false; }
    virtual bool PairOff(uint64_t IEEEaddr, uint16_t shortAddr, int seconds) { return false; }

    virtual bool AbortInclusion(uint16_t nodeId = 0x0) { return false; }
    virtual void NetworkReset() {}

    virtual void sendPacket(std::shared_ptr<BaseLib::Systems::Packet> packet) {}
    virtual void sendPacket(std::shared_ptr<BaseLib::Systems::Packet> packet, bool wakeup) {}


    virtual bool enqueuePacket(std::shared_ptr<BaseLib::Systems::Packet> packet, bool wakeupDevice)
    {
        return false;
    };
    virtual bool tryToSend(uint32_t addr, bool wakeupDevice, bool trymore = false)
    {
        return false;
    };

    virtual void processPacket(uint32_t addr, uint8_t endPoint, std::vector<uint8_t>& data, int offset, uint8_t LinkQuality);

    virtual bool isNetworkDevice() override { return false; }

    virtual bool IsInNetworkAdminMode() const { return false; }
    virtual double TimeSinceLastAdminStage() { return std::numeric_limits<double>::max(); }

    static void addCrc8(std::vector<uint8_t>& packet);
    static uint8_t getCrc8(const std::vector<uint8_t>& packet);

    std::vector<uint8_t> psk;
protected:
    BaseLib::SharedObjects* _bl = nullptr;
    BaseLib::Output _out;

    virtual void rawSend(const std::vector<uint8_t>& packet) {}
};

}

#endif // IZIGBEEINTERFACE_H_
