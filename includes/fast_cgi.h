/*
 * Copyright (C) 2013 Aggregate
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

#include <dbconn.h>
#include <locale.hpp>

namespace FastCGI
{
	class Request;
	class Session;
	typedef std::tr1::shared_ptr<Session> SessionPtr;

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

	class Application
	{
		friend class Request;

		typedef std::pair<tyme::time_t, SessionPtr> SessionCacheItem;
		typedef std::map<std::string, SessionCacheItem> Sessions;

		long m_pid;
		FCGX_Request m_request;
		db::ConnectionPtr m_dbConn;
		Sessions m_sessions;
		lng::Locale m_locale;

		void cleanSessionCache();
	public:
		Application();
		~Application();
		int init(const char* localeRoot);
		int pid() const { return m_pid; }
		bool accept();
		db::ConnectionPtr dbConn(Request& request);
		SessionPtr getSession(Request& request, const std::string& sessionId);
		SessionPtr startSession(Request& request, const char* email);
		void endSession(Request& request, const std::string& sessionId);
		lng::TranslationPtr httpAcceptLanguage(const char* header) { return m_locale.httpAcceptLanguage(header); }

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
	private:
		ReqList m_requs;
#endif
	};

	typedef const char* param_t;

	class FinishResponse {};

	struct RequestState
	{
		virtual ~RequestState() {}
	};
	typedef std::tr1::shared_ptr<RequestState> RequestStatePtr;

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

		Application& m_app;
		bool m_headersSent;
		fcgi_streambuf m_streambufCin;
		fcgi_streambuf m_streambufCout;
		fcgi_streambuf m_cerr;
		Headers m_headers;
		ResponseCookies m_respCookies;
		RequestCookies m_reqCookies;
		RequestVariables m_reqVars;
		std::ostream m_cout;
		std::istream m_cin;
		mutable bool m_alreadyReadSomething;
		RequestStatePtr m_requestState;

		void unpackCookies();
		void unpackVariables(const char* data, size_t len);
		void unpackVariables();
		void readAll();
		void ensureInputWasRead();
		void buildCookieHeader();
		void printHeaders();

	public:
		std::ostream cerr;

		explicit Request(Application& app);
		~Request();
   		const char * const* envp() const { return m_app.m_request.envp; }
		Application& app() { return m_app; }

		void setHeader(const std::string& name, const std::string& value);
		void setCookie(const std::string& name, const std::string& value, tyme::time_t expire = 0);
		long long calcStreamSize();
		param_t getParam(const char* name) const { return FCGX_GetParam(name, m_app.m_request.envp); }
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
		lng::TranslationPtr httpAcceptLanguage();

		RequestStatePtr getRequestState() { return m_requestState; }
		void setRequestState(RequestStatePtr state) { m_requestState = state; }

		template <typename T>
		Request& operator << (const T& obj)
		{
			ensureInputWasRead();
			printHeaders();
			m_cout << obj;
			return *this;
		}

		template <typename T>
		const Request& operator >> (T& obj) const
		{
			m_read_something = true;
			m_cin >> obj;
			return *this;
		}

		std::streamsize read(void* ptr, std::streamsize length)
		{
			m_alreadyReadSomething = true;
			m_cin.read((char*)ptr, length);
			return m_cin.gcount();
		}

		template<std::streamsize length>
		std::streamsize read(char* (&ptr)[length])
		{
			m_alreadyReadSomething = true;
			m_cin.read(ptr, length);
			return m_cin.gcount();
		}

#if DEBUG_CGI
		const std::map<std::string, std::string>& cookieDebugData() const {
			return m_reqCookies;
		}
#endif
	};
}

namespace fcgi = FastCGI;

#endif //__SERVER_FAST_CFGI_H__