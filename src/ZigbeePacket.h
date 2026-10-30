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

#ifndef ZIGBEEPACKET_H_
#define ZIGBEEPACKET_H_

#include <cstdint>

#include <homegear-base/BaseLib.h>

#include <vector>

namespace Zigbee
{

class ZigbeePacket : public BaseLib::Systems::Packet
{
public:
    explicit ZigbeePacket(const std::vector<uint8_t>& data, int offset = 0);

    virtual ~ZigbeePacket();

    uint8_t length() const { return _length; }
    int32_t senderAddress() const { return _senderAddress; }
    int32_t destinationAddress() const { return _destinationAddress; }
    std::vector<uint8_t>& payload() { return _payload; }

    void setSenderAddress(uint32_t addr);
    void setDestinationAddress(uint32_t addr);

    unsigned int getHeaderSize() const
    {
        return _headerSize;
    }


    void setPacketNumber(unsigned int packetNumber)
    {
        _packetNumber = packetNumber;
    }
    unsigned int getPacketNumber() const
    {
        return _packetNumber;
    }

    // sometimes a notification comes for a the same packet, but previously sent (before retrying to send it again)
    // this checks to see if it was one of those transaction ids, if yes, it's still considered a success
    bool IsValidTransId(uint8_t transId) const
    {
        if (transId == _packetNumber) return true; // behavior like before, it's for the current transaction id, sendCount should be 1

        uint8_t cnt = _packetNumber;
        for (uint8_t i = 1; i < sendCount; ++i)
        {
            --cnt;
            if (0 == cnt) cnt = 0xFF;
            if (cnt == transId) return true;
        }

        return false;
    }

    uint32_t getType() const
    {
        uint32_t type = 0xFF;

        if (_payload.size() > 2)
        {
            // the convention is the same as for zigbee for now: in packet the cluster id is in little endian
            // but here it is reverted, so the cluster id will be in bigger endian in the packet 'type'

            // cluster:
            type = _payload[1];
            type <<= 8;
            type |= _payload[0];
            type <<= 8;

            // cmd:
            // for a manufacturer specific frame, it's in some other position in the frame
            bool ManufacturerSpecific = false;
            if (_payload.size() > 3)
                ManufacturerSpecific = (_payload[2] & 0x4) ? true : false; // check frame control

            if (_payload.size() >= (ManufacturerSpecific ? 7 : 5))
                type |= ManufacturerSpecific ? _payload[6] : _payload[4];
            else
                type |= 0xFF; // a way to identify a bogus packet - hopefully the 0xff command is not defined for a cluster, the 'global' ones are from 0 up to 0x16
        }

        return type;
    }

    uint16_t getCluster() const
    {
        uint16_t clusterId = 0xFF;

        if (_payload.size() > 1)
        {
            clusterId = _payload[1];
            clusterId <<= 8;
            clusterId |= _payload[0];
        }

        return clusterId;
    }

    void setCluster(uint16_t clusterId)
    {
        if (_payload.size() < 2) _payload.resize(2);

        _payload[0] = (clusterId & 0xFF);
        _payload[1] = (clusterId & 0xFF00) >> 8;
    }

    std::vector<uint8_t> getZCLData() const
    {
        if (_payload.size() < getHeaderSize() + 2) return std::vector<uint8_t>();

        // skip over the cluster id
        return std::vector<uint8_t>(_payload.begin() + getHeaderSize() + 2, _payload.end());
    }

    void setZCLData(const std::vector<uint8_t>& data)
    {
        _payload.resize(2 + data.size());

        std::copy(data.begin(), data.end(), _payload.begin() + 2);
    }

    void setClusterAndZCLData(uint16_t clusterId, const std::vector<uint8_t>& data)
    {
        _payload.resize(2 + data.size());
        setCluster(clusterId);
        setZCLData(data);
    }

    void resize(int sz)
    {
        _payload.resize(sz);
        _length = sz;
    }

    std::vector<uint8_t> getData() const
    {
        if (_payload.size() < getHeaderSize()) return std::vector<uint8_t>();

        return std::vector<uint8_t>(_payload.begin() + getHeaderSize(), _payload.end());
    }

    uint8_t GetEndPoint() const
    {
        const uint32_t fakeAddr = destinationAddress();
        return (fakeAddr & 0xFF0000) >> 16;
    }

    int8_t getRssi() const
    {
        return _rssi;
    }

    void setRssi(int8_t rssi)
    {
        _rssi = rssi;
    }

    std::vector<uint8_t> getBitPosition(uint32_t position, uint32_t size);
    void setBitPosition(uint32_t position, uint32_t size, const std::vector<uint8_t>& source);

    std::vector<uint8_t> getPosition(uint32_t position, uint32_t size);
    void setPosition(uint32_t position, const std::vector<uint8_t>& source);

    std::atomic_ushort ack; // numbers of acks received for the packet
    std::atomic_uint sendCount; // how many times it was sent (during this wakeup cycle if wakeup device)

    std::atomic_bool expectsResponse; // the 'ack' packet
    std::atomic_bool expectsRequestedResponse; // it's a query packet, an 'ack' might come, then a response (as in attr value for an attr request)
    std::atomic_bool receivedExpectedResponse;

    std::atomic_bool hasWaitThread;

    std::string packetID; // will be used to remove 'duplicates' from the queue - only for packets defined in device xml file

private:
    uint8_t _length = 0;
    int32_t _senderAddress = 0;
    int32_t _destinationAddress = 0;
    std::vector<uint8_t> _payload;
    unsigned int _headerSize;
    unsigned int _packetNumber;
    int8_t _rssi = 0;
};

typedef std::shared_ptr<ZigbeePacket> PZigbeePacket;

}
#endif
