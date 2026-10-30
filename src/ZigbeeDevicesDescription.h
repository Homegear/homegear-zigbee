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

#ifndef ZIGBEEDEVICESDESCRIPTION_H
#define ZIGBEEDEVICESDESCRIPTION_H

#include <cstdint>

#include <homegear-base/BaseLib.h>
#include "PhysicalInterfaces/NodeInfo.h"

namespace Zigbee
{


class DeviceParameter : public ZigbeeType
{
public:
    bool IsEmpty() const { return !attrInfo && commandsReceived.empty() && commandsSent.empty(); }

    bool IsReadable() const
    {
        return (attrInfo && (attrInfo->IsReadable() || attrInfo->IsReportable())) || !commandsSent.empty();
    }

    bool IsWriteable() const
    {
        return (attrInfo && attrInfo->IsWriteable()) || !commandsReceived.empty();
    }


    static bool IsGet(const std::string& name)
    {
        bool isGet = (name.size() > 4 && name.substr(0, 4) == "GET_");
        if (!isGet && name.size() > 8 && name.substr(name.length() - 8) == "_REQUEST")
            // some names do not start with "GET" but they end in "REQUEST"
            isGet = true;

        return isGet;
    }

    static bool IsResponse(const std::string& name)
    {
        return name.length() > 9 && name.substr(name.length() - 9) == "_RESPONSE";
    }

    static bool IsNotification(const std::string& name)
    {
        return name.length() > 13 && name.substr(name.length() - 13) == "_NOTIFICATION";
    }

    bool HasStructure() const
    {
        if (attrInfo) return attrInfo->HasStructure();
        else if (paramInfo) return paramInfo->HasStructure();

        return false;
    }

    const ClustersInfo::ClusterInfoExt *clusterInfo = nullptr;
    const ClustersInfo::AttrInfoExt *attrInfo = nullptr;

    const ClustersInfo::Param *paramInfo = nullptr;

    std::map<uint8_t, const ClustersInfo::ClusterInfoExt::Command*> commandsReceived; // could be more than one?
    std::map<uint8_t, const ClustersInfo::ClusterInfoExt::Command*> commandsSent; // could be more than one?

    bool lastParam = false;
    bool isSubParam = false;
    bool isBitmaskField = false;
};

class ZigbeeFunction : public BaseLib::DeviceDescription::Function
{
public:
    explicit ZigbeeFunction(BaseLib::SharedObjects* baseLib) : BaseLib::DeviceDescription::Function(baseLib) {}
    virtual ~ZigbeeFunction() {}
};

class ZigbeeDescPacket : public BaseLib::DeviceDescription::Packet
{
public:
    explicit ZigbeeDescPacket(BaseLib::SharedObjects* baseLib) : BaseLib::DeviceDescription::Packet(baseLib) {}
    virtual ~ZigbeeDescPacket() {}
};


class ZigbeeParameter : public BaseLib::DeviceDescription::Parameter
{
public:
    ZigbeeParameter(BaseLib::SharedObjects* baseLib, PParameterGroup parent, const DeviceParameter* param = nullptr);
    virtual ~ZigbeeParameter();

    bool isLastParam() const
    {
        if (deviceParameter.attrInfo && !isReportingConfig) return true;

        return deviceParameter.lastParam;
    }

    bool isConfig;
    int channelNo;
    std::string description;
    bool isReportingConfig;

    DeviceParameter deviceParameter;
};

typedef std::shared_ptr<ZigbeeParameter> MyPParameter;

class ZigbeeDevicesDescription : public BaseLib::DeviceDescription::Devices
{
public:
    ZigbeeDevicesDescription(BaseLib::SharedObjects* baseLib, IDevicesEventSink* eventHandler, int32_t family);
    virtual ~ZigbeeDevicesDescription();


    BaseLib::DeviceDescription::PHomegearDevice AddDevice(const std::string& id, uint32_t deviceType, const ZigbeeNodeInfo* info, uint8_t endPoint, const BaseLib::DeviceDescription::PHomegearDevice modelDevice);

    uint64_t getMaxDeviceType() const;

    static std::string GetBaseHomegearName(const std::string& valueKey);

    void FillDevicesMap(std::map<std::string, std::map<uint32_t, uint32_t>>& devicesTypeMap);

    bool CheckDevice(BaseLib::DeviceDescription::PHomegearDevice device);

    void RemoveDeviceType(uint32_t deviceType, uint32_t version);

    static std::string GetStatusString(uint8_t code);
    static std::string GetStatusDescription(uint8_t code);

    static const std::map<uint8_t, std::string> statusCodesMap;

protected:
    std::mutex devicesMutex;

    bool AddFunction(std::shared_ptr<HomegearDevice> device, int channelNo, const ZigbeeNodeInfo::EndpointInfo::ClusterInfo* pDeviceCluster, const ClustersInfo::ClusterInfoExt* pXmlCluster);
    void AddReportingConfigFunction(std::shared_ptr<HomegearDevice> device, int channelNo, const ZigbeeNodeInfo::EndpointInfo::ClusterInfo* pDeviceCluster, const ClustersInfo::ClusterInfoExt* pXmlCluster);

    void AddParameter(PFunction function, MyPParameter parameter, bool config);
    void AddConfigReadonlyParameter(PFunction function, const std::string& name, bool stringVal);
    void AddConfigUint16ReadonlyParameter(PFunction function, const std::string& name);
    void AddConfigReadonlyBoolParameter(PFunction function, const std::string& name);
    void AddReadonlyBoolParameter(PFunction function, const std::string& name);
    void AddBoolParameter(PFunction function, const std::string& name);

    static void SetDevicePacket(BaseLib::DeviceDescription::PHomegearDevice device, BaseLib::DeviceDescription::PPacket packet);

    void SetLogicalAndPhysical(MyPParameter parameter);

    void SetLogicalAndPhysicalEnum(MyPParameter parameter, uint16_t maxValue);
    void SetLogicalAndPhysicalInteger(MyPParameter parameter, uint32_t minval, uint32_t maxval);
    void SetLogicalAndPhysicalInteger64(MyPParameter parameter, uint64_t minval, uint64_t maxval);
    void SetLogicalAndPhysicalString(MyPParameter parameter);
    void SetLogicalAndPhysicalArray(MyPParameter parameter);
    void SetLogicalAndPhysicalStruct(MyPParameter parameter);
    void SetLogicalAndPhysicalSemi(MyPParameter parameter);
    void SetLogicalAndPhysicalFloat(MyPParameter parameter);
    void SetLogicalAndPhysicalDouble(MyPParameter parameter);
    void SetLogicalAndPhysicalBool(MyPParameter parameter);
    void SetLogicalAndPhysicalAction(PParameter parameter);

    BaseLib::DeviceDescription::PHomegearDevice CloneDevice(const BaseLib::DeviceDescription::PHomegearDevice device);
    BaseLib::DeviceDescription::PFunction CloneFunction(const BaseLib::DeviceDescription::PFunction func);

    static void CleanupDevice(BaseLib::DeviceDescription::PHomegearDevice device);
};

}

#endif // ZIGBEEDEVICESDESCRIPTION_H
