/* Copyright 2019 Homegear GmbH */

#include "../GD.h"

#include "AF.h"
#include "SYS.h"
#include "UTIL.h"
#include "APPCFG.h"
#include "SAPI.h"
#include "ZDO.h"
#include "ZCL.h"

#define _SERIAL_HPP

namespace Zigbee {

template<typename Impl> const std::map<uint8_t, std::string> Serial<Impl>::errorCodesMap =
    {
        {0x00, "ZSuccess"},
        {0x01, "Zfailure"},
        {0x02, "ZinvalidParameter"},
        {0x09, "NV_ITEM_UNINIT"},
        {0x0a, "NV_OPER_FAILED"},
        {0x0c, "NV_BAD_ITEM_LEN"},
        {0x10, "ZmemError"},
        {0x11, "ZbufferFull"},
        {0x12, "ZunsupportedMode"},
        {0x13, "ZmacMemError"},
        {0x18, "MAC_UNSUPPORTED_NOT_SPOORT"},
        {0x19, "MAC_BAD_STATE"},
        {0x20, "MAC_NO_RESOURCES"},
        {0x21, "MAC_ACK_PENDING"},
        {0x22, "MAC_NO_TIME"},
        {0x23, "MAC_TX_ABORTED"},
        {0x24, "SAPI_IN_PROGRESS"},
        {0x25, "SAPI_TIMEOUT"},
        {0x26, "SAPI_INIT"},
        {0x70, "REQUEST_DENIED"},
        {0x71, "MULTIPLE_REQUEST_NOT_ALLOWED"},
        {0x72, "INDICATION_REDIRECTION_TO_AP"},
        {0x73, "PREFERENCE_DENIED"},
        {0x74, "PREFERENCE_IGNORED"},
        {0x7E, "NOT_AUTHORIZED"},
        {0x80, "zdoInvalidRequestType"},
        {0x81, "UNSUP_CLUSTER_CMD"},
        {0x82, "zdoInvalidEndpoint"},
        {0x84, "zdoUnsupported"},
        {0x85, "zdoTimeout"},
        {0x86, "zdoNoMatch"},
        {0x87, "zdoReserved"},
        {0x88, "zdoNoBindEntry"},
        {0x89, "zdoNoDescriptor"},
        {0x8A, "zdoInsufficientSpace"},
        {0x8B, "zdoNotPermitted"},
        {0x8C, "zdoTableFull"},
        {0x8D, "zdoNotAuthorized"},
        {0x8E, "zdoDeviceBindingTableFull"},
        {0x95, "OTA_ABORT"},
        {0x96, "OTA_IMAGE_INVALID"},
        {0x97, "OTA_WAIT_FOR_DATA"},
        {0x98, "OTA_NO_IMAGE_AVAILABLE"},
        {0x99, "OTA_REQUIRE_MORE_IMAGE"},
        {0xa1, "ZsecNoKey"},
        {0xa2, "ZsecOldFrmCount"},
        {0xa3, "ZsecMaxFrmCount"},
        {0xa4, "ZsecCCMFail"},
        {0xb1, "ZapsFail"},
        {0xb2, "ZapsTableFull"},
        {0xb3, "ZapsIllegalRequest"},
        {0xb4, "ZapsInvalidBinding"},
        {0xb5, "ZapsUnsupportedAttrib"},
        {0xb6, "ZapsNotSupported"},
        {0xb7, "ZapsNoAck"},
        {0xb8, "ZapsDuplicateEntry"},
        {0xb9, "ZapsNoBoundDevice"},
        {0xba, "ZapsNotAllowed"},
        {0xbb, "ZapsNotAuthenticated"},
        {0xc1, "ZnwkInvalidParam"},
        {0xc2, "ZnwkInvalidRequest"},
        {0xc3, "ZnwkNotPermitted"},
        {0xc4, "ZnwkStartupFailure"},
        {0xc5, "ZnwkAlreadyPresent"},
        {0xc6, "ZnwkSyncFailure"},
        {0xc7, "ZnwkTableFull"},
        {0xc8, "ZnwkUnknownDevice"},
        {0xc9, "ZnwkUnsupportedAttribute"},
        {0xca, "ZnwkNoNetworks"},
        {0xcb, "ZnwkLeaveUnconfirmed"},
        {0xcc, "ZnwkNoAck"},
        {0xcd, "ZnwkNoRoute"},
        {0xe0, "MAC_BEACON_LOSS"},
        {0xe1, "MAC_CHANNEL_ACCESS_FAILURE"},
        {0xe2, "MAC_DENIED"},
        {0xe3, "MAC_DISABLE_TRX_FAILURE"},
        {0xe4, "MAC_FAILED_SECURITY_CHECK"},
        {0xe5, "MAC_FRAME_TOO_LONG"},
        {0xe6, "MAC_INVALID_GTS"},
        {0xe7, "MAC_INVALID_HANDLE"},
        {0xe8, "MAC_INVALID_PARAMETER"},
        {0xe9, "MAC_NO_ACK"},
        {0xea, "MAC_NO_BEACON"},
        {0xeb, "MAC_NO_DATA"},
        {0xec, "MAC_NO_SHORT_ADDR"},
        {0xed, "MAC_OUT_OF_CAP"},
        {0xee, "MAC_PANID_CONFLICT"},
        {0xef, "MAC_REALIGNMENT"},
        {0xf0, "MAC_TRANSACTION_EXPIRED"},
        {0xf1, "MAC_TRANSACTION_OVERFLOW"},
        {0xf2, "MAC_TX_ACTIVE"},
        {0xf3, "MAC_UNAVAILABLE_KEY"},
        {0xf4, "MAC_UNSUPPORTED_ATTRIBUTE"},
        {0xf5, "MAC_UNSUPPORTED"},
        {0xf6, "MAC_ON_TIME_TOO_LING"},
        {0xf7, "MAC_PAST_TIME"},
        {0xf8, "MAC_TRACKING_OFF"},
        {0xfc, "MAC_SCAN_IN_PROGRESS"},
        {0xff, "MAC_SRC_MATCH_INVALID_INDEX"}
    };

template<class Impl>
std::string Serial<Impl>::GetErrorString(uint8_t code) {
  std::map<uint8_t, std::string>::const_iterator it = errorCodesMap.find(code);

  if (it != errorCodesMap.end())
    return it->second;

  return "Unknown";
}

template<class Impl>
Serial<Impl>::Serial(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings) : IZigbeeInterface(settings),
                                                                                              _initComplete(false),
                                                                                              _commissioning(false),
                                                                                              _whileResetting(false),
                                                                                              serialImplementation(this),
                                                                                              IEEEAddress(0),
                                                                                              transactionCount(0),
                                                                                              zclTransactionCount(0),
                                                                                              m_processPacketsThreadsPool(this),
                                                                                              m_tryToSendWorker(this),
                                                                                              waitingThread(this),
                                                                                              _packetNumber(0) {
  networkAdmin.serial = this;
  networkAdmin._out.init(_bl);
  networkAdmin._out.setPrefix("Module Zigbee: Network Management: ");

  if (settings->listenThreadPriority == -1) {
    settings->listenThreadPriority = 0;
    settings->listenThreadPolicy = SCHED_OTHER;
  }

  psk = GD::bl->hf.getUBinary(settings->password);

  // just ensure that the psk is of the correct size
  const int len = psk.size();
  if (len > 16) {
    psk.resize(16);
    _out.printInfo("The password is too long. Truncating.");
  } else if (len < 16) {
    static const uint8_t defpass[16] = {0xA1, 0x13, 0xB3, 0x12, 0x18, 0x0C, 0x06, 0xA8, 0xC5, 0xE1, 0xD0, 0x72, 0x11, 0x34, 0x1A, 0xCC};
    psk.resize(16);
    std::copy(defpass + len, defpass + 16, psk.begin() + len);
    if (len > 0)
      _out.printInfo("The password is too short. Completing.");
    else
      _out.printInfo("No password is set. Setting a default password. Please ensure a password is set!");
  }

  waitingThread.Start();

  m_tryToSendWorker.Start();

  m_processPacketsThreadsPool.Start();

  _out.setPrefix(GD::out.getPrefix() + "Zigbee serial module \"" + settings->id + "\": ");
}

template<class Impl>
Serial<Impl>::~Serial() {
  _stopCallbackThread = true;

  _out.printInfo("Joining init thread");
  _bl->threadManager.join(_initThread);

  serialImplementation.SetStopped(false); // ensure it's really stopped

  stopListening();

  waitingThread.Stop();

  m_tryToSendWorker.Stop();

  m_processPacketsThreadsPool.Stop();
}

template<class Impl>
void Serial<Impl>::saveSettingToDatabase(std::string setting, std::vector<uint8_t> &value) {
  try {
    if (setting.empty())
      return;
    Database::DataRow data;
    std::string name = _settings->id + '.' + setting;
    data.push_back(std::shared_ptr<Database::DataColumn>(new Database::DataColumn(_familyId)));
    data.push_back(std::shared_ptr<Database::DataColumn>(new Database::DataColumn(1000)));
    data.push_back(std::shared_ptr<Database::DataColumn>(new Database::DataColumn(name)));
    data.push_back(std::shared_ptr<Database::DataColumn>(new Database::DataColumn(_familyId)));
    data.push_back(std::shared_ptr<Database::DataColumn>(new Database::DataColumn(1000)));
    data.push_back(std::shared_ptr<Database::DataColumn>(new Database::DataColumn(name)));
    data.push_back(std::shared_ptr<Database::DataColumn>(new Database::DataColumn()));
    data.push_back(std::shared_ptr<Database::DataColumn>(new Database::DataColumn()));
    data.push_back(std::shared_ptr<Database::DataColumn>(new Database::DataColumn(value)));
    _bl->db->saveFamilyVariableAsynchronous(_familyId, data);
  }
  catch (const std::exception &ex) {
    _bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

template<class Impl>
void Serial<Impl>::setup(int32_t userID, int32_t groupID, bool setPermissions) {
  try {
    if (setPermissions)
      setDevicePermission(userID, groupID);
  }
  catch (const std::exception &ex) {
    _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

template<class Impl>
void Serial<Impl>::RetryInit() {
  if (_whileResetting) return;

  int cnt = 0;
  while (!_initComplete && ++cnt < 20 && !_stopCallbackThread) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    init();
    // not a solution at all
    //if (!_initComplete && !_stopCallbackThread) std::this_thread::sleep_for(std::chrono::seconds(_commissioning ? 300 : 15));
  }
  if (!_initComplete) {
    serialImplementation.SetStopped();
  }
}

template<class Impl>
void Serial<Impl>::startListening() {
  bool error = false;

  try {
    stopListening();

    if (Impl::IsLocal()) {
      if (_settings->device.empty()) {
        _out.printError("Error: No device defined for Zigbee module. Please specify it in \"zigbee.conf\".");
        return;
      }
    } else if (Impl::IsGateway()) {
      _myAddress = GD::family->getCentral()->getAddress();

      if (_settings->host.empty() || _settings->port.empty() || _settings->caFile.empty() || _settings->certFile.empty() || _settings->keyFile.empty()) {
        _out.printError("Error: Configuration of Homegear Gateway is incomplete. Please correct it in \"zigbee.conf\".");

        /*
        if (_settings->host.empty()) _out.printError("Error: Host is empty");
        if (_settings->port.empty()) _out.printError("Error: Port is empty");
        if (_settings->caFile.empty()) _out.printError("Error: CA file is empty");
        if (_settings->certFile.empty()) _out.printError("Error: cert file is empty");
        if (_settings->keyFile.empty()) _out.printError("Error: key file is empty");
        */

        return;
      }
    }

    serialImplementation.Reset();
    if (!serialImplementation.Open()) {
      // TODO: start the thread nevertheless, might open the device when available?

      if (!Impl::IsLocal()) // for non local devices, do that
      {
        _out.printWarning("Warning: Interface not connected, listening nevertheless");
        _stopCallbackThread = false;

        // don't start the listen thread for HGDC, it has a callback
        if (Impl::IsGateway()) {
          if (_settings->listenThreadPriority > -1)
            _bl->threadManager.start(_listenThread, true, _settings->listenThreadPriority, _settings->listenThreadPolicy, &Impl::listen, &serialImplementation);
          else
            _bl->threadManager.start(_listenThread, true, &Impl::listen, &serialImplementation);
        }

        IZigbeeInterface::startListening();
      } else if (Impl::IsLocal())
        _out.printWarning("Warning: Could not open interface");

      return;
    }

    _stopCallbackThread = false;

    if (Impl::IsLocal())
      serialImplementation.EmptyReadBuffers();

    // don't start the listen thread for HGDC, it has a callback
    if (Impl::IsLocal() || Impl::IsGateway()) {
      if (_settings->listenThreadPriority > -1)
        _bl->threadManager.start(_listenThread, true, _settings->listenThreadPriority, _settings->listenThreadPolicy, &Impl::listen, &serialImplementation);
      else
        _bl->threadManager.start(_listenThread, true, &Impl::listen, &serialImplementation);
    }

    IZigbeeInterface::startListening();

    RetryInit();
  }
  catch (const std::exception &ex) {
    error = true;
    _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }

  try {
    if (error) {
      _out.printWarning("Warning: Interface not connected, listening nevertheless");
      _stopCallbackThread = false;

      // don't start the listen thread for HGDC, it has a callback
      if (Impl::IsLocal() || Impl::IsGateway()) {
        if (_settings->listenThreadPriority > -1)
          _bl->threadManager.start(_listenThread, true, _settings->listenThreadPriority, _settings->listenThreadPolicy, &Impl::listen, &serialImplementation);
        else
          _bl->threadManager.start(_listenThread, true, &Impl::listen, &serialImplementation);
      }

      IZigbeeInterface::startListening();
    }
  }
  catch (const std::exception &ex) {
    _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

template<class Impl>
void Serial<Impl>::stopListening() {
  try {
    if (!_stopped) {
      _stopCallbackThread = true;
      if (Impl::IsLocal() || Impl::IsGateway()) _bl->threadManager.join(_listenThread);
      serialImplementation.SetStopped();
      _initComplete = false;

      serialImplementation.Close();
      IZigbeeInterface::stopListening();
    }
  }
  catch (const std::exception &ex) {
    _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

template<class Impl>
bool Serial<Impl>::SysOsalNVWrite(uint16_t id, const std::vector<uint8_t> &value) {
  ZigbeeCommands::SysOsalNVWriteRequest nvWriteRequest;
  ZigbeeCommands::SysOsalNVWriteResponse nvWriteResponse;
  std::vector<uint8_t> response;

  _out.printInfo("Info: SysOsalNVWrite, id: 0x" + BaseLib::HelperFunctions::getHexString(id));

  nvWriteRequest.id = id;
  nvWriteRequest.value = value;
  getResponse(nvWriteRequest, response, 0, 1, 15);

  if (nvWriteResponse.Decode(response)) {
    _out.printInfo("Info: NV write response status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(nvWriteResponse.status)));
    if (ZigbeeReturnIds::ZSuccess != nvWriteResponse.status)
      return false;
  } else {
    _out.printDebug("Debug: Couldn't decode NV write response: 0x" + BaseLib::HelperFunctions::getHexString(response));
    return false;
  }

  return true;
}

template<class Impl>
bool Serial<Impl>::SysOsalNVRead(uint16_t id, std::vector<uint8_t> &value) {
  ZigbeeCommands::SysOsalNVReadRequest nvReadRequest;
  ZigbeeCommands::SysOsalNVReadResponse nvReadResponse;
  std::vector<uint8_t> response;

  _out.printInfo("Info: SysOsalNVRead, id: 0x" + BaseLib::HelperFunctions::getHexString(id));

  nvReadRequest.id = id;
  getResponse(nvReadRequest, response, 0, 1, 10);

  if (nvReadResponse.Decode(response)) {
    _out.printInfo("Info: NV read response status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(nvReadResponse.status)));
    if (ZigbeeReturnIds::ZSuccess != nvReadResponse.status)
      return false;

    value = nvReadResponse.value;

    return true;
  } else {
    _out.printDebug("Debug: Couldn't decode NV read response: 0x" + BaseLib::HelperFunctions::getHexString(response));
    return false;
  }

  return true;
}

template<class Impl>
bool Serial<Impl>::RegisterEndpoints() {
  // TODO: register endpoint(s) before commissioning!
  ZigbeeCommands::AFRegisterRequest epRegisterRequest;

  // Client: A cluster interface which is listed in the output cluster list of the simple descriptor on an endpoint.
  // Typically this interface sends commands that manipulate the attributes on the corresponding server cluster. A
  // client cluster communicates with a corresponding remote server cluster with the same cluster identifier.

  // Server: A cluster interface which is listed in the input cluster list of the simple descriptor on an endpoint.
  // Typically this interface supports all or most of the attributes of the cluster. A server cluster communicates
  // with a corresponding remote client cluster with the same cluster identifier.

  // TODO: for now, there is an end point only (experimental) for 'home automation' profile. Maybe I should add an end point for most (all?) of the public profiles
  // to be checked if that's true, I think the profile is actually set in firmware in the zstack at compile time

  // NOTE: Don't need to register all clusters we need notifications for, binding appears to work even if the clusters aren't here (at least for the plug device)
  // the attribute reporting works, too!

  // server clusters
  //epRegisterRequest.AppInClusterList.push_back(1);
  //epRegisterRequest.AppInClusterList.push_back(3);

  //epRegisterRequest.AppInClusterList.push_back(0x6);
  //epRegisterRequest.AppInClusterList.push_back(0x10);
  //epRegisterRequest.AppInClusterList.push_back(0x0B04);

  // client clusters
  //epRegisterRequest.AppOutClusterList.push_back(1);
  //epRegisterRequest.AppOutClusterList.push_back(3);

  //epRegisterRequest.AppOutClusterList.push_back(0x6);
  //epRegisterRequest.AppOutClusterList.push_back(0x10);
  //epRegisterRequest.AppOutClusterList.push_back(0x0B04);

  std::vector<uint8_t> response;
  getResponse(epRegisterRequest, response, 0, 1, 10);
  ZigbeeCommands::AFRegisterResponse epRegisterResponse;
  if (epRegisterResponse.Decode(response)) {
    _out.printInfo("Info: End point registering went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(epRegisterResponse.status)));
    if (ZigbeeReturnIds::ZSuccess != epRegisterResponse.status && ZigbeeReturnIds::ZapsDuplicateEntry != epRegisterResponse.status)
      return false;
  } else {
    _out.printDebug("Debug: Couldn't decode end point registering response: 0x" + BaseLib::HelperFunctions::getHexString(response));
    return false;
  }

  return true;
}

template<class Impl>
void Serial<Impl>::sendMagicByte() {
  const std::vector<uint8_t> magic{0xEF};
  serialImplementation.rawSend(magic);
}

template<class Impl>
bool Serial<Impl>::init() {
  if (_stopCallbackThread)
    return false;

  if (_whileResetting)
    return true;

  try {
    _out.printInfo("Info: Initializing interface");

    int cnt = 0; // to prevent an endless loop in some odd cases

    do {
      sendMagicByte();
      serialImplementation.EmptyReadBuffers(30);
      ++cnt;
    } while (cnt < 2 && !_stopCallbackThread);

    if (_stopCallbackThread)
      return false;

    // just a ping test

    if (!Ping()) {
      _out.printDebug("Debug: PING did not go well, trying to continue nevertheless...");

      std::this_thread::sleep_for(std::chrono::seconds(3)); // leave it some time

      if (!Reset(false)) {
        _out.printDebug("Debug: The reset attempt did not work as well, still trying...");
      } else {
        _out.printDebug("Debug: Reset tried, ping again...");
        std::this_thread::sleep_for(std::chrono::seconds(5)); // leave it some time to start
      }

      if (!Ping()) {
        _out.printDebug("Debug: Second PING did not go well, trying to continue nevertheless...");
      }
    }

    std::vector<uint8_t> response;

    // get some info, see if it's already set
    bool shouldInitTheNetwork = false;

    ZigbeeCommands::UtilGetDeviceInfoRequest infoRequest;
    getResponse(infoRequest, response, 0, 1, 15);

    ZigbeeCommands::UtilGetDeviceInfoResponse infoResponse; // TODO: output more info! Also check it, short address should be 0, we're the coordinator
    if (infoResponse.Decode(response)) {
      _out.printInfo("Info: Info request went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(infoResponse.status)));
      if (ZigbeeReturnIds::ZSuccess != infoResponse.status || !(infoResponse.deviceType & 0x1))
        return false;

      // if it's not set as coordinator yet (addr 0x0000), or it's set, but it's in some other state than 'on hold' or 'coordinator' or 'coordinator starting'
      // TODO: do more proper checks here to see if the stick needs to be re-initialized
      // maybe use the NV value 0x0F00 (ZNP_HAS_CONFIGURED)?
      if (/*0 != infoResponse.shortAddress ||*/ !(0x00 == infoResponse.deviceState || 0x08 == infoResponse.deviceState || 0x09 == infoResponse.deviceState)) {
        _out.printInfo("Info: Coordinator device will be initialized, device state: 0x" + BaseLib::HelperFunctions::getHexString(infoResponse.deviceState));
        //if (0 != infoResponse.shortAddress) _out.printInfo("Info: The device does not have the short addr set to zero");

        shouldInitTheNetwork = true;
      } else if (0x0 == infoResponse.deviceState) // is the 'stick' in DEV_HOLD state?
      {
        // check the value for 'coordinator'
        std::vector<uint8_t> val(1, 0xFF);
        if (!SysOsalNVRead(0x87, val) || 0 != val[0]) {
          val[0] = 0;
          if (!SysOsalNVWrite(0x87 /*ZCD_NV_LOGICAL_TYPE**/, val)) {
            // it's not something critical, so continue after warning
            _out.printDebug("Debug: Couldn't enable coordinator");
          }
        } else
          _out.printInfo("Info: Already set as coordinator");

        // check the value for 'concentrator'
        val[0] = 0;
        if (!SysOsalNVRead(0x32, val) || 0 == val[0]) {
          val[0] = 1;
          if (!SysOsalNVWrite(0x32 /*ZCD_NV_CONCENTRATOR_ENABLE*/, val)) {
            // it's not something critical, so continue after warning
            _out.printDebug("Debug: Couldn't enable concentrator");
          }
        } else
          _out.printInfo("Info: Already set as concentrator");

        _out.printInfo("Info: Device on hold, starting (1)...");

        ZigbeeCommands::ZDOStartupFromAppRequest startRequest;
        ZigbeeCommands::ZDOStartupFromAppResponse startResponse;
        getResponse(startRequest, response, 0, 1, 15); // increased timeout, because on cc2530 I've seen it timeout

        if (startResponse.Decode(response)) {
          _out.printInfo(std::string("Info: start request response went well, status: ") + ((startResponse.status == 0) ? "Restored network state" : ((startResponse.status == 1) ? "New network state" : "Leave and not started")));

          // at this point the stick was started, but some odd ones might go into commissioning by themselves, let them do that
          std::this_thread::sleep_for(std::chrono::seconds(5));

          // TODO: if it goes into auto-commissioning, it will have its own settings, not what it's set when commissioning by the complete ResetNetwork
          // should I redo a full reset?
          // it would take too much, better inform users to do a manual reset if they have such a stick

          // in some cases commissioning is reported for restoring network status. In that case the 3 seconds wait above is enough to let the process develop
          // so when reaching this point, _commissioning should be already false
          // but even if true, not much harm should be done if going into this

          if (_commissioning) {
            _out.printInfo("Info: Commissioning, waiting...");

            ResetNetwork(true); // wait for it to finish
          }

          getResponse(infoRequest, response, 0, 1, 15);
          if (infoResponse.Decode(response)) {
            _out.printInfo("Info: Info request went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(infoResponse.status)));
            if (ZigbeeReturnIds::ZSuccess != infoResponse.status || !(infoResponse.deviceType & 0x1))
              return false;

            if (0x0 == infoResponse.deviceState) {
              _out.printInfo("Info: The device still on hold after attempt to start");
            } else if (0 != infoResponse.shortAddress) {
              _out.printInfo("Info: The device does not have the short addr set to zero");

              shouldInitTheNetwork = true;
            }
          } else {
            _out.printDebug("Debug: Couldn't decode info request response 2: 0x" + BaseLib::HelperFunctions::getHexString(response));
            return false;
          }
        } else {
          _out.printDebug("Debug: Couldn't decode start request response 1: 0x" + BaseLib::HelperFunctions::getHexString(response));
          return false;
        }
      }
    } else {
      _out.printDebug("Debug: Couldn't decode info request response 1: 0x" + BaseLib::HelperFunctions::getHexString(response));
      return false;
    }

    IEEEAddress = infoResponse.IEEEaddress;

    //const bool onlyCoordinator = (infoResponse.deviceType == 0x1);

    if (!RegisterForMessages())
      return false;

    // some sticks might have some values corrupted, attempt to fix them here, setting them to default

    //{
    //    std::vector<uint8_t> addrval(2, 0);

    //    if (!SysOsalNVWrite(0x89 /*ZCD_NV_NWKMGR_ADDR*/, addrval))
    // it's not something critical, so continue after warning
    //        _out.printDebug("Debug: Couldn't set network manager addr");
    //}

    // ***********************************************************************************************


    // The following should be done only when initializing the network, I guess we won't do this at each startup
    //shouldInitTheNetwork = true; // force for test
    if (shouldInitTheNetwork) {
      if (!ResetNetwork())
        return false;
    } else if (0x0 == infoResponse.deviceState) // is the 'stick' in DEV_HOLD state?
    {
      _out.printInfo("Info: Device on hold, starting (2)...");

      ZigbeeCommands::ZDOStartupFromAppRequest startRequest;
      ZigbeeCommands::ZDOStartupFromAppResponse startResponse;
      getResponse(startRequest, response, 0, 1, 15);  // increased timeout, because on cc2530 I've seen it timeout

      if (startResponse.Decode(response)) {
        _out.printInfo(std::string("Info: start request response went well, status: ") + ((startResponse.status == 0) ? "Restored network state" : ((startResponse.status == 1) ? "New network state" : "Leave and not started")));
      } else {
        _out.printDebug("Debug: Couldn't decode start request response 2: 0x" + BaseLib::HelperFunctions::getHexString(response));
        return false;
      }

      if (!RegisterEndpoints()) // TODO: perhaps delete it first to avoid the 'duplicate entries' status
        return false;

      // get device info to be sure it's set ok
      getResponse(infoRequest, response, 0, 1, 15);

      if (infoResponse.Decode(response)) {
        _out.printInfo("Info: Info request went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(infoResponse.status)));
        if (0 != infoResponse.shortAddress)
          _out.printInfo("Info: The device does not have the short addr set to zero");

        if (ZigbeeReturnIds::ZSuccess != infoResponse.status)
          return false;
      } else {
        _out.printDebug("Debug: Couldn't decode info request response 3: 0x" + BaseLib::HelperFunctions::getHexString(response));
        return false;
      }

      IEEEAddress = infoResponse.IEEEaddress;
    } else {
      // wait some time to be sure it fully restarted, if it's still starting
      if (0x08 == infoResponse.deviceState) {
        _out.printInfo("Info: Device starting, waiting...");
        std::this_thread::sleep_for(std::chrono::seconds(5));
      } else {
        _out.printInfo("Info: Device already started as a coordinator");
      }

      if (!RegisterEndpoints())  // TODO: perhaps delete it first to avoid the 'duplicate entries' status
        return false;

      // get device info to be sure it's set ok
      getResponse(infoRequest, response, 0, 1, 15);

      if (infoResponse.Decode(response)) {
        _out.printInfo("Info: Info request went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(infoResponse.status)));
        if (0 != infoResponse.shortAddress)
          _out.printInfo("Info: The device does not have the short addr set to zero");

        if (ZigbeeReturnIds::ZSuccess != infoResponse.status)
          return false;
      } else {
        _out.printDebug("Debug: Couldn't decode info request response 4: 0x" + BaseLib::HelperFunctions::getHexString(response));
        return false;
      }

      IEEEAddress = infoResponse.IEEEaddress;
    }


    // ZCD_NV_APS_ALLOW_R19_SECURITY = 0x67, default false

    std::vector<uint8_t> val(1, 0);
    if (_settings->all.find("allowr19security") != _settings->all.end()) {
      const std::string sval = _settings->all["allowr19security"]->stringValue;

      _out.printInfo("Allow R19 security setting found, value: " + sval);

      if (sval == "no" || sval == "n" || sval == "N" || sval == "NO" || sval == "No")
        val[0] = 0x0;
      else
        val[0] = 0x1;
    }

    if (!SysOsalNVWrite(0x67, val)) // ZCD_NV_APS_ALLOW_R19_SECURITY
      return false;

    // TODO: for configuration, use APP_CNF_BDB_SET_TC_REQUIRE_KEY_EXCHANGE to allow or not devices to join unsecurely (this allows legacy devices to join)
    // currently this is done in the firmware by setting BDB_DEFAULT_TC_REQUIRE_KEY_EXCHANGE=FALSE
    // here we can override that with some preference perhaps set in config file

    ZigbeeCommands::AppCnfBdbSetTCRequireKeyExchangeRequest TCRequireKeyExchangeRequest;

    if (_settings->all.find("linkkeyexchange") != _settings->all.end()) {
      const std::string sval = _settings->all["linkkeyexchange"]->stringValue;

      _out.printInfo("Link Key Exchange setting found, value: " + sval);

      if (sval == "no" || sval == "n" || sval == "N" || sval == "NO" || sval == "No")
        TCRequireKeyExchangeRequest.bdbTrustCenterRequireKeyExchange = 0x0;
      else
        TCRequireKeyExchangeRequest.bdbTrustCenterRequireKeyExchange = 0x1;
    }

    _out.printInfo(std::string("Info: Setting Link Key Exchange as ") + (TCRequireKeyExchangeRequest.bdbTrustCenterRequireKeyExchange ? "required" : "not required"));

    getResponse(TCRequireKeyExchangeRequest, response, 0, 1, 15);

    ZigbeeCommands::AppCnfBdbSetTCRequireKeyExchangeResponse TCRequireKeyExchangeResponse;
    if (TCRequireKeyExchangeResponse.Decode(response)) {
      _out.printInfo("Info: TC Require Key Exchange went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(TCRequireKeyExchangeResponse.status)));
    } else {
      _out.printInfo("Info: Couldn't decode TC Require Key Exchange response: 0x" + BaseLib::HelperFunctions::getHexString(response));
    }

    // TODO: after initialization without network resetting (which would do network commissioning)
    // I get a notification meaning BDB_COMMISSIONING_FORMATION_FAILURE, BDB_COMMISSIONING_FORMATION, no 'remaining commissioning modes'
    // raw packet: FE034F80080200C6
    // the network seems to work fine afterwards, though

    //val[0] = 0x66;
    //if (!SysOsalNVWrite(0x0F00 /*ZNP_HAS_CONFIGURED*/, val)) // returns invalid id!!!!
    //    return false;


    // there are some sticks that seem to go by themselves into commissioning mode after starting
    // and do not restore the network settings if unpowered
    // this tries to ensure they have the restoring option set
    // NOTE: this does not seem to help :(

    //if (!SysOsalNVRead(0x3, val) || 0 == val[0])
    //{
    //    val[0] = 0x4;  // restore network state
    //    if (!SysOsalNVWrite(0x3 /*ZCD_NV_STARTUP_OPTION*/, val))
    //    {
    //        _out.printDebug("Debug: Couldn't set startup option to restore network state");
    //    }
    //}

    _initComplete = true;
    tryToSend(0, false);

    return true;
  }
  catch (const std::exception &ex) {
    _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }

  return false;
}

template<class Impl>
ZigbeeCommands::AFDataRequest Serial<Impl>::GetSimpleCommand(uint16_t addrDev, uint8_t endPoint, uint16_t clusterId, uint8_t cmd, bool dir) {
  ZigbeeCommands::AFDataRequest dataRequest;
  dataRequest.DstAddr = addrDev;
  dataRequest.DstEndpoint = endPoint;
  //dataRequest.SrcEndpoint = 1;
  dataRequest.ClusterId = clusterId; //onoff
  dataRequest.TransId = GetNextTransactionCount();
  //dataRequest.Options = 0x30; // ACK, discover route (TODO: do we need ACK if 'default response' comes back?)

  // Note: the ACK comes as a AFDataNotification and it's with 'success' in status even if the sent command is wrong (probably it means only delivery success and nothing more)
  // if I set 'wildcard' for profile id, it does not appear to work!
  // a default response (AFIncomingMsgNotification) comes back with the same zcl transaction id in it (but different seq number, apparently that's counted on the device, not reflects what we send)

  ZigbeeCommands::ZCLFrame zclFrame;

  zclFrame.SetGlobal(false);
  zclFrame.SetDirection(dir); // from client to server!
  zclFrame.TransactionSeq = GetZCLNextTransactionCount();
  zclFrame.cmd = cmd;

  dataRequest.Data = zclFrame.GetEncoded();

  return dataRequest;
}

template<class Impl>
std::shared_ptr<ZigbeeCommands::AFDataRequest> Serial<Impl>::GetReadAttr(uint16_t addrDev, uint8_t endPoint, uint16_t clusterId, uint16_t attr, bool dir) {
  std::shared_ptr<ZigbeeCommands::AFDataRequest> dataRequest = std::make_shared<ZigbeeCommands::AFDataRequest>();

  dataRequest->DstAddr = addrDev;
  dataRequest->DstEndpoint = endPoint;
  //dataRequest->SrcEndpoint = 1;
  dataRequest->ClusterId = clusterId; //onoff
  dataRequest->TransId = GetNextTransactionCount();
  //dataRequest->Options = 0x30; // ACK, discover route (TODO: do we need ACK if 'default response' comes back?)

  // Note: the ACK comes as a AFDataNotification and it's with 'success' in status even if the sent command is wrong (probably it means only delivery success and nothing more)
  // if I set 'wildcard' for profile id, it does not appear to work!
  // a default response (AFIncomingMsgNotification) comes back with the same zcl transaction id in it (but different seq number, apparently that's counted on the device, not reflects what we send)

  ZigbeeCommands::ZCLFrame zclFrame;

  zclFrame.SetGlobal(true);
  zclFrame.SetDirection(dir); // from client to server!
  zclFrame.TransactionSeq = GetZCLNextTransactionCount();
  zclFrame.cmd = 0x00; // read attr
  zclFrame.payload.resize(2);
  ZigbeeCommands::AFDataRequest::WriteUint16(0, attr, zclFrame.payload);

  dataRequest->Data = zclFrame.GetEncoded();

  return dataRequest;
}

template<class Impl>
bool Serial<Impl>::Ping() {
  try {
    std::vector<uint8_t> response;

    ZigbeeCommands::SysPingSend ping;
    getResponse(ping, response, 0, 1, 15);

    ZigbeeCommands::SysPingResp respPing;
    if (respPing.Decode(response)) {
      _out.printInfo("Info: PING went well, capabilities: 0x" + BaseLib::HelperFunctions::getHexString(respPing.capabilities));

      return true;
    } else
      _out.printDebug("Debug: Couldn't decode PING response: 0x" + BaseLib::HelperFunctions::getHexString(response));
  }
  catch (const std::exception &ex) {
    _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }

  return false;
}

template<class Impl>
bool Serial<Impl>::Reset(bool hardReset) {
  _whileResetting = true;

  try {
    ZigbeeCommands::SysResetRequest resetRequest; // type is 0, hard reset
    resetRequest.type = (hardReset ? 0 : 1);

    ZigbeeCommands::SysResetNotification resetResponse;

    std::vector<uint8_t> response;
    getResponse(resetRequest, response, resetResponse.GetCommandId(), 1, 15); // increased timeout, just in case it takes more until restart

    if (resetResponse.Decode(response)) {
      _out.printInfo("Info: Reset response decoded");

      _whileResetting = false;

      return true;
    } else {
      _out.printDebug("Debug: Couldn't decode reset response: 0x" + BaseLib::HelperFunctions::getHexString(response));
      // still return true, apparently there are 'sticks' out there that do not notify about a reset

      std::this_thread::sleep_for(std::chrono::seconds(10)); // this might be one of the misbehaving sticks, wait longer to be sure it's restarted

      _whileResetting = false;

      return true;
    }
  }
  catch (const std::exception &ex) {
    _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }

  _whileResetting = false;

  return false;
}

template<class Impl>
bool Serial<Impl>::StartOnHoldStick() {
  std::vector<uint8_t> response;

  // what if after reset it stays on hold? Apparently some sticks might do that.
  ZigbeeCommands::UtilGetDeviceInfoRequest infoRequest;
  getResponse(infoRequest, response, 0, 1, 15);

  ZigbeeCommands::UtilGetDeviceInfoResponse infoResponse; // TODO: output more info! Also check it, short address should be 0, we're the coordinator
  if (infoResponse.Decode(response)) {
    _out.printInfo("Info: Info request went well when resetting network, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(infoResponse.status)));
    if (ZigbeeReturnIds::ZSuccess != infoResponse.status)
      return true; // return true on almost all branches, we don't want this prevent the attempt to reset the stick

    if (!(infoResponse.deviceType & 0x1))
      return false; // we cannot deal with a stick that cannot be coordinator

    // if it's not set as coordinator yet (addr 0x0000), or it's set, but it's in some other state than 'on hold' or 'coordinator' or 'coordinator starting'
    // TODO: do more proper checks here to see if the stick needs to be re-initialized
    // maybe use the NV value 0x0F00 (ZNP_HAS_CONFIGURED)?
    if (/*0 != infoResponse.shortAddress ||*/ !(0x00 == infoResponse.deviceState || 0x08 == infoResponse.deviceState || 0x09 == infoResponse.deviceState)) {
      _out.printInfo("Info: Coordinator device when resetting, device state: 0x" + BaseLib::HelperFunctions::getHexString(infoResponse.deviceState));
      //if (0 != infoResponse.shortAddress) _out.printInfo("Info: The device does not have the short addr set to zero");
    } else if (0x0 == infoResponse.deviceState) // is the 'stick' in DEV_HOLD state?
    {
      _out.printInfo("Info: Device on hold when resetting the network, starting...");

      ZigbeeCommands::ZDOStartupFromAppRequest startRequest;
      ZigbeeCommands::ZDOStartupFromAppResponse startResponse;
      getResponse(startRequest, response, 0, 1, 15); // increased timeout, because on cc2530 I've seen it timeout

      if (startResponse.Decode(response)) {
        _out.printInfo(
            std::string("Info: start request response went well when resetting the network, status: ") + ((startResponse.status == 0) ? "Restored network state" : ((startResponse.status == 1) ? "New network state" : "Leave and not started")));
      } else {
        _out.printDebug("Debug: couldn't decode start request response when resetting the network: 0x" + BaseLib::HelperFunctions::getHexString(response));
        return false;
      }
    }
  }

  return true;
}

template<class Impl>
bool Serial<Impl>::ResetNetwork(bool onlyWait) {
  std::vector<uint8_t> response;

  auto lambdaWait = [this](const std::vector<uint8_t> &packet) -> bool {
    ZigbeeCommands::AppCnfBdbStartCommissioningResponse commissioningResponse;
    ZigbeeCommands::ZDOStateChangeNotification zdoStateChangeNotification;
    ZigbeeCommands::AppCnfBdbCommissioningNotification appCnfBdbCommissioningNotification;

    if (commissioningResponse.Decode(packet)) {
      _out.printInfo("Info: Reset Network Wait: Start commissioning request for network formation went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(commissioningResponse.status)));

      if (ZigbeeReturnIds::ZSuccess != commissioningResponse.status) {
        _out.printCritical("Info: Reset Network Wait: Commissioning formation FAILURE response"); // this should be 'printed' no matter what so the user knows about this failure, to retry again

        return true; // bail out, don't wait anymore, we received some failure
      }
    } else if (zdoStateChangeNotification.Decode(packet))
      _out.printInfo("Info: Reset Network Wait: ZDO state change notification, status: 0x" + BaseLib::HelperFunctions::getHexString(zdoStateChangeNotification.state));
    else if (appCnfBdbCommissioningNotification.Decode(packet)) {
      _out.printInfo("Info: Reset Network Wait: Commissioning notification, status: 0x" + BaseLib::HelperFunctions::getHexString(appCnfBdbCommissioningNotification.status));
      if (0 == appCnfBdbCommissioningNotification.status /*|| 0x0D == appCnfBdbCommissioningNotification.status*/) // bail out for success
        return true;
      else if (0x8 == appCnfBdbCommissioningNotification.status)  //bail out not only for success, but for formation failure, too
      {
        _out.printCritical("Info: Reset Network Wait: Commissioning formation FAILURE notification"); // this should be 'printed' no matter what so the user knows about this failure, to retry again
        return true;
      }
    } else
      _out.printInfo("Info: Reset Network Wait: Unknown packet received while waiting for commissioning to finish: 0x" + BaseLib::HelperFunctions::getHexString(packet));

    return false;
  };

  if (!onlyWait) {
    if (_commissioning) // might be possible that it went into commissioning mode by itself, being one of the misbehaving sticks
      onlyWait = true;
    else {
      _commissioning = true;

      /*
      if (!Reset(false))
      {
          _out.printDebug("Debug: Couldn't reset");

          return false;
      }
      */

      std::vector<uint8_t> clearNIB(116, 0);
      if (!SysOsalNVWrite(0x21 /* NIB 'table' */, clearNIB)) {
        _out.printDebug("Debug: Reset Network: Couldn't clear NIB, attempting to clear NVRAM and reset anyway");
      }

      // write startup option to clear NV when reset
      std::vector<uint8_t> val = {0x3};  // STARTOPT_CLEAR_STATE (bit 1) | STARTOPT_CLEAR_CONFIG (bit 0) - bit 7 is ZCD_STARTOPT_CLEAR_NWK_FRAME_COUNTER, don't use it, unless for debug purposes
      if (!SysOsalNVWrite(0x3 /*ZCD_NV_STARTUP_OPTION*/, val))
        return false;

      // reset to clear NV

      if (!Reset(false)) {
        _out.printDebug("Debug: Reset Network: Couldn't reset");

        return false;
      }

      // I don't think I need to actually start it from 'hold' for those settings

      //if (!StartOnHoldStick())
      //    return false;

      // wait some time to be sure it fully restarted, but not too much, the 'start on hold' above waited for a notification
      //std::this_thread::sleep_for (std::chrono::seconds(3));

      //if (!onlyCoordinator)
      {
        // set as coordinator - with our firmware it's a coordinator by default, but just in case it's some other firmware
        val[0] = 0;
        if (!SysOsalNVWrite(0x87 /*ZCD_NV_LOGICAL_TYPE*/, val)) {
          _commissioning = false;

          _out.printDebug("Debug: Reset Network: Couldn't set coordinator type");
          return false;
        }
      }

      // enable concentrator
      // I compiled the firmwares with 'concentrator enabled' but might end up with using one that does not have it enabled by default
      // to be sure, set it here
      val[0] = 0x1;
      if (!SysOsalNVWrite(0x32 /*ZCD_NV_CONCENTRATOR_ENABLE*/, val)) {
        // it's not something critical, so continue after warning
        _out.printDebug("Debug: Reset Network: Couldn't enable concentrator");
      }


      // PAN ID
      if (_settings->all.find("panid") != _settings->all.end()) {
        _out.printInfo("Info: Reset Network: Setting up PAN ID");
        const uint32_t panIDval = std::stoul(_settings->all["panid"]->stringValue, 0, 16);
        std::vector<uint8_t> panID =
            {
                static_cast<uint8_t>(panIDval >> 8),
                static_cast<uint8_t>(panIDval & 0xFF)
            };
        // alternative: use UtilSetPanIDRequest
        if (!SysOsalNVWrite(0x83, panID)) //ZCD_NV_PANID
        {
          _commissioning = false;

          _out.printDebug("Debug: Reset Network: Couldn't set PAN ID");
          return false;
        }
      }

      // set channel mask, for now to channel 13, but let it be configurable

      // alternative: use UtilSetChannelsRequest
      ZigbeeCommands::AppCnfBdbSetChannelRequest setChannelRequest;
      setChannelRequest.isPrimary = 1;
      setChannelRequest.channel = 0x2000;

      if (_settings->all.find("channelsmask") != _settings->all.end()) {
        setChannelRequest.channel = std::stoul(_settings->all["channelsmask"]->stringValue, 0, 16);

        _out.printInfo("Info: Reset Network: Channels mask setting found");
      }

      _out.printInfo("Info: Reset Network: Setting channel mask to 0x" + BaseLib::HelperFunctions::getHexString(setChannelRequest.channel));

      getResponse(setChannelRequest, response, 0, 1, 15);

      ZigbeeCommands::AppCnfBdbSetChannelResponse setChannelResponse;
      if (setChannelResponse.Decode(response)) {
        _out.printInfo("Info: Reset Network: Set channel response went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(setChannelResponse.status)));
        if (ZigbeeReturnIds::ZSuccess != setChannelResponse.status) {
          _commissioning = false;

          return false;
        }
      } else {
        _commissioning = false;

        _out.printDebug("Debug: Reset Network: Couldn't decode set channel response: 0x" + BaseLib::HelperFunctions::getHexString(response));
        return false;
      }

      // disable secondary channel

      setChannelRequest.isPrimary = 0;
      setChannelRequest.channel = 0x00;
      getResponse(setChannelRequest, response, 0, 1, 15);

      if (setChannelResponse.Decode(response)) {
        _out.printInfo("Info: Reset Network: Set secondary channel response went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(setChannelResponse.status)));
        if (ZigbeeReturnIds::ZSuccess != setChannelResponse.status) {
          _commissioning = false;

          return false;
        }
      } else {
        _commissioning = false;

        _out.printDebug("Debug: Reset Network: Couldn't decode set secondary channel response: 0x" + BaseLib::HelperFunctions::getHexString(response));
        return false;
      }

      // deal with network keys

      // TODO: uncomment this when keys can be used, otherwise rely on default

      val = psk;
      if (!SysOsalNVWrite(0x62, val)) // PRECFGKEY
      {
        _commissioning = false;

        return false;
      }

      val = {0}; // 0 only coord defaultKey need to be set, and OTA to set other devices in the network
      // 1 also all devices need to set their defaultKey (the same key), or they can't not join the network
      if (!SysOsalNVWrite(0x63, val)) // PRECFGKEYS_ENABLE
      {
        _commissioning = false;

        return false;
      }

      // ZCD_NV_APS_ALLOW_R19_SECURITY = 0x67, default false

      /*
      // ZigBee Alliance Pre-configured TC Link Key - "ZigBeeAlliance09"
      val = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x5a, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41, 0x6c, 0x6c, 0x69, 0x61, 0x6e, 0x63, 0x65, 0x30, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
      if (!SysOsalNVWrite(0x101, val)) // TCLK_TABLE_START
          return false;
      */

      RegisterForMessages();

      // reset for the new settings to become active
      if (!Reset(true)) {
        _commissioning = false;

        return false;
      }

      if (!StartOnHoldStick()) {
        _commissioning = false;

        return false;
      }

      if (!RegisterEndpoints()) {
        _commissioning = false;

        return false;
      }
    }
  } else
    RegisterForMessages(); // still needing notifications, so ensure it's registered for them

  // start commissioning for network formation - this should be done only when network initialization, not at each startup

  ZigbeeCommands::AppCnfBdbStartCommissioningRequest commissioningRequest;
  commissioningRequest.mode = 0x4; // network formation

  if (onlyWait) {
    _out.printInfo("Info: Reset Network: Waiting for commissioning to finish...");
    getResponseOnlyWait(commissioningRequest, response, lambdaWait, 254);
  } else
    getResponse(commissioningRequest, response, lambdaWait, 1, 254);

  ZigbeeCommands::AppCnfBdbCommissioningNotification appCnfBdbCommissioningNotification;
  if (appCnfBdbCommissioningNotification.Decode(response)) {
    _out.printInfo("Info: Reset Network: Commissioning notification, status: 0x" + BaseLib::HelperFunctions::getHexString(appCnfBdbCommissioningNotification.status));

    if (appCnfBdbCommissioningNotification.status != 0) {
      _commissioning = false;

      return false;
    }
  } else {
    _commissioning = false;

    return false; // if the response is not the above, it was either a timeout or some 'expected' packet with a failure
  }

  // get device info to be sure it's set ok

  ZigbeeCommands::UtilGetDeviceInfoRequest infoRequest;
  getResponse(infoRequest, response, 0, 1, 10);

  ZigbeeCommands::UtilGetDeviceInfoResponse infoResponse;
  if (infoResponse.Decode(response)) {
    _out.printInfo("Info: Reset Network: Info request went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(infoResponse.status)));
    if (0 != infoResponse.shortAddress)
      _out.printInfo("Info: Reset Network: The device does not have the short addr set to zero");

    if (ZigbeeReturnIds::ZSuccess != infoResponse.status) {
      _commissioning = false;

      return false;
    }

    IEEEAddress = infoResponse.IEEEaddress;
  } else {
    _commissioning = false;

    _out.printDebug("Debug: Reset Network: Couldn't decode info request response: 0x" + BaseLib::HelperFunctions::getHexString(response));
    return false;
  }

  IEEEAddress = infoResponse.IEEEaddress;

  // next time restore network state
  std::vector<uint8_t> val = {0x4}; // restore network state
  if (!SysOsalNVWrite(0x3 /*ZCD_NV_STARTUP_OPTION*/, val)) {
    _commissioning = false;

    _out.printDebug("Debug: Reset Network: Couldn't set startup option to restore network state");
    return false;
  }

  _commissioning = false;

  return true;
}

template<class Impl>
void Serial<Impl>::reconnect() {
  try {
    //SoftResetStickWait();

    serialImplementation.Close();
    _initComplete = false;
    if (!serialImplementation.Open()) {
      _out.printWarning("Warning: Could not open interface");

      return;
    }

    _bl->threadManager.join(_initThread);
    _bl->threadManager.start(_initThread, true, &Serial::RetryInit, this);
  }
  catch (const std::exception &ex) {
    _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

template<class Impl>
void Serial<Impl>::reconnectNoClose() {
  try {
    _initComplete = false;
    _bl->threadManager.join(_initThread);
    _bl->threadManager.start(_initThread, true, &Serial::RetryInit, this);
  }
  catch (const std::exception &ex) {
    _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

template<class Impl>
void Serial<Impl>::getResponse(const ZigbeeCommands::MTCmd &cmd, std::vector<uint8_t> &responsePacket, const std::function<bool(const std::vector<uint8_t> &)> &checkFunction, int retries, uint8_t timeoutSecs) {
  getResponse(cmd.GetCmdTypeAndSubsys(), cmd.GetEncoded(), responsePacket, cmd.GetCommandId(), cmd.ExpectsResponse(), 0, retries, timeoutSecs, checkFunction);
}

template<class Impl>
void Serial<Impl>::getResponse(const ZigbeeCommands::MTCmd &cmd, std::vector<uint8_t> &responsePacket, uint8_t expectedAsync, int retries, uint8_t timeoutSecs, const std::function<bool(const std::vector<uint8_t> &)> &checkFunction) {
  getResponse(cmd.GetCmdTypeAndSubsys(), cmd.GetEncoded(), responsePacket, cmd.GetCommandId(), cmd.ExpectsResponse(), expectedAsync, retries, timeoutSecs, checkFunction);
}

template<class Impl>
void Serial<Impl>::getResponseOnlyWait(const ZigbeeCommands::MTCmd &cmd, std::vector<uint8_t> &responsePacket, const std::function<bool(const std::vector<uint8_t> &)> &checkFunction, uint8_t timeoutSecs) {
  const std::vector<uint8_t> requestPacket; // if it's empty, it won't be sent
  getResponse(cmd.GetCmdTypeAndSubsys(), requestPacket, responsePacket, cmd.GetCommandId(), cmd.ExpectsResponse(), 0, 1, timeoutSecs, checkFunction);
}

template<class Impl>
void Serial<Impl>::getResponse(uint8_t packetType,
                               const std::vector<uint8_t> &requestPacket,
                               std::vector<uint8_t> &responsePacket,
                               uint8_t cmd,
                               bool waitForResponse,
                               uint8_t expectedAsync,
                               int retries,
                               uint8_t timeoutSecs,
                               const std::function<bool(const std::vector<uint8_t> &)> &checkFunction) {
  try {
    if (_stopped) {
      _out.printInfo("Info: Trying to send packet: " + BaseLib::HelperFunctions::getHexString(requestPacket) + " but serial is stopped");

      return;
    }

    responsePacket.clear();

    if (_request.waiting)
      _request.waitingEvent.WaitSeconds(timeoutSecs);

    _request.waiting = true;
    _request.Unset();

    std::unique_lock<std::mutex> accessGuard(_request.accessMutex);

    _request.packetType = packetType;
    _request.request = requestPacket;
    _request.response.clear();
    _request.resend = false;
    _request.waitForResponse = waitForResponse;
    _request.cmd = cmd;
    _request.expectedAsync = expectedAsync;
    _request.checkFunction = checkFunction;

    accessGuard.unlock();

    bool failure = true;

    for (int32_t i = 0; failure && i < retries; ++i) {
      try {
        if (!requestPacket.empty())
          rawSend(requestPacket);
      }
      catch (const BaseLib::Exception &ex) {
        _out.printError("Error sending packet: " + std::string(ex.what()));

        _request.Clear();
        accessGuard.unlock();

        _request.waiting = false;
        _request.waitingEvent.SetFlag();
        _request.waitingEvent.NotifyAll();

        return;
      }

      if (waitForResponse) {
        bool responseReceived = true;
        if (!_request.event.WaitSeconds(timeoutSecs)) {
          responseReceived = false;
          _out.printError("Error: No response received to packet: " + BaseLib::HelperFunctions::getHexString(requestPacket));
        }

        accessGuard.lock();

        if (_request.resend) {
          _request.resend = false;
          accessGuard.unlock();

          --i;
          continue;
        } else if (responseReceived) {
          responsePacket = _request.response;
          failure = false;
        }
      }

      _request.Clear();
      accessGuard.unlock();

      _request.waiting = false;
      _request.waitingEvent.SetFlag();
      _request.waitingEvent.NotifyAll();

      break; // for resends, see above
    }
  }
  catch (const std::exception &ex) {
    _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

template<class Impl>
void Serial<Impl>::processPacket(uint32_t addr, uint8_t endPoint, std::vector<uint8_t> &data, int offset, uint8_t LinkQuality) {
  try {
    if (data.size() >= size_t(offset + 2)) {
      //bool trytosend = false;

      // process it and so on

      IZigbeeInterface::processPacket(addr, endPoint, data, offset, LinkQuality);
    }
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

template<class Impl>
void Serial<Impl>::printPacketDescription(bool sent, const std::vector<uint8_t> &data) {
  ZigbeeCommands::MTCmd mtCmd(0);
  if (mtCmd.ForceDecode(data)) {
    const std::string desc = "packet " + BaseLib::HelperFunctions::getHexString(data) + " is " + mtCmd.getDescription();
    if (sent) {
      _out.printInfo("Info: Sent " + desc);
    } else {
      _out.printInfo("Info: Received " + desc);
    }

  }
}

template<class Impl>
void Serial<Impl>::processRawPacket(std::vector<uint8_t> &data) {
  try {
    if (_bl->debugLevel >= 5) {
      if (data.size() > 0) {
        _out.printInfo("Info: Received packet " + BaseLib::HelperFunctions::getHexString(data));
      }
    }

    if (data.size() < 5)
      return;

    if (_bl->debugLevel >= 5) {
      printPacketDescription(false, data);
    }

    bool notify = false;
    {
      std::unique_lock<std::mutex> accessGuard(_request.accessMutex);

      // TODO: remove this, just trying to be sure of something
      //if (_bl->debugLevel >= 5) _out.printInfo("Info: Checking if packet is expected: " + BaseLib::HelperFunctions::getHexString(data));

      if (_request.IsExpectedPacket(data)) {
        _request.response = data;
        _request.resend = false;
        notify = true;
        _out.printInfo("Info: Received expected packet, notifying: " + BaseLib::HelperFunctions::getHexString(data));
      }
    }
    if (notify) {
      _request.Notify();

      // TODO: remove this, just trying to be sure of something
      //if (_bl->debugLevel >= 5) _out.printInfo("Info: Notified about: " + BaseLib::HelperFunctions::getHexString(data));
    }

    ZigbeeCommands::ERROR_MTCmdResponse errorResponse;
    if (errorResponse.Decode(data)) {
      _out.printDebug("Debug: Received error packet for command type and sybsys: 0x" + BaseLib::HelperFunctions::getHexString(errorResponse.GetCmdTypeAndSubsysForError()) + " command id: 0x"
                          + BaseLib::HelperFunctions::getHexString(errorResponse.GetCommandIdForError()) + " error code: 0x" + BaseLib::HelperFunctions::getHexString(errorResponse.errorCode));
      return;
    }

    // TODO: maybe if it's handled in network admin mode, should not deal with packets further (as in calling processPacket)
    // of course, notifications and calling addressChanged should go on, but processPacket maybe should not
    networkAdmin.ProcessAdminPacket(data);

    ZigbeeCommands::ZDOEndDeviceAnnceNotification annce;
    if (annce.Decode(data)) {
      _out.printInfo("Info: Device announce for address 0x" + BaseLib::HelperFunctions::getHexString(annce.srcAddr));

      if (GD::family)
        GD::family->addressChanged(annce.srcIEEEAddr, annce.srcAddr);
    } else {
      // this is a check for an odd case at initialization when the stick goes in commissioning mode by itself
      ZigbeeCommands::AppCnfBdbCommissioningNotification bdbCommissioning;
      if (!_initComplete && bdbCommissioning.Decode(data) && (bdbCommissioning.status == 0x2 /*no network*/ || bdbCommissioning.status == 0x0D /*restored network state*/) && bdbCommissioning.commissioningMode == 0
          && bdbCommissioning.remainingCommissioningModes == 0x0) {
        if (0x2 == bdbCommissioning.status) {
          _out.printInfo("Info: The stick appears to go into commissioning mode by itself");

          _commissioning = true;
        } else {
          _out.printInfo("Info: Commissioning ended up with a network restore status");

          _commissioning = false;
        }
      } else {
        ZigbeeCommands::AFIncomingMsgNotification msgNotification;

        if (msgNotification.Decode(data)) {
          _out.printInfo("Info: Received AF incoming MSG notification: 0x" + BaseLib::HelperFunctions::getHexString(data) + " for src addr: 0x" + BaseLib::HelperFunctions::getHexString(msgNotification.SrcAddr) + " src endpoint: 0x"
                             + BaseLib::HelperFunctions::getHexString(msgNotification.SrcEndpoint) + " cluster: 0x" + BaseLib::HelperFunctions::getHexString(msgNotification.ClusterId));

          auto curpack = curSendingPacket;
          const uint32_t addr = curpack ? curpack->destinationAddress() : 0;

          ZigbeeCommands::ZCLFrame zclFrame;
          if (zclFrame.Decode(msgNotification.Data)) {
            _out.printInfo("Info: It's a ZCL frame: 0x" + BaseLib::HelperFunctions::getHexString(msgNotification.Data) + " for src addr: 0x" + BaseLib::HelperFunctions::getHexString(msgNotification.SrcAddr) + " src endpoint: 0x"
                               + BaseLib::HelperFunctions::getHexString(msgNotification.SrcEndpoint) + " cluster: 0x" + BaseLib::HelperFunctions::getHexString(msgNotification.ClusterId));
            _out.printInfo("Info: ZCL frame type: " + zclFrame.GetDescription());

            // it's a zcl frame, check if it's for current sent packet that expects a response
            // if yes, get it out of the queue and send the next one
            uint32_t epPart = msgNotification.SrcEndpoint;
            epPart <<= 16;
            uint32_t rcvaddr = msgNotification.SrcAddr;
            rcvaddr |= epPart;

            if (curpack && addr == rcvaddr && curpack->expectsRequestedResponse) // is the response for current packet?
            {
              _out.printInfo("Info: It's a response for the current sent packet");

              if (curpack->payload().size() >= 2) {
                const uint16_t ClusterId = ZigbeeCommands::MTCmd::ReadUint16(0, curpack->payload());
                if (ClusterId == msgNotification.ClusterId) // is the response for the corresponding cluster?
                {
                  _out.printInfo("Info: It's a response for the current sent packet cluster");

                  // check against the sent zcl frame
                  std::vector<uint8_t> request(curpack->payload().begin() + 2, curpack->payload().end());

                  ZigbeeCommands::ZCLFrame zclFrameSent;
                  if (zclFrameSent.Decode(request)) {
                    if (zclFrame.IsExpectedResponseTo(zclFrameSent.cmd)) {
                      _out.printInfo("Info: It's the expected response for the current sent packet");

                      if (_bl->debugLevel >= 4 && zclFrame.IsDefaultResponseError(zclFrameSent.cmd))
                        _out.printInfo("Info: Default response error: 0x" + BaseLib::HelperFunctions::getHexString(zclFrame.payload[1]) + " description: " + ZigbeeCommands::ZCLFrame::GetErrorString(zclFrame.payload[1]));

                      {
                        if (!notify) _request.Notify(); // if notify flag is set, was already notified
                        waitingThread.NotifyCmdFinished();
                        std::lock_guard<std::mutex> queueLock(packetsQueueMutex);
                        if (!packetsQueue.empty())
                          packetsQueue.pop_front();
                        curSendingPacket.reset();
                      }


                      // TODO: Check if it's an error and if it is, notify the peer with curpack?

                      // TODO: for the thermostat device, although it reports supporting 0x0A cluster (timer)
                      // when querying attributes it reports the cluster as unsupported, which rises the notification
                      // means the configuration will never be marked as finished
                      // as it's the last in the clusters list, the only problem is that it shows the mandatory timers attrs in a channel
                      // and of course that 'config pending' stays forever
                      // this should be checked to see what can be done about it

                      if (zclFrame.IsDefaultResponseError(zclFrameSent.cmd)) {
                        if (GD::family)
                          GD::family->NotifyError(curpack, zclFrame);
                      }

                      tryToSend(addr, IsWakeupDevice(addr));
                    } else
                      _out.printInfo("Info: It's not the expected response for the current sent packet");
                  } else
                    _out.printDebug("The current sent packet is not a ZCL frame");
                } else
                  _out.printInfo("Info: It's a not response for the current sent packet cluster");
              }
            } else if (curpack)
              _out.printInfo("Info: Not a response for the current sent packet");
          } else
            _out.printInfo("Info: The AF incoming MSG notification is not a ZCL frame");

          // TODO: decide about what info to send further (before having too complex xml devices files)
          // to homegear, and stick to it

          // cluster ID in two bytes followed by the ZCL frame
          std::vector<uint8_t> homegearPacket(2 + msgNotification.Data.size());
          ZigbeeCommands::MTCmd::WriteUint16(0, msgNotification.ClusterId, homegearPacket);
          std::copy(msgNotification.Data.begin(), msgNotification.Data.end(), homegearPacket.begin() + 2);

          processPacket(msgNotification.SrcAddr, msgNotification.SrcEndpoint, homegearPacket, 0, msgNotification.LinkQuality);
        } else {
          ZigbeeCommands::AFDataNotification dataNotification; // this is a confirmation from the host - might not come for 'wakeup' devices...
          // TODO: identify which sent packets would receive back a response (for example attributes requests should get those back!)
          // it's possible to also force an 'ACK' (it's a 'default response') for a packet, but this should be done maybe for packets that do not expect back a response

          if (dataNotification.Decode(data)) {
            _out.printInfo("Info: Received AF data notification, status: 0x" + BaseLib::HelperFunctions::getHexString(dataNotification.status) + " description: " + GetErrorString(dataNotification.status) + " endpoint: 0x"
                               + BaseLib::HelperFunctions::getHexString(dataNotification.EndPoint));

            // TODO: check transaction id and endpoint to match the sent packet, too!
            auto curpack = curSendingPacket;
            const uint32_t addr = curpack ? curpack->destinationAddress() : 0;

            bool match = false;
            if (curpack && curpack->expectsResponse && curpack->IsValidTransId(dataNotification.TransId) && dataNotification.EndPoint == 1/*((addr & 0xFF0000) >> 16)*/) {
              match = true;
            } else if (curpack && curpack->expectsResponse) {
              if (curpack->IsValidTransId(dataNotification.TransId))
                _out.printInfo("Transaction id no match");

              if (dataNotification.EndPoint != 1/*((addr & 0xFF0000) >> 16)*/)
                _out.printInfo("End point no match");
            }

            if (0 != dataNotification.status) {
              // retry!
              // *************************************************************************************************************************************
              _out.printInfo("Notification with failure received, status: 0x" + BaseLib::HelperFunctions::getHexString(dataNotification.status) + " description: " + GetErrorString(dataNotification.status) + " endpoint: 0x"
                                 + BaseLib::HelperFunctions::getHexString(dataNotification.EndPoint));

              // NOTE: for battery devices I received failures with MAC_TRANSACTION_EXPIRED, but not for all packets that got a timeout, so it seems that the failure notification is unreliable
              // the transaction id and endpoint do match the sent packet
              // they come back in batches, after more than 30 seconds, so I think it's worthless to wait for them, it could slow things down a lot
              // the devices that did that (sleeping before the negotiation ended) were very slow to reply to packets

              if (match) {
                // TODO: this may lead to a deadlock if expecting a reply
                // needs more investigation - check out 'getResponse' and the way it's used in 'network management' class

                if (_request.NotifyResend()) // will try to resend 3 times
                {
                  _out.printInfo("Notification with failure received, notified resend");

                  if (curpack && curpack->hasWaitThread) {
                    _out.printInfo("Notification with failure received, current packet has a wait thread");

                    waitingThread.RestartWaitThread(addr, 5);
                  } else
                    _out.printInfo("Notification with failure received, there is no current packet or it has no wait thread");
                } else {
                  _out.printInfo("Notification with failure received, cannot retry");
                  ReceivedResponse(false);
                }
              }

              // *************************************************************************************************************************************
            } else {
              // success, ensure it's for the 'current packet', get it out of the queue and go with the next one
              // but only if it doesn't expect a response!

              // *************************************************************************************************************************************
              _out.printInfo(std::string("Notification with success received, match: ") + (match ? "yes" : "no"));

              if (match && !curpack->expectsRequestedResponse)
                ReceivedResponse(true);

              // *************************************************************************************************************************************
            }
          }
        }

        /*
        else
        {
            // this is unlikely to come, as currently I don't register individually for this, but for all notifications, which come in the form above
            // it's here just in case
            ZigbeeCommands::ZDOMsgCbIncomingNotification msgNotification;

            if (msgNotification.Decode(data))
            {
                // TODO: decide about what info to send further (before having too complex xml devices files)
                // to homegear, and stick to it
                std::vector<uint8_t> homegearPacket(2 + msgNotification.Data.size());
                ZigbeeCommands::MTCmd::WriteUint16(0, msgNotification.ClusterId, homegearPacket);
                std::copy(msgNotification.Data.begin(), msgNotification.Data.end(), homegearPacket.begin() + 2);

                processPacket(msgNotification.SrcAddr, 0, homegearPacket, 0); // the problem is that in this case I don't know the endpoint, so I cannot identify the 'peer'
            }
        }
        */
      }
    }
  }
  catch (const std::exception &ex) {
    _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

template<class Impl>
void Serial<Impl>::ReceivedResponse(bool success) {
  try {
    _out.printInfo(std::string("Received response: ") + (success ? "success" : "failure"));

    // failure or success, notify the writing thread to end as well
    _request.Notify();

    std::unique_lock<std::mutex> lock(accessMutex);

    auto curpack = curSendingPacket;

    if (curpack) {
      _out.printInfo("Received response for the packet: " + BaseLib::HelperFunctions::getHexString(curpack->payload()));

      uint32_t addr = curpack->destinationAddress();

      if (success)
        ++curpack->ack;
      else
        curSendingPacket.reset(); // will remain in the queue, try again

      lock.unlock();

      // end the waiting thread only if it's a canceled send (NACK) or if it's a packet that does not expect a response
      if (!curpack->expectsRequestedResponse || !success) {
        _out.printInfo("Finished dealing with the packet: " + BaseLib::HelperFunctions::getHexString(curpack->payload()) + ", notifying the wait thread to finish...");
        if (curpack->hasWaitThread)
          waitingThread.NotifyCmdFinished();

        curSendingPacket.reset(); //just in case it was stopped for some other reason before
      } else if (curpack->expectsRequestedResponse) {
        _out.printInfo("The packet: " + BaseLib::HelperFunctions::getHexString(curpack->payload()) + " still expects a report response...");
        waitingThread.RestartWaitThread(addr, 5);
      }

      // remove the current packet if send is a success and it's not expecting a response
      if (success && !curpack->expectsRequestedResponse) {
        std::lock_guard<std::mutex> queueLock(packetsQueueMutex);
        if (!packetsQueue.empty())
          packetsQueue.pop_front();
      }

      if (!curpack->expectsRequestedResponse || !success)
        tryToSend(addr, IsWakeupDevice(addr));
    } else
      waitingThread.NotifyCmdFinished(); // stop the waiting thread anyway
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

template<class Impl>
bool Serial<Impl>::tryToSend(uint32_t addr, bool wkupDevice, bool trymore) {
  try {
    if (networkAdmin._networkManagement) {
      _out.printDebug("tryToSend: Nothing done, in network management");
      return false;
    } else if (curSendingPacket) {
      _out.printDebug("tryToSend: Nothing done, currently a sending packet is set");
      return false;
    } else if (!_initComplete) {
      _out.printInfo("Info: tryToSend: Waiting one second, because init is not complete.");
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      if (!_initComplete) {
        _out.printWarning("Warning: tryToSend: !!!Not!!! sending packet, because init is not complete.");
        return false;
      }
    }

    TryToSendJob job(addr, wkupDevice, trymore);
    m_tryToSendWorker.AddJob(job);

    return true;
  }
  catch (const std::exception &ex) {
    _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }

  return false;
}

template<class Impl>
void Serial<Impl>::_tryToSend(uint32_t addr, bool wkupDevice, bool trymore) {
  try {
    std::unique_lock<std::mutex> lock(accessMutex);

    if (curSendingPacket)
      return;

    lock.unlock();

    //PZigbeePacket myPacket = queues.GetPacketFromQueues(addr, wakeup, trymore);
    std::unique_lock<std::mutex> queueLock(packetsQueueMutex);

    PZigbeePacket myPacket;
    while (!packetsQueue.empty()) {
      myPacket = packetsQueue.front();

      queueLock.unlock();
      const bool wakeup = IsWakeupDevice(myPacket->destinationAddress());
      queueLock.lock();

      if (myPacket->ack < (wakeup ? 2 : 3) && myPacket->sendCount < (wakeup ? 2 : 4)) // a 'good' packet, was not sent and acknowledged enough
        break;
      else {
        // tried enough
        if (!packetsQueue.empty())
          packetsQueue.pop_front();

        // TODO: There were timeouts, notify the peer with myPacket?
        if (GD::family) {
          queueLock.unlock();
          GD::family->NotifyTimeout(myPacket);
          queueLock.lock();
        }
      }
    }

    if (!myPacket)
      return;

    queueLock.unlock();

    lock.lock();
    //Send packet
    ++myPacket->sendCount;

    const uint32_t fakeAddr = myPacket->destinationAddress();

    curSendingPacket = myPacket;

    lock.unlock();

    if (myPacket->payload().size() < 2) {
      // remove it from the queue
      queueLock.lock();
      if (!packetsQueue.empty())
        packetsQueue.pop_front();
      curSendingPacket.reset();
      queueLock.unlock();

      tryToSend(addr, wkupDevice);
    } else {
      _out.printInfo("Info: Sending the packet normally");

      // now make up the AF packet and send it
      ZigbeeCommands::AFDataRequest dataRequest;
      dataRequest.DstAddr = fakeAddr & 0xFFFF;
      dataRequest.DstEndpoint = myPacket->GetEndPoint();
      dataRequest.ClusterId = ZigbeeCommands::MTCmd::ReadUint16(0, myPacket->payload());

      dataRequest.TransId = GetNextTransactionCount();

      std::vector<uint8_t> request(myPacket->payload().begin() + 2, myPacket->payload().end());

      bool expectsRequestedResponse = false;

      // decode the payload, see if what's sending is a ZCL frame, in which case overwrite the transaction count
      // then encode it back
      ZigbeeCommands::ZCLFrame zclFrame;
      if (zclFrame.Decode(request)) {
        zclFrame.TransactionSeq = GetZCLNextTransactionCount();
        //if (!wakeup)
        expectsRequestedResponse = zclFrame.ExpectsResponse();
        request = zclFrame.GetEncoded();
      }

      myPacket->expectsResponse = true; // expectsResponse;
      myPacket->expectsRequestedResponse = expectsRequestedResponse;
      myPacket->setPacketNumber(dataRequest.TransId);

      dataRequest.Data = request;

      std::vector<uint8_t> response;
      getResponse(dataRequest, response);

      ZigbeeCommands::AFDataResponse dataResponse;
      if (dataResponse.Decode(response)) {
        if (ZigbeeReturnIds::ZSuccess != dataResponse.status)
          _out.printDebug("Failure status received for sending data request: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(dataResponse.status)));
      } else
        _out.printDebug("Unknown sync response received for sent data request");

      myPacket->hasWaitThread = true;

      waitingThread.WaitForWaitThreadToFinishJob();
      waitingThread.UnsetCmdFinished();
      waitingThread.StartWaiting(fakeAddr, 5);

      _lastPacketSent = BaseLib::HelperFunctions::getTime();
    }
  }
  catch (const std::exception &ex) {
    _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }

  //--detachedThreads;
}

template<class Impl>
bool Serial<Impl>::IsWakeupDevice(uint32_t addr) {
  if (GD::family) {
    return GD::family->IsWakeup(addr);
  }

  return false;
}

template<class Impl>
void Serial<Impl>::sendPacket(std::shared_ptr<BaseLib::Systems::Packet> packet) {
  try {
    PZigbeePacket zigbeePacket = std::static_pointer_cast<ZigbeePacket>(packet);

    const uint32_t addr = zigbeePacket->destinationAddress();
    const bool isWakeup = IsWakeupDevice(addr);

    sendPacket(packet, isWakeup);
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

template<class Impl>
void Serial<Impl>::sendPacket(std::shared_ptr<BaseLib::Systems::Packet> packet, bool wakeup) {
  try {
    PZigbeePacket zigbeePacket = std::static_pointer_cast<ZigbeePacket>(packet);
    if (enqueuePacket(packet, wakeup))
      tryToSend(zigbeePacket->destinationAddress(), wakeup);
  }
  catch (const std::exception &ex) {
    GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }

}

}
