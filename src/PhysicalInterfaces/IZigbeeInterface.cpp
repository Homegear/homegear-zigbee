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

#include "../GD.h"
#include "IZigbeeInterface.h"
#include "../ZigbeePacket.h"

namespace Zigbee
{


IZigbeeInterface::IZigbeeInterface(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings)
: IPhysicalInterface(GD::bl, GD::family->getFamily(), settings)
{
    _settings = settings;
    _bl = GD::bl;
    _out.init(GD::bl);
    _out.setPrefix("Module Zigbee: Interface: ");
}

IZigbeeInterface::~IZigbeeInterface()
{
    // wait a little to give a chance for threads to end
    std::this_thread::sleep_for(std::chrono::seconds(5));
}


uint8_t IZigbeeInterface::getCrc8(const std::vector<uint8_t>& packet)
{
    uint8_t crc8 = 0x0;

    for(uint32_t i = 1; i < packet.size() - 1; ++i)
        crc8 ^= packet[i];

    return crc8;
}


void IZigbeeInterface::addCrc8(std::vector<uint8_t>& packet)
{
    if(packet.size() < 2) return;

    packet.back() = getCrc8(packet);
}


void IZigbeeInterface::processPacket(uint32_t addr, uint8_t endPoint, std::vector<uint8_t>& data, int offset, uint8_t LinkQuality)
{
    int savePos = offset;

    // a place to pre-process them
    /*
    if (data.size() >= size_t(offset + 2))
    {
    }
    */

    PZigbeePacket packet = std::make_shared<ZigbeePacket>(data, savePos);

    uint32_t epPart = endPoint;
    epPart <<= 16;

    packet->setSenderAddress(addr | epPart);

    // cc2538 datasheet: receiver sensitivity -97 dBm
    // the same datasheet: 73 dB offset (used when the rssi is given)
    // I found the same values in the datasheet for cc2530

    // although this link seems to use some other offset: https://sunmaysky.blogspot.com/2017/02/conversion-between-rssi-and-lqi-in-z.html
    // see also: https://e2e.ti.com/support/wireless-connectivity/sub-1-ghz/f/156/t/740870

    // "MIN_ED is ED_RF_POWER_MIN_DBM and MAX_ED is ED_RF_POWER_MAX_DBM in Z-Stack source code" Good for checking.

    // in the firmware I compiled, in mac_radio.c :

    //   ed = (MAC_SPEC_ED_MAX * (rssiDbm - ED_RF_POWER_MIN_DBM)) / (ED_RF_POWER_MAX_DBM - ED_RF_POWER_MIN_DBM);

    // MAC_SPEC_ED_MAX is 0xFF, ED_RF_POWER_MAX_DBM is MAC_RADIO_RECEIVER_SATURATION_DBM which is 10
    // ED_RF_POWER_MIN_DBM is (MAC_RADIO_RECEIVER_SENSITIVITY_DBM + MAC_SPEC_ED_MIN_DBM_ABOVE_RECEIVER_SENSITIVITY)
    // MAC_RADIO_RECEIVER_SENSITIVITY_DBM is -97 and MAC_SPEC_ED_MIN_DBM_ABOVE_RECEIVER_SENSITIVITY is 10 so ED_RF_POWER_MIN_DBM is -87

    // so the formula is
    // ed = 255 * (rssiDbm + 87) / (10 + 87) = 255 * (rssiDbm + 87) / 97
    // so rssiDbm = ed * 97 / 255 - 87


    // should actually fit in an int8, because min value is -87 and max value is 10 (although probably it never goes positive)
    packet->setRssi(static_cast<int8_t>(LinkQuality * 97. / 255. - 87.));

    raisePacketReceived(packet);

    // a place for special notifications, if there are any
    /*
    if (data.size() >= (unsigned int)savePos + 2 && ZWAVEXml::ZWAVECmdClasses::IsWakeupPacket(data[savePos], data[savePos + 1]))
        NotifyWakeup(addr);
    */
}

}
