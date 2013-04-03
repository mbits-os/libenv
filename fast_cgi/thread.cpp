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
#include <fast_cgi/thread.hpp>
#include <fast_cgi/application.hpp>
#include <fast_cgi/request.hpp>
#include <fast_cgi/backends.hpp>

#ifdef _WIN32
#define INI "..\\conn.ini"
#endif

#ifdef POSIX
#define INI "../conn.ini"
#endif

#ifndef INI
#error There is no path to conn.ini defined...
#endif

namespace FastCGI
{
	Thread::Thread()
		: m_backend(new (std::nothrow) impl::LibFCGIThread)
	{
	}

	Thread::Thread(const char* uri)
		: m_backend(new (std::nothrow) impl::STLThread(uri))
	{
	}

	Thread::~Thread()
	{
	}

	bool Thread::init()
	{
		if (!m_backend.get() || !m_app)
			return false;

		m_backend->init();
		return true;
	}

	bool Thread::accept()
	{
		try {
			// Some platforms require accept() serialization, some don't..
			static mt::AsyncData accept_guard;

			Synchronize the(accept_guard);

			bool ret = m_backend->accept();
#if DEBUG_CGI
			if (ret)
				m_app->report((char**)envp());
#endif
			return ret;
		} catch (std::runtime_error) {
			return false;
		}
	}

	void Thread::run()
	{
		if (!init())
			return;
		while (accept())
		{
			handleRequest();
			m_backend->release();

			if (shouldStop())
				break;
		}
	}

	void Thread::handleRequest()
	{
		FastCGI::Request req(*this);

		try { onRequest(req); }
		catch(FastCGI::FinishResponse) {} // die() lands here
	}

	db::ConnectionPtr Thread::dbConn(Request& request)
	{
		Synchronize on (*this);

		//restart if needed
		if (!m_dbConn.get())
			m_dbConn = db::Connection::open(INI);
		else if (!m_dbConn->isStillAlive())
			m_dbConn->reconnect();

		if (!m_dbConn.get() || !m_dbConn->isStillAlive())
			request.on500();

		return m_dbConn;
	}
}
