/******************************************************************************
 * Icinga 2                                                                   *
 * Copyright (C) 2012-2013 Icinga Development Team (http://www.icinga.org/)   *
 *                                                                            *
 * This program is free software; you can redistribute it and/or              *
 * modify it under the terms of the GNU General Public License                *
 * as published by the Free Software Foundation; either version 2             *
 * of the License, or (at your option) any later version.                     *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program; if not, write to the Free Software Foundation     *
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ******************************************************************************/

#include "base/thinmutex.hpp"
#include "base/initialize.hpp"
#include "base/timer.hpp"
#include "base/convert.hpp"
#include "base/logger_fwd.hpp"
#include <boost/thread.hpp>

using namespace icinga;

uintptr_t ThinMutex::m_TotalMutexes;
uintptr_t ThinMutex::m_InflatedMutexes;
uintptr_t ThinMutex::m_DeadMutexes;

static Timer::Ptr l_Timer;

static void ThinMutexTimerHandler(void)
{
	Log(LogInformation, "base", "Mutexes: " + Convert::ToString(ThinMutex::m_TotalMutexes) +
	    ", Inflated Mutexes: " + Convert::ToString(ThinMutex::m_InflatedMutexes) +
	    ", Dead Mutexes: " + Convert::ToString(ThinMutex::m_DeadMutexes));
}

static void InitThinMutex(void)
{
	l_Timer = make_shared<Timer>();
	l_Timer->SetInterval(10);
	l_Timer->OnTimerExpired.connect(boost::bind(&ThinMutexTimerHandler));
	l_Timer->Start();
}

INITIALIZE_ONCE(&InitThinMutex);

void ThinMutex::MakeNative(void)
{
	boost::mutex *mtx = new boost::mutex();
	mtx->lock();
#ifdef _WIN32
#	ifdef _WIN64
	InterlockedCompareExchange64(&m_Data, reinterpret_cast<LONGLONG>(mtx), THINLOCK_LOCKED);
#	else /* _WIN64 */
	InterlockedCompareExchange(&m_Data, reinterpret_cast<LONG>(mtx), THINLOCK_LOCKED);
#	endif /* _WIN64 */
#else /* _WIN32 */
	__sync_bool_compare_and_swap(&m_Data, THINLOCK_LOCKED, reinterpret_cast<uintptr_t>(mtx));
#endif /* _WIN32 */

	__sync_fetch_and_add(&m_InflatedMutexes, 1);
}

void ThinMutex::DestroyNative(void)
{
	delete reinterpret_cast<boost::mutex *>(m_Data);
}

void ThinMutex::LockNative(void)
{
	boost::mutex *mtx = reinterpret_cast<boost::mutex *>(m_Data);
	mtx->lock();
}

void ThinMutex::UnlockNative(void)
{
	boost::mutex *mtx = reinterpret_cast<boost::mutex *>(m_Data);
	mtx->unlock();
}

