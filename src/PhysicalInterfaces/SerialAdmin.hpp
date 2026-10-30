/* Copyright 2013-2019 Homegear GmbH */

#include "Serial.h"
#include "../GD.h"
#include "../ZigbeeCentral.h"


#include "AF.h"
#include "SYS.h"
#include "UTIL.h"
#include "APPCFG.h"
#include "SAPI.h"
#include "ZDO.h"
#include "ZCL.h"

#include <chrono>
#include <thread>

#define SERIAL_ADMIN_IMPL

namespace Zigbee
{

template<class Serial> SerialAdmin<Serial>::SerialAdmin()
    :  serial(nullptr), _networkManagement(false), _networkAdminFunc(NetworkAdminFunction::NoAdmin), pairingPhase(PairingPhase::WaitingForAnnounce),
       retries(0), timerThread(this),
       _adminNodeId(0), _adminStage(AdminStage::None),
       IEEEaddrToDelete(0)
{
    //ctor
    _adminStageMessageId[AdminStage::PairOnStart] = "l10n.zigbee.pairing.pairOnStart";
    _adminStageMessageId[AdminStage::AddFound] = "l10n.zigbee.pairing.addFound";
    _adminStageMessageId[AdminStage::AddDone] = "l10n.zigbee.pairing.addDone";
    _adminStageMessageId[AdminStage::AddFailed] = "l10n.zigbee.pairing.addFailed";
    _adminStageMessageId[AdminStage::AddUnknown] = "l10n.zigbee.pairing.addUnknown";
    _adminStageMessageId[AdminStage::RemoveStart] = "l10n.zigbee.pairing.removeStart";
    _adminStageMessageId[AdminStage::RemoveDone] = "l10n.zigbee.pairing.removeDone";

    SetStageTime();
}

template<class Serial> SerialAdmin<Serial>::~SerialAdmin()
{
    NotifyAdmFinished();
}

template<class Serial> void SerialAdmin<Serial>::SetStageTime()
{
    std::lock_guard<std::mutex> lock(_stageMutex);

    lastStageTime = std::chrono::system_clock::now();
}

template<class Serial> void SerialAdmin<Serial>::SetAdminStage(AdminStage stage)
{
    _adminStage = stage;

    // can do more than setting the stage

    // for example, adding pairing messages:
    if (NetworkAdminFunction::AddNode == _networkAdminFunc)
    {
        // make a message depending on the stage
        if (GD::family)
        {
            std::shared_ptr<ZigbeeCentral> central = std::dynamic_pointer_cast<ZigbeeCentral>(GD::family->getCentral());
            if (central)
            {
                // now can set a messages into it
                if (_adminStageMessageId.find(stage) != _adminStageMessageId.end())
                    central->AddPairingMessage(_adminStageMessageId[stage]);
            }
        }
    }
}


template<class Serial> double SerialAdmin<Serial>::GetTimeSinceLastStage()
{
    auto endtime = std::chrono::system_clock::now();

    std::lock_guard<std::mutex> lock(_stageMutex);

    std::chrono::duration<double> diff = endtime - lastStageTime;

    return diff.count();
}


template<class Serial> bool SerialAdmin<Serial>::StartNetworkAdmin()
{
    if (_networkManagement.exchange(true))
    {
        _out.printInfo("Already in network management, cannot enter");
        return false;
    }

    _out.printInfo("Entering network management");

    WaitForSerial();

    SetStageTime();

    std::lock_guard<std::mutex> lock(nodeMutex);
    ClearInfo();

    return true;
}

template<class Serial> void SerialAdmin<Serial>::WaitForSerial()
{
    serial->waitingThread.cmdFinishedEvent.WaitSeconds(5);
}



template<class Serial> bool SerialAdmin<Serial>::HandlePermitJoinNotification(std::vector<uint8_t>& packet)
{
    ZigbeeCommands::ZDOMgmtPermitJoinNotification permitJoinNotification;

    if (permitJoinNotification.Decode(packet))
    {
        _out.printInfo("Info: Permit Join Request notification went well, status: 0x" + BaseLib::HelperFunctions::getHexString(permitJoinNotification.status));

        if (static_cast<uint8_t>(ZigbeeReturnIds::ZSuccess) != permitJoinNotification.status)
        {
            // bail out, don't wait anymore, we received some failure
            SetAdminStage(AdminStage::AddFailed);

            EndNetworkAdmin();
        }

        return true;
    }

    return false;
}

template<class Serial> bool SerialAdmin<Serial>::HandleEndDeviceAnnceNotification(std::vector<uint8_t>& packet)
{
    ZigbeeCommands::ZDOEndDeviceAnnceNotification annce;

    if (annce.Decode(packet))
    {
        const uint64_t devIEEEAddr = annce.srcIEEEAddr;

        if (devIEEEAddr == IEEEaddrToDelete) // blacklisted?
        {
            // this might interfere with regular packets sending/receiving but it must be done for misbehaving devices
            // for now I met only one that does that (the iluminize relay):
            // after requested to leave (with rejoin set to false)
            // it acknowledges (with rejoin set to false, too) and leaves
            // but then it resets and rejoins!
            // this will force it out
            // unfortunately this will probably happen out of admin mode so the sending/receiving of regular packets is resumed

            // maybe the rejoining should be ignored, but for this particular device, a pon with a unpower/repower won't bring the device announce, so it's hard to pair
            // it needs a reset for that
            // I'll leave this on for a while, if issues are found with it, I'll disable it

            // the odd thing is that it doesn't always do that

            ZigbeeCommands::ZDOMgmtLeaveRequest leaveRequest;
            leaveRequest.DeviceAddress = devIEEEAddr;
            leaveRequest.DstAddr = annce.srcAddr;

            std::vector<uint8_t> response;

            _out.printInfo("Trying to remove blacklisted node");

            serial->getResponse(leaveRequest, response);

            ZigbeeCommands::ZDOMgmtLeaveResponse leaveResponse;
            if (leaveResponse.Decode(response))
            {
                _out.printInfo("Info: Leave Request response went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(leaveResponse.status)));

                if (ZigbeeReturnIds::ZSuccess == leaveResponse.status)
                    return true;
            }
            else
                _out.printDebug("Couldn't decode Leave Request response: 0x" + BaseLib::HelperFunctions::getHexString(response));

            return false;
        }

        if (!_networkManagement || NetworkAdminFunction::AddNode != _networkAdminFunc)
            return false;

        if (GD::family && GD::family->peerIEEEExists(devIEEEAddr))
            return false;

        _out.printInfo("Info: Notification about device for address 0x" + BaseLib::HelperFunctions::getHexString(annce.srcAddr));

        const uint16_t devShortAddr = annce.srcAddr;

        std::unique_lock<std::mutex> lock(nodeMutex);

        // there are two cases when such a notification comes
        if (addrPairs.find(devIEEEAddr) != addrPairs.end())
        {
            // already existing, might be due of changing short address
            const uint16_t devShortAddrOld = addrPairs[devIEEEAddr];
            if (devShortAddrOld != devShortAddr) // really changed
            {
                addrPairs[devIEEEAddr] = devShortAddr;

                NodeInfoMap[devShortAddr] = NodeInfoMap[devShortAddrOld];
                NodeInfoMap.erase(devShortAddrOld);
                NodeInfoMap[devShortAddr].SetInfo(annce);

                lock.unlock();

                // restart node addition, because the other one will fail due of the changed address
                if (_networkManagement)
                {
                    pairingPhase = PairingPhase::WaitingForNodeInfo;
                    timerThread.EndTimer();

                    return RequestNodeInfo(devShortAddr);
                    /*
                    if (!RequestNodeInfo(devShortAddr))
                    {
                        if (!RequestPowerInfo(devShortAddr))
                        {
                            if (!RequestActiveEndpoint(devShortAddr))
                            {
                                // this is important, bail out if it doesn't work
                                SetAdminStage(AdminStage::AddFailed);

                                EndNetworkAdmin();
                            }
                        }
                    }
                    */
                }
            }
            else
            {
                // sometimes, instead of replying to the 'request node info', the notification comes again

                if (_networkManagement)
                {
                    bool retry = false;

                    lock.lock();

                    // notification really came before but 'request node info' did not get a reply
                    if (NodeInfoMap[devShortAddr].DeviceInfoValid && !NodeInfoMap[devShortAddr].NodeDescInfoValid)
                        retry = true;

                    lock.unlock();

                    if (retry)
                    {
                        pairingPhase = PairingPhase::WaitingForNodeInfo;
                        timerThread.EndTimer();

                        return RequestNodeInfo(devShortAddr);
                    }

                }
            }
        }
        else
        {
            // device just added, but if there is already one in the map, bail out, it will be added the next pon
            if (!addrPairs.empty()) return false;

            addrPairs[devIEEEAddr] = devShortAddr;
            NodeInfoMap[devShortAddr].SetInfo(annce);

            lock.unlock();

            AbortInclusion(devShortAddr); // don't want to allow other nodes join!!!!

            // now launch the next query for the node:
            pairingPhase = PairingPhase::WaitingForNodeInfo;
            timerThread.EndTimer();

            return RequestNodeInfo(devShortAddr);
            /*
            if (!RequestNodeInfo(devShortAddr))
            {
                if (!RequestPowerInfo(devShortAddr))
                {
                    if (!RequestActiveEndpoint(devShortAddr))
                    {
                        // this is important, bail out if it doesn't work
                        SetAdminStage(AdminStage::AddFailed);

                        EndNetworkAdmin();
                    }
                }
            }
            */
        }

        //return true; // success!!
    }

    return false;
}

template<class Serial> bool SerialAdmin<Serial>::HandleNodeDescNotification(std::vector<uint8_t>& packet)
{
    ZigbeeCommands::ZDONodeDescNotification nodeDescNotification;

    if (nodeDescNotification.Decode(packet))
    {
        if (pairingPhase != PairingPhase::WaitingForNodeInfo)
        {
            _out.printDebug("Received a desc notification in another pairing stage, ignoring");

            return false;
        }

        _out.printInfo("Info: Node Desc Request notification went well, status: 0x" + BaseLib::HelperFunctions::getHexString(nodeDescNotification.Status));

        if (0 == nodeDescNotification.Status)
        {
            std::unique_lock<std::mutex> lock(nodeMutex);

            if (NodeInfoMap.find(nodeDescNotification.NWKAddr) != NodeInfoMap.end())
            {
                NodeInfoMap[nodeDescNotification.NWKAddr].SetInfo(nodeDescNotification);

                lock.unlock();

                pairingPhase = PairingPhase::WaitingForPowerInfo;
                timerThread.EndTimer();

                if (!RequestPowerInfo(nodeDescNotification.NWKAddr))
                {
                    pairingPhase = PairingPhase::WaitingForActiveEndpoint;
                    timerThread.EndTimer();

                    if (!RequestActiveEndpoint(nodeDescNotification.NWKAddr))
                    {
                        // this is important, bail out if it doesn't work
                        SetAdminStage(AdminStage::AddFailed);

                        EndNetworkAdmin();
                    }
                }
            }
        }

        return true;
    }

    return false;
}

template<class Serial> bool SerialAdmin<Serial>::HandlePowerDescNotification(std::vector<uint8_t>& packet)
{
    ZigbeeCommands::ZDOPowerDescNotification powerDescNotification;

    if (powerDescNotification.Decode(packet))
    {
        if (pairingPhase != PairingPhase::WaitingForPowerInfo)
        {
            _out.printDebug("Received a power info notification in another pairing stage, ignoring");

            return false;
        }

        _out.printInfo("Info: Power Desc Request notification went well, status: 0x" + BaseLib::HelperFunctions::getHexString(powerDescNotification.Status));

        std::unique_lock<std::mutex> lock(nodeMutex);

        auto nodeit = NodeInfoMap.find(powerDescNotification.NWKAddr);
        if (nodeit != NodeInfoMap.end())
        {
            if (0 == powerDescNotification.Status)
                NodeInfoMap[powerDescNotification.NWKAddr].SetInfo(powerDescNotification);

            lock.unlock();

            pairingPhase = PairingPhase::WaitingForActiveEndpoint;
            timerThread.EndTimer();

            if (!RequestActiveEndpoint(powerDescNotification.NWKAddr))
            {
                // this is important, bail out if it doesn't work
                SetAdminStage(AdminStage::AddFailed);

                EndNetworkAdmin();
            }
        }

        return true;
    }

    return false;
}

template<class Serial> bool SerialAdmin<Serial>::HandleEndPointNotification(std::vector<uint8_t>& packet)
{
    ZigbeeCommands::ZDOActiveEndPointNotification endPointNotification;

    if (endPointNotification.Decode(packet))
    {
        if (pairingPhase != PairingPhase::WaitingForActiveEndpoint)
        {
            _out.printDebug("Received an active endpoints notification in another pairing stage, ignoring");

            return false;
        }

        _out.printInfo("Info: Notification about active end points for address 0x" + BaseLib::HelperFunctions::getHexString(endPointNotification.NWKAddr) + ", status: 0x" + BaseLib::HelperFunctions::getHexString(endPointNotification.Status));

        // active end point for plug device: 0x03

        if (0 == endPointNotification.Status || !endPointNotification.endPoints.empty()) // even if there is some error code, try to recover if there are some endpoints in there
        {
            if (GD::bl->debugLevel >= 5)
            {
                std::string endPointsStr("End Points:");

                for (uint8_t ep : endPointNotification.endPoints)
                    endPointsStr += " 0x" + BaseLib::HelperFunctions::getHexString(ep);

                _out.printDebug(endPointsStr);
            }

            std::unique_lock<std::mutex> lock(nodeMutex);

            auto nodeit = NodeInfoMap.find(endPointNotification.NWKAddr);
            if (nodeit != NodeInfoMap.end())
            {
                nodeit->second.SetInfo(endPointNotification);

                // now, launch 'simple desc' queries for each end point! Start with the first one.
                if (!nodeit->second.EndPoints.empty())
                {
                    nodeit->second.curEndPointInd = 0;
                    lock.unlock();


                    pairingPhase = PairingPhase::SimpleDescInfoPhase;
                    timerThread.EndTimer();
                    curRequest.reset();

                    bool success = RequestNextSimpleDescInfo(endPointNotification.NWKAddr);
                    lock.lock();

                    if (!success)
                    {
                        nodeit = NodeInfoMap.find(endPointNotification.NWKAddr);
                        if (nodeit == NodeInfoMap.end())
                            return false;

                        nodeit->second.curEndPointInd = 0;

                        lock.unlock();

                        pairingPhase = PairingPhase::ModelInfoPhase;
                        timerThread.EndTimer();
                        curRequest.reset();

                        success = RequestNextModelInfo(endPointNotification.NWKAddr);
                        lock.lock();
                    }


                    if (!success)
                    {
                        nodeit = NodeInfoMap.find(endPointNotification.NWKAddr);
                        if (nodeit == NodeInfoMap.end())
                            return false;

                        nodeit->second.curEndPointInd = 0;
                        nodeit->second.EndPoints[nodeit->second.EndPointsIds[0]].curClusterInd = 0;
                        lock.unlock();

                        pairingPhase = PairingPhase::EndpointBindPhase;
                        timerThread.EndTimer();
                        curRequest.reset();

                        //success =
                        RequestNextEndpointBind(endPointNotification.NWKAddr);
                    }
                }
            }
            // TODO: what to do if somehow there are no endpoints when returning success?
        }
        else
        {
            // otherwise, still try for endpoint 1?

            // this is important, bail out if it doesn't work
            SetAdminStage(AdminStage::AddFailed);

            EndNetworkAdmin();
        }

        return true;
    }

    return false;
}

template<class Serial> bool SerialAdmin<Serial>::RequestNextSimpleDescInfo(uint16_t devShortAddr)
{
    bool success = false;

    std::unique_lock<std::mutex> lock(nodeMutex);
    auto nodeit = NodeInfoMap.find(devShortAddr);

    while (nodeit != NodeInfoMap.end() && nodeit->second.curEndPointInd < nodeit->second.EndPointsIds.size() && !success)
    {
        const uint16_t shortAddr = nodeit->second.srcAddr;
        const uint8_t ep = nodeit->second.EndPointsIds[nodeit->second.curEndPointInd++];

        lock.unlock();
        success = RequestSimpleDescInfo(shortAddr, ep);
        lock.lock();
        nodeit = NodeInfoMap.find(devShortAddr); // must be done in case the node changed its address in the meantime
    }

    return success;
}

template<class Serial> bool SerialAdmin<Serial>::RequestNextModelInfo(uint16_t devShortAddr)
{
    bool success = false;

    std::unique_lock<std::mutex> lock(nodeMutex);
    auto nodeit = NodeInfoMap.find(devShortAddr);

    while (nodeit != NodeInfoMap.end() && nodeit->second.curEndPointInd < nodeit->second.EndPointsIds.size())
    {
        const uint8_t ep = nodeit->second.EndPointsIds[nodeit->second.curEndPointInd];
        auto epinfoit = nodeit->second.EndPoints.find(ep);

        if (epinfoit != nodeit->second.EndPoints.end() && epinfoit->second.HasBasicClusterServerSupport())
        {
            lock.unlock();
            success = RequestModelInfo(devShortAddr, ep);

            // skip to next end point only if the request for the current one was not successful, otherwise stay on the current one
            if (success)
                break;

            lock.lock();
            nodeit = NodeInfoMap.find(devShortAddr); // must be done in case the node changed its address in the meantime

            if (nodeit == NodeInfoMap.end())
                return false;
        }

        ++nodeit->second.curEndPointInd;
    }

    return success;
}


template<class Serial> bool SerialAdmin<Serial>::RequestNextEndpointBind(uint16_t devShortAddr)
{
    bool success = false;

    std::unique_lock<std::mutex> lock(nodeMutex);
    auto nodeit = NodeInfoMap.find(devShortAddr);


    while (nodeit != NodeInfoMap.end() && nodeit->second.curEndPointInd < nodeit->second.EndPointsIds.size())
    {
        auto epinfoit = nodeit->second.EndPoints.find(nodeit->second.EndPointsIds[nodeit->second.curEndPointInd]);
        while (epinfoit != nodeit->second.EndPoints.end() && epinfoit->second.curClusterInd < epinfoit->second.AllClusterList.size() && !success)
        {
            const uint16_t clusterId = epinfoit->second.AllClusterList[epinfoit->second.curClusterInd++];

            lock.unlock();
            success = RequestBinding(devShortAddr, clusterId);
            lock.lock();

            nodeit = NodeInfoMap.find(devShortAddr); // must be done in case the node changed its address in the meantime
            if (nodeit == NodeInfoMap.end())
                return false;

            epinfoit = nodeit->second.EndPoints.find(nodeit->second.EndPointsIds[nodeit->second.curEndPointInd]);
        }

        // skip to next end point only if the request for the current one was not successful, otherwise stay on the current one
        if (success)
            break;

        epinfoit->second.curClusterInd = 0;
        ++nodeit->second.curEndPointInd;

        // if skipped to the next endpoint (and there are still endpoint, that is, not past the end), start from the first cluster of the endpoint
        if (nodeit->second.curEndPointInd < nodeit->second.EndPointsIds.size())
            nodeit->second.EndPoints[nodeit->second.EndPointsIds[nodeit->second.curEndPointInd]].curClusterInd = 0;
    }

    return success;
}


template<class Serial> bool SerialAdmin<Serial>::RequestNextAttrsAndCommands(uint16_t devShortAddr)
{
    bool success = false;

    if(GD::bl->debugLevel >= 6) _out.printDebug("Trying to request next info...");

    std::unique_lock<std::mutex> lock(nodeMutex);
    auto nodeit = NodeInfoMap.find(devShortAddr);

    while (nodeit != NodeInfoMap.end() && nodeit->second.curEndPointInd < nodeit->second.EndPointsIds.size())
    {
        auto epinfoit = nodeit->second.EndPoints.find(nodeit->second.EndPointsIds[nodeit->second.curEndPointInd]);

        while (epinfoit != nodeit->second.EndPoints.end() && epinfoit->second.curClusterInd < epinfoit->second.AllClusterList.size() && !success)
        {
            const ZigbeeNodeInfo::RequestingStage requestingStage = nodeit->second.requestingStage;

            lock.unlock();

            if (requestingStage == ZigbeeNodeInfo::RequestingAttrsStage)
                success = RequestAttrs(devShortAddr);
            else if (requestingStage == ZigbeeNodeInfo::RequestingReceivedCommandsStage)
                success = RequestReceivedCommands(devShortAddr);
            else
                success = RequestSentCommands(devShortAddr);

            lock.lock();

            nodeit = NodeInfoMap.find(devShortAddr); // must be done in case the node changed its address in the meantime
            if (nodeit == NodeInfoMap.end())
                return false;

            epinfoit = nodeit->second.EndPoints.find(nodeit->second.EndPointsIds[nodeit->second.curEndPointInd]);

            if (epinfoit == nodeit->second.EndPoints.end())
                return false;

            if (!success)
            {
                ++epinfoit->second.curClusterInd;
            }

        }

        // skip to next end point only if the request for the current one was not successful, otherwise stay on the current one
        if (success)
            break;

        if (epinfoit != nodeit->second.EndPoints.end())
            epinfoit->second.curClusterInd = 0;

        ++nodeit->second.curEndPointInd;

        // if skipped to the next endpoint (and there are still endpoint, that is, not past the end), start from the first cluster of the endpoint
        nodeit->second.ResetCurrentEndpointIndexes();
    }

    if (!success && nodeit != NodeInfoMap.end())
    {
        // start again querying from the first node, first of its clusters, but for the next query stage
        if (nodeit->second.requestingStage == ZigbeeNodeInfo::RequestingAttrsStage)
        {
            if(GD::bl->debugLevel >= 6) _out.printDebug("Not succeeded in request next info, switching to next stage (received commands)...");

            nodeit->second.curEndPointInd = 0;
            nodeit->second.ResetCurrentEndpointIndexes();
            nodeit->second.requestingStage = ZigbeeNodeInfo::RequestingReceivedCommandsStage;

            lock.unlock();

            pairingPhase = PairingPhase::AttrsCommandsQueryPhase;
            timerThread.EndTimer();
            curRequest.reset();

            return RequestNextAttrsAndCommands(devShortAddr);
        }
        else if (nodeit->second.requestingStage == ZigbeeNodeInfo::RequestingReceivedCommandsStage)
        {
            if(GD::bl->debugLevel >= 6) _out.printDebug("Not succeeded in request next info, switching to next stage (sent commands)...");

            nodeit->second.curEndPointInd = 0;
            nodeit->second.ResetCurrentEndpointIndexes();
            nodeit->second.requestingStage = ZigbeeNodeInfo::RequestingSentCommandsStage;

            const bool supportsQueryingCommands = nodeit->second.SupportsDiscoverCommandsReceived;

            lock.unlock();

            pairingPhase = PairingPhase::AttrsCommandsQueryPhase;
            timerThread.EndTimer();
            curRequest.reset();

            if (supportsQueryingCommands)
                return RequestNextAttrsAndCommands(devShortAddr);
            else
                return false;
        }
    }

    return success;
}


template<class Serial> bool SerialAdmin<Serial>::HandleSimpleDescNotification(std::vector<uint8_t>& packet)
{
    ZigbeeCommands::ZDOSimpleDescNotification simpleDescNotification; // this should come for each endpoint

    if (simpleDescNotification.Decode(packet)) // comes for each endpoint
    {
        if (pairingPhase != PairingPhase::SimpleDescInfoPhase)
        {
            _out.printDebug("Received a simple desc notification in another pairing stage, ignoring");

            return false;
        }

        _out.printInfo("Info: Simple desc notification for address 0x" + BaseLib::HelperFunctions::getHexString(simpleDescNotification.NWKAddr) + " endpoint: 0x" + BaseLib::HelperFunctions::getHexString(simpleDescNotification.Endpoint)+ ", status: 0x" + BaseLib::HelperFunctions::getHexString(simpleDescNotification.Status));

        if (GD::bl->debugLevel >= 5)
        {
            std::string clusters("IN Clusters:");

            for (uint16_t cluster : simpleDescNotification.InClusterList)
            {
                clusters += " 0x" + BaseLib::HelperFunctions::getHexString(cluster);
            }

            _out.printInfo("Info: " + clusters);

            clusters = "OUT clusters: ";

            for (uint16_t cluster : simpleDescNotification.OutClusterList)
            {
                clusters += " 0x" + BaseLib::HelperFunctions::getHexString(cluster);
            }

            _out.printInfo("Info: " + clusters);
        }

        // launch the next endpoint query, if there are more endpoints

        // endpoint 3 clusters for plug device:
        //001000000300040005000600040B0FFC0119008C
        // touchlink:
        //0010 - 10 // not bound
        // Basic:
        //0000 - 0
        //  identify, groups, scenes:
        //0300 - 3 // not bound
        //0400 - 4 // not bound
        //0500 - 5 // not bound
        //0600 - 6 // on off, bound
        //040B - 0B04 // bound
        //0FFC - FC0F // bound

        // if the endpoint supports cluster 0 (Basic), then also query for:
        // the Application Version can be read from attribute 0x0001 of the Basic Cluster, the Manufacturer value can be read from attribute 0x0004 of the Basic Cluster and the most important, the Model value can be read from attribute 0x0005 of the Basic Cluster

        std::unique_lock<std::mutex> lock(nodeMutex);

        auto nodeit = NodeInfoMap.find(simpleDescNotification.SrcAddr);
        if (nodeit != NodeInfoMap.end())
        {
            const bool wasAdded = nodeit->second.SetInfo(simpleDescNotification);
            if (!wasAdded)
            {
                _out.printInfo("Info: Was not added, it does not support standard clusters");

                if (nodeit->second.curEndPointInd > 0)
                    --nodeit->second.curEndPointInd; // was incremented when the request was sent
                // erase it from the ids
                nodeit->second.EndPointsIds.erase(std::remove(nodeit->second.EndPointsIds.begin(), nodeit->second.EndPointsIds.end(), simpleDescNotification.Endpoint), nodeit->second.EndPointsIds.end());
                nodeit->second.EndPoints.erase(simpleDescNotification.Endpoint); // shouldn't be there, but it shouldn't hurt to erase it just in case
            }

            lock.unlock();
            bool success = RequestNextSimpleDescInfo(simpleDescNotification.SrcAddr);
            lock.lock();

            if (!success)
            {
                nodeit = NodeInfoMap.find(simpleDescNotification.SrcAddr);
                if (nodeit == NodeInfoMap.end())
                    return false;

                nodeit->second.curEndPointInd = 0;
                lock.unlock();

                pairingPhase = PairingPhase::ModelInfoPhase;
                timerThread.EndTimer();
                curRequest.reset();

                success = RequestNextModelInfo(simpleDescNotification.SrcAddr);
                lock.lock();
            }

            // if not, start binding each cluster for each endpoint, avoiding the ones we don't need binding (such as identify, groups, scenes, configuration...)
            if (!success)
            {
                nodeit = NodeInfoMap.find(simpleDescNotification.SrcAddr);
                if (nodeit == NodeInfoMap.end())
                    return false;

                nodeit->second.curEndPointInd = 0;
                nodeit->second.EndPoints[nodeit->second.EndPointsIds[0]].curClusterInd = 0;

                lock.unlock();

                pairingPhase = PairingPhase::EndpointBindPhase;
                timerThread.EndTimer();
                curRequest.reset();

                //success =
                RequestNextEndpointBind(simpleDescNotification.SrcAddr);
            }
        }

        return true;
    }

    return false;
}

template<class Serial> bool SerialAdmin<Serial>::HandleBindNotification(std::vector<uint8_t>& packet)
{
    ZigbeeCommands::ZDOBindNotification bindNotification; // bind notification for each requested cluster on each endpoint

    if (bindNotification.Decode(packet))
    {
        if (pairingPhase != PairingPhase::EndpointBindPhase)
        {
            _out.printDebug("Received a bind notification in another pairing stage, ignoring");

            return false;
        }

        std::unique_lock<std::mutex> lock(nodeMutex);

        if (NodeInfoMap.find(bindNotification.srcAddress) != NodeInfoMap.end())
        {
            // TODO: no matter of the status, launch the next bind request
            lock.unlock();

            pairingPhase = PairingPhase::EndpointBindPhase;
            timerThread.EndTimer();
            curRequest.reset();

            bool success = RequestNextEndpointBind(bindNotification.srcAddress);
            if (success)
                return true;

            lock.lock();
            if (NodeInfoMap.find(bindNotification.srcAddress) == NodeInfoMap.end())
                return false;

            ZigbeeNodeInfo &node = NodeInfoMap[bindNotification.srcAddress];

            // end network admin if finished with the node, but what if more than one node joins?
            // TODO: this must be very well thought to avoid issues!!!!
            if (node.curEndPointInd >= node.EndPointsIds.size())
            {
                // start requesting attributes
                node.curEndPointInd = 0;
                node.ResetCurrentEndpointIndexes();

                lock.unlock();

                pairingPhase = PairingPhase::AttrsCommandsQueryPhase;
                timerThread.EndTimer();
                curRequest.reset();

                RequestNextAttrsAndCommands(bindNotification.srcAddress);
            }

            return true;
        }
    }

    return false;
}



template<class Serial> bool SerialAdmin<Serial>::HandleLeaveNotification(std::vector<uint8_t>& packet)
{
    {
        ZigbeeCommands::ZDOMgmtLeaveNotification leaveNotification;

        if (leaveNotification.Decode(packet))
        {
            _out.printInfo("Info: Leave notification for address 0x" + BaseLib::HelperFunctions::getHexString(leaveNotification.srcAddress) + ", status: 0x" + BaseLib::HelperFunctions::getHexString(leaveNotification.status));

            // TODO: I've got a case where only this one came, so probably I should delete it here?

            if (0 == leaveNotification.status)
            {
                // remove the node from the central
                if (GD::family)
                {
                    GD::family->deletePeers(IEEEaddrToDelete, leaveNotification.srcAddress);
                }

                // don't exit from network admin if in add node mode, when a device is reset it might send a 'leave' then join again
                if (_networkAdminFunc == NetworkAdminFunction::RemoveNode)
                {
                    SetAdminStage(AdminStage::RemoveDone);

                    EndNetworkAdmin();
                }
            }

            return true;
        }
    }

    // maybe use only this, because it has IEEE addr in it, too?
    {
        ZigbeeCommands::ZDOLeaveNotification leaveNotification;

        if (leaveNotification.Decode(packet))
        {
            _out.printInfo("Info: Leave notification for address 0x" + BaseLib::HelperFunctions::getHexString(leaveNotification.srcAddr) + ", request: " + (leaveNotification.request ? "request" : "indication"));

            if (0 == leaveNotification.rejoin)
            {
                // remove the node from the central
                // remove it before exiting from network admin, to avoid issues
                if (GD::family)
                {
                    GD::family->deletePeers(leaveNotification.extAddr, leaveNotification.srcAddr);
                }

                if (IEEEaddrToDelete != leaveNotification.extAddr)
                    _out.printDebug("Debug: The leaving notification is for another IEEE address than the one requested to delete");

                // don't exit from network admin if in add node mode, when a device is reset it might send a 'leave' then join again
                if (_networkAdminFunc == NetworkAdminFunction::RemoveNode)
                {
                    SetAdminStage(AdminStage::RemoveDone);

                    EndNetworkAdmin();
                }
            }

            return true;
        }
    }


    return false;
}


template<class Serial> bool SerialAdmin<Serial>::HandleTCNotification(std::vector<uint8_t>& packet)
{
    ZigbeeCommands::ZDOTCDeviceNotification tcNotification;

    // TODO: does not seem to be a good way to identify if a node is added securely!!!

    if (tcNotification.Decode(packet))
    {
        _out.printInfo("Info: Trust center notification for address 0x" + BaseLib::HelperFunctions::getHexString(tcNotification.srcAddr));

        //std::lock_guard<std::mutex> lock(nodeMutex);
        //tcPairs.insert(std::make_pair(tcNotification.srcIEEEAddr, tcNotification.srcAddr));

        return true;
    }

    return false;
}



template<class Serial> bool SerialAdmin<Serial>::HandleDefaultResponse(const ZigbeeCommands::AFIncomingMsgNotification& msgNotification, const ZigbeeCommands::ZCLFrame& zclFrame)
{
    // default response, has 'default response disabled - it looks that some devices do not set that flag, so I'll comment the check out for now
    if (0x0b == zclFrame.cmd /*&& zclFrame.IsDefaultResponseDisabled()*/)
    {
        // is this for 'basic' attribute?
        if (0x0 == msgNotification.ClusterId && zclFrame.payload.size() >= 1 && 0 == zclFrame.payload[0] /* default response for 'read attributes' */)
        {
            if (zclFrame.payload.size() >= 2)
            {
                if (zclFrame.payload[1] != 0)
                {
                    if (pairingPhase != PairingPhase::ModelInfoPhase)
                    {
                        _out.printDebug("Received a default response for model info in another pairing stage, ignoring");

                        return false;
                    }

                    _out.printDebug("Default response received for a model identifier attribute request, failure: " + ZigbeeCommands::ZCLFrame::GetErrorString(zclFrame.payload[1]));
                    // TODO: what to do in such case?
                }
                else
                    _out.printDebug("Default response received for a model identifier attribute request, success code (should not happen, default response should be disabled)!");
            }
            else
                _out.printDebug("Default response received for a model identifier attribute request, payload too short, ignoring");

            return true;
        }
        else if (zclFrame.payload.size() >= 1 && 0x0C == zclFrame.payload[0] /* default response for 'discover attributes' */)
        {
            if (zclFrame.payload.size() >= 2)
            {
                if (zclFrame.payload[1] != 0)
                {
                    _out.printInfo("Info: Default response received for a discover attributes request, failure: " + ZigbeeCommands::ZCLFrame::GetErrorString(zclFrame.payload[1]));

                    if (pairingPhase != PairingPhase::AttrsCommandsQueryPhase)
                    {
                        _out.printDebug("Received a default response for attr/command in another pairing stage, ignoring");

                        return false;
                    }

                    std::unique_lock<std::mutex> lock(nodeMutex);
                    auto nodeit = NodeInfoMap.find(msgNotification.SrcAddr);
                    if (nodeit != NodeInfoMap.end())
                    {
                        if (static_cast<ZigbeeZCLReturnIds>(zclFrame.payload[1]) == ZigbeeZCLReturnIds::UNSUP_GENERAL_COMMAND)
                            nodeit->second.SupportsDiscoverAttrsSimple = false;

                        nodeit->second.curEndPointInd = 0;
                        nodeit->second.ResetCurrentEndpointIndexes();
                        isWakeupAdmin = nodeit->second.IsWakeupDevice();

                        bool shouldCreatePeers = false;
                        if (isWakeupAdmin && !nodeit->second.SupportsDiscoverAttrsSimple)
                        {
                            shouldCreatePeers = true;
                        }
                        else
                        {
                            nodeit->second.requestingStage = ZigbeeNodeInfo::RequestingReceivedCommandsStage;
                            lock.unlock();

                            pairingPhase = PairingPhase::AttrsCommandsQueryPhase;
                            timerThread.EndTimer();
                            curRequest.reset();

                            if (!RequestNextAttrsAndCommands(msgNotification.SrcAddr)) // retry, but with the 'discover commands received'
                            {
                                shouldCreatePeers = true;
                                lock.lock();
                                nodeit = NodeInfoMap.find(msgNotification.SrcAddr);
                                if (nodeit == NodeInfoMap.end())
                                {
                                    _out.printDebug("Node not in the map, bailing out...");
                                    return true;
                                }
                            }
                        }

                        if (shouldCreatePeers)
                        {
                            _out.printDebug("Will create peers (1)...");

                            // if it's on battery and it didn't support attr discovery
                            // since those devices tend to go to sleep very fast
                            // avoid querying commands, there is a very good chance that there is no support for querying those, too
                            nodeit->second.SupportsDiscoverCommandsReceived = false;
                            nodeit->second.SupportsDiscoverCommandsSent = false;

                            if (GD::family)
                                GD::family->_clustersInfo->CheckAndAddMandatory(&nodeit->second, _out);

                            SetAdminStage(AdminStage::AddDone);
                            EndNetworkAdmin();

                            // TODO: now check the node to see if everything was retrieved correctly, then do this:
                            nodeit->second.QueryCompleted = true;
                            ZigbeeNodeInfo nodeCopy = nodeit->second;
                            lock.unlock();

                            timerThread.EndTimer();
                            curRequest.reset();

                            CreatePeers(nodeCopy);

                            lock.lock();
                            ClearInfo();
                        }
                    }
                }
                else
                    _out.printDebug("Default response received for a discover attributes request, success code (should not happen, default response should be disabled)!");
            }
            else
                _out.printDebug("Default response received for a discover attributes request, payload too short, ignoring");

            return true;
        }
        else if (zclFrame.payload.size() >= 1 && 0x15 == zclFrame.payload[0] /* default response for 'discover attributes extended' */)
        {
            if (zclFrame.payload.size() >= 2)
            {
                if (zclFrame.payload[1] != 0)
                {
                    _out.printInfo("Info: Default response received for a discover attributes extended request, failure: " + ZigbeeCommands::ZCLFrame::GetErrorString(zclFrame.payload[1]));

                    if (pairingPhase != PairingPhase::AttrsCommandsQueryPhase)
                    {
                        _out.printDebug("Received a default response for attr/command in another pairing stage, ignoring");

                        return false;
                    }

                    // TODO: what to do in such case?

                    // it happened for cluster zero for a lightbulb device, I guess I should fill with 'mandatory' info in such case
                    // then continue querying

                    // TODO: check to see what's going on with this case and 'fix' it better!!!!!!!!!!!!!!!!!!!!

                    // TODO: maybe try to recover querying for attributes with the 0x0c Discover Attributes instead?
                    // TODO: what should be then done about attrs types?
                    // for known clusters, they could be hardwired (in an xml file that could be used for defining clusters, used for 'dynamic' definitions, as in zwave module), another way of getting the type would be to actually request the attr value, it comes back along with its type

                    std::unique_lock<std::mutex> lock(nodeMutex);

                    if (NodeInfoMap.find(msgNotification.SrcAddr) != NodeInfoMap.end())
                    {
                        ZigbeeNodeInfo &node = NodeInfoMap[msgNotification.SrcAddr];

                        node.curEndPointInd = 0;
                        node.ResetCurrentEndpointIndexes();

                        node.SupportsDiscoverAttrsExtended = false;

                        lock.unlock();

                        pairingPhase = PairingPhase::AttrsCommandsQueryPhase;
                        timerThread.EndTimer();
                        curRequest.reset();

                        if (!RequestNextAttrsAndCommands(msgNotification.SrcAddr)) // retry, but with the simple request for attrs
                        {
                            _out.printDebug("Retry with simple request for attributes did not work as well. This is unrecoverable.");

                            return false;
                        }
                    }
                }
                else
                    _out.printDebug("Default response received for a discover attributes extended request, success code (should not happen, default response should be disabled)!");
            }
            else
                _out.printDebug("Default response received for a discover attributes extended request, payload too short, ignoring");

            return true;
        }
        else if (zclFrame.payload.size() >= 1 && 0x11 == zclFrame.payload[0] /* default response for 'discover commands received' */)
        {
            if (zclFrame.payload.size() >= 2)
            {
                if (zclFrame.payload[1] != 0)
                {
                    _out.printDebug("Default response received for a discover command received request, failure: " + ZigbeeCommands::ZCLFrame::GetErrorString(zclFrame.payload[1]));

                    if (pairingPhase != PairingPhase::AttrsCommandsQueryPhase)
                    {
                        _out.printDebug("Received a default response for attr/command in another pairing stage, ignoring");

                        return false;
                    }

                    std::unique_lock<std::mutex> lock(nodeMutex);

                    auto nodeit = NodeInfoMap.find(msgNotification.SrcAddr);
                    if (nodeit != NodeInfoMap.end())
                    {
                        if (static_cast<ZigbeeZCLReturnIds>(zclFrame.payload[1]) == ZigbeeZCLReturnIds::UNSUP_GENERAL_COMMAND)
                            nodeit->second.SupportsDiscoverCommandsReceived = false;

                        if (nodeit->second.EndPointsIds[nodeit->second.curEndPointInd] == msgNotification.SrcEndpoint)
                        {
                            ZigbeeNodeInfo::EndpointInfo &epinfo = nodeit->second.EndPoints[nodeit->second.EndPointsIds[nodeit->second.curEndPointInd]];

                            if (epinfo.AllClusterList[epinfo.curClusterInd] == msgNotification.ClusterId)
                            {
                                // TODO: set here the info that it does not support the command, in the node
                                // then restart with 0x0c Discover Attributes instead

                                ++epinfo.curClusterInd;

                                // if there is no next cluster, switch to the next endpoint
                                if (epinfo.curClusterInd >= epinfo.AllClusterList.size())
                                {
                                    ++nodeit->second.curEndPointInd;
                                    nodeit->second.ResetCurrentEndpointIndexes();
                                }

                                bool shouldCreatePeers = false;

                                if (nodeit->second.curEndPointInd >= nodeit->second.EndPointsIds.size() || !nodeit->second.SupportsDiscoverCommandsReceived)
                                {
                                    // if those are exhausted, too, terminate (for now)

                                    // TODO: actually here start setting reporting for 'reportable' attributes

                                    nodeit->second.curEndPointInd = 0;
                                    nodeit->second.ResetCurrentEndpointIndexes();

                                    nodeit->second.requestingStage = ZigbeeNodeInfo::RequestingSentCommandsStage;


                                    if (nodeit->second.SupportsDiscoverCommandsReceived)
                                    {
                                        lock.unlock();

                                        pairingPhase = PairingPhase::AttrsCommandsQueryPhase;
                                        timerThread.EndTimer();
                                        curRequest.reset();

                                        if (!RequestNextAttrsAndCommands(msgNotification.SrcAddr)) // retry, but with the 'discover commands sent'
                                        {
                                            shouldCreatePeers = true;
                                            lock.lock();
                                            nodeit = NodeInfoMap.find(msgNotification.SrcAddr);
                                            if (nodeit == NodeInfoMap.end())
                                            {
                                                _out.printDebug("Node not in the map, bailing out...");
                                                return true;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        shouldCreatePeers = true;
                                    }
                                }
                                else
                                {
                                    lock.unlock();

                                    pairingPhase = PairingPhase::AttrsCommandsQueryPhase;
                                    timerThread.EndTimer();
                                    curRequest.reset();

                                    if (!RequestNextAttrsAndCommands(msgNotification.SrcAddr)) // continue querying
                                    {
                                        shouldCreatePeers = true;
                                        lock.lock();
                                        nodeit = NodeInfoMap.find(msgNotification.SrcAddr);
                                        if (nodeit == NodeInfoMap.end())
                                        {
                                            _out.printDebug("Node not in the map, bailing out...");
                                            return true;
                                        }
                                    }
                                }

                                if (shouldCreatePeers)
                                {
                                    _out.printDebug("Will create peers (2)...");

                                    // did not support discover commands received, probably does not support the discover commands sent either, so skip that
                                    nodeit->second.SupportsDiscoverCommandsSent = false;

                                    if (GD::family)
                                        GD::family->_clustersInfo->CheckAndAddMandatory(&nodeit->second, _out);

                                    isWakeupAdmin = nodeit->second.IsWakeupDevice();

                                    SetAdminStage(AdminStage::AddDone);
                                    EndNetworkAdmin();

                                    // TODO: now check the node to see if everything was retrieved correctly, then do this:
                                    nodeit->second.QueryCompleted = true;
                                    ZigbeeNodeInfo nodeCopy = nodeit->second;
                                    lock.unlock();

                                    timerThread.EndTimer();
                                    curRequest.reset();

                                    CreatePeers(nodeCopy);

                                    lock.lock();
                                    ClearInfo();
                                }
                            }
                        }
                    }
                }
                else
                    _out.printDebug("Default response received for a discover command received request, success code (should not happen, default response should be disabled)!");
            }
            else
                _out.printDebug("Default response received for a discover command received request, payload too short, ignoring");

            return true;
        }
        else if (zclFrame.payload.size() >= 1 && 0x13 == zclFrame.payload[0] /* default response for 'discover commands generated' */)
        {
            if (zclFrame.payload.size() >= 2)
            {
                if (zclFrame.payload[1] != 0)
                {
                    _out.printDebug("Default response received for a discover command generated request, failure: " + ZigbeeCommands::ZCLFrame::GetErrorString(zclFrame.payload[1]));

                    if (pairingPhase != PairingPhase::AttrsCommandsQueryPhase)
                    {
                        _out.printDebug("Received a default response for attr/command in another pairing stage, ignoring");

                        return false;
                    }

                    std::unique_lock<std::mutex> lock(nodeMutex);

                    auto nodeit = NodeInfoMap.find(msgNotification.SrcAddr);
                    if (nodeit != NodeInfoMap.end())
                    {
                        if (static_cast<ZigbeeZCLReturnIds>(zclFrame.payload[1]) == ZigbeeZCLReturnIds::UNSUP_GENERAL_COMMAND)
                            nodeit->second.SupportsDiscoverCommandsSent = false;

                        if (nodeit->second.EndPointsIds[nodeit->second.curEndPointInd] == msgNotification.SrcEndpoint)
                        {
                            ZigbeeNodeInfo::EndpointInfo &epinfo = nodeit->second.EndPoints[nodeit->second.EndPointsIds[nodeit->second.curEndPointInd]];

                            if (epinfo.AllClusterList[epinfo.curClusterInd] == msgNotification.ClusterId)
                            {
                                // TODO: set here the info that it does not support the command, in the node
                                // then restart with 0x0c Discover Attributes instead

                                ++epinfo.curClusterInd;

                                // if there is no next cluster, switch to the next endpoint
                                if (epinfo.curClusterInd >= epinfo.AllClusterList.size())
                                {
                                    ++nodeit->second.curEndPointInd;
                                    nodeit->second.ResetCurrentEndpointIndexes();
                                }

                                bool shouldCreatePeers = false;

                                if (nodeit->second.curEndPointInd >= nodeit->second.EndPointsIds.size() || !nodeit->second.SupportsDiscoverCommandsSent)
                                {
                                    shouldCreatePeers = true;
                                }
                                else
                                {
                                    lock.unlock();

                                    pairingPhase = PairingPhase::AttrsCommandsQueryPhase;
                                    timerThread.EndTimer();
                                    curRequest.reset();

                                    if (!RequestNextAttrsAndCommands(msgNotification.SrcAddr))
                                    {
                                        shouldCreatePeers = true;
                                        lock.lock();
                                        nodeit = NodeInfoMap.find(msgNotification.SrcAddr);
                                        if (nodeit == NodeInfoMap.end())
                                        {
                                            _out.printDebug("Node not in the map, bailing out...");
                                            return true;
                                        }
                                    }
                                }

                                if (shouldCreatePeers)
                                {
                                    _out.printDebug("Will create peers (3)...");

                                    // if those are exhausted, too, terminate (for now)

                                    // TODO: actually here start setting reporting for 'reportable' attributes

                                    nodeit->second.curEndPointInd = 0;
                                    nodeit->second.ResetCurrentEndpointIndexes();

                                    // before starting configuring reporting, be sure that at least mandatory attrs are in (should work for those even if querying the attrs failed)
                                    if (GD::family)
                                        GD::family->_clustersInfo->CheckAndAddMandatory(&nodeit->second, _out);

                                    isWakeupAdmin = nodeit->second.IsWakeupDevice();

                                    SetAdminStage(AdminStage::AddDone);
                                    EndNetworkAdmin();

                                    // TODO: now check the node to see if everything was retrieved correctly, then do this:
                                    nodeit->second.QueryCompleted = true;
                                    ZigbeeNodeInfo nodeCopy = nodeit->second;
                                    lock.unlock();

                                    timerThread.EndTimer();
                                    curRequest.reset();

                                    CreatePeers(nodeCopy);

                                    lock.lock();
                                    ClearInfo();
                                }
                            }
                        }
                    }
                }
                else
                    _out.printDebug("Default response received for a discover command generated request, success code (should not happen, default response should be disabled)!");
            }
            else
                _out.printDebug("Default response received for a discover command generated request, payload too short, ignoring");

            return true;
        }
        else if (_networkManagement)
            _out.printInfo("Info: Default response received, not for basic cluster attribute value, discover request or reports config, ignoring");
    }

    return false;
}


template<class Serial> bool SerialAdmin<Serial>::HandleIncomingMsgNotification(std::vector<uint8_t>& packet)
{
    //ZigbeeCommands::ZDOMsgCbIncomingNotification msgNotification;
    ZigbeeCommands::AFIncomingMsgNotification msgNotification;

    if (msgNotification.Decode(packet))
    {
        if (_networkManagement)
        {
            _out.printInfo("Info: Received AF msg notification, cluster id: 0x" + BaseLib::HelperFunctions::getHexString(msgNotification.ClusterId) + " Src addr: 0x" + BaseLib::HelperFunctions::getHexString(msgNotification.SrcAddr) + " Src endpoint: 0x" + BaseLib::HelperFunctions::getHexString(msgNotification.SrcEndpoint) + " Dst endpoint: 0x" + BaseLib::HelperFunctions::getHexString(msgNotification.DstEndpoint));
            //if (msgNotification.SecurityUse) _out.printInfo("Info: Uses security");
            //else _out.printInfo("Info: Not using security");
        }
        // not all of them are for admin!

        // TODO: check the fields!

        ZigbeeCommands::ZCLFrame zclFrame;

        if (zclFrame.Decode(msgNotification.Data))
        {
            if (_networkManagement)
                _out.printInfo("Info: Decoded ZCL frame, type: " + zclFrame.GetDescription());

            if (zclFrame.IsGlobal())
            {
                // TODO: check the fields in the zcl frame!
                // we might be interested (depending on the config phase in admin) on:
                // zclFrame.cmd ==

                // also check 0x0b Default Response - might want to disable this for queries, since we need replies back anyway here
                // even if disabled, it comes back for a 'command' that does not work!

                /*
                0x01 Read Attributes Response - response to querying some fields in the 'basic' cluster, such as the ModelIdentifier (0x0005) - type 'char string' 0x42
                0x07 Configure Reporting Response - response to a config reporting request
                0x0d Discover Attributes Response - response query the attributes - maybe don't use this, use the 'extended', because it has more info (read/write/reportable, the latest being very important)
                0x12 Discover Commands Received Response - response to query the commands
                0x14 Discover Commands Generated Response - response to query the commands
                0x16 Discover Attributes Extended Response - response query the attributes
                */

                if (HandleDefaultResponse(msgNotification, zclFrame))
                    return true;
                else if (0x01 == zclFrame.cmd && 0x0 == msgNotification.ClusterId) // Model Identifier (from 'basic' cluster) response to 'read attr'
                {
                    // in 'payload': 2 octets attrib id, followed by one byte 'status'
                    // if success, one byte data type then the attr value (variable, depends on data type) follows

                    if (_networkManagement)
                        _out.printInfo("Info: Received Model Identifier (from 'basic' cluster) response to 'read attr'");

                    // TODO: check more fields, including transaction id (must be saved somewhere, probably)
                    if (zclFrame.payload.size() >= 3)
                    {
                        const uint16_t attr = ZigbeeCommands::MTCmd::ReadUint16(0, zclFrame.payload);
                        if (0x0005 == attr)
                        {
                            const uint8_t status = zclFrame.payload[2];
                            if (0 == status) //success
                            {
                                if (pairingPhase != PairingPhase::ModelInfoPhase)
                                {
                                    _out.printDebug("Received a model info in another pairing stage, ignoring");

                                    return false;
                                }

                                // ok, now grab it
                                if (zclFrame.payload.size() > 4)
                                {
                                    const uint8_t type = zclFrame.payload[3];
                                    if (0x42 == type) // string
                                    {
                                        uint8_t len = zclFrame.payload[4];

                                        if (len == zclFrame.payload.size() - 5)
                                        {
                                            for (int i = 5; i < len + 5; ++i)
                                            {
                                                if (0 == zclFrame.payload[i])
                                                {
                                                    len = i - 5;
                                                    break;
                                                }
                                            }
                                            const std::string val(reinterpret_cast<const char *>(zclFrame.payload.data()) + 5, len);

                                            std::unique_lock<std::mutex> lock(nodeMutex);

                                            auto nodeit = NodeInfoMap.find(msgNotification.SrcAddr);
                                            if (nodeit != NodeInfoMap.end())
                                            {
                                                _out.printInfo("Info: Received model identifier attribute: " + val);

                                                // TODO: no matter of the status, launch the next bind request

                                                if (nodeit->second.EndPointsIds[nodeit->second.curEndPointInd] == msgNotification.SrcEndpoint)
                                                {
                                                    nodeit->second.EndPoints[msgNotification.SrcEndpoint].modelIdentifier = val;

                                                    ++nodeit->second.curEndPointInd;

                                                    // copy the value in all other end points where the info is not present, just in case
                                                    for (uint8_t endPointInd = 0; endPointInd < nodeit->second.EndPointsIds.size(); ++endPointInd)
                                                    {
                                                        if (nodeit->second.EndPoints[nodeit->second.EndPointsIds[endPointInd]].modelIdentifier.empty())
                                                            nodeit->second.EndPoints[nodeit->second.EndPointsIds[endPointInd]].modelIdentifier = val;
                                                    }

                                                    lock.unlock();

                                                    pairingPhase = PairingPhase::ModelInfoPhase;
                                                    timerThread.EndTimer();
                                                    curRequest.reset();

                                                    bool success = RequestNextModelInfo(msgNotification.SrcAddr);
                                                    lock.lock();


                                                    // if not, start binding each cluster for each endpoint, avoiding the ones we don't need binding (such as identify, groups, scenes, configuration...)

                                                    if (!success)
                                                    {
                                                        nodeit = NodeInfoMap.find(msgNotification.SrcAddr);
                                                        if (nodeit == NodeInfoMap.end())
                                                            return false;

                                                        nodeit->second.curEndPointInd = 0;
                                                        if (!nodeit->second.EndPointsIds.empty())
                                                            nodeit->second.EndPoints[nodeit->second.EndPointsIds[0]].curClusterInd = 0;

                                                        lock.unlock();
                                                        pairingPhase = PairingPhase::EndpointBindPhase;
                                                        timerThread.EndTimer();
                                                        curRequest.reset();

                                                        success = RequestNextEndpointBind(msgNotification.SrcAddr);

                                                        if (!success)
                                                            _out.printInfo("Info: Couldn't start binding");
                                                    }

                                                    return true;
                                                }
                                                else if (_networkManagement)
                                                    _out.printDebug("Received model identifier attribute for unexpected end point"); // TODO: what to do about this one?
                                            }
                                        }
                                        else if (_networkManagement)
                                            _out.printDebug("Received unexpected value length for model identifier attribute request"); // TODO: what to do about this one (should not happen, it's 'standard')?
                                    }
                                    else if (_networkManagement)
                                        _out.printDebug("Received unexpected value type for model identifier attribute request"); // TODO: what to do about this one (should not happen, it's 'standard')?
                                }
                                else if (_networkManagement)
                                    _out.printDebug("Received empty value for model identifier attribute request"); // TODO: what to do about this one?
                            }
                            else if (_networkManagement)
                                _out.printDebug("Received failure for model identifier attribute request: " + ZigbeeCommands::ZCLFrame::GetErrorString(status)); // TODO: what to do about this one?
                        }
                        else if (_networkManagement)
                            _out.printDebug("Unexpected attribute value for basic cluster received, ignoring, attr: 0x" + BaseLib::HelperFunctions::getHexString(attr));
                    }
                    else
                        _out.printDebug("Invalid payload size for ZCL frame received, ignoring");
                }
                else if (0x16 == zclFrame.cmd || 0x0D == zclFrame.cmd || 0x12 == zclFrame.cmd || 0x14 == zclFrame.cmd)
                {
                    if (0x16 == zclFrame.cmd)
                        _out.printInfo("Info: Received Discover Attributes Extended Response");
                    else if (0x0D == zclFrame.cmd)
                        _out.printInfo("Info: Received Discover Attributes Response");
                    else if (0x12 == zclFrame.cmd)
                        _out.printInfo("Info: Received Discover Commands Received Response");
                    else
                        _out.printInfo("Info: Received Discover Commands Generated Response");

                    if (zclFrame.payload.size() >= 1)
                    {
                        std::unique_lock<std::mutex> lock(nodeMutex);

                        auto nodeit = NodeInfoMap.find(msgNotification.SrcAddr);
                        if (nodeit != NodeInfoMap.end())
                        {
                            if (nodeit->second.EndPointsIds[nodeit->second.curEndPointInd] == msgNotification.SrcEndpoint)
                            {
                                ZigbeeNodeInfo::EndpointInfo &epinfo = nodeit->second.EndPoints[nodeit->second.EndPointsIds[nodeit->second.curEndPointInd]];

                                if (epinfo.AllClusterList[epinfo.curClusterInd] == msgNotification.ClusterId)
                                {
                                    if (pairingPhase != PairingPhase::AttrsCommandsQueryPhase)
                                    {
                                        _out.printDebug("Received an attr/command info in another pairing stage, ignoring");

                                        return false;
                                    }

                                    ZigbeeNodeInfo::EndpointInfo::ClusterInfo& clusterInfo = epinfo.ServersClustersInfoMap[msgNotification.ClusterId];

                                    bool somethingAdded = false;

                                    if (0x16 == zclFrame.cmd) // Discover Attributes Extended Response
                                    {
                                        if (nodeit->second.requestingStage != ZigbeeNodeInfo::RequestingAttrsStage)
                                            _out.printDebug("Interrogating node in some other stage, but received an attributes response");

                                        int offset = 1;
                                        while (offset <= static_cast<int>(zclFrame.payload.size()) - 4)
                                        {
                                            const uint16_t attrId = ZigbeeCommands::MTCmd::ReadUint16(offset, zclFrame.payload);

                                            if (clusterInfo.attrsMap.find(attrId) != clusterInfo.attrsMap.end())
                                            {
                                                _out.printDebug("The device answered with some already added attribute, skipping: 0x" + BaseLib::HelperFunctions::getHexString(attrId));
                                            }
                                            else
                                            {
                                                clusterInfo.attrsList.emplace_back(attrId);

                                                ZigbeeNodeInfo::EndpointInfo::ClusterInfo::AttrInfo& attrInfo = clusterInfo.attrsMap[attrId];
                                                attrInfo.attrId = attrId;

                                                attrInfo.type = zclFrame.payload[offset + 2];
                                                attrInfo.accessControl = zclFrame.payload[offset + 3];

                                                somethingAdded = true;

                                                if (GD::bl->debugLevel >= 5)
                                                    _out.printInfo("Info: Ext attr: 0x" + BaseLib::HelperFunctions::getHexString(attrId) + " Type: 0x" + BaseLib::HelperFunctions::getHexString(attrInfo.type) + " Access control: 0x" + BaseLib::HelperFunctions::getHexString(attrInfo.accessControl));
                                            }

                                            offset += 4;
                                        }
                                    }
                                    else if (0x0D == zclFrame.cmd) // Discover Attributes Response
                                    {
                                        if (nodeit->second.requestingStage != ZigbeeNodeInfo::RequestingAttrsStage)
                                            _out.printDebug("Interrogating node in some other stage, but received an attributes response");

                                        int offset = 1;
                                        while (offset <= static_cast<int>(zclFrame.payload.size()) - 3)
                                        {
                                            const uint16_t attrId = ZigbeeCommands::MTCmd::ReadUint16(offset, zclFrame.payload);


                                            if (clusterInfo.attrsMap.find(attrId) != clusterInfo.attrsMap.end())
                                            {
                                                _out.printDebug("The device answered with some already added attribute, skipping: 0x" + BaseLib::HelperFunctions::getHexString(attrId));
                                            }
                                            else
                                            {
                                                clusterInfo.attrsList.emplace_back(attrId);

                                                ZigbeeNodeInfo::EndpointInfo::ClusterInfo::AttrInfo& attrInfo = clusterInfo.attrsMap[attrId];
                                                attrInfo.attrId = attrId;
                                                attrInfo.type = zclFrame.payload[offset + 2];

                                                somethingAdded = true;

                                                if (GD::bl->debugLevel >= 5)
                                                    _out.printInfo("Info: Ext attr: 0x" + BaseLib::HelperFunctions::getHexString(attrId) + " Type: 0x" + BaseLib::HelperFunctions::getHexString(attrInfo.type));
                                            }

                                            offset += 3;
                                        }

                                    }
                                    else if (0x12 == zclFrame.cmd) // Discover Commands Received Response
                                    {
                                        if (nodeit->second.requestingStage != ZigbeeNodeInfo::RequestingReceivedCommandsStage)
                                            _out.printDebug("Interrogating node in some other stage, but received a received commands response");

                                        if (zclFrame.payload.size() > 1)
                                        {
                                            somethingAdded = true;

                                            clusterInfo.commandsReceived.insert(zclFrame.payload.begin() + 1, zclFrame.payload.end());

                                            if (GD::bl->debugLevel >= 5)
                                            {
                                                std::string cmds("IN Commands: ");
                                                for (auto it = zclFrame.payload.begin() + 1; it != zclFrame.payload.end(); ++it)
                                                    cmds +=  " 0x" + BaseLib::HelperFunctions::getHexString(*it);

                                                _out.printInfo("Info: " + cmds);
                                            }
                                        }
                                    }
                                    else // Discover Commands Generated Response
                                    {
                                        if (nodeit->second.requestingStage != ZigbeeNodeInfo::RequestingSentCommandsStage)
                                            _out.printDebug("Interrogating node in some other stage, but received a sent commands response");

                                        if (zclFrame.payload.size() > 1)
                                        {
                                            somethingAdded = true;
                                            clusterInfo.commandsSent.insert(zclFrame.payload.begin() + 1, zclFrame.payload.end());

                                            if (GD::bl->debugLevel >= 5)
                                            {
                                                std::string cmds("OUT Commands: ");
                                                for (auto it = zclFrame.payload.begin() + 1; it != zclFrame.payload.end(); ++it)
                                                    cmds +=  " 0x" + BaseLib::HelperFunctions::getHexString(*it);

                                                _out.printInfo("Info: " + cmds);
                                            }
                                        }
                                    }

                                    bool shouldCreatePeers = false;

                                    // two cases:
                                    if (0 != zclFrame.payload[0] || !somethingAdded)
                                    {
                                        _out.printDebug("Discovery complete, switching...");

                                        // 1:
                                        // current cluster was investigated completely, switch to next cluster
                                        ++epinfo.curClusterInd;

                                        // if there is no next cluster, switch to the next endpoint
                                        if (epinfo.curClusterInd >= epinfo.AllClusterList.size())
                                        {
                                            _out.printDebug("All clusters interrogated, switching...");

                                            ++nodeit->second.curEndPointInd;
                                            nodeit->second.ResetCurrentEndpointIndexes();
                                        }

                                        if (nodeit->second.curEndPointInd >= nodeit->second.EndPointsIds.size())
                                        {
                                            _out.printDebug("All end points interrogated, switching...");

                                            if (nodeit->second.requestingStage == ZigbeeNodeInfo::RequestingAttrsStage)
                                            {
                                                nodeit->second.curEndPointInd = 0;
                                                nodeit->second.ResetCurrentEndpointIndexes();
                                                nodeit->second.requestingStage = ZigbeeNodeInfo::RequestingReceivedCommandsStage;

                                                _out.printDebug("Switched to requesting received commands stage...");
                                            }
                                            else if (nodeit->second.requestingStage == ZigbeeNodeInfo::RequestingReceivedCommandsStage)
                                            {
                                                nodeit->second.curEndPointInd = 0;
                                                nodeit->second.ResetCurrentEndpointIndexes();
                                                nodeit->second.requestingStage = ZigbeeNodeInfo::RequestingSentCommandsStage;

                                                _out.printDebug("Switched to requesting sent commands stage...");
                                            }
                                            else
                                                _out.printDebug("All stages interrogated, switching...");
                                        }

                                        if (nodeit->second.curEndPointInd >= nodeit->second.EndPointsIds.size())
                                        {
                                            // if those are exhausted, too, terminate (for now)
                                            nodeit->second.curEndPointInd = 0;
                                            nodeit->second.ResetCurrentEndpointIndexes();
                                            shouldCreatePeers = true;

                                            _out.printDebug("All end points interrogated...");
                                        }
                                        else
                                        {
                                            lock.unlock();

                                            pairingPhase = PairingPhase::AttrsCommandsQueryPhase;
                                            timerThread.EndTimer();
                                            curRequest.reset();

                                            _out.printDebug("More interrogation follows...");

                                            if (!RequestNextAttrsAndCommands(msgNotification.SrcAddr))
                                            {
                                                _out.printDebug("Nothing more to interrogate. Will create peers (1)...");

                                                shouldCreatePeers = true;
                                                lock.lock();
                                                nodeit = NodeInfoMap.find(msgNotification.SrcAddr);
                                                if (nodeit == NodeInfoMap.end())
                                                {
                                                    _out.printDebug("Node not in the map, bailing out...");
                                                    return true;
                                                }
                                            }
                                        }
                                    }
                                    else
                                    {
                                        // 2:
                                        // not yet finished with retrieval of attrs, continue to retrieve them
                                        lock.unlock();

                                        _out.printDebug("Discovery not complete, requesting more...");

                                        pairingPhase = PairingPhase::AttrsCommandsQueryPhase;
                                        timerThread.EndTimer();
                                        curRequest.reset();

                                        if (!RequestNextAttrsAndCommands(msgNotification.SrcAddr))
                                        {
                                            _out.printDebug("Nothing more to interrogate. Will create peers (2)...");

                                            shouldCreatePeers = true;
                                            lock.lock();
                                            nodeit = NodeInfoMap.find(msgNotification.SrcAddr);
                                            if (nodeit == NodeInfoMap.end())
                                            {
                                                _out.printDebug("Node not in the map, bailing out...");
                                                return true;
                                            }
                                        }
                                    }

                                    if (shouldCreatePeers)
                                    {
                                        _out.printDebug("Will create peers (4)...");

                                        nodeit->second.curEndPointInd = 0;
                                        nodeit->second.ResetCurrentEndpointIndexes();

                                        // before starting configuring reporting, be sure that at least mandatory attrs are in (should work for those even if querying the attrs failed)
                                        if (GD::family)
                                            GD::family->_clustersInfo->CheckAndAddMandatory(&nodeit->second, _out);

                                        isWakeupAdmin = nodeit->second.IsWakeupDevice();

                                        SetAdminStage(AdminStage::AddDone);

                                        EndNetworkAdmin();

                                        // TODO: now check the node to see if everything was retrieved correctly, then do this:
                                        nodeit->second.QueryCompleted = true;

                                        ZigbeeNodeInfo nodeCopy = nodeit->second;

                                        lock.unlock();

                                        timerThread.EndTimer();
                                        curRequest.reset();
                                        CreatePeers(nodeCopy);

                                        lock.lock();
                                        ClearInfo();
                                    }

                                    return true;
                                }
                                else
                                    _out.printDebug("Received Discover Attributes/Commands for unexpected cluster"); // TODO: what to do about this one?
                            }
                        }
                        else
                            _out.printDebug("Received Discover Attributes/Commands for unexpected end point"); // TODO: what to do about this one?
                    }
                    else
                        _out.printDebug("Invalid payload size for ZCL frame received for Discover Attributes/Commands, ignoring");
                }
                else if (_networkManagement)
                    _out.printInfo("Info: Unknown ZCL frame received, ignoring");

                // TODO: deal with more reply types

            }
            else if (_networkManagement)
                _out.printInfo("Info: 'Local' ZCL frame received, ignoring");
        }
        else if (_networkManagement)
            _out.printInfo("Info: Couldn't decode ZCL frame from AF msg notification, ignoring: 0x" + BaseLib::HelperFunctions::getHexString(msgNotification.Data));
    }

    return false;
}


template<class Serial> void SerialAdmin<Serial>::CreatePeers(ZigbeeNodeInfo &node)
{
    if (GD::family)
    {
        for (uint8_t ep : node.EndPointsIds)
            GD::family->updatePeer(node, ep, serial->getID());
    }
}


template<class Serial> bool SerialAdmin<Serial>::ProcessAdminPacket(std::vector<uint8_t>& packet)
{

    if (HandlePermitJoinNotification(packet) || HandleEndDeviceAnnceNotification(packet) || HandleNodeDescNotification(packet) || HandlePowerDescNotification(packet) ||
            HandleEndPointNotification(packet) || HandleSimpleDescNotification(packet) || HandleBindNotification(packet) || HandleIncomingMsgNotification(packet) ||
            HandleLeaveNotification(packet) || HandleTCNotification(packet))
        return true;

    // TODO: will need to request attributes for each cluster, then set reporting on them :(
    // looks more complex than anticipated

    // TODO: in the end it might query all attributes and commands for each cluster in each endpoint!

    return false;
}

template<class Serial> void SerialAdmin<Serial>::StartFailTimer()
{
    retries = 0;

    timerThread.RestartTimer(10000);
}

template<class Serial> void SerialAdmin<Serial>::FireTimeoutCallback()
{
    // this should be done in anohter thead, or else it will deadlock (restarting the timer, more precisely)
    std::thread(&SerialAdmin<Serial>::RetryRequest, this).detach();
}


template<class Serial> bool SerialAdmin<Serial>::RetryRequest()
{
    _out.printInfo("Info: Timeout!");

    if (!curRequest) return false;
    else if (++retries > 2) return false;

    _out.printInfo("Info: Retrying...");

    std::vector<uint8_t> response;

    // timer timeout, restart it
    timerThread.RestartTimer(10000);

    serial->getResponse(*curRequest, response);
    if (response.empty()) return false;

    {
        ZigbeeCommands::ZDONodeDescResponse nodeDescResponse;
        if (nodeDescResponse.Decode(response))
        {
            _out.printInfo("Info: Node info retry request went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(nodeDescResponse.status)));

            if (ZigbeeReturnIds::ZSuccess == nodeDescResponse.status)
                return true;

            return false;
        }
    }

    {
        ZigbeeCommands::ZDOPowerDescResponse powerDescResponse;
        if (powerDescResponse.Decode(response))
        {
            _out.printInfo("Info: Power desc retry request went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(powerDescResponse.status)));

            if (ZigbeeReturnIds::ZSuccess == powerDescResponse.status)
                return true;

            return false;
        }
    }

    {
        ZigbeeCommands::ZDOActiveEndPointResponse endPointResponse;
        if (endPointResponse.Decode(response))
        {
            _out.printInfo("Info: End points retry request for end device went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(endPointResponse.status)));

            if (ZigbeeReturnIds::ZSuccess == endPointResponse.status)
                return true;

            return false;
        }
    }

    {
        ZigbeeCommands::ZDOSimpleDescResponse simpleDescResponse;
        if (simpleDescResponse.Decode(response))
        {
            _out.printInfo("Info: End point retry request for simple description went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(simpleDescResponse.status)));

            if (ZigbeeReturnIds::ZSuccess == simpleDescResponse.status)
                return true;

            return false;
        }
    }

    {
        ZigbeeCommands::AFDataResponse dataResponse;
        if (dataResponse.Decode(response))
        {
            _out.printInfo("Info: Data retry request for model identifier or attrs or commands went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(dataResponse.status)));

            if (ZigbeeReturnIds::ZSuccess == dataResponse.status)
                return true;

            return false;
        }
    }

    {
        ZigbeeCommands::ZDOBindResponse bindResponse;
        if (bindResponse.Decode(response))
        {
            _out.printInfo("Info: Bind retry request for end device went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(bindResponse.status)));

            if (ZigbeeReturnIds::ZSuccess == bindResponse.status)
                return true;

            return false;
        }
    }

    return false;
}



template<class Serial> bool SerialAdmin<Serial>::RequestNodeInfo(uint16_t devShortAddr)
{
    _out.printInfo("Info: Requesting node info for addr 0x" + BaseLib::HelperFunctions::getHexString(devShortAddr));

    std::shared_ptr<ZigbeeCommands::ZDONodeDescRequest> nodeDescRequest = std::make_shared<ZigbeeCommands::ZDONodeDescRequest>();

    nodeDescRequest->dstAddr = devShortAddr;
    nodeDescRequest->addrOfInterest = devShortAddr;

    curRequest = nodeDescRequest;

    std::vector<uint8_t> response;

    StartFailTimer();
    serial->getResponse(*nodeDescRequest, response);

    ZigbeeCommands::ZDONodeDescResponse nodeDescResponse;
    if (nodeDescResponse.Decode(response))
    {
        _out.printInfo("Info: Node info request went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(nodeDescResponse.status)) + " Short addr: 0x" + BaseLib::HelperFunctions::getHexString(devShortAddr));

        if (ZigbeeReturnIds::ZSuccess == nodeDescResponse.status)
            return true;
    }
    else
        _out.printDebug("Couldn't decode node info request response: 0x" + BaseLib::HelperFunctions::getHexString(response));

    return false;
}


template<class Serial> bool SerialAdmin<Serial>::RequestPowerInfo(uint16_t devShortAddr)
{
    _out.printInfo("Info: Requesting power info for addr 0x" + BaseLib::HelperFunctions::getHexString(devShortAddr));

    std::shared_ptr<ZigbeeCommands::ZDOPowerDescRequest> powerDescRequest = std::make_shared<ZigbeeCommands::ZDOPowerDescRequest>();
    powerDescRequest->dstAddr = devShortAddr;
    powerDescRequest->addrOfInterest = devShortAddr;

    curRequest = powerDescRequest;

    std::vector<uint8_t> response;

    StartFailTimer();
    serial->getResponse(*powerDescRequest, response);

    ZigbeeCommands::ZDOPowerDescResponse powerDescResponse;
    if (powerDescResponse.Decode(response))
    {
        _out.printInfo("Info: Power desc request went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(powerDescResponse.status)) + " Short addr: 0x" + BaseLib::HelperFunctions::getHexString(devShortAddr));

        if (ZigbeeReturnIds::ZSuccess == powerDescResponse.status)
            return true;
    }
    else
        _out.printDebug("Couldn't decode power desc request response: 0x" + BaseLib::HelperFunctions::getHexString(response));

    return false;
}

template<class Serial> bool SerialAdmin<Serial>::RequestActiveEndpoint(uint16_t devShortAddr)
{
    _out.printInfo("Info: Requesting active endpoints for addr 0x" + BaseLib::HelperFunctions::getHexString(devShortAddr));

    std::shared_ptr<ZigbeeCommands::ZDOActiveEndPointRequest> endPointRequest = std::make_shared<ZigbeeCommands::ZDOActiveEndPointRequest>();

    endPointRequest->dstAddr = devShortAddr; // that's odd
    endPointRequest->addrOfInterest = devShortAddr;//0xF46A;

    curRequest = endPointRequest;

    std::vector<uint8_t> response;

    StartFailTimer();
    serial->getResponse(*endPointRequest, response);

    ZigbeeCommands::ZDOActiveEndPointResponse endPointResponse;
    if (endPointResponse.Decode(response))
    {
        _out.printInfo("Info: End points request for end device went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(endPointResponse.status)) + " Short addr: 0x" + BaseLib::HelperFunctions::getHexString(devShortAddr));

        if (ZigbeeReturnIds::ZSuccess == endPointResponse.status)
            return true;
    }
    else
        _out.printDebug("Couldn't decode end points request response: 0x" + BaseLib::HelperFunctions::getHexString(response));

    return false;
}


template<class Serial> bool SerialAdmin<Serial>::RequestSimpleDescInfo(uint16_t devShortAddr, uint8_t ep)
{
    _out.printInfo("Info: Requesting simple description for addr 0x" + BaseLib::HelperFunctions::getHexString(devShortAddr) + " End point: 0x" + BaseLib::HelperFunctions::getHexString(ep));

    std::shared_ptr<ZigbeeCommands::ZDOSimpleDescRequest> simpleDescRequest = std::make_shared<ZigbeeCommands::ZDOSimpleDescRequest>();
    simpleDescRequest->endPoint = ep;
    simpleDescRequest->addrOfInterest = devShortAddr;
    simpleDescRequest->dstAddr = devShortAddr;

    curRequest = simpleDescRequest;

    std::vector<uint8_t> response;

    StartFailTimer();
    serial->getResponse(*simpleDescRequest, response);

    ZigbeeCommands::ZDOSimpleDescResponse simpleDescResponse;
    if (simpleDescResponse.Decode(response))
    {
        _out.printInfo("Info: End point request for simple description went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(simpleDescResponse.status)) + " Short addr: 0x" + BaseLib::HelperFunctions::getHexString(devShortAddr) + " End point: 0x" + BaseLib::HelperFunctions::getHexString(ep));

        if (ZigbeeReturnIds::ZSuccess == simpleDescResponse.status)
            return true;
    }
    else
        _out.printDebug("Couldn't decode simple desc request response: 0x" + BaseLib::HelperFunctions::getHexString(response));

    return false;
}

template<class Serial> bool SerialAdmin<Serial>::RequestModelInfo(uint16_t devShortAddr, uint8_t ep)
{
    _out.printInfo("Info: Requesting model info for addr 0x" + BaseLib::HelperFunctions::getHexString(devShortAddr) + " End point: 0x" + BaseLib::HelperFunctions::getHexString(ep));

    std::shared_ptr<ZigbeeCommands::AFDataRequest> dataRequest = serial->GetReadAttr(devShortAddr, ep, 0x0 /*basic cluster*/, 0x0005 /*Model Identifier attribute*/);

    curRequest = dataRequest;

    std::vector<uint8_t> response;

    StartFailTimer();
    serial->getResponse(*dataRequest, response);

    ZigbeeCommands::AFDataResponse dataResponse;
    if (dataResponse.Decode(response))
    {
        _out.printInfo("Info: Data request for model identifier went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(dataResponse.status)) + " Short addr: 0x" + BaseLib::HelperFunctions::getHexString(devShortAddr) + " End point: 0x" + BaseLib::HelperFunctions::getHexString(ep));

        if (ZigbeeReturnIds::ZSuccess == dataResponse.status)
            return true;
    }
    else
        _out.printDebug("Couldn't decode data request response for model identifier: 0x" + BaseLib::HelperFunctions::getHexString(response));

    return false;
}


template<class Serial> bool SerialAdmin<Serial>::RequestBinding(uint16_t devShortAddr, uint16_t cluster)
{
    if (!ZigbeeNodeInfo::ShouldBeBound(cluster))
        return false;

    std::shared_ptr<ZigbeeCommands::ZDOBindRequest> bindRequest = std::make_shared<ZigbeeCommands::ZDOBindRequest>();
    bindRequest->DstAddr = devShortAddr;

    std::unique_lock<std::mutex> lock(nodeMutex);

    auto nodeit = NodeInfoMap.find(devShortAddr);
    if (nodeit == NodeInfoMap.end())
        return false;

    bindRequest->SrcAddress = nodeit->second.srcIEEEAddr;
    bindRequest->SrcEndpoint = nodeit->second.EndPointsIds[nodeit->second.curEndPointInd];

    lock.unlock();

    bindRequest->ClusterId = cluster;

    bindRequest->DstAddressIEEE = serial->IEEEAddress; // the IEEEAddress of the coordinator
    bindRequest->DstEndpoint = 0x1; // active end point on the coordinator, it is registered

    _out.printInfo("Info: Requesting binding for addr 0x" + BaseLib::HelperFunctions::getHexString(devShortAddr) + " End point: 0x" + BaseLib::HelperFunctions::getHexString(bindRequest->SrcEndpoint) + " Cluster: 0x" + BaseLib::HelperFunctions::getHexString(cluster));

    curRequest = bindRequest;

    std::vector<uint8_t> response;

    StartFailTimer();
    serial->getResponse(*bindRequest, response);

    ZigbeeCommands::ZDOBindResponse bindResponse;
    if (bindResponse.Decode(response))
    {
        _out.printInfo("Info: Bind request for end device went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(bindResponse.status)));

        if (ZigbeeReturnIds::ZSuccess == bindResponse.status)
            return true;
    }
    else
        _out.printDebug("Couldn't decode bind request response: 0x" + BaseLib::HelperFunctions::getHexString(response));

    return false;
}

template<class Serial> bool SerialAdmin<Serial>::RequestAttrs(uint16_t devShortAddr)
{
    std::unique_lock<std::mutex> lock(nodeMutex);

    auto nodeit = NodeInfoMap.find(devShortAddr);
    if (nodeit == NodeInfoMap.end())
        return false;

    uint8_t ep = nodeit->second.EndPointsIds[nodeit->second.curEndPointInd];
    //if (ep > 240) return false; // end points with bigger values are 'special' - this is redundant now, such end point should be eliminated from the list


    ZigbeeNodeInfo::EndpointInfo &epinfo = nodeit->second.EndPoints[ep];

    const uint16_t cluster = epinfo.AllClusterList[epinfo.curClusterInd];

    // for now we're interested only in attributes for 'server' clusters and only for 'standard' ones (that is, not manufacturer-specific, as we don't have descriptions for those), in the future this should be extended to query all of them
    // for that case, see 'direction'
    if (!epinfo.ServersClustersInfoMap[cluster].server || ZigbeeNodeInfo::EndpointInfo::ClusterInfo::IsManufacturerSpecific(cluster))
        return false;

    uint16_t attrsStart = 0;
    if (!epinfo.ServersClustersInfoMap[cluster].attrsList.empty())
        attrsStart = epinfo.ServersClustersInfoMap[cluster].attrsList.back();

    const bool SupportsDiscoverAttrsExtended = nodeit->second.SupportsDiscoverAttrsExtended;

    std::shared_ptr<ZigbeeCommands::AFDataRequest> attrsRequest = SupportsDiscoverAttrsExtended ? GetAttrDiscoverExtendedConfig(nodeit->second.srcAddr, ep, cluster, attrsStart) : GetAttrDiscoverConfig(nodeit->second.srcAddr, ep, cluster, attrsStart);

    lock.unlock();

    _out.printInfo(std::string("Info: Requesting ") + (SupportsDiscoverAttrsExtended ? "Extended " : "") + "Attrs for addr 0x" + BaseLib::HelperFunctions::getHexString(devShortAddr) + " End point: 0x" + BaseLib::HelperFunctions::getHexString(ep) + " Cluster: 0x" + BaseLib::HelperFunctions::getHexString(cluster) + " Start index: 0x" + BaseLib::HelperFunctions::getHexString(attrsStart));

    std::vector<uint8_t> response;

    curRequest = attrsRequest;

    StartFailTimer();
    serial->getResponse(*attrsRequest, response);

    ZigbeeCommands::AFDataResponse attrsResponse;
    if (attrsResponse.Decode(response))
    {
        _out.printInfo(std::string("Info: ")  + (SupportsDiscoverAttrsExtended ? "Extended " : "") +  "Attrs request response went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(attrsResponse.status)) + " Short addr: 0x"  + BaseLib::HelperFunctions::getHexString(devShortAddr) + " End point: 0x" + BaseLib::HelperFunctions::getHexString(ep) + " Cluster: 0x" + BaseLib::HelperFunctions::getHexString(cluster) + " Start index: 0x" + BaseLib::HelperFunctions::getHexString(attrsStart));

        if (ZigbeeReturnIds::ZSuccess == attrsResponse.status)
            return true;
    }
    else
        _out.printDebug(std::string("Couldn't decode ")  + (SupportsDiscoverAttrsExtended ? "extended " : "") + "Attrs request response: 0x" + BaseLib::HelperFunctions::getHexString(response));

    return false;
}


template<class Serial> bool SerialAdmin<Serial>::RequestReceivedCommands(uint16_t devShortAddr)
{
    std::unique_lock<std::mutex> lock(nodeMutex);

    auto nodeit = NodeInfoMap.find(devShortAddr);
    if (nodeit == NodeInfoMap.end())
        return false;

    uint8_t ep = nodeit->second.EndPointsIds[nodeit->second.curEndPointInd];
    //if (ep > 240) return false; // end points with bigger values are 'special' - this is redundant now, such end point should be eliminated from the list

    ZigbeeNodeInfo::EndpointInfo &epinfo = nodeit->second.EndPoints[ep];

    const uint16_t cluster = epinfo.AllClusterList[epinfo.curClusterInd];

    // for now we're interested only in commands for 'server' clusters and only for 'standard' ones (that is, not manufacturer-specific, as we don't have descriptions for those), in the future this should be extended to query all of them
    // for that case, see 'direction'
    if (!epinfo.ServersClustersInfoMap[cluster].server || ZigbeeNodeInfo::EndpointInfo::ClusterInfo::IsManufacturerSpecific(cluster))
        return false;

    const uint16_t cmdsStart = epinfo.ServersClustersInfoMap[cluster].commandsReceived.empty() ? 0 : (*epinfo.ServersClustersInfoMap[cluster].commandsReceived.rbegin() + 1);
    std::shared_ptr<ZigbeeCommands::AFDataRequest> cmdsRequest = GetReceivedCommandsDiscoverConfig(nodeit->second.srcAddr, ep, cluster, cmdsStart);

    lock.unlock();

    _out.printInfo(std::string("Info: Requesting Commands Received for addr 0x") + BaseLib::HelperFunctions::getHexString(devShortAddr) + " End point: 0x" + BaseLib::HelperFunctions::getHexString(ep) + " Cluster: 0x" + BaseLib::HelperFunctions::getHexString(cluster) + " Start index: 0x" + BaseLib::HelperFunctions::getHexString(cmdsStart));

    std::vector<uint8_t> response;

    curRequest = cmdsRequest;

    StartFailTimer();
    serial->getResponse(*cmdsRequest, response);

    ZigbeeCommands::AFDataResponse cmdsResponse;
    if (cmdsResponse.Decode(response))
    {
        _out.printInfo(std::string("Info: Commands Received request response went well, status: 0x") + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(cmdsResponse.status)) + " Short addr: 0x"  + BaseLib::HelperFunctions::getHexString(devShortAddr) + " End point: 0x" + BaseLib::HelperFunctions::getHexString(ep) + " Cluster: 0x" + BaseLib::HelperFunctions::getHexString(cluster) + " Start index: 0x" + BaseLib::HelperFunctions::getHexString(cmdsStart));

        if (ZigbeeReturnIds::ZSuccess == cmdsResponse.status)
            return true;
    }
    else
        _out.printDebug("Couldn't decode Commands Received request response: 0x" + BaseLib::HelperFunctions::getHexString(response));

    return false;
}


template<class Serial> bool SerialAdmin<Serial>::RequestSentCommands(uint16_t devShortAddr)
{
    std::unique_lock<std::mutex> lock(nodeMutex);

    auto nodeit = NodeInfoMap.find(devShortAddr);
    if (nodeit == NodeInfoMap.end())
        return false;

    uint8_t ep = nodeit->second.EndPointsIds[nodeit->second.curEndPointInd];
    //if (ep > 240) return false; // end points with bigger values are 'special' - this is redundant now, such end point should be eliminated from the list

    ZigbeeNodeInfo::EndpointInfo &epinfo = nodeit->second.EndPoints[ep];

    const uint16_t cluster = epinfo.AllClusterList[epinfo.curClusterInd];

    // for now we're interested only in commands for 'server' clusters and only for 'standard' ones (that is, not manufacturer-specific, as we don't have descriptions for those), in the future this should be extended to query all of them
    // for that case, see 'direction'
    if (!epinfo.ServersClustersInfoMap[cluster].server || ZigbeeNodeInfo::EndpointInfo::ClusterInfo::IsManufacturerSpecific(cluster))
        return false;

    const uint16_t cmdsStart = epinfo.ServersClustersInfoMap[cluster].commandsSent.empty() ? 0 : (*epinfo.ServersClustersInfoMap[cluster].commandsSent.rbegin() + 1);
    std::shared_ptr<ZigbeeCommands::AFDataRequest> cmdsRequest = GetSentCommandsDiscoverConfig(nodeit->second.srcAddr, ep, cluster, cmdsStart);

    lock.unlock();

    _out.printInfo(std::string("Info: Requesting Commands Generated for addr 0x") + BaseLib::HelperFunctions::getHexString(devShortAddr) + " End point: 0x" + BaseLib::HelperFunctions::getHexString(ep) + " Cluster: 0x" + BaseLib::HelperFunctions::getHexString(cluster) + " Start index: 0x" + BaseLib::HelperFunctions::getHexString(cmdsStart));

    std::vector<uint8_t> response;

    curRequest = cmdsRequest;

    StartFailTimer();
    serial->getResponse(*cmdsRequest, response);

    ZigbeeCommands::AFDataResponse cmdsResponse;
    if (cmdsResponse.Decode(response))
    {
        _out.printInfo(std::string("Info: Commands Generated request response went well, status: 0x") + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(cmdsResponse.status)) + " Short addr: 0x"  + BaseLib::HelperFunctions::getHexString(devShortAddr) + " End point: 0x" + BaseLib::HelperFunctions::getHexString(ep) + " Cluster: 0x" + BaseLib::HelperFunctions::getHexString(cluster) + " Start index: 0x" + BaseLib::HelperFunctions::getHexString(cmdsStart));

        if (ZigbeeReturnIds::ZSuccess == cmdsResponse.status)
            return true;
    }
    else
        _out.printDebug("Couldn't decode Commands Generated request response: 0x" + BaseLib::HelperFunctions::getHexString(response));

    return false;
}


template<class Serial> std::shared_ptr<ZigbeeCommands::AFDataRequest> SerialAdmin<Serial>::GetAttrDiscoverExtendedConfig(uint16_t addrDev, uint8_t endPoint, uint16_t clusterId, uint16_t startAttr, uint8_t maxAttr, bool dir)
{
    std::shared_ptr<ZigbeeCommands::AFDataRequest> dataRequest = std::make_shared<ZigbeeCommands::AFDataRequest>();
    dataRequest->DstAddr = addrDev;
    dataRequest->DstEndpoint = endPoint;
    //dataRequest->SrcEndpoint = 1;
    dataRequest->ClusterId = clusterId; //onoff
    dataRequest->TransId = serial->GetNextTransactionCount();
    //dataRequest->Options = 0x30; // ACK, discover route (TODO: do we need ACK if 'default response' comes back?)

    // Note: the ACK comes as a AFDataNotification and it's with 'success' in status even if the sent command is wrong (probably it means only delivery success and nothing more)
    // if I set 'wildcard' for profile id, it does not appear to work!
    // a default response (AFIncomingMsgNotification) comes back with the same zcl transaction id in it (but different seq number, apparently that's counted on the device, not reflects what we send)

    ZigbeeCommands::ZCLFrame zclFrame;

    zclFrame.SetGlobal(true);
    zclFrame.SetDirection(dir); // from client to server!
    zclFrame.TransactionSeq = serial->GetZCLNextTransactionCount();
    zclFrame.cmd = 0x15; // discover attrs extended
    zclFrame.payload =
    {
        0x00, 0x00, maxAttr
    };

    ZigbeeCommands::AFDataRequest::WriteUint16(0, startAttr, zclFrame.payload);

    dataRequest->Data = zclFrame.GetEncoded();

    return dataRequest;
}


template<class Serial> std::shared_ptr<ZigbeeCommands::AFDataRequest> SerialAdmin<Serial>::GetAttrDiscoverConfig(uint16_t addrDev, uint8_t endPoint, uint16_t clusterId, uint16_t startAttr, uint8_t maxAttr, bool dir)
{
    std::shared_ptr<ZigbeeCommands::AFDataRequest> dataRequest = std::make_shared<ZigbeeCommands::AFDataRequest>();
    dataRequest->DstAddr = addrDev;
    dataRequest->DstEndpoint = endPoint;
    //dataRequest->SrcEndpoint = 1;
    dataRequest->ClusterId = clusterId; //onoff
    dataRequest->TransId = serial->GetNextTransactionCount();
    //dataRequest->Options = 0x30; // ACK, discover route (TODO: do we need ACK if 'default response' comes back?)

    // Note: the ACK comes as a AFDataNotification and it's with 'success' in status even if the sent command is wrong (probably it means only delivery success and nothing more)
    // if I set 'wildcard' for profile id, it does not appear to work!
    // a default response (AFIncomingMsgNotification) comes back with the same zcl transaction id in it (but different seq number, apparently that's counted on the device, not reflects what we send)

    ZigbeeCommands::ZCLFrame zclFrame;

    zclFrame.SetGlobal(true);
    zclFrame.SetDirection(dir); // from client to server!
    zclFrame.TransactionSeq = serial->GetZCLNextTransactionCount();
    zclFrame.cmd = 0x0C; // discover attrs
    zclFrame.payload =
    {
        0x00, 0x00, maxAttr
    };

    ZigbeeCommands::AFDataRequest::WriteUint16(0, startAttr, zclFrame.payload);

    dataRequest->Data = zclFrame.GetEncoded();

    return dataRequest;
}


template<class Serial> std::shared_ptr<ZigbeeCommands::AFDataRequest> SerialAdmin<Serial>::GetReceivedCommandsDiscoverConfig(uint16_t addrDev, uint8_t endPoint, uint16_t clusterId, uint8_t startCmd, uint8_t maxCmd, bool dir)
{
    std::shared_ptr<ZigbeeCommands::AFDataRequest> dataRequest = std::make_shared<ZigbeeCommands::AFDataRequest>();
    dataRequest->DstAddr = addrDev;
    dataRequest->DstEndpoint = endPoint;
    //dataRequest->SrcEndpoint = 1;
    dataRequest->ClusterId = clusterId; //onoff
    dataRequest->TransId = serial->GetNextTransactionCount();
    //dataRequest->Options = 0x30; // ACK, discover route (TODO: do we need ACK if 'default response' comes back?)

    // Note: the ACK comes as a AFDataNotification and it's with 'success' in status even if the sent command is wrong (probably it means only delivery success and nothing more)
    // if I set 'wildcard' for profile id, it does not appear to work!
    // a default response (AFIncomingMsgNotification) comes back with the same zcl transaction id in it (but different seq number, apparently that's counted on the device, not reflects what we send)

    ZigbeeCommands::ZCLFrame zclFrame;

    zclFrame.SetGlobal(true);
    zclFrame.SetDirection(dir); // from client to server!
    zclFrame.TransactionSeq = serial->GetZCLNextTransactionCount();
    zclFrame.cmd = 0x11; // Discover Commands Received
    zclFrame.payload =
    {
        startCmd, maxCmd
    };

    dataRequest->Data = zclFrame.GetEncoded();

    return dataRequest;
}


template<class Serial> std::shared_ptr<ZigbeeCommands::AFDataRequest> SerialAdmin<Serial>::GetSentCommandsDiscoverConfig(uint16_t addrDev, uint8_t endPoint, uint16_t clusterId, uint8_t startCmd, uint8_t maxCmd, bool dir)
{
    std::shared_ptr<ZigbeeCommands::AFDataRequest> dataRequest = std::make_shared<ZigbeeCommands::AFDataRequest>();
    dataRequest->DstAddr = addrDev;
    dataRequest->DstEndpoint = endPoint;
    //dataRequest->SrcEndpoint = 1;
    dataRequest->ClusterId = clusterId; //onoff
    dataRequest->TransId = serial->GetNextTransactionCount();
    //dataRequest->Options = 0x30; // ACK, discover route (TODO: do we need ACK if 'default response' comes back?)

    // Note: the ACK comes as a AFDataNotification and it's with 'success' in status even if the sent command is wrong (probably it means only delivery success and nothing more)
    // if I set 'wildcard' for profile id, it does not appear to work!
    // a default response (AFIncomingMsgNotification) comes back with the same zcl transaction id in it (but different seq number, apparently that's counted on the device, not reflects what we send)

    ZigbeeCommands::ZCLFrame zclFrame;

    zclFrame.SetGlobal(true);
    zclFrame.SetDirection(dir); // from client to server!
    zclFrame.TransactionSeq = serial->GetZCLNextTransactionCount();
    zclFrame.cmd = 0x13; // Discover Commands Generated
    zclFrame.payload =
    {
        startCmd, maxCmd
    };

    dataRequest->Data = zclFrame.GetEncoded();

    return dataRequest;
}


template<class Serial> bool SerialAdmin<Serial>::PairOn(int seconds)
{
    if (!StartNetworkAdmin())
        return false;

    _out.printInfo("Pair on");

    IEEEaddrToDelete = 0; // now I want to leave in the blacklisted addr

    // begin anyway
    _networkAdminFunc = NetworkAdminFunction::AddNode;

    _out.printInfo("Trying to add node");

    SetAdminStage(AdminStage::PairOnStart);

    pairingPhase = PairingPhase::WaitingForAnnounce;
    curRequest.reset();

    ZigbeeCommands::ZDOMgmtPermitJoinRequest permitJoinRequest;
    permitJoinRequest.Duration = seconds;

    std::vector<uint8_t> response;
    serial->getResponse(permitJoinRequest, response);

    ZigbeeCommands::ZDOMgmtPermitJoinResponse permitJoinResponse;
    if (permitJoinResponse.Decode(response))
    {
        _out.printInfo("Info: Permit Join Request response went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(permitJoinResponse.status)));

        if (ZigbeeReturnIds::ZSuccess != permitJoinResponse.status)
        {
            SetAdminStage(AdminStage::AddFailed);

            EndNetworkAdmin();

            return false;
        }
    }
    else
    {
        _out.printDebug("Couldn't decode Permit Join Request response: 0x" + BaseLib::HelperFunctions::getHexString(response));

        SetAdminStage(AdminStage::AddFailed);

        EndNetworkAdmin();

        return false;
    }

    return true;
}

template<class Serial> bool SerialAdmin<Serial>::PairOff(uint64_t IEEEaddr, uint16_t shortAddr, int seconds)
{
    if (!StartNetworkAdmin())
        return false;

    _out.printInfo("Pair off");

    _networkAdminFunc = NetworkAdminFunction::RemoveNode;

    SetAdminStage(AdminStage::RemoveStart);

    ZigbeeCommands::ZDOMgmtLeaveRequest leaveRequest;
    leaveRequest.DeviceAddress = IEEEaddr;
    leaveRequest.DstAddr = shortAddr;

    std::vector<uint8_t> response;

    IEEEaddrToDelete = IEEEaddr;

    _out.printInfo("Trying to remove node");

    serial->getResponse(leaveRequest, response);

    ZigbeeCommands::ZDOMgmtLeaveResponse leaveResponse;
    if (leaveResponse.Decode(response))
    {
        _out.printInfo("Info: Leave Request response went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(leaveResponse.status)));

        if (ZigbeeReturnIds::ZSuccess == leaveResponse.status)
            return true;
    }
    else
        _out.printDebug("Couldn't decode Leave Request response: 0x" + BaseLib::HelperFunctions::getHexString(response));

    SetAdminStage(AdminStage::RemoveDone);
    EndNetworkAdmin();

    return false;
}




template<class Serial> void SerialAdmin<Serial>::NetworkReset()
{
    if (!StartNetworkAdmin())
        return;

    SetStageTime();

    _out.printInfo("Reset network");

    SetAdminStage(AdminStage::StickReset);

    _adminNodeId = 0;
    _networkAdminFunc = NetworkAdminFunction::Reset;

    serial->ResetStick();


    serial->CleanCmdQueues();
    serial->startListening();

    EndNetworkAdmin();
}



template<class Serial> bool SerialAdmin<Serial>::AbortInclusion(uint16_t nodeId)
{
    if (!_networkManagement)
        return false;

    if (NetworkAdminFunction::AddNode == _networkAdminFunc || NetworkAdminFunction::RemoveNode == _networkAdminFunc)
    {
        ZigbeeCommands::ZDOMgmtPermitJoinRequest permitJoinRequest;
        permitJoinRequest.Duration = 0;

        std::vector<uint8_t> response;
        serial->getResponse(permitJoinRequest, response);

        ZigbeeCommands::ZDOMgmtPermitJoinResponse permitJoinResponse;
        if (permitJoinResponse.Decode(response))
        {
            _out.printInfo("Info: Permit Join Request stop response went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(permitJoinResponse.status)));

            if (ZigbeeReturnIds::ZSuccess != permitJoinResponse.status)
                return false;
        }
        else
        {
            _out.printDebug("Couldn't decode Permit Join Request stop response: 0x" + BaseLib::HelperFunctions::getHexString(response));
            return false;
        }
    }

    if (0x0 == nodeId)
        EndNetworkAdmin();

    return true;
}


template<class Serial> void SerialAdmin<Serial>::EndNetworkAdmin(bool trySending)
{
    const NetworkAdminFunction admin = _networkAdminFunc;
    const bool isWakeup = isWakeupAdmin;

    _networkAdminFunc = NetworkAdminFunction::NoAdmin;
    _networkManagement = false;
    isWakeupAdmin = false;

    _out.printInfo("End network admin");

    if (trySending)
    {
        if (admin == NetworkAdminFunction::AddNode)
            serial->tryToSend(0, isWakeup, true);
        else
            serial->tryToSend(0, false);
    }

    SetStageTime();
}



template<class Serial> void SerialAdmin<Serial>::StartWaitingThread()
{
    serial->_bl->threadManager.join(waitThread);
    serial->_bl->threadManager.start(waitThread, true, &SerialAdmin::waitForTimeoutThread, this);
}

template<class Serial> void SerialAdmin<Serial>::waitForTimeoutThread()
{
    if (!admFinishedEvent.WaitSeconds(60))
        // was a timeout
        EndNetworkAdmin();
}


}
