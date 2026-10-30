AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -Wall -std=c++20 -DFORTIFY_SOURCE=2 -DGCRYPT_NO_DEPRECATED
AM_LDFLAGS = -Wl,-rpath=/lib/homegear -Wl,-rpath=/usr/lib/homegear -Wl,-rpath=/usr/local/lib/homegear

libdir = $(localstatedir)/lib/homegear/modules
lib_LTLIBRARIES = mod_zigbee.la
mod_zigbee_la_SOURCES = Zigbee.cpp Zigbee.h ZigbeePacket.cpp ZigbeePacket.h ZigbeePeer.cpp ZigbeePeer.h Factory.cpp Factory.h GD.cpp GD.h ZigbeeCentral.cpp ZigbeeCentral.h ZigbeeDevicesDescription.cpp ZigbeeDevicesDescription.h PhysicalInterfaces/IZigbeeInterface.cpp PhysicalInterfaces/IZigbeeInterface.h Interfaces.h Interfaces.cpp PhysicalInterfaces/ZigbeeIds.h PhysicalInterfaces/SerialImpl.cpp PhysicalInterfaces/SerialImpl.h PhysicalInterfaces/Serial.h PhysicalInterfaces/Serial.hpp utils/Event.h utils/CallbackTimer.h utils/Threading.h PhysicalInterfaces/MT.h PhysicalInterfaces/MT.cpp PhysicalInterfaces/AF.h PhysicalInterfaces/SYS.h PhysicalInterfaces/UTIL.h PhysicalInterfaces/APPCFG.h PhysicalInterfaces/SAPI.h PhysicalInterfaces/ZDO.h PhysicalInterfaces/ZCL.h PhysicalInterfaces/ZCL.cpp PhysicalInterfaces/SerialAdmin.h PhysicalInterfaces/SerialAdmin.hpp PhysicalInterfaces/NodeInfo.h PhysicalInterfaces/NodeInfo.cpp PhysicalInterfaces/GatewayImpl.h PhysicalInterfaces/GatewayImpl.cpp PhysicalInterfaces/Hgdc.h PhysicalInterfaces/Hgdc.cpp
mod_zigbee_la_LDFLAGS =-module -avoid-version -shared -lssl -lcrypto
install-exec-hook:
	rm -f $(DESTDIR)$(libdir)/mod_zigbee.la
