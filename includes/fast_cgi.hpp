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

#ifndef __SERVER_FAST_CFGI_H__
#define __SERVER_FAST_CFGI_H__

#include <dbconn.hpp>
#include <locale.hpp>
#include <mt.hpp>
#include <fstream>

namespace FastCGI
{
	class Request;
	class Session;
	class Application;
	typedef std::shared_ptr<Session> SessionPtr;

	typedef const char* param_t;
	class FinishResponse {};

	namespace impl
	{
		struct RequestBackend
		{
			virtual std::ostream& cout() = 0;
			virtual std::ostream& cerr() = 0;
			virtual std::istream& cin() = 0;
		};

		class LibFCGIRequest: public RequestBackend
		{
			fcgi_streambuf m_streambufCin;
			fcgi_streambuf m_streambufCout;
			fcgi_streambuf m_streambufCerr;
			std::ostream m_cout;
			std::ostream m_cerr;
			std::istream m_cin;
		public:
			LibFCGIRequest(FCGX_Request& request);
			std::ostream& cout() { return m_cout; }
			std::ostream& cerr() { return m_cerr; }
			std::istream& cin() { return m_cin; }
		};

		class STLRequest: public RequestBackend
		{
		public:
			std::ostream& cout() { return std::cout; }
			std::ostream& cerr() { return std::cerr; }
			std::istream& cin() { return std::cin; }
		};

		struct ThreadBackend
		{
			virtual void init() {}
			virtual const char * const* envp() const = 0;
			virtual bool accept() = 0;
			virtual void release() = 0;
			virtual std::shared_ptr<RequestBackend> newRequestBackend() = 0;
		};

		class LibFCGIThread: public ThreadBackend
		{
			FCGX_Request m_request;
		public:
			void init() { FCGX_InitRequest(&m_request, 0, 0); }
			const char * const* envp() const { return m_request.envp; }
			bool accept() { return FCGX_Accept_r(&m_request) == 0; }
			void release() { FCGX_Finish_r(&m_request); }
			std::shared_ptr<RequestBackend> newRequestBackend() { return std::shared_ptr<RequestBackend>(new (std::nothrow) LibFCGIRequest(m_request)); }
		};

		class STLThread: public ThreadBackend
		{
			char* REQUEST_URI;
			char* QUERY_STRING;
			const char* environment[8];
		public:
			STLThread(const char* uri);
			~STLThread();
			void init() {}
			const char * const* envp() const { return environment; }
			bool accept() { return false; }
			void release() { }
			std::shared_ptr<RequestBackend> newRequestBackend() { return std::shared_ptr<RequestBackend>(new (std::nothrow) STLRequest()); }
		};
	};

	class Session
	{
		long long m_id;
		std::string m_name;
		std::string m_email;
		std::string m_hash;
		tyme::time_t m_setOn;
		lng::TranslationPtr m_tr;
		Session()
		{
		}
		Session(long long id, const std::string& name, const std::string& email, const std::string& hash, tyme::time_t setOn)
			: m_id(id)
			, m_name(name)
			, m_email(email)
			, m_hash(hash)
			, m_setOn(setOn)
		{
		}
	public:
		static SessionPtr stlSession();
		static SessionPtr fromDB(db::ConnectionPtr db, const char* sessionId);
		static SessionPtr startSession(db::ConnectionPtr db, const char* email);
		static void endSession(db::ConnectionPtr db, const char* sessionId);
		long long getId() const { return m_id; }
		const std::string& getName() const { return m_name; }
		const std::string& getEmail() const { return m_email; }
		const std::string& getSessionId() const { return m_hash; }
		tyme::time_t getStartTime() const { return m_setOn; }
		lng::TranslationPtr getTranslation() { return m_tr; }
		void setTranslation(lng::TranslationPtr tr) { m_tr = tr; }
	};

	class Application: public mt::AsyncData
	{
		typedef std::pair<tyme::time_t, SessionPtr> SessionCacheItem;
		typedef std::map<std::string, SessionCacheItem> Sessions;

		long m_pid;
		db::ConnectionPtr m_dbConn;
		Sessions m_sessions;
		lng::Locale m_locale;

		std::ofstream m_log;

		void cleanSessionCache();
	public:
		Application();
		~Application();
		void addStlSession();
		int init(const char* localeRoot);
		int pid() const { return m_pid; }
		db::ConnectionPtr dbConn(Request& request);
		SessionPtr getSession(Request& request, const std::string& sessionId);
		SessionPtr startSession(Request& request, const char* email);
		void endSession(Request& request, const std::string& sessionId);
		lng::TranslationPtr httpAcceptLanguage(const char* header) { return m_locale.httpAcceptLanguage(header); }
		std::string getLocalizedFilename(const char* header, const char* filename) { return m_locale.getFilename(header, filename); }
		std::ofstream& log() { return m_log; }

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
	private:
		ReqList m_requs;
#endif
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

	struct RequestState
	{
		virtual ~RequestState() {}
	};
	typedef std::shared_ptr<RequestState> RequestStatePtr;

	class PageTranslation
	{
		lng::TranslationPtr m_translation;
		std::string m_badString;
	public:
		bool init(SessionPtr session, Request& request);
		const char* operator()(lng::LNG stringId);
	};

	class Content
	{
	public:
		virtual ~Content() {}
		virtual void render(SessionPtr session, Request& request, PageTranslation& tr) = 0;
		virtual const char* getPageTitle(PageTranslation& tr) { return nullptr; }
	};
	typedef std::shared_ptr<Content> ContentPtr;

	class Thread: public mt::Thread
	{
		friend class Request;

		Application* m_app;
		std::shared_ptr<impl::ThreadBackend> m_backend;
		static void* thread_run(void* _this)
		{
			((Thread*)_this)->run();
			return nullptr;
		}
	public:
		Thread();
		explicit Thread(const char* uri);
		virtual ~Thread();

		bool init();
		void setApplication(Application& app) { m_app = &app; }

		Application* app() { return m_app; }
		const char * const* envp() const { return m_backend->envp(); }
		bool accept();
		void handleRequest();
		virtual void onRequest(Request& request) = 0;

		void run();
	};

	class Request
	{
		typedef std::map<std::string, std::string> Headers;

		struct Cookie
		{
			std::string m_name;
			std::string m_value;
			tyme::time_t m_expire;

			Cookie(): m_expire(0) {}
			Cookie(const std::string& name, const std::string& value, tyme::time_t expire)
				: m_name(name)
				, m_value(value)
				, m_expire(expire)
			{}
		};
		typedef std::map<std::string, Cookie> ResponseCookies;
		typedef std::map<std::string, std::string> RequestCookies;
		typedef std::map<std::string, std::string> RequestVariables;

		Thread& m_thread;
		bool m_headersSent;
		Headers m_headers;
		ResponseCookies m_respCookies;
		RequestCookies m_reqCookies;
		RequestVariables m_reqVars;
		mutable bool m_alreadyReadSomething;
		RequestStatePtr m_requestState;
		ContentPtr m_content;
		std::shared_ptr<impl::RequestBackend> m_backend;

		void unpackCookies();
		void unpackVariables(const char* data, size_t len);
		void unpackVariables();
		void readAll();
		void ensureInputWasRead();
		void buildCookieHeader();
		void printHeaders();

	public:
		std::ostream& cerr() { return m_backend->cerr(); }

		explicit Request(Thread& thread);
		~Request();
		const char * const* envp() const { return m_thread.envp(); }
		Application& app()
		{
			Application* ptr = m_thread.app();
			if (!ptr) on500();
			return *ptr;
		}
		db::ConnectionPtr dbConn() { return app().dbConn(*this); }

		void setHeader(const std::string& name, const std::string& value);
		void setCookie(const std::string& name, const std::string& value, tyme::time_t expire = 0);
		long long calcStreamSize();
		param_t getParam(const char* name) const { return FCGX_GetParam(name, (char**)envp()); }
		param_t getCookie(const char* name) const {
			RequestCookies::const_iterator _it = m_reqCookies.find(name);
			if (_it == m_reqCookies.end())
				return nullptr;
			return _it->second.c_str();
		}
		param_t getVariable(const char* name) const {
			RequestVariables::const_iterator _it = m_reqVars.find(name);
			if (_it == m_reqVars.end())
				return nullptr;
			return _it->second.c_str();
		}

		void die() { throw FinishResponse(); }

		std::string serverUri(const std::string& resource, bool withQuery = true);
		void redirectUrl(const std::string& url);
		void redirect(const std::string& resource, bool withQuery = true)
		{
			redirectUrl(serverUri(resource, withQuery));
		}

		void on404();
		void on500();

		SessionPtr getSession(bool require = true);
		SessionPtr startSession(bool long_session, const char* email);
		void endSession(const std::string& sessionId);

		lng::TranslationPtr httpAcceptLanguage();
		void sendMail(const char* mailFile, const char* email);

		RequestStatePtr getRequestState() { return m_requestState; }
		void setRequestState(RequestStatePtr state) { m_requestState = state; }

		ContentPtr getContent() { return m_content; }
		void setContent(ContentPtr content) { m_content = content; }
		template <typename T>
		void setContent(std::shared_ptr<T> content) { m_content = std::static_pointer_cast<Content>(content); }

		template <typename T>
		Request& operator << (const T& obj)
		{
			ensureInputWasRead();
			printHeaders();
			m_backend->cout() << obj;
			return *this;
		}

		template <typename T>
		Request& operator << (const T* obj)
		{
			ensureInputWasRead();
			printHeaders();
			if (obj)
				m_backend->cout() << obj;
			else
				m_backend->cout() << "(nullptr)";
			return *this;
		}

		template <typename T>
		const Request& operator >> (T& obj) const
		{
			m_alreadyReadSomething = true;
			m_backend->cin() >> obj;
			return *this;
		}

		std::streamsize read(void* ptr, std::streamsize length)
		{
			m_alreadyReadSomething = true;
			m_backend->cin().read((char*)ptr, length);
			return m_backend->cin().gcount();
		}

		template<std::streamsize length>
		std::streamsize read(char* (&ptr)[length])
		{
			m_alreadyReadSomething = true;
			m_backend->cin().read(ptr, length);
			return m_backend->cin().gcount();
		}

#if DEBUG_CGI
		const std::map<std::string, std::string>& cookieDebugData() const {
			return m_reqCookies;
		}
		const std::map<std::string, std::string>& varDebugData() const {
			return m_reqVars;
		}
#endif
	};
}

namespace fcgi = FastCGI;

#endif //__SERVER_FAST_CFGI_H__