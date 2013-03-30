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
		return GetCurrentThreadId();
	}

	static void thread_run(void* ptr)
	{
		auto _this = (Thread*)ptr;
		_this->run();
		_endthreadex(0);
	}

	void Thread::start()
	{
		m_thread = _beginthread(thread_run, 0, this);
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
