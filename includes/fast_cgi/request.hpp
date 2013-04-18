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

#ifndef __REQUEST_HPP__
#define __REQUEST_HPP__

#include <dbconn.hpp>
#include <locale.hpp>
#include <mt.hpp>
#include <fstream>
#include <utils.hpp>

#include <fast_cgi/thread.hpp>

namespace lng
{
	class Translation;
	typedef std::shared_ptr<Translation> TranslationPtr;
	enum LNG;
};

namespace FastCGI
{
	class Application;
	class Session;
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
	};

	class PageTranslation
	{
		lng::TranslationPtr m_translation;
		std::string m_badString;
	public:
		bool init(SessionPtr session, Request& request);
		const char* operator()(lng::LNG stringId);
	};

	struct RequestState
	{
		virtual ~RequestState() {}
	};
	typedef std::shared_ptr<RequestState> RequestStatePtr;

	class Content
	{
	public:
		virtual ~Content() {}
		virtual void render(SessionPtr session, Request& request, PageTranslation& tr) = 0;
		virtual const char* getPageTitle(PageTranslation& tr) { return nullptr; }
	};
	typedef std::shared_ptr<Content> ContentPtr;

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
		db::ConnectionPtr dbConn() { return m_thread.dbConn(*this); }

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

		void onLastModified(tyme::time_t lastModified);
		void on400(const char* reason = nullptr);
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

#endif //__REQUEST_HPP__