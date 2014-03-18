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
#include <crypt.hpp>
#include <fstream>
#if defined(WIN32) && !defined(NDEBUG)
#include <__file__.win32.hpp>
#else
#define BUILD_DIR_LEN 6
#endif

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

	FLogSource::FLogSource()
	{
		g_log = this;
	}

	bool FLogSource::open(const filesystem::path& path)
	{
		Synchronize on(*this);
		m_path = path.native();
		std::fstream log{ m_path, std::ios_base::out | std::ios_base::app };
		return log.is_open();
	}

	void FLogSource::log(const std::string& line)
	{
		Synchronize on(*this);
		std::fstream log{ m_path, std::ios_base::out | std::ios_base::app };
		log << line << std::endl;
	}

	Application::Application()
	{
		m_pid = _getpid();
		g_app = this;
	}

	Application::~Application()
	{
	}

	int Application::init(const filesystem::path& localeRoot, const UserInfoFactoryPtr& userInfoFactory)
	{
		if (!userInfoFactory)
			return 1;
		m_userInfoFactory = userInfoFactory;

		int ret = FCGX_Init();
		if (ret != 0)
			return ret;

		m_locale.init(localeRoot);

		return 0;
	}

	void Application::reload(const filesystem::path& localeRoot)
	{
		m_locale.reload(localeRoot);

		m_sessions.clear(); // dropping all sessions will restart them from DB, with new settings...

		for (auto&& thread : m_threads)
			thread->reload();
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
	FrozenState::FrozenState(char** envp, const Request& req, const std::string& icicle)
		: m_icicle(icicle)
		, m_get(req.varDebugData())
		, m_cookies(req.cookieDebugData())
		, m_resource(FCGX_GetParam("REQUEST_URI", envp))
		, m_server(FCGX_GetParam("SERVER_NAME", envp))
		, m_remote_addr(FCGX_GetParam("REMOTE_ADDR", envp))
		, m_remote_port(FCGX_GetParam("REMOTE_PORT", envp))
		, m_now(tyme::now())
	{
		for (; *envp; ++envp)
		{
			const char* eq = strchr(*envp, '=');
			std::string key, value;
			if (!eq)
				key = *envp;
			else if (eq != *envp)
				key.assign(*envp, eq - *envp);

			if (eq)
			{
				if (eq != *envp)
					++eq;

				value = eq;
			}

			m_environment[key] = std::move(value);
		}
	}

	FrozenStatePtr Application::frozen(const std::string& icicle)
	{
		auto it = m_iceberg.find(icicle);
		if (it == m_iceberg.end())
			return nullptr;

		return it->second;
	}

	std::string Application::freeze(char** envp, const Request& req)
	{
		constexpr size_t length = (Crypt::SessionHash::HASH_SIZE * 8 + 5) / 6 + 1;

		char seed[20];
		char base64[length];
		Crypt::session_t sessionId;
		Crypt::newSalt(seed);
		Crypt::session(seed, sessionId);
		Crypt::base64_encode(sessionId, sizeof(sessionId), base64);
		base64[length - 1] = 0;

		std::string icicle{ base64 };
		auto ptr = std::make_shared<FrozenState>(envp, std::ref(req), icicle);

		if (!ptr)
			return std::string();

		m_iceberg[icicle] = std::move(ptr);

		return icicle;
	}

	void Application::report(char** envp, const std::string& icicle)
	{
		ReqInfo info;
		info.icicle      = icicle;
		info.resource    = FCGX_GetParam("REQUEST_URI", envp);
		info.server      = FCGX_GetParam("SERVER_NAME", envp);
		info.remote_addr = FCGX_GetParam("REMOTE_ADDR", envp);
		info.remote_port = FCGX_GetParam("REMOTE_PORT", envp);
		info.now         = tyme::now();
		m_requs.push_back(info);
	}

	void Application::reportHeader(const std::string& header, const std::string& icicle)
	{
		auto frozen = this->frozen(icicle);
		if (frozen)
			frozen->report_header(header);
	}

	void FrozenState::report_header(const std::string& header)
	{
		auto pos = header.find(':');
		if (pos == std::string::npos)
			m_response.emplace_back(std::trim(header), std::string());
		else
			m_response.emplace_back(std::trim(header.substr(0, pos)), std::trim(header.substr(pos + 1)));
	}
#endif

	void Application::cleanSessionCache()
	{
		tyme::time_t treshold = tyme::now() - 30 * 60;

		Sessions copy;
		for (auto&& pair: m_sessions)
		{
			copy.insert(pair);
		};

		m_sessions = copy;
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
				out = Session::fromDB(db, m_userInfoFactory, sessionId.c_str());

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

	SessionPtr Application::startSession(Request& request, const char* login)
	{
		Synchronize on (*this);

		cleanSessionCache();

		SessionPtr out;
		db::ConnectionPtr db = request.dbConn();
		if (db.get())
			out = Session::startSession(db, m_userInfoFactory, login);
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
	{
		m_log << "[" << _getpid() << "] @" << mt::Thread::currentId() << " "; // << (file + BUILD_DIR_LEN) << ":" << line << ": ";
	}

	ApplicationLog::~ApplicationLog()
	{
		g_log->log(m_log.str());
	}
}

extern "C" void flog(const char* file, int line, const char* fmt, ...)
{
	char buffer[8196];
	va_list args;
	va_start(args, fmt);
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
	vsprintf(buffer, fmt, args);
#ifdef _MSC_VER
#pragma warning(pop)
#endif
	va_end(args);
	FastCGI::ApplicationLog(file, line) << buffer;
}
