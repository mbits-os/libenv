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

#include <db/conn.hpp>
#include <locale.hpp>
#include <mt.hpp>
#include <sstream>
#if DEBUG_CGI
#include <chrono>
#endif

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
	struct UserInfoFactory;
	typedef std::shared_ptr<Thread> ThreadPtr;
	typedef std::shared_ptr<Session> SessionPtr;
	using UserInfoFactoryPtr = std::shared_ptr<UserInfoFactory>;

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
		std::ostringstream m_log;
	public:
		ApplicationLog(const char* file, int line);
		~ApplicationLog();
		template <typename T>
		ApplicationLog& operator << (const T& t)
		{
			m_log << t;
			return *this;
		}

		std::ostream& log() { return m_log; }
	};
#define FLOG FastCGI::ApplicationLog(__FILE__, __LINE__)

	class FLogSource: public mt::AsyncData
	{
		std::string m_path;
	public:
		FLogSource();
		bool open(const filesystem::path& path);
		void log(const std::string& line);
	};

	inline FLOGBlock::FLOGBlock(const char* tag, const char* file, int line)
		: tag(tag), file(file), line(line) { ApplicationLog(file, line) << "[in] " << tag; }
	inline FLOGBlock::~FLOGBlock() { ApplicationLog(file, line) << "[out] " << tag; }

#if DEBUG_CGI
	class FrozenState
	{
	public:
		using clock_t = std::chrono::high_resolution_clock;
		using duration_t = clock_t::duration;

		typedef std::map<std::string, std::string> string_map;
		typedef std::vector<std::pair<std::string, std::string>> string_pair_list;

		FrozenState(char** envp, const Request& req, const std::string& icicle);

		const std::string& icicle() const { return m_icicle; }
		const string_map& environment() const { return m_environment; }
		const string_map& get() const { return m_get; }
		const string_map& cookies() const { return m_cookies; }
		void report_header(const std::string& header);
		const string_pair_list& response() const { return m_response; }
		const std::string& resource() const { return m_resource; }
		const std::string& server() const { return m_server; }
		const std::string& remote_addr() const { return m_remote_addr; }
		const std::string& remote_port() const { return m_remote_port; }
		const std::string& session_user() const { return m_session_user; }
		void session_user(const std::string& user) { m_session_user = user; }
		const std::string& culture() const { return m_culture; }
		bool culture_from_db() const { return m_culture_from_db; }
		void culture(const std::string& culture, bool from_db) { m_culture = culture; m_culture_from_db = from_db; }

		tyme::time_t now() const { return m_now; }

		duration_t duration() const { return m_duration; }
		void duration(const duration_t& value) { m_duration = value; }
	private:
		std::string m_icicle;

		string_map m_environment;
		string_map m_get;
		string_map m_cookies;
		string_pair_list m_response;

		std::string m_resource;
		std::string m_server;
		std::string m_remote_addr;
		std::string m_remote_port;
		std::string m_session_user;
		std::string m_culture;
		bool m_culture_from_db;

		duration_t m_duration;

		tyme::time_t m_now;
	};

	using FrozenStatePtr = std::shared_ptr<FrozenState>;
	using Iceberg = std::map<std::string, FrozenStatePtr>;
#endif

	struct ErrorHandler
	{
		virtual ~ErrorHandler() {}
		virtual void onError(int errorCode, Request& request) = 0;
	};

	using ErrorHandlerPtr = std::shared_ptr<ErrorHandler>;

	class Application: public mt::AsyncData
	{
		typedef std::pair<tyme::time_t, SessionPtr> SessionCacheItem;
		typedef std::map<std::string, SessionCacheItem> Sessions;
		typedef std::list<ThreadPtr> Threads;

		long m_pid;
		std::string m_staticWeb;
		filesystem::path m_dataDir;
		filesystem::path m_dbConf;
		filesystem::path m_smtpConf;
		filesystem::path m_accessLog;
		Sessions m_sessions;
		Threads m_threads;
		lng::Locale m_locale;
		std::map<int, ErrorHandlerPtr> m_errorHandlers;
		UserInfoFactoryPtr m_userInfoFactory;

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

		void shutdown();
		int init(const filesystem::path& localeRoot, const UserInfoFactoryPtr& userInfoFactory);
		void reload(const filesystem::path& localeRoot); // informs everyone there are new configs...
		void run();
		int pid() const { return m_pid; }
		SessionPtr getSession(Request& request, const std::string& sessionId);
		SessionPtr startSession(Request& request, const char* login);
		void endSession(Request& request, const std::string& sessionId);
		lng::LocaleInfos knownLanguages() { return m_locale.knownLanguages(); }
		lng::TranslationPtr getTranslation(const std::string& lang) { return m_locale.getTranslation(lang); }
		lng::TranslationPtr httpAcceptLanguage(const char* header) { return m_locale.httpAcceptLanguage(header); }
		filesystem::path getLocalizedFilename(const char* header, const filesystem::path& filename) { return m_locale.getFilename(header, filename); }

		void setStaticResources(const std::string& url) { m_staticWeb = url; }
		const std::string& getStaticResources() const { return m_staticWeb; }

		void setDataDir(const filesystem::path& conf) { m_dataDir = conf; }
		const filesystem::path& getDataDir() const { return m_dataDir; }

		void setDBConn(const filesystem::path& conf) { m_dbConf = conf; }
		const filesystem::path& getDBConn() const { return m_dbConf; }

		void setSMTPConn(const filesystem::path& conf) { m_smtpConf = conf; }
		const filesystem::path& getSMTPConn() const { return m_smtpConf; }

		void setAccessLog(const filesystem::path& log) { m_accessLog = log; }
		const filesystem::path& getAccessLog() const { return m_accessLog; }

		void setErrorHandler(int error, const ErrorHandlerPtr& ptr) { m_errorHandlers[error] = ptr; }
		ErrorHandlerPtr getErrorHandler(int error)
		{
			auto it = m_errorHandlers.find(error);
			if (it == m_errorHandlers.end()) it = m_errorHandlers.find(error / 100);
			if (it == m_errorHandlers.end()) it = m_errorHandlers.find(0);
			if (it == m_errorHandlers.end()) return nullptr;

			return it->second;
		}

#if DEBUG_CGI
		struct ReqInfo
		{
			std::string icicle;
			std::string resource;
			std::string server;
			std::string remote_addr;
			std::string remote_port;
			tyme::time_t now;
		};
		typedef std::list<ReqInfo> ReqList;
		const ReqList& requs() const { return m_requs; }
		FrozenStatePtr frozen(const std::string&);
		std::string freeze(char** envp, const Request& req);
		void report(char** envp, const std::string& icicle);
		void reportHeader(const std::string& header, const std::string& icicle);
		std::list<ThreadPtr> getThreads() const { return m_threads; }
	private:
		ReqList m_requs;
		Iceberg m_iceberg;
#endif
	};
}

extern "C" void flog(const char* file, int line, const char* fmt, ...);
#define DBG(fmt, ...) flog(__FILE__, __LINE__, fmt, __VA_ARGS__)

#endif //__APPLICATION_HPP__
