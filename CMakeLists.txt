cmake_minimum_required(VERSION 3.9)
project(homegear_zigbee)

set(CMAKE_CXX_STANDARD 17)

set(SOURCE_FILES
        src/PhysicalInterfaces/IZigbeeInterface.cpp
        src/PhysicalInterfaces/IZigbeeInterface.h
        src/PhysicalInterfaces/ZigbeeIds.h
        src/PhysicalInterfaces/SerialImpl.cpp
        src/PhysicalInterfaces/SerialImpl.h
        src/PhysicalInterfaces/Serial.h
        src/PhysicalInterfaces/Serial.hpp
        src/PhysicalInterfaces/MT.h
        src/PhysicalInterfaces/MT.cpp
        src/PhysicalInterfaces/AF.h
        src/PhysicalInterfaces/SYS.h
        src/PhysicalInterfaces/UTIL.h
        src/PhysicalInterfaces/APPCFG.h
        src/PhysicalInterfaces/SAPI.h
        src/PhysicalInterfaces/ZDO.h
        src/PhysicalInterfaces/ZCL.h
	src/PhysicalInterfaces/ZCL.cpp
        src/PhysicalInterfaces/SerialAdmin.h
        src/PhysicalInterfaces/SerialAdmin.hpp
        src/PhysicalInterfaces/NodeInfo.h
        src/PhysicalInterfaces/NodeInfo.cpp
	src/PhysicalInterfaces/GatewayImpl.h
	src/PhysicalInterfaces/GatewayImpl.cpp
	src/PhysicalInterfaces/Hgdc.h
	src/PhysicalInterfaces/Hgdc.cpp
        src/utils/Event.h
        src/utils/CallbackTimer.h
        src/utils/Threading.h
        src/Factory.cpp
        src/Factory.h
        src/GD.cpp
        src/GD.h
        src/Interfaces.cpp
        src/Interfaces.h
        src/Zigbee.cpp
        src/Zigbee.h
        src/ZigbeeCentral.cpp
        src/ZigbeeCentral.h
        src/ZigbeePacket.cpp
        src/ZigbeePacket.h
        src/ZigbeePeer.cpp
        src/ZigbeePeer.h
	src/ZigbeeDevicesDescription.cpp
	src/ZigbeeDevicesDescription.h)

add_custom_target(homegear COMMAND ../../makeAll.sh SOURCES ${SOURCE_FILES})

add_library(homegear_zigbee ${SOURCE_FILES})
