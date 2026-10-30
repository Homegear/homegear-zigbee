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

#include "NodeInfo.h"

#include "../ZigbeeCentral.h"
#include "../Zigbee.h"
#include "../GD.h"

#include "../ZigbeeDevicesDescription.h"

namespace Zigbee
{

// TODO: refine it for enums, bitmasks and if possible for structs, arrays and so on

BaseLib::PVariable ZigbeeType::GetVariable(const std::vector<uint8_t>& buffer, int offset) const
{
    std::vector<uint8_t> data = GetRawData(buffer, offset, false);
    const unsigned int expectedSize = GetSize();

    if (expectedSize > 0 && data.size() < expectedSize)
        return std::make_shared<BaseLib::Variable>(0);

    switch(type)
    {
    case 0x08: // 8-bit data
    case 0x18: // 8-bit bitmap
    case 0x20: // uint 8bit
    case 0x30: // enum8
    {
        const uint8_t val = data[0];
        return std::make_shared<BaseLib::Variable>(val);
    }
    case 0x10: // bool
    {
        const uint8_t val = data[0];
        if (val == 0xff)
            return std::make_shared<BaseLib::Variable>(0); // invalid value
        else if (val == 0x00)
            return std::make_shared<BaseLib::Variable>(false);

        return std::make_shared<BaseLib::Variable>(true);
    }
    case 0x28: // int8
    {
        const int8_t val = static_cast<int8_t>(data[0]);
        return std::make_shared<BaseLib::Variable>(val);
    }
    case 0x09: // 16-bit data
    case 0x19: // 16-bit bitmap
    case 0x21: // uint 16bit
    case 0x31: // enum16
    case 0xe8: // cluster id
    case 0xe9: // attr id
    {
        uint16_t val = ZigbeeCommands::MTCmd::ReadUint16(0, data);
        return std::make_shared<BaseLib::Variable>(val);
    }
    case 0x29: // int16
    {
        int16_t val = static_cast<int16_t>(ZigbeeCommands::MTCmd::ReadUint16(0, data));
        return std::make_shared<BaseLib::Variable>(val);
    }
    case 0x0a: // 24-bit data
    case 0x1a: // 24-bit bitmap
    case 0x22: // uint24
    {
        uint32_t val =  ZigbeeCommands::MTCmd::ReadUint24(0, data);
        return std::make_shared<BaseLib::Variable>(val);
    }
    case 0x2a: // int24
    {
        int32_t val =  ZigbeeCommands::MTCmd::ReadInt24(0, data);
        return std::make_shared<BaseLib::Variable>(val);
    }

    case 0x0b: // 32-bit data
    case 0x1b: // 32-bit bitmap
    case 0x23: // uint32
    {
        uint32_t val =  ZigbeeCommands::MTCmd::ReadUint32(0, data);
        return std::make_shared<BaseLib::Variable>(val);
    }
    case 0x2b: // int32
    {
        int32_t val = static_cast<int32_t>(ZigbeeCommands::MTCmd::ReadUint32(0, data));
        return std::make_shared<BaseLib::Variable>(val);
    }


    case 0xe0: // time of day
    case 0xe1: // date
    {
        // not clear what the order of bytes is, for now get them as they come
        uint32_t val = data[0]; // hours or year
        val <<= 8;
        val |= data[1]; // minutes or month
        val <<= 8;
        val |= data[2]; // seconds or day of the month
        val <<= 8;
        val |= data[3]; // Hundredths or day of the week
        return std::make_shared<BaseLib::Variable>(val);
    }

    case 0xe2: // utc time
    {
        // it's just a number of seconds
        uint32_t val =  ZigbeeCommands::MTCmd::ReadUint32(0, data);
        return std::make_shared<BaseLib::Variable>(val);
    }

    case 0xea: // bac oid
    {
        uint32_t val =  ZigbeeCommands::MTCmd::ReadUint32(0, data);
        return std::make_shared<BaseLib::Variable>(val);
    }

    case 0x0c: // 40-bit data
    case 0x1c: // 40-bit bitmap
    case 0x24: // uint40
    {
        uint64_t val =  ZigbeeCommands::MTCmd::ReadUint40(0, data);
        return std::make_shared<BaseLib::Variable>(val);
    }

    case 0x2c: // int40
    {
        int64_t val =  ZigbeeCommands::MTCmd::ReadInt40(0, data);
        return std::make_shared<BaseLib::Variable>(val);
    }
    case 0x0d: // 48-bit data
    case 0x1d: // 48-bit bitmap
    case 0x25: // uint48
    {
        uint64_t val =  ZigbeeCommands::MTCmd::ReadUint48(0, data);
        return std::make_shared<BaseLib::Variable>(val);
    }

    case 0x2d: // int48
    {
        int64_t val =  ZigbeeCommands::MTCmd::ReadInt48(0, data);
        return std::make_shared<BaseLib::Variable>(val);
    }

    case 0x0e: // 56-bit data
    case 0x1e: // 56-bit bitmap
    case 0x26: // uint56
    {
        uint64_t val =  ZigbeeCommands::MTCmd::ReadUint56(0, data);
        return std::make_shared<BaseLib::Variable>(val);
    }

    case 0x2e: // int56
    {
        int64_t val =  ZigbeeCommands::MTCmd::ReadInt56(0, data);
        return std::make_shared<BaseLib::Variable>(val);
    }

    case 0x0f: // 64-bit data
    case 0x1f: // 64-bit bitmap
    case 0x27: // uint64
    {
        uint64_t val =  ZigbeeCommands::MTCmd::ReadUint64(0, data);
        return std::make_shared<BaseLib::Variable>(val);
    }
    case 0x2f: // int64
    {
        int64_t val =  static_cast<int64_t>(ZigbeeCommands::MTCmd::ReadUint64(0, data));
        return std::make_shared<BaseLib::Variable>(val);
    }
    case 0x38: // semi - float (16 bits)
        // TODO: implement it
        return std::make_shared<BaseLib::Variable>(0); // invalid value
    case 0x39: // single float (32 bits)
    {
        // TODO: must be checked
        // the problem is that it's not clear what the order of bits is
        const uint32_t val = ZigbeeCommands::MTCmd::ReadUint32(0, data);
        float fval;
        memcpy(&fval, &val, 4);

        return std::make_shared<BaseLib::Variable>(fval);
    }
    case 0x3a: // double (64 bit)
    {
        // TODO: must be checked
        // the problem is that it's not clear what the order of bits is

        const uint64_t val =  ZigbeeCommands::MTCmd::ReadUint64(0, data);
        double dval;
        memcpy(&dval, &val, 8);

        return std::make_shared<BaseLib::Variable>(dval);
    }
    case 0xf0: // IEEE address (8 bytes)
    case 0xf1: // key128 (16 bytes)
        return std::make_shared<BaseLib::Variable>(data); // TODO: check to see the order of bytes!

    }

    if (LengthInFirstOctet() || LengthInFirstTwoOctets())
    {
        if (IsCharString())
        {
            const std::string valStr(data.begin(), data.end());
            std::make_shared<BaseLib::Variable>(valStr);
        }
        else
            return std::make_shared<BaseLib::Variable>(data);
    }


    // 'Problem' types:

    //struct 0x4C - this has structure, there is no such type in the xml clusters yet, should be handled just in case
    // but in GetComplexVariable!!!!

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

    // instead of handling as below, should be handled more properly
    // the handling below is all right, though, because it's either an attribute or (at least for the cases in the clusters xml file) the last param of the command (or in the case of sub parameters in records, the last one in the record)


    // no length, just return up to the end
    data = std::vector<uint8_t>(buffer.begin() + offset, buffer.end());

    return std::make_shared<BaseLib::Variable>(data);
}


bool ZigbeeType::SetVariable(BaseLib::PVariable variable, std::vector<uint8_t>& buffer, bool setLengthPrefix) const
{
    if (!variable || 0 == type || 0xFF == type)
    {
        buffer.resize(0);
        return true;
    }

    switch(variable->type)
    {
    case VariableType::tVoid:
        buffer.resize(0);
        return true;
    case VariableType::tInteger64:
    {
        int64_t val = variable->integerValue64;
        const int size = GetSize();
        buffer.resize(size, 0);

        if (IsSignedInt() || IsUnsignedInt() || IsTime() || IsEnum() || IsBitmap() || IsGeneralData() || 0xf0 == type) // the last type is IEEE address
        {
            // big endian ordering
            for (int offset = size - 1; offset >= 0; --offset)
            {
                buffer[offset] = val & 0xFF;
                val >>= 8;
            }

            return true;
        }
        else if (IsFloatingPoint())
        {
            // TODO: check the needed order for bytes in the data for floating point values
            if (0x39 == type) // float
            {
                const float fval = val;
                uint32_t v;
                memcpy(&v, &fval, 4);

                ZigbeeCommands::MTCmd::WriteUint32(0, v, buffer);
            }
            else if (0x3a == type) // double
            {
                const double dval = val;
                uint64_t v;
                memcpy(&v, &dval, 8);

                ZigbeeCommands::MTCmd::WriteUint64(0, v, buffer);
            }
            else
            {
                GD::out.printDebug("Semi float is not implemented");
                return false;
            }

            return true;
        }
        else if (IsBool())
        {
            buffer[size - 1] = (val ? 0x1 : 0x0);

            return true;
        }
    }
    break;
    case VariableType::tInteger:
    {
        int32_t val = variable->integerValue;
        int size = GetSize();

        buffer.resize(size, 0);

        if (IsSignedInt() || IsUnsignedInt() || IsTime() || IsEnum() || IsBitmap() || IsGeneralData() || 0xf0 == type) // the last type is IEEE address
        {
            // big endian ordering
            size = std::min(size, 4);
            for (int offset = size - 1; offset >= 0; --offset)
            {
                buffer[offset] = val & 0xFF;
                val >>= 8;
            }

            return true;
        }
        else if (IsFloatingPoint())
        {
            // TODO: check the needed order for bytes in the data for floating point values
            if (0x39 == type) // float
            {
                const float fval = val;
                uint32_t v;
                memcpy(&v, &fval, 4);

                ZigbeeCommands::MTCmd::WriteUint32(0, v, buffer);
            }
            else if (0x3a == type) // double
            {
                const double dval = val;
                uint64_t v;
                memcpy(&v, &dval, 8);

                ZigbeeCommands::MTCmd::WriteUint64(0, v, buffer);
            }
            else
            {
                GD::out.printDebug("Semi float is not implemented");
                return false;
            }

            return true;
        }
        else if (IsBool())
        {
            buffer[size - 1] = (val ? 0x1 : 0x0);

            return true;
        }
    }
    break;
    case VariableType::tBoolean:
    {
        const int size = GetSize();
        if (0 == size)
            return false;
        buffer.resize(size, 0);

        buffer[size - 1] = variable->booleanValue ? 0x1 : 0x0;

        return true;
    }
    break;
    case VariableType::tFloat:
    {
        const double val = variable->floatValue;
        const int size = GetSize();

        buffer.resize(size, 0);

        if (IsSignedInt() || IsUnsignedInt() || IsTime() || IsEnum() || IsBitmap() || IsGeneralData() || 0xf0 == type) // the last type is IEEE address
        {
            int64_t dval = val;
            // big endian ordering
            for (int offset = size - 1; offset >= 0; --offset)
            {
                buffer[offset] = dval & 0xFF;
                dval >>= 8;
            }

            return true;
        }
        else if (IsFloatingPoint())
        {
            // TODO: check the needed order for bytes in the data for floating point values
            if (0x39 == type) // float
            {
                const float fval = val;
                uint32_t v;
                memcpy(&v, &fval, 4);

                ZigbeeCommands::MTCmd::WriteUint32(0, v, buffer);
            }
            else if (0x3a == type) // double
            {
                uint64_t v;
                memcpy(&v, &val, 8);

                ZigbeeCommands::MTCmd::WriteUint64(0, v, buffer);
            }
            else
            {
                GD::out.printDebug("Semi float is not implemented");
                return false;
            }

            return true;
        }
        else if (IsBool())
        {
            buffer[size - 1] = ((val != 0.0) ? 0x1 : 0x0);

            return true;
        }
    }
    break;
    case VariableType::tBinary:
        if (HasVariableLength() || GetSize() == static_cast<int>(variable->binaryValue.size()))
        {
            if (setLengthPrefix)
            {
                if (GetSize() > 0)
                {
                    int setSize = GetSize();
                    // TODO: should I revert it here to convert from bigger endian to little?
                    // for now just do a dumb copy
                    buffer.resize(setSize, 0);
                    std::copy(variable->binaryValue.begin(), variable->binaryValue.begin() + std::min(buffer.size(), variable->binaryValue.size()), buffer.begin());
                }
                else if (LengthInFirstOctet())
                {
                    buffer.resize(1);
                    buffer[0] = variable->binaryValue.size();
                    buffer.insert(buffer.end(), variable->stringValue.begin(), variable->stringValue.end());
                }
                else if (LengthInFirstTwoOctets())
                {
                    buffer.resize(2);
                    if (0x44 == type) ZigbeeCommands::MTCmd::WriteUint16(0, variable->binaryValue.size() / 2, buffer);
                    else ZigbeeCommands::MTCmd::WriteUint16(0, variable->binaryValue.size(), buffer);
                    buffer.insert(buffer.end(), variable->stringValue.begin(), variable->stringValue.end());
                }
                else
                {
                    buffer = variable->binaryValue;
                }
            }
            else
            {
                buffer = variable->binaryValue;
            }

            return true;
        }
        break;
    case VariableType::tString:
        if (HasVariableLength())
        {
            if (setLengthPrefix)
            {
                if (LengthInFirstOctet())
                {
                    buffer.resize(1);
                    buffer[0] = variable->stringValue.size();
                }
                else if (LengthInFirstTwoOctets())
                {
                    buffer.resize(2);
                    if (0x44 == type) ZigbeeCommands::MTCmd::WriteUint16(0, variable->stringValue.size() / 2, buffer);
                    else ZigbeeCommands::MTCmd::WriteUint16(0, variable->stringValue.size(), buffer);
                }
                else
                {
                    buffer.resize(0);
                }
            }
            else
            {
                buffer.resize(0);
            }
            buffer.insert(buffer.end(), variable->stringValue.begin(), variable->stringValue.end());

            return true;
        }
        break;

    case VariableType::tArray:
        // TODO: actually this should be done somewhere else (also for struct)
        // where there is access to params to be checked against passed values in the array

        /*
        if (HasVariableLength())
        {
            buffer.clear();

            for (const auto val : *(variable->arrayValue))
            {
                if (BaseLib::VariableType::tString == val->type)
                    buffer.insert(buffer.end(), val->stringValue.begin(), val->stringValue.end());
                else if (BaseLib::VariableType::tInteger == val->type)
                    buffer.push_back(uint8_t(val->integerValue));
                else if (BaseLib::VariableType::tInteger64 == val->type)
                    buffer.push_back(uint8_t(val->integerValue64));
                else if (BaseLib::VariableType::tBoolean == val->type)
                    buffer.push_back(val->booleanValue ? 1 : 0);
                else if (BaseLib::VariableType::tBinary == val->type)
                    buffer.insert(buffer.end(), val->binaryValue.begin(), val->binaryValue.end());
            }

            return true;
        }
        */
        break;

    // TODO: implement those cases as well
    case VariableType::tStruct:
        break;
    case VariableType::tVariant:
        GD::out.printDebug("Variant conversion is not implemented");
        break;
    case VariableType::tBase64:
        GD::out.printDebug("Base64 conversion is not implemented");
        break;
    }

    return false;
}


void ClustersInfo::Parse(char *str)
{
    rapidxml::xml_document doc;    // character type defaults to char

    doc.parse<0>(str);    // 0 means default parse flags

    for (rapidxml::xml_node *rnode = doc.first_node(); rnode; rnode = rnode->next_sibling())
    {
        if (rnode->type() != rapidxml::node_element)
            continue;

        const std::string name = rnode->name();
        if (std::string("zigbee_clusters") != name)
            continue;

        for (rapidxml::xml_node *node = rnode->first_node(); node; node = node->next_sibling())
        {
            if (node->type() != rapidxml::node_element)
                continue;

            const std::string nodename = node->name();
            if (nodename == "cluster")
            {
                ClustersInfo::ClusterInfoExt clusterInfo;
                clusterInfo.Parse(node);

                if (clusterInfo.server)
                    serversClustersInfoMap[clusterInfo.clusterId] = clusterInfo;
                else
                    clientsClustersInfoMap[clusterInfo.clusterId] = clusterInfo;
            }
        }
    }
}

void ClustersInfo::CheckAndAddMandatory(ZigbeeNodeInfo* info, BaseLib::Output& out)
{
    if (!info)
        return;

    for (std::map<uint8_t, ZigbeeNodeInfo::EndpointInfo>::iterator endPointIt = info->EndPoints.begin(); endPointIt != info->EndPoints.end(); ++endPointIt)
    {
        for (std::map<uint16_t, ZigbeeNodeInfo::EndpointInfo::ClusterInfo>::iterator clusterIt = endPointIt->second.ServersClustersInfoMap.begin(); clusterIt != endPointIt->second.ServersClustersInfoMap.end(); ++clusterIt)
        {
            CheckAndAddMandatory(&clusterIt->second, out);
        }
    }
}

void ClustersInfo::CheckAndAddMandatory(ZigbeeNodeInfo::EndpointInfo::ClusterInfo* info, BaseLib::Output& out)
{
    if (!info)
        return;

    if (ZigbeeNodeInfo::EndpointInfo::ClusterInfo::IsManufacturerSpecific(info->clusterId))
        return; // cannot adjust those


    if (serversClustersInfoMap.find(info->clusterId) == serversClustersInfoMap.end())
    {
        out.printInfo("The cluster 0x" + BaseLib::HelperFunctions::getHexString(info->clusterId) + " does not exist in the xml clusters definitions file");
        return;
    }

    const ClusterInfoExt& clusterInfoExt = serversClustersInfoMap[info->clusterId];

    for (const auto& attrPair : clusterInfoExt.attrsMap)
    {
        // first, ensure that mandatory attrs exist
        if (attrPair.second.mandatory && info->attrsMap.find(attrPair.second.attrId) == info->attrsMap.end())
        {
            out.printInfo("The mandatory attribute 0x" + BaseLib::HelperFunctions::getHexString(attrPair.second.attrId) + " for cluster 0x" + BaseLib::HelperFunctions::getHexString(info->clusterId) + " does not exist, adding it...");
            info->attrsMap.insert(attrPair);
            info->attrsList.push_back(attrPair.second.attrId);
        }

        // second, set the type if not already set, from the xml file, otherwise warn if types do not match
        if (info->attrsMap.find(attrPair.second.attrId) != info->attrsMap.end())
        {
            if (info->attrsMap[attrPair.second.attrId].type != attrPair.second.type)
            {
                if (info->attrsMap[attrPair.second.attrId].IsUnknown())
                {
                    out.printInfo("The attribute 0x" + BaseLib::HelperFunctions::getHexString(attrPair.second.attrId) + " for cluster 0x" + BaseLib::HelperFunctions::getHexString(info->clusterId) +  " has unknown type, setting it...");
                    info->attrsMap[attrPair.second.attrId].type = attrPair.second.type;
                    info->attrsMap[attrPair.second.attrId].accessControl = attrPair.second.accessControl;
                }
                else
                {
                    out.printDebug("The attribute 0x" + BaseLib::HelperFunctions::getHexString(attrPair.second.attrId) + " for cluster 0x" + BaseLib::HelperFunctions::getHexString(info->clusterId) + " has different reported type than the one in the xml clusters definitions file");
                    out.printDebug("Reported type: 0x" + BaseLib::HelperFunctions::getHexString(info->attrsMap[attrPair.second.attrId].type) + " in xml clusters definitions file: 0x" + BaseLib::HelperFunctions::getHexString(attrPair.second.type) + " for attr: " + attrPair.second.name);
                }
            }
            else
            {
                if (info->attrsMap[attrPair.second.attrId].accessControl != attrPair.second.accessControl)
                {
                    if (info->attrsMap[attrPair.second.attrId].accessControl != 0)
                        out.printDebug("The attribute 0x" + BaseLib::HelperFunctions::getHexString(attrPair.second.attrId) + " for cluster 0x" + BaseLib::HelperFunctions::getHexString(info->clusterId) + " has different reported access control than the one in the xml clusters definitions file");
                    info->attrsMap[attrPair.second.attrId].accessControl = attrPair.second.accessControl;
                }
            }
        }
    }

    // do the same for mandatory commands

    for (const auto& cmdPair : clusterInfoExt.commandsReceived)
    {
        if (cmdPair.second.mandatory)
            info->commandsReceived.insert(cmdPair.second.key);
    }

    for (const auto& cmdPair : clusterInfoExt.commandsSent)
    {
        if (cmdPair.second.mandatory)
            info->commandsSent.insert(cmdPair.second.key);
    }
}




void ClustersInfo::ClusterInfoExt::Parse(rapidxml::xml_node *node)
{
    for (rapidxml::xml_attribute *attr = node->first_attribute(); attr; attr = attr->next_attribute())
    {
        const std::string attrname = attr->name();
        const std::string value = attr->value();

        if (attrname == "key")
        {
            clusterId = (uint16_t)std::stoi(value, 0, 16);
        }
        else if (attrname == "name")
        {
            name = value;
        }
        else if (attrname == "server")
        {
            if (value == "yes" || value == "true")
                server = true;
            else
                server = false;
        }
    }

    for (rapidxml::xml_node *subnode = node->first_node(); subnode; subnode = subnode->next_sibling())
    {
        if (subnode->type() != rapidxml::node_element)
            continue;

        const std::string sname = subnode->name();
        if (sname == "attr")
        {
            ClustersInfo::AttrInfoExt attrInfo;
            attrInfo.Parse(subnode);

            attrsList.emplace_back(attrInfo.attrId);
            attrsMap[attrInfo.attrId] = attrInfo;
        }
        else if (sname == "cmd_received")
        {
            ClustersInfo::ClusterInfoExt::Command cmd;
            cmd.received = true;
            cmd.Parse(subnode);

            commandsReceived[cmd.key] = cmd;
        }
        else if (sname == "cmd_generated")
        {
            ClustersInfo::ClusterInfoExt::Command cmd;
            cmd.received = false;
            cmd.Parse(subnode);

            commandsSent[cmd.key] = cmd;
        }
    }
}

void ClustersInfo::AttrInfoExt::Parse(rapidxml::xml_node *node)
{
    for (rapidxml::xml_attribute *attr = node->first_attribute(); attr; attr = attr->next_attribute())
    {
        const std::string attrname = attr->name();
        const std::string value = attr->value();

        if (attrname == "key")
        {
            attrId = (uint16_t)std::stoi(value, 0, 16);
        }
        else if (attrname == "name")
        {
            name = value;
        }
        else if (attrname == "type")
        {
            type = GetType(value);
            if (0xFF == type)
                GD::out.printDebug("Parsed type is unknown for attr: " + name);
        }
        else if (attrname == "access") // r, w, t (read, write, reportable)
        {
            if (value.find('r') != std::string::npos)
                accessControl = 0x1;
            if (value.find('w') != std::string::npos)
                accessControl |= 0x2;
            if (value.find('t') != std::string::npos)
                accessControl |= 0x4;
            if (value.find('s') != std::string::npos)
                scene = true;
            else
                scene = false;
        }
        else if (attrname == "mandatory")
        {
            if (value == "M")
                mandatory = true;
            else
            {
                mandatory = false;
                if (value.size() >= 2 && value[0] == 'M')
                {
                    mandatoryBits = value.substr(1);
                }
            }
        }
        else if (attrname == "mandatory_attr")
        {
            mandatoryAttribute = value;
        }
        else if (attrname == "mandatory_cond")
        {
            if (value == "=")
                mandatory_if_equal = true;
            else
                mandatory_if_equal = false;
        }
        else if (attrname == "len")
        {
            len = std::stoi(value);
        }
        else if (attrname == "size_type")
        {
            if (value.substr(0, 3) == "oct" || value.substr(0, 4) == "byte")
                sizeType = 1;
            else
                sizeType = 2;
        }
        else if (attrname == "default")
        {
            // handling depends on type

        }
    }

    unsigned long long int counter = 0;

    // might have sub-nodes, for enum types for example
    for (rapidxml::xml_node *subnode = node->first_node(); subnode; subnode = subnode->next_sibling())
    {
        if (subnode->type() != rapidxml::node_element)
            continue;

        const std::string subname = subnode->name();

        // there are four kinds of sub-nodes, somehow similar with z-wave:
        // for a bitmap:
        // 1. a bitfield, which covers more than one bit, has a name, a mask and a shifter - I suspect it could be structured (as in being an enum, which would require subfields) but for now I'll ignore that, I'll refine it later
        // 2. a bitflag - this will be only one bit so it will end up in a bool parameter in homegear, has only a name and a mask
        if (subname == "bitfield" || subname == "bitflag")
        {
            ClustersInfo::Bitfield bitfield;

            bitfield.name = ClustersInfo::GetAttrValue(subnode, "name");
            ClustersInfo::TransformNameToHomegear(bitfield.name);

            bitfield.mask = std::stoull(ClustersInfo::GetAttrValue(subnode, "mask"), 0, 16);
            const std::string val = ClustersInfo::GetAttrValue(subnode, "shifter");
            if (!val.empty())
            {
                const std::string starts = val.substr(0, 2);
                if (starts == "0x" || starts == "0X")
                    bitfield.shifter = std::stoi(val, 0 ,16);
                else bitfield.shifter = std::stoi(val);
            }

            bitfield.parentAttr = this;

            bitfields.emplace_back(bitfield);
        }
        // 3. can be an enum, in which case enum sub-nodes with name and value are to be parsed - the value could be omitted, in which case it starts from 0 and it is incremented by 1
        else if (subname == "enum")
        {
            ClustersInfo::Enum enumVal;

            enumVal.name = ClustersInfo::GetAttrValue(subnode, "name");

            const std::string val = ClustersInfo::GetAttrValue(subnode, "val");
            if (val.empty())
            {
                enumVal.val = counter++;
            }
            else
            {
                enumVal.val = counter = std::stoull(val, 0, 16);
                ++counter;
            }

            if (name == "STATUS" && enumVal.name.empty())
                enumVal.name = ZigbeeDevicesDescription::GetStatusString(enumVal.val);

            enumVal.parentAttr = this;

            enums.emplace_back(enumVal);
        }
        // 4. can be a record, in which case individual params are enumerated here, they should be as in the case of the typical attribute
        else if (subname == "param")
        {
            ClustersInfo::Param param;
            param.Parse(subnode, params.empty() ? nullptr : &params.back(), bitfields.empty() ? nullptr : &bitfields.back());

            param.parentAttr = this;

            params.emplace_back(param);
        }
    }

    if (name == "STATUS" && IsEnum() && enums.empty()) // if 'STATUS' was not set in the xml file, just set all available enums
    {
        for (const auto& status : ZigbeeDevicesDescription::statusCodesMap)
        {
            ClustersInfo::Enum enumVal;

            enumVal.val = status.first;
            enumVal.name = status.second;

            enumVal.parentAttr = this;

            enums.emplace_back(enumVal);
        }
    }
}

void ClustersInfo::ClusterInfoExt::Command::Parse(rapidxml::xml_node *node)
{
    for (rapidxml::xml_attribute *attr = node->first_attribute(); attr; attr = attr->next_attribute())
    {
        const std::string attrname = attr->name();
        const std::string value = attr->value();

        if (attrname == "key")
        {
            key = (uint16_t)std::stoi(value, 0, 16);
        }
        else if (attrname == "name")
        {
            name = value;
        }
        else if (attrname == "mandatory")
        {
            if (value == "M")
                mandatory = true;
            else
            {
                mandatory = false;
                if (value.size() >= 2 && value[0] == 'M')
                {
                    mandatoryBits = value.substr(1);
                }
            }
        }
        else if (attrname == "mandatory_attr")
        {
            mandatoryAttribute = value;
        }
        else if (attrname == "mandatory_cond")
        {
            if (value == "=")
                mandatory_if_equal = true;
            else
                mandatory_if_equal = false;
        }
    }

    ClustersInfo::Param* prevSizeParam = nullptr;
    ClustersInfo::Bitfield* prevSizeBitfield = nullptr;
    for (rapidxml::xml_node *subnode = node->first_node(); subnode; subnode = subnode->next_sibling())
    {
        if (subnode->type() != rapidxml::node_element)
            continue;
        else if (std::string("param") != subnode->name())
            continue;

        ClustersInfo::Param param;
        param.Parse(subnode, prevSizeParam, prevSizeBitfield);
        params.emplace_back(param);
        if (param.IsSizeField())
            prevSizeParam = &params.back();
        else if (!param.params.empty())
        {
            // the size field can be actually a subfield (a bitmask is the only case I noticed in clusters definitions)
            for (auto pit = param.params.begin(); pit != param.params.end(); ++pit)
            {
                if (pit->IsSizeField())
                {
                    prevSizeParam = &*pit;
                    prevSizeBitfield = nullptr;
                    break;
                }
            }
        }
        else if (!param.bitfields.empty())
        {
            for (auto pit = param.bitfields.begin(); pit != param.bitfields.end(); ++pit)
            {
                if (pit->IsSizeField())
                {
                    prevSizeParam = nullptr;
                    prevSizeBitfield = &*pit;
                    break;
                }
            }
        }
        else if (!param.IsExplicitSizeField()) // only fields marked explicitly as size fields are retained to be a 'size' for whatever field follows and needs a size
        {
            prevSizeParam = nullptr;
            prevSizeBitfield = nullptr;
        }
    }
}

void ClustersInfo::Param::Parse(rapidxml::xml_node *node, ClustersInfo::Param* prevSizeParam, ClustersInfo::Bitfield* prevSizeBitfield)
{
    for (rapidxml::xml_attribute *attr = node->first_attribute(); attr; attr = attr->next_attribute())
    {
        const std::string attrname = attr->name();
        const std::string value = attr->value();

        if (attrname == "name")
        {
            name = value;
        }
        else if (attrname == "type")
        {
            type = ClustersInfo::AttrInfoExt::GetType(value);
            if (0xFF == type)
                GD::out.printDebug("Parsed type is unknown for param: " + name);
        }
        else if (attrname == "len")
        {
            len = std::stoi(value);
        }
        else if (attrname == "size_type")
        {
            if (value.substr(0, 3) == "oct" || value.substr(0, 4) == "byte")
                sizeType = 1;
            else
                sizeType = 2;
        }
        else if (attrname == "altern_field")
        {
            alternField = value;
        }
        else if (attrname == "might_miss")
        {
            if (value == "yes" || value =="true" || value == "YES" || value=="Yes" || value=="True" || value == "TRUE")
                mightMiss = true;
            else
                mightMiss = false;
        }
        else if (attrname == "if_not_exists")
        {
            ifNotExistsAttr = std::stoi(value, 0, 16);
        }
        else if (attrname == "if_field")
        {
            ifField = value;
        }
        else if (attrname == "if_field_value")
        {
            ifFieldValue = value;
        }
        else if (attrname == "if_not_field")
        {
            ifNotField = value;
        }
    }

    // don't set a size param for fields that do not need it
    if (TypeWithLengthComplexities() && prevSizeParam && prevSizeParam->IsSizeField())
    {
        sizeParam = prevSizeParam;

        if (0 == sizeType)
        {
            // copy the setting from the 'size' param if it wasn't set here, too
            if (0 != prevSizeParam->sizeType)
            {
                if (HasKnowableSize())
                    GD::out.printDebug("Size type is set for a param with a known size: " + name);

                sizeType = prevSizeParam->sizeType;
            }
        }
        else if (0 == prevSizeParam->sizeType)
        {
            prevSizeParam->sizeType = sizeType;

            if (HasKnowableSize())
                GD::out.printDebug("Size type is set for a SIZE param for a param with a known size: " + name);
        }
    }

    unsigned long long int counter = 0;

    // might have sub-nodes, for enum types for example
    for (rapidxml::xml_node *subnode = node->first_node(); subnode; subnode = subnode->next_sibling())
    {
        if (subnode->type() != rapidxml::node_element)
            continue;

        const std::string subname = subnode->name();

        // there are four kinds of sub-nodes, somehow similar with z-wave:
        // for a bitmap:
        // 1. a bitfield, which covers more than one bit, has a name, a mask and a shifter - I suspect it could be structured (as in being an enum, which would require subfields) but for now I'll ignore that, I'll refine it later
        // 2. a bitflag - this will be only one bit so it will end up in a bool parameter in homegear, has only a name and a mask
        if (subname == "bitfield" || subname == "bitflag")
        {
            ClustersInfo::Bitfield bitfield;

            bitfield.name = ClustersInfo::GetAttrValue(subnode, "name");
            ClustersInfo::TransformNameToHomegear(bitfield.name);

            bitfield.mask = std::stoull(ClustersInfo::GetAttrValue(subnode, "mask"), 0, 16);
            std::string val = ClustersInfo::GetAttrValue(subnode, "shifter");
            if (!val.empty())
            {
                const std::string starts = val.substr(0, 2);
                if (starts == "0x" || starts == "0X")
                    bitfield.shifter = std::stoi(val, 0 ,16);
                else bitfield.shifter = std::stoi(val);
            }

            val = ClustersInfo::GetAttrValue(subnode, "size_type");
            if (val.substr(0, 3) == "oct" || val.substr(0, 4) == "byte")
                bitfield.sizeType = 1;
            else if (!val.empty())
                bitfield.sizeType = 2;

            bitfield.parentParam = this;

            bitfields.emplace_back(bitfield);
        }
        // 3. can be an enum, in which case enum sub-nodes with name and value are to be parsed - the value could be omitted, in which case it starts from 0 and it is incremented by 1
        else if (subname == "enum")
        {
            ClustersInfo::Enum enumVal;

            enumVal.name = ClustersInfo::GetAttrValue(subnode, "name");

            const std::string val = ClustersInfo::GetAttrValue(subnode, "val");
            if (val.empty())
            {
                enumVal.val = counter++;
            }
            else
            {
                enumVal.val = counter = std::stoull(val, 0, 16);
                ++counter;
            }

            if (name == "STATUS" && enumVal.name.empty())
                enumVal.name = ZigbeeDevicesDescription::GetStatusString(enumVal.val);

            enumVal.parentParam = this;

            enums.emplace_back(enumVal);
        }
        // 4. can be a record, in which case individual params are enumerated here, they should be as in the case of the typical attribute
        else if (subname == "param")
        {
            ClustersInfo::Param param;
            param.Parse(subnode, params.empty() ? nullptr : &params.back(), bitfields.empty() ? nullptr : &bitfields.back());
            param.parentParam = this;

            params.emplace_back(param);
        }
        else if (subname == "altern")
        {
            AlternRecord record;
            // grab attrs
            record.value = std::stoull(ClustersInfo::GetAttrValue(subnode, "val"), 0, 16);
            std::string lstr = ClustersInfo::GetAttrValue(subnode, "len");
            if (!lstr.empty()) record.len = std::stoi(lstr);
            lstr = ClustersInfo::GetAttrValue(subnode, "size_type");
            if (lstr.substr(0, 3) == "oct" || lstr.substr(0, 4) == "byte")
                record.sizeType = 1;
            else
                record.sizeType = 2;

            // grab params
            for (rapidxml::xml_node *subsubnode = subnode->first_node(); subsubnode; subsubnode = subsubnode->next_sibling())
            {
                if (subsubnode->type() != rapidxml::node_element)
                    continue;

                const std::string subsubname = subsubnode->name();
                if (subsubname == "param")
                {
                    ClustersInfo::Param subparam;
                    subparam.Parse(subsubnode, nullptr, nullptr);
                    subparam.parentParam = this;

                    record.params.emplace_back(subparam);
                }
            }

            if (!record.params.empty())
                alternRecords[record.value] = record;
        }
    }

    if (name == "STATUS" && IsEnum() && enums.empty()) // if 'STATUS' was not set in the xml file, just set all available enums
    {
        for (const auto& status : ZigbeeDevicesDescription::statusCodesMap)
        {
            ClustersInfo::Enum enumVal;

            enumVal.val = status.first;
            enumVal.name = status.second;

            enumVal.parentParam = this;

            enums.emplace_back(enumVal);
        }
    }
}


void ClustersInfo::printDebug(const std::string& str)
{
    GD::out.printDebug(str);
}

BaseLib::PVariable ClustersInfo::Param::GetComplexVariable(BaseLib::PVariable sizeVariable, bool octets, BaseLib::PVariable binaryVariable, bool isStruct) const
{
    return ClustersInfo::GetComplexVariable(this, sizeVariable, octets, binaryVariable, isStruct);
}

BaseLib::PVariable ClustersInfo::Param::AlternRecord::GetComplexVariable(BaseLib::PVariable sizeVariable, bool octets, BaseLib::PVariable binaryVariable, bool isStruct) const
{
    return ClustersInfo::GetComplexVariable(this, sizeVariable, octets, binaryVariable, isStruct);
}

// TODO: this is duplicated from above, get rid of it, have only one implementation for both Param and AttrInfo
BaseLib::PVariable ClustersInfo::AttrInfoExt::GetComplexVariable(BaseLib::PVariable sizeVariable, bool octets, BaseLib::PVariable binaryVariable, bool isStruct) const
{
    return ClustersInfo::GetComplexVariable(this, sizeVariable, octets, binaryVariable, isStruct);
}

}
