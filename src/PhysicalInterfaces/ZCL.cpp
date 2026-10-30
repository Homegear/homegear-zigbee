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

#include "ZCL.h"

namespace ZigbeeCommands
{

const std::map<uint8_t, std::string> ZCLFrame::errorCodesMap =
{
    {0x0,  "Operation was successful"},
    {0x01, "Operation was not successful"},
    {0x70, "Request was denied due to lack of permission"},
    {0x71, "Request of multiple contents is not supported"},
    {0x72, "Server Indicates to change the access to the AP for this content"},
    {0x73, "The preference was not accepted. Because it was not understandable or invalid"},
    {0x74, "Inform that preference was not used to modify the content which is provided by this command"},
    {0x7e, "The sender of the command does not have authorization to carry out this command"},
    {0x7f, "A reserved field/subfield/bit contains a non-zero value"},
    {0x80, "The command appears to contain the wrong fields, as detected either by the presence of one or more invalid field entries or by there being missing fields"},
    {0x81, "The specified cluster command is not supported"},
    {0x82, "The specified general ZCL command is not supported"},
    {0x83, "A manufacturer specific unicast, cluster specific command was received with an unknown manufacturer code, or the manufacturer code was recognized but the command is not supported"},
    {0x84, "A manufacturer specific unicast, ZCL specific command was received with an unknown manufacturer code, or the manufacturer code was recognized but the command is not supported"},
    {0x85, "At least one field of the command contains an incorrect value"},
    {0x86, "The specified attribute does not exist"},
    {0x87, "Out of range error, or set to a reserved value"},
    {0x88, "Attempt to write a read only attribute"},
    {0x89, "An operation failed due to an insufficient amount of free space available"},
    {0x8a, "An attempt to create an entry in a table failed due to a duplicate entry already being present in the table"},
    {0x8b, "The requested information could not be found"},
    {0x8c, "Periodic reports cannot be issued for this attribute"},
    {0x8d, "The data type given for an attribute is incorrect"},
    {0x8e, "The selector for an attribute is incorrect"},
    {0x8f, "A request has been made to read an attribute that the requestor is not authorized to read"},
    {0x90, "Setting the requested values would put the device in an inconsistent state on startup"},
    {0x91, "An attempt has been made to write an attribute that is present but is defined using an out-of-band method and not over the air"},
    {0x92, "The supplied values are inconsistent"},
    {0x93, "The credentials presented by the device sending the command are not sufficient to perform this action"},
    {0x94, "The exchange was aborted due to excessive response time"},
    {0x95, "Failed case when a client or a server decides to abort the upgrade process"},
    {0x96, "Invalid OTA upgrade image"},
    {0x97, "Server does not have data block available yet"},
    {0x98, "No OTA upgrade image available for a particular client"},
    {0x99, "The client still requires more OTA upgrade image files in order to successfully upgrade"},
    {0x9a, "The command has been received and is being processed"},
    {0xc0, "An operation was unsuccessful due to a hardware failure"},
    {0xc1, "An operation was unsuccessful due to a software failure"},
    {0xc2, "An error occurred during calibration"},
    {0xc3, "The cluster is not supported"}
};

std::string ZCLFrame::GetErrorString(uint8_t code)
{
    std::map<uint8_t, std::string>::const_iterator it = errorCodesMap.find(code);

    if (it != errorCodesMap.end())
        return it->second;

    return "Unknown";
}


}

