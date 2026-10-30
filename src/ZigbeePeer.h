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

#ifndef ZIGBEEPEER_H_
#define ZIGBEEPEER_H_

#include <cstdint>

#include <homegear-base/BaseLib.h>

#include <list>
#include <atomic>

#include "ZigbeePacket.h"
#include "PhysicalInterfaces/IZigbeeInterface.h"
#include "PhysicalInterfaces/NodeInfo.h"
#include "PhysicalInterfaces/ZCL.h"

#include <thread>

using namespace BaseLib;
using namespace BaseLib::DeviceDescription;

namespace Zigbee
{
class ZigbeeCentral;
class DeviceParameter;

class ZigbeePeer : public BaseLib::Systems::Peer, public BaseLib::Rpc::IWebserverEventSink
{
public:
    ZigbeePeer(uint32_t parentID, IPeerEventSink* eventHandler);
    ZigbeePeer(int32_t id, std::string serialNumber, uint32_t parentID, IPeerEventSink* eventHandler);
    virtual ~ZigbeePeer() override;

    void removeHooks();
    void init();
    virtual void dispose() override;

    //Features
    virtual bool wireless() override
    {
        return true;
    }

    bool wakeupDevice()
    {
        std::lock_guard<std::mutex> serviceLock(infoMutex);

        return nodeInfo.IsWakeupDevice();
    }

    //End features

    //{{{ In table variables
    std::string getPhysicalInterfaceId();
    void setPhysicalInterfaceId(std::string);
    //}}}

    std::shared_ptr<IZigbeeInterface>& getPhysicalInterface()
    {
        return _physicalInterface;
    }
    virtual void setPhysicalInterface(std::shared_ptr<IZigbeeInterface> interface);


    virtual void worker();
    virtual std::string handleCliCommand(std::string command) override;
    void packetReceived(PZigbeePacket packet);

    virtual bool load(BaseLib::Systems::ICentral* central) override;
    virtual void savePeers() override {}

    virtual int32_t getChannelGroupedWith(int32_t channel)
    {
        return -1;
    }
    virtual int32_t getNewFirmwareVersion() override
    {
        return 0;
    }
    virtual std::string getFirmwareVersionString(int32_t firmwareVersion) override
    {
        return "1.0";
    }
    virtual bool firmwareUpdateAvailable()
    {
        return false;
    }

    std::string printConfig();

    /**
     * {@inheritDoc}
     */
    virtual void homegearStarted() override;

    /**
     * {@inheritDoc}
     */
    virtual void homegearShuttingDown() override;


    //RPC methods
    virtual PVariable getDeviceInfo(BaseLib::PRpcClientInfo clientInfo, std::map<std::string, bool> fields) override;

    virtual PVariable getConfigParameter(PRpcClientInfo clientInfo, uint32_t channel, std::string name) override;

    virtual PVariable getParamsetDescription(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, bool checkAcls) override;
    virtual PVariable getParamset(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, bool checkAcls) override;


    virtual PVariable getValueFromDevice(std::shared_ptr<Parameter>& parameter, int32_t channel, bool asynchronous) override;
    virtual PVariable getValue(BaseLib::PRpcClientInfo clientInfo, uint32_t channel, std::string valueKey, bool requestFromDevice, bool asynchronous) override;

    virtual PVariable putParamset(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, PVariable variables, bool checkAcls, bool onlyPushing = false) override;
    virtual PVariable setValue(BaseLib::PRpcClientInfo clientInfo, uint32_t channel, std::string valueKey, PVariable value, bool wait) override;
    //End RPC methods

    void SendAllGetPackets(bool wakeupDevice);

    bool isConfigPending() const
    {
        return config_pending;
    }

    bool isGetStillOn() const
    {
        return !getPacketsSent;
    }

    void initializeServiceVariables();

    void RefreshDeviceDescription();

    void NotifyTimeout(PZigbeePacket packet);
    void NotifyError(PZigbeePacket packet, const ZigbeeCommands::ZCLFrame& zclFrame);

    uint64_t IEEEaddr;
    uint8_t endPoint;

    std::atomic_bool getPacketsSent;
    std::atomic_bool config_pending;
    std::atomic_bool errorOrTimeoutOccured;
    std::atomic_bool metadataRefreshed;

    std::thread getThread;

    std::mutex infoMutex;
    ZigbeeNodeInfo nodeInfo;

protected:
	class FrameValue
	{
	public:
		std::list<uint32_t> channels;
		std::vector<uint8_t> value;
	};

	class FrameValues
	{
	public:
		std::string frameID;
		std::list<uint32_t> paramsetChannels;
		ParameterGroup::Type::Enum parameterSetType;
		std::map<std::string, FrameValue> values;
	};


    //In table variables:
    std::string _physicalInterfaceId;

    std::atomic_bool _shuttingDown;

    std::shared_ptr<IZigbeeInterface> _physicalInterface;

  	uint32_t _lastRssiDevice = 0;

    std::mutex serviceMessagesMutex;

    //{{{ getValueFromDevice
    struct GetValueFromDeviceInfo
    {
        std::mutex getValueMutex;
        std::mutex accessMutex;
        bool requested = false;
        std::mutex conditionMutex;
        std::condition_variable conditionVariable;
        bool mutexReady = false;

        int32_t channel = -1;
        std::string variableName;
        PVariable value;
    };

    GetValueFromDeviceInfo _getValueFromDeviceInfo;
    //}}}


    std::mutex valuesMutex;

    std::pair<uint16_t, uint16_t> lastClusterAndAttr;
    std::map<std::string, std::pair<BaseLib::PVariable, std::vector<uint8_t>>> valuesMap;

    class SetConfig
    {
    public:
        uint16_t clusterId = 0;
        uint16_t attrId = 0;
        ZigbeeType type;
    };

    class AttrSetConfig : public SetConfig
    {
    public:
        std::vector<uint8_t> value;
    };

    class ReportingConfig : public SetConfig
    {
    public:
        uint16_t minReportingInterval = 0;
        uint16_t maxReportingInterval = 0xFFFF;
        std::vector<uint8_t> reportableChange; // only for 'analog' data types, has the same type as the attribute
    };

    class ConfigInfo
    {
    public:
        std::map<std::pair<uint16_t, uint16_t>, AttrSetConfig> attrSetConfigMap;
        std::map<std::pair<uint16_t, uint16_t>, ReportingConfig> reportingConfigMap;
    };


    void CheckAddOptionalMandatory();
    void SetValuesFromValuesMap();
    void SendReportConfigPackets(ConfigInfo& conf);
    void SendReportConfigGetPackets();

    bool LoadConfigFile(ConfigInfo& conf);
    void SendAttrSetPackes(ConfigInfo& conf);

    bool ParsePacketStatic(PZigbeePacket packet, std::map<uint32_t, std::shared_ptr<std::vector<std::string>>>& valueKeys, std::map<uint32_t, std::shared_ptr<std::vector<PVariable>>>& rpcValues, bool fromNode);


    bool ParseAttributeZCLFrame(PZigbeePacket packet, std::map<uint32_t, std::shared_ptr<std::vector<std::string>>>& valueKeys, std::map<uint32_t, std::shared_ptr<std::vector<PVariable>>>& rpcValues, bool fromNode, const ZigbeeCommands::ZCLFrame& zclFrame, uint16_t clusterId, bool& modified, bool& lastResponsePacket);
    bool ParseReportingZCLFrame(PZigbeePacket packet, std::map<uint32_t, std::shared_ptr<std::vector<std::string>>>& valueKeys, std::map<uint32_t, std::shared_ptr<std::vector<PVariable>>>& rpcValues, bool fromNode, const ZigbeeCommands::ZCLFrame& zclFrame, uint16_t clusterId);
    bool ParseLocalZCLFrame(PZigbeePacket packet, std::map<uint32_t, std::shared_ptr<std::vector<std::string>>>& valueKeys, std::map<uint32_t, std::shared_ptr<std::vector<PVariable>>>& rpcValues, bool fromNode, const ZigbeeCommands::ZCLFrame& zclFrame, uint16_t clusterId);

    bool ParsePacketDynamic(PZigbeePacket packet, std::map<uint32_t, std::shared_ptr<std::vector<std::string>>>& valueKeys, std::map<uint32_t, std::shared_ptr<std::vector<PVariable>>>& rpcValues, bool fromNode);

    bool DealWithStaticSpecialParams(std::map<uint32_t, std::shared_ptr<std::vector<std::string>>>& valueKeys, std::map<uint32_t, std::shared_ptr<std::vector<PVariable>>>& rpcValues);

    void AddValueForEvents(std::map<uint32_t, std::shared_ptr<std::vector<std::string>>>& valueKeys, std::map<uint32_t, std::shared_ptr<std::vector<PVariable>>>& rpcValues, int channel, const std::string& valueKey, PVariable variable);

    virtual void loadVariables(BaseLib::Systems::ICentral* central, std::shared_ptr<BaseLib::Database::DataTable>& rows) override;
    virtual void saveVariables() override;

    virtual std::shared_ptr<BaseLib::Systems::ICentral> getCentral() override;

    virtual PParameterGroup getParameterSet(int32_t channel, ParameterGroup::Type::Enum type) override;

    void getValuesFromPacket(PZigbeePacket packet, std::vector<FrameValues>& frameValues);

    void AddSetPacketsToMap(std::map<std::string, std::shared_ptr<Parameter::Packet>>& setPacketsMap, std::map<std::string, std::shared_ptr<Parameter>>& setParametersMap, const Struct::iterator& variable, BaseLib::Systems::RpcConfigurationParameter& parameter);

    static void WaitForPacket(const PZigbeePacket& packet, bool& packetSent, bool& receivedAck);

    PVariable setValue(BaseLib::PRpcClientInfo clientInfo, uint32_t channel, std::string valueKey, PVariable value, bool wait, bool sendPackets);

    PVariable getChannelZeroVariable(std::string valueKey);
    void setServiceVariable(const std::string& id);

    bool HandleSpecialAttr(uint16_t clusterId, uint16_t attrId, uint8_t cmd, const BaseLib::PVariable value);
    bool HandleSpecialCommand(uint16_t clusterId, const ZigbeeCommands::ZCLFrame& zclFrame);
    bool HandleSpecialPacket(const PZigbeePacket& packet, std::map<uint32_t, std::shared_ptr<std::vector<std::string>>>& valueKeys, std::map<uint32_t, std::shared_ptr<std::vector<PVariable>>>& rpcValues);

    void Notify(int channel, std::string valueKey, PVariable value);

    PZigbeePacket ConstructGetOrSetPacket(uint32_t addr, const std::shared_ptr<Parameter::Packet>& getRequest, const std::shared_ptr<Parameter>& parameter, std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>>::iterator channelIterator, bool isGet);
    PZigbeePacket ConstructGetPacketForAttr(uint16_t clusterId, uint16_t attrId);

    void setRssiDevice(int8_t rssi);

    static bool SetSimpleVariableOrStructToBinary(const std::vector<ClustersInfo::Param> *params, PVariable value, std::vector<uint8_t>& buffer);

    static bool SetComplexVariableBinary(const std::vector<ClustersInfo::Param> *params, uint8_t type, PVariable value, std::vector<uint8_t>& buffer);
    static bool SetComplexVariableBinary(const DeviceParameter* deviceParameter, PVariable value, std::vector<uint8_t>& buffer);

    static bool LoadVector(std::vector<uint16_t>& vect, std::shared_ptr<std::vector<char>> binaryValue);

	static void memcpyLittleEndian(int32_t& to, const std::vector<uint8_t>& from);
	static void memcpyLittleEndian(std::vector<uint8_t>& to, int32_t from);
};


}

#endif
