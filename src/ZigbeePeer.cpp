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

#include "ZigbeePeer.h"

#include "GD.h"
#include "ZigbeePacket.h"
#include "ZigbeeCentral.h"
#include "ZigbeeDevicesDescription.h"

#include <homegear-base/Variable.h>

#include <cassert>
#include <iomanip>
#include <memory>

namespace Zigbee
{

std::shared_ptr<BaseLib::Systems::ICentral> ZigbeePeer::getCentral()
{
    try
    {
        if(_central)
            return _central;
        _central = GD::family->getCentral();
        return _central;
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return std::shared_ptr<BaseLib::Systems::ICentral>();
}

ZigbeePeer::ZigbeePeer(uint32_t parentID, IPeerEventSink* eventHandler)
    : BaseLib::Systems::Peer(GD::bl, parentID, eventHandler),
      IEEEaddr(0), endPoint(0),
      getPacketsSent(false), config_pending(true), errorOrTimeoutOccured(false), metadataRefreshed(false), _shuttingDown(false), lastClusterAndAttr(0xFF, 0xFF)
{
    init();
}

ZigbeePeer::ZigbeePeer(int32_t id, std::string serialNumber, uint32_t parentID, IPeerEventSink* eventHandler)
    : BaseLib::Systems::Peer(GD::bl, id, -1, serialNumber, parentID, eventHandler),
      IEEEaddr(0), endPoint(0),
      getPacketsSent(false), config_pending(true), errorOrTimeoutOccured(false), metadataRefreshed(false), _shuttingDown(false), lastClusterAndAttr(0xFF, 0xFF)
{
    init();
}

ZigbeePeer::~ZigbeePeer()
{
    try
    {
        if (getThread.joinable())
            _bl->threadManager.join(getThread);
        dispose();
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void ZigbeePeer::worker()
{
    try
    {
        if(_disposing)
            return;

        std::lock_guard<std::mutex> lock(serviceMessagesMutex);
        if(serviceMessages && _rpcDevice)
            serviceMessages->checkUnreach(_rpcDevice->timeout, getLastPacketReceived());
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}


void ZigbeePeer::memcpyLittleEndian(int32_t& to, const std::vector<uint8_t>& from)
{
    try
    {
        to = 0;
        if (from.empty())
            return;

        for (int i = std::min(3, int(from.size() - 1)); i >= 0; --i)
        {
            to |= from[i];
            if (i)
                to <<= 8;
        }
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void ZigbeePeer::memcpyLittleEndian(std::vector<uint8_t>& to, int32_t from)
{
    try
    {
        if (to.empty())
            to.resize(4);

        for (int i = 0; i < std::min(4, int(to.size())); ++i)
        {
            to[i] = static_cast<uint8_t>(from & 0xFF);
            from >>= 8;
        }
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}


void ZigbeePeer::init()
{
    getPacketsSent = false;
    raiseAddWebserverEventHandler(this);
}

void ZigbeePeer::dispose()
{
    if(_disposing)
        return;
    Peer::dispose();
}

void ZigbeePeer::removeHooks()
{
    raiseRemoveWebserverEventHandler();
}

void ZigbeePeer::homegearStarted()
{
    try
    {
        Peer::homegearStarted();
        raiseAddWebserverEventHandler(this);
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void ZigbeePeer::homegearShuttingDown()
{
    try
    {
        _shuttingDown = true;
        Peer::homegearShuttingDown();
        removeHooks();
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

std::string ZigbeePeer::handleCliCommand(std::string command)
{
    try
    {
        std::ostringstream stringStream;

        if(command == "help")
        {
            stringStream << "List of commands:" << std::endl << std::endl;
            stringStream << "For more information about the individual command type: COMMAND help" << std::endl << std::endl;
            stringStream << "unselect\t\tUnselect this peer" << std::endl;
            stringStream << "channel count\t\tPrint the number of channels of this peer" << std::endl;
            stringStream << "config print\t\tPrints all configuration parameters and their values" << std::endl;
            return stringStream.str();
        }
        if(command.compare(0, 13, "channel count") == 0)
        {
            std::stringstream stream(command);
            std::string element;
            int32_t index = 0;
            while(std::getline(stream, element, ' '))
            {
                if(index < 2)
                {
                    index++;
                    continue;
                }
                else if(index == 2)
                {
                    if(element == "help")
                    {
                        stringStream << "Description: This command prints this peer's number of channels." << std::endl;
                        stringStream << "Usage: channel count" << std::endl << std::endl;
                        stringStream << "Parameters:" << std::endl;
                        stringStream << "  There are no parameters." << std::endl;
                        return stringStream.str();
                    }
                }
                index++;
            }

            stringStream << "Peer has " << _rpcDevice->functions.size() << " channels." << std::endl;
            return stringStream.str();
        }
        else if(command.compare(0, 12, "config print") == 0)
        {
            std::stringstream stream(command);
            std::string element;
            int32_t index = 0;
            while(std::getline(stream, element, ' '))
            {
                if(index < 2)
                {
                    index++;
                    continue;
                }
                else if(index == 2)
                {
                    if(element == "help")
                    {
                        stringStream << "Description: This command prints all configuration parameters of this peer. The values are in BidCoS packet format." << std::endl;
                        stringStream << "Usage: config print" << std::endl << std::endl;
                        stringStream << "Parameters:" << std::endl;
                        stringStream << "  There are no parameters." << std::endl;
                        return stringStream.str();
                    }
                }
                index++;
            }

            return printConfig();
        }
        else
            return "Unknown command.\n";
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return "Error executing command. See log file for more details.\n";
}

std::string ZigbeePeer::printConfig()
{
    try
    {
        std::ostringstream stringStream;
        stringStream << "MASTER" << std::endl;
        stringStream << "{" << std::endl;
        for(std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator i = configCentral.begin(); i != configCentral.end(); ++i)
        {
            stringStream << "\t" << "Channel: " << std::dec << i->first << std::endl;
            stringStream << "\t{" << std::endl;
            for(std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator j = i->second.begin(); j != i->second.end(); ++j)
            {
                stringStream << "\t\t[" << j->first << "]: ";
                if(!j->second.rpcParameter)
                    stringStream << "(No RPC parameter) ";

                auto element = j->second.getLogicalData();
                if (!element || element->type != VariableType::tString || element->stringValue.empty())
                {
                    std::vector<uint8_t> data = j->second.getBinaryData();
                    for(std::vector<uint8_t>::const_iterator k = data.begin(); k != data.end(); ++k)
                    {
                        stringStream << std::hex << std::setfill('0') << std::setw(2) << (int32_t)*k << " ";
                    }

                }
                else
                    stringStream << element->stringValue;

                stringStream << std::endl;
            }
            stringStream << "\t}" << std::endl;
        }
        stringStream << "}" << std::endl << std::endl;

        stringStream << "VALUES" << std::endl;
        stringStream << "{" << std::endl;
        for(std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator i = valuesCentral.begin(); i != valuesCentral.end(); ++i)
        {
            stringStream << "\t" << "Channel: " << std::dec << i->first << std::endl;
            stringStream << "\t{" << std::endl;
            for(std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator j = i->second.begin(); j != i->second.end(); ++j)
            {
                stringStream << "\t\t[" << j->first << "]: ";
                if(!j->second.rpcParameter)
                    stringStream << "(No RPC parameter) ";

                auto element = j->second.getLogicalData();
                if (!element || element->type != VariableType::tString || element->stringValue.empty())
                {
                    std::vector<uint8_t> data = j->second.getBinaryData();
                    for(std::vector<uint8_t>::const_iterator k = data.begin(); k != data.end(); ++k)
                    {
                        stringStream << std::hex << std::setfill('0') << std::setw(2) << (int32_t)*k << " ";
                    }
                }
                else
                    stringStream << element->stringValue;

                stringStream << std::endl;
            }
            stringStream << "\t}" << std::endl;
        }
        stringStream << "}" << std::endl << std::endl;

        return stringStream.str();
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return "";
}


std::string ZigbeePeer::getPhysicalInterfaceId()
{
    if(_physicalInterfaceId.empty())
        setPhysicalInterfaceId(GD::interfaces->getDefaultInterface()->getID());
    return _physicalInterfaceId;
}

void ZigbeePeer::setPhysicalInterfaceId(std::string id)
{
    if(id.empty() || GD::interfaces->hasInterface(id))
    {
        _physicalInterfaceId = id;
        setPhysicalInterface(id.empty() ? GD::interfaces->getDefaultInterface() : GD::interfaces->getInterface(_physicalInterfaceId));
        saveVariable(19, _physicalInterfaceId);
    }
    else
    {
        setPhysicalInterface(GD::interfaces->getDefaultInterface());
        saveVariable(19, _physicalInterfaceId);
    }
}

void ZigbeePeer::setPhysicalInterface(std::shared_ptr<IZigbeeInterface> interface)
{
    try
    {
        if(!interface)
            return;
        _physicalInterface = interface;
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

bool ZigbeePeer::LoadVector(std::vector<uint16_t>& vect, std::shared_ptr<std::vector<char>> binaryValue)
{
    for (std::vector<char>::iterator it = binaryValue->begin(); it != binaryValue->end(); ++it)
    {
        uint16_t val = *it;
        val <<= 8;
        ++it;
        if (it == binaryValue->end())
            return false;
        val |= *it;
        vect.emplace_back(val);
    }

    return true;
}


void ZigbeePeer::loadVariables(BaseLib::Systems::ICentral* central, std::shared_ptr<BaseLib::Database::DataTable>& rows)
{
    try
    {
        if(!rows)
            rows = _bl->db->getPeerVariables(_peerID);

        Peer::loadVariables(central, rows);

        std::lock_guard<std::mutex> serviceLock(infoMutex);
        nodeInfo.EndPoints.clear();
        nodeInfo.EndPointsIds.clear();

        uint16_t curCluster = 0;

        BaseLib::Database::DataTable dataTableDetails;

        // the following values can come in any order
        for(BaseLib::Database::DataTable::iterator row = rows->begin(); row != rows->end(); ++row)
        {
            const int rowIndex = row->second.at(2)->intValue;

            if (rowIndex <= 35)
            {
                switch(rowIndex)
                {
                case 19:
                    _physicalInterfaceId = row->second.at(4)->textValue;
                    if(GD::interfaces->hasInterface(_physicalInterfaceId))
                        setPhysicalInterface(GD::interfaces->getInterface(_physicalInterfaceId));
                    break;
                case 20:
                    nodeInfo.srcAddr = row->second.at(3)->intValue;
                    break;
                case 21:
                    IEEEaddr = nodeInfo.srcIEEEAddr = row->second.at(3)->intValue;
                    break;
                case 22:
                    nodeInfo.capabilities = row->second.at(3)->intValue;
                    break;
                case 23:
                    nodeInfo.LogicalType_ComplexDescriptorAvailable_UserDescriptorAvailable = row->second.at(3)->intValue;
                    break;
                case 24:
                    nodeInfo.APSFlags_FrequencyBand = row->second.at(3)->intValue;
                    break;
                case 25:
                    nodeInfo.MacCapabilitiesFlags = row->second.at(3)->intValue;
                    break;
                case 26:
                    nodeInfo.ManufacturerCode = row->second.at(3)->intValue;
                    break;
                case 27:
                    nodeInfo.MaxBufferSize = row->second.at(3)->intValue;
                    break;
                case 28:
                    nodeInfo.MaxInTransferSize = row->second.at(3)->intValue;
                    break;
                case 29:
                    nodeInfo.ServerMask = row->second.at(3)->intValue;
                    break;
                case 30:
                    nodeInfo.MaxOutTransferSize = row->second.at(3)->intValue;
                    break;
                case 31:
                    nodeInfo.DescriptorCapabilities = row->second.at(3)->intValue;
                    break;
                case 32:
                    nodeInfo.CurrentPowerMode_AvailablePowerSources = row->second.at(3)->intValue;
                    break;
                case 33:
                    nodeInfo.CurrentPowerSource_CurrentPowerSourceLevel = row->second.at(3)->intValue;
                    break;
                case 34:
                    getPacketsSent = row->second.at(3)->intValue;
                    break;
                case 35:
                    metadataRefreshed = (bool)row->second.at(3)->intValue;
                    break;
                default:
                    break;
                }
            }
            else
            {
                // need ordering by 'rowIndex'
                if (rowIndex < 1000)
                    dataTableDetails[rowIndex] = row->second;
            }
        }

        // now deal with values that must come in a particular order

        // this is in the proper order now
        for(BaseLib::Database::DataTable::iterator row = dataTableDetails.begin(); row != dataTableDetails.end(); ++row)
        {
            const int rowIndex = row->second.at(2)->intValue;

            if (rowIndex <= 61)
            {
                switch(rowIndex)
                {
                case 50:
                    nodeInfo.EndPointsIds.resize(1);
                    endPoint = nodeInfo.EndPointsIds[0] = row->second.at(3)->intValue;
                    nodeInfo.EndPoints[endPoint].Endpoint = endPoint;
                    nodeInfo.EndPoints[endPoint].ServersClustersInfoMap.clear();
                    break;
                case 51:
                    nodeInfo.EndPoints[endPoint].ProfileId = row->second.at(3)->intValue;
                    break;
                case 52:
                    nodeInfo.EndPoints[endPoint].DeviceId = row->second.at(3)->intValue;
                    break;
                case 53:
                    nodeInfo.EndPoints[endPoint].DeviceVersion = row->second.at(3)->intValue;
                    break;
                case 54:
                    nodeInfo.EndPoints[endPoint].InClusterList.clear();
                    LoadVector(nodeInfo.EndPoints[endPoint].InClusterList, row->second.at(5)->binaryValue);
                    break;
                case 55:
                    nodeInfo.EndPoints[endPoint].OutClusterList.clear();
                    LoadVector(nodeInfo.EndPoints[endPoint].OutClusterList, row->second.at(5)->binaryValue);
                    nodeInfo.EndPoints[endPoint].FillAllClustersList();
                    break;
                case 56:
                    nodeInfo.EndPoints[endPoint].modelIdentifier = row->second.at(4)->textValue;
                    break;
                case 57:
                    nodeInfo.SupportsDiscoverAttrsExtended = (bool)row->second.at(3)->intValue;
                    break;
                case 58:
                    nodeInfo.SupportsDiscoverAttrsSimple = (bool)row->second.at(3)->intValue;
                    break;
                case 59:
                    nodeInfo.SupportsDiscoverCommandsReceived = (bool)row->second.at(3)->intValue;
                    break;
                case 60:
                    nodeInfo.SupportsDiscoverCommandsSent = (bool)row->second.at(3)->intValue;
                    break;
                case 61:
                    config_pending = (bool)row->second.at(3)->intValue;
                    break;
                default:
                    break;
                }
            }
            else
            {
                const int clusterInfoIndex = rowIndex - 62;

                switch(clusterInfoIndex % 5)
                {
                case 0:
                    curCluster = row->second.at(3)->intValue;
                    nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[curCluster].attrsList.clear();
                    nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[curCluster].attrsMap.clear();
                    nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[curCluster].commandsReceived.clear();
                    nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[curCluster].commandsSent.clear();

                    //GD::out.printInfo("Loading cluster: 0x" + BaseLib::HelperFunctions::getHexString(curCluster));
                    break;
                case 1:
                    nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[curCluster].attrsList.clear();
                    LoadVector(nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[curCluster].attrsList, row->second.at(5)->binaryValue);
                    break;
                case 2:
                {
                    nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[curCluster].attrsMap.clear();
                    std::vector<char>::iterator end = row->second.at(5)->binaryValue->end();
                    for (std::vector<char>::iterator it = row->second.at(5)->binaryValue->begin(); it != end; ++it)
                    {
                        uint16_t attrId = *it;
                        attrId <<= 8;
                        ++it;
                        if (it == end)
                            break;
                        attrId |= *it;
                        nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[curCluster].attrsMap[attrId].attrId = attrId;

                        ++it;
                        if (it == end)
                            break;
                        nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[curCluster].attrsMap[attrId].type = *it;

                        ++it;
                        if (it == end)
                            break;
                        nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[curCluster].attrsMap[attrId].accessControl = *it;
                    }
                }
                break;
                case 3:
                    nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[curCluster].commandsReceived.clear();
                    for (std::vector<char>::iterator it = row->second.at(5)->binaryValue->begin(); it != row->second.at(5)->binaryValue->end(); ++it)
                        nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[curCluster].commandsReceived.insert(*it);
                    break;
                case 4:
                    nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[curCluster].commandsSent.clear();
                    for (std::vector<char>::iterator it = row->second.at(5)->binaryValue->begin(); it != row->second.at(5)->binaryValue->end(); ++it)
                        nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[curCluster].commandsSent.insert(*it);
                    break;
                }
            }
        }

        if(!_physicalInterface)
            _physicalInterface = GD::interfaces->getDefaultInterface();

        uint32_t epPart = endPoint;
        const uint32_t fakeAddr = (epPart << 16) | nodeInfo.srcAddr;
        setAddress(fakeAddr);

        _deviceType = GD::family->createDeviceForNode(nodeInfo, endPoint);

        _firmwareVersion = 0;
        _rpcDevice = GD::family->getRpcDevices()->find(_deviceType, _firmwareVersion, -1);
        if(!_rpcDevice)
        {
            GD::out.printDebug("Could not find the rpc device for peer: " + std::to_string(_peerID));
            return;
        }

        //initializeServiceVariables();
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

bool ZigbeePeer::load(BaseLib::Systems::ICentral* central)
{
    try
    {
        std::shared_ptr<BaseLib::Database::DataTable> rows;

        loadVariables(central, rows);
        if(!_rpcDevice)
        {
            GD::out.printError("Error loading peer " + std::to_string(_peerID) + ": Device type not found: 0x" + BaseLib::HelperFunctions::getHexString(_deviceType) + " Firmware version: " + std::to_string(_firmwareVersion));
            return false;
        }

        initializeTypeString();

        loadConfig();
        initializeCentralConfig();

        {
            std::lock_guard<std::mutex> lock(serviceMessagesMutex);
            serviceMessages = std::make_shared<BaseLib::Systems::ServiceMessages>(_bl, _peerID, _serialNumber, this);
            serviceMessages->load();
        }

        std::lock_guard<std::mutex> serviceLock(infoMutex);
        initializeServiceVariables();

        return true;
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return false;
}


void ZigbeePeer::RefreshDeviceDescription()
{
    try
    {
        if(_disposing)
            return;
        std::shared_ptr<ZigbeeCentral> central = std::dynamic_pointer_cast<ZigbeeCentral>(getCentral());
        if (!central)
            return;

        if(GD::bl->debugLevel >= 4)
            GD::out.printInfo("Refresh device description for peer: " + std::to_string(_peerID));

        std::shared_ptr<ZigbeePeer> peer = central->getPeer(_peerID); // save it here, to be added back

        if (!central->RemovePeerFromArrays(_peerID))
            return;
        if (!central->WaitForPeer(peer, 2)) // use_count is 2 because here there is a smart pointer used and one is used by central's onPacketReceived
        {
            central->AddPeerIntoArrays(peer); // add it back, it can be useful even without refreshing
            return;
        }

        deleting = true;

        auto peerParameters = _bl->db->getPeerParameters(_peerID);

        for (auto row = peerParameters->begin(); row != peerParameters->end(); ++row)
        {
            BaseLib::Database::DataRow data;
            data.push_back(std::make_shared<BaseLib::Database::DataColumn>(_peerID));
            data.push_back(std::make_shared<BaseLib::Database::DataColumn>(row->second.at(0)->intValue));
            _bl->db->deletePeerParameter(_peerID, data);
        }

        //deleteFromDatabase();

        configCentral.clear();
        valuesCentral.clear();
        _rpcDevice.reset();

        deleting = false;

        {
            std::lock_guard<std::mutex> infoLock(infoMutex);

            // TODO: delete the old _deviceType from data structures?
            const uint32_t _oldDeviceType = _deviceType;
            const uint32_t _oldVersion = _firmwareVersion;

            _deviceType = GD::family->createDeviceForNode(nodeInfo, endPoint);

            std::shared_ptr<ZigbeeDevicesDescription> devices = std::dynamic_pointer_cast<ZigbeeDevicesDescription>(GD::family->getRpcDevices());

            if (devices)
            {
                setRpcDevice(devices->find(_deviceType, _firmwareVersion, -1));
                if (_oldDeviceType)
                    devices->RemoveDeviceType(_oldDeviceType, _oldVersion);
            }

            if(!_rpcDevice)
            {
                central->AddPeerIntoArrays(peer); // add it as it is, might help with diagnosing
                return;
            }
        }

        initializeCentralConfig();

        {
            std::lock_guard<std::mutex> infoLock(infoMutex);

            if(GD::bl->debugLevel >= 4)
                GD::out.printInfo("Initialize service variables for peer: " + std::to_string(_peerID));

            initializeServiceVariables();
        }

        central->AddPeerIntoArrays(peer);

        save(true, true, false);

        if(GD::bl->debugLevel >= 4)
            GD::out.printInfo("Refreshed device for peer: " + std::to_string(_peerID));

        //Todo: Call RPCUpdateDevice here as the peer was only updated
        raiseRPCUpdateDevice(_peerID, 0, _serialNumber + ":" + std::to_string(0), 0);
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}


void ZigbeePeer::CheckAddOptionalMandatory()
{
    try
    {
        std::shared_ptr<ClustersInfo> xmlClustersInfo;
        if (GD::family)
            xmlClustersInfo = GD::family->_clustersInfo;
        if (!xmlClustersInfo)
            return;

        std::lock(infoMutex, valuesMutex);
        std::lock_guard<std::mutex> infoLock(infoMutex, std::adopt_lock);
        std::lock_guard<std::mutex> valuesLock(valuesMutex, std::adopt_lock);

        for (uint16_t clusterIdd : nodeInfo.EndPoints[endPoint].AllClusterList)
        {
            if (nodeInfo.EndPoints[endPoint].ServersClustersInfoMap.find(clusterIdd) == nodeInfo.EndPoints[endPoint].ServersClustersInfoMap.end())
                continue;

            std::map<uint16_t, ClustersInfo::ClusterInfoExt>::iterator clusterXmlIt = xmlClustersInfo->serversClustersInfoMap.find(clusterIdd);
            if (clusterXmlIt == xmlClustersInfo->serversClustersInfoMap.end())
                continue;

            // now check each attribute and command in this cluster

            // first, attributes:

            for (auto& attrPair : clusterXmlIt->second.attrsMap)
            {
                if (!attrPair.second.mandatoryAttribute.empty())
                {
                    const std::string mandatoryAttrName = clusterXmlIt->second.name + "." + attrPair.second.mandatoryAttribute;

                    if (valuesMap.find(mandatoryAttrName) == valuesMap.end())
                        continue;

                    PVariable pvalue = valuesMap[mandatoryAttrName].first;
                    if (!pvalue)
                    {
                        GD::out.printDebug("packetReceived: No value obtained for dependent mandatory attribute: " + mandatoryAttrName);
                        continue;
                    }

                    if (pvalue->type != BaseLib::VariableType::tInteger && pvalue->type != BaseLib::VariableType::tInteger64)
                    {
                        GD::out.printDebug("packetReceived: Dependent mandatory attribute value not integer for: " + mandatoryAttrName);
                        continue;
                    }

                    uint64_t val = (pvalue->type == BaseLib::VariableType::tInteger ? uint64_t(pvalue->integerValue) : uint64_t(pvalue->integerValue64));
                    if (attrPair.second.mandatory_if_equal)
                    {
                        // mandatory bits is actually bits
                        for (const char& c : attrPair.second.mandatoryBits)
                        {
                            uint8_t shift = c - '0';
                            val >>= shift;
                            if (val & 0x1)
                            {
                                // it is mandatory!
                                // the attribute is mandatory, check if it exists
                                const std::string depMandatoryAttrName = clusterXmlIt->second.name + "." + attrPair.second.name;

                                // first, in values map
                                if (valuesMap.find(depMandatoryAttrName) == valuesMap.end())
                                {
                                    // if not found, try to find it in the attrs map in nodeInfo, if it's not there, log a debug message
                                    const uint16_t attrIdd = attrPair.second.attrId;
                                    if (nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterIdd].attrsMap.find(attrIdd) == nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterIdd].attrsMap.end())
                                    {
                                        GD::out.printDebug("packetReceived: Mandatory attribute not found, adding: " + depMandatoryAttrName);
                                        nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterIdd].attrsMap[attrIdd].attrId = attrIdd;
                                        nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterIdd].attrsMap[attrIdd].type = attrPair.second.type;
                                        nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterIdd].attrsMap[attrIdd].accessControl = attrPair.second.accessControl;
                                        nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterIdd].attrsList.emplace_back(attrIdd);
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        // mandatory bits is actually a value
                        const uint64_t valToCompare = std::stoul(attrPair.second.mandatoryBits);
                        if (val > valToCompare)
                        {
                            // the attribute is mandatory, check if it exists
                            const std::string depMandatoryAttrName = clusterXmlIt->second.name + "." + attrPair.second.name;

                            // first, in values map
                            if (valuesMap.find(depMandatoryAttrName) == valuesMap.end())
                            {
                                // if not found, try to find it in the attrs map in nodeInfo, if it's not there, log a debug message
                                const uint16_t attrIdd = attrPair.second.attrId;
                                if (nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterIdd].attrsMap.find(attrIdd) == nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterIdd].attrsMap.end())
                                {
                                    GD::out.printDebug("packetReceived: Mandatory attribute not found, adding: " + depMandatoryAttrName);
                                    nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterIdd].attrsMap[attrIdd].attrId = attrIdd;
                                    nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterIdd].attrsMap[attrIdd].type = attrPair.second.type;
                                    nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterIdd].attrsMap[attrIdd].accessControl = attrPair.second.accessControl;
                                    nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterIdd].attrsList.emplace_back(attrIdd);
                                }
                            }
                        }
                    }
                }
            }

            // then, commands:

            for (auto& cmdPair : clusterXmlIt->second.commandsReceived)
            {
                if (!cmdPair.second.mandatoryAttribute.empty())
                {
                    const std::string mandatoryAttrName = clusterXmlIt->second.name + "." + cmdPair.second.mandatoryAttribute;

                    if (valuesMap.find(mandatoryAttrName) == valuesMap.end())
                        continue;

                    PVariable pvalue = valuesMap[mandatoryAttrName].first;
                    if (!pvalue)
                    {
                        GD::out.printDebug("packetReceived: No value obtained for dependent mandatory attribute: " + mandatoryAttrName);
                        continue;
                    }

                    if (pvalue->type != BaseLib::VariableType::tInteger && pvalue->type != BaseLib::VariableType::tInteger64)
                    {
                        GD::out.printDebug("packetReceived: Dependent mandatory attribute value not integer: " + mandatoryAttrName);
                        continue;
                    }

                    uint64_t val = (pvalue->type == BaseLib::VariableType::tInteger ? uint64_t(pvalue->integerValue) : uint64_t(pvalue->integerValue64));
                    if (cmdPair.second.mandatory_if_equal)
                    {
                        // mandatory bits is actually bits
                        for (const char& c : cmdPair.second.mandatoryBits)
                        {
                            uint8_t shift = c - '0';
                            val >>= shift;
                            if (val & 0x1)
                            {
                                // it is mandatory!
                                const uint8_t cmdId = cmdPair.second.key;
                                if (nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterIdd].commandsReceived.find(cmdId) == nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterIdd].commandsReceived.end())
                                {
                                    GD::out.printInfo("packetReceived: Adding mandatory received command 0x" + BaseLib::HelperFunctions::getHexString(cmdId) + " in cluster 0x" + BaseLib::HelperFunctions::getHexString(clusterIdd));
                                    nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterIdd].commandsReceived.insert(cmdId);
                                }
                            }
                        }
                    }
                    else
                    {
                        // mandatory bits is actually a value
                        const uint64_t valToCompare = std::stoul(cmdPair.second.mandatoryBits);
                        if (val > valToCompare)
                        {
                            // the command is mandatory, check if it exists
                            const uint8_t cmdId = cmdPair.second.key;
                            GD::out.printInfo("packetReceived: Adding mandatory received command 0x" + BaseLib::HelperFunctions::getHexString(cmdId) + " in cluster 0x" + BaseLib::HelperFunctions::getHexString(clusterIdd));
                            nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterIdd].commandsReceived.insert(cmdId);
                        }
                    }
                }
            }

            for (auto& cmdPair : clusterXmlIt->second.commandsSent)
            {
                if (!cmdPair.second.mandatoryAttribute.empty())
                {
                    const std::string mandatoryAttrName = clusterXmlIt->second.name + "." + cmdPair.second.mandatoryAttribute;

                    if (valuesMap.find(mandatoryAttrName) == valuesMap.end())
                        continue;

                    PVariable pvalue = valuesMap[mandatoryAttrName].first;
                    if (!pvalue)
                    {
                        GD::out.printDebug("packetReceived: No value obtained for dependent mandatory attribute");
                        continue;
                    }

                    if (pvalue->type != BaseLib::VariableType::tInteger && pvalue->type != BaseLib::VariableType::tInteger64)
                    {
                        GD::out.printDebug("packetReceived: Dependent mandatory attribute value not integer");
                        continue;
                    }

                    uint64_t val = (pvalue->type == BaseLib::VariableType::tInteger ? uint64_t(pvalue->integerValue) : uint64_t(pvalue->integerValue64));
                    if (cmdPair.second.mandatory_if_equal)
                    {
                        // mandatory bits is actually bits
                        for (const char& c : cmdPair.second.mandatoryBits)
                        {
                            uint8_t shift = c - '0';
                            val >>= shift;
                            if (val & 0x1)
                            {
                                // it is mandatory!
                                const uint8_t cmdId = cmdPair.second.key;
                                if (nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterIdd].commandsSent.find(cmdId) == nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterIdd].commandsSent.end())
                                {
                                    GD::out.printInfo("packetReceived: Adding mandatory sent command 0x" + BaseLib::HelperFunctions::getHexString(cmdId) + " in cluster 0x" + BaseLib::HelperFunctions::getHexString(clusterIdd));
                                    nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterIdd].commandsSent.insert(cmdId);
                                }
                            }
                        }
                    }
                    else
                    {
                        // mandatory bits is actually a value
                        const uint64_t valToCompare = std::stoul(cmdPair.second.mandatoryBits);
                        if (val > valToCompare)
                        {
                            // the command is mandatory, check if it exists
                            const uint8_t cmdId = cmdPair.second.key;
                            GD::out.printInfo("packetReceived: Adding mandatory sent command 0x" + BaseLib::HelperFunctions::getHexString(cmdId) + " in cluster 0x" + BaseLib::HelperFunctions::getHexString(clusterIdd));
                            nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterIdd].commandsSent.insert(cmdId);
                        }
                    }
                }
            }
        }
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}


void ZigbeePeer::saveVariables()
{
    try
    {
        if(_peerID == 0)
            return;
        Peer::saveVariables();

        saveVariable(19, _physicalInterfaceId);

        std::lock_guard<std::mutex> serviceLock(infoMutex);

        saveVariable(20, nodeInfo.srcAddr);
        saveVariable(21, (int64_t)nodeInfo.srcIEEEAddr);
        saveVariable(22, nodeInfo.capabilities);
        saveVariable(23, nodeInfo.LogicalType_ComplexDescriptorAvailable_UserDescriptorAvailable);
        saveVariable(24, nodeInfo.APSFlags_FrequencyBand);
        saveVariable(25, nodeInfo.MacCapabilitiesFlags);
        saveVariable(26, nodeInfo.ManufacturerCode);
        saveVariable(27, nodeInfo.MaxBufferSize);
        saveVariable(28, nodeInfo.MaxInTransferSize);
        saveVariable(29, nodeInfo.ServerMask);
        saveVariable(30, nodeInfo.MaxOutTransferSize);
        saveVariable(31, nodeInfo.DescriptorCapabilities);
        saveVariable(32, nodeInfo.CurrentPowerMode_AvailablePowerSources);
        saveVariable(33, nodeInfo.CurrentPowerSource_CurrentPowerSourceLevel);
        saveVariable(34, getPacketsSent);
        saveVariable(35, metadataRefreshed);

        saveVariable(50, endPoint);
        saveVariable(51, nodeInfo.EndPoints[endPoint].ProfileId);
        saveVariable(52, nodeInfo.EndPoints[endPoint].DeviceId);
        saveVariable(53, nodeInfo.EndPoints[endPoint].DeviceVersion);

        std::vector<uint8_t> binary;

        for (uint16_t clid : nodeInfo.EndPoints[endPoint].InClusterList)
        {
            binary.emplace_back((clid & 0xff00) >> 8);
            binary.emplace_back(clid & 0xff);
        }
        saveVariable(54, binary);

        binary.clear();
        for (uint16_t clid : nodeInfo.EndPoints[endPoint].OutClusterList)
        {
            binary.emplace_back((clid & 0xff00) >> 8);
            binary.emplace_back(clid & 0xff);
        }
        saveVariable(55, binary);

        saveVariable(56, nodeInfo.EndPoints[endPoint].modelIdentifier);

        saveVariable(57, nodeInfo.SupportsDiscoverAttrsExtended);
        saveVariable(58, nodeInfo.SupportsDiscoverAttrsSimple);
        saveVariable(59, nodeInfo.SupportsDiscoverCommandsReceived);
        saveVariable(60, nodeInfo.SupportsDiscoverCommandsSent);

        saveVariable(61, (int32_t)config_pending);

        int index = 62;

        for (uint16_t curCluster : nodeInfo.EndPoints[endPoint].AllClusterList)
        {
            saveVariable(index++, curCluster);

            binary.clear();

            for (uint16_t attrId : nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[curCluster].attrsList)
            {
                binary.emplace_back((attrId & 0xFF00) >> 8);
                binary.emplace_back(attrId & 0xFF);
            }

            saveVariable(index++, binary);

            binary.clear();
            for (uint16_t attrId : nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[curCluster].attrsList)
            {
                binary.emplace_back((attrId & 0xFF00) >> 8);
                binary.emplace_back(attrId & 0xFF);
                binary.emplace_back(nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[curCluster].attrsMap[attrId].type);
                binary.emplace_back(nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[curCluster].attrsMap[attrId].accessControl);
            }

            saveVariable(index++, binary);

            binary.clear();
            for (uint8_t cmd : nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[curCluster].commandsReceived)
                binary.emplace_back(cmd);
            saveVariable(index++, binary);

            binary.clear();
            for (uint8_t cmd : nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[curCluster].commandsSent)
                binary.emplace_back(cmd);
            saveVariable(index++, binary);
        }
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

PParameterGroup ZigbeePeer::getParameterSet(int32_t channel, ParameterGroup::Type::Enum type)
{
    try
    {
        PFunction rpcChannel = _rpcDevice->functions.at(channel);
        if(type == ParameterGroup::Type::Enum::variables)
            return rpcChannel->variables;
        else if(type == ParameterGroup::Type::Enum::config)
            return rpcChannel->configParameters;
        else if(type == ParameterGroup::Type::Enum::link)
            return rpcChannel->linkParameters;
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return PParameterGroup();
}



PVariable ZigbeePeer::getDeviceInfo(BaseLib::PRpcClientInfo clientInfo, std::map<std::string, bool> fields)
{
    try
    {
        PVariable info(Peer::getDeviceInfo(clientInfo, fields));
        return info;
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return PVariable();
}


void ZigbeePeer::getValuesFromPacket(PZigbeePacket packet, std::vector<FrameValues>& frameValues)
{
    try
    {
        if(!_rpcDevice)
            return;

        const uint32_t packetType = packet->getType();

        GD::out.printInfo("getValuesFromPacket, packet type: 0x" + BaseLib::HelperFunctions::getHexString(packetType));

        //GD::out.printInfo("getValuesFromPacket, there are: " + std::to_string(_rpcDevice->packetsByMessageType.size()) + " packets in the map");

        //for (PacketsByMessageType::iterator i =  _rpcDevice->packetsByMessageType.begin(); i !=  _rpcDevice->packetsByMessageType.end(); ++i)
        //    GD::out.printInfo("getValuesFromPacket, packet type in multimap: 0x" + BaseLib::HelperFunctions::getHexString(i->first));

        std::pair<BaseLib::DeviceDescription::PacketsByMessageType::iterator, BaseLib::DeviceDescription::PacketsByMessageType::iterator> range = _rpcDevice->packetsByMessageType.equal_range(packetType);

        for (BaseLib::DeviceDescription::PacketsByMessageType::iterator i = range.first; i != range.second; ++i)
        {
            //GD::out.printInfo("getValuesFromPacket, checking a frame");

            const std::shared_ptr<ZigbeeDescPacket> tframe = std::dynamic_pointer_cast<ZigbeeDescPacket>(i->second);
            if (tframe)
            {
                //GD::out.printInfo("getValuesFromPacket, skipping dynamic frame");
                continue; // if it's dynamically added, it's handled later
            }

            //GD::out.printInfo("getValuesFromPacket, found a static frame");

            PPacket frame(i->second);
            if(!frame)
                continue;

            std::vector<uint8_t> erpPacket = packet->getData();
            //if(erpPacket.empty()) break; // might be an empty frame that still contains some zero length fake 'variable'

            const uint32_t erpPacketBitSize = erpPacket.size() * 8;

            int32_t channel = -1;
            if(frame->channel > -1)
                channel = frame->channel;
            if(channel == -1)
                continue;

            FrameValues currentFrameValues;
            currentFrameValues.frameID = frame->id;

            bool abort = false;

            GD::out.printInfo("getValuesFromPacket, found a frame for data (hex): " + BaseLib::HelperFunctions::getHexString(erpPacket));

            //if (frame->binaryPayloads.empty()) GD::out.printInfo("getValuesFromPacket, frame has no binary payloads");

            for(BinaryPayloads::iterator j = frame->binaryPayloads.begin(); j != frame->binaryPayloads.end(); ++j)
            {
                GD::out.printInfo("getValuesFromPacket, checking binary payload, bit index: " + std::to_string((*j)->bitIndex) + " bit size: " + std::to_string((*j)->bitSize) + " constant: " + std::to_string((*j)->constValueInteger));

                std::vector<uint8_t> data;
                if((*j)->bitSize > 0 && (*j)->bitIndex >= 0)
                {
                    if((*j)->bitIndex >= erpPacketBitSize)
                        continue;

                    data = packet->getBitPosition((*j)->bitIndex, (*j)->bitSize);

                    GD::out.printInfo("getValuesFromPacket, parameter: " + (*j)->parameterId + " binary payload (hex): " + BaseLib::HelperFunctions::getHexString(data) + " const value integer: 0x" + BaseLib::HelperFunctions::getHexString((*j)->constValueInteger));

                    if((*j)->constValueInteger > -1)
                    {
                        int32_t intValue = 0;
                        _bl->hf.memcpyBigEndian(intValue, data);
                        //memcpyLittleEndian(intValue, data);

                        if(intValue != (*j)->constValueInteger)
                        {
                            //GD::out.printInfo("getValuesFromPacket, constant does not match");
                            abort = true;
                            break;
                        }
                        else if((*j)->parameterId.empty())
                            continue;
                    }
                }
                else if((*j)->constValueInteger > -1)
                {
                    //memcpyLittleEndian(data, (*j)->constValueInteger);
                    _bl->hf.memcpyBigEndian(data, (*j)->constValueInteger);
                }
                else if(0 == (*j)->bitSize)
                {
                    static const int32_t constVal = 1;
                    //memcpyLittleEndian(data, constVal);
                    _bl->hf.memcpyBigEndian(data, constVal);
                }
                else
                    continue;

                //if (frame->associatedVariables.empty()) GD::out.printInfo("getValuesFromPacket, frame has no associated variables");

                for(std::vector<PParameter>::iterator k = frame->associatedVariables.begin(); k != frame->associatedVariables.end(); ++k)
                {
                    GD::out.printInfo("getValuesFromPacket, checking associated variable: " + (*k)->physical->groupId);

                    if((*k)->physical->groupId != (*j)->parameterId)
                        continue;

                    currentFrameValues.parameterSetType = (*k)->parent()->type();
                    bool setValues = false;
                    if(currentFrameValues.paramsetChannels.empty()) //Fill paramsetChannels
                    {
                        int32_t startChannel = (channel < 0) ? 0 : channel;
                        int32_t endChannel;
                        //When fixedChannel is -2 (means '*') cycle through all channels
                        if(frame->channel == -2)
                        {
                            startChannel = 0;
                            endChannel = _rpcDevice->functions.rbegin()->first;
                        }
                        else
                            endChannel = startChannel;
                        for(int32_t l = startChannel; l <= endChannel; ++l)
                        {
                            Functions::iterator functionIterator = _rpcDevice->functions.find(l);
                            if(functionIterator == _rpcDevice->functions.end())
                                continue;
                            PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(currentFrameValues.parameterSetType);
                            if(!parameterGroup || parameterGroup->parameters.find((*k)->id) == parameterGroup->parameters.end())
                                continue;
                            currentFrameValues.paramsetChannels.push_back(l);

                            currentFrameValues.values[(*k)->id].channels.push_back(l);
                            setValues = true;
                        }
                    }
                    else //Use paramsetChannels
                    {
                        for(std::list<uint32_t>::const_iterator l = currentFrameValues.paramsetChannels.begin(); l != currentFrameValues.paramsetChannels.end(); ++l)
                        {
                            Functions::iterator functionIterator = _rpcDevice->functions.find(*l);
                            if(functionIterator == _rpcDevice->functions.end())
                                continue;
                            PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(currentFrameValues.parameterSetType);
                            if(!parameterGroup || parameterGroup->parameters.find((*k)->id) == parameterGroup->parameters.end())
                                continue;

                            currentFrameValues.values[(*k)->id].channels.push_back(*l);
                            setValues = true;
                        }
                    }

                    if(setValues)
                        currentFrameValues.values[(*k)->id].value.insert(currentFrameValues.values[(*k)->id].value.end(), data.begin(), data.end());
                }
            }

            if(abort)
                continue;
            if(!currentFrameValues.values.empty())
                frameValues.push_back(currentFrameValues);
        }
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}


void ZigbeePeer::Notify(int channel, std::string valueKey, PVariable value)
{
    std::unique_lock<std::mutex> accessLock(_getValueFromDeviceInfo.accessMutex);
    if(_getValueFromDeviceInfo.requested && channel == _getValueFromDeviceInfo.channel && valueKey == _getValueFromDeviceInfo.variableName)
    {
        _getValueFromDeviceInfo.requested = false;
        _getValueFromDeviceInfo.value = value;
        accessLock.unlock();

        {
            std::lock_guard<std::mutex> conditionLock(_getValueFromDeviceInfo.conditionMutex);
            _getValueFromDeviceInfo.mutexReady = true;
        }

        if(GD::bl->debugLevel >= 4)
            GD::out.printInfo("Notifying about: " + _getValueFromDeviceInfo.variableName);

        _getValueFromDeviceInfo.conditionVariable.notify_all();
    }
}



void ZigbeePeer::NotifyTimeout(PZigbeePacket packet)
{
    // a timeout while config pending
    if (config_pending)
    {
        errorOrTimeoutOccured = true;
        if(GD::bl->debugLevel >= 4)
            GD::out.printInfo("Peer: received a timeout notification while config is pending");

        if (metadataRefreshed)
            return;

        std::vector<uint8_t> zclData = packet->getZCLData();

        ZigbeeCommands::ZCLFrame zclFrame;
        // should decode, should be globla and should be a 'read attr'
        if (zclFrame.Decode(zclData) && zclFrame.IsGlobal() && 0x0 == zclFrame.cmd && zclFrame.payload.size() >= 2)
        {
            const uint16_t clusterId = packet->getCluster();
            const uint16_t attrId = ZigbeeCommands::MTCmd::ReadUint16(0, zclFrame.payload);

            bool lastPacket = false;
            {
                std::lock_guard<std::mutex> lock(valuesMutex);
                if (lastClusterAndAttr == std::make_pair(clusterId, attrId))
                    lastPacket = true;
            }

            if (lastPacket)
            {
                CheckAddOptionalMandatory();
                RefreshDeviceDescription();

                metadataRefreshed = true;

                SetValuesFromValuesMap();

                {
                    std::lock_guard<std::mutex> serviceLock(infoMutex);
                    initializeServiceVariables();
                }

                saveVariables();

                ConfigInfo configInfo;
                LoadConfigFile(configInfo);
                // now set attributes reporting
                SendReportConfigPackets(configInfo);

                SendAttrSetPackes(configInfo);

                SendReportConfigGetPackets();
            }
        }
    }
}


void ZigbeePeer::NotifyError(PZigbeePacket packet, const ZigbeeCommands::ZCLFrame& zclFrame)
{
    if (config_pending)
    {
        if(GD::bl->debugLevel >= 4)
            GD::out.printInfo("Peer: received an error notification while config is pending");

        if (metadataRefreshed)
            return;

        std::vector<uint8_t> zclData = packet->getZCLData();

        ZigbeeCommands::ZCLFrame zclSentFrame;
        // should decode, should be global and should be a 'read attr'
        if (zclSentFrame.Decode(zclData) && zclSentFrame.IsGlobal() && 0x0 == zclSentFrame.cmd && zclSentFrame.payload.size() >= 2)
        {
            if(GD::bl->debugLevel >= 4)
                GD::out.printInfo("Peer: Error notification is for a 'get attr'");

            const uint16_t clusterId = packet->getCluster();
            const uint16_t attrId = ZigbeeCommands::MTCmd::ReadUint16(0, zclSentFrame.payload);

            if (zclFrame.IsDefaultResponseError(zclSentFrame.cmd))
            {
                const uint8_t errorCode = zclFrame.payload[1];

                // this is added because a test thermostat device describes a cluster as supported
                // but then when asking for a mandatory attr value, it replies with 'not supported'

                if (0x86 == errorCode || 0xC3 == errorCode) // specified attribute does not exist or the cluster is not supported
                {
                    // simply remove from metadata the offending attribute? But maybe it cannot be read, but it could be reported and/or written?
                    //std::unique_lock<std::mutex> serviceLock(infoMutex);
                    std::lock_guard<std::mutex> serviceLock(infoMutex);

                    if (nodeInfo.EndPoints.find(endPoint) != nodeInfo.EndPoints.end())
                    {
                        auto clusterIt = nodeInfo.EndPoints[endPoint].ServersClustersInfoMap.find(clusterId);
                        if (clusterIt != nodeInfo.EndPoints[endPoint].ServersClustersInfoMap.end())
                        {
                            GD::out.printDebug("Debug: Received an error for attribute request, cluster: 0x" + BaseLib::HelperFunctions::getHexString(clusterId) + ", attr id: 0x" + BaseLib::HelperFunctions::getHexString(attrId) + " error code: 0x" + BaseLib::HelperFunctions::getHexString(errorCode) + " removing it...");

                            clusterIt->second.attrsMap.erase(attrId);
                            clusterIt->second.attrsList.erase(std::remove(clusterIt->second.attrsList.begin(), clusterIt->second.attrsList.end(), attrId), clusterIt->second.attrsList.end());

                            // if there are no more attributes, then remove the cluster?
                            if (clusterIt->second.attrsList.empty() && (clusterIt->second.commandsReceived.empty() || 0xC3 == errorCode))
                            {
                                GD::out.printDebug("Debug: It was the last one in the cluster, removing the cluster: 0x" +  BaseLib::HelperFunctions::getHexString(clusterId));

                                nodeInfo.EndPoints[endPoint].ServersClustersInfoMap.erase(clusterId);

                                nodeInfo.EndPoints[endPoint].InClusterList.erase(std::remove(nodeInfo.EndPoints[endPoint].InClusterList.begin(), nodeInfo.EndPoints[endPoint].InClusterList.end(), clusterId), nodeInfo.EndPoints[endPoint].InClusterList.end());
                                nodeInfo.EndPoints[endPoint].AllClusterList.erase(std::remove(nodeInfo.EndPoints[endPoint].AllClusterList.begin(), nodeInfo.EndPoints[endPoint].AllClusterList.end(), clusterId), nodeInfo.EndPoints[endPoint].AllClusterList.end());

                                // maybe I should not refresh it here, wait for the process to end
                                //serviceLock.unlock();
                                //RefreshDeviceDescription();
                            }
                        }
                    }
                }
                else
                    errorOrTimeoutOccured = true;
            }
            else
                errorOrTimeoutOccured = true;

            bool lastPacket = false;
            {
                std::lock_guard<std::mutex> lock(valuesMutex);
                if (lastClusterAndAttr == std::make_pair(clusterId, attrId))
                    lastPacket = true;
            }

            if (lastPacket)
            {
                CheckAddOptionalMandatory();
                RefreshDeviceDescription();

                metadataRefreshed = true;
                if (!errorOrTimeoutOccured)
                    config_pending = false;

                SetValuesFromValuesMap();

                {
                    std::lock_guard<std::mutex> serviceLock(infoMutex);
                    initializeServiceVariables();
                }

                saveVariables();

                ConfigInfo configInfo;
                LoadConfigFile(configInfo);
                // now set attributes reporting
                SendReportConfigPackets(configInfo);

                SendAttrSetPackes(configInfo);

                SendReportConfigGetPackets();
            }
        }
        else
            errorOrTimeoutOccured = true;
    }
}



bool ZigbeePeer::ParsePacketStatic(PZigbeePacket packet, std::map<uint32_t, std::shared_ptr<std::vector<std::string>>>& valueKeys, std::map<uint32_t, std::shared_ptr<std::vector<PVariable>>>& rpcValues, bool fromNode)
{
    try
    {
        if (!packet)
            return false;

        // TODO: set the address!
        uint32_t addr = 0;

        if (fromNode)
            addr = getAddress();

        std::vector<FrameValues> frameValues;
        getValuesFromPacket(packet, frameValues);

        //Loop through all matching frames
        for(std::vector<FrameValues>::iterator frameValIt = frameValues.begin(); frameValIt != frameValues.end(); ++frameValIt)
        {
            PPacket frame;
            if(!frameValIt->frameID.empty())
                frame = _rpcDevice->packetsById.at(frameValIt->frameID);
            if(!frame)
                continue;

            /*
            {
            	std::lock_guard<std::mutex> requestsGuard(_rpcRequestsMutex);
            	auto rpcRequestIterator = _rpcRequests.find(frameValIt->frameID);
            	if(rpcRequestIterator != _rpcRequests.end()) rpcRequestIterator->second->conditionVariable.notify_all();
            	else
            	{
            		rpcRequestIterator = _rpcRequests.find("ANY");
            		if(rpcRequestIterator != _rpcRequests.end()) rpcRequestIterator->second->conditionVariable.notify_all();
            	}
            }
            */

            for(std::map<std::string, FrameValue>::iterator valIt = frameValIt->values.begin(); valIt != frameValIt->values.end(); ++valIt)
            {
                for(std::list<uint32_t>::const_iterator chanIt = frameValIt->paramsetChannels.begin(); chanIt != frameValIt->paramsetChannels.end(); ++chanIt)
                {
                    if(std::find(valIt->second.channels.begin(), valIt->second.channels.end(), *chanIt) == valIt->second.channels.end())
                        continue;

                    BaseLib::Systems::RpcConfigurationParameter& parameter = valuesCentral[*chanIt][valIt->first];

                    // TODO: should I do this? Does not seem right, one might want a notification even if nothing changes, for example if the same button is pressed again
                    //if(parameter.equals(valIt->second.value))
                    //    continue;

                    if(_rpcDevice->functions.find(*chanIt) == _rpcDevice->functions.end())
                    {
                        GD::out.printWarning("Warning: Can't set value of parameter " + valIt->first + " for channel " + std::to_string(*chanIt) + ". Channel not found.");
                        continue;
                    }
                    PParameterGroup parameterSet = getParameterSet(*chanIt, frameValIt->parameterSetType);
                    if(!parameterSet)
                    {
                        GD::out.printWarning("Warning: Can't set value of parameter " + valIt->first + " for channel " + std::to_string(*chanIt) + ". Value parameter set not found.");
                        continue;
                    }
                    PParameter currentParameter = parameterSet->getParameter(valIt->first);
                    if(!currentParameter)
                    {
                        GD::out.printWarning("Warning: Can't set value of parameter " + valIt->first + " for channel " + std::to_string(*chanIt) + ". Value parameter set not found.");
                        continue;
                    }

                    parameter.setBinaryData(valIt->second.value);
                    if (parameter.rpcParameter)
                    {
                        PVariable variable = parameter.rpcParameter->convertFromPacket(valIt->second.value, parameter.mainRole(), true);
                        parameter.setLogicalData(variable);
                    }

                    if(parameter.databaseId > 0)
                        saveParameter(parameter.databaseId, valIt->second.value);
                    else
                        saveParameter(0, ParameterGroup::Type::Enum::variables, *chanIt, valIt->first, valIt->second.value);

                    if(_bl->debugLevel >= 4)
                        GD::out.printInfo("Info: " + valIt->first + " on channel " + std::to_string(*chanIt) + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber  + " was set to 0x" + BaseLib::HelperFunctions::getHexString(valIt->second.value) + ".");

                    if(parameter.rpcParameter)
                    {
                        //Process service messages
                        if(parameter.rpcParameter->service && !valIt->second.value.empty())
                        {
                            if(parameter.rpcParameter->logical->type == ILogical::Type::Enum::tEnum)
                            {
                                serviceMessages->set(valIt->first, valIt->second.value.at(0), *chanIt);
                            }
                            else if(parameter.rpcParameter->logical->type == ILogical::Type::Enum::tBoolean)
                            {
                                serviceMessages->set(valIt->first, (bool)valIt->second.value.at(0));
                            }
                        }

                        AddValueForEvents(valueKeys, rpcValues, *chanIt, valIt->first, parameter.getLogicalData());
                    }

                    if (fromNode)
                    {
                        if (config_pending)
                        {
                            std::lock_guard<std::mutex> lock(valuesMutex);

                            valuesMap[valIt->first] = std::make_pair(parameter.getLogicalData(), valIt->second.value);

                            GD::out.printInfo("Adding value to map: " + valIt->first);
                        }


                        PVariable variable;
                        if (parameter.rpcParameter)
                            variable = parameter.getLogicalData();
                        else
                            variable = std::make_shared<Variable>(valIt->second.value);

                        Notify(*chanIt, valIt->first, variable);
                    }
                }
            }

            if(fromNode && !frame->responseTypeId.empty())
            {
                PacketsById::iterator packetIterator = _rpcDevice->packetsById.find(frame->responseTypeId);
                if(packetIterator == _rpcDevice->packetsById.end())
                    GD::out.printError("Error: Response packet with ID \"" + frame->responseTypeId + "\" not found.");
                else
                {
                    PPacket responseFrame = packetIterator->second;
                    if(responseFrame->subtype == -1)
                        responseFrame->subtype = 1;

                    std::vector<uint8_t> data;

                    PZigbeePacket packetSend = std::make_shared<ZigbeePacket>(data);

                    packetSend->setSenderAddress(_physicalInterface->getAddress());
                    packetSend->setDestinationAddress(addr);
                    //packetSend->expectsResponse = true; // set by the sending code anyway

                    for(BinaryPayloads::iterator i = responseFrame->binaryPayloads.begin(); i != responseFrame->binaryPayloads.end(); ++i)
                    {
                        if((*i)->constValueInteger > -1)
                        {
                            std::vector<uint8_t> dataBin;
                            const unsigned int requiredSize = (*i)->bitSize / 8 + (((*i)->bitSize & 7) != 0 ? 1 : 0);
                            dataBin.resize(requiredSize, 0);
                            _bl->hf.memcpyBigEndian(dataBin, (*i)->constValueInteger);
                            //memcpyLittleEndian(dataBin, (*i)->constValueInteger);
                            packetSend->setBitPosition((*i)->bitIndex, (*i)->bitSize, dataBin);
                            continue;
                        }
                        bool paramFound = false;
                        for(std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator j = valuesCentral[responseFrame->channel].begin(); j != valuesCentral[responseFrame->channel].end(); ++j)
                        {
                            //Only compare id. Till now looking for value_id was not necessary.
                            if((*i)->parameterId == j->second.rpcParameter->physical->groupId)
                            {
                                std::vector<uint8_t> dataBin = j->second.getBinaryData();
                                packetSend->setBitPosition((*i)->bitIndex, (*i)->bitSize, dataBin);
                                paramFound = true;
                                break;
                            }
                        }
                        if(!paramFound)
                            GD::out.printError("Error constructing packet. param \"" + (*i)->parameterId + "\" not found. Peer: " + std::to_string(_peerID) + " Serial number: " + _serialNumber + " Frame: " + responseFrame->id);
                    }

                    _physicalInterface->sendPacket(packetSend);
                }
            }
        }

        return true;
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return false;
}


void ZigbeePeer::SetValuesFromValuesMap()
{
    std::lock_guard<std::mutex> lock(valuesMutex);

    // now put the collected values in the new metadata
    for (auto& val : valuesMap)
    {
        bool found = false;

        for (auto& channel : configCentral)
        {
            auto parameterIterator = channel.second.find(val.first);
            if (parameterIterator != channel.second.end())
            {
                found = true;

                //GD::out.printInfo("Info: Setting value for: " + val.first);

                parameterIterator->second.setBinaryData(val.second.second);

                if(parameterIterator->second.databaseId > 0)
                    saveParameter(parameterIterator->second.databaseId, val.second.second);
                else
                    saveParameter(0, ParameterGroup::Type::Enum::config, channel.first, val.first, val.second.second);

                PVariable pvalue = val.second.first;

                if (parameterIterator->second.rpcParameter && parameterIterator->second.hasRoles())
                    pvalue = parameterIterator->second.rpcParameter->convertFromPacket(val.second.second, parameterIterator->second.mainRole(), true);

                parameterIterator->second.setLogicalData(pvalue);

                break;
            }
        }

        if (!found)
        {
            for (auto& channel : valuesCentral)
            {
                auto parameterIterator = channel.second.find(val.first);
                if (parameterIterator != channel.second.end())
                {
                    found = true;

                    //GD::out.printInfo("Info: Setting value for: " + val.first);
                    parameterIterator->second.setBinaryData(val.second.second);

                    if(parameterIterator->second.databaseId > 0)
                        saveParameter(parameterIterator->second.databaseId, val.second.second);
                    else
                        saveParameter(0, ParameterGroup::Type::Enum::variables, channel.first, val.first, val.second.second);

                    PVariable pvalue = val.second.first;

                    if (parameterIterator->second.rpcParameter && parameterIterator->second.hasRoles())
                        pvalue = parameterIterator->second.rpcParameter->convertFromPacket(val.second.second, parameterIterator->second.mainRole(), true);

                    parameterIterator->second.setLogicalData(pvalue);

                    break;
                }
            }
        }
    }

    valuesMap.clear();
}


bool ZigbeePeer::LoadConfigFile(ConfigInfo& configInfo)
{
    std::string modelType;

    {
        std::unique_lock<std::mutex> serviceLock(infoMutex);
        modelType = nodeInfo.GetModelType(endPoint);
    }

    const std::string xmlConfigFile = GD::bl->settings.deviceDescriptionPath() + std::to_string(ZIGBEE_FAMILY_ID) + "/conf/conf-" + modelType + ".xml";

    if(GD::bl->debugLevel >= 6)
        GD::out.printInfo( "Info: Looking for a configuration file: " + xmlConfigFile);

    std::ifstream in(xmlConfigFile);
    if(!in.is_open())
        return false;

    if(GD::bl->debugLevel >= 6)
        GD::out.printInfo( "Info: Configuration file opened...");

    std::stringstream buffer;
    buffer << in.rdbuf();
    in.close();

    const std::string contents(buffer.str());

    // now parse it
    rapidxml::xml_document doc;    // character type defaults to char

    doc.parse<0>((char*)contents.data());    // 0 means default parse flags

    if(GD::bl->debugLevel >= 6)
        GD::out.printInfo( "Info: Configuration file parsed...");

    for (rapidxml::xml_node *rnode = doc.first_node(); rnode; rnode = rnode->next_sibling())
    {
        if (rnode->type() != rapidxml::node_element)
            continue;

        std::string name = rnode->name();

        if (name == "config_values")
        {
            for (rapidxml::xml_node *node = rnode->first_node(); node; node = node->next_sibling())
            {
                if (node->type() != rapidxml::node_element)
                    continue;

                const std::string nodename = node->name();

                uint16_t clusterId = 0xFFFF;
                uint16_t attrId = 0xFFFF;

                if (nodename == "attr")
                {
                    AttrSetConfig attrConfig;
                    std::string val;

                    for (rapidxml::xml_attribute *attr = node->first_attribute(); attr; attr = attr->next_attribute())
                    {
                        const std::string attrname = attr->name();
                        const std::string value = attr->value();

                        if (attrname == "cluster")
                        {
                            clusterId = std::stoi(value, 0, 16);
                        }
                        else if (attrname == "attr")
                        {
                            attrId = std::stoi(value, 0, 16);
                        }
                        else if (attrname == "type")
                        {
                            const std::string pre = value.substr(0, 2);
                            if (pre == "0x" || pre == "0X")
                                attrConfig.type.type = std::stoi(value, 0, 16);
                            else
                                attrConfig.type.type = ClustersInfo::AttrInfoExt::GetType(value);
                        }
                        else if (attrname == "value")
                        {
                            // this is kind of hard to parse
                            val = value;
                        }
                    }

                    if (0xFFFF != clusterId && 0xFFFF != attrId && attrConfig.type.type != 0xFF)
                    {
                        // first parse 'val'

                        if (attrConfig.type.IsInt())
                        {
                            const std::string pre = val.substr(0, 2);
                            uint64_t theVal;
                            if (pre == "0x" || pre == "0X")
                                theVal = std::stoi(val, 0, 16);
                            else
                                theVal = std::stoi(val, 0, 10);


                            BaseLib::PVariable pvar = std::make_shared<BaseLib::Variable>();

                            if (attrConfig.type.GetSize() > 4)
                            {
                                pvar->type = VariableType::tInteger64;
                                pvar->integerValue64 = theVal;
                            }
                            else
                            {
                                pvar->type = VariableType::tInteger;
                                pvar->integerValue = theVal;
                            }

                            attrConfig.type.SetVariable(pvar, attrConfig.value, false);
                        }
                        else if (attrConfig.type.IsFloatingPoint())
                        {
                            BaseLib::PVariable pvar = std::make_shared<BaseLib::Variable>();
                            pvar->type = VariableType::tFloat;
                            pvar->floatValue = std::stod(val);

                            attrConfig.type.SetVariable(pvar, attrConfig.value, false);
                        }
                        else
                        {
                            // can be only some kind of date/time (three zigbee types for that)
                            GD::out.printDebug("Debug: Parsing this type of values is not yet implemented for set attr config, type: 0x" + BaseLib::HelperFunctions::getHexString(attrConfig.type.type));
                        }

                        configInfo.attrSetConfigMap[std::make_pair(clusterId, attrId)] = attrConfig;
                    }
                }
                else if (nodename == "report")
                {
                    ReportingConfig reportingConfig;
                    std::string val;

                    for (rapidxml::xml_attribute *attr = node->first_attribute(); attr; attr = attr->next_attribute())
                    {
                        const std::string attrname = attr->name();
                        const std::string value = attr->value();

                        if (attrname == "cluster")
                        {
                            clusterId = std::stoi(value, 0, 16);
                        }
                        else if (attrname == "attr")
                        {
                            attrId = std::stoi(value, 0, 16);
                        }
                        else if (attrname == "type")
                        {
                            const std::string pre = value.substr(0, 2);
                            if (pre == "0x" || pre == "0X")
                                reportingConfig.type.type = std::stoi(value, 0, 16);
                            else
                                reportingConfig.type.type = ClustersInfo::AttrInfoExt::GetType(value);
                        }
                        else if (attrname == "minReportingInterval")
                        {
                            const std::string pre = value.substr(0, 2);
                            if (pre == "0x" || pre == "0X")
                                reportingConfig.minReportingInterval = std::stoi(value, 0, 16);
                            else
                                reportingConfig.minReportingInterval = std::stoi(value, 0, 10);
                        }
                        else if (attrname == "maxReportingInterval")
                        {
                            const std::string pre = value.substr(0, 2);
                            if (pre == "0x" || pre == "0X")
                                reportingConfig.maxReportingInterval = std::stoi(value, 0, 16);
                            else
                                reportingConfig.maxReportingInterval = std::stoi(value, 0, 10);
                        }
                        else if (attrname == "reportableChange")
                        {
                            // this is kind of hard to parse
                            val = value;
                        }
                    }

                    if (0xFFFF != clusterId && 0xFFFF != attrId && reportingConfig.type.type != 0xFF)
                    {
                        // first parse 'val'

                        // for reportable change only 'analog values' are allowed
                        if (reportingConfig.type.IsAnalog())
                        {
                            if (reportingConfig.type.IsInt())
                            {
                                const std::string pre = val.substr(0, 2);
                                uint64_t theVal;
                                if (pre == "0x" || pre == "0X")
                                    theVal = std::stoi(val, 0, 16);
                                else
                                    theVal = std::stoi(val, 0, 10);


                                BaseLib::PVariable pvar = std::make_shared<BaseLib::Variable>();

                                if (reportingConfig.type.GetSize() > 4)
                                {
                                    pvar->type = VariableType::tInteger64;
                                    pvar->integerValue64 = theVal;
                                }
                                else
                                {
                                    pvar->type = VariableType::tInteger;
                                    pvar->integerValue = theVal;
                                }

                                reportingConfig.type.SetVariable(pvar, reportingConfig.reportableChange, false);
                            }
                            else if (reportingConfig.type.IsFloatingPoint())
                            {
                                BaseLib::PVariable pvar = std::make_shared<BaseLib::Variable>();
                                pvar->type = VariableType::tFloat;
                                pvar->floatValue = std::stod(val);

                                reportingConfig.type.SetVariable(pvar, reportingConfig.reportableChange, false);
                            }
                            else
                            {
                                // can be only some kind of date/time (three zigbee types for that)
                                GD::out.printDebug("Debug: Parsing date/time values is not yet implemented for reporting config");
                            }
                        }
                        configInfo.reportingConfigMap[std::make_pair(clusterId, attrId)] = reportingConfig;
                    }

                }
            }
        }
    }

    return true;
}

void ZigbeePeer::SendAttrSetPackes(ConfigInfo& configInfo)
{
    const uint32_t addr = getAddress();
    const bool wakeupDev = wakeupDevice();

    bool hasPackets = false;
    for (auto it = configInfo.attrSetConfigMap.begin(); it != configInfo.attrSetConfigMap.end(); ++it)
    {
        if (it->second.value.empty()) continue;

        std::vector<uint8_t> data;
        PZigbeePacket packet = std::make_shared<ZigbeePacket>(data);

        packet->setSenderAddress(_physicalInterface->getAddress());
        packet->setDestinationAddress(addr);

        ZigbeeCommands::ZCLFrame zclFrame;

        zclFrame.SetGlobal(true);
        zclFrame.SetDirection(false); // from client to server!

        zclFrame.cmd = 0x02; // write attr

        zclFrame.payload.resize(3);

        ZigbeeCommands::MTCmd::WriteUint16(0, it->first.second, zclFrame.payload); // attr id

        zclFrame.payload[2] = it->second.type.type;

        int offset = 3;
        it->second.type.SetRawData(zclFrame.payload, offset, it->second.value); // this changes the offset as well

        packet->setClusterAndZCLData(it->first.first, zclFrame.GetEncoded()); // cluster id and frame

        hasPackets = true;
    }

    if (hasPackets)
    {
        _physicalInterface->tryToSend(addr, wakeupDev);
    }
}

void ZigbeePeer::SendReportConfigPackets(ConfigInfo& configInfo)
{
    std::shared_ptr<ClustersInfo> xmlClustersInfo;
    if (GD::family)
        xmlClustersInfo = GD::family->_clustersInfo;
    if (!xmlClustersInfo)
        return;

    bool hasPackets = false;
    const uint32_t addr = getAddress();
    const bool wakeupDev = wakeupDevice();

    {
        std::unique_lock<std::mutex> serviceLock(infoMutex);

        if (nodeInfo.EndPoints.find(endPoint) != nodeInfo.EndPoints.end())
        {
            ZigbeeNodeInfo::EndpointInfo &epinfo = nodeInfo.EndPoints[endPoint];

            for (unsigned int i = 0; i < epinfo.AllClusterList.size(); ++i)
            {
                const uint16_t clusterIdd = epinfo.AllClusterList[i];
                ZigbeeNodeInfo::EndpointInfo::ClusterInfo& clusterInfo = epinfo.ServersClustersInfoMap[clusterIdd];
                auto clusterXmlIt = xmlClustersInfo->serversClustersInfoMap.find(clusterIdd);

                // don't subscribe to unknown attributes for now
                if (clusterXmlIt == xmlClustersInfo->serversClustersInfoMap.end())
                {
                    GD::out.printDebug("Debug: Cluster not defined in clusters xml file, not sending reporting config packets. Cluster id: 0x" + BaseLib::HelperFunctions::getHexString(clusterIdd));
                    continue;
                }

                for (unsigned int a = 0; a < clusterInfo.attrsList.size(); ++a)
                {
                    const uint16_t attrId = clusterInfo.attrsList[a];

                    if (clusterInfo.attrsMap.find(attrId) == clusterInfo.attrsMap.end())
                    {
                        GD::out.printDebug("Debug: Attribute in attributes list is not in attributes map, cluster: " + clusterXmlIt->second.name + ", attr id: 0x" + BaseLib::HelperFunctions::getHexString(attrId));
                        continue;
                    }

                    const auto attrInfoIt = clusterXmlIt->second.attrsMap.find(attrId);
                    if (attrInfoIt == clusterXmlIt->second.attrsMap.end())
                    {
                        GD::out.printDebug("Debug: Attribute is not in xml cluster definition, cluster: " + clusterXmlIt->second.name + ", attr id: 0x" + BaseLib::HelperFunctions::getHexString(attrId));

                        continue;
                    }

                    if (configInfo.reportingConfigMap.find(std::make_pair(clusterIdd, attrId)) != configInfo.reportingConfigMap.end())
                    {
                        serviceLock.unlock();

                        const ReportingConfig& reportingConfig = configInfo.reportingConfigMap[std::make_pair(clusterIdd, attrId)];

                        // set reporting
                        std::vector<uint8_t> data;
                        PZigbeePacket packetToSend = std::make_shared<ZigbeePacket>(data);

                        packetToSend->setSenderAddress(_physicalInterface->getAddress());
                        packetToSend->setDestinationAddress(addr);

                        ZigbeeCommands::ZCLFrame zclFrameToSend;

                        zclFrameToSend.SetGlobal(true);
                        zclFrameToSend.SetDirection(false); // from client to server!
                        zclFrameToSend.cmd = 0x06; // configure reporting
                        zclFrameToSend.payload =
                        {
                            0x00, //direction - send reports to coordinator (1 would mean an indication that the coordinator will send reports to the device)
                            0x00, 0x00, // attribute id - 0 for onoff attribute - it is changed below
                            reportingConfig.type.type, //data type
                            // 0 for now because I want only the action to be reported
                            // TODO: change them to something more appropriate, especially the max reporting interval
                            0, 0, // apparently setting this to non zero will prevent reports for changes of 'discrete' values being sent each time
                            static_cast<uint8_t>(wakeupDev ? 0x10 : 0x58), static_cast<uint8_t>(wakeupDev ? 0x0E : 0x02), // Maximum Reporting Interval Field - 3600 sec for wakeup devices, 10 min for online
                            //0x01 // reportable change - for 'discrete' types (onoff, for example), it is omitted
                        };
                        ZigbeeCommands::MTCmd::WriteUint16(1, attrId, zclFrameToSend.payload);

                        ZigbeeCommands::MTCmd::WriteUint16(4, reportingConfig.minReportingInterval, zclFrameToSend.payload);
                        ZigbeeCommands::MTCmd::WriteUint16(6, reportingConfig.maxReportingInterval, zclFrameToSend.payload);

                        if (reportingConfig.type.IsAnalog() && !reportingConfig.reportableChange.empty())
                        {
                            zclFrameToSend.payload.insert(zclFrameToSend.payload.end(), reportingConfig.reportableChange.begin(), reportingConfig.reportableChange.end());
                        }

                        packetToSend->setClusterAndZCLData(clusterIdd, zclFrameToSend.GetEncoded());

                        GD::out.printDebug("Debug: Sending reporting config packet configured in config xml file");

                        _physicalInterface->enqueuePacket(packetToSend, wakeupDev);

                        hasPackets = true;

                        serviceLock.lock();
                    }
                    else if (attrInfoIt->second.IsReportable())
                    {
                        serviceLock.unlock();

                        // set reporting
                        std::vector<uint8_t> data;
                        PZigbeePacket packetToSend = std::make_shared<ZigbeePacket>(data);

                        packetToSend->setSenderAddress(_physicalInterface->getAddress());
                        packetToSend->setDestinationAddress(addr);


                        const ZigbeeNodeInfo::EndpointInfo::ClusterInfo::AttrInfo& attrInfoNode = clusterInfo.attrsMap[attrId];

                        ZigbeeCommands::ZCLFrame zclFrameToSend;

                        zclFrameToSend.SetGlobal(true);
                        zclFrameToSend.SetDirection(false); // from client to server!
                        zclFrameToSend.cmd = 0x06; // configure reporting
                        zclFrameToSend.payload =
                        {
                            0x00, //direction - send reports to coordinator (1 would mean an indication that the coordinator will send reports to the device)
                            0x00, 0x00, // attribute id - 0 for onoff attribute - it is changed below
                            attrInfoNode.type, //data type
                            // 0 for now because I want only the action to be reported
                            // TODO: change them to something more appropriate, especially the max reporting interval
                            0, 0, // apparently setting this to non zero will prevent reports for changes of 'discrete' values being sent each time
                            static_cast<uint8_t>(wakeupDev ? 0x10 : 0x58), static_cast<uint8_t>(wakeupDev ? 0x0E : 0x02), // Maximum Reporting Interval Field - 3600 sec for wakeup devices, 10 min for online
                            //0x01 // reportable change - for 'discrete' types (onoff, for example), it is omitted
                        };
                        ZigbeeCommands::MTCmd::WriteUint16(1, attrId, zclFrameToSend.payload);

                        // reportable change
                        if (attrInfoNode.IsAnalog()) // for discrete types, it does not exist
                        {
                            std::vector<uint8_t> zeroVal(attrInfoNode.GetSize(), 0);
                            zeroVal[0] = 1; // some value different than zero, or else it might send reports too often

                            zclFrameToSend.payload.insert(zclFrameToSend.payload.end(), zeroVal.begin(), zeroVal.end());
                        }

                        packetToSend->setClusterAndZCLData(clusterIdd, zclFrameToSend.GetEncoded());

                        GD::out.printDebug("Debug: Sending default reporting config packet");

                        _physicalInterface->enqueuePacket(packetToSend, wakeupDev);

                        hasPackets = true;

                        serviceLock.lock();
                    }
                }
            }
        }
    }

    if (hasPackets)
    {
        _physicalInterface->tryToSend(addr, wakeupDev);
    }
}

void ZigbeePeer::SendReportConfigGetPackets()
{
    bool hasPackets = false;
    const uint32_t addr = getAddress();
    const bool wakeupDev = wakeupDevice();

    {
        std::unique_lock<std::mutex> serviceLock(infoMutex);

        // if it supports discover attrs extended, reporting was set in admin stage when adding the node
        if (nodeInfo.EndPoints.find(endPoint) != nodeInfo.EndPoints.end())
        {
            ZigbeeNodeInfo::EndpointInfo &epinfo = nodeInfo.EndPoints[endPoint];

            for (unsigned int i = 0; i < epinfo.AllClusterList.size(); ++i)
            {
                const uint16_t clusterIdd = epinfo.AllClusterList[i];
                ZigbeeNodeInfo::EndpointInfo::ClusterInfo& clusterInfo = epinfo.ServersClustersInfoMap[clusterIdd];

                for (unsigned int a = 0; a < clusterInfo.attrsList.size(); ++a)
                {
                    const uint16_t attrId = clusterInfo.attrsList[a];
                    const ZigbeeNodeInfo::EndpointInfo::ClusterInfo::AttrInfo& attrInfoNode = clusterInfo.attrsMap[attrId];

                    if (attrInfoNode.IsReportable())
                    {
                        serviceLock.unlock();

                        // set reporting
                        std::vector<uint8_t> data;
                        PZigbeePacket packetToSend = std::make_shared<ZigbeePacket>(data);

                        packetToSend->setSenderAddress(_physicalInterface->getAddress());
                        packetToSend->setDestinationAddress(addr);

                        ZigbeeCommands::ZCLFrame zclFrameToSend;

                        zclFrameToSend.SetGlobal(true);
                        zclFrameToSend.SetDirection(false); // from client to server!
                        zclFrameToSend.cmd = 0x08; // read reporting config
                        zclFrameToSend.payload =
                        {
                            0x00, //direction - 0x0 = attribute is reported, 0x1 = attribute is received
                            0x00, 0x00, // attribute id - 0 for onoff attribute - it is changed below
                        };

                        ZigbeeCommands::MTCmd::WriteUint16(1, attrId, zclFrameToSend.payload);

                        packetToSend->setClusterAndZCLData(clusterIdd, zclFrameToSend.GetEncoded());

                        _physicalInterface->enqueuePacket(packetToSend, wakeupDev);

                        hasPackets = true;

                        serviceLock.lock();
                    }
                }
            }
        }
    }

    if (hasPackets)
    {
        _physicalInterface->tryToSend(addr, wakeupDev);
    }
}

bool ZigbeePeer::HandleSpecialAttr(uint16_t clusterId, uint16_t attrId, uint8_t cmd, const BaseLib::PVariable value)
{
    if (!value) return false;

    try
    {
        if (0x1 == clusterId) // power configuration
        {
            if (0x21 == attrId && (0x0A == cmd || 0x01 == cmd)) // BatteryPercentageRemaining, either a report or (unlikely) from 'read attributes'
            {
                // it's an uint8, so it should be integer
                if (BaseLib::VariableType::tInteger == value->type)
                {
                    std::lock_guard<std::mutex> lock(serviceMessagesMutex);
                    serviceMessages->set("LOWBAT", value->integerValue < 20); // kind or arbitrary???

                    return true;
                }
            }
        }
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }


    return false;
}


bool ZigbeePeer::ParseAttributeZCLFrame(PZigbeePacket packet, std::map<uint32_t, std::shared_ptr<std::vector<std::string>>>& valueKeys, std::map<uint32_t, std::shared_ptr<std::vector<PVariable>>>& rpcValues, bool fromNode,
                                        const ZigbeeCommands::ZCLFrame& zclFrame, uint16_t clusterId, bool& modified, bool& lastResponsePacket)
{
    std::shared_ptr<ClustersInfo> xmlClustersInfo;
    if (GD::family)
        xmlClustersInfo = GD::family->_clustersInfo;
    if (!xmlClustersInfo)
        return false;

    auto clusterXmlIt = xmlClustersInfo->serversClustersInfoMap.find(clusterId);
    if (clusterXmlIt == xmlClustersInfo->serversClustersInfoMap.end())
        return false;

    const bool isConfig = ZigbeeNodeInfo::EndpointInfo::ClusterInfo::IsConfigCluster(clusterId);

    GD::out.printInfo(std::string("Info: packetReceived: received an attr frame: ") + (zclFrame.cmd == 0x0a ? "Report attributes" : (zclFrame.cmd == 0x1 ? "Read attributes response" : "Write attributes")));

    // the payload is one or several records:
    // attr id (16 bits) and status (1 byte), which if 'success' (0) is followed by data type (1 byte) and the attribute value (variable length)
    unsigned int offset = 0;
    while (offset < zclFrame.payload.size())
    {
        // first, get attr id:
        if (offset + 2 > zclFrame.payload.size()) // we'll need to read two bytes
            break;

        const uint16_t attrId = ZigbeeCommands::MTCmd::ReadUint16(offset, zclFrame.payload);

        offset += 2;
        if (offset >= zclFrame.payload.size())  // there is no more data, bail out
            break;

        // status only for read attr response
        if (0x01 == zclFrame.cmd)
        {
            if (offset + 1 > zclFrame.payload.size()) // we'll need to read one byte
                break;

            const uint8_t status = zclFrame.payload[offset++];

            if (config_pending)
            {
                std::lock_guard<std::mutex> lock(valuesMutex);
                if (lastClusterAndAttr == std::make_pair(clusterId, attrId))
                    lastResponsePacket = true;
            }

            if (status != 0)
            {
                GD::out.printInfo("Info: packetReceived: attr status: 0x" + BaseLib::HelperFunctions::getHexString(status) + " description: " + ZigbeeCommands::ZCLFrame::GetErrorString(status));

                continue;
            }

            if (offset >= zclFrame.payload.size()) // there is no more data, bail out
                break;
        }

        if (offset + 1 > zclFrame.payload.size()) // we'll need to read one byte
            break;

        const uint8_t type = zclFrame.payload[offset++];

        if (offset >= zclFrame.payload.size())  // there is no more data, bail out
            break;

        // now deal with the value
        if (clusterXmlIt->second.attrsMap.find(attrId) != clusterXmlIt->second.attrsMap.end())
        {
            ClustersInfo::AttrInfoExt attrInfo = clusterXmlIt->second.attrsMap[attrId]; // copy it, might need to change the type if types do not match

            if (attrInfo.type != type && 0 != type) // sometimes I've seen unsupported attrs sent with 'no data' type
                GD::out.printDebug("Received type did not match the expected one for attribute: " + attrInfo.name + " received type: 0x" + BaseLib::HelperFunctions::getHexString(type));

            if (0 == type)
            {
                GD::out.printDebug("Received 'no data' for attribute: " + attrInfo.name + " ignoring...");
                continue; // skip 'no data'
            }

            BaseLib::PVariable pvalue = attrInfo.GetVariable(zclFrame.payload, offset);
            BaseLib::PVariable bogusSize;
            pvalue = attrInfo.GetComplexVariable(bogusSize, false, pvalue, attrInfo.IsStruct());

            std::vector<uint8_t> rawData = attrInfo.GetRawData(zclFrame.payload, offset);
            if (rawData.empty())
                rawData = std::vector<uint8_t>(zclFrame.payload.begin() + offset, zclFrame.payload.end());

            const int len = attrInfo.GetSize();
            if (len > 0)
                offset += len;
            else if (attrInfo.LengthInFirstOctet())
                offset += 1 + rawData.size();
            else if (attrInfo.LengthInFirstTwoOctets())
                offset += 2 + rawData.size();
            else
                offset += rawData.size();


            // now set the values in the data structures
            const std::string valueKey = clusterXmlIt->second.name + "." + attrInfo.name;
            std::string packetId = clusterXmlIt->second.name + "_" + attrInfo.name;

            if (0x01 == zclFrame.cmd) // read attr response
                packetId += "_EVENT";
            else if (0x0A == zclFrame.cmd) // attr report
                packetId += "_REPORT";
            else
                packetId += "_SET";

            HandleSpecialAttr(clusterId, attrId, zclFrame.cmd, pvalue);

// ********************************************************************************************************************************************

            {
                std::lock_guard<std::mutex> serviceLock(infoMutex);

                if (nodeInfo.EndPoints.find(endPoint) != nodeInfo.EndPoints.end())
                {
                    auto clusterIt = nodeInfo.EndPoints[endPoint].ServersClustersInfoMap.find(clusterId);
                    if (clusterIt != nodeInfo.EndPoints[endPoint].ServersClustersInfoMap.end())
                    {
                        auto attrIt = nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterId].attrsMap.find(attrId);
                        if (attrIt == nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterId].attrsMap.end())
                        {
                            // just add it
                            nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterId].attrsMap[attrId].attrId = attrId;
                            nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterId].attrsList.emplace_back(attrId);

                            attrIt = nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterId].attrsMap.find(attrId);

                            attrIt->second.type = attrInfo.type;
                            attrIt->second.accessControl = attrInfo.accessControl;

                            modified = true;

                            GD::out.printInfo("Info: packetReceived: Added attribute: 0x" + BaseLib::HelperFunctions::getHexString(attrId) + " in cluster: 0x" + BaseLib::HelperFunctions::getHexString(clusterId));
                        }

                        if (attrIt->second.type != type)
                        {
                            nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterId].attrsMap[attrId].type = type;
                            modified = true;
                        }
                        if (0x01 == zclFrame.cmd && !nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterId].attrsMap[attrId].IsReadable())
                        {
                            nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterId].attrsMap[attrId].accessControl |= 0x1;
                            modified = true;
                            GD::out.printInfo("Info: packetReceived: Attribute not specified as readable but it could be read");
                        }
                        if (0x0A == zclFrame.cmd && !nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterId].attrsMap[attrId].IsReportable())
                        {
                            nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterId].attrsMap[attrId].accessControl |= 0x4;
                            modified = true;
                            GD::out.printInfo("Info: packetReceived: Attribute not specified as reportable but got a report for it");
                        }
                    }
                    else
                        GD::out.printDebug("Cluster not found in info: 0x" + BaseLib::HelperFunctions::getHexString(clusterId));
                }
                else
                    GD::out.printDebug("End point not found in info");
            }

            if (fromNode && config_pending)
            {
                std::lock_guard<std::mutex> lock(valuesMutex);

                valuesMap[valueKey] = std::make_pair(pvalue, rawData);

                //GD::out.printInfo("Adding value to map: " + valueKey);
            }

// ********************************************************************************************************************************************


            auto packetIterator = _rpcDevice->packetsById.find(packetId);
            if (packetIterator != _rpcDevice->packetsById.end())
            {
                PPacket frame = _rpcDevice->packetsById.at(packetId);
                const int channel = frame->channel;

                bool channelFound = false;
                std::unordered_map<uint32_t, std::unordered_map<std::string, Systems::RpcConfigurationParameter>>::iterator channelIterator;
                if (isConfig)
                {
                    channelIterator = configCentral.find(channel);
                    if (channelIterator != configCentral.end())
                        channelFound = true;
                }
                else
                {
                    channelIterator = valuesCentral.find(channel);
                    if (channelIterator != valuesCentral.end())
                        channelFound = true;
                }

                if (channelFound)
                {
                    auto parameterIterator = channelIterator->second.find(valueKey);
                    if (parameterIterator != channelIterator->second.end())
                    {
                        parameterIterator->second.setBinaryData(rawData);

                        if(parameterIterator->second.databaseId > 0)
                            saveParameter(parameterIterator->second.databaseId, rawData);
                        else
                            saveParameter(0, isConfig ? ParameterGroup::Type::Enum::config : ParameterGroup::Type::Enum::variables, channel, valueKey, rawData);

                        if (parameterIterator->second.rpcParameter && parameterIterator->second.hasRoles())
                            pvalue = parameterIterator->second.rpcParameter->convertFromPacket(rawData, parameterIterator->second.mainRole(), true);

                        parameterIterator->second.setLogicalData(pvalue);

                        AddValueForEvents(valueKeys, rpcValues, channel, valueKey, pvalue);

                        if(_bl->debugLevel >= 4)
                        {
                            GD::out.printInfo("Info: packetReceived: Setting value for " + valueKey + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":" + std::to_string(channel)
                                              + " to value: " + BaseLib::HelperFunctions::getHexString(rawData));
                        }

                        if (fromNode)
                        {
                            Notify(channel, valueKey, pvalue);
                        }


                        // now see if if has bitfields, but only if the parsed value is integer
                        if (VariableType::tInteger == pvalue->type)
                        {
                            const uint32_t bitfieldVal = pvalue->integerValue;

                            for (const auto& bitfield : attrInfo.bitfields)
                            {
                                const uint32_t theVal = (bitfieldVal & bitfield.mask) >> bitfield.shifter;
                                const std::string bitfieldValueKey = clusterXmlIt->second.name + "." + attrInfo.name + "__" + bitfield.name;

                                auto bitfieldParameterIterator = channelIterator->second.find(bitfieldValueKey);
                                if (bitfieldParameterIterator != channelIterator->second.end())
                                {
                                    BaseLib::PVariable bitfieldVariable;
                                    std::vector<uint8_t> bRawData;
                                    if (bitfield.IsSingleBit())
                                    {
                                        bitfieldVariable = std::make_shared<BaseLib::Variable>(1 == theVal ? true : false);
                                        bRawData.resize(1);
                                        bRawData[0] = theVal;
                                    }
                                    else
                                    {
                                        bitfieldVariable = std::make_shared<BaseLib::Variable>(theVal);
                                        bRawData.resize(4);
                                        bRawData[0] = (theVal >> 24) & 0xFF;
                                        bRawData[1] = (theVal >> 16) & 0xFF;
                                        bRawData[2] = (theVal >> 8) & 0xFF;
                                        bRawData[3] = theVal & 0xFF;
                                    }


                                    if (fromNode && config_pending)
                                    {
                                        std::lock_guard<std::mutex> lock(valuesMutex);

                                        valuesMap[bitfieldValueKey] = std::make_pair(bitfieldVariable, bRawData);

                                        //GD::out.printInfo("Adding value to map: " + valueKey);
                                    }

                                    bitfieldParameterIterator->second.setBinaryData(bRawData);

                                    if(bitfieldParameterIterator->second.databaseId > 0)
                                        saveParameter(bitfieldParameterIterator->second.databaseId, bRawData);
                                    else
                                        saveParameter(0, isConfig ? ParameterGroup::Type::Enum::config : ParameterGroup::Type::Enum::variables, channel, bitfieldValueKey, bRawData);

                                    if (bitfieldParameterIterator->second.rpcParameter && bitfieldParameterIterator->second.hasRoles())
                                        bitfieldVariable = bitfieldParameterIterator->second.rpcParameter->convertFromPacket(bRawData, bitfieldParameterIterator->second.mainRole(), true);

                                    bitfieldParameterIterator->second.setLogicalData(bitfieldVariable);

                                    AddValueForEvents(valueKeys, rpcValues, channel, bitfieldValueKey, bitfieldVariable);

                                    if(_bl->debugLevel >= 4)
                                    {
                                        GD::out.printInfo("Info: packetReceived: Setting value for " + bitfieldValueKey + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":" + std::to_string(channel)
                                                          + " to value: " + BaseLib::HelperFunctions::getHexString(bRawData));
                                    }

                                    if (fromNode)
                                    {
                                        Notify(channel, bitfieldValueKey, bitfieldVariable);
                                    }
                                }
                                else
                                    GD::out.printDebug("packetReceived: bitfield parameter not found in the channel");
                            }
                        }
                    }
                    else
                        GD::out.printDebug("packetReceived: parameter not found in the channel");
                }
                else
                    GD::out.printDebug("packetReceived: channel for packet not found");
            }
            else if(GD::bl->debugLevel >= 4)
                GD::out.printInfo("Info: packetReceived: packet not found: " + packetId);
        }
        else
        {
            GD::out.printInfo("Info: packetReceived: Attribute not found: 0x" + BaseLib::HelperFunctions::getHexString(attrId) + " for cluster: 0x" + BaseLib::HelperFunctions::getHexString(clusterId));

            // but jump over its data
            ClustersInfo::AttrInfoExt attrInfo;
            attrInfo.type = type;
            const int len = attrInfo.GetSize();

            std::vector<uint8_t> rawData = attrInfo.GetRawData(zclFrame.payload, offset);
            if (rawData.empty())
                rawData = std::vector<uint8_t>(zclFrame.payload.begin() + offset, zclFrame.payload.end());

            if (len > 0)
                offset += len;
            else if (attrInfo.LengthInFirstOctet())
                offset += 1 + rawData.size();
            else if (attrInfo.LengthInFirstTwoOctets())
                offset += 2 + rawData.size();
            else
                offset += rawData.size();
        }
    }

    return true;
}



bool ZigbeePeer::ParseReportingZCLFrame(PZigbeePacket packet, std::map<uint32_t, std::shared_ptr<std::vector<std::string>>>& valueKeys, std::map<uint32_t, std::shared_ptr<std::vector<PVariable>>>& rpcValues, bool fromNode,
                                        const ZigbeeCommands::ZCLFrame& zclFrame, uint16_t clusterId)
{
    std::shared_ptr<ClustersInfo> xmlClustersInfo;
    if (GD::family)
        xmlClustersInfo = GD::family->_clustersInfo;
    if (!xmlClustersInfo)
        return false;

    auto clusterXmlIt = xmlClustersInfo->serversClustersInfoMap.find(clusterId);
    if (clusterXmlIt == xmlClustersInfo->serversClustersInfoMap.end())
        return false;

    //const bool isConfig = ZigbeeNodeInfo::EndpointInfo::ClusterInfo::IsConfigCluster(clusterId);

    GD::out.printInfo("Info: packetReceived: received a reporting frame");

    unsigned int offset = 0;
    while (offset < zclFrame.payload.size())
    {
        uint8_t status = 0;
        uint8_t direction = 0;
        if (0x06 == zclFrame.cmd) // configure reporting has a direction field, the response has both direction and status
        {
            direction = zclFrame.payload[offset++];
        }
        else
        {
            status = zclFrame.payload[offset++];

            if (offset + 1 > zclFrame.payload.size())
                break;

            direction = zclFrame.payload[offset++];
        }

        // first, get attr id:
        if (offset + 2 > zclFrame.payload.size()) // we'll need to read two bytes
            break;

        const uint16_t attrId = ZigbeeCommands::MTCmd::ReadUint16(offset, zclFrame.payload);
        offset += 2;

        GD::out.printInfo("Info: packetReceived: received a reporting frame, info on attr: 0x" + BaseLib::HelperFunctions::getHexString(attrId) + " for cluster: 0x" + BaseLib::HelperFunctions::getHexString(clusterId));

        if (0 == direction && 0 == status)
        {
            if (offset + 1 > zclFrame.payload.size()) // we'll need to read one byte
                break;
            const uint8_t type = zclFrame.payload[offset++];

            GD::out.printInfo("Info: packetReceived: received a reporting frame, type: 0x" + BaseLib::HelperFunctions::getHexString(type));

            if (offset + 2 > zclFrame.payload.size()) // we'll need to read two bytes
                break;

            ZigbeeType Uint16Type(0x21);

            uint16_t minReportingInterval = ZigbeeCommands::MTCmd::ReadUint16(offset, zclFrame.payload);
            BaseLib::PVariable minReportingIntervalVariable = Uint16Type.GetVariable(zclFrame.payload, offset);

            GD::out.printInfo("Info: packetReceived: received a reporting frame, min reporting interval: 0x" + BaseLib::HelperFunctions::getHexString(minReportingInterval));

            offset += 2;

            if (offset + 2 > zclFrame.payload.size()) // we'll need to read two bytes
                break;
            uint16_t maxReportingInterval = ZigbeeCommands::MTCmd::ReadUint16(offset, zclFrame.payload);
            BaseLib::PVariable maxReportingIntervalVariable = Uint16Type.GetVariable(zclFrame.payload, offset);

            GD::out.printInfo("Info: packetReceived: received a reporting frame, max reporting interval: 0x" + BaseLib::HelperFunctions::getHexString(maxReportingInterval));

            offset += 2;

            ZigbeeType attrDataType(type);

            bool hasReportableChange = false;
            std::vector<uint8_t> reportableChangeData;
            BaseLib::PVariable reportableChangeVariable;
            if (attrDataType.IsAnalog()) // only 'analog' types have a reportable change
            {
                GD::out.printInfo("Info: packetReceived: received a reporting frame, has 'reportable change'");

                if (offset + attrDataType.GetSize() > zclFrame.payload.size()) // we'll need to read several bytes
                    break;

                reportableChangeData = attrDataType.GetRawData(zclFrame.payload, offset);
                reportableChangeVariable = attrDataType.GetVariable(zclFrame.payload, offset);
                hasReportableChange = true;

                offset += attrDataType.GetSize();
            }

            // now deal with the values
            if (clusterXmlIt->second.attrsMap.find(attrId) != clusterXmlIt->second.attrsMap.end())
            {
                ClustersInfo::AttrInfoExt attrInfo = clusterXmlIt->second.attrsMap[attrId]; // copy it, might need to change the type if types do not match

                if (attrInfo.type != type)
                    GD::out.printDebug("Received type did not match the expected one for attribute: " + attrInfo.name + " received type: 0x" + BaseLib::HelperFunctions::getHexString(type) + " in reporting config frame");

                const std::string valueKeyBase = clusterXmlIt->second.name + "_REPORTING_CONFIG." + attrInfo.name;
                std::string packetId = clusterXmlIt->second.name + "_REPORTING_CONFIG_" + attrInfo.name;

                if (0x06 == zclFrame.cmd)
                    packetId += "_SET";
                else
                    packetId += "_EVENT";

                GD::out.printInfo("Info: packetReceived: received a reporting frame, packet id: " + packetId);

                auto packetIterator = _rpcDevice->packetsById.find(packetId);
                if (packetIterator != _rpcDevice->packetsById.end())
                {
                    PPacket frame = _rpcDevice->packetsById.at(packetId);
                    const int channel = frame->channel;

                    bool channelFound = false;
                    std::unordered_map<uint32_t, std::unordered_map<std::string, Systems::RpcConfigurationParameter>>::iterator channelIterator;
                    channelIterator = configCentral.find(channel);
                    if (channelIterator != configCentral.end())
                        channelFound = true;

                    if (channelFound)
                    {
                        const std::string minReportingIntervalKey = valueKeyBase + ".MIN_REPORTING_INTERVAL";

                        auto parameterIterator = channelIterator->second.find(minReportingIntervalKey);
                        if (parameterIterator != channelIterator->second.end())
                        {
                            std::vector<uint8_t> rawData(2);
                            rawData[0] = (minReportingInterval & 0xFF00) >> 8;
                            rawData[1] = (minReportingInterval & 0xFF);

                            parameterIterator->second.setBinaryData(rawData);

                            if(parameterIterator->second.databaseId > 0)
                                saveParameter(parameterIterator->second.databaseId, rawData);
                            else
                                saveParameter(0, ParameterGroup::Type::Enum::config, channel, minReportingIntervalKey, rawData);

                            if (parameterIterator->second.rpcParameter && parameterIterator->second.hasRoles())
                                    minReportingIntervalVariable = parameterIterator->second.rpcParameter->convertFromPacket(rawData, parameterIterator->second.mainRole(), true);

                            parameterIterator->second.setLogicalData(minReportingIntervalVariable);

                            AddValueForEvents(valueKeys, rpcValues, channel, minReportingIntervalKey, minReportingIntervalVariable);

                            if(_bl->debugLevel >= 4)
                            {
                                GD::out.printInfo("Info: packetReceived: Setting value for " + minReportingIntervalKey + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":" + std::to_string(channel)
                                                  + " to value: " + BaseLib::HelperFunctions::getHexString(rawData));
                            }

                            if (fromNode)
                            {
                                Notify(channel, minReportingIntervalKey, minReportingIntervalVariable);
                            }
                        }
                        else
                            GD::out.printDebug("packetReceived: parameter not found in the channel");

                        const std::string maxReportingIntervalKey = valueKeyBase + ".MAX_REPORTING_INTERVAL";

                        parameterIterator = channelIterator->second.find(maxReportingIntervalKey);
                        if (parameterIterator != channelIterator->second.end())
                        {
                            std::vector<uint8_t> rawData(2);
                            rawData[0] = (maxReportingInterval & 0xFF00) >> 8;
                            rawData[1] = (maxReportingInterval & 0xFF);

                            parameterIterator->second.setBinaryData(rawData);

                            if(parameterIterator->second.databaseId > 0)
                                saveParameter(parameterIterator->second.databaseId, rawData);
                            else
                                saveParameter(0, ParameterGroup::Type::Enum::config, channel, maxReportingIntervalKey, rawData);

                            if (parameterIterator->second.rpcParameter && parameterIterator->second.hasRoles())
                                    maxReportingIntervalVariable = parameterIterator->second.rpcParameter->convertFromPacket(rawData, parameterIterator->second.mainRole(), true);

                            parameterIterator->second.setLogicalData(maxReportingIntervalVariable);

                            AddValueForEvents(valueKeys, rpcValues, channel, maxReportingIntervalKey, maxReportingIntervalVariable);

                            if(_bl->debugLevel >= 4)
                            {
                                GD::out.printInfo("Info: packetReceived: Setting value for " + maxReportingIntervalKey + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":" + std::to_string(channel)
                                                  + " to value: " + BaseLib::HelperFunctions::getHexString(rawData));
                            }

                            if (fromNode)
                            {
                                Notify(channel, maxReportingIntervalKey, maxReportingIntervalVariable);
                            }
                        }
                        else
                            GD::out.printDebug("packetReceived: parameter not found in the channel");




                        if (hasReportableChange)
                        {
                            const std::string reportableChangeKey = valueKeyBase + ".REPORTABLE_CHANGE";

                            parameterIterator = channelIterator->second.find(reportableChangeKey);
                            if (parameterIterator != channelIterator->second.end())
                            {
                                parameterIterator->second.setBinaryData(reportableChangeData);

                                if(parameterIterator->second.databaseId > 0)
                                    saveParameter(parameterIterator->second.databaseId, reportableChangeData);
                                else
                                    saveParameter(0, ParameterGroup::Type::Enum::config, channel, reportableChangeKey, reportableChangeData);

                                if (parameterIterator->second.rpcParameter && parameterIterator->second.hasRoles())
                                    reportableChangeVariable = parameterIterator->second.rpcParameter->convertFromPacket(reportableChangeData, parameterIterator->second.mainRole(), true);

                                parameterIterator->second.setLogicalData(reportableChangeVariable);

                                AddValueForEvents(valueKeys, rpcValues, channel, reportableChangeKey, reportableChangeVariable);

                                if(_bl->debugLevel >= 4)
                                {
                                    GD::out.printInfo("Info: packetReceived: Setting value for " + reportableChangeKey + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":" + std::to_string(channel)
                                                      + " to value: " + BaseLib::HelperFunctions::getHexString(reportableChangeData));
                                }

                                if (fromNode)
                                {
                                    Notify(channel, reportableChangeKey, reportableChangeVariable);
                                }
                            }
                            else
                                GD::out.printDebug("packetReceived: parameter not found in the channel");
                        }
                    }
                    else
                        GD::out.printDebug("packetReceived: channel for packet not found");
                }
                else if(GD::bl->debugLevel >= 4)
                    GD::out.printInfo("Info: packetReceived: packet not found: " + packetId);
            }
            else
            {
                GD::out.printInfo("Info: packetReceived: Attribute not found: 0x" + BaseLib::HelperFunctions::getHexString(attrId) + " for cluster: 0x" + BaseLib::HelperFunctions::getHexString(clusterId));

                // but jump over its data
                ClustersInfo::AttrInfoExt attrInfo;
                attrInfo.type = type;
                const int len = attrInfo.GetSize();

                std::vector<uint8_t> rawData = attrInfo.GetRawData(zclFrame.payload, offset);
                if (rawData.empty())
                    rawData = std::vector<uint8_t>(zclFrame.payload.begin() + offset, zclFrame.payload.end());

                if (len > 0)
                    offset += len;
                else if (attrInfo.LengthInFirstOctet())
                    offset += 1 + rawData.size();
                else if (attrInfo.LengthInFirstTwoOctets())
                    offset += 2 + rawData.size();
                else
                    offset += rawData.size();
            }
        }
        else if (0 == status)
        {
            GD::out.printInfo("Info: packetReceived: configuration frame direction not zero");

            if (offset + 2 > zclFrame.payload.size()) // we'll need to read two bytes
                break;

            //const uint16_t timeoutPeriod = ZigbeeCommands::MTCmd::ReadUint16(offset, zclFrame.payload); // TODO: what to do with this? Add a read only parameter to expose it?
            offset += 2;
        }
        else
            GD::out.printInfo("Info: packetReceived: configuration frame status not zero: " + ZigbeeCommands::ZCLFrame::GetErrorString(status));
    }

    return true;
}


bool ZigbeePeer::HandleSpecialCommand(uint16_t clusterId, const ZigbeeCommands::ZCLFrame& zclFrame)
{
    if (zclFrame.IsGlobal()) return false; // not a command

    try
    {
        if (0x9 == clusterId) // alarms cluster
        {
            if (zclFrame.DirectionFromServerToClient()) // from device to homegear
            {
                if (0x0 == zclFrame.cmd) // alarm!
                {
                    if (zclFrame.payload.size() < 3) return false;

                    const uint8_t alarmCode = zclFrame.payload[0];
                    const uint16_t alarmCluster = ZigbeeCommands::MTCmd::ReadUint16(1, zclFrame.payload);

                    if (0x1 == alarmCluster) // alarm about the power config cluster?
                    {
                        if (0x10 == alarmCode || 0x11 == alarmCode) // BatteryVoltageMinThreshold or BatteryPercentageMinThreshold or BatteryVoltageThreshold1 or BatteryPercentageThreshold1
                        {
                            std::lock_guard<std::mutex> lock(serviceMessagesMutex);
                            serviceMessages->set("LOWBAT", true);
                        }
                    }
                }
            }
            else
            {
                if (0x0 == zclFrame.cmd) // reset alarm
                {
                    if (zclFrame.payload.size() < 3) return false;

                    const uint8_t alarmCode = zclFrame.payload[0];
                    const uint16_t alarmCluster = ZigbeeCommands::MTCmd::ReadUint16(1, zclFrame.payload);

                    if (0x1 == alarmCluster) // alarm about the power config cluster?
                    {
                        if (0x10 == alarmCode || 0x11 == alarmCode) // BatteryVoltageMinThreshold or BatteryPercentageMinThreshold or BatteryVoltageThreshold1 or BatteryPercentageThreshold1
                        {
                            std::lock_guard<std::mutex> lock(serviceMessagesMutex);
                            serviceMessages->set("LOWBAT", false);
                        }
                    }
                }
                else if (0x1 == zclFrame.cmd) // reset all alarms
                {
                    std::lock_guard<std::mutex> lock(serviceMessagesMutex);
                    serviceMessages->set("LOWBAT", false);
                }
            }
        }
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }


    return false;
}



bool ZigbeePeer::ParseLocalZCLFrame(PZigbeePacket packet, std::map<uint32_t, std::shared_ptr<std::vector<std::string>>>& valueKeys, std::map<uint32_t, std::shared_ptr<std::vector<PVariable>>>& rpcValues, bool fromNode,
                                    const ZigbeeCommands::ZCLFrame& zclFrame, uint16_t clusterId)
{
    std::shared_ptr<ClustersInfo> xmlClustersInfo;
    if (GD::family)
        xmlClustersInfo = GD::family->_clustersInfo;
    if (!xmlClustersInfo)
        return false;

    auto clusterXmlIt = xmlClustersInfo->serversClustersInfoMap.find(clusterId);
    if (clusterXmlIt == xmlClustersInfo->serversClustersInfoMap.end())
        return false;

    const bool isConfig = ZigbeeNodeInfo::EndpointInfo::ClusterInfo::IsConfigCluster(clusterId);


    GD::out.printInfo("Info: packetReceived: received a command frame");

    if (fromNode)
        HandleSpecialCommand(clusterId, zclFrame);


    std::map<uint8_t, ClustersInfo::ClusterInfoExt::Command>::iterator cmdIt;

    bool commandFound = false;

    // local, can be a command, typically a 'generated' one (from device to homegear), but could be a 'received' one (from homegear to device) if it's one generated by static packet handling, see above
    if (zclFrame.DirectionFromServerToClient()) // 'generated'
    {
        cmdIt = clusterXmlIt->second.commandsSent.find(zclFrame.cmd);
        if (cmdIt != clusterXmlIt->second.commandsSent.end())
            commandFound = true;
    }
    else // 'received'
    {
        cmdIt = clusterXmlIt->second.commandsReceived.find(zclFrame.cmd);
        if (cmdIt != clusterXmlIt->second.commandsReceived.end())
            commandFound = true;
    }

    if (commandFound)
    {
        const std::string packetId = clusterXmlIt->second.name + "_" + cmdIt->second.name;

        auto packetIterator = _rpcDevice->packetsById.find(packetId);

        if (packetIterator != _rpcDevice->packetsById.end())
        {
            PPacket frame = _rpcDevice->packetsById.at(packetId);
            const int channel = frame->channel;

            bool channelFound = false;
            std::unordered_map<uint32_t, std::unordered_map<std::string, Systems::RpcConfigurationParameter>>::iterator channelIterator;
            if (isConfig)
            {
                channelIterator = configCentral.find(channel);
                if (channelIterator != configCentral.end())
                    channelFound = true;
            }
            else
            {
                channelIterator = valuesCentral.find(channel);
                if (channelIterator != valuesCentral.end())
                    channelFound = true;
            }

            if (channelFound)
            {
                std::string valueKey = clusterXmlIt->second.name + ".";

                if (zclFrame.payload.empty() && cmdIt->second.params.empty())
                {
                    // the case for an ".ACTION" value
                    valueKey += cmdIt->second.name + ".ACTION";

                    auto parameterIterator = channelIterator->second.find(valueKey);
                    if (parameterIterator == channelIterator->second.end())
                    {
                        std::vector<uint8_t> val(1, 0xff);
                        parameterIterator->second.setBinaryData(val);

                        if(parameterIterator->second.databaseId > 0)
                            saveParameter(parameterIterator->second.databaseId, val);
                        else
                            saveParameter(0, isConfig ? ParameterGroup::Type::Enum::config : ParameterGroup::Type::Enum::variables, channel, valueKey, val);

                        BaseLib::PVariable pvalue = std::make_shared<BaseLib::Variable>(true);

                        if (parameterIterator->second.rpcParameter && parameterIterator->second.hasRoles())
                            pvalue = parameterIterator->second.rpcParameter->convertFromPacket(val, parameterIterator->second.mainRole(), true);

                        parameterIterator->second.setLogicalData(pvalue);

                        AddValueForEvents(valueKeys, rpcValues, channel, valueKey, pvalue);


                        if(_bl->debugLevel >= 4)
                            GD::out.printInfo("Info: packetReceived: Setting event for " + valueKey + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":" + std::to_string(channel));

                        if (fromNode)
                        {
                            Notify(channel, valueKey, pvalue);
                        }
                    }
                    else
                        GD::out.printDebug("packetReceived: parameter not found in the channel");
                }
                else if (zclFrame.payload.empty())
                {
                    GD::out.printDebug("packetReceived: command payload empty but there should be parameters");
                    return false;
                }
                else if (cmdIt->second.params.empty())
                {
                    GD::out.printDebug("packetReceived: command payload not empty but there are no parameters");
                    return false;
                }
                else
                {
                    const std::string valueKeyBase = valueKey + cmdIt->second.name + ".";
                    std::string valueKeyBaseAlt;
                    if (DeviceParameter::IsResponse(cmdIt->second.name))
                        valueKeyBaseAlt = valueKey + cmdIt->second.name.substr(0, cmdIt->second.name.length() - 9) + ".";

                    unsigned int offset = 0;
                    BaseLib::PVariable sizeVariable;
                    bool octets = true;

                    // workaround for cases where a field might miss

                    const unsigned int expectedSize = cmdIt->second.GetSize();
                    if (expectedSize && expectedSize < zclFrame.payload.size())
                    {
                        GD::out.printDebug("packetReceived: Expected a smaller payload for this packet, payload: " + BaseLib::HelperFunctions::getHexString(zclFrame.payload));
                    }

                    // it's much easier to collect values in a map to use them later if needed,
                    // than getting them from the channel or rpcValues
                    bool collectValues = false;
                    if (expectedSize > 0 && expectedSize != zclFrame.payload.size() && cmdIt->second.MightMissFields())
                        collectValues = true;

                    std::map<std::string, BaseLib::PVariable> packetValuesMap;

                    for (auto paramIt = cmdIt->second.params.begin(); offset < zclFrame.payload.size() && paramIt != cmdIt->second.params.end(); ++paramIt)
                    {
                        // ckecking if the field should be skipped...
                        if (expectedSize > 0 and expectedSize != zclFrame.payload.size() && paramIt->mightMiss)
                        {
                            bool skip = false;

                            if (0xFF != paramIt->ifNotExistsAttr)
                            {
                                // the attribute was specified, check if it exists
                                std::lock_guard<std::mutex> lock(infoMutex);

                                if (nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterXmlIt->second.clusterId].attrsMap.find(paramIt->ifNotExistsAttr) == nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterXmlIt->second.clusterId].attrsMap.end())
                                    skip = true;
                            }
                            else if (!paramIt->ifField.empty())
                            {
                                // two cases, regular fields and subfields (only bitflags/bitfields are handled)

                                // find the saved value for the 'if' field
                                if (packetValuesMap.find(paramIt->ifField) != packetValuesMap.end())
                                {
                                    // check the value
                                    BaseLib::PVariable ifvalue = packetValuesMap[paramIt->ifField];
                                    if (ifvalue)
                                    {
                                        if (ifvalue->type != BaseLib::VariableType::tBoolean && ifvalue->type != BaseLib::VariableType::tInteger)
                                        {
                                            GD::out.printDebug("packetReceived: 'If' field for: " + paramIt->name + " is not boolean or integer, please correct in clusters xml file");
                                        }
                                        else if (ifvalue->type == BaseLib::VariableType::tBoolean && paramIt->ifFieldValue.empty())
                                        {
                                            if (ifvalue->booleanValue)
                                                skip = true;
                                        }
                                        else if (paramIt->ifFieldValue.empty())
                                        {
                                            if (ifvalue->integerValue)
                                                skip = true;
                                        }
                                        else if (static_cast<unsigned int>(ifvalue->integerValue) == paramIt->GetIfFieldValue())
                                            skip = true;
                                    }
                                }
                            }
                            else if (!paramIt->ifNotField.empty())
                            {
                                // two cases, regular fields and subfields (only bitflags/bitfields are handled)

                                // find the saved value for the 'if' field
                                if (packetValuesMap.find(paramIt->ifNotField) != packetValuesMap.end())
                                {
                                    // check the value
                                    BaseLib::PVariable ifnotvalue = packetValuesMap[paramIt->ifNotField];
                                    if (ifnotvalue)
                                    {
                                        if (ifnotvalue->type != BaseLib::VariableType::tBoolean && ifnotvalue->type != BaseLib::VariableType::tInteger)
                                        {
                                            GD::out.printDebug("packetReceived: 'If' field for: " + paramIt->name + " is not boolean or integer, please correct in clusters xml file");
                                        }
                                        else if (ifnotvalue->type == BaseLib::VariableType::tBoolean)
                                        {
                                            if (!ifnotvalue->booleanValue)
                                                skip = true;
                                        }
                                        else
                                        {
                                            if (!ifnotvalue->integerValue)
                                                skip = true;
                                        }
                                    }
                                }
                            }
                            else
                                skip = true;

                            if (skip)
                            {
                                GD::out.printInfo("packetReceived: Looks like a parameter is missing from the packet: " + paramIt->name + " skipping...");
                                continue;
                            }
                        }
                        // end checking of field

                        valueKey = valueKeyBase + paramIt->name;
                        auto parameterIterator = channelIterator->second.find(valueKey);
                        if (parameterIterator == channelIterator->second.end())
                        {
                            valueKey = valueKeyBaseAlt + paramIt->name;
                            parameterIterator = channelIterator->second.find(valueKey);
                        }

                        if (parameterIterator != channelIterator->second.end())
                        {
                            BaseLib::PVariable pvalue = paramIt->GetVariable(zclFrame.payload, offset);
                            std::vector<uint8_t> rawData;

                            if (paramIt->alternField.empty())
                            {
                                pvalue = paramIt->GetComplexVariable(sizeVariable, octets, pvalue, paramIt->IsStruct());

                                rawData = paramIt->GetRawData(zclFrame.payload, offset);
                            }
                            else
                            {
                                if (!paramIt->params.empty())
                                    GD::out.printDebug("packetReceived: Looks like an 'altern' parameter has parameters, problem parameter: " + paramIt->name +" please fix it in the clusters xml file");

                                if (paramIt->alternRecords.empty())
                                    GD::out.printDebug("packetReceived: Looks like a parameter with 'altern' has no 'altern' records, problem parameter: " + paramIt->name + " please fix it in the clusters xml file");

                                //1. find the deciding parameter - use valueKeyBase and valueKeyBaseAlt when trying to find it
                                std::string alternValueKey = valueKeyBase + paramIt->alternField;
                                auto alternParameterIterator = channelIterator->second.find(alternValueKey);
                                if (alternParameterIterator == channelIterator->second.end())
                                {
                                    alternValueKey = valueKeyBaseAlt + paramIt->alternField;
                                    alternParameterIterator = channelIterator->second.find(alternValueKey);
                                }

                                if (alternParameterIterator != channelIterator->second.end())
                                {
                                    //2. if found, get its value with either getValue (no retrieval for the host/no sending packets, get the cached value, it was obtained from this packet, or better, directly
                                    // if should be some integer
                                    const BaseLib::PVariable alternValue = alternParameterIterator->second.getLogicalData();
                                    if (alternValue->type == BaseLib::VariableType::tInteger || alternValue->type == BaseLib::VariableType::tInteger64)
                                    {
                                        //3. using its value, locate the corresponding record in alternRecords
                                        uint64_t altervalint = alternValue->type == BaseLib::VariableType::tInteger ? static_cast<uint64_t>(alternValue->integerValue) : static_cast<uint64_t>(alternValue->integerValue64);

                                        if (paramIt->alternRecords.find(altervalint) != paramIt->alternRecords.end())
                                        {
                                            const ClustersInfo::Param::AlternRecord& alternRecord = paramIt->alternRecords[altervalint];
                                            // use it to call GetComplexVariable
                                            pvalue = alternRecord.GetComplexVariable(sizeVariable, octets, pvalue, paramIt->IsStruct());
                                        }
                                        else
                                        {
                                            GD::out.printDebug("packetReceived: Looks like an 'altern' record does not exist for 'altern' value, problem parameter: " + paramIt->name + " please fix it in the clusters xml file");

                                            // try the old way
                                            pvalue = paramIt->GetComplexVariable(sizeVariable, octets, pvalue, paramIt->IsStruct());

                                            rawData = paramIt->GetRawData(zclFrame.payload, offset);
                                        }
                                    }
                                    else
                                        GD::out.printDebug("packetReceived: Looks like an 'altern' parameter does not have an integer value, problem parameter: " + paramIt->name + " please fix it in the clusters xml file");
                                }
                                else
                                {
                                    GD::out.printDebug("packetReceived: Looks like an 'altern' parameter does not exist, problem parameter: " + paramIt->name + " please fix it in the clusters xml file");

                                    // try the old way
                                    pvalue = paramIt->GetComplexVariable(sizeVariable, octets, pvalue, paramIt->IsStruct());

                                    rawData = paramIt->GetRawData(zclFrame.payload, offset);
                                }
                            }

                            // TODO: usually complex types are the last in the params list, so this should be ok
                            // otherwise this should be checked and size computed accordingly
                            // for attributes this issue does not exist, there is a single attribute, the 'raw data' is the whole data

                            if (rawData.empty())
                            {
                                rawData = std::vector<uint8_t>(zclFrame.payload.begin() + offset, zclFrame.payload.end());

                                auto paramCopy = paramIt;
                                ++paramCopy;
                                if (paramCopy != cmdIt->second.params.end())
                                    GD::out.printDebug("packetReceived: Looks like a complex parameter was not the last, problem parameter: " + paramIt->name + " the next: " + paramCopy->name);
                            }


                            const int len = paramIt->GetSize();
                            if (len > 0)
                                offset += len;
                            else if (paramIt->LengthInFirstOctet())
                                offset += 1 + rawData.size();
                            else if (paramIt->LengthInFirstTwoOctets())
                                offset += 2 + rawData.size();
                            else
                                offset += rawData.size();

                            if (parameterIterator->second.rpcParameter && parameterIterator->second.hasRoles())
                                pvalue = parameterIterator->second.rpcParameter->convertFromPacket(rawData, parameterIterator->second.mainRole(), true);

                            parameterIterator->second.setLogicalData(pvalue);

                            parameterIterator->second.setBinaryData(rawData);

                            AddValueForEvents(valueKeys, rpcValues, channel, valueKey, pvalue);


                            if (collectValues)
                                packetValuesMap[paramIt->name] = pvalue;

                            if (paramIt->IsSizeField())
                            {
                                sizeVariable = pvalue;
                                octets = (paramIt->sizeType != 2);
                            }
                            else
                            {
                                if (!paramIt->IsExplicitSizeField())
                                {
                                    sizeVariable.reset();
                                    octets = true;
                                }
                            }

                            // check if the param has proper len, otherwise warn
                            if (paramIt->len > 0)
                            {
                                unsigned int explen = paramIt->len;
                                if (paramIt->sizeType == 2)
                                    explen *= paramIt->GetFixedSize();

                                if (explen != rawData.size())
                                    GD::out.printDebug("packetReceived: A parameter is marked with a length in clusters xml file, problem parameter: " + paramIt->name + " expected length (bytes): " + std::to_string(explen) + " actual length: " + std::to_string(rawData.size()));
                            }

                            if(parameterIterator->second.databaseId > 0)
                                saveParameter(parameterIterator->second.databaseId, rawData);
                            else
                                saveParameter(0, isConfig ? ParameterGroup::Type::Enum::config : ParameterGroup::Type::Enum::variables, channel, valueKey, rawData);

                            if(_bl->debugLevel >= 4)
                            {
                                GD::out.printInfo("Info: packetReceived: Setting value for " + valueKey + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":" + std::to_string(channel)
                                                  + " to value: " + BaseLib::HelperFunctions::getHexString(rawData));
                            }

                            if (fromNode)
                            {
                                Notify(channel, valueKey, pvalue);
                            }


                            //  now see if if has bitfields
                            if (VariableType::tInteger == pvalue->type)
                            {
                                const uint32_t bitfieldVal = pvalue->integerValue;

                                for (const auto& bitfield : paramIt->bitfields)
                                {
                                    const uint32_t theVal = (bitfieldVal & bitfield.mask) >> bitfield.shifter;
                                    std::string bitfieldValueKey = valueKeyBase + paramIt->name + "__" + bitfield.name;
                                    auto bitfieldParameterIterator = channelIterator->second.find(bitfieldValueKey);
                                    if (bitfieldParameterIterator == channelIterator->second.end())
                                    {
                                        bitfieldValueKey = valueKeyBaseAlt + paramIt->name + "__" + bitfield.name;
                                        bitfieldParameterIterator = channelIterator->second.find(bitfieldValueKey);
                                    }

                                    if (bitfieldParameterIterator != channelIterator->second.end())
                                    {
                                        BaseLib::PVariable bitfieldVariable;
                                        std::vector<uint8_t> bRawData;
                                        if (bitfield.IsSingleBit())
                                        {
                                            bitfieldVariable = std::make_shared<BaseLib::Variable>(1 == theVal ? true : false);
                                            bRawData.resize(1);
                                            bRawData[0] = theVal;
                                        }
                                        else
                                        {
                                            bitfieldVariable = std::make_shared<BaseLib::Variable>(theVal);
                                            bRawData.resize(4);
                                            bRawData[0] = (theVal >> 24) & 0xFF;
                                            bRawData[1] = (theVal >> 16) & 0xFF;
                                            bRawData[2] = (theVal >> 8) & 0xFF;
                                            bRawData[3] = theVal & 0xFF;
                                        }

                                        if (fromNode && config_pending)
                                        {
                                            std::lock_guard<std::mutex> lock(valuesMutex);

                                            valuesMap[bitfieldValueKey] = std::make_pair(bitfieldVariable, bRawData);

                                            //GD::out.printInfo("Adding value to map: " + valueKey);
                                        }

                                        if (bitfieldParameterIterator->second.rpcParameter && bitfieldParameterIterator->second.hasRoles())
                                            bitfieldVariable = bitfieldParameterIterator->second.rpcParameter->convertFromPacket(bRawData, bitfieldParameterIterator->second.mainRole(), true);

                                        bitfieldParameterIterator->second.setLogicalData(bitfieldVariable);
                                        bitfieldParameterIterator->second.setBinaryData(bRawData);

                                        AddValueForEvents(valueKeys, rpcValues, channel, bitfieldValueKey, bitfieldVariable);

                                        if (collectValues)
                                            packetValuesMap[paramIt->name + "." + bitfield.name] = pvalue;

                                        if (bitfield.IsSizeField())
                                        {
                                            sizeVariable = bitfieldVariable;
                                            octets = (bitfield.sizeType != 2);
                                        }


                                        if(parameterIterator->second.databaseId > 0)
                                            saveParameter(bitfieldParameterIterator->second.databaseId, bRawData);
                                        else
                                            saveParameter(0, isConfig ? ParameterGroup::Type::Enum::config : ParameterGroup::Type::Enum::variables, channel, bitfieldValueKey, bRawData);


                                        if(_bl->debugLevel >= 4)
                                        {
                                            GD::out.printInfo("Info: packetReceived: Setting value for " + bitfieldValueKey + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":" + std::to_string(channel)
                                                              + " to value: " + BaseLib::HelperFunctions::getHexString(bRawData));
                                        }

                                        if (fromNode)
                                        {
                                            Notify(channel, bitfieldValueKey, bitfieldVariable);
                                        }
                                    }
                                    else
                                        GD::out.printDebug("packetReceived: bitfield parameter not found in the channel");
                                }
                            }
                        }
                        else
                        {
                            GD::out.printDebug("packetReceived: parameter not found in the channel");
                            break;
                        }
                    }
                }
            }
            else
                GD::out.printDebug("packetReceived: channel for packet not found");

        }
        else if(GD::bl->debugLevel >= 4)
            GD::out.printInfo("Info: packetReceived: packet not found: " + packetId);

        return true;
    }


    return false;
}



bool ZigbeePeer::ParsePacketDynamic(PZigbeePacket packet, std::map<uint32_t, std::shared_ptr<std::vector<std::string>>>& valueKeys, std::map<uint32_t, std::shared_ptr<std::vector<PVariable>>>& rpcValues, bool fromNode)
{
    try
    {
        if (!packet)
            return false;

        if(GD::bl->debugLevel >= 4)
            GD::out.printInfo("Entering ParsePacketDynamic");

        // this will be implemented later, when ZDO will be implemented

        // identify the packets 'by type', bail out if there aren't any

        const uint32_t packetType = packet->getType();
        bool packetDefined = false;

        std::pair<BaseLib::DeviceDescription::PacketsByMessageType::iterator, BaseLib::DeviceDescription::PacketsByMessageType::iterator> range = _rpcDevice->packetsByMessageType.equal_range(packetType);

        for (BaseLib::DeviceDescription::PacketsByMessageType::iterator i = range.first; i != range.second; ++i)
        {
            //GD::out.printInfo("ParsePacketDynamic, checking a frame");

            const std::shared_ptr<ZigbeeDescPacket> tframe = std::dynamic_pointer_cast<ZigbeeDescPacket>(i->second);
            if (!tframe)
            {
                //GD::out.printInfo("getValuesFromPacket, skipping static frame");
                continue;
            }

            packetDefined = true;
            break;
        }

        if (!packetDefined)
        {
            if(GD::bl->debugLevel >= 4)
                GD::out.printInfo("No dynamic packet defined for this");

            // TODO: metadata not defined for an attribute, might be new, so add it here

            //return false;
        }

        std::shared_ptr<ClustersInfo> xmlClustersInfo;
        if (GD::family)
            xmlClustersInfo = GD::family->_clustersInfo;
        if (!xmlClustersInfo)
            return false;

        const uint16_t clusterId = packet->getCluster();

        // 1. Identify the cluster info corresponding with the clusterId above
        auto clusterXmlIt = xmlClustersInfo->serversClustersInfoMap.find(clusterId);
        if (clusterXmlIt == xmlClustersInfo->serversClustersInfoMap.end())
        {
            GD::out.printInfo("Info: packetReceived: received a frame for an unknown cluster: 0x" + BaseLib::HelperFunctions::getHexString(clusterId));
            return false;
        }

        std::vector<uint8_t> zclData = packet->getZCLData();

        ZigbeeCommands::ZCLFrame zclFrame;
        if (zclFrame.Decode(zclData))
        {
            if (zclFrame.IsManufacturerSpecific())
                GD::out.printInfo("Info: packetReceived: received a manufacturer specific ZCL frame, manufacturer code: 0x" + BaseLib::HelperFunctions::getHexString(zclFrame.ManufacturerCode));
            // 2. identify what kind of packet it is and depending on that, identify the 'parameters' that correspond to it
            // 3. parse the packet and extract the 'physical' and 'logical' data for each parameter
            // as a note, not all packets have all parameters inside (maybe I should mark them as mandatory/optional?). Some of the last ones might miss. For example, the first param might be 'STATUS'. If status is failure, the next params might miss.

            // 4. set the values in homegear data structures
            // the names are CLUSTER_NAME.ATTRIBUTE_NAME (for attributes) or CLUSTER_NAME.COMMAND_PACKET_NAME.PARAMETER_NAME (for parameters in commands).
            // If the command packet is a '_RESPONSE' the parameter might have the name as the one from the command that requests the response (due of joining, it will be also both readable and writable).


            // handle it depending on the zcl frame type (might be several packets above identified 'by type' select the one(s) compatible with the actual zcl frame)
            if (zclFrame.IsGlobal()) // something related with attributes probably
            {
                // global, might be an attribute response or report
                // or even something else? Could be an attribute set, for example, the parsing here could be for a 'set' packet generated by the 'static' packet handling (for keeping in sync 'dynamic' and 'static' parameters)

                // the nice thing about those packets is that usually they contain a single value (not truly the case for reports, though and should not be assumed true for attribute responses, either, currently I'm asking for a single attribute with 'get' packets, but this could change, make it flexible)

                // TODO: also deal with 'structured' things

                // TODO: adjust the data 'local' structures (in info) by adding missing attributes and setting properties
                // save the new ones and generate metadata for them

                bool modified = false;
                bool lastResponsePacket = false;

                bool handled = false;

                if (0x01 == zclFrame.cmd || 0x0a == zclFrame.cmd // read attributes response or report attributes
                        || 0x02 == zclFrame.cmd || 0x03 == zclFrame.cmd || 0x05 == zclFrame.cmd) // or write attributes, they all have similar 'records', except that read response also has 'status' field that must be handled separately

                    handled = ParseAttributeZCLFrame(packet, valueKeys, rpcValues, fromNode, zclFrame, clusterId, modified, lastResponsePacket);

                else if (0x06 == zclFrame.cmd || 0x09 == zclFrame.cmd) // Configure Reporting (set) or Read Reporting Configuration Response

                    handled = ParseReportingZCLFrame(packet, valueKeys, rpcValues, fromNode, zclFrame, clusterId);

                /*
                else if (0x0B == zclFrame.cmd) // default response. How do I know to which sent packet is associated? cluster and cmd, together with transaction nr?
                {
                    handled = true;
                }
                */

                if (modified)
                    saveVariables();

                if (lastResponsePacket && !errorOrTimeoutOccured)
                {
                    // TODO: add attributes and commands that are 'mandatory-optional' (that is, they are mandatory only is some attr has a certain value, otherwise they are optional) - if they become mandatory because of the retrieved value
                    // the attributes are supposed to be added anyway because the 'get' for them retrieves a value if they are supported
                    // so only a check is required here, but for the commands the check should be done and they should be added in node info before calling RefreshDeviceDescription() if they become mandatory because of the retrieved value

                    CheckAddOptionalMandatory();


                    // refresh the metadata
                    RefreshDeviceDescription();

                    // TODO: Add attributes reporting config for the case when attributes description is not available from the device
                    // TODO: Also to allow configuring later this, add 'fake' attributes for the real ones to allow setting attribute reporting!!!!!!!

                    config_pending = false;
                    metadataRefreshed = true;

                    SetValuesFromValuesMap();

                    {
                        std::lock_guard<std::mutex> serviceLock(infoMutex);
                        initializeServiceVariables();
                    }

                    saveVariables();

                    ConfigInfo configInfo;
                    LoadConfigFile(configInfo);

                    // now set attributes reporting
                    SendReportConfigPackets(configInfo);

                    SendAttrSetPackes(configInfo);

                    SendReportConfigGetPackets();
                    // TODO: maybe after receiving those, delete the ones that are reported as not being reportable (there shouldn't be any if the zigbee specs are respected, but some devices might not comply)
                }

                return handled;
            }
            else // some local command
                return ParseLocalZCLFrame(packet, valueKeys, rpcValues, fromNode, zclFrame, clusterId);
        }
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return false;
}


void ZigbeePeer::packetReceived(PZigbeePacket packet)
{
    try
    {
        if(_disposing || !packet || !_rpcDevice || packet->payload().size() < packet->getHeaderSize() /*|| packet->senderAddress() != (int32_t)service.GetIpAddress()*/)
            return;

        std::shared_ptr<ZigbeeCentral> central = std::dynamic_pointer_cast<ZigbeeCentral>(getCentral());
        if(!central)
            return;

        if(GD::bl->debugLevel >= 4)
            GD::out.printInfo("Entering packet received");

        setLastPacketReceived();

        setRssiDevice(packet->getRssi() * -1);

        {
            std::lock_guard<std::mutex> lock(serviceMessagesMutex);
            serviceMessages->endUnreach();
        }


        // from here deal with special cases to be handled

        // for example packets for LOWBAT if existing


        // until here

        std::map<uint32_t, std::shared_ptr<std::vector<std::string>>> valueKeys;
        std::map<uint32_t, std::shared_ptr<std::vector<PVariable>>> rpcValues;

        // handling of packets specified in the 'model' xml file
        // ***********************************************************************************************************************************************************

        ParsePacketStatic(packet, valueKeys, rpcValues, true);

        // ***********************************************************************************************************************************************************
        // From here, generic handling

        ParsePacketDynamic(packet, valueKeys, rpcValues, true);

        HandleSpecialPacket(packet, valueKeys, rpcValues);

        //if(!rpcValues.empty() && !resendPacket)
        if(!rpcValues.empty())
        {
            DealWithStaticSpecialParams(valueKeys, rpcValues);

            for(std::map<uint32_t, std::shared_ptr<std::vector<std::string>>>::iterator j = valueKeys.begin(); j != valueKeys.end(); ++j)
            {
                if(j->second->empty())
                    continue;

                std::string eventSource = "device-" + std::to_string(_peerID);
                std::string address(_serialNumber + ":" + std::to_string(j->first));
                raiseEvent(eventSource, _peerID, j->first, j->second, rpcValues.at(j->first));
                raiseRPCEvent(eventSource, _peerID, j->first, address, j->second, rpcValues.at(j->first));
            }
        }
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}


bool ZigbeePeer::DealWithStaticSpecialParams(std::map<uint32_t, std::shared_ptr<std::vector<std::string>>>& valueKeys, std::map<uint32_t, std::shared_ptr<std::vector<PVariable>>>& rpcValues)
{
    if(GD::bl->debugLevel >= 4)
        GD::out.printInfo("Entering dealing with special params");

    bool valueSet = false;

    for(std::map<uint32_t, std::shared_ptr<std::vector<std::string>>>::iterator j = valueKeys.begin(); j != valueKeys.end(); ++j)
    {
        if(j->second->empty())
            continue;

        if (j->first != 1)
            continue;

        for(std::vector<std::string>::iterator i = j->second->begin(); i != j->second->end(); ++i)
        {
            if((*i == "HUE" || *i == "SATURATION" || *i == "LEVEL") //Calculate RGB
                    && valuesCentral.at(1).find("HUE") != valuesCentral.at(1).end()) //Does this peer support colors?
            {
                // check to see if RGB really exists
                std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator channelIterator = valuesCentral.find(1);
                if(channelIterator == valuesCentral.end())
                    break;

                if(channelIterator->second.find("RGB") == channelIterator->second.end())
                    break;


                std::vector<uint8_t> parameterData = valuesCentral.at(1).at("LEVEL").getBinaryData();
                const uint8_t brightness = parameterData.empty() ? 0 : parameterData[0];
                parameterData = valuesCentral.at(1).at("SATURATION").getBinaryData();
                const uint8_t saturation = parameterData.empty() ? 0 : parameterData[0];
                parameterData = valuesCentral.at(1).at("HUE").getBinaryData();
                const uint8_t hue = parameterData.empty() ? 0 : parameterData[0];

                BaseLib::Color::HSV hsv((double)hue / 254. * 360., (double)saturation / 254.0, (double)brightness / 254.0);

                BaseLib::Color::RGB rgb = hsv.toRGB();
                int RGBint = rgb.getRed();
                RGBint <<= 8;
                RGBint |= rgb.getGreen();
                RGBint <<= 8;
                RGBint |= rgb.getBlue();

                BaseLib::Systems::RpcConfigurationParameter& rgbParameter = valuesCentral.at(1).at("RGB");
                parameterData.resize(3);
                parameterData[0] = rgb.getRed();
                parameterData[1] = rgb.getGreen();
                parameterData[2] = rgb.getBlue();

                rgbParameter.setBinaryData(parameterData);

                if(rgbParameter.databaseId > 0) saveParameter(rgbParameter.databaseId, parameterData);
                else saveParameter(0, ParameterGroup::Type::Enum::variables, 1, "RGB", parameterData);

                PVariable pvalue = rgbParameter.rpcParameter->convertFromPacket(parameterData, rgbParameter.mainRole(), true);
                rgbParameter.setLogicalData(pvalue);

                AddValueForEvents(valueKeys, rpcValues, 1, "RGB", pvalue);

                valueSet = true;

                break;
            }
        }
    }

    return valueSet;
}


void ZigbeePeer::AddValueForEvents(std::map<uint32_t, std::shared_ptr<std::vector<std::string>>>& valueKeys, std::map<uint32_t, std::shared_ptr<std::vector<PVariable>>>& rpcValues, int channel, const std::string& valueKey, PVariable variable)
{
    if (!variable) return;

    if(!valueKeys[channel] || !rpcValues[channel])
    {
        valueKeys[channel].reset(new std::vector<std::string>());
        rpcValues[channel].reset(new std::vector<PVariable>());
    }

    for(unsigned int i = 0; i < valueKeys[channel]->size(); ++i)
    {
        if (valueKey == valueKeys[channel]->at(i))
        {
            // it's already added, replace the value
            (*rpcValues[channel])[i] = variable;
            return;
        }
    }

    valueKeys[channel]->push_back(valueKey);
    rpcValues[channel]->push_back(variable);
}



bool ZigbeePeer::HandleSpecialPacket(const PZigbeePacket& packet, std::map<uint32_t, std::shared_ptr<std::vector<std::string>>>& valueKeys, std::map<uint32_t, std::shared_ptr<std::vector<PVariable>>>& rpcValues)
{
    try
    {
        if (!packet)
            return false;

        if(GD::bl->debugLevel >= 4)
            GD::out.printInfo("Entering handling special packets");

        // identify the packets 'by type', bail out if there aren't any

        //const uint32_t packetType = packet->getType();

        const uint16_t clusterId = packet->getCluster();

        std::vector<uint8_t> zclData = packet->getZCLData();

        ZigbeeCommands::ZCLFrame zclFrame;
        if (zclFrame.Decode(zclData))
        {
            if (zclFrame.IsManufacturerSpecific())
                return false;

            bool hasPackets = false;

            const uint32_t addr = getAddress();
            const bool wakeupDev = wakeupDevice();

            if (zclFrame.IsGlobal()) // something related with attributes probably
            {
                if (0x00 == zclFrame.cmd) // read attr request
                {
                    // for now, handle TIME cluster request
                    if (0x0A == clusterId)
                    {
                        if (2 > zclFrame.payload.size()) // we'll need to read two bytes
                            return false;

                        const uint16_t attrId = ZigbeeCommands::MTCmd::ReadUint16(0, zclFrame.payload);

                        if (0x00 == attrId) // a Time attr request, the only one dealt with for now
                        {
                            std::vector<uint8_t> data;
                            PZigbeePacket packetToSend = std::make_shared<ZigbeePacket>(data);

                            packetToSend->setSenderAddress(_physicalInterface->getAddress());
                            packetToSend->setDestinationAddress(addr);

                            ZigbeeCommands::ZCLFrame zclFrameToSend;

                            zclFrameToSend.SetGlobal(true);
                            zclFrameToSend.SetDirection(!zclFrame.DirectionFromServerToClient());
                            //zclFrameToSend.SetDefaultResponseDisabled(true); // this is by default
                            zclFrameToSend.cmd = 0x01; // read attrs response
                            zclFrameToSend.payload =
                            {
                                0x00, 0x00, // attr id = Time
                                0x00, // status
                                0xE2, // data type = UTC
                                0xff, 0xff, 0xff, 0xff // invalid value, must be changed to something valid below
                            };

                            struct tm timeinfo;
                            timeinfo.tm_year = 100;
                            timeinfo.tm_yday = 0;
                            timeinfo.tm_mon = 0;
                            timeinfo.tm_mday = 1;
                            timeinfo.tm_hour = 0;
                            timeinfo.tm_min = 0;
                            timeinfo.tm_sec = 0;
                            timeinfo.tm_wday = -1;
                            timeinfo.tm_isdst = -1;

                            // timeinfo->tm_wday will be set
                            const time_t s = mktime(&timeinfo);
                            const time_t n = std::time(nullptr);

                            const uint32_t res = static_cast<uint32_t>(n - s);

                            ZigbeeCommands::MTCmd::WriteUint32(4, res, zclFrameToSend.payload);

                            packetToSend->setClusterAndZCLData(clusterId, zclFrameToSend.GetEncoded());

                            hasPackets = true;

                            _physicalInterface->enqueuePacket(packetToSend, wakeupDev);

                            ParsePacketStatic(packetToSend, valueKeys, rpcValues, false);
                            ParsePacketDynamic(packetToSend, valueKeys, rpcValues, false);
                        }
                    }
                }
            }

            if (hasPackets)
            {
                _physicalInterface->tryToSend(addr, wakeupDev);

                return true;
            }
        }
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return false;
}



PVariable ZigbeePeer::getParamset(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, bool checkAcls)
{
    try
    {
        if(_disposing)
            return Variable::createError(-32500, "Peer is disposing.");
        if(channel < 0)
            channel = 0;
        if(remoteChannel < 0)
            remoteChannel = 0;
        Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
        if(functionIterator == _rpcDevice->functions.end())
            return Variable::createError(-2, "Unknown channel.");
        if(type == ParameterGroup::Type::none)
            type = ParameterGroup::Type::link;
        PFunction rpcFunction = functionIterator->second;
        PParameterGroup parameterGroup = rpcFunction->getParameterGroup(type);
        if(!parameterGroup)
            return Variable::createError(-3, "Unknown parameter set.");
        PVariable variables = std::make_shared<Variable>(VariableType::tStruct);

        auto central = getCentral();
        if(!central)
            return Variable::createError(-32500, "Could not get central.");

        for(Parameters::iterator i = parameterGroup->parameters.begin(); i != parameterGroup->parameters.end(); ++i)
        {
            if(!i->second || i->second->id.empty() || !i->second->visible)
                continue;
            if(!i->second->visible && !i->second->service && !i->second->internal && !i->second->transform)
            {
                GD::out.printDebug("Debug: Omitting parameter " + i->second->id + " because of it's ui flag.");
                continue;
            }
            PVariable element;
            if(type == ParameterGroup::Type::Enum::variables)
            {
                if(checkAcls && !clientInfo->acls->checkVariableReadAccess(central->getPeer(_peerID), channel, i->first))
                    continue;

                if(!i->second->readable)
                    continue;
                if(valuesCentral.find(channel) == valuesCentral.end())
                    continue;
                if(valuesCentral[channel].find(i->second->id) == valuesCentral[channel].end())
                    continue;

                if (0 == channel)
                    element = getChannelZeroVariable(i->second->id);

                if (!element)
                {
                    element = valuesCentral[channel][i->second->id].getLogicalData();
                    if (!element || element->type == VariableType::tVoid)
                    {
                        auto& parameter = valuesCentral[channel][i->second->id];
                        std::vector<uint8_t> data = valuesCentral[channel][i->second->id].getBinaryData();
                        element = i->second->convertFromPacket(data, parameter.mainRole(), false);
                        if (!element || element->type == VariableType::tVoid)
                            element = i->second->logical->getDefaultValue();
                    }
                }
            }
            else if(type == ParameterGroup::Type::Enum::config)
            {
                if(configCentral.find(channel) == configCentral.end())
                    continue;
                if(configCentral[channel].find(i->second->id) == configCentral[channel].end())
                    continue;

                if (0 == channel)
                    element = getChannelZeroVariable(i->second->id);

                if (!element)
                {
                    element = configCentral[channel][i->second->id].getLogicalData();

                    if (!element || element->type == VariableType::tVoid)
                    {
                        auto& parameter = configCentral[channel][i->second->id];
                        std::vector<uint8_t> data = parameter.getBinaryData();
                        element = i->second->convertFromPacket(data, parameter.mainRole(), false);

                        if (!element || element->type == VariableType::tVoid)
                            element = i->second->logical->getDefaultValue();
                    }
                }
            }
            else if(type == ParameterGroup::Type::Enum::link)
            {
                return Variable::createError(-3, "Parameter set type is not supported.");
            }

            if(!element)
                continue;

            if(i->second->password)
                element = std::make_shared<BaseLib::Variable>(element->type);
            if(element->type == VariableType::tVoid)
                continue;
            variables->structValue->insert(StructElement(i->second->id, element));
        }

        return variables;
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return Variable::createError(-32500, "Unknown application error.");
}

PVariable ZigbeePeer::getParamsetDescription(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, bool checkAcls)
{
    try
    {
        if(_disposing)
            return Variable::createError(-32500, "Peer is disposing.");
        if(channel < 0)
            channel = 0;
        Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
        if(functionIterator == _rpcDevice->functions.end())
            return Variable::createError(-2, "Unknown channel");
        PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(type);
        if(!parameterGroup)
            return Variable::createError(-3, "Unknown parameter set");
        if(type == ParameterGroup::Type::link && remoteID > 0)
        {
            std::shared_ptr<BaseLib::Systems::BasicPeer> remotePeer = getPeer(channel, remoteID, remoteChannel);
            if(!remotePeer)
                return Variable::createError(-2, "Unknown remote peer.");
        }

        return Peer::getParamsetDescription(clientInfo, channel, parameterGroup, checkAcls);
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return Variable::createError(-32500, "Unknown application error.");
}


PVariable ZigbeePeer::getConfigParameter(PRpcClientInfo clientInfo, uint32_t channel, std::string name)
{
    try
    {
        if(_disposing)
            return Variable::createError(-32500, "Peer is disposing.");
        if(!_rpcDevice)
            return Variable::createError(-32500, "Unknown application error.");

        auto channelIterator = configCentral.find(channel);
        if(channelIterator == configCentral.end())
            return Variable::createError(-2, "Unknown channel.");
        auto parameterIterator = channelIterator->second.find(name);
        if(parameterIterator == channelIterator->second.end())
            return Variable::createError(-5, "Unknown parameter.");

        //Check if channel still exists in device description
        Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
        if(functionIterator == _rpcDevice->functions.end())
            return Variable::createError(-2, "Unknown channel (2).");

        PParameterGroup parameterGroup = getParameterSet(channel, ParameterGroup::Type::Enum::config);
        PParameter parameter = parameterGroup->parameters.at(name);
        if(!parameter)
            return Variable::createError(-5, "Unknown parameter.");
        if(!parameter->readable)
            return Variable::createError(-6, "Parameter is not readable.");

        PVariable variable = configCentral[channel][parameter->id].getLogicalData();

        if (!variable || variable->type == VariableType::tVoid)
        {
            std::vector<uint8_t> parameterData = parameterIterator->second.getBinaryData();
            variable = parameter->convertFromPacket(parameterData, parameterIterator->second.mainRole(), false);

            if (!variable || variable->type == VariableType::tVoid)
                variable = parameter->logical->getDefaultValue();
        }

        if(parameter->password)
            variable = std::make_shared<Variable>(variable->type);

        return variable;
    }
    catch(const std::exception& ex)
    {
        _bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return Variable::createError(-32500, "Unknown application error.");
}

PZigbeePacket ZigbeePeer::ConstructGetOrSetPacket(uint32_t addr, const std::shared_ptr<Parameter::Packet>& getRequest, const std::shared_ptr<Parameter>& parameter, std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator channelIterator, bool isGet)
{
    PacketsById::iterator packetIterator = _rpcDevice->packetsById.find(getRequest->id);
    if(packetIterator != _rpcDevice->packetsById.end())
    {
        PPacket frame = packetIterator->second;

        bool isCommand = (frame->subtype == 1);

        const std::shared_ptr<ZigbeeParameter> zParameter = std::dynamic_pointer_cast<ZigbeeParameter>(parameter);
        if (zParameter)
        {
            const DeviceParameter* deviceParameter = &zParameter->deviceParameter;

            if (isCommand)
            {
                if (deviceParameter->attrInfo)
                    GD::out.printDebug(std::string(isGet ? "Get" : "Set") + " packet marked as command but the parameter seems to be an attribute");
                if (deviceParameter->commandsReceived.empty())
                    GD::out.printDebug(std::string(isGet ? "Get" : "Set") + " packet marked as command but the parameter does not have a received command set");
            }
            else if (!deviceParameter->attrInfo)
                GD::out.printDebug(std::string(isGet ? "Get" : "Set") + " packet marked as for attribute but the parameter does not appear to be an attribute");


            const uint16_t clusterId = (frame->type >> 8);

            std::shared_ptr<ClustersInfo> xmlClustersInfo;
            if (GD::family)
                xmlClustersInfo = GD::family->_clustersInfo;

            auto clusterXmlIt = xmlClustersInfo->serversClustersInfoMap.find(clusterId);
            if (clusterXmlIt != xmlClustersInfo->serversClustersInfoMap.end())
            {
                std::vector<uint8_t> data;
                PZigbeePacket packet = std::make_shared<ZigbeePacket>(data);

                packet->setSenderAddress(_physicalInterface->getAddress());
                packet->setDestinationAddress(addr);

                ZigbeeCommands::ZCLFrame zclFrame;
                bool constructed = false;

                if (isCommand)
                {
                    // a set/get command, this is more complex, must be constructed out of individual parameters
                    if (!deviceParameter->commandsReceived.empty() && deviceParameter->commandsReceived.begin()->second)
                    {
                        zclFrame.SetGlobal(false);
                        zclFrame.SetDirection(false); // from client to server!

                        const ClustersInfo::ClusterInfoExt::Command* command = deviceParameter->commandsReceived.begin()->second;

                        zclFrame.cmd = command->key;

                        //GD::out.printInfo("Constructing command 0x" + BaseLib::HelperFunctions::getHexString(zclFrame.cmd));

                        constructed = true;

                        // it's much easier to collect values in a map to use them later if needed,
                        // than getting them from the channel
                        bool collectValues = false;
                        if (command->MightMissFields())
                            collectValues = true;

                        std::map<std::string, BaseLib::PVariable> packetValuesMap;

                        const std::string valueKeyBase =  clusterXmlIt->second.name + "." + command->name + ".";
                        std::string valueKeyBaseAlt;
                        if (DeviceParameter::IsResponse(command->name))
                            valueKeyBaseAlt = clusterXmlIt->second.name + "." + command->name.substr(0, command->name.length() - 9) + ".";


                        int offset = 0;
                        for (const ClustersInfo::Param& param : command->params)
                        {
                            // check if the param should miss from the packet
                            if (param.mightMiss)
                            {
                                bool skip = false;

                                if (0xFF != param.ifNotExistsAttr)
                                {
                                    // the attribute was specified, check if it exists
                                    std::lock_guard<std::mutex> lock(infoMutex);
                                    if (nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterXmlIt->second.clusterId].attrsMap.find(param.ifNotExistsAttr) == nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterXmlIt->second.clusterId].attrsMap.end())
                                        skip = true;
                                }
                                else if (!param.ifField.empty())
                                {
                                    // two cases, regular fields and subfields (only bitflags/bitfields are handled)

                                    // find the saved value for the 'if' field
                                    if (packetValuesMap.find(param.ifField) != packetValuesMap.end())
                                    {
                                        // check the value
                                        BaseLib::PVariable ifvalue = packetValuesMap[param.ifField];
                                        if (ifvalue)
                                        {
                                            if (ifvalue->type != BaseLib::VariableType::tBoolean && ifvalue->type != BaseLib::VariableType::tInteger)
                                            {
                                                GD::out.printDebug("ConstructGetOrSetPacket: 'If' field for: " + param.name + " is not boolean or integer, please correct in clusters xml file");
                                            }
                                            else if (ifvalue->type == BaseLib::VariableType::tBoolean && param.ifFieldValue.empty())
                                            {
                                                if (ifvalue->booleanValue)
                                                    skip = true;
                                            }
                                            else if (param.ifFieldValue.empty())
                                            {
                                                if (ifvalue->integerValue)
                                                    skip = true;
                                            }
                                            else if (static_cast<unsigned int>(ifvalue->integerValue) == param.GetIfFieldValue())
                                                skip = true;
                                        }
                                    }
                                }
                                else if (!param.ifNotField.empty())
                                {
                                    // two cases, regular fields and subfields (only bitflags/bitfields are handled)

                                    // find the saved value for the 'if' field
                                    if (packetValuesMap.find(param.ifNotField) != packetValuesMap.end())
                                    {
                                        // check the value
                                        BaseLib::PVariable ifnotvalue = packetValuesMap[param.ifNotField];
                                        if (ifnotvalue)
                                        {
                                            if (ifnotvalue->type != BaseLib::VariableType::tBoolean && ifnotvalue->type != BaseLib::VariableType::tInteger)
                                            {
                                                GD::out.printDebug("ConstructGetOrSetPacket: 'If' field for: " + param.name + " is not boolean or integer, please correct in clusters xml file");
                                            }
                                            else if (ifnotvalue->type == BaseLib::VariableType::tBoolean)
                                            {
                                                if (ifnotvalue->booleanValue)
                                                    skip = true;
                                            }
                                            else
                                            {
                                                if (ifnotvalue->integerValue)
                                                    skip = true;
                                            }
                                        }
                                    }
                                }
                                else
                                    skip = true;

                                if (skip)
                                {
                                    GD::out.printInfo("ConstructGetOrSetPacket: Looks like a parameter is missing from the packet: " + param.name + " skipping...");
                                    continue;
                                }
                            }
                            // end check param

                            // locate the proper item
                            std::string valueKey = valueKeyBase + param.name;
                            auto parameterIterator = channelIterator->second.find(valueKey);
                            if (parameterIterator == channelIterator->second.end())
                            {
                                valueKey = valueKeyBaseAlt + param.name;
                                parameterIterator = channelIterator->second.find(valueKey);
                            }

                            if (parameterIterator == channelIterator->second.end())
                            {
                                GD::out.printDebug("Debug: Couldn't find parameter: " + valueKey);
                                constructed = false;
                                break;
                            }

                            BaseLib::Systems::RpcConfigurationParameter& rpcparam = parameterIterator->second;

                            data = rpcparam.getBinaryData();

                            ZigbeeType zType;
                            zType.type = param.type;

                            zType.SetRawData(zclFrame.payload, offset, data); // this changes the offset as well

                            if (collectValues)
                            {
                                BaseLib::PVariable paramValue = rpcparam.getLogicalData();
                                packetValuesMap[param.name] = paramValue;

                                // also save the bitflag/bitmap values
                                if (paramValue && paramValue->type == BaseLib::VariableType::tInteger)
                                {
                                    for (const auto& bitfield : param.bitfields)
                                    {
                                        const uint32_t val = (paramValue->integerValue & bitfield.mask) >> bitfield.shifter;
                                        packetValuesMap[param.name + "." + bitfield.name] = std::make_shared<BaseLib::Variable>(val);
                                    }
                                }
                            }
                        }
                    }
                    else
                        GD::out.printDebug("Debug: No 'sent command' exists to be used to " + std::string(isGet ? "retrieve" : "set") + " the parameter value");
                }
                else if (deviceParameter->attrInfo)
                {
                    zclFrame.SetGlobal(true);
                    zclFrame.SetDirection(false); // from client to server!

                    if (zParameter->isReportingConfig)
                    {
                        if (isGet)
                            zclFrame.cmd = 0x08; // Read Reporting Configuration
                        else
                            zclFrame.cmd = 0x06; // Configure Reporting

                        const int restSize = (isGet ? 0 : (7 + (deviceParameter->attrInfo->IsAnalog() ? deviceParameter->attrInfo->GetSize() : 0)));

                        zclFrame.payload.resize(3 + restSize);
                        zclFrame.payload[0] = 0; // direction

                        ZigbeeCommands::MTCmd::WriteUint16(1, deviceParameter->attrInfo->attrId, zclFrame.payload);

                        constructed = true;

                        if (!isGet)
                        {
                            zclFrame.payload[3] = deviceParameter->attrInfo->type;

                            const std::string valueKeyBase = clusterXmlIt->second.name + "_REPORTING_CONFIG." + deviceParameter->attrInfo->name;
                            // minimum reporting interval
                            std::string valueKey = valueKeyBase + ".MIN_REPORTING_INTERVAL";

                            int offset = 4;
                            auto parameterIterator = channelIterator->second.find(valueKey);
                            if (parameterIterator == channelIterator->second.end())
                            {
                                GD::out.printDebug("Debug: Couldn't find parameter: " + valueKey);
                                constructed = false;
                            }
                            else
                            {
                                BaseLib::Systems::RpcConfigurationParameter& rpcparam = parameterIterator->second;

                                data = rpcparam.getBinaryData();
                                if (data.size() != 2)
                                {
                                    GD::out.printDebug("Debug: Data expected to be uint16 has bad length for parameter: " + valueKey + ", will be resized");
                                    data.resize(2, 0);
                                    rpcparam.setBinaryData(data);
                                }

                                ZigbeeType zType(0x21); //uint16

                                zType.SetRawData(zclFrame.payload, offset, data); // this changes the offset as well
                            }

                            if (constructed)
                            {
                                // maximum  reporting interval
                                valueKey = valueKeyBase + ".MAX_REPORTING_INTERVAL";

                                parameterIterator = channelIterator->second.find(valueKey);
                                if (parameterIterator == channelIterator->second.end())
                                {
                                    GD::out.printDebug("Debug: Couldn't find parameter: " + valueKey);
                                    constructed = false;
                                }
                                else
                                {
                                    BaseLib::Systems::RpcConfigurationParameter& rpcparam = parameterIterator->second;

                                    data = rpcparam.getBinaryData();
                                    if (data.size() != 2)
                                    {
                                        GD::out.printDebug("Debug: Data expected to be uint16 has bad length for parameter: " + valueKey + ", will be resized");
                                        data.resize(2, 0);
                                        rpcparam.setBinaryData(data);
                                    }

                                    ZigbeeType zType(0x21); //uint16

                                    zType.SetRawData(zclFrame.payload, offset, data); // this changes the offset as well
                                }


                                if (constructed)
                                {
                                    // reportable change if it exists
                                    if (deviceParameter->attrInfo->IsAnalog())
                                    {
                                        valueKey = valueKeyBase + ".REPORTABLE_CHANGE";

                                        parameterIterator = channelIterator->second.find(valueKey);
                                        if (parameterIterator == channelIterator->second.end())
                                        {
                                            GD::out.printDebug("Debug: Couldn't find parameter: " + valueKey);
                                            constructed = false;
                                        }
                                        else
                                        {
                                            BaseLib::Systems::RpcConfigurationParameter& rpcparam = parameterIterator->second;

                                            data = rpcparam.getBinaryData();
                                            if (data.size() != static_cast<size_t>(deviceParameter->attrInfo->GetSize()))
                                            {
                                                GD::out.printDebug("Debug: Data has bad length for parameter: " + valueKey + ", will be resized");
                                                data.resize(deviceParameter->attrInfo->GetSize(), 0);
                                                rpcparam.setBinaryData(data);
                                            }

                                            deviceParameter->attrInfo->SetRawData(zclFrame.payload, offset, data); // this changes the offset as well
                                        }
                                    }

                                    // timeout period
                                    if (constructed)
                                    {
                                        valueKey = valueKeyBase + ".TIMEOUT_PERIOD";

                                        parameterIterator = channelIterator->second.find(valueKey);
                                        if (parameterIterator == channelIterator->second.end())
                                        {
                                            GD::out.printDebug("Debug: Couldn't find parameter: " + valueKey);
                                            constructed = false;
                                        }
                                        else
                                        {
                                            BaseLib::Systems::RpcConfigurationParameter& rpcparam = parameterIterator->second;

                                            data = rpcparam.getBinaryData();
                                            if (data.size() != 2)
                                            {
                                                GD::out.printDebug("Debug: Data expected to be uint16 has bad length for parameter: " + valueKey + ", will be resized");
                                                data.resize(2, 0);
                                                rpcparam.setBinaryData(data);
                                            }

                                            ZigbeeType zType(0x21); //uint16

                                            zType.SetRawData(zclFrame.payload, offset, data); // this changes the offset as well
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        // get for an attribute, simpler
                        if (isGet)
                            zclFrame.cmd = 0x00; // read attr
                        else
                            zclFrame.cmd = 0x02; // write attr

                        zclFrame.payload.resize(isGet ? 2 : 3);

                        ZigbeeCommands::MTCmd::WriteUint16(0, deviceParameter->attrInfo->attrId, zclFrame.payload);

                        constructed = true;

                        if (!isGet)
                        {
                            zclFrame.payload[2] = deviceParameter->attrInfo->type;

                            const std::string valueKey = clusterXmlIt->second.name + "." + deviceParameter->attrInfo->name;
                            auto parameterIterator = channelIterator->second.find(valueKey);

                            if (parameterIterator == channelIterator->second.end())
                            {
                                GD::out.printDebug("Debug: Couldn't find parameter: " + valueKey);
                                constructed = false;
                            }
                            else
                            {
                                BaseLib::Systems::RpcConfigurationParameter& rpcparam = parameterIterator->second;

                                data = rpcparam.getBinaryData();

                                ZigbeeType zType;
                                zType.type = deviceParameter->attrInfo->type;

                                int offset = 3;
                                zType.SetRawData(zclFrame.payload, offset, data); // this changes the offset as well
                            }
                        }
                    }
                }

                if (constructed)
                {
                    packet->setClusterAndZCLData(clusterId, zclFrame.GetEncoded());

                    return packet;
                }
            }
            else
                GD::out.printDebug("Debug: Cluster not found");

        }
        else
            GD::out.printDebug("Couldn't cast to a dynamic parameter despite having a " + std::string(isGet ? "Get" : "Set") + " packet that indicate it's dynamic");
    }
    else
        GD::out.printDebug("No frame was found for parameter " + parameter->metadata);

    PZigbeePacket packet;

    return packet;
}

PZigbeePacket ZigbeePeer::ConstructGetPacketForAttr(uint16_t clusterId, uint16_t attrId)
{
    std::shared_ptr<ClustersInfo> xmlClustersInfo;
    if (GD::family)
        xmlClustersInfo = GD::family->_clustersInfo;

    auto clusterXmlIt = xmlClustersInfo->serversClustersInfoMap.find(clusterId);
    if (clusterXmlIt != xmlClustersInfo->serversClustersInfoMap.end())
    {
        auto attrXmlIt = clusterXmlIt->second.attrsMap.find(attrId);
        if (attrXmlIt != clusterXmlIt->second.attrsMap.end())
        {
            const uint32_t addr = getAddress();

            std::vector<uint8_t> data;
            PZigbeePacket packet = std::make_shared<ZigbeePacket>(data);

            packet->setSenderAddress(_physicalInterface->getAddress());
            packet->setDestinationAddress(addr);

            ZigbeeCommands::ZCLFrame zclFrame;

            // get for an attribute, simpler
            zclFrame.SetGlobal(true);
            zclFrame.SetDirection(false); // from client to server!

            zclFrame.cmd = 0x00; // read attr

            zclFrame.payload.resize(2);

            ZigbeeCommands::MTCmd::WriteUint16(0, attrId, zclFrame.payload);

            packet->setClusterAndZCLData(clusterId, zclFrame.GetEncoded());

            return packet;
        }
        else
            GD::out.printDebug("Debug: Attribute not found: 0x" + BaseLib::HelperFunctions::getHexString(attrId) + " in cluster: 0x" + BaseLib::HelperFunctions::getHexString(clusterId));
    }
    else
        GD::out.printDebug("Debug: Cluster not found: 0x" + BaseLib::HelperFunctions::getHexString(clusterId));

    PZigbeePacket packet;

    return packet;
}

PVariable ZigbeePeer::getValueFromDevice(std::shared_ptr<Parameter>& parameter, int32_t channel, bool asynchronous)
{
    try
    {
        if(!parameter)
            return Variable::createError(-32500, "parameter is nullptr.");

        bool isConfig = false;

        std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator channelIterator = valuesCentral.find(channel);
        if(channelIterator == valuesCentral.end())
        {
            channelIterator = configCentral.find(channel);
            if (channelIterator == configCentral.end())
                return Variable::createError(-2, "Unknown channel.");
            else
                isConfig = true;
        }

        std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator valuesIterator = channelIterator->second.find(parameter->id);
        if(!isConfig && (valuesIterator == valuesCentral[channel].end() || !valuesIterator->second.rpcParameter))
        {
            // maybe it's a config value, try to find it
            channelIterator = configCentral.find(channel);
            if (channelIterator == configCentral.end() || !valuesIterator->second.rpcParameter)
                return Variable::createError(-5, "Unknown parameter.");

            valuesIterator = channelIterator->second.find(parameter->id);
            if (valuesIterator == configCentral[channel].end() || !valuesIterator->second.rpcParameter)
                return Variable::createError(-5, "Unknown parameter.");
            else
                isConfig = true;
        }
        else if (isConfig && (valuesIterator == configCentral[channel].end() || !valuesIterator->second.rpcParameter))
            return Variable::createError(-5, "Unknown parameter.");

        bool wakeupDev = wakeupDevice();

        if (wakeupDev && !asynchronous)
            return Variable::createError(-6, "Parameter can't be requested actively, because the device isn't reachable all the time.");

        std::vector<std::shared_ptr<Parameter::Packet>> getRequests;
        std::vector<std::shared_ptr<Parameter::Packet>> getRequestsXml; // xml file ones

        // TODO: set the actual address!
        const uint32_t addr = getAddress();

        for(std::vector<std::shared_ptr<Parameter::Packet>>::iterator i = parameter->getPackets.begin(); i != parameter->getPackets.end(); ++i)
        {
            PacketsById::iterator packetIterator = _rpcDevice->packetsById.find((*i)->id);
            if (packetIterator == _rpcDevice->packetsById.end())
            {
                GD::out.printError("Error: Get packet with ID \"" + (*i)->id + "\" not found.");
                continue;
            }

            // uncomment this later when ZDO is implemented
            std::shared_ptr<ZigbeeDescPacket> p = std::dynamic_pointer_cast<ZigbeeDescPacket>(packetIterator->second);
            if (p)
                getRequests.push_back(*i);
            else if (BaseLib::DeviceDescription::Packet::Direction::toCentral != packetIterator->second->direction) // ignore packets that are marked as incoming, they cannot be 'get'
                getRequestsXml.push_back(*i);
        }

        if (getRequests.empty() && getRequestsXml.empty())
            return Variable::createError(-6, "No frame was found for parameter " + parameter->id);

        // *******************************************

        std::unique_lock<std::mutex> getValueLock(_getValueFromDeviceInfo.getValueMutex, std::defer_lock);
        std::unique_lock<std::mutex> accessLock(_getValueFromDeviceInfo.accessMutex, std::defer_lock);
        if (!asynchronous)
        {
            getValueLock.lock();
            accessLock.lock();
            _getValueFromDeviceInfo.channel = channel;
            _getValueFromDeviceInfo.variableName = parameter->id;
            _getValueFromDeviceInfo.requested = true;
            _getValueFromDeviceInfo.mutexReady = false;
            _getValueFromDeviceInfo.value = std::make_shared<Variable>(VariableType::tVoid);
            accessLock.unlock();
        }


        for(std::shared_ptr<Parameter::Packet> getRequest : getRequestsXml)
        {
            PacketsById::iterator packetIterator = _rpcDevice->packetsById.find(getRequest->id);
            if(packetIterator == _rpcDevice->packetsById.end())
                return Variable::createError(-6, "No frame was found for parameter " + parameter->id);
            PPacket frame = packetIterator->second;

            if(frame->subtype == -1)
                frame->subtype = 1;

            std::vector<uint8_t> data;

            PZigbeePacket packet = std::make_shared<ZigbeePacket>(data);

            packet->setSenderAddress(_physicalInterface->getAddress());
            packet->setDestinationAddress(addr);
            //packet->expectsResponse = true; // set by the sending code anyway
            //packet->expectsRequestedResponse = asynchronous; // set by the sending code anyway

            for(BinaryPayloads::iterator i = frame->binaryPayloads.begin(); i != frame->binaryPayloads.end(); ++i)
            {
                if((*i)->constValueInteger > -1)
                {
                    std::vector<uint8_t> dataBin;
                    const unsigned int requiredSize = (*i)->bitSize / 8 + (((*i)->bitSize & 7) != 0 ? 1 : 0);
                    dataBin.resize(requiredSize, 0);
                    //memcpyLittleEndian(dataBin, (*i)->constValueInteger);
                    _bl->hf.memcpyBigEndian(dataBin, (*i)->constValueInteger);
                    packet->setBitPosition((*i)->bitIndex, (*i)->bitSize, dataBin);
                    continue;
                }

                //We can't just search for param, because it is ambiguous
                if((*i)->parameterId == parameter->physical->groupId)
                {
                    std::vector<uint8_t> dataBin = valuesIterator->second.getBinaryData();
                    packet->setBitPosition((*i)->bitIndex, (*i)->bitSize, dataBin);
                }
                //Search for all other parameters
                else
                {
                    bool paramFound = false;
                    for(std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator j = valuesCentral[channel].begin(); j != valuesCentral[channel].end(); ++j)
                    {
                        //Only compare id. Till now looking for value_id was not necessary.
                        if((*i)->parameterId == j->second.rpcParameter->physical->groupId)
                        {
                            std::vector<uint8_t> dataBin = j->second.getBinaryData();
                            packet->setBitPosition((*i)->bitIndex, (*i)->bitSize, dataBin);
                            paramFound = true;
                            break;
                        }
                    }
                    if(!paramFound)
                        GD::out.printError("Error constructing packet. param \"" + (*i)->parameterId + "\" not found. Peer: " + std::to_string(_peerID) + " Serial number: " + _serialNumber + " Frame: " + frame->id);
                }
            }

            _physicalInterface->enqueuePacket(packet, wakeupDev);
        }

        // construct the 'get' packet:

        if (!getRequests.empty())
        {
            if (getRequests.size() > 1)
                GD::out.printDebug("More than one get request found for a value");

            PZigbeePacket packet = ConstructGetOrSetPacket(addr, getRequests[0], parameter, channelIterator, true);

            if (!packet)
                return Variable::createError(-6, "No frame was found for parameter " + parameter->metadata);

            _physicalInterface->enqueuePacket(packet, wakeupDev);
        }

        if (!getRequests.empty() || !getRequestsXml.empty())
        {
            _physicalInterface->tryToSend(addr, wakeupDev);
        }

        if (asynchronous)
            return std::make_shared<Variable>(VariableType::tVoid);

        // wait for the packet to be sent

        auto when = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);

        std::unique_lock<std::mutex> conditionLock(_getValueFromDeviceInfo.conditionMutex);

        for(;;)
        {
            if (_getValueFromDeviceInfo.conditionVariable.wait_until(conditionLock, when) == std::cv_status::timeout)
            {
                break;
            }
            else if (_getValueFromDeviceInfo.mutexReady)
            {
                break;
            }
            // else it's spurious, ignore it
        }

        if (!_getValueFromDeviceInfo.mutexReady)
        {
            GD::out.printDebug("Debug: Waiting for device response failed");

            return std::make_shared<Variable>(VariableType::tVoid);
        }

        return _getValueFromDeviceInfo.value;
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return Variable::createError(-32500, "Unknown application error.");
}

PVariable ZigbeePeer::getChannelZeroVariable(std::string valueKey)
{
    if (valueKey == "ROUTER")
    {
        std::lock_guard<std::mutex> lock(infoMutex);
        return std::make_shared<Variable>(nodeInfo.IsRouter());
    }
    else if (valueKey == "MAINS_POWERED")
    {
        std::lock_guard<std::mutex> lock(infoMutex);
        return std::make_shared<Variable>(nodeInfo.IsMainsPowerDevice());
    }
    else if (valueKey == "LISTENING")
    {
        std::lock_guard<std::mutex> lock(infoMutex);
        return std::make_shared<Variable>(nodeInfo.IsReceiverWhenIdleDevice());
    }
    else if (valueKey == "CONFIG_PENDING")
    {
        return std::make_shared<Variable>(isConfigPending());
    }
    else if (valueKey == "SHORT_ADDR")
    {
        const uint32_t addr = getAddress();
        const uint16_t shortAddr = addr & 0xFFFF;
        return std::make_shared<Variable>(shortAddr);
    }
    else if (valueKey == "END_POINT")
    {
        return std::make_shared<Variable>(endPoint);
    }

    PVariable empty;

    return empty;
}


PVariable ZigbeePeer::getValue(BaseLib::PRpcClientInfo clientInfo, uint32_t channel, std::string valueKey, bool requestFromDevice, bool asynchronous)
{
    try
    {
        if(_disposing)
            return Variable::createError(-32500, "Peer is disposing.");
        else if(!_rpcDevice)
            return Variable::createError(-32500, "Unknown application error.");
        else if(valueKey.empty())
            return Variable::createError(-5, "Value key is empty.");

        if (0 == channel)
        {
            PVariable value = getChannelZeroVariable(valueKey);

            if (value)
                return value;
        }

        std::shared_ptr<ZigbeeCentral> central = std::dynamic_pointer_cast<ZigbeeCentral>(getCentral());
        if(!central)
            return Variable::createError(-32500, "Could not get central object.");;

        bool isConfig = false;

        std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator channelIterator = valuesCentral.find(channel);
        if(channelIterator == valuesCentral.end())
        {
            channelIterator = configCentral.find(channel);
            if (channelIterator == configCentral.end())
                return Variable::createError(-2, "Unknown channel.");
            else
                isConfig = true;
        }

        std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator parameterIterator = channelIterator->second.find(valueKey);

        // the 0 channel might have both config and regular values
        if (0 == channel && parameterIterator == channelIterator->second.end())
        {
            if (isConfig)
                channelIterator = valuesCentral.find(0);
            else
                channelIterator = configCentral.find(0);

            isConfig = !isConfig;
        }

        if(!isConfig && parameterIterator == channelIterator->second.end())
        {
            // maybe it's a config value, try to find it
            channelIterator = configCentral.find(channel);
            if (channelIterator == configCentral.end())
                return Variable::createError(-5, "Unknown parameter.");

            parameterIterator = channelIterator->second.find(valueKey);
            if (parameterIterator == channelIterator->second.end())
                return Variable::createError(-5, "Unknown parameter.");
            else
                isConfig = true;
        }
        else if (isConfig && parameterIterator == channelIterator->second.end())
            return Variable::createError(-5, "Unknown parameter.");

        //Check if channel still exists in device description
        Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
        if(functionIterator == _rpcDevice->functions.end())
            return Variable::createError(-2, "Unknown channel (2).");

        PParameterGroup parameterGroup = getParameterSet(channel, isConfig ? ParameterGroup::Type::Enum::config : ParameterGroup::Type::Enum::variables);
        PParameter parameter = parameterGroup->parameters.at(valueKey);
        if(!parameter)
            return Variable::createError(-5, "Unknown parameter.");


        if(!parameter->readable)
            return Variable::createError(-6, "Parameter is not readable.");


        if (1 == channel && valueKey == "RGB")
        {
            PVariable result = getValue(clientInfo, 1, "LEVEL", requestFromDevice, asynchronous);
            if(result->errorStruct) return result;
            result = getValue(clientInfo, 1, "SATURATION", requestFromDevice, asynchronous);
            if(result->errorStruct) return result;
            result =  getValue(clientInfo, 1, "HUE", requestFromDevice, asynchronous);
            if(result->errorStruct) return result;
        }

        BaseLib::PVariable variable;

        if(requestFromDevice && !parameter->getPackets.empty())
        {
            if(GD::bl->debugLevel >= 4)
                GD::out.printInfo( "Info: getValue request from device");

            variable = getValueFromDevice(parameter, channel, asynchronous);
            if(parameter->password)
                variable = std::make_shared<Variable>(variable->type);
            if((!asynchronous && variable->type != VariableType::tVoid) || variable->errorStruct)
                return variable;
        }

        variable = parameterIterator->second.getLogicalData();

        if (!variable || variable->type == VariableType::tVoid)
        {
            if(GD::bl->debugLevel >= 4)
                GD::out.printInfo( "Info: getValue trying to convert from binary data");
            std::vector<uint8_t> data = parameterIterator->second.getBinaryData();
            variable = parameter->convertFromPacket(data, parameterIterator->second.mainRole(), false);

            if (!variable || variable->type == VariableType::tVoid)
                variable = parameter->logical->getDefaultValue();
        }

        if(parameter->password)
            variable = std::make_shared<Variable>(variable->type);

        return variable;
    }
    catch(const std::exception& ex)
    {
        _bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return Variable::createError(-32500, "Unknown application error.");
}

void ZigbeePeer::AddSetPacketsToMap(std::map<std::string, std::shared_ptr<Parameter::Packet>>& setPacketsMap, std::map<std::string, std::shared_ptr<Parameter>>& setParametersMap, const Struct::iterator& variable, BaseLib::Systems::RpcConfigurationParameter& parameter)
{
    bool hasSetPacket = false;
    for(std::vector<std::shared_ptr<Parameter::Packet>>::iterator p = parameter.rpcParameter->setPackets.begin(); p != parameter.rpcParameter->setPackets.end(); ++p)
    {
        if((*p)->conditionOperator != Parameter::Packet::ConditionOperator::none)
        {
            int32_t intValue = variable->second->integerValue;
            if(parameter.rpcParameter->logical->type == BaseLib::DeviceDescription::ILogical::Type::Enum::tBoolean)
                intValue = variable->second->booleanValue;
            if(!(*p)->checkCondition(intValue))
                continue;
        }
        setPacketsMap[(*p)->id] = *p;
        setParametersMap[(*p)->id] = parameter.rpcParameter;

        hasSetPacket = true;
    }

    if (!hasSetPacket)
        for(std::vector<std::shared_ptr<Parameter::Packet>>::iterator p = parameter.rpcParameter->eventPackets.begin(); p != parameter.rpcParameter->eventPackets.end(); ++p)
        {
            if((*p)->conditionOperator != Parameter::Packet::ConditionOperator::none)
            {
                int32_t intValue = variable->second->integerValue;
                if(parameter.rpcParameter->logical->type == BaseLib::DeviceDescription::ILogical::Type::Enum::tBoolean)
                    intValue = variable->second->booleanValue;
                if(!(*p)->checkCondition(intValue))
                    continue;
            }
            setPacketsMap[(*p)->id] = *p;
            setParametersMap[(*p)->id] = parameter.rpcParameter;
        }
}

PVariable ZigbeePeer::putParamset(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, PVariable variables, bool checkAcls, bool onlyPushing)
{
    if(GD::bl->debugLevel >= 6)
        GD::out.printInfo("Info: Entering putParamset...");

    try
    {
        if(_disposing)
            return Variable::createError(-32500, "Peer is disposing.");
        if(channel < 0)
            channel = 0;
        if(remoteChannel < 0)
            remoteChannel = 0;

        Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
        if(functionIterator == _rpcDevice->functions.end())
            return Variable::createError(-2, "Unknown channel.");
        if(type == ParameterGroup::Type::none)
            type = ParameterGroup::Type::link;
        PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(type);
        if(!parameterGroup)
            return Variable::createError(-3, "Unknown parameter set.");
        if(variables->structValue->empty())
            return std::make_shared<Variable>(VariableType::tVoid);

        auto central = getCentral();
        if(!central)
            return Variable::createError(-32500, "Could not get central.");

        std::map<std::string, std::shared_ptr<Parameter::Packet>> setPacketsMap;
        std::map<std::string, std::shared_ptr<Parameter>> setParametersMap;

        std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator channelIterator;

        bool wakeupDev = wakeupDevice();

        if(type == ParameterGroup::Type::Enum::config)
        {
            if(GD::bl->debugLevel >= 6)
                GD::out.printInfo("Info: putParamset: CONFIG");

            channelIterator = configCentral.find(channel);
            if(channelIterator == configCentral.end())
                return Variable::createError(-2, "Unknown channel.");

            bool anyKnown = false;
            for(Struct::iterator i = variables->structValue->begin(); i != variables->structValue->end(); ++i)
            {
                if(i->first.empty() || !i->second)
                    continue;
                else if(configCentral[channel].find(i->first) == configCentral[channel].end())
                    continue;
                anyKnown = true; // found one, break out
                break;
            }
            if (!anyKnown)
                return Variable::createError(-5, "Unknown parameter.");


            bool atLeastOneGoodResult = false;
            PVariable lastError;

            for(Struct::iterator i = variables->structValue->begin(); i != variables->structValue->end(); ++i)
            {
                if(i->first.empty() || !i->second)
                    continue;
                if(configCentral[channel].find(i->first) == configCentral[channel].end())
                    continue;

                BaseLib::Systems::RpcConfigurationParameter& parameter = configCentral[channel][i->first];
                if(!parameter.rpcParameter)
                    continue;
                if(parameter.rpcParameter->password && i->second->stringValue.empty())
                    continue; //Don't safe password if empty

                PVariable result = setValue(clientInfo, channel, i->first, i->second, false, false);
                if (result)
                {
                    if (result->errorStruct)
                        lastError = result;
                    else
                        atLeastOneGoodResult = true;
                }

                AddSetPacketsToMap(setPacketsMap, setParametersMap, i, parameter);
            }

            if (atLeastOneGoodResult)
                raiseRPCUpdateDevice(_peerID, channel, _serialNumber + ":" + std::to_string(channel), 0);
            else
                return lastError;
        }
        else if(type == ParameterGroup::Type::Enum::variables)
        {
            if(GD::bl->debugLevel >= 6)
                GD::out.printInfo("Info: putParamset: VARIABLES");

            channelIterator = valuesCentral.find(channel);
            if(channelIterator == valuesCentral.end())
                return Variable::createError(-2, "Unknown channel.");

            bool anyKnown = false;
            for(Struct::iterator i = variables->structValue->begin(); i != variables->structValue->end(); ++i)
            {
                if(i->first.empty() || !i->second)
                    continue;
                else if(valuesCentral[channel].find(i->first) == valuesCentral[channel].end())
                    continue;
                anyKnown = true; // found one, break out
                break;
            }
            if (!anyKnown)
                return Variable::createError(-5, "Unknown parameter.");

            bool atLeastOneGoodResult = false;
            PVariable lastError;

            for(Struct::iterator i = variables->structValue->begin(); i != variables->structValue->end(); ++i)
            {
                if(i->first.empty() || !i->second)
                    continue;

                if(checkAcls && !clientInfo->acls->checkVariableWriteAccess(central->getPeer(_peerID), channel, i->first))
                    continue;

                if(valuesCentral[channel].find(i->first) == valuesCentral[channel].end())
                    continue;
                BaseLib::Systems::RpcConfigurationParameter& parameter = valuesCentral[channel][i->first];
                if(!parameter.rpcParameter)
                    continue;
                if(parameter.rpcParameter->password && i->second->stringValue.empty())
                    continue; //Don't safe password if empty

                PVariable result = setValue(clientInfo, channel, i->first, i->second, false, false);
                if (result)
                {
                    if (result->errorStruct)
                        lastError = result;
                    else
                        atLeastOneGoodResult = true;
                }

                AddSetPacketsToMap(setPacketsMap, setParametersMap, i, parameter);
            }

            if (!atLeastOneGoodResult)
                return lastError;
        }
        else
        {
            return Variable::createError(-3, "Parameter set type is not supported.");
        }


        const uint32_t addr = getAddress();

        // TODO: this should be enough if setValue syncs values between static and dynamic definitions, in this case, static->dynamic is needing sync

        for(auto setRequestElem : setPacketsMap)
        {
            std::shared_ptr<Parameter::Packet> setRequest = setRequestElem.second;

            PacketsById::iterator packetIterator = _rpcDevice->packetsById.find(setRequest->id);
            if(packetIterator != _rpcDevice->packetsById.end())
            {
                PPacket frame = packetIterator->second;

                if(frame->subtype == -1)
                    frame->subtype = 1;

                // TODO: construct a 'set' packet

                PZigbeePacket packet = ConstructGetOrSetPacket(addr, setRequest, setParametersMap[setRequestElem.first], channelIterator, false);

                if (!packet)
                    return Variable::createError(-6, "No frame was found for parameter " + setParametersMap[setRequestElem.first]->metadata);

                _physicalInterface->enqueuePacket(packet, wakeupDev);
            }
        }

        if (!setPacketsMap.empty())
        {
            _physicalInterface->tryToSend(addr, wakeupDev);
        }

        return std::make_shared<Variable>(VariableType::tVoid);
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return Variable::createError(-32500, "Unknown application error.");
}

void ZigbeePeer::WaitForPacket(const PZigbeePacket& packet, bool& packetSent, bool& receivedAck)
{
    // wait until the packet is sent but up to 30 seconds
    for (int i = 0; i < 150; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (packet->sendCount > 0)
        {
            packetSent = true;
            break;
        }
    }

    // wait up to 3 seconds
    for (int i = 0; i < 15; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (packet->ack > 0)
        {
            receivedAck = true;
            break;
        }
    }
}

PVariable ZigbeePeer::setValue(BaseLib::PRpcClientInfo clientInfo, uint32_t channel, std::string valueKey, PVariable value, bool wait, bool sendPackets)
{
    try
    {
        if(GD::bl->debugLevel >= 4)
            GD::out.printInfo("Info: Entering SetValue...");

        if(_disposing)
            return Variable::createError(-32500, "Peer is disposing.");

        if(valueKey.empty())
            return Variable::createError(-5, "Value key is empty.");

        if(!clientInfo)
            clientInfo = std::make_shared<BaseLib::RpcClientInfo>();

        if (BaseLib::VariableType::tBoolean == value->type)
        {
            value->type = BaseLib::VariableType::tInteger;
            value->integerValue = (value->booleanValue ? 0x1 : 0x0);
        }

        BaseLib::VariableType savedType = value->type; // setValue might change the type, so save it first

        const bool wakeupDev = wakeupDevice();
        if (wakeupDev) wait = false; // don't wait if it's a wakeup device

        Peer::setValue(clientInfo, channel, valueKey, value, wait); //Ignore result, otherwise setHomegerValue might not be executed

        // if the type was changed, convert the value, too

        if (savedType != value->type)
        {
            if (BaseLib::VariableType::tFloat == value->type)
            {
                if (BaseLib::VariableType::tInteger == savedType)
                    value->floatValue = value->integerValue;
                else if (BaseLib::VariableType::tInteger64 == savedType)
                    value->floatValue = value->integerValue64;
            }
            else if (BaseLib::VariableType::tString == savedType)
            {
                if (BaseLib::VariableType::tFloat == value->type)
                    value->floatValue = std::stod(value->stringValue);
                else
                {
                    bool converted = false;
                    if (value->stringValue.size() > 1 && value->stringValue[0] == '#')
                    {
                        const std::string val = value->stringValue.substr(1);

                        value->integerValue = std::stoi(val, 0, 16);
                        value->integerValue64 = std::stoll(val, 0, 16);

                        converted = true;
                    }
                    else if (value->stringValue.size() > 2)
                    {
                        const std::string pre = value->stringValue.substr(0, 2);
                        if (pre == "0x" || pre == "0X")
                        {
                            value->integerValue = std::stoi(value->stringValue, 0, 16);
                            value->integerValue64 = std::stoll(value->stringValue, 0, 16);

                            converted = true;
                        }
                        else if (1 == channel && valueKey == "RGB" && value->stringValue.find(',') != std::string::npos) // check this only if it's the special RGB value
                        {
                            std::stringstream sstr(value->stringValue);

                            int r, g, b;
                            bool rconv, gconv, bconv;

                            r = g = b = 0;
                            rconv = gconv = bconv = false;

                            std::string val;
                            if (std::getline(sstr, val, ','))
                            {
                                r = std::stoi(val);
                                rconv = true;
                            }
                            if (std::getline(sstr, val, ','))
                            {
                                g = std::stoi(val);
                                gconv = true;
                            }
                            if (std::getline(sstr, val, ','))
                            {
                                b = std::stoi(val);
                                bconv = true;
                            }

                            if (rconv && bconv && gconv && r <= 255 && g <= 255 && b <= 255)
                            {
                                const int rgb = (r << 16) | (g << 8) | b;
                                value->integerValue = rgb;
                                value->integerValue64 = rgb;
                                converted = true;
                            }
                        }
                    }

                    if (!converted)
                    {
                        value->integerValue = std::stoi(value->stringValue);
                        value->integerValue64 = std::stoll(value->stringValue);
                    }
                }
            }
        }

        if (BaseLib::VariableType::tFloat == savedType)
        {
            value->integerValue64 = value->integerValue = int32_t(value->floatValue);
        }


        std::shared_ptr<ZigbeeCentral> central = std::dynamic_pointer_cast<ZigbeeCentral>(getCentral());
        if(!central)
            return Variable::createError(-32500, "Could not get central object.");

        if(0 == channel)
        {
            std::lock_guard<std::mutex> lock(serviceMessagesMutex);
            if (serviceMessages->set(valueKey, value->booleanValue))
                return std::make_shared<Variable>(VariableType::tVoid);
        }

        bool isConfig = false;

        std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator channelIterator = valuesCentral.find(channel);
        if(channelIterator == valuesCentral.end())
        {
            channelIterator = configCentral.find(channel);
            if (channelIterator == configCentral.end())
                return Variable::createError(-2, "Unknown channel.");
            else
                isConfig = true;
        }

        std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator parameterIterator = channelIterator->second.find(valueKey);
        if(!isConfig && parameterIterator == valuesCentral[channel].end())
        {
            // maybe it's a config value, try to find it
            channelIterator = configCentral.find(channel);
            if (channelIterator == configCentral.end())
                return Variable::createError(-5, "Unknown parameter.");

            parameterIterator = channelIterator->second.find(valueKey);
            if (parameterIterator == configCentral[channel].end())
                return Variable::createError(-5, "Unknown parameter.");
            else
                isConfig = true;
        }
        else if (isConfig && parameterIterator == configCentral[channel].end())
            return Variable::createError(-5, "Unknown parameter.");


        BaseLib::Systems::RpcConfigurationParameter& parameter = parameterIterator->second;

        PParameter rpcParameter = parameter.rpcParameter;
        if(!rpcParameter)
            return Variable::createError(-5, "Unknown parameter.");

        if(rpcParameter->logical->type == ILogical::Type::tAction && !value->booleanValue)
            return Variable::createError(-5, "Parameter of type action cannot be set to \"false\".");

        std::map<uint32_t, std::shared_ptr<std::vector<std::string>>> valueKeys;
        std::map<uint32_t, std::shared_ptr<std::vector<PVariable>>> rpcValues;

        //std::shared_ptr<std::vector<std::string>> valueKeys = std::make_shared<std::vector<std::string>>();
        //std::shared_ptr<std::vector<PVariable>> values = std::make_shared<std::vector<PVariable>>();

        if(rpcParameter->readable)
            AddValueForEvents(valueKeys, rpcValues, channel, valueKey, value);

        if (1 == channel && valueKey == "RGB")
        {
            // TODO: some light bulbs might expose only X and Y values, not HUE and SATURATION
            // maybe check what's exposed and deal with them accordingly
            // also hue and saturation could be set in one command packet (move to hue saturation) instead of setting the values separately
            // this would require going to a lower level here to make that packet
            // or search for the channel that exposes the COLOR_CONTROL.MOVE_TO_HUE_AND_SATURATION.HUE, COLOR_CONTROL.MOVE_TO_HUE_AND_SATURATION.SATURATION and COLOR_CONTROL.MOVE_TO_HUE_AND_SATURATION.TRANSITION_TIME
            // and set them all (setting the last one triggers sending the packet)

            const uint32_t irgb = static_cast<uint32_t>(value->integerValue);
            const uint8_t red = ((irgb >> 16) & 0xFF);
            const uint8_t green = ((irgb >> 8) & 0xFF);
            const uint8_t blue = (irgb & 0xFF);

            BaseLib::Color::RGB cRGB(red, green, blue);
            BaseLib::Color::NormalizedRGB nRGB(cRGB);
            BaseLib::Color::HSV hsv = nRGB.toHSV();

            unsigned int commandChannel = 2;
            bool found = false;
            for (;commandChannel < valuesCentral.size(); ++commandChannel)
            {
                if (valuesCentral[commandChannel].find("COLOR_CONTROL.MOVE_TO_HUE_AND_SATURATION.HUE") != valuesCentral[commandChannel].end())
                {
                    found = true;
                    break;
                }
            }

            const uint8_t brightness = std::lround(hsv.getBrightness() * 254.0);
            const int32_t hue = std::lround(hsv.getHue() / 360. * 254.);
            const uint8_t saturation = std::lround(hsv.getSaturation() * 254.0);

            PVariable result;

            if (found)
            {
                result = setValue(clientInfo, commandChannel, "COLOR_CONTROL.MOVE_TO_HUE_AND_SATURATION.HUE", PVariable(new Variable(hue)), false, false);
                if(result->errorStruct) return result;
                result = setValue(clientInfo, commandChannel, "COLOR_CONTROL.MOVE_TO_HUE_AND_SATURATION.SATURATION", PVariable(new Variable((int32_t)saturation)), false, false);
                if(result->errorStruct) return result;
                result = setValue(clientInfo, commandChannel, "COLOR_CONTROL.MOVE_TO_HUE_AND_SATURATION.TRANSITION_TIME", PVariable(new Variable(0)), wait, sendPackets);
                if(result->errorStruct) return result;
            }
            else
            {
                result = setValue(clientInfo, 1, "HUE", PVariable(new Variable(hue)), wait, sendPackets);
                if(result->errorStruct) return result;

                // some light bulbs do not like being sent commands immediately one after another, so wait a little
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                result = setValue(clientInfo, 1, "SATURATION", PVariable(new Variable((int32_t)saturation)), wait, sendPackets);
                if(result->errorStruct) return result;
            }

            // some light bulbs do not like being sent commands immediately one after another, so wait a little
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            result = setValue(clientInfo, 1, "LEVEL", PVariable(new Variable((int32_t)brightness)), wait, sendPackets);
            if(result->errorStruct) return result;

            //Convert back, because the value might be different than the passed one.
            cRGB = hsv.toRGB();

            std::vector<uint8_t> parameterData{cRGB.getRed(), cRGB.getGreen(), cRGB.getBlue()};

            parameter.setLogicalData(value);

            if (parameter.rpcParameter && parameter.hasRoles())
                parameter.rpcParameter->convertToPacket(value, parameter.mainRole(), parameterData);

            parameter.setBinaryData(parameterData);

            if(parameter.databaseId > 0) saveParameter(parameter.databaseId, parameterData);
            else saveParameter(0, ParameterGroup::Type::Enum::variables, channel, valueKey, parameterData);

            AddValueForEvents(valueKeys, rpcValues, channel, valueKey, value);

            std::string address(_serialNumber + ":" + std::to_string(channel));

            raiseEvent(clientInfo->initInterfaceId, _peerID, channel, valueKeys[channel], rpcValues[channel]);
            raiseRPCEvent(clientInfo->initInterfaceId, _peerID, channel, address, valueKeys[channel], rpcValues[channel]);

            return PVariable(new Variable(VariableType::tVoid));
        }

        if(rpcParameter->physical->operationType != IPhysical::OperationType::Enum::store && rpcParameter->physical->operationType != IPhysical::OperationType::Enum::command)
            return Variable::createError(-6, "Parameter is not settable.");
        else if(rpcParameter->setPackets.empty() && rpcParameter->eventPackets.empty() && rpcParameter->getPackets.empty())
            return Variable::createError(-6, "parameter is read only");

        std::vector<std::shared_ptr<Parameter::Packet>> setRequests; // the 'dynamic' ones
        std::vector<std::shared_ptr<Parameter::Packet>> setRequestsXml; // xml file ones

        std::vector<std::shared_ptr<Parameter::Packet>> getRequests; // allow setting here the 'get' values, too

        bool firstTime = true;
        std::vector<uint8_t> physicalValue;

        for(std::vector<std::shared_ptr<Parameter::Packet>>::iterator i = rpcParameter->setPackets.begin(); i != rpcParameter->setPackets.end(); ++i)
        {
            PacketsById::iterator packetIterator = _rpcDevice->packetsById.find((*i)->id);
            if(packetIterator == _rpcDevice->packetsById.end())
            {
                GD::out.printDebug("Debug: Could not find a packet for: " + (*i)->id);
                continue;
            }

            std::shared_ptr<ZigbeeDescPacket> p = std::dynamic_pointer_cast<ZigbeeDescPacket>(packetIterator->second);
            if (p)
            {
                if((*i)->conditionOperator != Parameter::Packet::ConditionOperator::none)
                {
                    int32_t intValue = value->integerValue;
                    if(parameter.rpcParameter->logical->type == BaseLib::DeviceDescription::ILogical::Type::Enum::tBoolean)
                        intValue = value->booleanValue;

                    if(!(*i)->checkCondition(intValue))
                        continue;
                }

                setRequests.push_back(*i);
            }
            else if (BaseLib::DeviceDescription::Packet::Direction::toCentral != packetIterator->second->direction) // ignore packets that are marked as incoming, those cannot be 'set' packets
            {
                if (firstTime)
                {
                    rpcParameter->convertToPacket(value, parameter.mainRole(), physicalValue);
                    firstTime = false;
                }
                if((*i)->conditionOperator != Parameter::Packet::ConditionOperator::none)
                {
                    int32_t intValue = 0;
                    //memcpyLittleEndian(intValue, physicalValue);
                    _bl->hf.memcpyBigEndian(intValue, physicalValue);
                    if(!(*i)->checkCondition(intValue))
                        continue;
                }
                setRequestsXml.push_back(*i);
            }
        }

        // allow sending event packets, too
        for(std::vector<std::shared_ptr<Parameter::Packet>>::iterator i = rpcParameter->eventPackets.begin(); i != rpcParameter->eventPackets.end(); ++i)
        {
            PacketsById::iterator packetIterator = _rpcDevice->packetsById.find((*i)->id);
            if(packetIterator == _rpcDevice->packetsById.end())
            {
                GD::out.printError("Error: Get/event packet with ID \"" + (*i)->id + "\" not found.");
                continue;
            }

            std::shared_ptr<ZigbeeDescPacket> p = std::dynamic_pointer_cast<ZigbeeDescPacket>(packetIterator->second);
            if (p)
            {
                if((*i)->conditionOperator != Parameter::Packet::ConditionOperator::none)
                {
                    int32_t intValue = value->integerValue;
                    if(parameter.rpcParameter->logical->type == BaseLib::DeviceDescription::ILogical::Type::Enum::tBoolean)
                        intValue = value->booleanValue;
                    if(!(*i)->checkCondition(intValue))
                        continue;
                }

                if (setRequests.empty())
                    setRequests.push_back(*i);
            }
            else if (BaseLib::DeviceDescription::Packet::Direction::toCentral != packetIterator->second->direction) // ignore packets that are marked as incoming, those cannot be 'set' packets
            {
                if (firstTime)
                {
                    rpcParameter->convertToPacket(value, parameter.mainRole(), physicalValue);
                    firstTime = false;
                }
                if((*i)->conditionOperator != Parameter::Packet::ConditionOperator::none)
                {
                    int32_t intValue = 0;
                    //memcpyLittleEndian(intValue, physicalValue);
                    _bl->hf.memcpyBigEndian(intValue, physicalValue);
                    if(!(*i)->checkCondition(intValue))
                        continue;
                }
                setRequestsXml.push_back(*i);
            }
        }


        for(std::vector<std::shared_ptr<Parameter::Packet>>::iterator i = rpcParameter->getPackets.begin(); i != rpcParameter->getPackets.end(); ++i)
        {
            PacketsById::iterator packetIterator = _rpcDevice->packetsById.find((*i)->id);
            if(packetIterator == _rpcDevice->packetsById.end())
            {
                GD::out.printError("Error: Get packet with ID \"" + (*i)->id + "\" not found.");
                continue;
            }

            std::shared_ptr<ZigbeeDescPacket> p = std::dynamic_pointer_cast<ZigbeeDescPacket>(packetIterator->second);
            if (p)
            {
                if((*i)->conditionOperator != Parameter::Packet::ConditionOperator::none)
                {
                    int32_t intValue = value->integerValue;
                    if(parameter.rpcParameter->logical->type == BaseLib::DeviceDescription::ILogical::Type::Enum::tBoolean)
                        intValue = value->booleanValue;
                    if(!(*i)->checkCondition(intValue))
                        continue;
                }

                if (getRequests.empty())
                    getRequests.push_back(*i);
            }
        }


        bool isBitmaskParameter = false;
        const std::shared_ptr<ZigbeeParameter> zBitfieldParameter = std::dynamic_pointer_cast<ZigbeeParameter>(rpcParameter);
        if (!(zBitfieldParameter && zBitfieldParameter->deviceParameter.isBitmaskField))
            isBitmaskParameter = true;

        if(setRequests.empty() && setRequestsXml.empty() && getRequests.empty() && rpcParameter->physical->operationType == IPhysical::OperationType::Enum::store)
        {
            parameter.setLogicalData(value);

            parameter.lock();
            try
            {
                rpcParameter->convertToPacket(value, parameter.mainRole(), parameter.getBinaryDataReference());
            }
            catch(...) {}
            parameter.unlock();

            std::vector<uint8_t> data = parameter.getBinaryData();
            if(parameter.databaseId > 0)
                saveParameter(parameter.databaseId, data);
            else
                saveParameter(0, isConfig ? ParameterGroup::Type::Enum::config : ParameterGroup::Type::Enum::variables, channel, valueKey, data);

            if(valueKeys[channel] && !valueKeys[channel]->empty())
            {
                std::string address(_serialNumber + ":" + std::to_string(channel));
                raiseEvent(clientInfo->initInterfaceId, _peerID, channel, valueKeys[channel], rpcValues[channel]);
                raiseRPCEvent(clientInfo->initInterfaceId, _peerID, channel, address, valueKeys[channel], rpcValues[channel]);
            }

            if (!isBitmaskParameter)
                return std::make_shared<Variable>(VariableType::tVoid);
        }


        if(setRequests.empty() && setRequestsXml.empty() && getRequests.empty())
        {
            if (isBitmaskParameter && zBitfieldParameter) // is it really bitmask?
            {
                // the value for the

                std::string realParamName = zBitfieldParameter->deviceParameter.clusterInfo->name + ".";
                if (zBitfieldParameter->deviceParameter.attrInfo)
                    realParamName += zBitfieldParameter->deviceParameter.attrInfo->name;
                else
                    realParamName += zBitfieldParameter->deviceParameter.paramInfo->name;

                std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator pit = channelIterator->second.find(realParamName);
                if (pit != channelIterator->second.end() && (value->type == BaseLib::VariableType::tBoolean || value->type == BaseLib::VariableType::tInteger || value->type == BaseLib::VariableType::tInteger64))
                {
                    uint32_t val = value->type == BaseLib::VariableType::tInteger ? value->integerValue : (value->type == BaseLib::VariableType::tBoolean ? (true == value->booleanValue ? 0x1 : 0x0) : uint32_t(value->integerValue64));

                    uint8_t shifter = 0;
                    uint32_t mask = 0;
                    bool found = false;

                    for (const auto& bitfield : (zBitfieldParameter->deviceParameter.attrInfo ? zBitfieldParameter->deviceParameter.attrInfo->bitfields : zBitfieldParameter->deviceParameter.paramInfo->bitfields))
                    {
                        if (bitfield.name == valueKey)
                        {
                            shifter = bitfield.shifter;
                            mask = bitfield.mask;
                            found = true;
                            break;
                        }
                    }

                    if (found)
                    {
                        val <<= shifter;
                        val &= mask;

                        auto& parentparam = pit->second;

                        auto variable = parentparam.getLogicalData();
                        if (variable && variable->type == BaseLib::VariableType::tInteger)
                        {
                            if(_bl->debugLevel >= 6)
                                GD::out.printInfo("Info: SetValue: Trying to set logical value for: " + parentparam.rpcParameter->id);

                            variable->integerValue = (variable->integerValue & ~mask) | val;
                        }
                        else if (!variable)
                        {
                            if(_bl->debugLevel >= 6)
                                GD::out.printInfo("Info: SetValue: Trying to set fresh logical value for: " + parentparam.rpcParameter->id);

                            variable = std::make_shared<BaseLib::Variable>(val);
                        }
                        else
                            GD::out.printDebug("Bitmask variable is not integer: " + valueKey);

                        // this will set the real parameter (it will result also in setting all the corresponding 'fake' ones, but there should be no harm in that)
                        // this is not the same as in the zwave case, where only the changing of the 'fake' parameter is raised, this implementation will also raise the event of changing the real parameter
                        return setValue(clientInfo, channel, realParamName, variable, wait, sendPackets);
                    }
                    else
                        GD::out.printDebug("Couldn't find bitmask for: " + valueKey);
                }
                else
                {
                    if (pit == channelIterator->second.end())
                        GD::out.printDebug("Couldn't find parent parameter for: " + valueKey);
                    else
                        GD::out.printDebug("Unsupported type for: " + valueKey);
                }
            }

            return Variable::createError(-6, "No frame was found for parameter " + valueKey);
        }
        else if (setRequests.size() > 1)
            return Variable::createError(-6, "More than one frame was found for parameter " + valueKey);

        bool packetSent = false;
        bool receivedAck = false;

        const uint32_t addr = getAddress(); // set to peer address

        // from here, deal with xml device defined packets

        if (!setRequestsXml.empty())
        {
            parameter.setLogicalData(value);
            parameter.setBinaryData(physicalValue); // was already converted to packet with main role

            if(parameter.databaseId > 0)
                saveParameter(parameter.databaseId, physicalValue);
            else
                saveParameter(0, isConfig ? ParameterGroup::Type::Enum::config : ParameterGroup::Type::Enum::variables, channel, valueKey, physicalValue);
            if(_bl->debugLevel > 4)
                GD::out.printDebug("Debug: " + valueKey + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":" + std::to_string(channel) + " was set to " + BaseLib::HelperFunctions::getHexString(physicalValue) + ".");

            if(rpcParameter->physical->operationType == IPhysical::OperationType::Enum::store)
            {
                if(valueKeys[channel] && !valueKeys[channel]->empty())
                {
                    std::string address(_serialNumber + ":" + std::to_string(channel));
                    raiseEvent(clientInfo->initInterfaceId, _peerID, channel, valueKeys[channel], rpcValues[channel]);
                    raiseRPCEvent(clientInfo->initInterfaceId, _peerID, channel, address, valueKeys[channel], rpcValues[channel]);
                }
                return PVariable(new Variable(VariableType::tVoid));
            }


            for(std::shared_ptr<Parameter::Packet> setRequest : setRequestsXml)
            {
                PacketsById::iterator packetIterator = _rpcDevice->packetsById.find(setRequest->id);
                if(packetIterator == _rpcDevice->packetsById.end())
                    return Variable::createError(-6, "No frame was found for parameter " + valueKey);
                PPacket frame = packetIterator->second;

                if(frame->subtype == -1)
                    frame->subtype = 1;

                std::vector<uint8_t> data;

                PZigbeePacket packet = std::make_shared<ZigbeePacket>(data);

                packet->setSenderAddress(_physicalInterface->getAddress());
                packet->setDestinationAddress(addr);
                //packet->expectsResponse = true; // set by the sending code anyway
                //packet->expectsRequestedResponse = false; // set by the sending code anyway
                packet->packetID = setRequest->id;

                // count the number of parameters in the frame, it might contain duplicates for odd ones, like XY or RGB for colors
                int numberOfParams = 0;
                for(BinaryPayloads::iterator i = frame->binaryPayloads.begin(); i != frame->binaryPayloads.end(); ++i)
                {
                    if((*i)->constValueInteger > -1)
                        continue;

                    if((*i)->parameterId == rpcParameter->physical->groupId)
                        ++numberOfParams;
                }

                int paramCount = 0;
                for(BinaryPayloads::iterator i = frame->binaryPayloads.begin(); i != frame->binaryPayloads.end(); ++i)
                {
                    if((*i)->constValueInteger > -1)
                    {
                        std::vector<uint8_t> dataBin;
                        const unsigned int requiredSize = (*i)->bitSize / 8 + (((*i)->bitSize & 7) != 0 ? 1 : 0);
                        dataBin.resize(requiredSize, 0);
                        //memcpyLittleEndian(dataBin, (*i)->constValueInteger);
                        _bl->hf.memcpyBigEndian(dataBin, (*i)->constValueInteger);
                        packet->setBitPosition((*i)->bitIndex, (*i)->bitSize, dataBin);
                        GD::out.printInfo("Set bit index: " + std::to_string((*i)->bitIndex) + " bit size: " + std::to_string((*i)->bitSize) + " data: " + BaseLib::HelperFunctions::getHexString(dataBin));
                        continue;
                    }
                    //We can't just search for param, because it is ambiguous
                    if((*i)->parameterId == rpcParameter->physical->groupId)
                    {
                        std::vector<uint8_t> dataBin = parameter.getBinaryData();

                        if (1 == numberOfParams)
                        {
                            packet->setBitPosition((*i)->bitIndex, (*i)->bitSize, dataBin);

                            GD::out.printInfo("Set bit index: " + std::to_string((*i)->bitIndex) + " bit size: " + std::to_string((*i)->bitSize) + " data: " + BaseLib::HelperFunctions::getHexString(dataBin));
                        }
                        else
                        {
                            const int bytes = (*i)->bitSize / 8;
                            const int totalLen = numberOfParams * bytes;

                            const int missing = totalLen - dataBin.size();
                            if (missing > 0)
                                dataBin.insert(dataBin.begin(), missing, 0);

                            std::vector<uint8_t> partialDataBin(dataBin.begin() + paramCount * bytes, dataBin.begin() + (paramCount + 1) * bytes);
                            packet->setBitPosition((*i)->bitIndex, (*i)->bitSize, partialDataBin);

                            GD::out.printInfo("Set bit index: " + std::to_string((*i)->bitIndex) + " bit size: " + std::to_string((*i)->bitSize) + " data: " + BaseLib::HelperFunctions::getHexString(partialDataBin));

                            ++paramCount;
                        }
                    }
                    //Search for all other parameters
                    else
                    {
                        bool paramFound = false;
                        for(std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator j = valuesCentral[channel].begin(); j != valuesCentral[channel].end(); ++j)
                        {
                            //Only compare id. Till now looking for value_id was not necessary.
                            if((*i)->parameterId == j->second.rpcParameter->physical->groupId)
                            {
                                std::vector<uint8_t> dataBin = j->second.getBinaryData();
                                packet->setBitPosition((*i)->bitIndex, (*i)->bitSize, dataBin);
                                GD::out.printInfo("Set bit index: " + std::to_string((*i)->bitIndex) + " bit size: " + std::to_string((*i)->bitSize) + " data: " + BaseLib::HelperFunctions::getHexString(dataBin));
                                paramFound = true;
                                break;
                            }
                        }
                        if(!paramFound)
                            GD::out.printError("Error constructing packet. param \"" + (*i)->parameterId + "\" not found. Peer: " + std::to_string(_peerID) + " Serial number: " + _serialNumber + " Frame: " + frame->id);
                    }
                }

                GD::out.printInfo("Info: Set packet is: " + BaseLib::HelperFunctions::getHexString(packet->payload()));

                // packet was generated from a setValue for a static parameter, parse the packet to sync the static ones that might be affected
                ParsePacketDynamic(packet, valueKeys, rpcValues, false);

                // TODO: what to do if the values are reset afterwards????

                if(!setRequest->autoReset.empty())
                {
                    for(std::vector<std::string>::iterator j = setRequest->autoReset.begin(); j != setRequest->autoReset.end(); ++j)
                    {
                        std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator resetParameterIterator = channelIterator->second.find(*j);
                        if(resetParameterIterator == channelIterator->second.end())
                            continue;
                        PVariable logicalDefaultValue = resetParameterIterator->second.rpcParameter->logical->getDefaultValue();
                        std::vector<uint8_t> defaultValue;
                        resetParameterIterator->second.rpcParameter->convertToPacket(logicalDefaultValue, resetParameterIterator->second.mainRole(), defaultValue);
                        if(!resetParameterIterator->second.equals(defaultValue))
                        {
                            resetParameterIterator->second.setBinaryData(defaultValue);
                            if(resetParameterIterator->second.databaseId > 0)
                                saveParameter(resetParameterIterator->second.databaseId, defaultValue);
                            else
                                saveParameter(0, ParameterGroup::Type::Enum::variables, channel, *j, defaultValue);
                            GD::out.printInfo( "Info: Parameter \"" + *j + "\" was reset to " + BaseLib::HelperFunctions::getHexString(defaultValue) + ". Peer: " + std::to_string(_peerID) + " Serial number: " + _serialNumber + " Frame: " + frame->id);

                            if(rpcParameter->readable)
                                AddValueForEvents(valueKeys, rpcValues, channel, *j, logicalDefaultValue);
                        }
                    }
                }

                if (sendPackets)
                {
                    if(GD::bl->debugLevel >= 4)
                        GD::out.printInfo( "Info: SetValue sending packet...");

                    _physicalInterface->sendPacket(packet, wakeupDev);

                    if(setRequest->delay > 0)
                        std::this_thread::sleep_for(std::chrono::milliseconds(setRequest->delay));

                    if (wait && !wakeupDev)
                        WaitForPacket(packet, packetSent, receivedAck);
                }
            }
        }


        // From here, deal with 'dynamic' packets

        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        // this must be done in a reverse way as when receiving a packet

        if (!setRequests.empty() || !getRequests.empty())
        {
            if(GD::bl->debugLevel >= 4)
                GD::out.printInfo("Info: SetValue, dynamic packets...");

            std::shared_ptr<Parameter::Packet> setRequest = setRequests.empty() ? getRequests[0] : setRequests[0];

            PacketsById::iterator packetIterator = _rpcDevice->packetsById.find(setRequest->id);
            if(packetIterator == _rpcDevice->packetsById.end())
                return Variable::createError(-6, "No frame was found for parameter " + valueKey);
            PPacket frame = packetIterator->second;

            // here there will be a lot of code when ZDO is ready, look into zwave code for the example and implement for zigbee

            parameter.setLogicalData(value);

            const std::shared_ptr<ZigbeeParameter> zParameter = std::dynamic_pointer_cast<ZigbeeParameter>(rpcParameter);
            if (zParameter)
            {
                const DeviceParameter* deviceParameter = &zParameter->deviceParameter;
                ZigbeeType zType(deviceParameter->type);

                parameter.lock();

                try
                {
                    if(_bl->debugLevel >= 6)
                        GD::out.printInfo("Info: SetValue: Trying to set binary data for: " + valueKey);

                    // let's see if it's an enum and the value is a string, in which case convert it to integer
                    if (VariableType::tString == value->type && ((deviceParameter->attrInfo && !deviceParameter->attrInfo->enums.empty()) || (deviceParameter->paramInfo && !deviceParameter->paramInfo->enums.empty())))
                    {
                        if (deviceParameter->attrInfo)
                        {
                            for (const auto& e : deviceParameter->attrInfo->enums)
                            {
                                if (e.name == value->stringValue)
                                {
                                    value->type = VariableType::tInteger;
                                    value->integerValue = e.val;
                                    break;
                                }
                            }
                        }
                        else
                        {
                            for (const auto& e : deviceParameter->paramInfo->enums)
                            {
                                if (e.name == value->stringValue)
                                {
                                    value->type = VariableType::tInteger;
                                    value->integerValue = e.val;
                                    break;
                                }
                            }
                        }
                    }

                    parameter.getBinaryDataReference().clear();
                    bool decoded = zType.SetVariable(value, parameter.getBinaryDataReference());
                    if (!decoded)
                    {
                        // was not set, it's some complex field, deal with it

                        // 'Problem' types:

                        //struct 0x4C - this has structure, there is no such type in the xml clusters yet, should be handled just in case

                        // the following contain possibly more than one value, each possibly having structure, maybe a fixed length, maybe not
                        // the element length might be easily calculable or not
                        // for example, it might have a fixed length that can be computed without the data (a struct that has only fixed length items)
                        // but it might be variable, for example containing a string which has a length that is known only from the data
                        // or it's even worse, it can contain something that by itself does not supply its length (array, set, bag)
                        // usually there is some info preceding that if it's a command (a field like size/length) that provides some info on length (but it's complicated that sometimes it says number of octets, sometimes it says number of elements)

                        //array 0x48 - does not exist in the clusters xml file yet, but should be handled just in case, probably the same as 'bag'

                        // the difference between the two is that the 'set' is supposed to not have duplicate entries
                        // but handle them the same way (also the array) nevertheless

                        //set 0x50
                        //bag 0x51

                        if (deviceParameter->attrInfo || (deviceParameter->paramInfo && deviceParameter->paramInfo->alternField.empty()))
                        {
                            decoded = SetComplexVariableBinary(deviceParameter, value, parameter.getBinaryDataReference());

                            if (deviceParameter->paramInfo && deviceParameter->paramInfo->len > 0)
                            {
                                unsigned int explen = deviceParameter->paramInfo->len;
                                if (deviceParameter->paramInfo->sizeType == 2)
                                    explen *= deviceParameter->paramInfo->GetFixedSize();

                                if (parameter.getBinaryDataReference().size() != explen)
                                    GD::out.printDebug("setValue: A parameter is marked with a length in clusters xml file, problem parameter: " + deviceParameter->paramInfo->name + " expected length (bytes): " + std::to_string(explen) + " actual length: " + std::to_string(parameter.getBinaryDataReference().size()));
                            }
                        }
                        else if (deviceParameter->paramInfo)
                        {
                            if (!deviceParameter->paramInfo->params.empty())
                                GD::out.printDebug("setValue: Looks like an 'altern' parameter has parameters, problem parameter: " + deviceParameter->paramInfo->name + " please fix it in the clusters xml file");

                            if (deviceParameter->paramInfo->alternRecords.empty())
                                GD::out.printDebug("setValue: Looks like a parameter with 'altern' has no 'altern' records, problem parameter: " + deviceParameter->paramInfo->name + " please fix it in the clusters xml file");

                            std::shared_ptr<ZigbeeDescPacket> zPacket = std::dynamic_pointer_cast<ZigbeeDescPacket>(frame);
                            if (zPacket)
                            {
                                const uint8_t cmdId = zPacket->type & 0xFF;
                                bool found = false;
                                auto cmdIt = deviceParameter->commandsReceived.find(cmdId);
                                if (cmdIt == deviceParameter->commandsReceived.end())
                                {
                                    cmdIt = deviceParameter->commandsSent.find(cmdId);
                                    if (cmdIt == deviceParameter->commandsSent.end())
                                        GD::out.printDebug("setValue: Couldn't find the command for parameter: " + deviceParameter->paramInfo->name);
                                    else
                                        found = true;
                                }
                                else
                                    found = true;

                                if (found && cmdIt->second)
                                {
                                    const std::string valueKeyBase = valueKey + cmdIt->second->name + ".";
                                    std::string valueKeyBaseAlt;
                                    if (DeviceParameter::IsResponse(cmdIt->second->name))
                                        valueKeyBaseAlt = valueKey + cmdIt->second->name.substr(0, cmdIt->second->name.length() - 9) + ".";


                                    //1. find the deciding parameter - use valueKeyBase and valueKeyBaseAlt when trying to find it
                                    std::string alternValueKey = valueKeyBase + deviceParameter->paramInfo->alternField;
                                    auto alternParameterIterator = channelIterator->second.find(alternValueKey);
                                    if (alternParameterIterator == channelIterator->second.end())
                                    {
                                        alternValueKey = valueKeyBaseAlt + deviceParameter->paramInfo->alternField;
                                        alternParameterIterator = channelIterator->second.find(alternValueKey);
                                    }

                                    if (alternParameterIterator != channelIterator->second.end())
                                    {
                                        //2. if found, get its value with either getValue (no retrieval for the host/no sending packets, get the cached value, it was obtained from this packet, or better, directly
                                        // if should be some integer
                                        const BaseLib::PVariable alternValue = alternParameterIterator->second.getLogicalData();
                                        if (alternValue->type == BaseLib::VariableType::tInteger || alternValue->type == BaseLib::VariableType::tInteger64)
                                        {
                                            //3. using its value, locate the corresponding record in alternRecords
                                            uint64_t altervalint = alternValue->type == BaseLib::VariableType::tInteger ? static_cast<uint64_t>(alternValue->integerValue) : static_cast<uint64_t>(alternValue->integerValue64);

                                            auto alterRecIt = deviceParameter->paramInfo->alternRecords.find(altervalint);
                                            if (alterRecIt != deviceParameter->paramInfo->alternRecords.end())
                                            {
                                                const std::vector<ClustersInfo::Param> *params = &alterRecIt->second.params;

                                                // now set the variable using the info in 'alternRecord' for its structure
                                                decoded = SetComplexVariableBinary(params, deviceParameter->type, value, parameter.getBinaryDataReference());
                                            }
                                            else
                                            {
                                                GD::out.printDebug("setValue: Looks like an 'altern' record does not exist for 'altern' value, problem parameter: " + deviceParameter->paramInfo->name + " please fix it in the clusters xml file");

                                                // try the old way
                                                decoded = SetComplexVariableBinary(deviceParameter, value, parameter.getBinaryDataReference());
                                            }
                                        }
                                        else
                                            GD::out.printDebug("setValue: Looks like an 'altern' parameter does not have an integer value, problem parameter: " + deviceParameter->paramInfo->name + " please fix it in the clusters xml file");
                                    }
                                    else
                                    {
                                        GD::out.printDebug("setValue: Looks like an 'altern' parameter does not exist, problem parameter: " + deviceParameter->paramInfo->name + " please fix it in the clusters xml file");

                                        // try the old way
                                        decoded = SetComplexVariableBinary(deviceParameter, value, parameter.getBinaryDataReference());
                                    }


                                    // in the end check out if there is a LENGTH/SIZE param and set it to either the length of the binary data or to the count of the 'records', depending on the type of the length
                                    // this currently is done only for params at the first level for a command, I'm not aware of having a similar situation for more complex types
                                    if (decoded && deviceParameter->paramInfo && (deviceParameter->paramInfo->sizeParam || deviceParameter->paramInfo->sizeBitfield))
                                    {
                                        BaseLib::PVariable sizeVariable;

                                        if (deviceParameter->paramInfo->sizeParam)
                                        {
                                            if (2 == deviceParameter->paramInfo->sizeParam->sizeType) // items
                                            {
                                                if (VariableType::tBinary != value->type) // if it was set to binary and it's a 'count' type size, don't set it, let whatever is set in there!
                                                    sizeVariable = std::make_shared<Variable>(VariableType::tArray == value->type ? value->arrayValue->size() : 1);
                                            }
                                            else // octets
                                                sizeVariable = std::make_shared<Variable>(parameter.getBinaryDataReference().size());

                                            if (sizeVariable)
                                            {
                                                std::string sizeParamName = valueKeyBase + deviceParameter->paramInfo->sizeParam->name;
                                                auto sizeParamIterator = channelIterator->second.find(sizeParamName);
                                                if (sizeParamIterator == channelIterator->second.end())
                                                    sizeParamName = valueKeyBaseAlt + deviceParameter->paramInfo->sizeParam->name;

                                                setValue(clientInfo, channel, sizeParamName, sizeVariable, false, false);
                                            }
                                        }
                                        else
                                        {
                                            if (2 == deviceParameter->paramInfo->sizeBitfield->sizeType) // items
                                            {
                                                if (VariableType::tBinary != value->type) // if it was set to binary and it's a 'count' type size, don't set it, let whatever is set in there!
                                                    sizeVariable = std::make_shared<Variable>(VariableType::tArray == value->type ? value->arrayValue->size() : 1);
                                            }
                                            else // octets
                                                sizeVariable = std::make_shared<Variable>(parameter.getBinaryDataReference().size());

                                            if (sizeVariable && deviceParameter->paramInfo->sizeBitfield->parentParam)
                                            {
                                                std::string sizeParamName = valueKeyBase + deviceParameter->paramInfo->sizeBitfield->parentParam->name + "." + deviceParameter->paramInfo->sizeBitfield->name;
                                                auto sizeParamIterator = channelIterator->second.find(sizeParamName);
                                                if (sizeParamIterator == channelIterator->second.end())
                                                    sizeParamName = valueKeyBaseAlt + deviceParameter->paramInfo->sizeBitfield->parentParam->name + "." + deviceParameter->paramInfo->sizeBitfield->name;

                                                setValue(clientInfo, channel, sizeParamName, sizeVariable, false, false);
                                            }
                                        }
                                    }

                                }
                                else
                                    decoded = SetComplexVariableBinary(deviceParameter, value, parameter.getBinaryDataReference());
                            }
                            else
                            {
                                GD::out.printDebug("setValue: couldn't cast to a zigbee packet");
                                decoded = SetComplexVariableBinary(deviceParameter, value, parameter.getBinaryDataReference());
                            }
                        }
                    }

                    // if it's a bitmask value, walk on all 'fake' params and set them
                    if (!isBitmaskParameter) // don't do that for 'fake' fields
                    {
                        const uint32_t bitfieldVal = value->integerValue;
                        for (const auto& bitfield : (deviceParameter->attrInfo ? deviceParameter->attrInfo->bitfields : deviceParameter->paramInfo->bitfields))
                        {
                            const uint32_t theVal = (bitfieldVal & bitfield.mask) >> bitfield.shifter;
                            const std::string bitfieldValueKey = deviceParameter->clusterInfo->name + "." + (deviceParameter->attrInfo ? deviceParameter->attrInfo->name : deviceParameter->paramInfo->name) + "__" + bitfield.name;

                            auto bitfieldParameterIterator = channelIterator->second.find(bitfieldValueKey);
                            if (bitfieldParameterIterator != channelIterator->second.end())
                            {
                                BaseLib::PVariable bitfieldVariable;
                                std::vector<uint8_t> bRawData;
                                if (bitfield.IsSingleBit())
                                {
                                    bitfieldVariable = std::make_shared<BaseLib::Variable>(1 == theVal ? true : false);
                                    bRawData.resize(1);
                                    bRawData[0] = theVal;
                                }
                                else
                                {
                                    bitfieldVariable = std::make_shared<BaseLib::Variable>(theVal);
                                    bRawData.resize(4);
                                    bRawData[0] = (theVal >> 24) & 0xFF;
                                    bRawData[1] = (theVal >> 16) & 0xFF;
                                    bRawData[2] = (theVal >> 8) & 0xFF;
                                    bRawData[3] = theVal & 0xFF;
                                }

                                bitfieldParameterIterator->second.setLogicalData(bitfieldVariable);

                                // only if there is a role set on it
                                if (bitfieldParameterIterator->second.rpcParameter && bitfieldParameterIterator->second.hasRoles())
                                    bitfieldParameterIterator->second.rpcParameter->convertToPacket(bitfieldVariable, bitfieldParameterIterator->second.mainRole(), bRawData);

                                bitfieldParameterIterator->second.setBinaryData(bRawData);

                                AddValueForEvents(valueKeys, rpcValues, channel, bitfieldValueKey, bitfieldVariable);

                                if(parameterIterator->second.databaseId > 0)
                                    saveParameter(bitfieldParameterIterator->second.databaseId, bRawData);
                                else
                                    saveParameter(0, isConfig ? ParameterGroup::Type::Enum::config : ParameterGroup::Type::Enum::variables, channel, bitfieldValueKey, bRawData);

                                if(_bl->debugLevel >= 4)
                                {
                                    GD::out.printInfo("Info: setValue: Setting value for " + bitfieldValueKey + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":" + std::to_string(channel)
                                                      + " to value: " + BaseLib::HelperFunctions::getHexString(bRawData));
                                }
                            }
                            else
                            {
                                GD::out.printDebug("Debug: setValue: Couldn't find the bitfield parameter: " + bitfieldValueKey);
                            }
                        }
                    }

                    if (parameter.getBinaryDataReference().empty() && rpcParameter->id.length() >= 7 && rpcParameter->id.substr(rpcParameter->id.length() - 7) == ".ACTION")
                    {
                        parameter.getBinaryDataReference().emplace_back(0xFF);
                    }

                    if(_bl->debugLevel >= 6)
                    {
                        if (decoded)
                            GD::out.printInfo("Info: SetValue: Decoded value for: " + valueKey + " set to value: " + BaseLib::HelperFunctions::getHexString(parameter.getBinaryDataReference()));
                        else
                            GD::out.printInfo("Info: SetValue: Failed to decode: " + valueKey);
                    }
                }
                catch(const std::exception& ex)
                {
                    if(_bl->debugLevel >= 6)
                        GD::out.printInfo("Info: SetValue: Failed to decode: " + valueKey + " an exception was thrown: " + ex.what());
                }
                parameter.unlock();
            }
            else
                return Variable::createError(-6, "No info was found for parameter " + valueKey);


            for(std::shared_ptr<Parameter::Packet> setReq : setRequests)
            {
                packetIterator = _rpcDevice->packetsById.find(setReq->id);
                if(packetIterator == _rpcDevice->packetsById.end())
                    return Variable::createError(-6, "No frame was found for parameter " + valueKey);
                frame = packetIterator->second;

                if(frame->subtype == -1)
                    frame->subtype = 1;

                // TODO: construct the 'set' packet

                PZigbeePacket packet = ConstructGetOrSetPacket(addr, setRequest, rpcParameter, channelIterator, false);

                if (!packet)
                    return Variable::createError(-6, "No frame was found for parameter " + rpcParameter->metadata);

                // packet was generated from a setValue for a dynamic parameter, parse the packet to sync the static ones that might be affected
                ParsePacketStatic(packet, valueKeys, rpcValues, false);


                if (IPhysical::OperationType::Enum::command == rpcParameter->physical->operationType && sendPackets)
                {
                    if(GD::bl->debugLevel >= 4)
                        GD::out.printInfo( "Info: SetValue sending set packet...");

                    _physicalInterface->sendPacket(packet, wakeupDev);

                    if(setReq->delay > 0)
                        std::this_thread::sleep_for(std::chrono::milliseconds(setReq->delay));

                    if (wait && !wakeupDev)
                        WaitForPacket(packet, packetSent, receivedAck);
                }
                else if (sendPackets)
                {
                    if(GD::bl->debugLevel >= 4)
                        GD::out.printInfo( "Info: sendPackets true but operation type is not command, so not sending");
                }
            }

            // identical with above, but for 'get'

            for(std::shared_ptr<Parameter::Packet> getReq : getRequests)
            {
                packetIterator = _rpcDevice->packetsById.find(getReq->id);
                if(packetIterator == _rpcDevice->packetsById.end())
                    return Variable::createError(-6, "No frame was found for parameter " + valueKey);
                frame = packetIterator->second;

                if(frame->subtype == -1)
                    frame->subtype = 1;

                PZigbeePacket packet = ConstructGetOrSetPacket(addr, setRequest, rpcParameter, channelIterator, true);

                if (!packet)
                    return Variable::createError(-6, "No frame was found for parameter " + rpcParameter->metadata);

                // packet was generated from a setValue for a dynamic parameter, parse the packet to sync the static ones that might be affected
                ParsePacketStatic(packet, valueKeys, rpcValues, false);

                if (IPhysical::OperationType::Enum::command == rpcParameter->physical->operationType && sendPackets)
                {
                    if(GD::bl->debugLevel >= 4)
                        GD::out.printInfo( "Info: SetValue sending get packet...");

                    _physicalInterface->sendPacket(packet, wakeupDev);

                    if(getReq->delay > 0)
                        std::this_thread::sleep_for(std::chrono::milliseconds(getReq->delay));

                    if (wait && !wakeupDev)
                        WaitForPacket(packet, packetSent, receivedAck);
                }
            }

        } // end handling 'dynamic' set value

        if(!rpcValues.empty())
        {
            DealWithStaticSpecialParams(valueKeys, rpcValues);

            for(std::map<uint32_t, std::shared_ptr<std::vector<std::string>>>::iterator j = valueKeys.begin(); j != valueKeys.end(); ++j)
            {
                if(j->second->empty())
                    continue;

                std::string address(_serialNumber + ":" + std::to_string(channel));
                raiseEvent(clientInfo->initInterfaceId, _peerID, j->first, j->second, rpcValues.at(j->first));
                raiseRPCEvent(clientInfo->initInterfaceId, _peerID, j->first, address, j->second, rpcValues.at(j->first));
            }
        }

        if (wait && !wakeupDev && sendPackets && !receivedAck)
        {
            if (packetSent)
                return Variable::createError(-100, "No ACK answer from device.");
            else
                return Variable::createError(-100, "Packet was not sent yet, wait timeout.");
        }

        return std::make_shared<Variable>(VariableType::tVoid);
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return Variable::createError(-32500, "Unknown application error. See error log for more details.");
}



PVariable ZigbeePeer::setValue(BaseLib::PRpcClientInfo clientInfo, uint32_t channel, std::string valueKey, PVariable value, bool wait)
{
    return setValue(clientInfo, channel, valueKey, value, wait, true);
}

bool ZigbeePeer::SetSimpleVariableOrStructToBinary(const std::vector<ClustersInfo::Param> *params, PVariable value, std::vector<uint8_t>& buffer)
{
    if (!params || !value)
        return false;

    if (VariableType::tStruct == value->type && value->structValue)
    {
        for (const auto& param : *params)
        {
            if (value->structValue->find(param.name) != value->structValue->end())
            {
                // 1. The value is simple, no more params
                if (param.params.empty())
                {
                    // if value is a string and the param is an enum, convert it to integer if possible
                    if (VariableType::tString == value->type && !param.enums.empty())
                    {
                        for (const auto& e : param.enums)
                        {
                            if (e.name == value->stringValue)
                            {
                                value->type = VariableType::tInteger;
                                value->integerValue = e.val;
                                break;
                            }
                        }
                    }

                    std::vector<uint8_t> result;
                    bool success = param.SetVariable(value->structValue->at(param.name), result, true);
                    if (success)
                        buffer.insert(buffer.end(), result.begin(), result.end());
                    else
                        return false;
                }
                else // 2. Not a simple parameter, call recursively
                {
                    if (param.IsStruct())
                    {
                        ZigbeeType uint16type(0x21);

                        PVariable lengthVar = std::make_shared<Variable>(param.params.size());

                        std::vector<uint8_t> result;
                        bool success = uint16type.SetVariable(lengthVar, result);
                        if (success)
                        {
                            buffer.insert(buffer.end(), result.begin(), result.end());
                        }
                    }

                    SetSimpleVariableOrStructToBinary(&param.params, value->structValue->at(param.name), buffer);
                }
            }
            // TODO: should I simply return false instead of setting to zero if something is not set in the structures?
            else if (param.GetSize())
                buffer.resize(buffer.size() + param.GetSize(), 0);
            else if (param.LengthInFirstOctet())
                buffer.resize(buffer.size() + 1, 0);
            else if (param.LengthInFirstTwoOctets())
                buffer.resize(buffer.size() + 2, 0);
            else
                return false;
        }

        return true;
    }
    else if (1 == params->size()) // could be a simple value
    {
        // if value is a string and the param is an enum, convert it to integer if possible
        if (VariableType::tString == value->type && !params->front().enums.empty())
        {
            for (const auto& e : params->front().enums)
            {
                if (e.name == value->stringValue)
                {
                    value->type = VariableType::tInteger;
                    value->integerValue = e.val;
                    break;
                }
            }
        }

        std::vector<uint8_t> result;

        bool success = params->front().SetVariable(value, result, true);
        if (success)
        {
            buffer.insert(buffer.end(), result.begin(), result.end());

            return true;
        }
        else if (!params->front().params.empty())
        {
            // call it recursively, it's a complex value

            if (params->front().IsStruct())
            {
                ZigbeeType uint16type(0x21);

                PVariable lengthVar = std::make_shared<Variable>(params->front().params.size());
                success = uint16type.SetVariable(lengthVar, result);
                if (success)
                {
                    buffer.insert(buffer.end(), result.begin(), result.end());
                }
            }

            return SetSimpleVariableOrStructToBinary(&params->front().params, value, buffer);
        }
    }

    return false;
}


bool ZigbeePeer::SetComplexVariableBinary(const std::vector<ClustersInfo::Param> *params, uint8_t type, PVariable value, std::vector<uint8_t>& buffer)
{
    if (!params || params->empty())
        return false;

    if (0x4C == type) // struct
    {
        if (value->type != VariableType::tStruct)
            return false;

        ZigbeeType uint16type(0x21);

        PVariable lengthVar = std::make_shared<Variable>(params->size());

        std::vector<uint8_t> result;
        bool success = uint16type.SetVariable(lengthVar, result);
        if (success)
        {
            buffer.insert(buffer.end(), result.begin(), result.end());
        }

        return SetSimpleVariableOrStructToBinary(params, value, buffer);
    }
    else if (0x48 == type || 0x50 == type || 0x51 == type) // array, set or bag
    {
        if (value->type != VariableType::tArray)
            return false;

        for (PVariable var : *(value->arrayValue))
        {
            bool res = SetSimpleVariableOrStructToBinary(params, var, buffer);

            if (!res)
                return false;
        }
    }

    return false;
}

bool ZigbeePeer::SetComplexVariableBinary(const DeviceParameter* deviceParameter, PVariable value, std::vector<uint8_t>& buffer)
{
    // 'Problem' types:

    //struct 0x4C - this has structure, there is no such type in the xml clusters yet, should be handled just in case

    // the following contain possibly more than one value, each possibly having structure, maybe a fixed length, maybe not
    // the element length might be easily calculable or not
    // for example, it might have a fixed length that can be computed without the data (a struct that has only fixed length items)
    // but it might be variable, for example containing a string which has a length that is known only from the data
    // or it's even worse, it can contain something that by itself does not supply its length (array, set, bag)
    // usually there is some info preceding that if it's a command (a field like size/length) that provides some info on length (but it's complicated that sometimes it says number of octets, sometimes it says number of elements)

    //array 0x48 - does not exist in the clusters xml file yet, but should be handled just in case, probably the same as 'bag'

    // the difference between the two is that the 'set' is supposed to not have duplicate entries
    // but handle them the same way (also the array) nevertheless

    //set 0x50
    //bag 0x51

    if (!deviceParameter || !value)
        return false;

    const std::vector<ClustersInfo::Param> *params = (deviceParameter->attrInfo ? &deviceParameter->attrInfo->params : (deviceParameter->paramInfo ? &deviceParameter->paramInfo->params : nullptr));

    return SetComplexVariableBinary(params, deviceParameter->type, value, buffer);
}

void ZigbeePeer::SendAllGetPackets(bool wakeupDevice)
{
    try
    {
        if (_disposing)
            return;
        else if (getPacketsSent.exchange(true))
            return;

        // the first commands here should be those very important
        // like those for finding info about the service/peer - for example supported command class versions

        if(GD::bl->debugLevel >= 4)
            GD::out.printInfo( "Info: Sending get packets...");

        std::shared_ptr<ClustersInfo> xmlClustersInfo;
        if (GD::family)
            xmlClustersInfo = GD::family->_clustersInfo;

        std::shared_ptr<HomegearDevice> rpcDevice = _rpcDevice; // copy it in case it is replaced while this thread executes

        const uint32_t addr = getAddress();

        bool hasGetsToSend = false;

        std::pair<uint16_t, uint16_t> lastClusterAndAttrLocal(0xFF, 0xFF);

        {
            std::lock_guard<std::mutex> serviceLock(infoMutex);


            if (nodeInfo.EndPoints.find(endPoint) == nodeInfo.EndPoints.end())
                return;

            bool supportsBasicCluster = false;

            for (uint16_t clusterId : nodeInfo.EndPoints[endPoint].InClusterList)
            {
                if (0 == clusterId)
                {
                    supportsBasicCluster = true;
                    break;
                }
            }

            for (uint16_t clusterId : nodeInfo.EndPoints[endPoint].InClusterList)
            {
                // send them fully also if the 'basic' cluster is not supported
                if ((!nodeInfo.SupportsDiscoverAttrsExtended && !nodeInfo.SupportsDiscoverAttrsSimple) || !supportsBasicCluster)
                {
                    // walk over all the attrs from the xml file and generate the packets
                    auto clusterXmlIt = xmlClustersInfo->serversClustersInfoMap.find(clusterId);
                    if (clusterXmlIt != xmlClustersInfo->serversClustersInfoMap.end())
                    {
                        for (auto attrIt = clusterXmlIt->second.attrsMap.begin(); attrIt != clusterXmlIt->second.attrsMap.end(); ++attrIt)
                        {
                            if (attrIt->second.IsReadable())
                            {
                                PZigbeePacket packet = ConstructGetPacketForAttr(clusterId, attrIt->second.attrId);
                                if (packet)
                                {
                                    hasGetsToSend = true;
                                    lastClusterAndAttrLocal = std::make_pair(clusterId, attrIt->second.attrId);
                                    _physicalInterface->enqueuePacket(packet, wakeupDevice);
                                }
                            }
                        }
                    }
                }
                else
                {
                    // means that attrs were retrieved, so we can use what we have in info to generate the packets
                    for (uint16_t attrId : nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterId].attrsList)
                    {
                        if (nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterId].attrsMap[attrId].IsReadable() || 0 == nodeInfo.EndPoints[endPoint].ServersClustersInfoMap[clusterId].attrsMap[attrId].accessControl)
                        {
                            PZigbeePacket packet = ConstructGetPacketForAttr(clusterId, attrId);
                            if (packet)
                            {
                                hasGetsToSend = true;
                                lastClusterAndAttrLocal = std::make_pair(clusterId, attrId);
                                _physicalInterface->enqueuePacket(packet, wakeupDevice);
                            }
                        }
                    }
                }
            }
        }

        // TODO: also send 'get' packets for static items? Could mean sending duplicates...

        if (hasGetsToSend)
        {
            {
                std::lock_guard<std::mutex> lock(valuesMutex);
                lastClusterAndAttr = lastClusterAndAttrLocal;
            }
            _physicalInterface->tryToSend(addr, wakeupDevice);
        }
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}


void ZigbeePeer::setServiceVariable(const std::string& id)
{
    std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator channelIterator = configCentral.find(0);
    if (channelIterator == configCentral.end())
    {
        channelIterator = valuesCentral.find(0);
        if (channelIterator == valuesCentral.end())
            return;
    }

    std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator parameterIterator = channelIterator->second.find(id);
    if (parameterIterator == channelIterator->second.end())
    {
        channelIterator = valuesCentral.find(0);
        if (channelIterator == valuesCentral.end())
            return;
        parameterIterator = channelIterator->second.find(id);
        if (parameterIterator == channelIterator->second.end())
            return;
    }


    BaseLib::Systems::RpcConfigurationParameter& parameter = parameterIterator->second;

    PParameter rpcParameter = parameterIterator->second.rpcParameter;
    if(!rpcParameter)
        return;

    PVariable element;

    // TODO: implement security
    if (id == "ROUTER")
    {
        element = std::make_shared<Variable>(nodeInfo.IsRouter());
    }
    else if (id == "MAINS_POWERED")
    {
        element = std::make_shared<Variable>(nodeInfo.IsMainsPowerDevice());
    }
    else if (id == "LISTENING")
    {
        element = std::make_shared<Variable>(nodeInfo.IsReceiverWhenIdleDevice());
    }
    else if (id == "CONFIG_PENDING")
    {
        const bool isPending = isConfigPending();
        element = std::make_shared<Variable>(isPending);

        {
            std::lock_guard<std::mutex> lock(serviceMessagesMutex);
            if (serviceMessages)
                serviceMessages->setConfigPending(isPending);
        }
    }
    else if (id == "SHORT_ADDR")
    {
        const uint32_t addr = getAddress();
        const uint16_t shortAddr = addr & 0xFFFF;

        element = std::make_shared<Variable>(shortAddr);
    }
    else if (id == "END_POINT")
    {
        element = std::make_shared<Variable>(endPoint);
    }

    parameter.lock();
    try
    {
        parameter.setLogicalData(element);
        rpcParameter->convertToPacket(element, parameter.mainRole(), parameter.getBinaryDataReference());
    }
    catch(...) {}
    parameter.unlock();
}

void ZigbeePeer::initializeServiceVariables()
{
    setServiceVariable("ROUTER");
    setServiceVariable("MAINS_POWERED");
    setServiceVariable("LISTENING");

    setServiceVariable("CONFIG_PENDING");

    setServiceVariable("SHORT_ADDR");
    setServiceVariable("END_POINT");
}


void ZigbeePeer::setRssiDevice(int8_t rssi)
{
  try {
    if (_disposing) return;

    uint32_t time = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    if (time - _lastRssiDevice > 10)
    {
      static const char* rssiItemName = "RSSI_DEVICE";
      _lastRssiDevice = time;

      auto channelIterator = valuesCentral.find(0);
      if (channelIterator == valuesCentral.end()) return;
      auto parameterIterator = channelIterator->second.find(rssiItemName);
      if (parameterIterator == channelIterator->second.end()) return;

      BaseLib::Systems::RpcConfigurationParameter &parameter = parameterIterator->second;
      std::vector<uint8_t> parameterData { static_cast<uint8_t>(rssi) };

      parameter.setBinaryData(parameterData);

      parameter.setLogicalData(std::make_shared<Variable>(static_cast<int32_t>(rssi) * -1));

      std::shared_ptr<std::vector<std::string>> valueKeys(new std::vector<std::string>({std::string(rssiItemName)}));
      std::shared_ptr<std::vector<PVariable>> rpcValues(new std::vector<PVariable>());
      rpcValues->push_back(parameter.rpcParameter->convertFromPacket(parameterData, parameter.mainRole(), false));

      std::string eventSource = "device-" + std::to_string(_peerID);
      std::string address = _serialNumber + ":0";

      raiseEvent(eventSource, _peerID, 0, valueKeys, rpcValues);
      raiseRPCEvent(eventSource, _peerID, 0, address, valueKeys, rpcValues);
    }
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}


}
