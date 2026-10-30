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

#ifndef SERIAL_H
#define SERIAL_H

#include <cstdint>

#include <map>
#include <string>
#include <functional>

#include "IZigbeeInterface.h"
#include "../ZigbeePacket.h"
#include "ZigbeeIds.h"

#include "SerialAdmin.h"

#include "../GD.h"
#include "../utils/Threading.h"
#include "../utils/Event.h"
#include "../utils/CallbackTimer.h"

#include "MT.h"

namespace Zigbee
{

template<class Impl> class Serial : public IZigbeeInterface
{
    friend Impl;
    friend class SerialAdmin<Serial>;
public:
    explicit Serial(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings);
    virtual ~Serial();

    virtual void startListening() override;
    virtual void stopListening() override;
    virtual void setup(int32_t userID, int32_t groupID, bool setPermissions);

    virtual bool PairOn(int seconds) override
    {
        return networkAdmin.PairOn(seconds);
    }

    virtual bool PairOff(uint64_t IEEEaddr, uint16_t shortAddr, int seconds) override
    {
        return networkAdmin.PairOff(IEEEaddr, shortAddr, seconds);
    }

    virtual bool AbortInclusion(uint16_t nodeId = 0x0) override
    {
        return networkAdmin.AbortInclusion(nodeId);
    }

    virtual void NetworkReset() override
    {
        networkAdmin.NetworkReset();
    }

    virtual bool isOpen() override
    {
        return serialImplementation.IsOpen();
    }

    virtual void sendPacket(std::shared_ptr<BaseLib::Systems::Packet> packet) override;
    virtual void sendPacket(std::shared_ptr<BaseLib::Systems::Packet> packet, bool wakeup) override;

    virtual bool enqueuePacket(std::shared_ptr<BaseLib::Systems::Packet> packet, bool wakeupDevice) override
    {
        PZigbeePacket zigbeePacket = std::static_pointer_cast<ZigbeePacket>(packet);
        if (!zigbeePacket) return false;

        // TODO: for now just do this, but later the simple queing might change
        std::lock_guard<std::mutex> lock(packetsQueueMutex);

        if (packetsQueue.size() >= 1000)
        {
            _out.printError("Error: Too many sent packets are queued to be sent. Dropping packet.");

            return false;
        }

        if (!zigbeePacket->packetID.empty())
        {
            packetsQueue.erase(std::remove_if(packetsQueue.begin(), packetsQueue.end(),
                       [&zigbeePacket](PZigbeePacket& colPacket){ return zigbeePacket->packetID == colPacket->packetID  && zigbeePacket->destinationAddress() == colPacket->destinationAddress(); }),
                       packetsQueue.end());
        }


        zigbeePacket->setPacketNumber(_packetNumber++);
        if (0 == _packetNumber) _packetNumber = 1;

        packetsQueue.push_back(zigbeePacket);

        return true;
    }

    virtual bool tryToSend(uint32_t addr, bool wakeupDevice, bool trymore = false) override;

    void RemoveSentPacket(PZigbeePacket packet, bool successSending);

    virtual bool IsInNetworkAdminMode() const override
    {
        return networkAdmin._networkManagement;
    }
    virtual double TimeSinceLastAdminStage() override
    {
        return networkAdmin.GetTimeSinceLastStage();
    }



    static void RemoveCrc(std::vector<uint8_t>& response)
    {
        const int len = response.size();
        if (len > 0)
            response.resize(len - 1); // take the crc out, it was already checked
    }

    bool ResetStick()
    {
        if (!ResetNetwork())
            return false;

        if (!RegisterForMessages())
            return false;

        // TODO: for configuration, use APP_CNF_BDB_SET_TC_REQUIRE_KEY_EXCHANGE to allow or not devices to join unsecurely (this allows legacy devices to join)
        // currently this is done in the firmware by setting BDB_DEFAULT_TC_REQUIRE_KEY_EXCHANGE=FALSE
        // here we can override that with some preference perhaps set in config file

        ZigbeeCommands::AppCnfBdbSetTCRequireKeyExchangeRequest TCRequireKeyExchangeRequest;

        std::vector<uint8_t> response;
        getResponse(TCRequireKeyExchangeRequest, response);

        ZigbeeCommands::AppCnfBdbSetTCRequireKeyExchangeResponse TCRequireKeyExchangeResponse;
        if (TCRequireKeyExchangeResponse.Decode(response))
        {
            _out.printInfo("Info: TC Require Key Exchange went well, status: 0x" + BaseLib::HelperFunctions::getHexString(static_cast<uint8_t>(TCRequireKeyExchangeResponse.status)));
        }
        else
        {
            _out.printInfo("Info: Couldn't decode TC Require Key Exchange response: 0x" + BaseLib::HelperFunctions::getHexString(response));
        }

        return true;
    }


    void CleanCmdQueues()
    {
        std::lock_guard<std::mutex> lock(packetsQueueMutex);

        packetsQueue.clear();
    }

    static std::string GetErrorString(uint8_t code);

protected:

    bool RegisterForMessages()
    {
        _out.printInfo("Info: Registering for receiving notifications");

        // register to receive ZDO messages - other way: use UTIL_CALLBACK_SUB_CMD
        std::vector<uint8_t> val = {0x1}; // all messages, otherwise ZDO_MSG_CB_REGISTER must be used for subscribing to specific ZDO callbacks
        if (!SysOsalNVWrite(0x8F, val)) // ZCD_NV_ZDO_DIRECT_CB
            return false;

        return true;
    }

    bool Ping();
    bool Reset(bool hardReset = true);

    bool ResetNetwork(bool onlyWait = false);
    bool StartOnHoldStick();


    class Request
    {
    public:
        std::mutex accessMutex;
        uint8_t packetType = 0;
        uint8_t expectedAsync = 0;

        ZigbeeUtils::Event event;

        bool resend = false;
        std::vector<uint8_t> request;
        std::vector<uint8_t> response;

        bool waitForResponse = false;
        uint8_t cmd = 0;
        uint8_t nrResends = 0;

        std::atomic_bool waiting;
        ZigbeeUtils::Event waitingEvent;

        std::function<bool(const std::vector<uint8_t>&)> checkFunction;

        Request() : waiting(false) {}
        virtual ~Request() {}

        void Notify()
        {
            event.SetFlag();
            event.NotifyAll();

            // ensure notification is received and there is no more waiting for the packet!
            //if (waiting)
            //    waitingEvent.WaitMilliseconds(1000);
        }

        bool NotifyResend()
        {
            bool result = false;

            accessMutex.lock();

            if (nrResends < 3) // only 3 retransmissions as in 6.3 retransmission in Serial API Host Appl. Prg. Guide
            {
                resend = true;
                ++nrResends;
                result = true;
            }
            else
            {
                resend = false;
                nrResends = 0;
            }
            accessMutex.unlock();

            Notify();

            return result;
        }

        void Unset()
        {
            waitingEvent.ResetFlag();
            event.ResetFlag();
        }

        void Clear()
        {
            //std::lock_guard<std::mutex> lock(accessMutex);
            packetType = 0;
            expectedAsync = 0;
            waitForResponse = false;
            cmd = 0;
            nrResends = 0;
            request.clear();
            response.clear();
            checkFunction = nullptr;
        }

        // lock this from outside before calling
        bool IsExpectedPacket(const std::vector<uint8_t>& packet)
        {
            if (packet.size() < 4 && !packet.empty()) return false; // if the packet is empty, might be an 'only wait'

            if (checkFunction) return checkFunction(packet);

            const uint8_t respType = (packet[2] & MT_TYPE_MASK);
            const uint8_t subsys = (packet[2] & SUBSYS_MASK);

            const uint8_t sentType = (packetType & MT_TYPE_MASK);
            const uint8_t expectedSubsys = (packetType & SUBSYS_MASK);

            const uint8_t recvCmd = packet[3];

            if (recvCmd != cmd && AREQ != respType) return false;
            else if (subsys != expectedSubsys) return false;
            else if (SREQ == sentType && SRSP == respType && 0 == expectedAsync) return true;
            else if (AREQ == respType && expectedAsync == packet[3]) return true;

            return false;
        }
    };

    Request _request;

    std::atomic_bool _initComplete;
    std::atomic_bool _commissioning;

    std::atomic_bool _whileResetting;

    std::thread _initThread;

    Impl serialImplementation;

    std::mutex accessMutex;
    PZigbeePacket curSendingPacket;

    SerialAdmin<Serial<Impl>> networkAdmin;

    //********************************************************************************************************************************************

    uint64_t IEEEAddress;
    std::atomic<uint8_t> transactionCount;

    uint8_t GetNextTransactionCount()
    {
        return ++transactionCount;
    }

    std::atomic<uint8_t> zclTransactionCount;

    uint8_t GetZCLNextTransactionCount()
    {
        return ++zclTransactionCount;
    }

    typedef ZigbeeUtils::WorkerThreadsPool<Serial, std::vector<uint8_t>> ProcessPacketsThreadsPool;
    friend ProcessPacketsThreadsPool;

    ProcessPacketsThreadsPool m_processPacketsThreadsPool;

    void AddPacketToProcessQueue(const std::vector<uint8_t>& data)
    {
        m_processPacketsThreadsPool.AddJob(data);
    }

    void ProcessJob(std::vector<uint8_t>& packet)
    {
        processRawPacket(packet);
    }

    //********************************************************************************************************************************************

    void RetryInit();

    bool HandleSpecialFunction(const std::vector<uint8_t>& data);

    virtual void processPacket(uint32_t addr, uint8_t endPoint, std::vector<uint8_t>& data, int offset, uint8_t LinkQuality) override;

    void getResponse(const ZigbeeCommands::MTCmd& cmd, std::vector<uint8_t>& responsePacket, uint8_t expectedAsync = 0, int retries = 1, uint8_t timeoutSecs = 5, const std::function<bool(const std::vector<uint8_t>&)>& checkFunction = nullptr);
    void getResponse(const ZigbeeCommands::MTCmd& cmd, std::vector<uint8_t>& responsePacket, const std::function<bool(const std::vector<uint8_t>&)>& checkFunction, int retries = 1, uint8_t timeoutSecs = 5);

    void getResponse(uint8_t packetType, const std::vector<uint8_t>& requestPacket, std::vector<uint8_t>& responsePacket, uint8_t cmd = 0, bool waitForResponse = false, uint8_t expectedAsync = 0, int retries = 1, uint8_t timeoutSecs = 5, const std::function<bool(const std::vector<uint8_t>&)>& checkFunction = nullptr);

    // this does not send the command, only waits for responses - needed for already started commissioning at initialization
    void getResponseOnlyWait(const ZigbeeCommands::MTCmd& cmd, std::vector<uint8_t>& responsePacket, const std::function<bool(const std::vector<uint8_t>&)>& checkFunction, uint8_t timeoutSecs = 5);

    bool init();
    void reconnect();
    void reconnectNoClose();
    void processRawPacket(std::vector<uint8_t>& data);



    //********************************************************************************************************************************************

    class TryToSendJob
    {
    public:
        TryToSendJob(uint32_t addr, bool wakeupDevice, bool trymore)
            : m_addr(addr), m_wakeupDevice(wakeupDevice), m_trymore(trymore)
        {
        }

        uint32_t m_addr;
        bool m_wakeupDevice;
        bool m_trymore;
    };


    typedef ZigbeeUtils::WorkerThread<Serial, TryToSendJob> TryToSendWorker;
    friend TryToSendWorker;

    TryToSendWorker m_tryToSendWorker;

    void ProcessJob(const TryToSendJob& job)
    {
        _tryToSend(job.m_addr, job.m_wakeupDevice, job.m_trymore);
    }

    void _tryToSend(uint32_t addr, bool wakeupDevice, bool trymore);

    //********************************************************************************************************************************************

    // might turn this into a generic class

    class WaitingThread
    {
    public:

        explicit WaitingThread(Serial* parent)
        : _stopCallbackThread(false), m_parent(parent)
        {
        }

        ~WaitingThread()
        {
            Stop();
        }

        void Start()
        {
            m_parent->_bl->threadManager.start(waitingThread, true, &WaitingThread::waitForCmdThread, this);
        }

        void Stop()
        {
            _stopCallbackThread = true;

            if (waitingThread.joinable())
            {
                waitThreadWaitingEvent.SetFlag();
                waitThreadWaitingEvent.NotifyOne();

                StartWaiting(0, 0);

                waitThreadCv.notify_all();
                NotifyCmdFinished();
                m_parent->_bl->threadManager.join(waitingThread);
            }
        }

        class WaitJob
        {
        public:
            WaitJob() : m_addr(0), m_seconds(5) {}
            WaitJob(uint32_t addr, int seconds = 5) : m_addr(addr), m_seconds(seconds) {}


            int GetSeconds() const
            {
                return m_seconds;
            }

            uint32_t m_addr;
            int m_seconds;
        };



        void NotifyCmdFinished()
        {
            cmdFinishedEvent.SetFlag();
            cmdFinishedEvent.NotifyAll();
        }

        void UnsetCmdFinished()
        {
            cmdFinishedEvent.ResetFlag();
        }

        void WaitForWaitThreadToFinishJob()
        {
            waitThreadWaitingEvent.Wait();
        }

        void StartWaiting(uint32_t addr, int seconds = 5)
        {
            WaitJob newJob(addr, seconds);
            StartWaiting(newJob);
        }

        void StartWaiting(const WaitJob& job)
        {
            {
                std::lock_guard<std::mutex> lck(waitThreadMutex);
                waitJob = job;
                waitWakeup = true;
            }
            UnsetCmdFinished();
            waitThreadCv.notify_one(); // 'restarts' the waiting thread
        }

        void NotifyReady()
        {
            waitThreadWaitingEvent.SetFlag();
            waitThreadWaitingEvent.NotifyOne();
        }

        void RestartWaitThread(uint32_t addr, int seconds)
        {
            m_parent->_out.printInfo("Restarting the waiting thread, or else it might timeout");

            NotifyCmdFinished(); // finish the waiting thread, or else it might timeout
            WaitForWaitThreadToFinishJob();
            UnsetCmdFinished();
            StartWaiting(addr, seconds);
            m_parent->_out.printInfo("Restarted");
        }

        void waitForCmdThread()
        {
            WaitJob theJob;

            while (!_stopCallbackThread)
            {
                NotifyReady();

                // waits for a 'waiting' job
                {
                    std::unique_lock<std::mutex> waitThreadLock(waitThreadMutex);
                    if (!waitWakeup)
                        waitThreadCv.wait(waitThreadLock, [this] {return waitWakeup;});
                    waitWakeup = false;

                    theJob = waitJob;
                }

                if (_stopCallbackThread)
                    return;

                // executes the 'waiting' job

                m_parent->_out.printInfo("Waiting thread started");

                if (!cmdFinishedEvent.WaitSeconds(theJob.m_seconds))
                {
                    // was a timeout
                    m_parent->curSendingPacket.reset();

                    if (_stopCallbackThread)
                        return;

                    m_parent->_out.printInfo("Waiting thread timeout");

                    m_parent->tryToSend(theJob.m_addr, true);
                }
                else
                {
                    m_parent->_out.printInfo("Waiting thread stopped");
                }
            }
        }


        ZigbeeUtils::Event cmdFinishedEvent;

    protected:

        std::mutex waitThreadMutex;
        std::condition_variable waitThreadCv;
        bool waitWakeup{false};
        WaitJob waitJob;

        ZigbeeUtils::Event waitThreadWaitingEvent;

        std::atomic_bool _stopCallbackThread;

        Serial* m_parent;
        std::thread waitingThread;
    };

    WaitingThread waitingThread;

    //********************************************************************************************************************************************

    // for now use a single queue, there are some unsolved issues with devices that are not listening, this might change in the future
    typedef std::list<PZigbeePacket> PacketsQueue;

    std::mutex packetsQueueMutex;
    PacketsQueue packetsQueue;
    std::atomic_uint _packetNumber;

    virtual void rawSend(const std::vector<uint8_t>& packet) override
    {
        serialImplementation.rawSend(packet);

        if (_bl->debugLevel >= 5)
        {
            printPacketDescription(true, packet);
        }
    }

    void sendMagicByte(); // see: https://github.com/Koenkk/zigbee2mqtt/issues/1343

    bool SysOsalNVWrite(uint16_t id, const std::vector<uint8_t>& value);
    bool SysOsalNVRead(uint16_t id, std::vector<uint8_t>& value);

    bool RegisterEndpoints();

    bool IsWakeupDevice(uint32_t addr);

    bool CreateNode(uint16_t nodeId);
    void RequestNodeInfo(uint16_t nodeId);

    void saveSettingToDatabase(std::string setting, std::vector<uint8_t>& value);

    void printPacketDescription(bool sent, const std::vector<uint8_t>& data);

    static uint8_t type(const std::vector<uint8_t>& data)
    {
        return data.size() > 2 ? (data.at(2) & MT_TYPE_MASK) : 0;
    }

    static uint8_t subsystem(const std::vector<uint8_t>& data)
    {
        return data.size() > 2 ? (data.at(2) & SUBSYS_MASK) : 0;
    }

    static uint8_t function(const std::vector<uint8_t>& data)
    {
        return data.size() > 3 ? data.at(3) : 0;
    }

    void ReceivedResponse(bool success);

    // by default, dir is 'from client to server'
    ZigbeeCommands::AFDataRequest GetSimpleCommand(uint16_t addrDev, uint8_t endPoint, uint16_t clusterId, uint8_t cmd, bool dir = false);
    std::shared_ptr<ZigbeeCommands::AFDataRequest> GetReadAttr(uint16_t addrDev, uint8_t endPoint, uint16_t clusterId, uint16_t attr, bool dir = false);

    static const std::map<uint8_t, std::string> errorCodesMap;
};

}

#ifndef _SERIAL_HPP
#include "Serial.hpp"
#endif

#endif
