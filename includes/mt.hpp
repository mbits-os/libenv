/*
 * Copyright (C) 2013 midnightBITS
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __MT_HPP__
#define __MT_HPP__

#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef POSIX
#include <pthread.h>
#endif

namespace mt
{
	class Thread
	{
#ifdef _WIN32
		uintptr_t m_thread;
		unsigned int m_threadId;
#endif
#ifdef POSIX
		pthread_t m_thread;
#endif
		bool stopThread;
	public:
		Thread(): m_thread(0) {}
		static unsigned long currentId();
		void attach();
		void start();
		bool stop();
		bool shouldStop() const { return stopThread; }
		virtual void run() = 0;
#ifdef _WIN32
		unsigned int threadId() const { return m_threadId; }
#endif
#ifdef POSIX
		pthread_t threadId() const { return m_thread; }
#endif

	};

 	struct Mutex
	{
#ifdef _WIN32
		CRITICAL_SECTION m_cs;
#endif
#ifdef POSIX
		pthread_mutex_t m_mutex;
#endif
		void init();
		void finalize();
		void lock();
		void unlock();
	};

	class AsyncData
	{
	protected:
		Mutex m_mtx;
	public:
		AsyncData() { m_mtx.init(); }
		~AsyncData() { m_mtx.finalize(); }
		void lock() { m_mtx.lock(); }
		void unlock() { m_mtx.unlock(); }
	};

	struct Synchronize
	{
		AsyncData& data;

		Synchronize(AsyncData& d): data(d) { data.lock(); };
		~Synchronize() { data.unlock(); };
	};
 
}

using mt::Synchronize;

#endif // __MT_HPP__