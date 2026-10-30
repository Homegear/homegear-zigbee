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

#include "ZigbeeCentral.h"
#include "GD.h"
#include "ZigbeeDevicesDescription.h"

#include <iomanip>

namespace Zigbee
{

ZigbeeCentral::ZigbeeCentral(ICentralEventSink* eventHandler)
    : BaseLib::Systems::ICentral(ZIGBEE_FAMILY_ID, GD::bl, eventHandler), _allowDeletePeer(false), _deviceTypeCounter(1)
{
    init();
}

ZigbeeCentral::ZigbeeCentral(uint32_t deviceID, std::string serialNumber, ICentralEventSink* eventHandler)
    : BaseLib::Systems::ICentral(ZIGBEE_FAMILY_ID, GD::bl, deviceID, serialNumber, -1, eventHandler), _allowDeletePeer(false), _deviceTypeCounter(1)
{
    init();
}

ZigbeeCentral::~ZigbeeCentral()
{
    dispose();
}

void ZigbeeCentral::dispose(bool wait)
{
    try
    {
        if(_disposing)
            return;
        _disposing = true;

        _stopWorkerThread = true;
        GD::bl->threadManager.join(_workerThread);
        _stopPairingModeThread = true;
        GD::bl->threadManager.join(_pairingModeThread);

        GD::out.printDebug("Removing device " + std::to_string(_deviceId) + " from physical device's event queue...");
        GD::interfaces->removeEventHandlers();
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void ZigbeeCentral::init()
{
    try
    {
        if(_initialized)
            return; //Prevent running init two times
        _initialized = true;

        GD::interfaces->addEventHandlers((BaseLib::Systems::IPhysicalInterface::IPhysicalInterfaceEventSink*)this);

        _bl->threadManager.start(_workerThread, true, _bl->settings.workerThreadPriority(), _bl->settings.workerThreadPolicy(), &ZigbeeCentral::worker, this);
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}


void ZigbeeCentral::worker()
{
    try
    {
        std::chrono::milliseconds sleepingTime(10);
        uint32_t counter = 0;
        uint32_t interfaceCounter = 0;
        uint64_t lastPeer = 0;
        //One loop on the Raspberry Pi takes about 30Âµs
        while(!_stopWorkerThread)
        {
            try
            {
                std::this_thread::sleep_for(sleepingTime);
                if(_stopWorkerThread)
                    return;

                ++interfaceCounter;
                if (sleepingTime.count() * interfaceCounter >= 30000)
                {
                    interfaceCounter = 0;
                    // hopefully does not hurt - if somehow the tryToSend loop is broken, this wakes it up
                    // only for online devices, the wakeup are waked up by a wakup notification packet
                    //for (auto &physInt : GD::physicalInterfaces)
                     //   physInt.second->tryToSend(0, false);
                     GD::interfaces->tryToSend();
                }

                if(counter > 10000)
                {
                    counter = 0;
                    std::lock_guard<std::mutex> lock(_peersMutex);
                    if(_peersById.size() > 0)
                    {
                        int32_t windowTimePerPeer = _bl->settings.workerThreadWindow() / _peersById.size();
                        if(windowTimePerPeer > 2)
                            windowTimePerPeer -= 2;
                        sleepingTime = std::chrono::milliseconds(windowTimePerPeer);
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(_peersMutex);
                    if(!_peersById.empty())
                    {
                        std::map<uint64_t, std::shared_ptr<BaseLib::Systems::Peer>>::iterator nextPeer = _peersById.find(lastPeer);
                        if(nextPeer != _peersById.end())
                        {
                            ++nextPeer;
                            if(nextPeer == _peersById.end())
                                nextPeer = _peersById.begin();
                        }
                        else
                            nextPeer = _peersById.begin();
                        lastPeer = nextPeer->first;
                    }
                }

                std::shared_ptr<ZigbeePeer> peer(getPeer(lastPeer));
                if(peer && !peer->deleting)
                    peer->worker();

                GD::interfaces->worker();

                counter++;
            }
            catch(const std::exception& ex)
            {
                GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
            }
        }
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void ZigbeeCentral::loadPeers()
{
    try
    {
        std::shared_ptr<BaseLib::Database::DataTable> rows = _bl->db->getPeers(_deviceId);
        for(BaseLib::Database::DataTable::iterator row = rows->begin(); row != rows->end(); ++row)
        {
            int32_t peerID = row->second.at(0)->intValue;
            GD::out.printMessage("Loading Zigbee peer " + std::to_string(peerID));
            std::shared_ptr<ZigbeePeer> peer = std::make_shared<ZigbeePeer>(peerID, row->second.at(3)->textValue, _deviceId, this);
            if(!peer->load(this))
                continue;

            if(!peer->getRpcDevice())
                continue;

            {
                std::lock_guard<std::mutex> lock(_peersMutex);
                if(!peer->getSerialNumber().empty())
                    _peersBySerial[peer->getSerialNumber()] = peer;
                _peersById[peerID] = peer;
                _peers[peer->getAddress()] = peer;
                peersByIEEE[peer->IEEEaddr][peer->endPoint] = peer;
            }

            if (!peer->getPacketsSent)
            {
                _bl->threadManager.start(peer->getThread, false, &ZigbeePeer::SendAllGetPackets, peer, peer->wakeupDevice());
            }
        }
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

std::shared_ptr<ZigbeePeer> ZigbeeCentral::getPeer(uint64_t id)
{
    try
    {
        std::lock_guard<std::mutex> lock(_peersMutex);
        if(_peersById.find(id) != _peersById.end())
        {
            std::shared_ptr<ZigbeePeer> peer(std::dynamic_pointer_cast<ZigbeePeer>(_peersById.at(id)));
            return peer;
        }
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return std::shared_ptr<ZigbeePeer>();
}

std::shared_ptr<ZigbeePeer> ZigbeeCentral::getPeer(uint32_t addr)
{
    try
    {
        std::lock_guard<std::mutex> lock(_peersMutex);
        if(_peers.find(addr) != _peers.end())
        {
            std::shared_ptr<ZigbeePeer> peer(std::dynamic_pointer_cast<ZigbeePeer>(_peers.at(addr)));
            return peer;
        }
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return std::shared_ptr<ZigbeePeer>();
}

std::shared_ptr<ZigbeePeer> ZigbeeCentral::getPeer(std::string serialNumber)
{
    try
    {
        std::lock_guard<std::mutex> lock(_peersMutex);
        if(_peersBySerial.find(serialNumber) != _peersBySerial.end())
        {
            std::shared_ptr<ZigbeePeer> peer(std::dynamic_pointer_cast<ZigbeePeer>(_peersBySerial.at(serialNumber)));
            return peer;
        }
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return std::shared_ptr<ZigbeePeer>();
}

void ZigbeeCentral::savePeers(bool full)
{
    try
    {
        std::lock_guard<std::mutex> lock(_peersMutex);
        for(std::map<uint64_t, std::shared_ptr<BaseLib::Systems::Peer>>::iterator i = _peersById.begin(); i != _peersById.end(); ++i)
        {
            //Necessary, because peers can be assigned to multiple virtual devices
            if(i->second->getParentID() != _deviceId)
                continue;
            //We are always printing this, because the init script needs it
            GD::out.printMessage("(Shutdown) => Saving Zigbee peer " + std::to_string(i->second->getID()));
            i->second->save(full, full, full);
        }
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void ZigbeeCentral::deletePeer(uint64_t id)
{
    try
    {
        if(_disposing)
            return;

        std::shared_ptr<ZigbeePeer> peer(getPeer(id));
        if(!peer)
            return;
        peer->deleting = true;
        peer->removeHooks();

        PVariable deviceAddresses = std::make_shared<Variable>(VariableType::tArray);
        deviceAddresses->arrayValue->push_back(std::make_shared<Variable>(peer->getSerialNumber()));

        PVariable deviceInfo= std::make_shared<Variable>(VariableType::tStruct);
        deviceInfo->structValue->insert(StructElement("ID", std::make_shared<Variable>((int32_t)peer->getID())));
        PVariable channels = std::make_shared<Variable>(VariableType::tArray);
        deviceInfo->structValue->insert(StructElement("CHANNELS", channels));

        for(Functions::iterator i = peer->getRpcDevice()->functions.begin(); i != peer->getRpcDevice()->functions.end(); ++i)
        {
            deviceAddresses->arrayValue->push_back(std::make_shared<Variable>(peer->getSerialNumber() + ":" + std::to_string(i->first)));
            channels->arrayValue->push_back(std::make_shared<Variable>(i->first));
        }

        std::vector<uint64_t> deletedIds{ id };
        raiseRPCDeleteDevices(deletedIds, deviceAddresses, deviceInfo);

        peer->deleteFromDatabase();

        {
            std::lock_guard<std::mutex> lock(_peersMutex);
            _peersBySerial.erase(peer->getSerialNumber());
            _peersById.erase(id);
            _peers.erase(peer->getAddress());
            if (peersByIEEE.end() != peersByIEEE.find(peer->IEEEaddr))
            {
                 peersByIEEE[peer->IEEEaddr].erase(peer->endPoint);
                 if (peersByIEEE[peer->IEEEaddr].empty()) peersByIEEE.erase(peer->IEEEaddr);
            }
        }
        GD::out.printMessage("Removed Zigbee peer " + std::to_string(peer->getID()));
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

std::string ZigbeeCentral::handleCliCommand(std::string command)
{
    try
    {
        std::vector<std::string> arguments;
        bool showHelp = false;
        std::ostringstream stringStream;
        if(BaseLib::HelperFunctions::checkCliCommand(command, "help", "h", "", 0, arguments, showHelp))
        {
            stringStream << "List of commands:" << std::endl << std::endl;
            stringStream << "For more information about the individual command type: COMMAND help" << std::endl << std::endl;
            stringStream << "peers list (ls)           List all peers" << std::endl;
            stringStream << "reset (res)               Network reset" << std::endl;
            stringStream << "pairing on (pon)          Enables pairing mode" << std::endl;
            stringStream << "remove on (ron)           Enables remove mode" << std::endl;
            stringStream << "peers select (ps)         Select a peer" << std::endl;
            stringStream << "peers setname (pn)        Name a peer" << std::endl;
            stringStream << "unselect (u)              Unselect this device" << std::endl;

            return stringStream.str();
        }
        else if(BaseLib::HelperFunctions::checkCliCommand(command, "peers list", "pl", "ls", 0, arguments, showHelp))
        {
            try
            {
                if(showHelp)
                {
                    stringStream << "Description: This command lists information about all peers." << std::endl;
                    stringStream << "Usage: peers list [FILTERTYPE] [FILTERVALUE]" << std::endl << std::endl;
                    stringStream << "Parameters:" << std::endl;
                    stringStream << "  FILTERTYPE:  See filter types below." << std::endl;
                    stringStream << "  FILTERVALUE: Depends on the filter type. If a number is required, it has to be in hexadecimal format." << std::endl << std::endl;
                    stringStream << "Filter types:" << std::endl;
                    stringStream << "  ID: Filter by id." << std::endl;
                    stringStream << "      FILTERVALUE: The id of the peer to filter (e. g. 513)." << std::endl;
                    stringStream << "  SERIAL: Filter by serial number." << std::endl;
                    stringStream << "      FILTERVALUE: The serial number of the peer to filter (e. g. JEQ0554309)." << std::endl;
                    stringStream << "  ADDRESS: Filter by saddress." << std::endl;
                    stringStream << "      FILTERVALUE: The address of the peer to filter (e. g. 128)." << std::endl;
                    stringStream << "  NAME: Filter by name." << std::endl;
                    stringStream << "      FILTERVALUE: The part of the name to search for (e. g. \"1st floor\")." << std::endl;
                    stringStream << "  TYPE: Filter by device type." << std::endl;
                    stringStream << "      FILTERVALUE: The 2 byte device type in hexadecimal format." << std::endl;
                    return stringStream.str();
                }

                std::string filterType;
                std::string filterValue;

                if(arguments.size() >= 2)
                {
                    filterType = BaseLib::HelperFunctions::toLower(arguments.at(0));
                    filterValue = arguments.at(1);
                    if(filterType == "name")
                        BaseLib::HelperFunctions::toLower(filterValue);
                }

                if(_peersById.empty())
                {
                    stringStream << "No peers are paired to this central." << std::endl;
                    return stringStream.str();
                }
                std::string bar(" │ ");
                const int32_t idWidth = 4;
                const int32_t nameWidth = 31;
                const int32_t serialWidth = 18;
                const int32_t typeWidth1 = 4;
                const int32_t typeWidth2 = 11;
                std::string nameHeader("Name");
                nameHeader.resize(nameWidth, ' ');
                std::string typeStringHeader("Type String");
                typeStringHeader.resize(typeWidth2, ' ');
                stringStream << std::setfill(' ')
                             << std::setw(idWidth) << "ID" << bar
                             << nameHeader << bar
                             << std::setw(serialWidth) << "Serial Number" << bar
                             << std::setw(typeWidth1) << "Type" << bar
                             << typeStringHeader
                             << std::endl;
                stringStream << "─────┼─────────────────────────────────┼────────────────────┼──────┼────────────" << std::endl;
                stringStream << std::setfill(' ')
                             << std::setw(idWidth) << " " << bar
                             << std::setw(nameWidth) << " " << bar
                             << std::setw(serialWidth) << " " << bar
                             << std::setw(typeWidth1) << " " << bar
                             << std::setw(typeWidth2)
                             << std::endl;

                {
                    std::lock_guard<std::mutex> lock(_peersMutex);

                    for(std::map<uint64_t, std::shared_ptr<BaseLib::Systems::Peer>>::iterator i = _peersById.begin(); i != _peersById.end(); ++i)
                    {
                        if(filterType == "id")
                        {
                            uint64_t id = BaseLib::Math::getNumber(filterValue, false);
                            if(i->second->getID() != id)
                                continue;
                        }
                        else if(filterType == "name")
                        {
                            std::string name = i->second->getName();
                            if((signed)BaseLib::HelperFunctions::toLower(name).find(filterValue) == (signed)std::string::npos)
                                continue;
                        }
                        else if(filterType == "serial")
                        {
                            if(i->second->getSerialNumber() != filterValue)
                                continue;
                        }
                        else if(filterType == "type")
                        {
                            int32_t deviceType = BaseLib::Math::getNumber(filterValue, true);
                            if((int32_t)i->second->getDeviceType() != deviceType)
                                continue;
                        }
                        stringStream << std::setw(idWidth) << std::setfill(' ') << std::to_string(i->second->getID()) << bar;

                        std::string name = i->second->getName();
                        size_t nameSize = BaseLib::HelperFunctions::utf8StringSize(name);
                        if(nameSize > (unsigned)nameWidth)
                        {
                            name = BaseLib::HelperFunctions::utf8Substring(name, 0, nameWidth - 3);
                            name += "...";
                        }
                        else
                            name.resize(nameWidth + (name.size() - nameSize), ' ');

                        std::string serial = i->second->getSerialNumber();
                        size_t serialSize = BaseLib::HelperFunctions::utf8StringSize(serial);
                        if(serialSize > (unsigned)serialWidth)
                        {
                            serial = BaseLib::HelperFunctions::utf8Substring(serial, 0, serialWidth - 3);
                            serial += "...";
                        }
                        else
                            serial.resize(serialWidth + (serial.size() - serialSize), ' ');

                        stringStream << std::setw(nameWidth) << name << bar
                                     << std::setw(serialWidth) << serial << bar
                                     << std::setw(typeWidth1) << BaseLib::HelperFunctions::getHexString(i->second->getDeviceType(), 4) << bar;
                        if(i->second->getRpcDevice())
                        {
                            PSupportedDevice type = i->second->getRpcDevice()->getType(i->second->getDeviceType(), i->second->getFirmwareVersion());
                            std::string typeID;
                            if(type)
                                typeID = type->id;
                            if(typeID.size() > (unsigned)typeWidth2)
                            {
                                typeID.resize(typeWidth2 - 3);
                                typeID += "...";
                            }

                            stringStream << std::setw(typeWidth2) << typeID;
                        }
                        else
                            stringStream << std::setw(typeWidth2);
                        stringStream << std::endl << std::dec;
                    }
                }

                stringStream << "─────┴─────────────────────────────────┴────────────────────┴──────┴────────────" << std::endl;

                return stringStream.str();
            }
            catch(const std::exception& ex)
            {
                GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
            }
        }
        else if(BaseLib::HelperFunctions::checkCliCommand(command, "reset", "res", "", 0, arguments, showHelp))
        {
            if(showHelp)
            {
                stringStream << "Description: This command issues a network reset command." << std::endl;
                stringStream << "Usage: reset" << std::endl << std::endl;
                stringStream << "Parameters:" << std::endl;
                stringStream << "  There are no parameters." << std::endl;
                return stringStream.str();
            }

            //for (auto &physInt : GD::physicalInterfaces)
            //{
            //    physInt.second->NetworkReset();
            //}
            GD::interfaces->NetworkReset();

            stringStream << "Network reset issued." << std::endl;
            return stringStream.str();
        }
        else if(BaseLib::HelperFunctions::checkCliCommand(command, "pairing on", "pon", "", 0, arguments, showHelp))
        {
            if(showHelp)
            {
                stringStream << "Description: This command enables pairing mode." << std::endl;
                stringStream << "Usage: pairing on [DURATION]" << std::endl << std::endl;
                stringStream << "Parameters:" << std::endl;
                stringStream << "  DURATION:\tOptional duration in seconds to stay in pairing mode." << std::endl;
                return stringStream.str();
            }

            int32_t duration = 60;
            if(!arguments.empty())
            {
                duration = BaseLib::Math::getNumber(arguments.at(0), false);
                if(duration < 30 || duration > 254)
                    return "Invalid duration. Duration has to be greater than 30 and less than 254.\n";
            }

            BaseLib::PVariable metadata = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
            auto error = setInstallMode(nullptr, true, duration, metadata, false);

            if (error->errorStruct && error->type == VariableType::tStruct)
            {
                auto it = error->structValue->find("faultString");
                if (it != error->structValue->end())
                {
                    auto val = it->second;
                    stringStream << val->stringValue << std::endl;
                }
            }
            else
                stringStream << "Pairing mode enabled." << std::endl;

            return stringStream.str();
        }
        else if(BaseLib::HelperFunctions::checkCliCommand(command, "remove on", "ron", "", 0, arguments, showHelp))
        {
            if(showHelp)
            {
                stringStream << "Description: This command enables remove mode." << std::endl;
                stringStream << "Usage: remove on PEERID [DURATION]" << std::endl << std::endl;
                stringStream << "Parameters:" << std::endl;
                stringStream << "  PEERID:\tThe id of the peer to set the name for. Example: 513" << std::endl;
                stringStream << "  DURATION:\tOptional duration in seconds to stay in remove mode." << std::endl;
                return stringStream.str();
            }

            uint64_t peerID = 0;

            if(arguments.empty())
                stringStream << "Peer should be specified.\n";
            else
            {
                peerID = BaseLib::Math::getNumber(arguments.at(0), false);
                if(0 == peerID)
                    stringStream << "Invalid peer ID\n";
                else if(!peerExists(peerID))
                {
                    stringStream << "This peer is not paired to this central." << std::endl;
                    peerID = 0;
                }
            }

            int32_t duration = 60;
            if(arguments.size() > 1)
            {
                duration = BaseLib::Math::getNumber(arguments.at(1), false);
                if(duration < 30 || duration > 300)
                    stringStream << "Invalid duration. Duration has to be greater than 30 and less than 300.\n";
            }

            if (0 != peerID)
            {
                auto error = setRemoveMode(nullptr, true, peerID, duration, false);
                if (error->errorStruct && error->type == VariableType::tStruct)
                {
                    auto it = error->structValue->find("faultString");
                    if (it != error->structValue->end())
                    {
                        auto val = it->second;
                        stringStream << val->stringValue << std::endl;
                    }
                }
                else
                    stringStream << "Remove mode enabled." << std::endl;
            }

            return stringStream.str();
        }
        else if(command.compare(0, 13, "peers setname") == 0 || command.compare(0, 2, "pn") == 0)
        {
            uint64_t peerID = 0;
            std::string name;

            std::stringstream stream(command);
            std::string element;
            int32_t offset = (command.at(1) == 'n') ? 0 : 1;
            int32_t index = 0;
            while(std::getline(stream, element, ' '))
            {
                if(index < 1 + offset)
                {
                    index++;
                    continue;
                }
                else if(index == 1 + offset)
                {
                    if(element == "help")
                        break;
                    else
                    {
                        peerID = BaseLib::Math::getNumber(element, false);
                        if(peerID == 0)
                            return "Invalid id.\n";
                    }
                }
                else if(index == 2 + offset)
                    name = element;
                else
                    name += ' ' + element;
                index++;
            }
            if(index == 1 + offset)
            {
                stringStream << "Description: This command sets or changes the name of a peer to identify it more easily." << std::endl;
                stringStream << "Usage: peers setname PEERID NAME" << std::endl << std::endl;
                stringStream << "Parameters:" << std::endl;
                stringStream << "  PEERID:\tThe id of the peer to set the name for. Example: 513" << std::endl;
                stringStream << "  NAME:\tThe name to set. Example: \"1st floor light switch\"." << std::endl;
                return stringStream.str();
            }

            if(!peerExists(peerID))
                stringStream << "This peer is not paired to this central." << std::endl;
            else
            {
                std::shared_ptr<ZigbeePeer> peer(getPeer(peerID));
                peer->setName(name);
                stringStream << "Name set to \"" << name << "\"." << std::endl;
            }
            return stringStream.str();
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


std::shared_ptr<ZigbeePeer> ZigbeeCentral::createPeer(uint32_t deviceType, int32_t address, std::string serialNumber, bool save)
{
    try
    {
        if(_disposing)
            return std::shared_ptr<ZigbeePeer>();
        std::shared_ptr<ZigbeePeer> peer = std::make_shared<ZigbeePeer>(_deviceId, this);

        peer->setDeviceType(deviceType);
        peer->setAddress(address);
        peer->setSerialNumber(serialNumber);

        if (serialNumber.substr(0, 2) == "ZG")
            serialNumber = serialNumber.substr(2);

        std::string epString = serialNumber.substr(0, 2);
        serialNumber = serialNumber.substr(2);

        std::stringstream ss;
        ss << epString;
        int ep;
        ss >> std::hex >> ep;
        peer->endPoint = ep;

        ss.clear();
        ss << serialNumber;
        ss >> std::hex >> peer->IEEEaddr;

        peer->setRpcDevice(GD::family->getRpcDevices()->find(deviceType, 0x0, -1));

        if(!peer->getRpcDevice())
            return std::shared_ptr<ZigbeePeer>();
        if(save)
            peer->save(true, true, false); //Save and create peerID
        return peer;
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return std::shared_ptr<ZigbeePeer>();
}

void  ZigbeeCentral::deletePeerByAddr(uint32_t addr)
{
    if (_disposing || addr <= 1 || addr == 0xff)
        return;

    std::shared_ptr<ZigbeePeer> peer(getPeer(addr));
    if(!peer)
        return;

    GD::out.printMessage("Deleting peer with addr: " + std::to_string(addr));

    BaseLib::PRpcClientInfo clientInfo;

    deleteDevice(clientInfo, peer->getID(), 0);
}

void ZigbeeCentral::deletePeers(uint64_t IEEEaddr, uint16_t shortAddr)
{
    for (;;)
    {
        std::unique_lock<std::mutex> lock(_peersMutex);
        // locate the peers for which the address changes:
        auto it = peersByIEEE.find(IEEEaddr);
        if (it == peersByIEEE.end()) break;

        if (it->second.empty()) break;

        auto pit = it->second.begin();
        if (0 != shortAddr && (pit->second->getAddress() & 0xFFFF) != shortAddr)
        {
            GD::out.printDebug("The short address did not match the IEEE one");

            break;
        }
        lock.unlock(); // must unlock since it will be locked by the following call

        deletePeer(pit->second->getID());
    }
}


bool ZigbeeCentral::peerIEEEExists(uint64_t IEEEaddr)
{
    std::lock_guard<std::mutex> lock(_peersMutex);
    // locate the peers for which the address changes:
    auto it = peersByIEEE.find(IEEEaddr);
    if (it == peersByIEEE.end()) return false;

    return true;
}

bool ZigbeeCentral::addressChanged(uint64_t IEEEaddr, uint16_t newShortAddr)
{
    std::lock_guard<std::mutex> lock(_peersMutex);
    // locate the peers for which the address changes:
    auto it = peersByIEEE.find(IEEEaddr);
    if (it == peersByIEEE.end()) return false;

    for (auto& peerPair : it->second)
    {
        const uint32_t oldAddr = peerPair.second->getAddress();
        _peers.erase(oldAddr);

        uint32_t epPart = peerPair.first;
        const uint32_t newAddr = (epPart << 16) | newShortAddr;
        peerPair.second->setAddress(newAddr);
        _peers[newAddr] = peerPair.second;
    }

    return false; // for now
}


uint32_t ZigbeeCentral::createDeviceForNode(ZigbeeNodeInfo& nodeInfo, uint8_t endPoint)
{
    const auto it = nodeInfo.EndPoints.find(endPoint);
    if (it == nodeInfo.EndPoints.end()) return -1;

    std::string modelType = nodeInfo.GetModelType(endPoint);

    uint32_t deviceType = 0;

    bool fillDevicesMap = false;
    std::shared_ptr<ZigbeeDevicesDescription> devices = std::dynamic_pointer_cast<ZigbeeDevicesDescription>(GD::family->getRpcDevices());
    if (!devices)
    {
        GD::out.printDebug("The RPC devices are not set");

        return 0;
    }

    if (_deviceTypeCounter <= 1)
    {
        _deviceTypeCounter = devices->getMaxDeviceType();
        fillDevicesMap = true;
        // TODO: should I leave some room here for the device counter to be able to distinguish from the generated ones and the ones loaded from xml files?
    }

    std::shared_ptr<HomegearDevice> modelDevice;

    {
        std::lock_guard<std::mutex> lock(_devicesTypeMutex);

        if (fillDevicesMap)
            devices->FillDevicesMap(_devicesTypeMap);

        //auto devTypePair = _devicesTypeMap.find(type);

        // since the devices can have different firmware versions even if they are the same type and model, supporting different classes and especially class versions, better create a new one each time

        //if (devTypePair == _devicesTypeMap.end())
        {
            deviceType = _deviceTypeCounter++;

            // new device type addition, here the device should be also dynamically specified
            GD::out.printInfo("Creating device with id: " + std::to_string(deviceType) + " device type: " + modelType);

            //_devicesTypeMap[type].insert(std::make_pair(version, deviceType)); // if there is one already there with the same version, it is not inserted
            //_devicesTypeMap[modelType][version] = deviceType; // no need to do that, it won't be used as a 'model'
        }
        /*
        else
        {
            deviceType = devTypePair->second;

            GD::out.printInfo("Using device with id: " + std::to_string(deviceType) + " device type: " + std::to_string(type));
        }
        */

        auto devTypeModelVersions = _devicesTypeMap.find(modelType);
        if (devTypeModelVersions == _devicesTypeMap.end())
        {
            // new device type addition, here the device should be also dynamically specified
            GD::out.printInfo("Did not find a model for device with id: " + std::to_string(deviceType) + " device type: " + modelType + ", using default");
            modelDevice = GD::family->GetModelDevice();
        }
        else
        {
            const uint32_t version = 1;
            uint32_t foundId = 0;

            auto devTypeModelVersion = devTypeModelVersions->second.find(version);
            if (devTypeModelVersion == devTypeModelVersions->second.end())
            {
                for (const auto& VerTypePair : devTypeModelVersions->second)
                {
                    if (VerTypePair.first == version)
                    {
                        foundId = VerTypePair.second;
                        break;
                    }
                    else
                    {
                        const uint16_t minVersion = VerTypePair.first & 0xffff;
                        const uint16_t maxVersion = VerTypePair.first >> 16;

                        if (version >= minVersion && (0 == maxVersion || version <= maxVersion))
                        {
                            foundId = VerTypePair.second;
                            break;
                        }
                    }
                }
            }
            else
            {
                foundId = devTypeModelVersion->second;
            }

            if (foundId) modelDevice = devices->find(foundId, version);

            if (modelDevice)
                GD::out.printInfo("Found a model for device with id: " + std::to_string(deviceType) + " device type: " + modelType + " Id for device model: " + std::to_string(foundId));
            else
                modelDevice = GD::family->GetModelDevice();
        }
    }

    if (_disposing)
        return deviceType;


    GD::out.printInfo("Adding device...");

    std::shared_ptr<HomegearDevice> device = devices->AddDevice(modelType, deviceType, &nodeInfo, endPoint, modelDevice);

    GD::out.printInfo("Device added, checking...");

    // check the device for correctness, maybe should call this only in debug mode?
    devices->CheckDevice(device);

    return deviceType;
}

PVariable ZigbeeCentral::createDevice(BaseLib::PRpcClientInfo clientInfo, int32_t deviceType, std::string serialNumber, int32_t address, int32_t firmwareVersion, std::string interfaceId)
{
    try
    {
        if (peerExists(serialNumber))
            return Variable::createError(-5, "This peer is already paired to this central.");

        std::shared_ptr<ZigbeePeer> peer = createPeer(deviceType, address, serialNumber, false);
        if (!peer || !peer->getRpcDevice())
            return Variable::createError(-6, "Unknown device type.");

        try
        {
            peer->save(true, true, false);
            peer->initializeCentralConfig();
            peer->setPhysicalInterfaceId(interfaceId);

            std::lock_guard<std::mutex> lock(_peersMutex);
            _peers[peer->getAddress()] = peer;
            _peersById[peer->getID()] = peer;
            if (!peer->getSerialNumber().empty())
                _peersBySerial[peer->getSerialNumber()] = peer;
            peersByIEEE[peer->IEEEaddr][peer->endPoint] = peer;
        }
        catch(const std::exception& ex)
        {
            GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
        }

        PVariable deviceDescriptions = std::make_shared<Variable>(VariableType::tArray);
        deviceDescriptions->arrayValue = peer->getDeviceDescriptions(clientInfo, true, std::map<std::string, bool>());
        std::vector<uint64_t> newIds{ peer->getID() };
        raiseRPCNewDevices(newIds, deviceDescriptions);


        AddPairingState(peer->getID(), "success", "l10n.zigbee.pairing.peerAddSuccess");

        GD::out.printMessage("Added peer 0x" + BaseLib::HelperFunctions::getHexString(peer->getID()) + ".");

        return std::make_shared<Variable>((uint32_t)peer->getID());
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return Variable::createError(-32500, "Unknown application error.");
}

void ZigbeeCentral::updatePeer(ZigbeeNodeInfo& nodeInfo, uint8_t endPoint, const std::string& interfaceID)
{
    if (_disposing || !nodeInfo.IsValid() || !nodeInfo.QueryCompleted)
        return;

    const std::string host_name = nodeInfo.GetSerial(endPoint);
    if (host_name.empty()) return;

    std::shared_ptr<ZigbeePeer> peer;

    //std::unique_lock<std::mutex> lockGuard(_pairMutex);

    const bool wakeupDev = nodeInfo.IsWakeupDevice();

    auto it = _peersBySerial.find(host_name);
    if (_peersBySerial.end() == it)
    {
        BaseLib::PRpcClientInfo clientInfo;

        GD::out.printMessage("Creating peer with serial: " + host_name);

        uint32_t deviceType = createDeviceForNode(nodeInfo, endPoint);

        GD::out.printInfo("Created device for serial: " + host_name);

        uint32_t epPart = endPoint;
        const uint32_t fakeAddr = (epPart << 16) | nodeInfo.srcAddr;

        createDevice(clientInfo, deviceType, host_name, fakeAddr, 0, interfaceID);

        GD::out.printInfo("Created device: " + host_name);

        peer = getPeer(host_name);
        if (peer)
        {
            //peer->newlyAdded = true;
            auto eit = nodeInfo.EndPoints.find(endPoint);
            if (eit != nodeInfo.EndPoints.end())
            {
                const std::string name = eit->second.modelIdentifier + std::string("_") + BaseLib::HelperFunctions::getHexString(endPoint);
                peer->setName(name);
            }
        }
        else GD::out.printDebug("Couldn't find device: " + host_name + " to set its name");
    }
    else
    {

        GD::out.printMessage("Updating peer with serial: " + host_name);

        peer = getPeer(host_name);
    }

    if (peer)
    {
        //if(_peersBySerial.find(peer->getSerialNumber()) != _peersBySerial.end()) _peersBySerial.erase(peer->getSerialNumber());
        //if(_peersById.find(peer->getID()) != _peersById.end()) _peersById.erase(peer->getID());
        //lockGuard.unlock();

        {
            std::lock_guard<std::mutex> lock(peer->infoMutex);

            peer->nodeInfo = nodeInfo;

            // no need to carry in each peer the whole info (also load and save it), drop the info not relevant to the peer's end point
            for (uint8_t ep : peer->nodeInfo.EndPointsIds)
            {
                if (ep != peer->endPoint)
                    peer->nodeInfo.EndPoints.erase(ep);
            }
            peer->nodeInfo.EndPointsIds.resize(1);
            peer->nodeInfo.EndPointsIds[0] = peer->endPoint;

            peer->initializeServiceVariables();
        }

        peer->save(true, true, false);

        if (!peer->getPacketsSent)
        {
            _bl->threadManager.start(peer->getThread, false, &ZigbeePeer::SendAllGetPackets, peer, wakeupDev);
        }
    }
}


bool ZigbeeCentral::RemovePeerFromArrays(uint64_t id)
{
    std::shared_ptr<ZigbeePeer> peer(getPeer(id));
    if(!peer)
        return false;

    std::lock_guard<std::mutex> lock(_peersMutex);
    _peersBySerial.erase(peer->getSerialNumber());
    _peersById.erase(id);
    _peers.erase(peer->getAddress());

    peersByIEEE[peer->IEEEaddr].erase(peer->endPoint);
    if (peersByIEEE[peer->IEEEaddr].empty())
        peersByIEEE.erase(peer->IEEEaddr);

    return true;
}

void ZigbeeCentral::AddPeerIntoArrays(std::shared_ptr<ZigbeePeer> peer)
{
    std::lock_guard<std::mutex> lock(_peersMutex);

    _peersBySerial[peer->getSerialNumber()] = peer;
    _peersById[peer->getID()] = peer;
    _peers[peer->getAddress()] = peer;

    peersByIEEE[peer->IEEEaddr][peer->endPoint] = peer;
}

bool ZigbeeCentral::WaitForPeer(std::shared_ptr<ZigbeePeer>& peer, unsigned int use_count)
{
    int32_t i = 0;

    while(peer.use_count() > use_count && i < 600)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ++i;
    }

    if(i == 600)
    {
        GD::out.printError("Error: Peer refresh took too long.");

        return false;
    }

    return true;
}


PVariable ZigbeeCentral::deleteDevice(BaseLib::PRpcClientInfo clientInfo, std::string serialNumber, int32_t flags)
{
    try
    {
        if(serialNumber.empty())
            return Variable::createError(-2, "Unknown device.");
        std::shared_ptr<ZigbeePeer> peer(getPeer(serialNumber));
        if(!peer)
            return std::make_shared<Variable>(VariableType::tVoid);

        return deleteDevice(clientInfo, peer->getID(), flags);
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return Variable::createError(-32500, "Unknown application error.");
}

PVariable ZigbeeCentral::deleteDevice(BaseLib::PRpcClientInfo clientInfo, uint64_t peerID, int32_t flags)
{
    try
    {
        if(peerID == 0)
            return Variable::createError(-2, "Unknown device.");
        std::shared_ptr<ZigbeePeer> peer(getPeer(peerID));
        if(!peer)
            return std::make_shared<Variable>(VariableType::tVoid);
        uint64_t id = peer->getID();

        deletePeer(id);

        if(peerExists(id))
            return Variable::createError(-1, "Error deleting peer. See log for more details.");

        return std::make_shared<Variable>(VariableType::tVoid);
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return Variable::createError(-32500, "Unknown application error.");
}

void ZigbeeCentral::pairingModeTimer(int32_t duration, bool debugOutput)
{
    try
    {
        _pairing = true;
        if(debugOutput)
            GD::out.printInfo("Info: Pairing mode enabled.");
        _timeLeftInPairingMode = duration;
        int64_t startTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        int64_t timePassed = 0;
        while(timePassed < ((int64_t)duration * 1000) && !_stopPairingModeThread)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            timePassed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - startTime;
            _timeLeftInPairingMode = duration - (timePassed / 1000);
        }
        _timeLeftInPairingMode = 0;

        //for (auto &physInt : GD::physicalInterfaces)
        //{
        //    physInt.second->AbortInclusion();
        //}
        GD::interfaces->AbortInclusion();

        _allowDeletePeer = false;
        _pairing = false;
        if(debugOutput)
            GD::out.printInfo("Info: Pairing mode disabled.");
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}



void ZigbeeCentral::AddPairingMessage(const std::string& messageId, const std::string& variable)
{
    auto pairingMsg = std::make_shared<PairingMessage>(messageId);

    if (!variable.empty())
        pairingMsg->variables.emplace_back(variable);

    std::lock_guard<std::mutex> newPeersGuard(_newPeersMutex);
    _pairingMessages.emplace_back(pairingMsg);
}


void ZigbeeCentral::AddPairingState(uint64_t peerId, const std::string& state, const std::string& messageId)
{
    auto pairingState = std::make_shared<PairingState>();
    pairingState->peerId = peerId;
    pairingState->state = "success";
    pairingState->messageId = messageId;

    std::lock_guard<std::mutex> newPeersGuard(_newPeersMutex);
    _newPeers[BaseLib::HelperFunctions::getTime()].emplace_back(std::move(pairingState));
}

PVariable ZigbeeCentral::getPairingState(BaseLib::PRpcClientInfo clientInfo)
{
    try
    {
        auto states = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);

        states->structValue->emplace("pairingModeEnabled", std::make_shared<BaseLib::Variable>(_pairing));
        states->structValue->emplace("pairingModeEndTime", std::make_shared<BaseLib::Variable>(BaseLib::HelperFunctions::getTimeSeconds() + _timeLeftInPairingMode));

        {
            std::lock_guard<std::mutex> newPeersGuard(_newPeersMutex);

            auto pairingMessages = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tArray);
            pairingMessages->arrayValue->reserve(_pairingMessages.size());
            for(auto& message : _pairingMessages)
            {
                auto pairingMessage = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
                pairingMessage->structValue->emplace("messageId", std::make_shared<BaseLib::Variable>(message->messageId));
                auto variables = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tArray);
                variables->arrayValue->reserve(message->variables.size());
                for(auto& variable : message->variables)
                {
                    variables->arrayValue->emplace_back(std::make_shared<BaseLib::Variable>(variable));
                }
                pairingMessage->structValue->emplace("variables", variables);
                pairingMessages->arrayValue->push_back(pairingMessage);
            }
            states->structValue->emplace("general", std::move(pairingMessages));

            for(auto& element : _newPeers)
            {
                for(auto& peer : element.second)
                {
                    auto peerState = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
                    peerState->structValue->emplace("state", std::make_shared<BaseLib::Variable>(peer->state));
                    peerState->structValue->emplace("messageId", std::make_shared<BaseLib::Variable>(peer->messageId));
                    auto variables = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tArray);
                    variables->arrayValue->reserve(peer->variables.size());
                    for(auto& variable : peer->variables)
                    {
                        variables->arrayValue->emplace_back(std::make_shared<BaseLib::Variable>(variable));
                    }
                    peerState->structValue->emplace("variables", variables);
                    states->structValue->emplace(std::to_string(peer->peerId), std::move(peerState));
                }
            }
        }

        return states;
    }
    catch(const std::exception& ex)
    {
        _bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return Variable::createError(-32500, "Unknown application error.");
}

PVariable ZigbeeCentral::setInstallMode(BaseLib::PRpcClientInfo clientInfo, bool on, uint32_t duration, BaseLib::PVariable metadata, bool debugOutput)
{
    try
    {
        if (on && GD::interfaces->IsInNetworkAdminMode())
            return Variable::createError(-32500, "Still in network admin mode. Please wait until it's finished.");

        double timeSinceLastStage = GD::interfaces->TimeSinceLastAdminStage();
        /*
        for (auto &physInt : GD::physicalInterfaces)
        {
            if (physInt.second->IsInNetworkAdminMode() && on)
                return Variable::createError(-32500, "Still in network admin mode. Please wait until it's finished.");

            timeSinceLastStage = std::min(timeSinceLastStage, physInt.second->TimeSinceLastAdminStage());
        }
        */

        if (on)
        {

            // check to see if there is some peer still needed configuration
            {
                std::lock_guard<std::mutex> lock(_peersMutex);

                bool configPending = false;
                bool initializationOn = false;

                for(std::map<uint64_t, std::shared_ptr<BaseLib::Systems::Peer>>::iterator i = _peersById.begin(); i != _peersById.end(); ++i)
                {
                    //Necessary, because peers can be assigned to multiple virtual devices
                    if(i->second->getParentID() != _deviceId)
                        continue;

                    std::shared_ptr<ZigbeePeer> peer(std::dynamic_pointer_cast<ZigbeePeer>(i->second));

                    if (peer->isConfigPending())
                        configPending = true;
                    else if (peer->isGetStillOn())
                        initializationOn = true;
                }

                if (timeSinceLastStage < 60)
                {
                    if (configPending)
                        return Variable::createError(-32500, "A peer has its configuration pending. Please wait until it's finished.");
                    else if (initializationOn)
                        return Variable::createError(-32500, "Information is still requested for a peer. Please wait until it's finished.");
                    else if (timeSinceLastStage < 15)
                        return Variable::createError(-32500, "Less than 15 seconds passed since last network administration stage. Please wait a little until retrying.");
                }
            }
        }
        else if (timeSinceLastStage < 30)
            return Variable::createError(-32500, "Less than 30 seconds passed since last network administration stage. Please wait a little until retrying.");

        std::lock_guard<std::mutex> pairingModeGuard(_pairingModeThreadMutex);
        if(_disposing)
            return Variable::createError(-32500, "Central is disposing.");
        _stopPairingModeThread = true;
        _bl->threadManager.join(_pairingModeThread);
        _stopPairingModeThread = false;
        _timeLeftInPairingMode = 0;

        if(on && duration >= 5)
        {
            std::lock_guard<std::mutex> newPeersGuard(_newPeersMutex);
            _newPeers.clear();
            _pairingMessages.clear();
        }

        /*
        for (auto &physInt : GD::physicalInterfaces)
        {
            if (on)
            {
                physInt.second->PairOn(duration);
            }
            else
                physInt.second->AbortInclusion();
        }
        */
        if (on)
            GD::interfaces->PairOn(duration);
        else
            GD::interfaces->AbortInclusion();

        if(on && duration >= 5)
        {
            _timeLeftInPairingMode = duration; //It's important to set it here, because the thread often doesn't completely initialize before getInstallMode requests _timeLeftInPairingMode
            _bl->threadManager.start(_pairingModeThread, true, &ZigbeeCentral::pairingModeTimer, this, duration, debugOutput);
        }
        return std::make_shared<Variable>(VariableType::tVoid);
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return Variable::createError(-32500, "Unknown application error.");
}

std::shared_ptr<Variable> ZigbeeCentral::setRemoveMode(BaseLib::PRpcClientInfo clientInfo, bool on, uint64_t peerID, uint32_t duration, bool debugOutput)
{
    try
    {
        if (on && GD::interfaces->IsInNetworkAdminMode())
            return Variable::createError(-32500, "Still in network admin mode. Please wait until it's finished.");

        double timeSinceLastStage = GD::interfaces->TimeSinceLastAdminStage();

        /*
        double timeSinceLastStage = 120;
        for (auto &physInt : GD::physicalInterfaces)
        {
            if (physInt.second->IsInNetworkAdminMode() && on)
                return Variable::createError(-32500, "Still in network admin mode. Please wait until it's finished.");

            timeSinceLastStage = std::min(timeSinceLastStage, physInt.second->TimeSinceLastAdminStage());
        }
        */

        if (on)
        {
            // check to see if there is some peer still needed configuration
            {
                std::lock_guard<std::mutex> lock(_peersMutex);

                bool configPending = false;
                bool initializationOn = false;

                for(std::map<uint64_t, std::shared_ptr<BaseLib::Systems::Peer>>::iterator i = _peersById.begin(); i != _peersById.end(); ++i)
                {
                    //Necessary, because peers can be assigned to multiple virtual devices
                    if(i->second->getParentID() != _deviceId)
                        continue;

                    std::shared_ptr<ZigbeePeer> peer(std::dynamic_pointer_cast<ZigbeePeer>(i->second));

                    if (peer->isConfigPending())
                        configPending = true;
                    else if (peer->isGetStillOn())
                        initializationOn = true;
                }

                if (timeSinceLastStage < 60)
                {
                    if (configPending)
                        return Variable::createError(-32500, "A peer has its configuration pending. Please wait until it's finished.");
                    else if (initializationOn)
                        return Variable::createError(-32500, "Information is still requested for a peer. Please wait until it's finished.");
                    else if (timeSinceLastStage < 15)
                        return Variable::createError(-32500, "Less than 15 seconds passed since last network administration stage. Please wait a little until retrying.");
                }
            }
        }
        else if (timeSinceLastStage < 30)
            return Variable::createError(-32500, "Less than 30 seconds passed since last network administration stage. Please wait a little until retrying.");

        uint64_t IEEEaddr = 0;
        uint16_t shortAddr = 0;
        {
            std::shared_ptr<ZigbeePeer> peer(getPeer(peerID));
            if(!peer)
                return Variable::createError(-32500, "Peer does not exist.");

            IEEEaddr = peer->IEEEaddr;
            shortAddr = (peer->getAddress() & 0xFFFF);
        }

        std::lock_guard<std::mutex> pairingModeGuard(_pairingModeThreadMutex);
        if(_disposing)
            return Variable::createError(-32500, "Central is disposing.");
        _stopPairingModeThread = true;
        _bl->threadManager.join(_pairingModeThread);
        _stopPairingModeThread = false;
        _timeLeftInPairingMode = 0;

        _allowDeletePeer = (on && duration >= 5) ? true : false;

        /*
        for (auto &physInt : GD::physicalInterfaces)
        {
            if (on)
                physInt.second->PairOff(IEEEaddr, shortAddr, duration);
            else
                physInt.second->AbortInclusion();
        }
        */
        if (on)
            GD::interfaces->PairOff(IEEEaddr, shortAddr, duration);
        else
            GD::interfaces->AbortInclusion();

        if(on && duration >= 5)
        {
            _timeLeftInPairingMode = duration; //It's important to set it here, because the thread often doesn't completely initialize before getInstallMode requests _timeLeftInPairingMode
            _bl->threadManager.start(_pairingModeThread, true, &ZigbeeCentral::pairingModeTimer, this, duration, debugOutput);
        }
        return std::make_shared<Variable>(VariableType::tVoid);
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return Variable::createError(-32500, "Unknown application error.");
}



bool ZigbeeCentral::onPacketReceived(std::string& senderID, std::shared_ptr<BaseLib::Systems::Packet> packet)
{
    try
    {
        if(_disposing)
            return false;
        PZigbeePacket myPacket(std::static_pointer_cast<ZigbeePacket>(packet));
        if(!myPacket)
            return false;

        const uint32_t addr = myPacket->senderAddress();

        if(_bl->debugLevel >= 4)
            _bl->out.printInfo(BaseLib::HelperFunctions::getTimeString(myPacket->getTimeReceived()) + " Zigbee packet received by the central - Sender address: 0x" + BaseLib::HelperFunctions::getHexString(addr, 8));

        std::shared_ptr<ZigbeePeer> peer(getPeer(addr));

        if(!peer)
        {
            GD::out.printDebug("Central: Peer does not exist!");

            return false;
        }

        /*
        if(senderID != peer->getPhysicalInterfaceId()) {
            GD::out.printDebug("Central: Wrong interface id for peer!");
            return false;
        }
        */

        peer->packetReceived(myPacket);

        return true;
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return false;
}

bool ZigbeeCentral::IsWakeup(uint32_t addr)
{
    try
    {
        std::shared_ptr<ZigbeePeer> peer(getPeer(addr));

        if (peer)
            return peer->wakeupDevice();
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return true;
}

void ZigbeeCentral::NotifyTimeout(PZigbeePacket packet)
{
    try
    {
        const uint32_t addr = packet->destinationAddress();

        if(_bl->debugLevel >= 4)
            _bl->out.printInfo(BaseLib::HelperFunctions::getTimeString(packet->getTimeReceived()) + " Notifying about timeout for Zigbee packet - Dst address: 0x" + BaseLib::HelperFunctions::getHexString(addr, 8));

        std::shared_ptr<ZigbeePeer> peer(getPeer(addr));
        if(!peer)
        {
            GD::out.printDebug("Central: Peer does not exist!");

            return;
        }
        peer->NotifyTimeout(packet);
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void ZigbeeCentral::NotifyError(PZigbeePacket packet, const ZigbeeCommands::ZCLFrame& zclFrame)
{
    try
    {
        const uint32_t addr = packet->destinationAddress();

        if(_bl->debugLevel >= 4)
            _bl->out.printInfo(BaseLib::HelperFunctions::getTimeString(packet->getTimeReceived()) + " Notifying about error for Zigbee packet - Dst address: 0x" + BaseLib::HelperFunctions::getHexString(addr, 8));

        std::shared_ptr<ZigbeePeer> peer(getPeer(addr));
        if(!peer)
        {
            GD::out.printDebug("Central: Peer does not exist!");

            return;
        }
        peer->NotifyError(packet, zclFrame);
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

}
