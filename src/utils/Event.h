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

#ifndef EVENT_H
#define EVENT_H

#include <mutex>

namespace ZigbeeUtils
{

class Event
{
public:
    Event() : m_Flag(false) {}

    void NotifyOne()
    {
        m_cv.notify_one();
    }

    void NotifyAll()
    {
        m_cv.notify_all();
    }

    void SetFlag()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_Flag = true;
    }

    void ResetFlag()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_Flag = false;
    }

    void Wait()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (!m_Flag)
            m_cv.wait(lock, [this]{ return m_Flag; });

        m_Flag = false;
    }

    template<typename T> bool Wait(T t)
    {
        auto when = std::chrono::steady_clock::now() + t;

        std::unique_lock<std::mutex> lock(m_mutex);

        if (m_Flag)
        {
            m_Flag = false;

            return true;
        }

        for(;;)
        {
            if (m_cv.wait_until(lock, when) == std::cv_status::timeout)
            {
                const bool res = m_Flag;
                m_Flag = false;

                return res;
            }
            else if (m_Flag)
            {
                m_Flag = false;

                return true;
            }
            // else it's spurious
        }

        // this is unreachable, but keep compiler happy

        return false;
    }

    bool WaitSeconds(int seconds)
    {
        return Wait(std::chrono::seconds(seconds));
    }

    bool WaitMilliseconds(int ms)
    {
        return Wait(std::chrono::milliseconds(ms));
    }

protected:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_Flag;
};

}

#endif
