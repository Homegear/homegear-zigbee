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

#ifndef NODEINFO_H_
#define NODEINFO_H_

#include <cstdint>

#include "ZDO.h"

#include <map>
#include <set>
#include <iomanip>
#include <bitset>

#include <homegear-base/Encoding/RapidXml/rapidxml.h>

#include "MT.h"

namespace Zigbee
{

class ZigbeeType
{
public:
    ZigbeeType(uint8_t t = 0xFF) :  type(t) {}

    int GetSize() const
    {
        switch(type)
        {
        case 0: // No data
        case 0xff: // unknown
            return 0;
        case 0x08: // 8-bit data
        case 0x10: // bool
        case 0x18: // 8-bit bitmap
        case 0x20: // uint 8bit
        case 0x28: // int8
        case 0x30: // enum8
            return 1;
        case 0x09: // 16-bit data
        case 0x19: // 16-bit bitmap
        case 0x21: // uint 16bit
        case 0x29: // int16
        case 0x31: // enum16
        case 0x38: // semi - float
        case 0xe8: // cluster id
        case 0xe9: // attr id
            return 2;
        case 0x0a: // 24-bit data
        case 0x1a: // 24-bit bitmap
        case 0x22: // uint24
        case 0x2a: // int24
            return 3;
        case 0x0b: // 32-bit data
        case 0x1b: // 32-bit bitmap
        case 0x23: // uint32
        case 0x2b: // int32
        case 0x39: // single float
        case 0xe0: // time of day
        case 0xe1: // date
        case 0xe2: // utc time
        case 0xea: // bac oid
            return 4;
        case 0x0c: // 40-bit data
        case 0x1c: // 40-bit bitmap
        case 0x24: // uint40
        case 0x2c: // int40
            return 5;
        case 0x0d: // 48-bit data
        case 0x1d: // 48-bit bitmap
        case 0x25: // uint48
        case 0x2d: // int48
            return 6;
        case 0x0e: // 56-bit data
        case 0x1e: // 56-bit bitmap
        case 0x26: // uint56
        case 0x2e: // int56
            return 7;
        case 0x0f: // 64-bit data
        case 0x1f: // 64-bit bitmap
        case 0x27: // uint64
        case 0x2f: // int64
        case 0x3a: // double
        case 0xf0: // IEEE address
            return 8;
        case 0xf1: // key128
            return 16;
        }

        return 0;
    }

    static uint8_t GetType(const std::string& name)
    {
        if (name == "data8")
            return 0x08;
        else if (name == "data16")
            return 0x09;
        else if (name == "data24")
            return 0x0A;
        else if (name == "data32")
            return 0x0B;
        else if (name == "data40")
            return 0x0C;
        else if (name == "data48")
            return 0x0D;
        else if (name == "data56")
            return 0x0E;
        else if (name == "data64")
            return 0x0F;
        else if (name == "bool")
            return 0x10;
        else if (name == "map8")
            return 0x18;
        else if (name == "map16")
            return 0x19;
        else if (name == "map24")
            return 0x1A;
        else if (name == "map32")
            return 0x1B;
        else if (name == "map40")
            return 0x1C;
        else if (name == "map48")
            return 0x1D;
        else if (name == "map56")
            return 0x1E;
        else if (name == "map64")
            return 0x1F;
        else if (name == "uint8")
            return 0x20;
        else if (name == "uint16")
            return 0x21;
        else if (name == "uint24")
            return 0x22;
        else if (name == "uint32")
            return 0x23;
        else if (name == "uint40")
            return 0x24;
        else if (name == "uint48")
            return 0x25;
        else if (name == "uint56")
            return 0x26;
        else if (name == "uint64")
            return 0x27;
        else if (name == "int8")
            return 0x28;
        else if (name == "int16")
            return 0x29;
        else if (name == "int24")
            return 0x2A;
        else if (name == "int32")
            return 0x2B;
        else if (name == "int40")
            return 0x2C;
        else if (name == "int48")
            return 0x2D;
        else if (name == "int56")
            return 0x2E;
        else if (name == "int64")
            return 0x2F;
        else if (name == "enum8")
            return 0x30;
        else if (name == "enum16")
            return 0x31;
        else if (name == "semi")
            return 0x38;
        else if (name == "single")
            return 0x39;
        else if (name == "double")
            return 0x3A;
        else if (name == "octstr")
            return 0x41;
        else if (name == "string")
            return 0x42;
        else if (name == "octstr16")
            return 0x43;
        else if (name == "string16")
            return 0x44;
        else if (name == "array")
            return 0x48;
        else if (name == "struct")
            return 0x4C;
        else if (name == "set")
            return 0x50;
        else if (name == "bag")
            return 0x51;
        else if (name == "ToD")
            return 0xE0;
        else if (name == "date")
            return 0xE1;
        else if (name == "UTC")
            return 0xE2;
        else if (name == "clusterId")
            return 0xE8;
        else if (name == "attrId")
            return 0xE9;
        else if (name == "bacOID")
            return 0xEA;
        else if (name == "EUI64")
            return 0xF0;
        else if (name == "key128")
            return 0xF1;
        else if (name == "unk")
            return 0xFF;
        else if (name == "nodata")
            return 0;

        return 0xFF;
    }

    bool TypeWithLengthComplexities() const
    {
        return 0x4C == type /*struct*/ || 0x48 == type /* array */ || 0x50 == type /* set */ || 0x51 == type /* bag*/;
    }


    bool LengthInFirstOctet() const
    {
        //Octet string, Character string
        if (0x41 == type || 0x42 == type)
            return true;

        return false;
    }

    bool LengthInFirstTwoOctets() const
    {
        // Long octet string, Long character string
        if (0x43 == type || 0x44 == type)
            return true;

        return false;
    }


    bool HasFixedSize() const
    {
        return GetSize() > 0;
    }

    bool HasKnowableSize() const
    {
        return HasFixedSize() || LengthInFirstOctet() || LengthInFirstTwoOctets();
    }

    int DataLength(const uint8_t* data) const
    {
        int size = GetSize();

        if (size > 0)
            return size;
        else if (LengthInFirstOctet())
            return data[0];
        else if (LengthInFirstTwoOctets())
        {
            size = data[1];
            size <<= 8;
            size |= data[0];

            // character strings count chars, not bytes (octstr counts bytes)
            // the 'long' string has chars of two bytes
            if (0x44 == type) return 2 * size;
            else return size;
        }

        return 0;
    }

    int DataLength(const std::vector<uint8_t>& buffer, int offset) const
    {
        return DataLength(buffer.data() + offset);
    }

    std::vector<uint8_t> GetRawData(const std::vector<uint8_t>& buffer, int offset, bool considerLittleEndian = true) const
    {
        std::vector<uint8_t> data;

        int headerSize = 0;
        if (LengthInFirstOctet())
            headerSize = 1;
        else if (LengthInFirstTwoOctets())
            headerSize = 2;

        if (considerLittleEndian && IsLittleEndian())
        {
            // copy in reverse
            std::vector<uint8_t> intermData(buffer.data() + offset + headerSize, buffer.data() + offset + headerSize + DataLength(buffer.data() + offset));

            data = std::vector<uint8_t>(intermData.rbegin(), intermData.rend());
        }
        else
            data = std::vector<uint8_t>(buffer.data() + offset + headerSize, buffer.data() + offset + headerSize + DataLength(buffer.data() + offset));

        return data;
    }

    void SetRawData(std::vector<uint8_t>& buffer, int& offset, const std::vector<uint8_t>& rawData, bool considerLittleEndian = true) const
    {
        int headerSize = 0;
        if (LengthInFirstOctet())
            headerSize = 1;
        else if (LengthInFirstTwoOctets())
            headerSize = 2;

        unsigned int dataSize = GetSize();

        std::vector<uint8_t> data = rawData;

        if (dataSize && data.size() != dataSize)
            data.resize(dataSize, 0);

        const unsigned int neededSize = offset + headerSize + data.size();
        if (buffer.size() < neededSize)
            buffer.resize(neededSize, 0);

        if (headerSize)
        {
            if (LengthInFirstOctet())
                buffer[offset] = data.size();
            else
            {
                uint16_t size = data.size();

                if (0x44 == type) size /= 2;

                buffer[offset] = (size & 0xFF);
                size >>= 8;
                buffer[offset + 1] = size;
            }

            offset += headerSize;
        }

        if (considerLittleEndian && IsLittleEndian())
            std::copy(data.rbegin(), data.rend(), buffer.begin() + offset);
        else
            std::copy(data.begin(), data.end(), buffer.begin() + offset);

        offset += data.size();
    }


    BaseLib::PVariable GetVariable(const std::vector<uint8_t>& buffer, int offset) const;

    bool SetVariable(BaseLib::PVariable variable, std::vector<uint8_t>& buffer, bool setLengthPrefix = false) const;

    // ordered sequences

    // length = 2 + sum of lengths of contents (the first two = nr of elems?)

    // the zeroth element for both array and struct is a int16 that contains the number of elements

    bool IsArray() const
    {
        return 0x48 == type;
    }

    bool IsStruct() const
    {
        return 0x4C == type;
    }

    // collections
    // length = Sum of lengths of contents

    // no duplicates
    bool IsSet() const
    {
        return 0x50 == type;
    }

    // bag is like a set, but allows duplicates
    bool IsBag() const
    {
        return 0x51 == type;
    }

    bool HasVariableLength() const
    {
        return LengthInFirstOctet() || LengthInFirstTwoOctets() || IsArray() || IsStruct() || IsSet() || IsBag();
    }

    bool IsLittleEndian() const
    {
        // almost everything is little endian in zigbee, but not all things, for example strings appear not to be
        // I'll refine this after having more experience with the data
        if (HasVariableLength())
            return false; // for now assume that only fixed length types are in little endian order

        return true;
    }

    bool IsBool() const
    {
        return 0x10 == type;
    }

    bool IsGeneralData() const
    {
        return type >= 0x08 && type <= 0x0f;
    }

    bool IsBitmap() const
    {
        return type >= 0x18 && type <= 0x1f;
    }

    bool IsUnsignedInt() const
    {
        return type >= 0x20 && type <= 0x27;
    }

    bool IsSignedInt() const
    {
        return type >= 0x28 && type <= 0x2f;
    }

    bool IsInt() const
    {
        return IsSignedInt() || IsUnsignedInt();
    }

    bool IsEnum() const
    {
        return 0x30 == type || 0x31 == type;
    }

    bool IsFloatingPoint() const
    {
        return type >= 0x38 && type <= 0x3a;
    }

    bool IsShortString() const
    {
        return 0x41 == type || 0x42 == type;
    }

    bool IsLongString() const
    {
        return 0x43 == type || 0x44 == type;
    }

    bool IsCharString() const
    {
        return 0x42 == type || 0x44 == type;
    }

    bool IsOctetString() const
    {
        return 0x41 == type || 0x43 == type;
    }

    bool IsOrderedSequence() const
    {
        return IsArray() || IsStruct();
    }

    bool IsCollection() const
    {
        return IsSet() || IsBag();
    }

    bool IsTime() const
    {
        return type >= 0xe0 && type <= 0xe2;
    }

    bool IsID() const
    {
        return type >= 0xe8 && type <= 0xea;
    }

    // discrete data types have individual meaning, cannot be added and subtracted,
    // the analog ones typically are measured values that vary continuously over a range, may be added and subtracted
    bool IsAnalog() const
    {
        return IsUnsignedInt() || IsSignedInt() || IsFloatingPoint() || IsTime();
    }

    bool IsDiscrete() const
    {
        return !IsAnalog();
    }

    bool IsUnknown() const
    {
        return 0xFF == type;
    }

    uint8_t type;
};

class ZigbeeNodeInfo
{
public:
    class EndpointInfo
    {
    public:
        class ClusterInfo
        {
        public:
            class AttrInfo : public ZigbeeType
            {
            public:
                AttrInfo(uint16_t id = 0) : attrId(id), accessControl(0) {}

                bool IsReadable() const
                {
                    return accessControl & 0x1;
                }
                bool IsWriteable() const
                {
                    return accessControl & 0x2;
                }
                bool IsReportable() const
                {
                    return accessControl & 0x4;
                }

                bool IsVendorSpecific() const
                {
                    return attrId >= 0x4000;
                }

                uint16_t attrId;
                uint8_t accessControl;
            };

            ClusterInfo(uint16_t id = 0) : clusterId(id), server(false), allAvailable(false), curAttrInd(0) {}

            static bool IsManufacturerSpecific(uint16_t cId)
            {
                return cId >= 0xFC00;
            }

            // TODO: add here all those that should be exposed in homegear as config channels
            static bool IsConfigCluster(uint16_t cId)
            {
                // BASIC, POWER_CONFIGURATION, DEVICE_TEMPERATURE_CONFIGURATION, IDENTIFY, GROUPS, SCENES
                return 0x0 == cId || 0x1 == cId || 0x2 == cId || 0x3 == cId || 0x4 == cId || 0x5 == cId ||
                       //ONOFF_SWITCH_CONFIGURATION
                       0x7 == cId;
            }

            uint16_t clusterId;
            bool server;

            bool allAvailable;

            uint8_t curAttrInd;

            std::vector<uint16_t> attrsList;
            std::map<uint16_t, AttrInfo> attrsMap;

            std::set<uint8_t> commandsReceived;
            std::set<uint8_t> commandsSent;
        };

        EndpointInfo(uint8_t ep = 0)
            : Endpoint(ep), ProfileId(0), DeviceId(0), DeviceVersion(0), curClusterInd(0), isValid(false)
        {
        }

        bool IsValid() const
        {
            return isValid;
        }

        void SetInfo(const ZigbeeCommands::ZDOSimpleDescNotification& simpleDescNotification)
        {
            if (simpleDescNotification.Endpoint != Endpoint)
                return;

            ProfileId = simpleDescNotification.ProfileId;
            DeviceId = simpleDescNotification.DeviceId;
            DeviceVersion = simpleDescNotification.DeviceVersion;

            InClusterList = simpleDescNotification.InClusterList;
            OutClusterList = simpleDescNotification.OutClusterList;

            FillAllClustersList();

            isValid = true;
        }

        void FillAllClustersList()
        {
            for (uint16_t cluster : InClusterList)
            {
                ServersClustersInfoMap[cluster].clusterId = cluster;
                ServersClustersInfoMap[cluster].server = true;
            }

            for (uint16_t cluster : OutClusterList)
            {
                ClientsClustersInfoMap[cluster].clusterId = cluster;
                ClientsClustersInfoMap[cluster].server = false;
            }

            AllClusterList = InClusterList;
            //AllClusterList.reserve(InClusterList.size() + OutClusterList.size());

            // this is not too bad, since the list is typically short
            std::sort(AllClusterList.begin(), AllClusterList.end());
            // just in case it has duplicates, remove them
            AllClusterList.erase(std::unique(AllClusterList.begin(), AllClusterList.end()), AllClusterList.end());

            // TODO: for now don't add the 'client' clusters to the AllClusters list
            // I'll see later if it's worth it to do so (or handle those differently)

            //auto end = AllClusterList.end(); // will grow, so save it
            /*
            for (uint16_t cluster : OutClusterList)
            {
                //if (std::find(AllClusterList.begin(), end, cluster) == end)
                    AllClusterList.emplace_back(cluster);
            }

            std::sort(AllClusterList.begin(), AllClusterList.end());
            */
        }

        bool HasBasicClusterServerSupport() const
        {
            return ServersClustersInfoMap.find(0) != ServersClustersInfoMap.end();
        }

        bool HasOnlyManufacturerSpecificServerClusters() const
        {
            for (uint16_t cluster : InClusterList)
            {
                if (!ClusterInfo::IsManufacturerSpecific(cluster))
                    return false;
            }

            return true;
        }

        uint8_t Endpoint;
        uint16_t ProfileId;
        uint16_t DeviceId;
        uint8_t DeviceVersion;

        std::vector<uint16_t> InClusterList;
        std::vector<uint16_t> OutClusterList;

        std::vector<uint16_t> AllClusterList;

        std::map<uint16_t, ClusterInfo> ServersClustersInfoMap;
        std::map<uint16_t, ClusterInfo> ClientsClustersInfoMap;

        std::string modelIdentifier; // a maximum of 32 bytes in length

        uint8_t curClusterInd;

    protected:
        bool isValid;
    };


    ZigbeeNodeInfo()
        : DeviceInfoValid(false), NodeDescInfoValid(false), PowerDescInfoValid(false), EndPointDescInfoValid(false),
          srcAddr(0), srcIEEEAddr(0), capabilities(0),
          LogicalType_ComplexDescriptorAvailable_UserDescriptorAvailable(0), APSFlags_FrequencyBand(0), MacCapabilitiesFlags(0), ManufacturerCode(0),
          MaxBufferSize(0), MaxInTransferSize(0), ServerMask(0), MaxOutTransferSize(0), DescriptorCapabilities(0),
          CurrentPowerMode_AvailablePowerSources(0), CurrentPowerSource_CurrentPowerSourceLevel(0),
          curEndPointInd(0), /*curClusterInd(0),*/ QueryCompleted(false), SupportsDiscoverAttrsExtended(true), SupportsDiscoverAttrsSimple(true),
          SupportsDiscoverCommandsReceived(true), SupportsDiscoverCommandsSent(false),
          requestingStage(RequestingAttrsStage)
    {

    }

    void ResetCurrentEndpointIndexes()
    {
        if (curEndPointInd < EndPointsIds.size())
        {
            const uint8_t ep = EndPointsIds[curEndPointInd];
            if (EndPoints.find(ep) != EndPoints.end())
            {
                EndPoints[ep].curClusterInd = 0;
                if (!EndPoints[ep].AllClusterList.empty())
                {
                    const uint16_t cluster = EndPoints[ep].AllClusterList[0];
                    if (EndPoints[ep].ServersClustersInfoMap.find(cluster) != EndPoints[ep].ServersClustersInfoMap.end())
                        EndPoints[ep].ServersClustersInfoMap[cluster].curAttrInd = 0;
                }
            }
        }
    }

    // TODO: check if a supported cluster has some reportable attribute, in which case bind the cluster no matter what this reports

    // this is only for HA:
    static bool ShouldBeBound(uint16_t cluster)
    {
        // Basic, temp config, power, identify, groups, scenes
        // basic and power should be allowed to bind, since they might send reports (basic some manufacturer specific ones, power for example battery reports)
        if (/*0x0 == cluster || 0x1 == cluster ||*/ 0x2 == cluster || 0x3 == cluster || 0x4 == cluster || 0x5 == cluster ||
                // on off switch configuration, poll control, power profile, meter identification
                0x7 == cluster || 0x20 == cluster || 0x1A == cluster || 0x0B01 == cluster ||
                // ballast configuration, thermostat user interface configuration
                0x0301 == cluster || 0x0204 == cluster ||
                // partition and anything from 'protocol interfaces' (also 'retail tunnel cluster)
                0x0016 == cluster || (cluster & 0xFF00) == 0x0600 ||
                // telecommunications, key estabilishment
                (cluster & 0xFF00) == 0x0900 || 0x0800 == cluster ||
                // commissioning, touchlink
                0x0015 == cluster || 0x1000 == cluster ||
                // also the others from 'retail'
                0x0022 == cluster || 0x0023 == cluster || 0x0024 == cluster ||
                0x0B00 == cluster // appliances identification
           )
            return false;

        return true;
    }

    void SetInfo(const ZigbeeCommands::ZDOEndDeviceAnnceNotification& annce)
    {
        srcAddr = annce.srcAddr;
        srcIEEEAddr = annce.srcIEEEAddr;
        capabilities = annce.capabilities;

        DeviceInfoValid = true;
    }

    void SetInfo(const ZigbeeCommands::ZDONodeDescNotification& nodeDescNotification)
    {
        LogicalType_ComplexDescriptorAvailable_UserDescriptorAvailable = nodeDescNotification.LogicalType_ComplexDescriptorAvailable_UserDescriptorAvailable;
        APSFlags_FrequencyBand = nodeDescNotification.APSFlags_FrequencyBand;
        MacCapabilitiesFlags = nodeDescNotification.MacCapabilitiesFlags;
        ManufacturerCode = nodeDescNotification.ManufacturerCode;
        MaxBufferSize = nodeDescNotification.MaxBufferSize;
        MaxInTransferSize = nodeDescNotification.MaxInTransferSize;
        ServerMask = nodeDescNotification.ServerMask;
        MaxOutTransferSize = nodeDescNotification.MaxOutTransferSize;
        DescriptorCapabilities = nodeDescNotification.DescriptorCapabilities;

        NodeDescInfoValid = true;
    }

    void SetInfo(const ZigbeeCommands::ZDOPowerDescNotification& powerDescNotification)
    {
        CurrentPowerMode_AvailablePowerSources = powerDescNotification.CurrentPowerMode_AvailablePowerSources;
        CurrentPowerSource_CurrentPowerSourceLevel = powerDescNotification.CurrentPowerSource_CurrentPowerSourceLevel;

        PowerDescInfoValid = true;
    }

    void SetInfo(const ZigbeeCommands::ZDOActiveEndPointNotification& endPointNotification)
    {
        EndPointsIds.clear();
        EndPointsIds.reserve(endPointNotification.endPoints.size());

        for (uint8_t ep : endPointNotification.endPoints)
        {
            if (ep > 240) continue; // the ones bigger than 240 are 'reserved'/'special', ignore them
            EndPointsIds.push_back(ep);
            EndPoints[ep] = EndpointInfo(ep);
        }

        EndPointDescInfoValid = true;
    }

    // for each endpoint
    bool SetInfo(ZigbeeCommands::ZDOSimpleDescNotification& simpleDescNotification)
    {
        // ensure to erase OTA cluster from the lists
        simpleDescNotification.InClusterList.erase(std::remove(simpleDescNotification.InClusterList.begin(), simpleDescNotification.InClusterList.end(), 0x0019), simpleDescNotification.InClusterList.end());
        simpleDescNotification.OutClusterList.erase(std::remove(simpleDescNotification.OutClusterList.begin(), simpleDescNotification.OutClusterList.end(), 0x0019), simpleDescNotification.OutClusterList.end());

        // remove all manufacturer specific clusters (at least for now)
        //simpleDescNotification.InClusterList.erase(std::remove_if(simpleDescNotification.InClusterList.begin(), simpleDescNotification.InClusterList.end(), [](uint16_t clusterId) { return ClusterInfo::IsManufacturerSpecific(clusterId); }), simpleDescNotification.InClusterList.end());
        //simpleDescNotification.OutClusterList.erase(std::remove_if(simpleDescNotification.OutClusterList.begin(), simpleDescNotification.OutClusterList.end(), [](uint16_t clusterId) { return ClusterInfo::IsManufacturerSpecific(clusterId); }), simpleDescNotification.OutClusterList.end());

        // don't add those that have no server clusters
        if (!simpleDescNotification.InClusterList.empty())
        {
            bool hasNonManufacturer = false;
            for (uint16_t clusterId : simpleDescNotification.InClusterList)
            {
                if (!EndpointInfo::ClusterInfo::IsManufacturerSpecific(clusterId))
                {
                    hasNonManufacturer = true;
                    break;
                }
            }

            // WARNING: This is a temporary workaround!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            // I found (for Philips Hue) that there is an end point that does not reply to queries, although simple attr discovery is supported
            // that stops the 'pairing'
            // TODO: some other workaround should be done: just do not query such endpoints, skip them but still add them (perhaps if they support known clusters only?)

            if (hasNonManufacturer)
            {
                EndPoints[simpleDescNotification.Endpoint].SetInfo(simpleDescNotification);

                return true;
            }
        }

        EndPoints.erase(simpleDescNotification.Endpoint);

        return false;
    }


    bool IsValid() const
    {
        if (EndPoints.empty())
            return false;

        for (const auto& EndPoint : EndPoints)
            if (!EndPoint.second.IsValid())
                return false;

        return DeviceInfoValid && NodeDescInfoValid && EndPointDescInfoValid;
    }


    uint8_t GetType() const
    {
        return LogicalType_ComplexDescriptorAvailable_UserDescriptorAvailable & 0x3;
    }
    bool IsRouter() const
    {
        return 0x1 == GetType();
    }
    bool IsEndDevice() const
    {
        return 0x2 == GetType();
    }

    bool GetComplexDescriptorAvailable() const
    {
        return LogicalType_ComplexDescriptorAvailable_UserDescriptorAvailable & 0x8;
    }
    bool GetUserDescriptorAvailable() const
    {
        return LogicalType_ComplexDescriptorAvailable_UserDescriptorAvailable & 0x10;
    }

    bool IsPANCoordinatorCapable() const
    {
        return 0x1 & MacCapabilitiesFlags;
    }
    bool IsFullFunctionDevice() const
    {
        return 0x2 & MacCapabilitiesFlags;
    }
    bool IsMainsPowerDevice() const
    {
        return 0x4 & MacCapabilitiesFlags;
    }
    bool IsReceiverWhenIdleDevice() const
    {
        return 0x8 & MacCapabilitiesFlags;
    }
    bool IsSecurityCapable() const
    {
        return 0x40 & MacCapabilitiesFlags;
    }
    bool AllocateAddress() const
    {
        return 0x80 & MacCapabilitiesFlags;
    }

    bool HasBattery() const
    {
        return !IsMainsPowerDevice();
    }

    // TODO: problem: some listening devices actually report as not being listening (example: the Spirit radiator thermostat)
    // that prevents sending commands to them for reading values (in the radiator case, that's awful)
    // what to do about it?
    bool IsWakeupDevice() const
    {
        return !(IsReceiverWhenIdleDevice() || IsMainsPowerDevice() || IsRouter());
    }

    bool IsPrimaryTrustCenter() const
    {
        return 0x1 & ServerMask;
    }
    bool IsBackupTrustCenter() const
    {
        return 0x2 & ServerMask;
    }
    bool IsPrimaryBindingTableCacheCenter() const
    {
        return 0x4 & ServerMask;
    }
    bool IsBackupBindingTableCacheCenter() const
    {
        return 0x8 & ServerMask;
    }
    bool IsPrimaryDiscoveryCache() const
    {
        return 0x10 & ServerMask;
    }
    bool IsBackupDiscoveryCache() const
    {
        return 0x20 & ServerMask;
    }
    bool IsNetworkManager() const
    {
        return 0x40 & ServerMask;
    }

    bool IsExtendedActiveEndpointsListAvailable() const
    {
        return 0x1 & DescriptorCapabilities;
    }
    bool IsSimpleDescriptorListAvailable() const
    {
        return 0x2 & DescriptorCapabilities;
    }

    bool IsAlternatePANCoordinatorCapableAnnounced() const
    {
        return 0x1 & capabilities;
    }
    bool IsRouterCapableAnnounced() const
    {
        return 0x2 & capabilities;
    }
    bool IsMainPoweredCapableAnnounced() const
    {
        return 0x3 & capabilities;
    }
    bool IsSecurityCapableAnnounced() const
    {
        return 0x40 & capabilities;
    }

    std::string GetModelType(uint8_t endPoint) const
    {
        // cook up an unique id to device type. Might not be the best, but it should work
        std::stringstream ss;
        ss << std::hex << ManufacturerCode;

        const auto it = EndPoints.find(endPoint);
        if (it != EndPoints.end())
        {
            ss << it->second.modelIdentifier;
            ss << std::hex << int(endPoint);
        }

        return ss.str();
    }

    std::string GetSerial(uint8_t endPoint) const
    {
        std::stringstream ss;

        ss << "ZG" << std::hex << std::setfill('0') << std::setw(2) << int(endPoint) << std::hex << std::setfill('0') << std::setw(16) << srcIEEEAddr;

        return ss.str();
    }

    static bool ShouldTheClusterBeIgnored(uint16_t clusterId)
    {
        return 0x0019 == clusterId;
    }

    bool DeviceInfoValid;
    bool NodeDescInfoValid;
    bool PowerDescInfoValid;
    bool EndPointDescInfoValid;


    uint16_t srcAddr;
    uint64_t srcIEEEAddr;
    uint8_t capabilities;

    uint8_t LogicalType_ComplexDescriptorAvailable_UserDescriptorAvailable;
    uint8_t APSFlags_FrequencyBand;
    uint8_t MacCapabilitiesFlags;
    uint16_t ManufacturerCode;
    uint8_t MaxBufferSize;
    uint16_t MaxInTransferSize;
    uint16_t ServerMask;
    uint16_t MaxOutTransferSize;
    uint8_t DescriptorCapabilities;

    uint8_t CurrentPowerMode_AvailablePowerSources;
    uint8_t CurrentPowerSource_CurrentPowerSourceLevel;

    std::vector<uint8_t> EndPointsIds;
    std::map<uint8_t, EndpointInfo> EndPoints;

    uint8_t curEndPointInd;
    //uint8_t curClusterInd;

    bool QueryCompleted;

    bool SupportsDiscoverAttrsExtended;
    bool SupportsDiscoverAttrsSimple;
    bool SupportsDiscoverCommandsReceived;
    bool SupportsDiscoverCommandsSent;

    typedef enum
    {
        RequestingAttrsStage = 0,
        RequestingReceivedCommandsStage,
        RequestingSentCommandsStage
    } RequestingStage;

    RequestingStage requestingStage;
};

// TODO:

// if querying the node for attrs was successful, use this info to check the attrs and display any issues (type mismatch, mandatory attr not exposed)
// if the 'ext' querying failed but the simple one did not, use this info to fill in the attributes types
// in both cases check if mandatory ones are set and display info if not

// if querying failed, use this info to set the mandatory attrs
// then launch retrieval of attributes for all attrs (including the optional ones)
// when values are returned, add the supported optional ones to the peer info, also check/set in the type

// I could do this each startup for devices that are listening, but have no idea yet how to deal with those that sleep
// in the worst case scenario devices will end up with only the mandatory attrs and functions set up, not with all supported ones

// TODO:
// 'join' attributes and commands in a similar way as for zwave classes




class ClustersInfo
{
public:
    class AttrInfoExt;
    class Param;

    class Enum
    {
    public:
        Enum() : val(0) {}
        std::string name;
        uint16_t val; // apparently the biggest one is 16 bits

        AttrInfoExt *parentAttr = nullptr;
        Param *parentParam = nullptr;
    };

    class Bitfield
    {
    public:
        Bitfield() : mask(0), shifter(0) {}

        std::string name;
        uint64_t mask;
        uint8_t shifter; // cannot be bigger than 63

        bool IsSingleBit() const
        {
            const std::bitset<64> b(mask);

            return 1 == b.count();
        }

        bool IsSizeField() const
        {
            if (sizeType != 0) // only if it's explicitly set
                return true;

            return false;
        }

        AttrInfoExt *parentAttr = nullptr;
        Param *parentParam = nullptr;
        int sizeType = 0; // 0 unknown, 1 octets, 2 items
    };


    static void printDebug(const std::string& str);
    template<class T> static BaseLib::PVariable GetComplexVariable(T *obj, BaseLib::PVariable sizeVariable, bool octets, BaseLib::PVariable binaryVariable, bool isStruct);


    class Param : public ZigbeeType
    {
    public:
        class AlternRecord
        {
        public:
            BaseLib::PVariable GetComplexVariable(BaseLib::PVariable sizeVariable, bool octets, BaseLib::PVariable binaryVariable, bool isStruct) const;

            bool TypeWithLengthComplexities() const
            {
                for (const auto& param : params)
                    if (param.TypeWithLengthComplexities())
                        return true;

                return false;
            }

            bool FixedRecordLength() const
            {
                for (const auto& param : params)
                    if (!param.HasFixedSize())
                        return false;

                return true;
            }

            int GetFixedSize() const
            {
                if (!FixedRecordLength())
                    return 0;

                int size = 0;

                for (const auto& param : params)
                    size += param.GetFixedSize();

                return size;
            }

            bool IsRecordLengthKnowable() const
            {
                std::string lastName;

                for (const auto& param : params)
                {
                    if (!param.HasKnowableSize() && lastName.empty())
                        return false;

                    if (param.IsSizeField())
                        lastName = param.name;
                    else
                        lastName = "";
                }


                if (params.empty())
                    return false;

                return true;
            }


            uint64_t value = 0;
            uint8_t len = 0;
            uint8_t sizeType = 0; // 0 unknown, 1 octets, 2 items

            std::vector<Param> params;
        };


        std::string name;

        void Parse(rapidxml::xml_node *node, Param* prevSizeParam, Bitfield* prevSizeBitfield);

        bool HasStructure() const
        {
            return !bitfields.empty() || !params.empty();
        }

        bool FixedRecordLength() const
        {
            if (HasFixedSize())
                return true;

            for (const auto& param : params)
                if (!param.HasFixedSize())
                    return false;

            return true;
        }

        int GetFixedSize() const
        {
            if (!FixedRecordLength())
                return 0;

            int size = GetSize();
            if (size > 0)
                return 0;

            for (const auto& param : params)
                size += param.GetFixedSize();

            return size;
        }

        bool IsRecordLengthKnowable() const
        {
            if (FixedRecordLength())
                return true;
            else if (HasKnowableSize())
                return true;

            std::string lastName;

            for (const auto& param : params)
            {
                if (!param.HasKnowableSize() && lastName.empty())
                    return false;

                if (param.IsSizeField())
                    lastName = param.name;
                else
                    lastName = "";
            }

            if (params.empty())
                return false;

            return true;
        }

        bool IsSizeField() const
        {
            if (FixedRecordLength() && name.size() >= 4 &&
                (name.substr(0, 6) == "LENGTH" || name.substr(0, 4) == "SIZE" || name.substr(0, 5) == "COUNT" ||
                 (name.size() >= 7 && name.substr(name.size() - 7, name.size()) == "_LENGTH") ||
                 (name.size() >= 5 && name.substr(name.size() - 5, name.size()) == "_SIZE") ||
                 (name.size() >= 6 && name.substr(name.size() - 6, name.size()) == "_COUNT") ||
                 sizeType != 0)
                && params.empty() && bitfields.empty()) // if it has subparams, the size field is actually a subparam, must handle that as well
                return true;

            return false;
        }

        bool IsExplicitSizeField() const
        {
            return sizeType != 0;
        }

        BaseLib::PVariable GetComplexVariable(BaseLib::PVariable sizeVariable, bool octets, BaseLib::PVariable binaryVariable, bool isStruct) const;

        uint32_t GetIfFieldValue() const
        {
            if (ifFieldValue.empty()) return 0;

            const std::string starts = ifFieldValue.substr(0, 2);
            if (starts == "0x" || starts == "0X")
                return std::stoi(ifFieldValue, 0, 16);

            return std::stoi(ifFieldValue);
        }

        uint8_t len = 0;
        uint8_t sizeType = 0; // 0 unknown, 1 octets, 2 items

        std::vector<Enum> enums;
        std::vector<Bitfield> bitfields;
        std::vector<Param> params;

        std::string alternField;
        std::map<uint64_t, AlternRecord> alternRecords;
        bool mightMiss = false;

        uint8_t ifNotExistsAttr = 0xFF;
        std::string ifField;
        std::string ifFieldValue;
        std::string ifNotField;

        AttrInfoExt *parentAttr = nullptr;
        Param *parentParam = nullptr;
        Param *sizeParam = nullptr;
        Bitfield *sizeBitfield = nullptr;
    };



    class AttrInfoExt : public ZigbeeNodeInfo::EndpointInfo::ClusterInfo::AttrInfo
    {
    public:
        AttrInfoExt() : AttrInfo(), mandatory(false), mandatory_if_equal(false), scene(false), len(0), sizeType(0) {}

        bool HasStructure() const
        {
            return !bitfields.empty() || !params.empty();
        }

        bool FixedRecordLength() const
        {
            if (HasFixedSize())
                return true;

            for (const auto& param : params)
                if (!param.HasFixedSize())
                    return false;

            return true;
        }

        int GetFixedSize() const
        {
            if (!FixedRecordLength())
                return 0;

            int size = GetSize();
            if (size > 0)
                return 0;

            for (const auto& param : params)
                size += param.GetFixedSize();

            return size;
        }

        bool IsRecordLengthKnowable() const
        {
            if (FixedRecordLength())
                return true;
            else if (HasKnowableSize())
                return true;

            std::string lastName;

            for (const auto& param : params)
            {
                if (!param.HasKnowableSize() && lastName.empty())
                    return false;

                if (param.IsSizeField())
                    lastName = param.name;
                else
                    lastName = "";
            }


            if (params.empty())
                return false;

            return true;
        }

        BaseLib::PVariable GetComplexVariable(BaseLib::PVariable sizeVariable, bool octets, BaseLib::PVariable binaryVariable, bool isStruct) const;

        std::string name;
        bool mandatory;

        std::string mandatoryAttribute;
        std::string mandatoryBits;
        // if true, only if mandatory bit is set in the mandatory attribute, if false, mandatory_bit is actually a value, the value of mandatory attr must be bigger than the mandatoryBit value in order to have the attribute mandatory (otherwise it is optional)
        bool mandatory_if_equal;

        bool scene;
        uint8_t len;
        uint8_t sizeType; // 0 unknown, 1 octets, 2 items

        std::vector<Enum> enums;
        std::vector<Bitfield> bitfields;
        std::vector<Param> params;

        void Parse(rapidxml::xml_node *node);
    };


    class ClusterInfoExt : public ZigbeeNodeInfo::EndpointInfo::ClusterInfo
    {
    public:

        class Command
        {
        public:
            bool received = true;

            uint8_t key;
            std::string name;

            bool mandatory = false;
            std::string mandatoryAttribute;
            std::string mandatoryBits;

            // if true, only if mandatory bit is set in the mandatory attribute, if false, mandatory_bit is actually a value, the value of mandatory attr must be bigger than the mandatoryBit value in order to have the attribute mandatory (otherwise it is optional)
            bool mandatory_if_equal = false;

            std::vector<Param> params;

            void Parse(rapidxml::xml_node *node);
            bool IsNotification() const
            {
                return params.empty();
            }

            bool CanComputeSize() const
            {
                for (const Param& param : params)
                    if (0 == param.GetSize())
                        return false;

                return true;
            }

            unsigned int GetSize() const
            {
                unsigned int size = 0;

                for (const Param& param : params)
                {
                    const int fsize = param.GetSize();
                    if (0 == fsize) return 0;
                    size += fsize;
                }

                return size;
            }

            bool MightMissFields() const
            {
                for (const Param& param : params)
                    if (param.mightMiss) return true;

                return false;
            }
        };

        void Parse(rapidxml::xml_node *node);

        std::string name;
        std::map<uint16_t, AttrInfoExt> attrsMap;

        std::map<uint8_t, Command> commandsReceived;
        std::map<uint8_t, Command> commandsSent;
    };

    void Parse(char *str);

    static std::string GetAttrValue(rapidxml::xml_node *node, const std::string& Name)
    {
        for (rapidxml::xml_attribute *attr = node->first_attribute(); attr; attr = attr->next_attribute())
        {
            const std::string attrname = attr->name();

            if (attrname == Name)
                return attr->value();
        }

        return "";
    }



    static void TransformNameToHomegear(std::string& name)
    {
        std::transform(name.begin(), name.end(), name.begin(), ::toupper);
        std::replace(name.begin(), name.end(),' ', '_');
    }

    void CheckAndAddMandatory(ZigbeeNodeInfo* info, BaseLib::Output& out);
    void CheckAndAddMandatory(ZigbeeNodeInfo::EndpointInfo::ClusterInfo* info, BaseLib::Output& out);

    std::map<uint16_t, ClusterInfoExt> serversClustersInfoMap;
    std::map<uint16_t, ClusterInfoExt> clientsClustersInfoMap;
};




template<class T> BaseLib::PVariable ClustersInfo::GetComplexVariable(T *obj, BaseLib::PVariable sizeVariable, bool octets, BaseLib::PVariable binaryVariable, bool isStruct)
{
    if (!obj || !obj->TypeWithLengthComplexities()) return binaryVariable;

    if (obj->params.empty())
    {
        if (sizeVariable)
        {
            if (sizeVariable->type == BaseLib::VariableType::tInteger)
            {
                if (octets)
                {
                    if (binaryVariable->type != BaseLib::VariableType::tBinary)
                        ClustersInfo::printDebug("Debug: Non binary variable passed to be parsed");
                    else
                    {
                        if (sizeVariable->integerValue >= static_cast<int32_t>(binaryVariable->binaryValue.size()))
                        {
                            std::vector<uint8_t> buf(binaryVariable->binaryValue.begin(), binaryVariable->binaryValue.begin() + sizeVariable->integerValue);

                            return std::make_shared<BaseLib::Variable>(buf);
                        }
                        else
                            ClustersInfo::printDebug("Debug: Non binary variable passed to be parsed");
                    }
                }
                else
                    ClustersInfo::printDebug("Debug: Size variable passed for a parameter that has no structure defined but size marked as number of items, not octets");
            }
            else
            {
                ClustersInfo::printDebug("Debug: Size variable passed with a non integer type");
            }
        }

        return binaryVariable; // cannot do anything about it
    }

    if (binaryVariable->type != BaseLib::VariableType::tBinary)
    {
        ClustersInfo::printDebug("Debug: Non binary variable passed to be parsed");

        return binaryVariable;
    }

    bool dontKnowSize = false;

    if (sizeVariable)
    {
        if (sizeVariable->type == BaseLib::VariableType::tInteger)
        {
            // two cases, 'octets' or 'items'
            if (octets)
            {
                // easy, just split up the vector into equal pieces that are parsed

                // first, make an array
                BaseLib::PArray parray;
                if (!isStruct) parray = std::make_shared<BaseLib::Array>();

                for (unsigned int offset = 0; offset < binaryVariable->binaryValue.size(); offset += sizeVariable->integerValue)
                {
                    unsigned int endPos = offset + sizeVariable->integerValue;
                    if (endPos <= binaryVariable->binaryValue.size())
                    {
                        BaseLib::PStruct structVar = std::make_shared<BaseLib::Struct>();

                        std::vector<uint8_t> recordBuffer(binaryVariable->binaryValue.begin() + offset, binaryVariable->binaryValue.begin() + endPos);

                        BaseLib::PVariable lengthValue;
                        bool octetsValue = true;
                        int recordOffset = 0;
                        for (const auto& param : obj->params)
                        {
                            BaseLib::PVariable paramValue = param.GetVariable(recordBuffer, recordOffset);

                            if (param.GetSize())
                                recordOffset += param.GetSize();
                            else if (param.LengthInFirstOctet())
                                recordOffset += recordBuffer[recordOffset] + 1;
                            else if (param.LengthInFirstTwoOctets())
                            {
                                const unsigned int len = ZigbeeCommands::MTCmd::ReadUint16(recordOffset, recordBuffer);
                                recordOffset +=  (0x44 == param.type ? len * 2 : len) + 2;
                            }
                            else
                                paramValue = param.GetComplexVariable(lengthValue, octetsValue, paramValue, param.IsStruct()); // recursively parse it

                            if (param.IsSizeField())
                            {
                                lengthValue = paramValue;
                                octetsValue = (2 == param.sizeType ? false : true);
                            }

                            structVar->insert(std::make_pair(param.name, paramValue));
                        }

                        if (isStruct) return std::make_shared<BaseLib::Variable>(structVar);
                        else parray->push_back(std::make_shared<BaseLib::Variable>(structVar));
                    }

                }

                return std::make_shared<BaseLib::Variable>(parray);
            }
            else
                dontKnowSize = true;
        }
        else
        {
            ClustersInfo::printDebug("Debug: Size variable passed with a non integer type");

            return binaryVariable;
        }
    }
    else
        dontKnowSize = true;


    if (dontKnowSize)
    {
        // no size variable, must have a 'knowable' size
        if (!obj->IsRecordLengthKnowable())
        {
            ClustersInfo::printDebug(std::string("Debug: Record size unknowable") + ((sizeVariable && sizeVariable->type == BaseLib::VariableType::tInteger) ? " although a number of items valid variable was passed" : ""));

            return binaryVariable;
        }

        BaseLib::PArray parray;
        if (!isStruct) parray = std::make_shared<BaseLib::Array>();

        BaseLib::PVariable lengthValue;
        bool octetsValue = true;

        const unsigned int structLen = (isStruct ? ZigbeeCommands::MTCmd::ReadUint16(0, binaryVariable->binaryValue) : 0);

        for (unsigned int offset = (isStruct ? 2 : 0); offset < binaryVariable->binaryValue.size(); )
        {
            BaseLib::PStruct structVar = std::make_shared<BaseLib::Struct>();

            for (const auto& param : obj->params)
            {
                BaseLib::PVariable paramValue = param.GetVariable(binaryVariable->binaryValue, offset);

                if (param.GetSize())
                    offset += param.GetSize();
                else if (param.LengthInFirstOctet())
                    offset += binaryVariable->binaryValue.at(offset) + 1;
                else if (param.LengthInFirstTwoOctets())
               {
                    const unsigned int len = ZigbeeCommands::MTCmd::ReadUint16(offset, binaryVariable->binaryValue);
                    offset += (0x44 == param.type ? len * 2 : len) + 2;
                }
                else
                {
                    paramValue = param.GetComplexVariable(lengthValue, octetsValue, paramValue, param.IsStruct()); // recursively parse it

                    if (paramValue && paramValue->type == BaseLib::VariableType::tBinary)
                        offset += paramValue->binaryValue.size();
                    else if (lengthValue && lengthValue->type == BaseLib::VariableType::tInteger && octetsValue)
                        offset += lengthValue->integerValue;
                    else
                    {
                        ClustersInfo::printDebug("Debug: Parameter with variable size, no valid length and also not parsed as binary");
                        break;
                    }
                }

                if (param.IsSizeField())
                {
                    lengthValue = paramValue;
                    octetsValue = (2 == param.sizeType ? false : true);
                }

                structVar->insert(std::make_pair(param.name, paramValue));
            }

            octetsValue = true;
            lengthValue.reset();

            if (isStruct)
            {
                if (structVar->size() != structLen)
                    ClustersInfo::printDebug("Debug: Parsed structure having wrong values count");

                return std::make_shared<BaseLib::Variable>(structVar);
            }
            else parray->push_back(std::make_shared<BaseLib::Variable>(structVar));
        }

        return std::make_shared<BaseLib::Variable>(parray);
    }

    return binaryVariable;
}


}

#endif // NODEINFO_H_

