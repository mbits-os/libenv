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

#ifndef __OS_HPP__
#define __OS_HPP__

#include <stdexcept>

namespace os
{
	class Thread
	{
	public:
		static unsigned long currentId() { return 0; }
		void start() {}
		virtual void run() = 0;
	};

	class CriticalSection
	{
	public:
		bool init() { return true; }
		void lock() {}
		void unlock() {}
	};

	class AsyncData
	{
		CriticalSection m_cs;
		bool m_inited;
	public:
		AsyncData()
		{
			m_inited = m_cs.init();
		}
		bool isInitialized() const { return m_inited; }

		void lock() { m_cs.lock(); }
		void unlock() { m_cs.unlock(); }
	};

	class InitializedAsyncData: public AsyncData
	{
	public:
		InitializedAsyncData()
		{
			if (!isInitialized())
				throw std::runtime_error("");
		}
	};

	class Lock
	{
		AsyncData& m_ref;
	public:
		Lock(AsyncData& ref)
			: m_ref(ref)
		{
			m_ref.lock();
		}
		~Lock()
		{
			m_ref.unlock();
		}
	};
}

#endif // __OS_HPP__