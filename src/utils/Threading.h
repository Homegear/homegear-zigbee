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

#ifndef THREADING_H
#define THREADING_H

#include <queue>
#include <mutex>
#include <thread>

namespace ZigbeeUtils
{

template<class ProcessorType, typename JobType> class WorkerThreadBase
{
public:
    explicit WorkerThreadBase(ProcessorType* processor)
    : m_stop(false), m_processor(processor)
    {
    }

protected:

    void SetStop()
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_stop = true;
    }

    bool m_stop;

    ProcessorType *m_processor;

    std::condition_variable m_cv;
    std::mutex m_mutex;
    std::queue<JobType> m_jobsQueue;
};


template<class ProcessorType, typename JobType> class WorkerThread : public WorkerThreadBase<ProcessorType, JobType>
{
public:
    typedef WorkerThreadBase<ProcessorType, JobType> Base;

    explicit WorkerThread(ProcessorType* processor, bool start = false)
    : Base(processor)
    {
        if (start) Start();
    }

    ~WorkerThread()
    {
        Stop();
    }

    void Start()
    {
        {
            std::lock_guard<std::mutex> lk(Base::m_mutex);
            Base::m_stop = false;
        }
        Zigbee::GD::bl->threadManager.start(m_thread, true, &WorkerThread::ThreadFunction, this);
    }

    void Stop()
    {
        Base::SetStop();
        Base::m_cv.notify_one();

        Zigbee::GD::bl->threadManager.join(m_thread);
    }

    void AddJob(const JobType& job)
    {
        {
            std::lock_guard<std::mutex> lk(Base::m_mutex);
            Base::m_jobsQueue.push(job);
        }
        Base::m_cv.notify_one();
    }

protected:

    bool TerminateWait() const
    {
        return !Base::m_jobsQueue.empty() || Base::m_stop;
    }

    void ThreadFunction()
    {
        do
        {
            std::unique_lock<std::mutex> lk(Base::m_mutex);
            Base::m_cv.wait(lk, [this]{ return TerminateWait(); });

            if (Base::m_stop) break;

            while(!Base::m_jobsQueue.empty() && !Base::m_stop)
            {
                JobType job(std::move(Base::m_jobsQueue.front()));
                Base::m_jobsQueue.pop();
                lk.unlock();

                Base::m_processor->ProcessJob(job); // TODO: make it more flexible?

                lk.lock();
            }

            lk.unlock();
        }
        while(true);
    }

    std::thread m_thread;
};


template<class ProcessorType, typename JobType, unsigned int initialNumber = 1, unsigned int maxWorkerThreads = 4> class WorkerThreadsPool : public WorkerThreadBase<ProcessorType, JobType>
{
public:
    typedef WorkerThreadBase<ProcessorType, JobType> Base;

    WorkerThreadsPool(ProcessorType* processor, bool start = false)
    : Base(processor)
    {
        if (start) Start();
    }

    ~WorkerThreadsPool()
    {
        Stop();
    }

    void Start()
    {
        std::lock_guard<std::mutex> lk(Base::m_mutex);
        Base::m_stop = false;

        m_threadsPool.reserve(maxWorkerThreads); // with some room to grow
        for (unsigned int i = 0; i < initialNumber; ++i)
        {
            std::thread theThread;
            Zigbee::GD::bl->threadManager.start(theThread, true, &WorkerThreadsPool::ThreadFunction, this);
            m_threadsPool.emplace_back(std::move(theThread));
        }
    }

    void Stop()
    {
        Base::SetStop();
        Base::m_cv.notify_all();

        for (std::thread& t : m_threadsPool)
            if (t.joinable()) Zigbee::GD::bl->threadManager.join(t);

        m_threadsPool.clear();
    }

    void AddJob(const JobType& job)
    {
        {
            std::lock_guard<std::mutex> lk(Base::m_mutex);
            Base::m_jobsQueue.push(job);

            const unsigned int threadsNr = m_threadsPool.size();
            if (threadsNr < maxWorkerThreads && Base::m_jobsQueue.size() > threadsNr - m_jobsRunning)
            {
                std::thread theThread;
                Zigbee::GD::bl->threadManager.start(theThread, true, &WorkerThreadsPool::ThreadFunction, this);
                m_threadsPool.emplace_back(std::move(theThread));
            }
        }

        Base::m_cv.notify_one();
    }


protected:

    bool TerminateWait() const
    {
        return !Base::m_jobsQueue.empty() || Base::m_stop;
    }

    void ThreadFunction()
    {
        do
        {
            std::unique_lock<std::mutex> lk(Base::m_mutex);
            Base::m_cv.wait(lk, [this]{ return TerminateWait(); });

            if (Base::m_stop) break;

            JobType job(std::move(Base::m_jobsQueue.front()));
            Base::m_jobsQueue.pop();

            ++m_jobsRunning;
            lk.unlock();

            Base::m_processor->ProcessJob(job); // TODO: make it more flexible?

            --m_jobsRunning;
        }
        while(true);
    }

    std::vector<std::thread> m_threadsPool; // will increase as much as needed
    std::atomic_int m_jobsRunning{0};
};

}

#endif
