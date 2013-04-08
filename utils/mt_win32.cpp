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
#include <fast_cgi.hpp>

namespace mt
{
	unsigned long Thread::currentId()
	{
		return GetCurrentThreadId();
	}

	struct ThreadArgs
	{
		Thread* thread;
		HANDLE  hEvent;
	};

	static unsigned int __stdcall thread_run(void* ptr)
	{
		//FLOG << "Thread started: " << mt::Thread::currentId();
		auto args = (ThreadArgs*)ptr;
		auto thread = args->thread;
		SetEvent(args->hEvent);
		args = nullptr;

		thread->run();

		_endthreadex(0);
		return 0;
	}

	void Thread::start()
	{
		//FLOG << "Starting a new thread";
		ThreadArgs args = { this };
		args.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (!args.hEvent) return;
		m_thread = _beginthreadex(nullptr, 0, thread_run, &args, 0, &m_threadId);
		if (!m_thread)
		{
			CloseHandle(args.hEvent);
			return;
		}
		WaitForSingleObject(args.hEvent, INFINITE);
		CloseHandle(args.hEvent);
		//FLOG << "Thread " << m_threadId << " signalled start. Moving on.";
	}

	void Thread::attach()
	{
		m_thread = NULL;
		m_threadId = mt::Thread::currentId();
		//FLOG << "Thread " << m_threadId << " attached. Running.";
		run();
	}

	bool Thread::stop()
	{
		//FLOG << "Requesting thread " << m_threadId << " to close";
		stopThread = true;
		bool ret = WaitForSingleObject((HANDLE)m_thread,INFINITE) == WAIT_OBJECT_0;
		//FLOG << "Thread " << m_threadId << " closed. Moving on.";
		return ret;
	}

	void Mutex::init()
	{
		InitializeCriticalSection(&m_cs);
	}

	void Mutex::finalize()
	{
		DeleteCriticalSection(&m_cs);
	}

	void Mutex::lock()
	{
		EnterCriticalSection(&m_cs);
	}

	void Mutex::unlock()
	{
		LeaveCriticalSection(&m_cs);
	}
}
