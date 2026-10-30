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

#ifndef ZIGBEECENTRAL_H_
#define ZIGBEECENTRAL_H_

#include <cstdint>

#include "../config.h"
#include <homegear-base/BaseLib.h>
#include "ZigbeePeer.h"
#include "ZigbeePacket.h"
#include "PhysicalInterfaces/NodeInfo.h"

#include <memory>
#include <mutex>
#include <string>
#include <atomic>

namespace Zigbee
{

class ZigbeeCentral : public BaseLib::Systems::ICentral
{
public:
    typedef std::map<uint32_t, uint32_t> DeviceVersionsMap;

    explicit ZigbeeCentral(ICentralEventSink* eventHandler);
    explicit ZigbeeCentral(uint32_t deviceType, std::string serialNumber, ICentralEventSink* eventHandler);
    virtual ~ZigbeeCentral();
    virtual void dispose(bool wait = true) override;

    virtual std::string handleCliCommand(std::string command) override;

    virtual bool onPacketReceived(std::string& senderID, std::shared_ptr<BaseLib::Systems::Packet> packet) override;

    std::shared_ptr<ZigbeePeer> getPeer(uint64_t id);
    std::shared_ptr<ZigbeePeer> getPeer(uint32_t addr);
    std::shared_ptr<ZigbeePeer> getPeer(std::string serialNumber);

    void deletePeerByAddr(uint32_t addr);
    void deletePeer(uint64_t id);
    void deletePeers(uint64_t IEEEaddr, uint16_t shortAddr);

    bool peerIEEEExists(uint64_t IEEEaddr);

    void updatePeer(ZigbeeNodeInfo& nodeInfo, uint8_t endPoint, const std::string& interfaceID);
    bool addressChanged(uint64_t IEEEaddr, uint16_t newShortAddr);
    uint32_t createDeviceForNode(ZigbeeNodeInfo& nodeInfo, uint8_t endPoint);

    virtual PVariable createDevice(BaseLib::PRpcClientInfo clientInfo, int32_t deviceType, std::string serialNumber, int32_t address, int32_t firmwareVersion, std::string interfaceId) override;
    virtual PVariable deleteDevice(BaseLib::PRpcClientInfo clientInfo, std::string serialNumber, int32_t flags) override;
    virtual PVariable deleteDevice(BaseLib::PRpcClientInfo clientInfo, uint64_t peerID, int32_t flags) override;
    virtual PVariable getPairingState(BaseLib::PRpcClientInfo clientInfo) override;
    virtual PVariable setInstallMode(BaseLib::PRpcClientInfo clientInfo, bool on, uint32_t duration, BaseLib::PVariable metadata, bool debugOutput = true) override;
    virtual PVariable setRemoveMode(BaseLib::PRpcClientInfo clientInfo, bool on, uint64_t peerID, uint32_t duration = 60, bool debugOutput = true);

    bool RemovePeerFromArrays(uint64_t id);
    void AddPeerIntoArrays(std::shared_ptr<ZigbeePeer> peer);
    bool WaitForPeer(std::shared_ptr<ZigbeePeer>& peer, unsigned int use_count);

    void AddPairingMessage(const std::string& messageId, const std::string& variable = "");
    void AddPairingState(uint64_t peerId, const std::string& state, const std::string& messageId = "");

    bool IsWakeup(uint32_t addr);

    void NotifyTimeout(PZigbeePacket packet);
    void NotifyError(PZigbeePacket packet, const ZigbeeCommands::ZCLFrame& zclFrame);

protected:
    std::atomic_bool _stopWorkerThread;
    std::thread _workerThread;

    std::atomic_bool _allowDeletePeer;

    std::atomic_bool _stopPairingModeThread;
    std::mutex _pairingModeThreadMutex;
    std::thread _pairingModeThread;

    std::atomic<uint32_t> _deviceTypeCounter;

    std::map<std::string, DeviceVersionsMap> _devicesTypeMap;
    std::mutex _devicesTypeMutex;

    // is protected by the same mutex as 'peerBy' in the base class
    // for each IEEE address there might be more than one peer, one for each 'end point'
    // this is mostly used to locate the peer(s) by IEEE address, for example when a short address changes
    std::map<uint64_t, std::map<uint8_t, std::shared_ptr<BaseLib::Systems::Peer>>> peersByIEEE;

    virtual void loadPeers() override;
    virtual void savePeers(bool full) override;
    virtual void loadVariables() override {}
    virtual void saveVariables() override {}

    std::shared_ptr<ZigbeePeer> createPeer(uint32_t deviceType, int32_t address, std::string serialNumber, bool save = true);

    virtual void worker();
    virtual void init();

    void pairingModeTimer(int32_t duration, bool debugOutput = true);
};

}

#endif
