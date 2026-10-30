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

#ifndef SERIALADMIN_H
#define SERIALADMIN_H

#include <cstdint>

#include <homegear-base/BaseLib.h>

#include "../utils/Event.h"

#include "NodeInfo.h"
#include "AF.h"
#include "ZCL.h"

#include "../utils/CallbackTimer.h"

//#include <list>

namespace Zigbee
{

template<class Serial> class SerialAdmin
{
public:
    SerialAdmin();
    ~SerialAdmin();

    bool PairOn(int seconds);
    bool PairOff(uint64_t IEEEaddr, uint16_t shortAddr, int seconds); // TODO: apparently there is a problem with the 'leave' command: you must target a device (using IEEE address), it's not as for the zwave case
    bool AbortInclusion(uint16_t nodeId = 0x0);
    void NetworkReset();

    Serial* serial;

    enum class NetworkAdminFunction
    {
        NoAdmin = 0,
        RemoveNode,
        AddNode,
        Reset,
    };

    std::atomic_bool _networkManagement;
    std::atomic<NetworkAdminFunction> _networkAdminFunc;

    bool isWakeupAdmin = false;

    std::vector<uint8_t> dskStart;

    BaseLib::Output _out;

    enum class AdminStage
    {
        None = 0,
        PairOnStart,
        RemoveStart,
        AddFound,
        AddDone,
        AddFailed,
        AddUnknown,
        RemoveDone,
        StickReset,
    };

    AdminStage GetAdminStage() const { return _adminStage; }

    double GetTimeSinceLastStage();

    bool StartNetworkAdmin();
    void EndNetworkAdmin(bool trySending = true);

    bool ProcessAdminPacket(std::vector<uint8_t>& packet);

protected:
    enum class PairingPhase
    {
        WaitingForAnnounce = 0,
        WaitingForNodeInfo = 1,
        WaitingForPowerInfo = 2,
        WaitingForActiveEndpoint = 3,
        SimpleDescInfoPhase = 4,
        ModelInfoPhase = 5,
        EndpointBindPhase = 6,
        AttrsCommandsQueryPhase = 7
    };

    std::atomic<PairingPhase> pairingPhase;

    std::shared_ptr<ZigbeeCommands::MTCmdRequest> curRequest;
    std::atomic<int8_t> retries;

    friend class ZigbeeUtils::TimerThreadOneTime<SerialAdmin<Serial>>;
    ZigbeeUtils::TimerThreadOneTime<SerialAdmin<Serial>> timerThread;


    std::atomic<uint16_t> _adminNodeId;

    std::map<AdminStage, std::string> _adminStageMessageId;

    std::mutex _stageMutex;
    std::chrono::time_point<std::chrono::system_clock> lastStageTime;
    std::atomic<AdminStage> _adminStage;

    void SetStageTime();
    void SetAdminStage(AdminStage stage);

    void WaitForSerial();

    void StartFailTimer();
    bool RetryRequest();

    void FireTimeoutCallback();

    void StartWaitingThread();
    void waitForTimeoutThread();


    bool HandlePermitJoinNotification(std::vector<uint8_t>& packet);
    bool HandleEndDeviceAnnceNotification(std::vector<uint8_t>& packet);
    bool HandleNodeDescNotification(std::vector<uint8_t>& packet);
    bool HandlePowerDescNotification(std::vector<uint8_t>& packet);
    bool HandleEndPointNotification(std::vector<uint8_t>& packet);
    bool HandleSimpleDescNotification(std::vector<uint8_t>& packet);
    bool HandleBindNotification(std::vector<uint8_t>& packet);
    bool HandleIncomingMsgNotification(std::vector<uint8_t>& packet);
    bool HandleLeaveNotification(std::vector<uint8_t>& packet);
    bool HandleTCNotification(std::vector<uint8_t>& packet);

    bool HandleDefaultResponse(const ZigbeeCommands::AFIncomingMsgNotification& msgNotification, const ZigbeeCommands::ZCLFrame& zclFrame);

    bool RequestNodeInfo(uint16_t devShortAddr);
    bool RequestPowerInfo(uint16_t devShortAddr);
    bool RequestActiveEndpoint(uint16_t devShortAddr);
    bool RequestSimpleDescInfo(uint16_t devShortAddr, uint8_t ep);
    bool RequestModelInfo(uint16_t devShortAddr, uint8_t ep);
    bool RequestBinding(uint16_t devShortAddr, uint16_t cluster);
    bool RequestAttrs(uint16_t devShortAddr);

    bool RequestNextSimpleDescInfo(uint16_t devShortAddr);
    bool RequestNextModelInfo(uint16_t devShortAddr);
    bool RequestNextEndpointBind(uint16_t devShortAddr);
    bool RequestNextAttrsAndCommands(uint16_t devShortAddr);
    bool RequestReceivedCommands(uint16_t devShortAddr);
    bool RequestSentCommands(uint16_t devShortAddr);

    void CreatePeers(ZigbeeNodeInfo &node);

    std::shared_ptr<ZigbeeCommands::AFDataRequest> GetAttrDiscoverExtendedConfig(uint16_t addrDev, uint8_t endPoint, uint16_t clusterId, uint16_t startAttr = 0, uint8_t maxAttr = 20, bool dir = false);
    std::shared_ptr<ZigbeeCommands::AFDataRequest> GetAttrDiscoverConfig(uint16_t addrDev, uint8_t endPoint, uint16_t clusterId, uint16_t startAttr = 0, uint8_t maxAttr = 20, bool dir = false);
    std::shared_ptr<ZigbeeCommands::AFDataRequest> GetReceivedCommandsDiscoverConfig(uint16_t addrDev, uint8_t endPoint, uint16_t clusterId, uint8_t startCmd = 0, uint8_t maxCmd = 20, bool dir = false);
    std::shared_ptr<ZigbeeCommands::AFDataRequest> GetSentCommandsDiscoverConfig(uint16_t addrDev, uint8_t endPoint, uint16_t clusterId, uint8_t startCmd = 0, uint8_t maxCmd = 20, bool dir = false);

    ZigbeeUtils::Event admFinishedEvent;

    void NotifyAdmFinished()
    {
        admFinishedEvent.SetFlag();
        admFinishedEvent.NotifyAll();
    }

    void ClearInfo()
    {
        addrPairs.clear();
        NodeInfoMap.clear();
        //tcPairs.clear();
    }

    std::thread waitThread;

    std::atomic_bool failure;

    std::mutex nodeMutex;
    std::map<uint64_t, uint16_t> addrPairs;
    std::map<uint16_t, ZigbeeNodeInfo> NodeInfoMap; // may have info on more than one!

    //std::set<std::pair<uint64_t, uint16_t>> tcPairs;


    std::atomic<uint64_t> IEEEaddrToDelete;
};

}

#ifndef SERIAL_ADMIN_IMPL
#include "SerialAdmin.hpp"
#endif // SERIAL_ADMIN_IMPL

#endif // SERIALADMIN_H
