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

#include "ZigbeePacket.h"

#include "GD.h"

namespace Zigbee
{
ZigbeePacket::ZigbeePacket(const std::vector<uint8_t>& data, int offset)
    :
    ack(0), sendCount(0),
    expectsResponse(false), expectsRequestedResponse(false), receivedExpectedResponse(false), hasWaitThread(true),
    _headerSize(offset), _packetNumber(0)
{
    _payload = data;
    _length = data.size();
}

ZigbeePacket::~ZigbeePacket()
{
}

void ZigbeePacket::setSenderAddress(uint32_t addr)
{
    _senderAddress = (int32_t)addr;
}

void ZigbeePacket::setDestinationAddress(uint32_t addr)
{
    _destinationAddress = (int32_t)addr;
}


std::vector<uint8_t> ZigbeePacket::getBitPosition(uint32_t position, uint32_t size)
{
    try
    {
        if (size % 8 || position % 8)
        {
            if (size > 8) GD::bl->out.printWarning("getBitPosition is used for a size that is not multiple of 8 but bigger than one byte. Is that intended?");
            else if (position / 8 < (position + size) / 8 && (size + position) % 8) GD::bl->out.printWarning("getBitPosition is used for a value that spans over byte boundary. Is that intended?");

            return BaseLib::BitReaderWriter::getPosition(_payload, getHeaderSize() * 8 + position, size);
        }
        else return getPosition(position / 8, size / 8);
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return std::vector<uint8_t>();
}

void ZigbeePacket::setBitPosition(uint32_t position, uint32_t size, const std::vector<uint8_t>& source)
{
	try
	{
	    if (size % 8 || position % 8)
        {
            if (size > 8) GD::bl->out.printWarning("setBitPosition is used for a size that is not multiple of 8 but bigger than one byte. Is that intended?");
            else if (position / 8 < (position + size) / 8 && (size + position) % 8) GD::bl->out.printWarning("setBitPosition is used for a value that spans over byte boundary. Is that intended?");

            std::vector<uint8_t> data;
            const unsigned int requiredSize = size / 8 + ((size & 7) != 0 ? 1 : 0);
            if (source.size() < requiredSize)
            {
                data.resize(requiredSize, 0);
                std::copy(source.begin(), source.end(), data.begin() + requiredSize - source.size());
            }
            else data = source;

            //std::reverse(data.begin(), data.end());

            BaseLib::BitReaderWriter::setPositionBE(getHeaderSize() * 8 + position, size, _payload, data);
        }
	    else
	    {
	        std::vector<uint8_t> data;

	        size /= 8;
	        if (source.size() < size)
            {
                data.resize(size, 0);
                std::copy(source.begin(), source.end(), data.begin() + size - source.size());
            }
            else data = source;

            setPosition(getHeaderSize() + position / 8, data);
	    }
	}
	catch(const std::exception& ex)
	{
		GD::bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
}

std::vector<uint8_t> ZigbeePacket::getPosition(uint32_t position, uint32_t size)
{
    try
    {
        const int index = getHeaderSize() + position;
        if (_payload.size() < index + size) return std::vector<uint8_t>();

        std::vector<uint8_t> res(size);

        std::reverse_copy(_payload.begin() + index, _payload.begin() + index + size, res.begin());

        return res;
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    return std::vector<uint8_t>();
}

void ZigbeePacket::setPosition(uint32_t position, const std::vector<uint8_t>& source)
{
    if (source.empty()) return;

    try
    {
        if (_payload.size() < position + source.size())
        {
            _length = position + source.size();
            _payload.resize(_length);
        }

        std::reverse_copy(source.begin(), source.end(), _payload.begin() + position);
    }
    catch(const std::exception& ex)
    {
        GD::bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}



}
