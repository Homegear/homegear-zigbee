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

#include "ZigbeeDevicesDescription.h"
#include "GD.h"
#include "PhysicalInterfaces/ZCL.h"

#include <locale>

namespace Zigbee
{


ZigbeeParameter::ZigbeeParameter(BaseLib::SharedObjects* baseLib, PParameterGroup parent, const DeviceParameter* param)
    : Parameter(baseLib, parent), isConfig(false), channelNo(-1), isReportingConfig(false)
{
    if (param)
        deviceParameter = *param;
}

ZigbeeParameter::~ZigbeeParameter()
{
}

ZigbeeDevicesDescription::ZigbeeDevicesDescription(BaseLib::SharedObjects* baseLib, IDevicesEventSink* eventHandler, int32_t family)
    : BaseLib::DeviceDescription::Devices(baseLib, eventHandler, family)
{
    //ctor
}

ZigbeeDevicesDescription::~ZigbeeDevicesDescription()
{
    //dtor
}

uint64_t ZigbeeDevicesDescription::getMaxDeviceType() const
{
    if (_devices.empty())
        return 1;

    uint64_t res = 1;
    for (const auto& dev : _devices)
    {
        for(const auto& sdev : dev->supportedDevices)
            res = std::max(res, sdev->typeNumber + 1);
    }

    return res;
}

void ZigbeeDevicesDescription::FillDevicesMap(std::map<std::string, std::map<uint32_t, uint32_t>>& devicesTypeMap)
{
    if (_devices.empty())
        return;

    for (const auto& dev : _devices)
    {
        for(const auto& sdev : dev->supportedDevices)
        {
            uint32_t version = sdev->maxFirmwareVersion;
            version <<= 16;
            version |= sdev->minFirmwareVersion;

            devicesTypeMap[sdev->id].insert(std::make_pair(version, sdev->typeNumber));

            _bl->out.printInfo("Adding device: " + sdev->id + " with type number: " + std::to_string(sdev->typeNumber));
        }
    }
}

void ZigbeeDevicesDescription::CleanupDevice(BaseLib::DeviceDescription::PHomegearDevice device)
{
    // erase all functions that are added here
    for (auto it = device->functions.begin(); it != device->functions.end();)
    {
        std::shared_ptr<ZigbeeFunction> f = std::dynamic_pointer_cast<ZigbeeFunction>(it->second);
        if (f)
            it = device->functions.erase(it);
        else ++it;
    }

    // and the packets
    for (auto it = device->packetsById.begin(); it != device->packetsById.end();)
    {
        std::shared_ptr<ZigbeeDescPacket> p = std::dynamic_pointer_cast<ZigbeeDescPacket>(it->second);
        if (p)
            it = device->packetsById.erase(it);
        else ++it;
    }

    for (auto it = device->packetsByMessageType.begin(); it != device->packetsByMessageType.end();)
    {
        std::shared_ptr<ZigbeeDescPacket> p = std::dynamic_pointer_cast<ZigbeeDescPacket>(it->second);
        if (p)
            it = device->packetsByMessageType.erase(it);
        else ++it;
    }

    for (auto it = device->packetsByFunction1.begin(); it != device->packetsByFunction1.end();)
    {
        std::shared_ptr<ZigbeeDescPacket> p = std::dynamic_pointer_cast<ZigbeeDescPacket>(it->second);
        if (p)
            it = device->packetsByFunction1.erase(it);
        else ++it;
    }

    for (auto it = device->packetsByFunction2.begin(); it != device->packetsByFunction2.end();)
    {
        std::shared_ptr<ZigbeeDescPacket> p = std::dynamic_pointer_cast<ZigbeeDescPacket>(it->second);
        if (p)
            it = device->packetsByFunction2.erase(it);
        else ++it;
    }

    // Currently this is not used in the Zigbee module
    // afaik, this is used only in HomeMatic modules, I don't know yet the purpose
    for (auto it = device->valueRequestPackets.begin(); it != device->valueRequestPackets.end(); ++it)
    {
        for (auto iit = it->second.begin(); iit != it->second.end();)
        {
            std::shared_ptr<ZigbeeDescPacket> p = std::dynamic_pointer_cast<ZigbeeDescPacket>(iit->second);
            if (p)
                iit = it->second.erase(iit);
            else ++iit;
        }
    }

    //GD::out.printDebug("There are " + std::to_string(device->packetsByMessageType.size()) + " packets already existing");
}


BaseLib::DeviceDescription::PHomegearDevice ZigbeeDevicesDescription::CloneDevice(const BaseLib::DeviceDescription::PHomegearDevice device)
{
    BaseLib::DeviceDescription::PHomegearDevice clonedDevice;

    clonedDevice = std::make_shared<BaseLib::DeviceDescription::HomegearDevice>(_bl);

    // just copy it, but then 'clone' everything that can be altered
    *clonedDevice = *device;

    clonedDevice->functions.clear();
    for (auto& func : device->functions)
    {
        // don't clone an empty one?
        //if ((func.second->configParameters && !func.second->configParameters->parameters.empty())
        //    || (func.second->variables && !func.second->variables->parameters.empty()))
        //{
            auto clone = CloneFunction(func.second);
            clonedDevice->functions[clone->channel] = clone;
        //}
    }


    return clonedDevice;
}

BaseLib::DeviceDescription::PFunction ZigbeeDevicesDescription::CloneFunction(const BaseLib::DeviceDescription::PFunction func)
{
    if (!func) return func;

    BaseLib::DeviceDescription::PFunction clonedFunc;

    std::shared_ptr<ZigbeeFunction> f = std::dynamic_pointer_cast<ZigbeeFunction>(func);
    if (f)
        clonedFunc = std::make_shared<ZigbeeFunction>(_bl);
    else
        clonedFunc = std::make_shared<BaseLib::DeviceDescription::Function>(_bl);

    // just copy it, but then 'clone' everything that could be altered
    *clonedFunc = *func;

    clonedFunc->configParameters = std::make_shared<BaseLib::DeviceDescription::ConfigParameters>(_bl);
    clonedFunc->variables = std::make_shared<BaseLib::DeviceDescription::Variables>(_bl);

    if (func->configParameters)
        for (const auto& param : func->configParameters->parameters)
        {
            clonedFunc->configParameters->parametersOrdered.push_back(param.second); // a vector
            clonedFunc->configParameters->parameters[param.second->id] = param.second;
        }

    if (func->variables)
        for (const auto& param : func->variables->parameters)
        {
            clonedFunc->variables->parametersOrdered.push_back(param.second); // a vector
            clonedFunc->variables->parameters[param.second->id] = param.second;
        }

    return clonedFunc;
}


void ZigbeeDevicesDescription::RemoveDeviceType(uint32_t deviceType, uint32_t version)
{
    std::lock_guard<std::mutex> lock(devicesMutex);

    bool found = false;
    for(auto it = _devices.begin(); it != _devices.end() && !found; ++it)
    {
        for(unsigned int j = 0; j < (*it)->supportedDevices.size(); ++j)
        {
            if((*it)->supportedDevices[j]->matches(deviceType, version))
            {
                _devices.erase(it);

                found = true;

                break;
            }
        }
    }
}


BaseLib::DeviceDescription::PHomegearDevice ZigbeeDevicesDescription::AddDevice(const std::string& id, uint32_t deviceType, const ZigbeeNodeInfo* info, uint8_t endPoint, const BaseLib::DeviceDescription::PHomegearDevice modelDevice)
{
    BaseLib::DeviceDescription::PHomegearDevice device;

    if (!info)
        return device;

    // it might be added again when all class versions are retrieved

    int deviceIndex = -1;


    PSupportedDevice supportedDevice;

    GD::out.printDebug("Trying to find a supported device description");

    {
        std::lock_guard<std::mutex> lock(devicesMutex);

        for(unsigned int i = 0; i < _devices.size(); ++i)
        {
            for(unsigned int j = 0; j < _devices[i]->supportedDevices.size(); ++j)
            {
                if(_devices[i]->supportedDevices[j]->matches(deviceType, 1))
                {
                    deviceIndex = i;
                    //_devices[i].reset(); // not correct if there are devices loaded from xml files

                    device = CloneDevice(_devices[i]);
                    device->hasBattery = info->HasBattery();
                    supportedDevice = device->supportedDevices[j];

                    // if there are, a 'cleanup' should be done, for things added by a previous 'AddDevice' (for a previous version)
                    // but the things loaded from xml should be left alone

                    CleanupDevice(device);

                    break;
                }
            }
        }
    }


    if (deviceIndex < 0)
    {
        GD::out.printDebug("No supported device description was found, creating a new generic one");

        device = std::make_shared<HomegearDevice>(_bl);
        // should copy here some things from modelDevice

        device->runProgram = modelDevice->runProgram;
        device->hasBattery = info->HasBattery();

        // start filling up info from service and classes
        device->version = 0;

        supportedDevice = std::make_shared<SupportedDevice>(_bl);

        supportedDevice->id = id;
        supportedDevice->typeNumber = deviceType;

        // TODO: fill it with more info

        device->supportedDevices.push_back(supportedDevice);
    }



    {
        std::lock_guard<std::mutex> lock(devicesMutex);

        // properties
        device->receiveModes = (info->IsWakeupDevice() ? BaseLib::DeviceDescription::HomegearDevice::ReceiveModes::Enum::wakeUp : BaseLib::DeviceDescription::HomegearDevice::ReceiveModes::Enum::always);

        //if (deviceIndex < 0)
        _devices.push_back(device);
        //else
        // _devices[deviceIndex] = device;
    }

    GD::out.printDebug("Adding channel zero");

    // copy the '0' function
    uint32_t channelNo = 0;

    if (modelDevice->functions.find(0) == modelDevice->functions.end())
    {
        PFunction function = std::make_shared<ZigbeeFunction>(_bl);
        function->channel = 0;
        function->type = std::string("ZIGBEE_CHANNEL_0");
        device->functions[0] = function;
    }
    else
        device->functions[0] = CloneFunction(modelDevice->functions[0]);


    AddConfigReadonlyBoolParameter(device->functions[0], "ROUTER");
    AddConfigReadonlyBoolParameter(device->functions[0], "MAINS_POWERED");
    AddConfigReadonlyBoolParameter(device->functions[0], "LISTENING");

    AddConfigUint16ReadonlyParameter(device->functions[0], "SHORT_ADDR");
    AddConfigUint16ReadonlyParameter(device->functions[0], "END_POINT");

    AddReadonlyBoolParameter(device->functions[0], "CONFIG_PENDING");

    AddReadonlyBoolParameter(device->functions[0], "UNREACH");
    AddBoolParameter(device->functions[0], "STICKY_UNREACH");
    if (info->HasBattery())
        AddReadonlyBoolParameter(device->functions[0], "LOWBAT");

    // TODO: maybe deep copy or something, because a match between some ids in the 'model' and the dynamically generated can mess things up by altering the 'model'

    // now copy the other functions

    GD::out.printDebug("Cloning functions");

    for (const auto& func : modelDevice->functions)
    {
        if (0 == func.second->channel)
            continue; // already dealt with
        device->functions[func.second->channel] = CloneFunction(func.second);
    }

    // now the rest

    GD::out.printDebug("Copying packets by id");

    for (auto it = modelDevice->packetsById.begin(); it != modelDevice->packetsById.end(); ++it)
    {
        device->packetsById[it->second->id] = it->second;
    }

    //GD::out.printDebug("There are " + std::to_string(modelDevice->packetsByMessageType.size()) + " packets already existing in the model device");
    GD::out.printDebug("Copying packets by message type");

    for (auto it = modelDevice->packetsByMessageType.begin(); it != modelDevice->packetsByMessageType.end(); ++it)
    {
        device->packetsByMessageType.insert(std::pair<uint32_t, BaseLib::DeviceDescription::PPacket>(it->second->type, it->second));
    }

    GD::out.printDebug("Copying packets by function");

    for (auto it = modelDevice->packetsByFunction1.begin(); it != modelDevice->packetsByFunction1.end(); ++it)
    {
        device->packetsByFunction1.insert(std::pair<std::string, BaseLib::DeviceDescription::PPacket>(it->second->function1, it->second));
    }

    for (auto it = modelDevice->packetsByFunction2.begin(); it != modelDevice->packetsByFunction2.end(); ++it)
    {
        device->packetsByFunction2.insert(std::pair<std::string, BaseLib::DeviceDescription::PPacket>(it->second->function2, it->second));
    }

    GD::out.printDebug("Copying value request packets");

    for (auto it = modelDevice->valueRequestPackets.begin(); it != modelDevice->valueRequestPackets.end(); ++it)
    {
        for (auto iit = it->second.begin(); iit != it->second.end(); ++iit)
        {
            device->valueRequestPackets[it->first][iit->first] = iit->second;
        }
    }

    // keep the channels separated from the ones from xml files (except the channel 0)
    for (const auto& func : device->functions)
        channelNo = std::max(channelNo, func.second->channel);

    // deal with dynamically generated attributes/commands
    auto itep = info->EndPoints.find(endPoint);
    if (itep == info->EndPoints.end())
        return device;

    ++channelNo;

    std::shared_ptr<ClustersInfo> xmlClustersInfo;
    if (GD::family)
        xmlClustersInfo = GD::family->_clustersInfo;
    if (!xmlClustersInfo)
        return device;

    for (uint16_t clusterid : itep->second.AllClusterList)
    {
        // don't add channels for those not described in the xml file, we need names (also commands params), which cannot be obtained by interrogating the device (even if it supports that)!

        // add channels only for 'server' clusters
        // TODO: what to do about the client ones?
        auto clusterXmlIt = xmlClustersInfo->serversClustersInfoMap.find(clusterid);
        auto clusterDevIt = itep->second.ServersClustersInfoMap.find(clusterid);
        if (clusterXmlIt != xmlClustersInfo->serversClustersInfoMap.end() && clusterDevIt != itep->second.ServersClustersInfoMap.end())
        {
            const ClustersInfo::ClusterInfoExt *pXmlCluster = &clusterXmlIt->second;
            const ZigbeeNodeInfo::EndpointInfo::ClusterInfo* pDeviceCluster = &clusterDevIt->second;

            const bool containsReportable =
                            AddFunction(device, channelNo, pDeviceCluster, pXmlCluster);
            ++channelNo;

            // TODO: after having attribute reporting configuring implemented, uncomment the following
            // still needs parsing the packets and generating and sending the 'set' and 'get' packets

            // if the above added channel contains 'reportable' attributes
            // add another channel for settings of reporting parameters for those reportable attributes
            if (containsReportable)
            {
                AddReportingConfigFunction(device, channelNo, pDeviceCluster, pXmlCluster);
                ++channelNo;
            }
        }
    }

    return device;
}


void ZigbeeDevicesDescription::AddReportingConfigFunction(std::shared_ptr<HomegearDevice> device, int channelNo, const ZigbeeNodeInfo::EndpointInfo::ClusterInfo* pDeviceCluster, const ClustersInfo::ClusterInfoExt* pXmlCluster)
{
    if (!pDeviceCluster || !pXmlCluster)
        return;

    PFunction function = std::make_shared<ZigbeeFunction>(_bl);
    function->channel = channelNo;
    function->type = std::string("ZIGBEE_CHANNEL_REPORTING_CONFIG_") + pXmlCluster->name/*std::to_string(channelNo)*/;

    function->configParametersId = std::string("zigbee_reporting_config_") + pXmlCluster->name/*std::to_string(channelNo)*/;
    if (!function->configParameters)
        function->configParameters = std::make_shared<ConfigParameters>(_bl);

    // parameters ordered and parameters in 'variables' member for function should be filled, too
    // similarly for configParameters

    // also those should appear in 'parameterGroups'?

    device->functions[function->channel] = function;

    // deal with each command from the class

    // each one will add a 'packet'



    // ************************************************************************************
    // Join the attributes and commands
    // ************************************************************************************

    // join them in here
    std::map<std::string, DeviceParameter> parametersMap;

    // walk over all supported attributes
    for (uint16_t attrid : pDeviceCluster->attrsList)
    {
        auto it = pXmlCluster->attrsMap.find(attrid);
        if (it == pXmlCluster->attrsMap.end())
        {
            GD::out.printDebug("Supported attribte not found in the xml file: 0x" + BaseLib::HelperFunctions::getHexString(attrid));
            continue;
        }

        if (!it->second.IsReportable())
            continue;

        const std::string name = it->second.name;

        //GD::out.printInfo("Merging Param: " + name);

        parametersMap[name].attrInfo = &it->second;
        parametersMap[name].type = parametersMap[name].attrInfo->type;
    }


    // TODO: maybe do some better joining than this?

    // ************************************************************************************
    // Now add the parameters for each joined parameter
    // ************************************************************************************

    for (const auto& jparam : parametersMap)
    {
        if (jparam.second.attrInfo)
        {
            std::shared_ptr<ZigbeeParameter> parameterMinReportingInterval = std::make_shared<ZigbeeParameter>(_bl, function->configParameters, &jparam.second);
            std::shared_ptr<ZigbeeParameter> parameterMaxReportingInterval = std::make_shared<ZigbeeParameter>(_bl, function->configParameters, &jparam.second);
            std::shared_ptr<ZigbeeParameter> parameterReportableChange;
            if (jparam.second.attrInfo->IsAnalog()) parameterReportableChange = std::make_shared<ZigbeeParameter>(_bl, function->configParameters, &jparam.second);

            parameterMinReportingInterval->channelNo = channelNo;
            parameterMaxReportingInterval->channelNo = channelNo;
            if (parameterReportableChange) parameterReportableChange->channelNo = channelNo;

            parameterMinReportingInterval->isConfig = true;
            parameterMaxReportingInterval->isConfig = true;
            if (parameterReportableChange) parameterReportableChange->isConfig = true;

            parameterMinReportingInterval->isReportingConfig = true;
            parameterMaxReportingInterval->isReportingConfig = true;
            if (parameterReportableChange) parameterReportableChange->isReportingConfig = true;

            parameterMinReportingInterval->id = pXmlCluster->name + "_REPORTING_CONFIG." + jparam.first + ".MIN_REPORTING_INTERVAL";
            parameterMaxReportingInterval->id = pXmlCluster->name + "_REPORTING_CONFIG." + jparam.first + ".MAX_REPORTING_INTERVAL";
            if (parameterReportableChange) parameterReportableChange->id = pXmlCluster->name + "_REPORTING_CONFIG." + jparam.first + ".REPORTABLE_CHANGE";

            //GD::out.printInfo("Adding Param: " + parameter->id);

            parameterMinReportingInterval->readable = true;
            parameterMaxReportingInterval->readable = true;
            if (parameterReportableChange) parameterReportableChange->readable = true;

            parameterMinReportingInterval->writeable = true;
            parameterMaxReportingInterval->writeable = true;
            if (parameterReportableChange) parameterReportableChange->writeable = true;

            if (parameterReportableChange) parameterReportableChange->deviceParameter.lastParam = true;
            else parameterMaxReportingInterval->deviceParameter.lastParam = true;

            // set types, except for the 'reportable change', where it's the same as for the attribute
            parameterMinReportingInterval->deviceParameter.type = 0x21; // uint16
            parameterMaxReportingInterval->deviceParameter.type = 0x21; // uint16



            // the 'GET' packet
            std::shared_ptr<Parameter::Packet> paramPacket = std::make_shared<Parameter::Packet>();
            paramPacket->id = pXmlCluster->name + "_REPORTING_CONFIG_" + jparam.first + "_GET";
            paramPacket->type = Parameter::Packet::Type::Enum::get;

            parameterMinReportingInterval->getPackets.push_back(paramPacket);
            parameterMaxReportingInterval->getPackets.push_back(paramPacket);
            if (parameterReportableChange) parameterReportableChange->getPackets.push_back(paramPacket);

            BaseLib::DeviceDescription::PPacket devPacket = std::make_shared<ZigbeeDescPacket>(_bl);
            devPacket->channel = channelNo;
            devPacket->direction = Packet::Direction::fromCentral;

            devPacket->id = paramPacket->id;
            devPacket->type = pXmlCluster->clusterId;
            devPacket->type <<= 8;
            devPacket->type |= 0;
            devPacket->subtype = 0x08; // Read Reporting Configuration
            devPacket->repeat = false;

            devPacket->responseType = pXmlCluster->clusterId;
            devPacket->responseType <<= 8;
            devPacket->responseType |= 0x09; // Read Reporting Configuration Response
            devPacket->responseTypeId = pXmlCluster->name + "_REPORTING_CONFIG_" + jparam.first + "_EVENT";

            SetDevicePacket(device, devPacket);

            // the 'response' packet
            paramPacket = std::make_shared<Parameter::Packet>();

            paramPacket->id = pXmlCluster->name + "_REPORTING_CONFIG_" + jparam.first + "_EVENT";
            paramPacket->type = Parameter::Packet::Type::Enum::event;

            parameterMinReportingInterval->eventPackets.push_back(paramPacket);
            parameterMaxReportingInterval->eventPackets.push_back(paramPacket);
            if (parameterReportableChange) parameterReportableChange->eventPackets.push_back(paramPacket);

            devPacket = std::make_shared<ZigbeeDescPacket>(_bl);
            devPacket->channel = channelNo;
            devPacket->direction = Packet::Direction::toCentral;

            devPacket->id = paramPacket->id;
            devPacket->type = pXmlCluster->clusterId;
            devPacket->type <<= 8;
            devPacket->type |= 0x09; // Read Reporting Configuration Response
            devPacket->subtype = 0;
            devPacket->repeat = false;

            SetDevicePacket(device, devPacket);

            // the 'SET' packet
            paramPacket = std::make_shared<Parameter::Packet>();
            paramPacket->id = pXmlCluster->name + "_REPORTING_CONFIG_" + jparam.first + "_SET";
            paramPacket->type = Parameter::Packet::Type::Enum::set;

            parameterMinReportingInterval->setPackets.push_back(paramPacket);
            parameterMaxReportingInterval->setPackets.push_back(paramPacket);
            if (parameterReportableChange) parameterReportableChange->setPackets.push_back(paramPacket);

            devPacket = std::make_shared<ZigbeeDescPacket>(_bl);
            devPacket->channel = channelNo;
            devPacket->direction = Packet::Direction::fromCentral;

            devPacket->id = paramPacket->id;
            devPacket->type = pXmlCluster->clusterId;
            devPacket->type <<= 8;
            devPacket->type |= 0x06; // Configure Reporting

            devPacket->subtype = 0;
            devPacket->repeat = false;

            SetDevicePacket(device, devPacket);

            SetLogicalAndPhysical(parameterMinReportingInterval);
            SetLogicalAndPhysical(parameterMaxReportingInterval);
            if (parameterReportableChange) SetLogicalAndPhysical(parameterReportableChange);

            AddParameter(function, parameterMinReportingInterval, true);
            AddParameter(function, parameterMaxReportingInterval, true);
            if (parameterReportableChange) AddParameter(function, parameterReportableChange, true);
        }
    }


    for(Parameters::iterator j = function->configParameters->parameters.begin(); j != function->configParameters->parameters.end(); ++j)
    {
        for(std::vector<std::shared_ptr<Parameter::Packet>>::iterator k = j->second->getPackets.begin(); k != j->second->getPackets.end(); ++k)
        {
            PacketsById::iterator packetIterator = device->packetsById.find((*k)->id);
            if(packetIterator != device->packetsById.end())
            {
                packetIterator->second->associatedVariables.push_back(j->second);
                if(function->physicalChannelIndexOffset != 0)
                    packetIterator->second->channelIndexOffset = function->physicalChannelIndexOffset;
                device->valueRequestPackets[function->channel][(*k)->id] = packetIterator->second;
            }
        }
        for(std::vector<std::shared_ptr<Parameter::Packet>>::iterator k = j->second->setPackets.begin(); k != j->second->setPackets.end(); ++k)
        {
            PacketsById::iterator packetIterator = device->packetsById.find((*k)->id);
            if(packetIterator != device->packetsById.end())
            {
                packetIterator->second->associatedVariables.push_back(j->second);
                if(function->physicalChannelIndexOffset != 0)
                    packetIterator->second->channelIndexOffset = function->physicalChannelIndexOffset;
            }
        }
        for(std::vector<std::shared_ptr<Parameter::Packet>>::iterator k = j->second->eventPackets.begin(); k != j->second->eventPackets.end(); ++k)
        {
            PacketsById::iterator packetIterator = device->packetsById.find((*k)->id);
            if(packetIterator != device->packetsById.end())
            {
                packetIterator->second->associatedVariables.push_back(j->second);
                if(function->physicalChannelIndexOffset != 0)
                    packetIterator->second->channelIndexOffset = function->physicalChannelIndexOffset;
            }
        }
    }
}


bool ZigbeeDevicesDescription::AddFunction(std::shared_ptr<HomegearDevice> device, int channelNo, const ZigbeeNodeInfo::EndpointInfo::ClusterInfo* pDeviceCluster, const ClustersInfo::ClusterInfoExt* pXmlCluster)
{
    if (!pDeviceCluster || !pXmlCluster)
        return false;

    PFunction function = std::make_shared<ZigbeeFunction>(_bl);
    function->channel = channelNo;
    function->type = std::string("ZIGBEE_CHANNEL_") + pXmlCluster->name/*std::to_string(channelNo)*/;

    bool isConfig = false;

    if (ZigbeeNodeInfo::EndpointInfo::ClusterInfo::IsConfigCluster(pDeviceCluster->clusterId))
    {
        function->configParametersId = std::string("zigbee_config_") + pXmlCluster->name/*std::to_string(channelNo)*/;
        if (!function->configParameters)
            function->configParameters = std::make_shared<ConfigParameters>(_bl);

        isConfig = true;
    }
    else
    {
        function->variablesId = std::string("zigbee_values_") + pXmlCluster->name/*std::to_string(channelNo)*/;
        if (!function->variables)
            function->variables = std::make_shared<Variables>(_bl);
    }


    // parameters ordered and parameters in 'variables' member for function should be filled, too
    // similarly for configParameters

    // also those should appear in 'parameterGroups'?

    device->functions[function->channel] = function;

    // deal with each command from the class

    // each one will add a 'packet'


    // packets:
    // read for an attribute will add a 'get' and a 'report' packet
    // write will add a 'set' packet
    // reportable will add another 'report' (event) packet

    // from 'commands':
    // 'command received' will add a set packet
    // 'command generated' (also called 'sent' in code) will add a 'report' packet

    // for attributes:
    // Report packet :
    // packet type will be (cluster id) << 8 | 0x0A (the one coming as a report for reportable attrs - report attributes)
    // packet type will be (cluster id) << 8 | 0x01 (the one coming as a response for a get packet - read attributes response)
    // direction 'toCentral'

    // get packet:
    // packet type will be (cluster id) << 8 | 0x00 (read attributes command)
    // direction 'fromCentral'

    // set packet:
    // packet type will be (cluster id) << 8 | 0x02 (write attributes command)
    // direction 'fromCentral'

    // for commands
    // packet type will be (cluster id) << 8 | command id

    // use 'subtype' to mark them: 0 for 'global' (the ones for attributes) and 1 for 'commands'
    // direction 'toCentral' for 'cmd generated'
    // direction 'fromCentral' for 'cmd received'

    // the 'frame control' should not be checked entirely for incoming packets
    // for outgoing, it should be 0x10 for 'write attr', 0x11 for 'set' commands (the last bit is for local/global, 0x10 disables 'default response')

    // when constructing and parsing the packet:
    // first two bytes are in little endian order, containing the cluster id (get it from the 'type')
    // next byte is the 'frame control'
    // next one is a dummy one, set to zero (it's a transaction id, it's overwritten when sent)
    // the other bytes depend on the packet:

    // for 'local' the next one is the command code (from 'commands')
    // then the parameters follow (if there are any)

    // for 'global' it's one of the global command coders (0x0A report/event, 0x00 read attr/get, 0x01 response to read attr, 0x02 write attr/set)

    // a problem is that a 'join' for attributes and commands cannot be done as in zwave module, after loading the xml file
    // for zwave, if a class is supported, it's supported entirely
    // not so for a cluster for zigbee: some attributes and commands are optional
    // so 'joining' should be done for each device separately
    // use info from pDeviceCluster (which is incomplete) to know what is supported by the device
    // and the info from pXmlCluster to complete the information (such as commands params)




    // ************************************************************************************
    // Join the attributes and commands
    // ************************************************************************************

    bool containsReportable;

    // join them in here
    std::map<std::string, DeviceParameter> parametersMap;

    // walk over all supported attributes
    for (uint16_t attrid : pDeviceCluster->attrsList)
    {
        auto it = pXmlCluster->attrsMap.find(attrid);
        if (it == pXmlCluster->attrsMap.end())
        {
            GD::out.printDebug("Supported attribte not found in the xml file: 0x" + BaseLib::HelperFunctions::getHexString(attrid));
            continue;
        }

        const std::string name = it->second.name;

        //GD::out.printInfo("Merging Param: " + name);

        parametersMap[name].clusterInfo = pXmlCluster;
        parametersMap[name].attrInfo = &it->second;
        parametersMap[name].type = parametersMap[name].attrInfo->type;


        if (it->second.IsReportable())
            containsReportable = true;
    }



    // TODO: maybe in the future an attribute could be 'joined' with commands params, but for now keep them separately, to ease things up


    for (const auto& jparam : parametersMap)
    {
        std::shared_ptr<ZigbeeParameter> parameter = (isConfig ? std::make_shared<ZigbeeParameter>(_bl, function->configParameters, &jparam.second) : std::make_shared<ZigbeeParameter>(_bl, function->variables, &jparam.second));

        parameter->channelNo = channelNo;
        parameter->isConfig = isConfig;

        parameter->id = pXmlCluster->name + "." + jparam.first;
        parameter->deviceParameter.lastParam = true;

        //GD::out.printInfo("Adding Param: " + parameter->id);

        parameter->readable = jparam.second.IsReadable();
        parameter->writeable = jparam.second.IsWriteable();

        if (!parameter->writeable)
            parameter->readable = true;

        // all possible packets for this param should be added to the parameter

        // then all possible packets should be checked if they do not exist for the device (maybe they were already added by previous params)
        // if not, they should be added for the device
        // the ones that correspond to an attribute should not be already added by other param, since they should be unique - just for that attribute

        if (jparam.second.attrInfo)
        {
            if (jparam.second.attrInfo->IsReadable()) // must have a get and a report
            {
                std::shared_ptr<Parameter::Packet> paramPacket = std::make_shared<Parameter::Packet>();
                paramPacket->id = pXmlCluster->name + "_" + jparam.first + "_GET";
                paramPacket->type = Parameter::Packet::Type::Enum::get;

                parameter->getPackets.push_back(paramPacket);

                BaseLib::DeviceDescription::PPacket devPacket = std::make_shared<ZigbeeDescPacket>(_bl);
                devPacket->channel = channelNo;
                devPacket->direction = Packet::Direction::fromCentral;

                devPacket->id = paramPacket->id;
                devPacket->type = pXmlCluster->clusterId;
                devPacket->type <<= 8;
                devPacket->type |= 0;
                devPacket->subtype = 0; // read attributes command
                devPacket->repeat = false;

                devPacket->responseType = pXmlCluster->clusterId;
                devPacket->responseType <<= 8;
                devPacket->responseType |= 0x01; // read attributes response
                devPacket->responseTypeId = pXmlCluster->name + "_" + jparam.first + "_EVENT";

                SetDevicePacket(device, devPacket);

                paramPacket = std::make_shared<Parameter::Packet>();
                paramPacket->id = pXmlCluster->name + "_" + jparam.first + "_EVENT";
                paramPacket->type = Parameter::Packet::Type::Enum::event;

                parameter->eventPackets.push_back(paramPacket);

                devPacket = std::make_shared<ZigbeeDescPacket>(_bl);
                devPacket->channel = channelNo;
                devPacket->direction = Packet::Direction::toCentral;

                devPacket->id = paramPacket->id;
                devPacket->type = pXmlCluster->clusterId;
                devPacket->type <<= 8;
                devPacket->type |= 0x01; // read attributes response
                devPacket->subtype = 0;
                devPacket->repeat = false;

                SetDevicePacket(device, devPacket);
            }

            // many attributes are marked as non-reportable but reports come nevertheless
            // so it does not hurt to create report packets for them
            //if (jparam.second.attrInfo->IsReportable()) // must have a different kind of 'event'
            {
                std::shared_ptr<Parameter::Packet> paramPacket = std::make_shared<Parameter::Packet>();
                paramPacket->id = pXmlCluster->name + "_" + jparam.first + "_REPORT";
                paramPacket->type = Parameter::Packet::Type::Enum::event;

                parameter->eventPackets.push_back(paramPacket);

                BaseLib::DeviceDescription::PPacket devPacket = std::make_shared<ZigbeeDescPacket>(_bl);
                devPacket->channel = channelNo;
                devPacket->direction = Packet::Direction::toCentral;

                devPacket->id = paramPacket->id;
                devPacket->type = pXmlCluster->clusterId;
                devPacket->type <<= 8;
                devPacket->type |= 0x0A; // report attributes
                devPacket->subtype = 0;
                devPacket->repeat = false;

                SetDevicePacket(device, devPacket);
            }

            if (jparam.second.attrInfo->IsWriteable()) // must have a set
            {
                std::shared_ptr<Parameter::Packet> paramPacket = std::make_shared<Parameter::Packet>();
                paramPacket->id = pXmlCluster->name + "_" + jparam.first + "_SET";
                paramPacket->type = Parameter::Packet::Type::Enum::set;

                parameter->setPackets.push_back(paramPacket);

                BaseLib::DeviceDescription::PPacket devPacket = std::make_shared<ZigbeeDescPacket>(_bl);
                devPacket->channel = channelNo;
                devPacket->direction = Packet::Direction::fromCentral;

                devPacket->id = paramPacket->id;
                devPacket->type = pXmlCluster->clusterId;
                devPacket->type <<= 8;
                devPacket->type |= 0x02; // write attributes command

                devPacket->subtype = 0;
                devPacket->repeat = false;

                SetDevicePacket(device, devPacket);
            }
        }

        SetLogicalAndPhysical(parameter);
        AddParameter(function, parameter, isConfig);

        if (jparam.second.attrInfo && !jparam.second.attrInfo->bitfields.empty())
        {
            for (const auto& bitfield : jparam.second.attrInfo->bitfields)
            {
                // don't add packets to this param because it's 'fake', it will use the true param added above for the work
                std::shared_ptr<ZigbeeParameter> bitfieldParameter = (isConfig ? std::make_shared<ZigbeeParameter>(_bl, function->configParameters, &jparam.second) : std::make_shared<ZigbeeParameter>(_bl, function->variables, &jparam.second));
                bitfieldParameter->deviceParameter.isBitmaskField = true;
                bitfieldParameter->channelNo = channelNo;
                bitfieldParameter->isConfig = isConfig;

                bitfieldParameter->id = pXmlCluster->name + "." + jparam.first + "__" + bitfield.name;

                //GD::out.printInfo("Adding Bitfield Param: " + parameter->id);

                bitfieldParameter->readable = jparam.second.IsReadable();
                bitfieldParameter->writeable = jparam.second.IsWriteable();

                if (!bitfieldParameter->writeable)
                    bitfieldParameter->readable = true;


                if (bitfield.IsSingleBit())
                    SetLogicalAndPhysicalBool(bitfieldParameter);
                else if (1 == jparam.second.attrInfo->GetSize())
                    SetLogicalAndPhysicalInteger(bitfieldParameter, 0, 0xFF);
                else if (2 == jparam.second.attrInfo->GetSize())
                    SetLogicalAndPhysicalInteger(bitfieldParameter, 0, 0xFFFF);
                else
                    SetLogicalAndPhysicalInteger(bitfieldParameter, 0, 0xFFFFFFFF);

                bitfieldParameter->isSigned = false;
                bitfieldParameter->isSignedSet = true;

                AddParameter(function, bitfieldParameter, isConfig);
                bitfieldParameter->physical->operationType = IPhysical::OperationType::Enum::store;
            }
        }
    }

    parametersMap.clear();


    // now go with the commands:



    // TODO: maybe try to join together the 'received' packets with the 'generated' ones that match the names (the id is not good for that) except the generated has a "_RESPONSE" ending for the name (example: VIEW_GROUP and VIEW_GROUP_RESPONSE)

    // TODO: also there are 'GET_'/"_REQUEST" packets that could be joined, also "SET_"

    // TODO: There are also _NOTIFICATION packets that could be joined with _RESPONSE ones

    // walk over all supported 'set' commands (some are actually 'get')

    // consider them 'set' unless they explicitly say "GET_" or "_REQUEST"

    for (uint8_t cmdid : pDeviceCluster->commandsReceived)
    {
        // find the corresponding command in the xml def
        auto it = pXmlCluster->commandsReceived.find(cmdid);
        if (it == pXmlCluster->commandsReceived.end())
        {
            GD::out.printDebug("Supported received command not found in the xml file: 0x" + BaseLib::HelperFunctions::getHexString(cmdid));
            continue;
        }

        // if it has no parameters, it's of 'event' type, cook up an event parameter for it
        if (it->second.params.empty())
        {
            const std::string name = it->second.name + ".ACTION";

            //GD::out.printInfo("Merging Param: " + name);
            parametersMap[name].clusterInfo = pXmlCluster;
            parametersMap[name].commandsReceived[cmdid] = &it->second;
            parametersMap[name].lastParam = true;
        }
        else
        {
            std::string lastParamName;

            // walk over parameters
            for (unsigned int paramcnt = 0; paramcnt < it->second.params.size(); ++paramcnt)
            {
                // check to see if it already exists
                const std::string name = it->second.name + "." + it->second.params[paramcnt].name;

                //GD::out.printInfo("Merging Param: " + name);

                if (parametersMap.find(name) == parametersMap.end())
                {
                    // not found, simply add it
                    parametersMap[name].clusterInfo = pXmlCluster;
                    parametersMap[name].commandsReceived[cmdid] = &it->second;
                    parametersMap[name].type = it->second.params[paramcnt].type;
                    parametersMap[name].paramInfo = &it->second.params[paramcnt];
                }
                else
                {
                    // found, check the type
                    if (parametersMap[name].type != it->second.params[paramcnt].type)
                    {
                        GD::out.printDebug("Command param type different than the type of the parameter joined, please fix the xlm clusters file. Param: " + name);

                        continue;
                    }

                    // add it
                    parametersMap[name].commandsReceived[cmdid] = &it->second;
                }

                lastParamName = name;
            }

            if (!lastParamName.empty())
                parametersMap[lastParamName].lastParam = true;
        }
    }


    // walk over all supported 'event' commands

    // could be notifications or responses to requests
    // maybe join not only "_RESPONSE" with "GET_" (or "_REQUEST"), but also "_NOTIFICATION"

    for (uint8_t cmdid : pDeviceCluster->commandsSent)
    {
        // find the corresponding command in the xml def
        auto it = pXmlCluster->commandsSent.find(cmdid);
        if (it == pXmlCluster->commandsSent.end())
        {
            GD::out.printDebug("Supported generated command not found in the xml file: 0x" + BaseLib::HelperFunctions::getHexString(cmdid));
            continue;
        }

        // if it has no parameters, it's of 'event' type, cook up an event parameter for it
        if (it->second.params.empty())
        {
            const std::string name = it->second.name + ".ACTION";

            //GD::out.printInfo("Merging Param: " + name);

            parametersMap[name].clusterInfo = pXmlCluster;
            parametersMap[name].commandsSent[cmdid] = &it->second;
            parametersMap[name].lastParam = true;
        }
        else
        {
            std::string lastParamName;

            // walk over parameters
            for (unsigned int paramcnt = 0; paramcnt < it->second.params.size(); ++paramcnt)
            {
                // check to see if it already exists
                // a parameter with the same name and type could exist in a 'received command', this being a 'response' for that command
                std::string name;
                bool already_exists = false;
                if (DeviceParameter::IsResponse(it->second.name))
                {
                    name = it->second.name.substr(0, it->second.name.length() - 9) + "." + it->second.params[paramcnt].name;
                    if (parametersMap.find(name) != parametersMap.end() &&  parametersMap[name].type == it->second.params[paramcnt].type)
                    {
                        GD::out.printInfo("Have a request match for a response param: " + name);

                        already_exists = true;
                    }
                }

                // TODO: should I cut out 'GET_' and "_RESPONSE", too?

                if (!already_exists)
                {
                    std::string firstPart = it->second.name;
                    if (firstPart.size() > 8 && firstPart.substr(firstPart.size() - 8) == "_REQUEST")
                        firstPart = firstPart.substr(0, firstPart.size() - 8);

                    name = firstPart + "." + it->second.params[paramcnt].name;
                }

                //GD::out.printInfo("Merging Param: " + name);

                if (parametersMap.find(name) == parametersMap.end())
                {
                    // not found, simply add it
                    parametersMap[name].clusterInfo = pXmlCluster;
                    parametersMap[name].commandsSent[cmdid] = &it->second;
                    parametersMap[name].type = it->second.params[paramcnt].type;
                    parametersMap[name].paramInfo = &it->second.params[paramcnt];
                }
                else
                {
                    // found, check the type
                    if (parametersMap[name].type != it->second.params[paramcnt].type)
                    {
                        GD::out.printDebug("Command param type different than the type of the parameter joined, please fix the xlm clusters file. Param: " + name);

                        continue;
                    }

                    // add it
                    parametersMap[name].commandsSent[cmdid] = &it->second;
                }

                lastParamName = name;
            }

            if (!lastParamName.empty())
                parametersMap[lastParamName].lastParam = true;
        }
    }



    // TODO: maybe do some better joining than this?

    // ************************************************************************************
    // Now add the parameters for each joined parameter
    // ************************************************************************************

    for (const auto& jparam : parametersMap)
    {
        std::shared_ptr<ZigbeeParameter> parameter = (isConfig ? std::make_shared<ZigbeeParameter>(_bl, function->configParameters, &jparam.second) : std::make_shared<ZigbeeParameter>(_bl, function->variables, &jparam.second));

        parameter->channelNo = channelNo;
        parameter->isConfig = isConfig;

        parameter->id = pXmlCluster->name + "." + jparam.first;

        //GD::out.printInfo("Adding Param: " + parameter->id);

        parameter->readable = jparam.second.IsReadable();
        parameter->writeable = jparam.second.IsWriteable();

        if (!parameter->writeable)
            parameter->readable = true;

        // all possible packets for this param should be added to the parameter

        // then all possible packets should be checked if they do not exist for the device (maybe they were already added by previous params)
        // if not, they should be added for the device
        // the ones that correspond to an attribute should not be already added by other param, since they should be unique - just for that attribute


        for (const std::pair<uint8_t, const ClustersInfo::ClusterInfoExt::Command*>& cmdpair : jparam.second.commandsReceived)
        {
            std::shared_ptr<Parameter::Packet> paramPacket = std::make_shared<Parameter::Packet>();
            paramPacket->id = pXmlCluster->name + "_" + cmdpair.second->name;

            bool isGet = DeviceParameter::IsGet(cmdpair.second->name);
            if (!isGet)
            {
                // if not explicitly a 'get', check to see if it has a 'response':
                const std::string respName = cmdpair.second->name + "_RESPONSE";
                for (const auto& cmdSent : jparam.second.commandsSent)
                {
                    if (cmdSent.second->name == respName)
                    {
                        isGet = true;
                        break;
                    }
                }
            }

            // two cases: no parameter, it's of 'event' type, with parameters is a 'set' (if not explicitly a 'get')
            if (isGet)
            {
                paramPacket->type = Parameter::Packet::Type::Enum::get;
                parameter->getPackets.push_back(paramPacket);
            }
            else if (cmdpair.second->params.empty())
            {
                paramPacket->type = Parameter::Packet::Type::Enum::event;
                parameter->eventPackets.push_back(paramPacket);
            }
            else
            {
                paramPacket->type = Parameter::Packet::Type::Enum::set;
                parameter->setPackets.push_back(paramPacket);
            }

            if (device->packetsById.find(paramPacket->id) == device->packetsById.end())
            {
                BaseLib::DeviceDescription::PPacket devPacket = std::make_shared<ZigbeeDescPacket>(_bl);
                devPacket->channel = channelNo;
                devPacket->direction = Packet::Direction::fromCentral;

                devPacket->id = paramPacket->id;
                devPacket->type = pXmlCluster->clusterId;
                devPacket->type <<= 8;
                devPacket->type |= cmdpair.second->key;
                devPacket->subtype = 1;
                devPacket->repeat = false;

                SetDevicePacket(device, devPacket);
            }
        }

        for (const std::pair<uint8_t, const ClustersInfo::ClusterInfoExt::Command*>& cmdpair : jparam.second.commandsSent)
        {
            std::shared_ptr<Parameter::Packet> paramPacket = std::make_shared<Parameter::Packet>();
            paramPacket->id = pXmlCluster->name + "_" + cmdpair.second->name;

            paramPacket->type = Parameter::Packet::Type::Enum::event;
            parameter->eventPackets.push_back(paramPacket);

            if (device->packetsById.find(paramPacket->id) == device->packetsById.end())
            {
                BaseLib::DeviceDescription::PPacket devPacket = std::make_shared<ZigbeeDescPacket>(_bl);
                devPacket->channel = channelNo;
                devPacket->direction = Packet::Direction::toCentral;

                devPacket->id = paramPacket->id;
                devPacket->type = pXmlCluster->clusterId;
                devPacket->type <<= 8;
                devPacket->type |= cmdpair.second->key;
                devPacket->subtype = 1;
                devPacket->repeat = false;

                SetDevicePacket(device, devPacket);
            }
        }

        SetLogicalAndPhysical(parameter);

        AddParameter(function, parameter, isConfig);

        if (jparam.second.paramInfo && !jparam.second.paramInfo->bitfields.empty())
        {
            for (const auto& bitfield : jparam.second.paramInfo->bitfields)
            {
                // don't add packets to this param because it's 'fake', it will use the true param added above for the work
                std::shared_ptr<ZigbeeParameter> bitfieldParameter = (isConfig ? std::make_shared<ZigbeeParameter>(_bl, function->configParameters, &jparam.second) : std::make_shared<ZigbeeParameter>(_bl, function->variables, &jparam.second));
                bitfieldParameter->deviceParameter.isBitmaskField = true;
                bitfieldParameter->channelNo = channelNo;
                bitfieldParameter->isConfig = isConfig;

                bitfieldParameter->id = pXmlCluster->name + "." + jparam.first + "__" + bitfield.name;

                //GD::out.printInfo("Adding Bitfield Param: " + bitfieldParameter->id);

                bitfieldParameter->readable = jparam.second.IsReadable();
                bitfieldParameter->writeable = jparam.second.IsWriteable();

                if (!bitfieldParameter->writeable)
                    bitfieldParameter->readable = true;


                if (bitfield.IsSingleBit())
                    SetLogicalAndPhysicalBool(bitfieldParameter);
                else if (1 == jparam.second.paramInfo->GetSize())
                    SetLogicalAndPhysicalInteger(bitfieldParameter, 0, 0xFF);
                else if (2 == jparam.second.paramInfo->GetSize())
                    SetLogicalAndPhysicalInteger(bitfieldParameter, 0, 0xFFFF);
                else
                    SetLogicalAndPhysicalInteger(bitfieldParameter, 0, 0xFFFFFFFF);

                bitfieldParameter->isSigned = false;
                bitfieldParameter->isSignedSet = true;

                AddParameter(function, bitfieldParameter, isConfig);
                bitfieldParameter->physical->operationType = IPhysical::OperationType::Enum::store;
            }
        }
    }


    // maybe also associatedVariables ???

    for(Parameters::iterator j = function->variables->parameters.begin(); j != function->variables->parameters.end(); ++j)
    {
        for(std::vector<std::shared_ptr<Parameter::Packet>>::iterator k = j->second->getPackets.begin(); k != j->second->getPackets.end(); ++k)
        {
            PacketsById::iterator packetIterator = device->packetsById.find((*k)->id);
            if(packetIterator != device->packetsById.end())
            {
                packetIterator->second->associatedVariables.push_back(j->second);
                if(function->physicalChannelIndexOffset != 0)
                    packetIterator->second->channelIndexOffset = function->physicalChannelIndexOffset;
                device->valueRequestPackets[function->channel][(*k)->id] = packetIterator->second;
            }
        }
        for(std::vector<std::shared_ptr<Parameter::Packet>>::iterator k = j->second->setPackets.begin(); k != j->second->setPackets.end(); ++k)
        {
            PacketsById::iterator packetIterator = device->packetsById.find((*k)->id);
            if(packetIterator != device->packetsById.end())
            {
                packetIterator->second->associatedVariables.push_back(j->second);
                if(function->physicalChannelIndexOffset != 0)
                    packetIterator->second->channelIndexOffset = function->physicalChannelIndexOffset;
            }
        }
        for(std::vector<std::shared_ptr<Parameter::Packet>>::iterator k = j->second->eventPackets.begin(); k != j->second->eventPackets.end(); ++k)
        {
            PacketsById::iterator packetIterator = device->packetsById.find((*k)->id);
            if(packetIterator != device->packetsById.end())
            {
                packetIterator->second->associatedVariables.push_back(j->second);
                if(function->physicalChannelIndexOffset != 0)
                    packetIterator->second->channelIndexOffset = function->physicalChannelIndexOffset;
            }
        }
    }


    for(Parameters::iterator j = function->configParameters->parameters.begin(); j != function->configParameters->parameters.end(); ++j)
    {
        for(std::vector<std::shared_ptr<Parameter::Packet>>::iterator k = j->second->getPackets.begin(); k != j->second->getPackets.end(); ++k)
        {
            PacketsById::iterator packetIterator = device->packetsById.find((*k)->id);
            if(packetIterator != device->packetsById.end())
            {
                packetIterator->second->associatedVariables.push_back(j->second);
                if(function->physicalChannelIndexOffset != 0)
                    packetIterator->second->channelIndexOffset = function->physicalChannelIndexOffset;
                device->valueRequestPackets[function->channel][(*k)->id] = packetIterator->second;
            }
        }
        for(std::vector<std::shared_ptr<Parameter::Packet>>::iterator k = j->second->setPackets.begin(); k != j->second->setPackets.end(); ++k)
        {
            PacketsById::iterator packetIterator = device->packetsById.find((*k)->id);
            if(packetIterator != device->packetsById.end())
            {
                packetIterator->second->associatedVariables.push_back(j->second);
                if(function->physicalChannelIndexOffset != 0)
                    packetIterator->second->channelIndexOffset = function->physicalChannelIndexOffset;
            }
        }
        for(std::vector<std::shared_ptr<Parameter::Packet>>::iterator k = j->second->eventPackets.begin(); k != j->second->eventPackets.end(); ++k)
        {
            PacketsById::iterator packetIterator = device->packetsById.find((*k)->id);
            if(packetIterator != device->packetsById.end())
            {
                packetIterator->second->associatedVariables.push_back(j->second);
                if(function->physicalChannelIndexOffset != 0)
                    packetIterator->second->channelIndexOffset = function->physicalChannelIndexOffset;
            }
        }
    }

    return containsReportable;
}


void ZigbeeDevicesDescription::SetLogicalAndPhysical(MyPParameter parameter)
{
    if (!parameter)
        return;

    std::shared_ptr<ZigbeeParameter> param = std::dynamic_pointer_cast<ZigbeeParameter>(parameter);
    if (!param)
        return;

    // TODO: set 'default' better

    if (parameter->id.length() >= 7 && parameter->id.substr(parameter->id.length() - 7) == ".ACTION")
    {
        SetLogicalAndPhysicalAction(parameter);
        parameter->physical->operationType = BaseLib::DeviceDescription::IPhysical::OperationType::command;

        return;
    }

    switch(param->deviceParameter.type)
    {
    case 0: // No data
    case 0xff: // unknown
        break;
    //return 0;

    case 0x30: // enum8
        // there are enums that are actually maps
        if ((param->deviceParameter.attrInfo && param->deviceParameter.attrInfo->enums.empty() && !param->deviceParameter.attrInfo->bitfields.empty()) ||
            (param->deviceParameter.paramInfo && param->deviceParameter.paramInfo->enums.empty() && !param->deviceParameter.paramInfo->bitfields.empty()))
            SetLogicalAndPhysicalInteger(parameter, 0, 0xFF);
        else
            SetLogicalAndPhysicalEnum(parameter, 0xFF);
        parameter->isSigned = false;
        parameter->isSignedSet = true;
        break;

    case 0x31: // enum16
        // there are enums that are actually maps
        if ((param->deviceParameter.attrInfo && param->deviceParameter.attrInfo->enums.empty() && !param->deviceParameter.attrInfo->bitfields.empty()) ||
            (param->deviceParameter.paramInfo && param->deviceParameter.paramInfo->enums.empty() && !param->deviceParameter.paramInfo->bitfields.empty()))
            SetLogicalAndPhysicalInteger(parameter, 0, 0xFFFF);
        else
            SetLogicalAndPhysicalEnum(parameter, 0xFFFF);

        parameter->isSigned = false;
        parameter->isSignedSet = true;
        break;

    case 0x08: // 8-bit data
    case 0x18: // 8-bit bitmap
    case 0x20: // uint 8bit
        if ((param->deviceParameter.attrInfo && param->deviceParameter.attrInfo->enums.empty()) ||
            (param->deviceParameter.paramInfo && param->deviceParameter.paramInfo->enums.empty()))
            SetLogicalAndPhysicalInteger(parameter, 0, 0xFF);
        else
            SetLogicalAndPhysicalEnum(parameter, 0xFF);
        parameter->isSigned = false;
        parameter->isSignedSet = true;
        break;

    case 0x28: // int8
        SetLogicalAndPhysicalInteger(parameter, std::numeric_limits<int8_t>::min(), std::numeric_limits<int8_t>::max());
        parameter->isSigned = true;
        parameter->isSignedSet = true;
        break;

    case 0x10: // bool
        SetLogicalAndPhysicalBool(parameter);
        parameter->isSigned = false;
        parameter->isSignedSet = true;
        break;
    //return 1;

    case 0x09: // 16-bit data
    case 0x21: // uint 16bit
        if ((param->deviceParameter.attrInfo && param->deviceParameter.attrInfo->enums.empty()) ||
            (param->deviceParameter.paramInfo && param->deviceParameter.paramInfo->enums.empty()))
            SetLogicalAndPhysicalInteger(parameter, 0, 0xFFFF);
        else
            SetLogicalAndPhysicalEnum(parameter, 0xFFFF);
        parameter->isSigned = false;
        parameter->isSignedSet = true;
        break;
    case 0x19: // 16-bit bitmap
    case 0xe8: // cluster id
    case 0xe9: // attr id
        SetLogicalAndPhysicalInteger(parameter, 0, 0xFFFF);
        parameter->isSigned = false;
        parameter->isSignedSet = true;
        break;

    case 0x29: // int16
        SetLogicalAndPhysicalInteger(parameter, std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max());
        parameter->isSigned = true;
        parameter->isSignedSet = true;
        break;
    //return 2;

    case 0x0a: // 24-bit data
    case 0x22: // uint24
    case 0x1a: // 24-bit bitmap
        SetLogicalAndPhysicalInteger(parameter, 0, 0xFFFFFF);
        parameter->isSigned = false;
        parameter->isSignedSet = true;
        break;
    case 0x2a: // int24
        SetLogicalAndPhysicalInteger(parameter, 0x800000, 0x7FFFFF);
        parameter->isSigned = true;
        parameter->isSignedSet = true;
        break;
    //return 3;

    case 0x0b: // 32-bit data
    case 0x23: // uint32
    case 0x1b: // 32-bit bitmap
    case 0xe0: // time of day
    case 0xe1: // date
    case 0xe2: // utc time
    case 0xea: // bac oid
        SetLogicalAndPhysicalInteger(parameter,std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::max());
        parameter->isSigned = false;
        parameter->isSignedSet = true;
        break;

    case 0x2b: // int32
        SetLogicalAndPhysicalInteger(parameter,std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max());
        parameter->isSigned = true;
        parameter->isSignedSet = true;
        break;
    //return 4;
    case 0x0c: // 40-bit data
    case 0x1c: // 40-bit bitmap
    case 0x24: // uint40
        SetLogicalAndPhysicalInteger64(parameter, 0, 0xffffffffff);
        parameter->isSigned = false;
        parameter->isSignedSet = true;
        break;

    case 0x2c: // int40
        SetLogicalAndPhysicalInteger64(parameter, 0x8000000000, 0x7fffffffff);
        parameter->isSigned = true;
        parameter->isSignedSet = true;
        break;

    //return 5;
    case 0x0d: // 48-bit data
    case 0x1d: // 48-bit bitmap
    case 0x25: // uint48
        SetLogicalAndPhysicalInteger64(parameter, 0, 0xffffffffffff);
        parameter->isSigned = false;
        parameter->isSignedSet = true;
        break;

    case 0x2d: // int48
        SetLogicalAndPhysicalInteger64(parameter, 0x800000000000, 0x7fffffffffff);
        parameter->isSigned = true;
        parameter->isSignedSet = true;
        break;
    //return 6;

    case 0x0e: // 56-bit data
    case 0x1e: // 56-bit bitmap
    case 0x26: // uint56
        SetLogicalAndPhysicalInteger64(parameter, 0, 0xffffffffffffff);
        parameter->isSigned = false;
        parameter->isSignedSet = true;
        break;

    case 0x2e: // int56
        SetLogicalAndPhysicalInteger64(parameter, 0x80000000000000, 0x7fffffffffffff);
        parameter->isSigned = true;
        parameter->isSignedSet = true;
        break;

    //return 7;

    case 0x2f: // int64
        SetLogicalAndPhysicalInteger64(parameter,std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max());
        parameter->isSigned = true;
        parameter->isSignedSet = true;
        break;

    case 0x0f: // 64-bit data
    case 0x1f: // 64-bit bitmap
    case 0x27: // uint64
        SetLogicalAndPhysicalInteger64(parameter, 0, std::numeric_limits<uint64_t>::max());
        parameter->isSigned = false;
        parameter->isSignedSet = true;
        break;

    case 0x38: // semi - float - don't forget to handle it!
        SetLogicalAndPhysicalSemi(parameter);
        break;

    case 0x39: // single float
        SetLogicalAndPhysicalFloat(parameter);
        break;

    case 0x3a: // double
        SetLogicalAndPhysicalDouble(parameter);
        break;
    case 0xf0: // IEEE address
        SetLogicalAndPhysicalInteger64(parameter, 0, std::numeric_limits<uint64_t>::max());
        break;
    //return 8;
    case 0xf1: // key128
        SetLogicalAndPhysicalArray(parameter);
        break;
    case 0x41: // octstr
    case 0x42: // string
        SetLogicalAndPhysicalString(parameter);
        break;

    // should I set the following as strings as well? They appear to be 'wide' strings
    case 0x43: // octstr 16
    case 0x44: // string 16
    case 0x48: // array
        SetLogicalAndPhysicalArray(parameter);
        break;
    case 0x4C: // struct
        SetLogicalAndPhysicalStruct(parameter);
        break;
    case 0x50: // set
    case 0x51: // bag
        SetLogicalAndPhysicalArray(parameter); // for now
        break;
    default:
        break;
    }

    if (parameter->physical)
    {
        if (parameter->writeable && param->isLastParam())
            parameter->physical->operationType = IPhysical::OperationType::Enum::command;
        else
            parameter->physical->operationType = IPhysical::OperationType::Enum::store;
    }
}


void ZigbeeDevicesDescription::SetLogicalAndPhysicalEnum(MyPParameter parameter, uint16_t maxValue)
{
    BaseLib::DeviceDescription::PLogicalEnumeration logical = std::make_shared<BaseLib::DeviceDescription::LogicalEnumeration>(_bl);
    BaseLib::DeviceDescription::PPhysicalInteger physical = std::make_shared<BaseLib::DeviceDescription::PhysicalInteger>(_bl);

    physical->endianess = BaseLib::DeviceDescription::IPhysical::Endianess::little;

    physical->size = (0xff == maxValue ? 1 : 2);
    physical->sizeDefined = true;

    logical->minimumValue = 0;
    logical->maximumValue = maxValue; //enums.size() - 1;

    //int maxVal = logical->maximumValue;

    if (parameter->deviceParameter.attrInfo)
        for (const auto& enumval : parameter->deviceParameter.attrInfo->enums)
        {
            logical->values.push_back(BaseLib::DeviceDescription::EnumerationValue(enumval.name, enumval.val));

            //if (enumval.val > maxVal)
            //    maxVal = enumval.val;
        }
    else if (parameter->deviceParameter.paramInfo)
        for (const auto& enumval : parameter->deviceParameter.paramInfo->enums)
        {
            logical->values.push_back(BaseLib::DeviceDescription::EnumerationValue(enumval.name, enumval.val));

            //if (enumval.val > maxVal)
            //    maxVal = enumval.val;
        }


    // Don't set it, it can be bigger than the values enumerated in the xml file (can be for example custom for manufacturer)
    //if (maxVal > logical->maximumValue)
    //    logical->maximumValue = maxVal;

    parameter->logical = logical;
    parameter->physical = physical;
}


void ZigbeeDevicesDescription::SetLogicalAndPhysicalInteger(MyPParameter parameter, uint32_t minval, uint32_t maxval)
{
    BaseLib::DeviceDescription::PLogicalInteger logical = std::make_shared<BaseLib::DeviceDescription::LogicalInteger>(_bl);
    BaseLib::DeviceDescription::PPhysicalInteger physical = std::make_shared<BaseLib::DeviceDescription::PhysicalInteger>(_bl);

    physical->endianess = BaseLib::DeviceDescription::IPhysical::Endianess::little;

    logical->minimumValue = (0xffffffff == maxval ? -2147483648 : minval);
    logical->maximumValue = (0xffffffff == maxval ? 2147483647 : maxval);

    physical->sizeDefined = true;

    if (0xff == maxval || 0x7f == maxval)
        physical->size = 1;
    else if (0xffff == maxval || 0x7fff == maxval)
        physical->size = 2;
    else if (0xffffff == maxval || 0x7fffff == maxval)
        physical->size = 3;
    else
        physical->size = 4;

    logical->defaultValue = 0;

    parameter->logical = logical;
    parameter->physical = physical;
}

void ZigbeeDevicesDescription::SetLogicalAndPhysicalInteger64(MyPParameter parameter, uint64_t minval, uint64_t maxval)
{
    BaseLib::DeviceDescription::PLogicalInteger64 logical = std::make_shared<BaseLib::DeviceDescription::LogicalInteger64>(_bl);
    BaseLib::DeviceDescription::PPhysicalInteger physical = std::make_shared<BaseLib::DeviceDescription::PhysicalInteger>(_bl);
    physical->endianess = BaseLib::DeviceDescription::IPhysical::Endianess::little;

    if (0xffffffffff == maxval || 0x7fffffffff == maxval)
        physical->size = 5;
    else if (0xffffffffffff == maxval || 0x7fffffffffff == maxval)
        physical->size = 6;
    else if (0xffffffffffffff == maxval || 0x7fffffffffffff == maxval)
        physical->size = 7;
    else
        physical->size = 8;

    logical->minimumValue = (0xffffffffffffffffl == maxval ? (-9223372036854775807ll - 1) : minval);
    logical->maximumValue = (0xffffffffffffffffl == maxval ? 9223372036854775807ll : maxval);

    physical->sizeDefined = true;

    logical->defaultValue = 0;

    parameter->logical = logical;
    parameter->physical = physical;
}


void ZigbeeDevicesDescription::SetLogicalAndPhysicalString(MyPParameter parameter)
{
    parameter->logical = std::make_shared<BaseLib::DeviceDescription::LogicalString>(_bl);
    parameter->physical = std::make_shared<BaseLib::DeviceDescription::PhysicalString>(_bl);
    parameter->physical->endianess = BaseLib::DeviceDescription::IPhysical::Endianess::little;
}

void ZigbeeDevicesDescription::SetLogicalAndPhysicalArray(MyPParameter parameter)
{
    parameter->logical = std::make_shared<BaseLib::DeviceDescription::LogicalArray>(_bl);
    parameter->physical = std::make_shared<BaseLib::DeviceDescription::Physical>(_bl);
    parameter->physical->endianess = BaseLib::DeviceDescription::IPhysical::Endianess::little;
}


void ZigbeeDevicesDescription::SetLogicalAndPhysicalStruct(MyPParameter parameter)
{
    parameter->logical = std::make_shared<BaseLib::DeviceDescription::LogicalStruct>(_bl);
    parameter->physical = std::make_shared<BaseLib::DeviceDescription::Physical>(_bl);
    parameter->physical->endianess = BaseLib::DeviceDescription::IPhysical::Endianess::little;
}


void ZigbeeDevicesDescription::SetLogicalAndPhysicalSemi(MyPParameter parameter)
{
    parameter->logical = std::make_shared<BaseLib::DeviceDescription::LogicalDecimal>(_bl);
    parameter->physical = std::make_shared<BaseLib::DeviceDescription::Physical>(_bl);
    parameter->physical->size = 4;
    parameter->physical->sizeDefined = true;
    parameter->physical->endianess = BaseLib::DeviceDescription::IPhysical::Endianess::little;
}

void ZigbeeDevicesDescription::SetLogicalAndPhysicalFloat(MyPParameter parameter)
{
    parameter->logical = std::make_shared<BaseLib::DeviceDescription::LogicalDecimal>(_bl);
    parameter->physical = std::make_shared<BaseLib::DeviceDescription::Physical>(_bl);
    parameter->physical->size = 8;
    parameter->physical->sizeDefined = true;
    parameter->physical->endianess = BaseLib::DeviceDescription::IPhysical::Endianess::little;
}

void ZigbeeDevicesDescription::SetLogicalAndPhysicalDouble(MyPParameter parameter)
{
    parameter->logical = std::make_shared<BaseLib::DeviceDescription::LogicalDecimal>(_bl);
    parameter->physical = std::make_shared<BaseLib::DeviceDescription::Physical>(_bl);
    parameter->physical->size = 16;
    parameter->physical->sizeDefined = true;
    parameter->physical->endianess = BaseLib::DeviceDescription::IPhysical::Endianess::little;
}


void ZigbeeDevicesDescription::SetLogicalAndPhysicalBool(MyPParameter parameter)
{
    parameter->logical = std::make_shared<BaseLib::DeviceDescription::LogicalBoolean>(_bl);
    parameter->physical = std::make_shared<BaseLib::DeviceDescription::Physical>(_bl);
    parameter->physical->sizeDefined = true;
    parameter->physical->size = 1;
    parameter->physical->endianess = BaseLib::DeviceDescription::IPhysical::Endianess::little;
}

void ZigbeeDevicesDescription::SetLogicalAndPhysicalAction(PParameter parameter)
{
    parameter->logical = std::make_shared<BaseLib::DeviceDescription::LogicalAction>(_bl);
    parameter->physical = std::make_shared<BaseLib::DeviceDescription::PhysicalInteger>(_bl);
}

void ZigbeeDevicesDescription::AddParameter(PFunction function, MyPParameter parameter, bool config)
{
    if (config)
    {
        if (function->configParameters->parameters.find(parameter->id) == function->configParameters->parameters.end())
            function->configParameters->parametersOrdered.push_back(parameter); // a vector
        else
        {
            for (unsigned int i = 0; i < function->configParameters->parametersOrdered.size(); ++i)
                if (function->configParameters->parametersOrdered[i]->id == parameter->id)
                {
                    function->configParameters->parametersOrdered[i] = parameter;
                    break;
                }
        }

        function->configParameters->parameters[parameter->id] = parameter; // a map
    }
    else
    {
        if (function->variables->parameters.find(parameter->id) == function->variables->parameters.end())
            function->variables->parametersOrdered.push_back(parameter); // a vector
        else
        {
            for (unsigned int i = 0; i < function->variables->parametersOrdered.size(); ++i)
                if (function->variables->parametersOrdered[i]->id == parameter->id)
                {
                    function->variables->parametersOrdered[i] = parameter;
                    break;
                }
        }

        function->variables->parameters[parameter->id] = parameter; // a map
    }
}

void ZigbeeDevicesDescription::AddConfigReadonlyParameter(PFunction function, const std::string& name, bool stringVal)
{
    std::shared_ptr<ZigbeeParameter> parameter = std::make_shared<ZigbeeParameter>(_bl, function->configParameters);

    parameter->channelNo = 0;
    parameter->isConfig = true;

    parameter->id = name;

    parameter->metadata = name;
    parameter->description = name;

    parameter->readable = true;
    parameter->writeable = false;

    if (stringVal)
        SetLogicalAndPhysicalString(parameter);
    else
        SetLogicalAndPhysicalInteger(parameter, 0, 0xffffffff);

    AddParameter(function, parameter, true);
}

void ZigbeeDevicesDescription::AddConfigUint16ReadonlyParameter(PFunction function, const std::string& name)
{
    std::shared_ptr<ZigbeeParameter> parameter = std::make_shared<ZigbeeParameter>(_bl, function->configParameters);

    parameter->channelNo = 0;
    parameter->isConfig = true;

    parameter->id = name;

    parameter->metadata = name;
    parameter->description = name;

    parameter->readable = true;
    parameter->writeable = false;

    parameter->isSigned = false;
    parameter->isSignedSet = true;

    SetLogicalAndPhysicalInteger(parameter, 0, 0xffff);

    AddParameter(function, parameter, true);
}



void ZigbeeDevicesDescription::AddConfigReadonlyBoolParameter(PFunction function, const std::string& name)
{
    std::shared_ptr<ZigbeeParameter> parameter = std::make_shared<ZigbeeParameter>(_bl, function->configParameters);

    parameter->channelNo = 0;
    parameter->isConfig = true;

    parameter->id = name;

    parameter->metadata = name;
    parameter->description = name;

    parameter->readable = true;
    parameter->writeable = false;

    SetLogicalAndPhysicalBool(parameter);

    AddParameter(function, parameter, true);
}



void ZigbeeDevicesDescription::AddReadonlyBoolParameter(PFunction function, const std::string& name)
{
    std::shared_ptr<ZigbeeParameter> parameter = std::make_shared<ZigbeeParameter>(_bl, function->configParameters);

    parameter->channelNo = 0;
    parameter->isConfig = false;

    parameter->id = name;

    parameter->metadata = name;
    parameter->description = name;

    parameter->readable = true;
    parameter->writeable = false;

    SetLogicalAndPhysicalBool(parameter);

    AddParameter(function, parameter, false);
}

void ZigbeeDevicesDescription::AddBoolParameter(PFunction function, const std::string& name)
{
    std::shared_ptr<ZigbeeParameter> parameter = std::make_shared<ZigbeeParameter>(_bl, function->configParameters);

    parameter->channelNo = 0;
    parameter->isConfig = false;

    parameter->id = name;

    parameter->metadata = name;
    parameter->description = name;

    parameter->readable = true;
    parameter->writeable = true;

    SetLogicalAndPhysicalBool(parameter);

    AddParameter(function, parameter, false);
}


void ZigbeeDevicesDescription::SetDevicePacket(BaseLib::DeviceDescription::PHomegearDevice device, BaseLib::DeviceDescription::PPacket packet)
{
    device->packetsByMessageType.insert(std::pair<uint32_t, BaseLib::DeviceDescription::PPacket>(packet->type, packet));
    device->packetsById[packet->id] = packet;

    if(!packet->function1.empty())
        device->packetsByFunction1.insert(std::pair<std::string, BaseLib::DeviceDescription::PPacket>(packet->function1, packet));
    if(!packet->function2.empty())
        device->packetsByFunction2.insert(std::pair<std::string, BaseLib::DeviceDescription::PPacket>(packet->function2, packet));
}


std::string ZigbeeDevicesDescription::GetBaseHomegearName(const std::string& valueKey)
{
    std::string baseKey;

    std::string::size_type pos = valueKey.find('.');
    if (std::string::npos != pos)
        baseKey = valueKey.substr(0, pos);

    return baseKey;
}



bool ZigbeeDevicesDescription::CheckDevice(BaseLib::DeviceDescription::PHomegearDevice device)
{
    try
    {
        if (!device)
        {
            _bl->out.printDebug("No device was passed to CheckDevice");

            return false;
        }

        _bl->out.printDebug("Checking device data structures...");

        bool valid = true;

        for (const auto& packet : device->packetsById)
        {
            const int channelNo = packet.second->channel;
            auto function = device->functions[channelNo];

            for (const auto& payload : packet.second->binaryPayloads)
            {
                if (!payload->parameterId.empty())
                {
                    bool found = false;
                    for (const auto& param : packet.second->associatedVariables)
                    {
                        if (param->id == payload->parameterId)
                        {
                            found = true;
                            break;
                        }
                    }

                    if (!found)
                    {
                        _bl->out.printDebug("Could not find associated variable " + payload->parameterId + " for packet: " + packet.second->id + " in payloads");
                        valid = false;
                    }
                }
            }

            if (function->variables)
            {
                for (const auto& variable : packet.second->associatedVariables)
                {
                    bool found = false;
                    if (function->variables)
                        found = function->variables->parameters.find(variable->id) != function->variables->parameters.end();

                    if (!found && function->configParameters)
                        found = function->configParameters->parameters.find(variable->id) != function->configParameters->parameters.end();

                    if (!found)
                    {
                        _bl->out.printDebug("Could not find associated variable " + variable->id + " for packet: " + packet.second->id + " in channel variables");
                        valid = false;
                    }
                }
            }
        }

        for (const auto& function : device->functions)
        {
            if (function.second->variables)
                for (const auto& variable : function.second->variables->parameters)
                {
                    const auto& param = variable.second;

                    // now check the get, set and event packets
                    for (const auto& packet : param->getPackets)
                    {
                        if (device->packetsById.find(packet->id) == device->packetsById.end())
                        {
                            valid = false;
                            _bl->out.printDebug("Could not find get packet " + packet->id + " for variable: " + param->id + " in device packets");
                        }

                    }
                    for (const auto& packet : param->setPackets)
                    {
                        if (device->packetsById.find(packet->id) == device->packetsById.end())
                        {
                            valid = false;
                            _bl->out.printDebug("Could not find set packet " + packet->id + " for variable: " + param->id + " in device packets");
                        }
                    }
                    for (const auto& packet : param->eventPackets)
                    {
                        if (device->packetsById.find(packet->id) == device->packetsById.end())
                        {
                            valid = false;
                            _bl->out.printDebug("Could not find event packet " + packet->id + " for variable: " + param->id + " in device packets");
                        }
                    }
                }

            if (function.second->configParameters)
                for (const auto& variable : function.second->configParameters->parameters)
                {
                    const auto& param = variable.second;

                    // now check the get, set and event packets
                    for (const auto& packet : param->getPackets)
                    {
                        if (device->packetsById.find(packet->id) == device->packetsById.end())
                        {
                            valid = false;
                            _bl->out.printDebug("Could not find get packet " + packet->id + " for config variable: " + param->id + " in device packets");
                        }
                    }
                    for (const auto& packet : param->setPackets)
                    {
                        if (device->packetsById.find(packet->id) == device->packetsById.end())
                        {
                            valid = false;
                            _bl->out.printDebug("Could not find set packet " + packet->id + " for config variable: " + param->id + " in device packets");
                        }
                    }
                    for (const auto& packet : param->eventPackets)
                    {
                        if (device->packetsById.find(packet->id) == device->packetsById.end())
                        {
                            valid = false;
                            _bl->out.printDebug("Could not find event packet " + packet->id + " for config variable: " + param->id + " in device packets");
                        }
                    }
                }
        }

        return valid;
    }
    catch(const std::exception& ex)
    {
        _bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return false;
}


// this is for adding 'STATUS' enum values and get their string and description
// the values won't be in the clusters xml file, there are too many STATUS attributes/params there to fill it up (maybe just add code and complete the description from here?)

const std::map<uint8_t, std::string> ZigbeeDevicesDescription::statusCodesMap =
{
    {0x00,  "SUCCESS"},
    {0x01,  "FAILURE"},
    {0x70,  "REQUEST_DENIED"},
    {0x71,  "MULTIPLE_REQUEST_NOT_ALLOWED"},
    {0x72,  "INDICATION_REDIRECTION_TO_AP"},
    {0x73,  "PREFERENCE_DENIED"},
    {0x74,  "PREFERENCE_IGNORED"},
    {0x7e,  "NOT_AUTHORIZED"},
    {0x7f,  "RESERVED_FIELD_NOT_ZERO"},
    {0x80,  "MALFORMED_COMMAND"},
    {0x81,  "UNSUP_CLUSTER_COMMAND"},
    {0x82,  "UNSUP_GENERAL_COMMAND"},
    {0x83,  "UNSUP_MANUF_CLUSTER_COMMAND"},
    {0x84,  "UNSUP_MANUF_GENERAL_COMMAND"},
    {0x85,  "INVALID_FIELD"},
    {0x86,  "UNSUPPORTED_ATTRIBUTE"},
    {0x87,  "INVALID_VALUE"},
    {0x88,  "READ_ONLY"},
    {0x89,  "INSUFFICIENT_SPACE"},
    {0x8A,  "DUPLICATE_EXISTS"},
    {0x8B,  "NOT_FOUND"},
    {0x8C,  "UNREPORTABLE_ATTRIBUTE"},
    {0x8D,  "INVALID_DATA_TYPE"},
    {0x8E,  "INVALID_SELECTOR"},
    {0x8F,  "WRITE_ONLY"},
    {0x90,  "INCONSISTENT_STARTUP_STATE"},
    {0x91,  "DEFINED_OUT_OF_BAND"},
    {0x92,  "INCONSISTENT"},
    {0x93,  "ACTION_DENIED"},
    {0x94,  "TIMEOUT"},
    {0x95,  "ABORT"},
    {0x96,  "INVALID_IMAGE"},
    {0x97,  "WAIT_FOR_DATA"},
    {0x98,  "NO_IMAGE_AVAILABLE"},
    {0x99,  "REQUIRE_MORE_IMAGE"},
    {0x9A,  "NOTIFICATION_PENDING"},
    {0xC0,  "HARDWARE_FAILURE"},
    {0xC1,  "SOFTWARE_FAILURE"},
    {0xC2,  "CALIBRATION_ERROR"},
    {0xC3,  "UNSUPPORTED_CLUSTER"},
};

std::string ZigbeeDevicesDescription::GetStatusString(uint8_t code)
{
    std::map<uint8_t, std::string>::const_iterator it = statusCodesMap.find(code);

    if (it != statusCodesMap.end())
        return it->second;

    return "Unknown";
}


std::string ZigbeeDevicesDescription::GetStatusDescription(uint8_t code)
{
    return ZigbeeCommands::ZCLFrame::GetErrorString(code);
}

}
