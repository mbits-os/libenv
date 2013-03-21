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

namespace FastCGI
{
	class Request;
	class Response;

	class Application
	{
		friend class Request;
		friend class Response;

		long m_pid;
		FCGX_Request m_request;
		db::ConnectionPtr m_dbConn;
	public:
		Application();
		~Application();
		int init();
		int pid() const { return m_pid; }
		bool accept();
		db::ConnectionPtr dbConn(Response& response);

#if DEBUG_CGI
		struct ReqInfo
		{
			std::string resource;
			std::string server;
			std::string remote_addr;
			std::string remote_port;
			time_t now;
		};
		typedef std::list<ReqInfo> ReqList;
		const ReqList& requs() const { return m_requs; }
	private:
		ReqList m_requs;
#endif
	};

	typedef const char* param_t;

	class FinishResponse {};

	class Response
	{
		typedef std::map<std::string, std::string> Headers;

		struct Cookie
		{
			std::string m_name;
			std::string m_value;
			time_t m_expire;

			Cookie(): m_expire(0) {}
			Cookie(const std::string& name, const std::string& value, time_t expire)
				: m_name(name)
				, m_value(value)
				, m_expire(expire)
			{}
		};
		typedef std::map<std::string, Cookie> Cookies;

		Request& m_req;
		Application& m_app;
		bool m_headers_sent;
		fcgi_streambuf m_streambuf;
		fcgi_streambuf m_cerr;
		Headers m_headers;
		Cookies m_cookies;
		std::ostream m_cout;

		void ensureInputWasRead();
		void buildCookieHeader();
		void printHeaders();
	public:
		std::ostream cerr;

		Response(Request& req, Application& app);
		~Response();
		Application& app() { return m_app; }
		Request& req() { return m_req; }

		void header(const std::string& name, const std::string& value);
		void setcookie(const std::string& name, const std::string& value, time_t expire = 0);

		void die() { throw FinishResponse(); }

		std::string server_uri(const std::string& resource, bool with_query = true);
		void redirect_url(const std::string& url);
		void redirect(const std::string& resource, bool with_query = true)
		{
			redirect_url(server_uri(resource, with_query));
		}

		void on404();
		void on500();

		template <typename T>
		Response& operator << (const T& obj)
		{
			ensureInputWasRead();
			printHeaders();
			m_cout << obj;
			return *this;
		}
	};

	class Request
	{
		friend class Response;

		typedef std::map<std::string, std::string> Cookies;

		Response m_resp;
		Application& m_app;
		fcgi_streambuf m_streambuf;
		Cookies m_cookies;

		std::istream m_cin;
		mutable bool m_read_something;

		void unpackCookies();
		void readAll();
	public:
		Request(Application& app);
		~Request();
   		const char * const* envp() const { return m_app.m_request.envp; }
		Application& app() { return m_app; }
		long long calcStreamSize();
		param_t getParam(const char* name) const { return FCGX_GetParam(name, m_app.m_request.envp); }
		param_t getCookie(const char* name) const {
			Cookies::const_iterator _it = m_cookies.find(name);
			if (_it == m_cookies.end())
				return nullptr;
			return _it->second.c_str();
		}

		Response& resp() { return m_resp; }

		template <typename T>
		const Request& operator >> (T& obj) const
		{
			m_read_something = true;
			m_cin >> obj;
			return *this;
		}

		std::streamsize read(void* ptr, std::streamsize length)
		{
			m_read_something = true;
			m_cin.read((char*)ptr, length);
			return m_cin.gcount();
		}

		template<std::streamsize length>
		std::streamsize read(char* (&ptr)[length])
		{
			m_read_something = true;
			m_cin.read(ptr, length);
			return m_cin.gcount();
		}

#if DEBUG_CGI
		const std::map<std::string, std::string>& cookieDebugData() const {
			return m_cookies;
		}
#endif
	};
}

namespace fcgi = FastCGI;

#endif //__SERVER_FAST_CFGI_H__