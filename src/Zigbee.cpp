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

#include "GD.h"
#include "Interfaces.h"
#include "Zigbee.h"
#include "ZigbeeCentral.h"
#include "ZigbeeDevicesDescription.h"


namespace Zigbee
{

Zigbee::Zigbee(BaseLib::SharedObjects* bl, BaseLib::Systems::IFamilyEventSink* eventHandler)
    : BaseLib::Systems::DeviceFamily(bl, eventHandler, ZIGBEE_FAMILY_ID, ZIGBEE_FAMILY_NAME)
{
    _clustersInfo = std::make_shared<ClustersInfo>();

    GD::bl = bl;
    GD::family = this;
    GD::out.init(bl);
    GD::out.setPrefix("Module Zigbee: ");
    GD::out.printDebug("Debug: Loading module...");

    GD::interfaces = std::make_shared<Interfaces>(bl, _settings->getPhysicalInterfaceSettings());
	_physicalInterfaces = GD::interfaces;

    const std::string xmlClustersConfigFile = bl->settings.familyConfigPath() + "zigbee-clusters.xml";
    std::ifstream in(xmlClustersConfigFile);

    if(in.is_open())
    {
        std::stringstream buffer;
        buffer << in.rdbuf();
        std::string contents(buffer.str());

        _clustersInfo->Parse((char*)contents.data());
    }
}

Zigbee::~Zigbee()
{
}

void Zigbee::dispose()
{
    if(_disposed) return;

    //for(std::map<std::string, std::shared_ptr<IZigbeeInterface>>::iterator i = GD::physicalInterfaces.begin(); i != GD::physicalInterfaces.end(); ++i)
    //{
    //    i->second->Close();
    //}

    DeviceFamily::dispose();

    _central.reset();
    GD::interfaces.reset();
    _physicalInterfaces.reset();
}

void Zigbee::createCentral()
{
    try
    {
        _central = std::make_shared<ZigbeeCentral>(0, "ZIGBEE001", this);
        GD::out.printMessage("Created Zigbee central with id " + std::to_string(_central->getId()) + ".");
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

std::shared_ptr<BaseLib::Systems::ICentral> Zigbee::initializeCentral(uint32_t deviceId, int32_t address, std::string serialNumber)
{
    return std::make_shared<ZigbeeCentral>(deviceId, serialNumber, this);
}

PVariable Zigbee::getPairingInfo()
{
    try
    {
        if(!_central) return std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
        PVariable info = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);

        //{{{ General
        info->structValue->emplace("searchInterfaces", std::make_shared<BaseLib::Variable>(false));
        //}}}

        //{{{ Pairing methods
        PVariable pairingMethods = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);

        //{{{ setInstallMode
        PVariable setInstallModeMetadata = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
        PVariable setInstallModeMetadataInfo = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);

        setInstallModeMetadataInfo->structValue->emplace("interfaceSelector", std::make_shared<BaseLib::Variable>(false));

        PVariable typeSelector = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);

        PVariable additionalFields = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);

        PVariable typeSelectorInsecure = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
        typeSelectorInsecure->structValue->emplace("name", std::make_shared<BaseLib::Variable>("Insecure"));
        typeSelectorInsecure->structValue->emplace("value", std::make_shared<BaseLib::Variable>(false));
        typeSelectorInsecure->structValue->emplace("additionalFields", additionalFields);

        typeSelector->structValue->emplace("insecure", typeSelectorInsecure);

        PVariable typeSelectorSecure = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
        typeSelectorSecure->structValue->emplace("name", std::make_shared<BaseLib::Variable>("Secure"));
        typeSelectorSecure->structValue->emplace("value", std::make_shared<BaseLib::Variable>(true));

        typeSelectorSecure->structValue->emplace("additionalFields", additionalFields);

        typeSelector->structValue->emplace("secure", typeSelectorSecure);

        setInstallModeMetadataInfo->structValue->emplace("typeSelector", typeSelector);
        setInstallModeMetadata->structValue->emplace("metadataInfo", setInstallModeMetadataInfo);

        pairingMethods->structValue->emplace("setInstallMode", setInstallModeMetadata);
        info->structValue->emplace("pairingMethods", pairingMethods);
        //}}}
        //}}}

        //{{{ interfaces
        PVariable interfaces = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);

        //{{{ Serial module
        auto interface = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
        interface->structValue->emplace("name", std::make_shared<BaseLib::Variable>(std::string("Serial module")));
        interface->structValue->emplace("ipDevice", std::make_shared<BaseLib::Variable>(false));

        auto field = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
        field->structValue->emplace("pos", std::make_shared<BaseLib::Variable>(0));
        field->structValue->emplace("label", std::make_shared<BaseLib::Variable>(std::string("l10n.zigbee.pairingInfo.id")));
        field->structValue->emplace("type", std::make_shared<BaseLib::Variable>(std::string("string")));
        interface->structValue->emplace("id", field);

        field = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
        field->structValue->emplace("pos", std::make_shared<BaseLib::Variable>(1));
        field->structValue->emplace("label", std::make_shared<BaseLib::Variable>(std::string("l10n.zigbee.pairingInfo.device")));
        field->structValue->emplace("type", std::make_shared<BaseLib::Variable>(std::string("string")));
        interface->structValue->emplace("device", field);

        field = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
        field->structValue->emplace("pos", std::make_shared<BaseLib::Variable>(2));
        field->structValue->emplace("label", std::make_shared<BaseLib::Variable>(std::string("l10n.zigbee.pairingInfo.aeskey")));
        field->structValue->emplace("type", std::make_shared<BaseLib::Variable>(std::string("string")));
        interface->structValue->emplace("password", field);

        interfaces->structValue->emplace("serial", interface);
        //}}}

		//{{{ Gateway
		interface = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		interface->structValue->emplace("name", std::make_shared<BaseLib::Variable>(std::string("Homegear Gateway")));
		interface->structValue->emplace("ipDevice", std::make_shared<BaseLib::Variable>(true));

		field = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		field->structValue->emplace("pos", std::make_shared<BaseLib::Variable>(0));
		field->structValue->emplace("label", std::make_shared<BaseLib::Variable>(std::string("l10n.zigbee.pairingInfo.id")));
		field->structValue->emplace("type", std::make_shared<BaseLib::Variable>(std::string("string")));
		interface->structValue->emplace("id", field);

		field = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		field->structValue->emplace("pos", std::make_shared<BaseLib::Variable>(1));
		field->structValue->emplace("label", std::make_shared<BaseLib::Variable>(std::string("l10n.zigbee.pairingInfo.hostname")));
		field->structValue->emplace("type", std::make_shared<BaseLib::Variable>(std::string("string")));
		interface->structValue->emplace("host", field);

		field = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		field->structValue->emplace("pos", std::make_shared<BaseLib::Variable>(2));
		field->structValue->emplace("label", std::make_shared<BaseLib::Variable>(std::string("l10n.zigbee.pairingInfo.password")));
		field->structValue->emplace("type", std::make_shared<BaseLib::Variable>(std::string("string")));
		interface->structValue->emplace("password", field);

		field = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		field->structValue->emplace("pos", std::make_shared<BaseLib::Variable>(2));
		field->structValue->emplace("label", std::make_shared<BaseLib::Variable>(std::string("l10n.zigbee.pairingInfo.default")));
		field->structValue->emplace("type", std::make_shared<BaseLib::Variable>(std::string("boolean")));
		interface->structValue->emplace("default", field);

		field = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		field->structValue->emplace("type", std::make_shared<BaseLib::Variable>(std::string("string")));
		field->structValue->emplace("const", std::make_shared<BaseLib::Variable>(std::string("2017")));
		interface->structValue->emplace("port", field);

		field = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		field->structValue->emplace("type", std::make_shared<BaseLib::Variable>(std::string("string")));
		field->structValue->emplace("const", std::make_shared<BaseLib::Variable>(std::string("/etc/homegear/ca/cacert.pem")));
		interface->structValue->emplace("caFile", field);

		field = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		field->structValue->emplace("type", std::make_shared<BaseLib::Variable>(std::string("string")));
		field->structValue->emplace("const", std::make_shared<BaseLib::Variable>(std::string("/etc/homegear/ca/certs/gateway-client.crt")));
		interface->structValue->emplace("certFile", field);

		field = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		field->structValue->emplace("type", std::make_shared<BaseLib::Variable>(std::string("string")));
		field->structValue->emplace("const", std::make_shared<BaseLib::Variable>(std::string("/etc/homegear/ca/private/gateway-client.key")));
		interface->structValue->emplace("keyFile", field);

		field = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
		field->structValue->emplace("type", std::make_shared<BaseLib::Variable>(std::string("integer")));
		field->structValue->emplace("const", std::make_shared<BaseLib::Variable>(90));
		interface->structValue->emplace("responseDelay", field);

		interfaces->structValue->emplace("homegeargateway", interface);
		//}}}

        info->structValue->emplace("interfaces", interfaces);
        //}}}

        return info;
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return Variable::createError(-32500, "Unknown application error.");
}

void Zigbee::homegearStarted()
{
    BaseLib::Systems::DeviceFamily::homegearStarted();

}

bool Zigbee::init()
{
    // replace the default created 'devices' object with the specialized one
    std::shared_ptr<ZigbeeDevicesDescription> devs = std::make_shared<ZigbeeDevicesDescription>(_bl, this, getFamily());
    _rpcDevices = devs;
    if (!BaseLib::Systems::DeviceFamily::init()) return false; // this will load the xml files in the new 'devices'
    _modelDevice = _rpcDevices->find(0, 0);

    return true;
}


bool Zigbee::peerIEEEExists(uint64_t IEEEaddr)
{
    if (!_disposed)
    {
        auto central = std::dynamic_pointer_cast<ZigbeeCentral>(_central);
        if (central) return central->peerIEEEExists(IEEEaddr);
    }

    return false;
}


bool Zigbee::addressChanged(uint64_t IEEEaddr, uint16_t newShortAddr)
{
    if (!_disposed)
    {
        auto central = std::dynamic_pointer_cast<ZigbeeCentral>(_central);
        if (central) return central->addressChanged(IEEEaddr, newShortAddr);
    }

    return false;
}


uint32_t Zigbee::createDeviceForNode(ZigbeeNodeInfo& nodeInfo, uint8_t endPoint)
{
    if (!_disposed)
    {
        auto central = std::dynamic_pointer_cast<ZigbeeCentral>(_central);
        if (central) return central->createDeviceForNode(nodeInfo, endPoint);
    }

    return -1;
}

void Zigbee::deletePeers(uint64_t IEEEaddr, uint16_t shortAddr)
{
    if (!_disposed)
    {
        auto central = std::dynamic_pointer_cast<ZigbeeCentral>(_central);
        if (central) return central->deletePeers(IEEEaddr, shortAddr);
    }
}

void Zigbee::updatePeer(ZigbeeNodeInfo& nodeInfo, uint8_t endPoint, const std::string& interfaceID)
{
    if (!_disposed)
    {
        auto central = std::dynamic_pointer_cast<ZigbeeCentral>(_central);
        if (central) return central->updatePeer(nodeInfo, endPoint, interfaceID);
    }
}

bool Zigbee::IsWakeup(uint32_t addr)
{
    if (!_disposed)
    {
        auto central = std::dynamic_pointer_cast<ZigbeeCentral>(_central);
        if (central) return central->IsWakeup(addr);
    }

    return false;
}

void Zigbee::NotifyTimeout(PZigbeePacket packet)
{
    if (!_disposed)
    {
        auto central = std::dynamic_pointer_cast<ZigbeeCentral>(_central);
        if (central) return central->NotifyTimeout(packet);
    }
}

void Zigbee::NotifyError(PZigbeePacket packet, const ZigbeeCommands::ZCLFrame& zclFrame)
{
    if (!_disposed)
    {
        auto central = std::dynamic_pointer_cast<ZigbeeCentral>(_central);
        if (central) return central->NotifyError(packet, zclFrame);
    }
}

}


