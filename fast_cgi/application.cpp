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

#include <fast_cgi/application.hpp>
#include <fast_cgi/thread.hpp>
#include <fast_cgi/session.hpp>
#include <fast_cgi/request.hpp>
#include <string.h>

#ifdef _WIN32
#define LOGFILE "..\\error-%u.log"
#endif

#ifdef POSIX
#define LOGFILE "../error-%u.log"
#endif

namespace FastCGI
{
	namespace
	{
		Application* g_app = nullptr;
		FLogSource* g_log = nullptr;
	}

	FLogSource::FLogSource(const char* path)
		: m_log(path, std::ios_base::out | std::ios_base::binary | std::ios_base::app)
	{
		g_log = this;
	}

	Application::Application()
	{
		m_pid = _getpid();
		g_app = this;
	}

	Application::~Application()
	{
	}

	int Application::init(const char* localeRoot)
	{
		int ret = FCGX_Init();
		if (ret != 0)
			return ret;

		m_locale.init(localeRoot);

		return 0;
	}

	void Application::run()
	{
		auto first = m_threads.begin(), end = m_threads.end();
		auto cur = first;
		if (first == end)
			return;
		for (++cur; cur != end; ++cur)
			(*cur)->start();

		(*first)->attach();

		std::for_each(++first, end, [](ThreadPtr thread) { thread->stop(); });
	}

	void Application::shutdown()
	{
		auto first = m_threads.begin();
		if (first == m_threads.end())
			return;

		(*first)->shutdown();
	}

#if DEBUG_CGI
	void Application::report(char** envp)
	{
		if (false) //turned off for now
		{
			ReqInfo info;
			info.resource    = FCGX_GetParam("REQUEST_URI", envp);
			info.server      = FCGX_GetParam("SERVER_NAME", envp);
			info.remote_addr = FCGX_GetParam("REMOTE_ADDR", envp);
			info.remote_port = FCGX_GetParam("REMOTE_PORT", envp);
			info.now = tyme::now();
			//m_requs.push_back(info);
		}
	}
#endif

	void Application::cleanSessionCache()
	{
		tyme::time_t treshold = tyme::now() - 30 * 60;

		Sessions copy;
		std::for_each(m_sessions.begin(), m_sessions.end(), [&copy](const Sessions::value_type& pair)
		{
			copy.insert(pair);
		});

		m_sessions = copy;
	}

	void Application::addStlSession()
	{
		SessionPtr out = Session::stlSession();
		if (out.get())
			m_sessions.insert(std::make_pair(out->getSessionId(), std::make_pair(out->getStartTime(), out)));
	}

	SessionPtr Application::getSession(Request& request, const std::string& sessionId)
	{
		Synchronize on (*this);

		cleanSessionCache();

		SessionPtr out;
		Sessions::iterator _it = m_sessions.find(sessionId);
		if (_it == m_sessions.end() || !_it->second.second.get())
		{
			db::ConnectionPtr db = request.dbConn();
			if (db.get())
				out = Session::fromDB(db, sessionId.c_str());

			// TODO: limits needed, or DoS eminent
			if (out.get())
				m_sessions.insert(std::make_pair(sessionId, std::make_pair(tyme::now(), out)));
		}
		else
		{
			out = _it->second.second;
			_it->second.first = tyme::now(); // ping the session
		}

		return out;
	}

	SessionPtr Application::startSession(Request& request, const char* email)
	{
		Synchronize on (*this);

		cleanSessionCache();

		SessionPtr out;
		db::ConnectionPtr db = request.dbConn();
		if (db.get())
			out = Session::startSession(db, email);
		// TODO: limits needed, or DoS eminent
		if (out.get())
			m_sessions.insert(std::make_pair(out->getSessionId(), std::make_pair(out->getStartTime(), out)));

		return out;
	}

	void Application::endSession(Request& request, const std::string& sessionId)
	{
		db::ConnectionPtr db = request.dbConn();
		if (db.get())
			Session::endSession(db, sessionId.c_str());
		Sessions::iterator _it = m_sessions.find(sessionId);
		if (_it != m_sessions.end())
			m_sessions.erase(_it);
	}

	ApplicationLog::ApplicationLog(const char* file, int line)
		: m_log(g_log->log())
	{
		// lock
		g_log->lock();
		m_log << file << ":" << line << " [" << _getpid() << "] @" << mt::Thread::currentId() << " ";
	}

	ApplicationLog::~ApplicationLog()
	{
		m_log << std::endl;
		m_log.flush();
		g_log->unlock();
		// unlock
	}
}

extern "C" void flog(const char* file, int line, const char* fmt, ...)
{
	char buffer[8196];
	va_list args;
	va_start(args, fmt);
	vsprintf(buffer, fmt, args);
	va_end(args);
	FastCGI::ApplicationLog(file, line) << buffer;
}
