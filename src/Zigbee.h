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

#ifndef ZIGBEE_H_
#define ZIGBEE_H_

#include <cstdint>

#include "../config.h"
#include <homegear-base/BaseLib.h>

#include "PhysicalInterfaces/NodeInfo.h"
#include "ZigbeePacket.h"
#include "PhysicalInterfaces/ZCL.h"

using namespace BaseLib;

namespace Zigbee
{
class ZigbeeCentral;
class ZigbeeNodeInfo;
class ClustersInfo;

class Zigbee : public BaseLib::Systems::DeviceFamily
{
public:
    Zigbee(BaseLib::SharedObjects* bl, BaseLib::Systems::IFamilyEventSink* eventHandler);
    virtual ~Zigbee();
    virtual void dispose();

    virtual bool hasPhysicalInterface()
    {
        return true;
    }
    virtual PVariable getPairingInfo();

    virtual void homegearStarted();
    virtual bool init();

    void updatePeer(ZigbeeNodeInfo& nodeInfo, uint8_t endPoint, const std::string& interfaceID);

    bool peerIEEEExists(uint64_t IEEEaddr);
    bool addressChanged(uint64_t IEEEaddr, uint16_t newShortAddr);
    uint32_t createDeviceForNode(ZigbeeNodeInfo& nodeInfo, uint8_t endPoint);
    void deletePeers(uint64_t IEEEaddr, uint16_t shortAddr);

    const std::shared_ptr<HomegearDevice> GetModelDevice() const
    {
        return _modelDevice;
    }

    bool IsWakeup(uint32_t addr);

    void NotifyTimeout(PZigbeePacket packet);
    void NotifyError(PZigbeePacket packet, const ZigbeeCommands::ZCLFrame& zclFrame);

    std::shared_ptr<ClustersInfo> _clustersInfo;
protected:
    virtual std::shared_ptr<BaseLib::Systems::ICentral> initializeCentral(uint32_t deviceId, int32_t address, std::string serialNumber);
    virtual void createCentral();

    std::shared_ptr<HomegearDevice> _modelDevice;
};

}

#endif
