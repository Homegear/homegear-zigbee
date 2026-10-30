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

#ifndef GATEWAY_IMPL_H
#define GATEWAY_IMPL_H

#include <cstdint>

namespace Zigbee {

template<class Impl>
class Serial;

class GatewayImpl {
 public:
  explicit GatewayImpl(Serial<GatewayImpl> *container);
  ~GatewayImpl() {
    if (_tcpSocket) _tcpSocket->Shutdown();
  }

  static bool IsLocal() { return false; }
  static bool IsGateway() { return true; }

  bool IsOpen() const {
    return !IsStopped() && _tcpSocket && _tcpSocket->Connected();
  }

  bool Open() {
    try {
      _tcpSocket->Open();
      if (!_tcpSocket->Connected()) {
        m_container->_out.printError("Error: Could not open device.");
        SetStopped(); // just to be sure
        return false;
      }
      SetStopped(false);

      return true;
    }
    catch (const std::exception &ex) {
      m_container->_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    SetStopped(); // just to be sure

    return false;
  }

  void Reset() {
    C1Net::TcpSocketInfo tcp_socket_info;
    tcp_socket_info.read_timeout = 5000;
    tcp_socket_info.write_timeout = 5000;

    C1Net::TcpSocketHostInfo tcp_socket_host_info{
        .host = m_container->_settings->host,
        .port = (uint16_t)BaseLib::Math::getUnsignedNumber(m_container->_settings->port),
        .tls = true,
        .verify_certificate = true,
        .ca_file = m_container->_settings->caFile,
        .client_cert_file = m_container->_settings->certFile,
        .client_key_file = m_container->_settings->keyFile,
        .connection_retries = 1
    };

    if(m_container->_settings->useIdForHostnameVerification) {
      tcp_socket_host_info.verify_custom_hostname = true;
      tcp_socket_host_info.custom_hostname = m_container->_settings->id;
    }

    _tcpSocket = std::make_unique<C1Net::TcpSocket>(tcp_socket_info, tcp_socket_host_info);
  }

  void Close() {
    if (_tcpSocket) _tcpSocket->Shutdown();
    SetStopped();
  }

  bool IsStopped() const {
    return m_container->_stopped;
  }

  void SetStopped(bool stop = true) {
    m_container->_stopped = stop;
  }

  void EmptyReadBuffers(int tryCount = 30);
  void rawSend(const std::vector<uint8_t> &packet);
  void listen();

 protected:
  Serial<GatewayImpl> *m_container;

  std::unique_ptr<C1Net::TcpSocket> _tcpSocket;
  std::unique_ptr<BaseLib::Rpc::BinaryRpc> _binaryRpc;
  std::unique_ptr<BaseLib::Rpc::RpcEncoder> _rpcEncoder;
  std::unique_ptr<BaseLib::Rpc::RpcDecoder> _rpcDecoder;

  std::mutex _invokeMutex;

  std::mutex _requestMutex;
  std::atomic_bool _waitForResponse;
  std::condition_variable _requestConditionVariable;
  BaseLib::PVariable _rpcResponse;

  BaseLib::PVariable invoke(std::string methodName, BaseLib::PArray &parameters);
  void processPacket(std::vector<uint8_t> &data);
};

}

#endif
