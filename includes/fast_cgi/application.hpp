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

#ifndef __APPLICATION_HPP__
#define __APPLICATION_HPP__

#include <dbconn.hpp>
#include <locale.hpp>
#include <mt.hpp>
#include <fstream>

#define LINED_2(name, line) name ## _ ## line
#define LINED_1(name, line) LINED_2(name, line)
#define LINED(name) LINED_1(name, __LINE__)

#define LOG_FUNC() FastCGI::FLOGBlock LINED(dummy)(__FUNCTION__, __FILE__, __LINE__)
#define LOG_BLOCK(tag) FastCGI::FLOGBlock LINED(dummy)(tag, __FILE__, __LINE__)

namespace FastCGI
{
	class Thread;
	class Request;
	class Session;
	typedef std::shared_ptr<Thread> ThreadPtr;
	typedef std::shared_ptr<Session> SessionPtr;

	struct FLOGBlock
	{
		const char* tag;
		const char* file;
		int line;
		inline FLOGBlock(const char* tag, const char* file, int line);
		inline ~FLOGBlock();
	};

	class ApplicationLog
	{
		std::ostream& m_log;
	public:
		ApplicationLog(const char* file, int line);
		~ApplicationLog();
		template <typename T>
		ApplicationLog& operator << (const T& t)
		{
			m_log << t;
			return *this;
		}
	};
#define FLOG FastCGI::ApplicationLog(__FILE__, __LINE__)

	class FLogSource: public mt::AsyncData
	{
		std::ofstream m_log;
	public:
		FLogSource(const char* path);
		std::ofstream& log() { return m_log; }
	};

	inline FLOGBlock::FLOGBlock(const char* tag, const char* file, int line)
		: tag(tag), file(file), line(line) { ApplicationLog(file, line) << "[in] " << tag; }
	inline FLOGBlock::~FLOGBlock() { ApplicationLog(file, line) << "[out] " << tag; }

	class Application: public mt::AsyncData
	{
		typedef std::pair<tyme::time_t, SessionPtr> SessionCacheItem;
		typedef std::map<std::string, SessionCacheItem> Sessions;
		typedef std::list<ThreadPtr> Threads;

		long m_pid;
		Sessions m_sessions;
		Threads m_threads;
		lng::Locale m_locale;

		void cleanSessionCache();
	public:
		Application();
		~Application();
		template <typename T>
		bool addThreads(int threadCount)
		{
			for (int i = 0; i < threadCount; ++i)
			{
				auto ptr = std::make_shared<T>();
				if (!ptr)
					return false;
				ptr->setApplication(*this);
				m_threads.push_back(ptr);
			}
			return true;
		}
		void addStlSession();
		int init(const char* localeRoot);
		void run();
		int pid() const { return m_pid; }
		SessionPtr getSession(Request& request, const std::string& sessionId);
		SessionPtr startSession(Request& request, const char* email);
		void endSession(Request& request, const std::string& sessionId);
		lng::TranslationPtr httpAcceptLanguage(const char* header) { return m_locale.httpAcceptLanguage(header); }
		std::string getLocalizedFilename(const char* header, const char* filename) { return m_locale.getFilename(header, filename); }

#if DEBUG_CGI
		struct ReqInfo
		{
			std::string resource;
			std::string server;
			std::string remote_addr;
			std::string remote_port;
			tyme::time_t now;
		};
		typedef std::list<ReqInfo> ReqList;
		const ReqList& requs() const { return m_requs; }
		void report(char** envp);
		std::list<ThreadPtr> getThreads() const { return m_threads; }
	private:
		ReqList m_requs;
#endif
	};
}

extern "C" void flog(const char* file, int line, const char* fmt, ...);
#define DBG(fmt, ...) flog(__FILE__, __LINE__, fmt, __VA_ARGS__)

#endif //__APPLICATION_HPP__