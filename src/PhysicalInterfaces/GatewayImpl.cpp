/* Copyright 2013-2019 Homegear GmbH
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

#include "Serial.h"
#include "GatewayImpl.h"

namespace Zigbee {

GatewayImpl::GatewayImpl(Serial<GatewayImpl> *container)
    : m_container(container), _waitForResponse(false) {
  _binaryRpc.reset(new BaseLib::Rpc::BinaryRpc(m_container->_bl));
  _rpcEncoder.reset(new BaseLib::Rpc::RpcEncoder(m_container->_bl, true, true));
  _rpcDecoder.reset(new BaseLib::Rpc::RpcDecoder(m_container->_bl, false, false));
}

void GatewayImpl::EmptyReadBuffers(int tryCount) {
  try {
    if (!IsOpen())
      return;

    m_container->_out.printInfo("Info: Gateway: Emptying read buffers");

    BaseLib::PArray parameters = std::make_shared<BaseLib::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<BaseLib::Variable>(ZIGBEE_FAMILY_ID));
    parameters->push_back(std::make_shared<BaseLib::Variable>(tryCount));

    auto result = invoke("emptyReadBuffers", parameters);
    if (result->errorStruct) {
      m_container->_out.printError("Gateway: Error sending emptyReadBuffers, error: " + result->structValue->at("faultString")->stringValue);
    }
  }
  catch (const std::exception &ex) {
    m_container->_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

void GatewayImpl::rawSend(const std::vector<uint8_t> &packet) {
  try {
    if (!_tcpSocket || !_tcpSocket->Connected())
      return;

    BaseLib::PArray parameters = std::make_shared<BaseLib::Array>();
    parameters->reserve(2);
    parameters->push_back(std::make_shared<BaseLib::Variable>(ZIGBEE_FAMILY_ID));

    std::vector<uint8_t> pcopy = packet; // workaround for Variable parameter not being a const
    parameters->push_back(std::make_shared<BaseLib::Variable>(pcopy));

    if (m_container->_bl->debugLevel >= 4)
      m_container->_out.printInfo("Info: Gateway: Sending: " + m_container->_bl->hf.getHexString(packet));

    auto result = invoke("sendPacket", parameters);
    if (result->errorStruct) {
      m_container->_out.printError("Gateway: Error sending packet " + m_container->_bl->hf.getHexString(packet) + ": " + result->structValue->at("faultString")->stringValue);
    }
  }
  catch (const std::exception &ex) {
    m_container->_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

void GatewayImpl::listen() {
  try {
    if (!IsOpen()) {
      try {
        _tcpSocket->Open();
        if (_tcpSocket->Connected()) {
          m_container->_out.printInfo("Info: Gateway: Successfully connected.");
          SetStopped(false);
        }
      }
      catch (const std::exception &ex) {
        m_container->_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
      }
    }

    std::vector<char> buffer(1024);
    int32_t processedBytes = 0;
    bool more_data = false;
    while (!m_container->_stopCallbackThread) {
      try {
        if (!IsOpen()) {
          if (m_container->_stopCallbackThread)
            return;
          if (IsStopped())
            m_container->_out.printWarning("Warning: Gateway: Connection to device closed. Trying to reconnect...");
          _tcpSocket->Shutdown();
          std::this_thread::sleep_for(std::chrono::milliseconds(1000));
          _tcpSocket->Open();
          if (_tcpSocket->Connected()) {
            m_container->_out.printInfo("Info: Gateway: Successfully connected.");
            SetStopped(false);

            if (!m_container->_stopCallbackThread)
              m_container->reconnectNoClose();
          }
          continue;
        }

        int32_t bytesRead = 0;
        try {
          bytesRead = _tcpSocket->Read((uint8_t *)buffer.data(), buffer.size(), more_data);
        }
        catch (C1Net::TimeoutException &ex) {
          continue;
        }
        if (bytesRead <= 0)
          continue;
        if (bytesRead > 1024)
          bytesRead = 1024;

        if (GD::bl->debugLevel >= 5)
          m_container->_out.printDebug("Debug: Gateway: TCP packet received: " + BaseLib::HelperFunctions::getHexString(buffer.data(), bytesRead));

        processedBytes = 0;
        while (processedBytes < bytesRead) {
          try {
            processedBytes += _binaryRpc->process(buffer.data() + processedBytes, bytesRead - processedBytes);
            if (_binaryRpc->isFinished()) {
              if (_binaryRpc->getType() == BaseLib::Rpc::BinaryRpc::Type::request) {
                std::string method;
                BaseLib::PArray parameters = _rpcDecoder->decodeRequest(_binaryRpc->getData(), method);

                if (method == "packetReceived" && parameters && parameters->size() == 2 && parameters->at(0)->integerValue64 == ZIGBEE_FAMILY_ID && !parameters->at(1)->binaryValue.empty()) {
                  m_container->_out.printInfo("Info: Gateway: Received packetReceived");
                  processPacket(parameters->at(1)->binaryValue);
                } else if (method == "reconnect" && parameters && parameters->size() == 1 && parameters->at(0)->integerValue64 == ZIGBEE_FAMILY_ID) {
                  m_container->_out.printInfo("Info: Gateway: Received reconnect");
                  if (!m_container->_stopCallbackThread)
                    m_container->reconnectNoClose();
                }

                BaseLib::PVariable response = std::make_shared<BaseLib::Variable>();
                std::vector<uint8_t> data;
                _rpcEncoder->encodeResponse(response, data);
                _tcpSocket->Send(data);
              } else if (_binaryRpc->getType() == BaseLib::Rpc::BinaryRpc::Type::response && _waitForResponse) {
                std::unique_lock<std::mutex> requestLock(_requestMutex);
                _rpcResponse = _rpcDecoder->decodeResponse(_binaryRpc->getData());
                requestLock.unlock();
                _requestConditionVariable.notify_all();
              }
              _binaryRpc->reset();
            }
          }
          catch (BaseLib::Rpc::BinaryRpcException &ex) {
            _binaryRpc->reset();
            m_container->_out.printError("Gateway: Error processing packet: " + std::string(ex.what()));
          }
        }
      }
      catch (const std::exception &ex) {
        SetStopped();
        m_container->_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
      }
    } // while
  }
  catch (const std::exception &ex) {
    m_container->_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

PVariable GatewayImpl::invoke(std::string methodName, PArray &parameters) {
  try {
    m_container->_out.printInfo("Info: Gateway: Invoking " + methodName);

    std::lock_guard<std::mutex> invokeGuard(_invokeMutex);

    std::unique_lock<std::mutex> requestLock(_requestMutex);
    _rpcResponse.reset();
    _waitForResponse = true;

    std::vector<uint8_t> encodedPacket;
    _rpcEncoder->encodeRequest(methodName, parameters, encodedPacket);

    int32_t i = 0;
    for (i = 0; i < 5; i++) {
      try {
        _tcpSocket->Send(encodedPacket);
        break;
      }
      catch (const C1Net::Exception &ex) {
        m_container->_out.printError("Error: " + std::string(ex.what()));
        if (i == 5)
          return BaseLib::Variable::createError(-32500, ex.what());
        _tcpSocket->Open();
      }
    }

    auto when = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
    i = 0;
    for (;;) {
      if (_requestConditionVariable.wait_until(requestLock, when) == std::cv_status::timeout) {
        ++i;

        if (_rpcResponse || i == 10 || IsStopped())
          break;
      } else if (_rpcResponse || IsStopped()) {
        break;
      }
      // else it's spurious, ignore it
    }

    _waitForResponse = false;
    if (!_rpcResponse)
      return BaseLib::Variable::createError(-32500, "No RPC response received.");

    return _rpcResponse;
  }
  catch (const std::exception &ex) {
    m_container->_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }

  return BaseLib::Variable::createError(-32500, "Unknown application error. See log for more details.");
}

void GatewayImpl::processPacket(std::vector<uint8_t> &data) {
  try {
    m_container->AddPacketToProcessQueue(data);
  }
  catch (const std::exception &ex) {
    m_container->_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
  }
}

}
