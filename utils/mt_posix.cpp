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

#include "pch.h"
#include <mt.hpp>

namespace mt
{
	unsigned long Thread::currentId()
	{
		return (unsigned long)pthread_self();
	}

	struct Event: public AsyncData
	{
		pthread_cond_t m_cond;
		bool m_condInited;
		bool m_alreadySignalled;
	public:
		Event(): m_condInited(false), m_alreadySignalled(false) {}
		~Event()
		{
			if (m_condInited)
				pthread_cond_destroy(&m_cond);
		}
		bool create()
		{
			m_condInited = (pthread_cond_init(&m_cond, NULL) == 0);
			return m_condInited;
		}

		bool signal()
		{
			Synchronize on (*this);
			int ret = pthread_cond_signal(&m_cond);
			m_alreadySignalled = true;
			return ret == 0;
		}

		bool wait()
		{
			Synchronize on (*this);
			if (m_alreadySignalled)
			{
				m_alreadySignalled = false;
				return true;
			}
			bool ret = pthread_cond_wait(&m_cond, &m_mtx.m_mutex) == 0;
			m_alreadySignalled = false;
			return ret;
		}
	}; 

	struct ThreadArgs
	{
		Thread* thread;
		Event   posixEvent;
	};

	static void* thread_run(void* ptr)
	{
		//printf("Thread started: 0x%08x\n", mt::Thread::currentId()); fflush(stdout);
		auto args = (ThreadArgs*)ptr;
		auto thread = args->thread;
		args->posixEvent.signal();
		args = nullptr;

		thread->run();
		return nullptr;
	}

	void Thread::start()
	{
		//printf("Starting a new thread\n"); fflush(stdout);
		ThreadArgs args = { this };
		if (!args.posixEvent.create()) return;

		if(pthread_create(&m_thread, NULL, thread_run, &args))
			return;

		args.posixEvent.wait();
		//printf("Thread 0x%08x signalled start. Moving on.\n", m_thread); fflush(stdout);

	}

	void Thread::attach()
	{
		m_thread = pthread_self();
		//printf("Thread %p attached. Running.\n", m_thread); fflush(stdout);
		run();
	}

	bool Thread::stop()
	{
		//printf("Requesting thread 0x%08x to close\n", m_thread); fflush(stdout);
		stopThread = true;
		bool ret = pthread_join(m_thread, NULL) == 0;
		//printf("Thread 0x%08x closed. Moving on.\n", m_thread); fflush(stdout);
		return ret;
	}
}
