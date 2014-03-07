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
#include <fast_cgi/request.hpp>
#include <fast_cgi/session.hpp>
#include <fast_cgi/thread.hpp>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <wiki.hpp>
#include <mail.hpp>
#include <wiki_mailer.hpp>

#define MAX_FORM_BUFFER 10240

FastCGI::static_resources_t static_web{};

namespace FastCGI
{
	struct RequestFilter : mail::Filter
	{
		Request& req;
		RequestFilter(Request& req) : req(req) {}

		void onChar(char c) override
		{
			req << c;
		}
	};

	struct RequestStream : wiki::stream
	{
		Request& req;
		explicit RequestStream(Request& req) : req(req) {}
		void write(const char* buffer, size_t size)
		{
			req << std::string(buffer, buffer + size);
		}
	};

	bool PageTranslation::init(SessionPtr session, Request& request)
	{
		if (session)
		{
			auto preferred = session->preferredLanguage();

			m_translation = session->getTranslation();
			if (m_translation)
			{
				auto culture = m_translation->tr(lng::CULTURE);
				if (!preferred.empty() && preferred != culture)
					m_translation = nullptr;

				if (m_translation && !m_translation->fresh())
				{
					preferred = culture;
					m_translation = nullptr;
				}

				// still survived? use it...
				if (m_translation)
					return true;
			}

			if (!preferred.empty())
			{
				m_translation = request.getTranslation(preferred);
				session->setTranslation(m_translation);

				if (m_translation)
					return true;
			}
		}

		m_translation = request.httpAcceptLanguage();
		if (session)
			session->setTranslation(m_translation);

		return !!m_translation;
	}

	static const char* error(std::string& s, lng::LNG stringId)
	{
		char buffer[100];
		printf(":%d:", stringId);
		s = buffer;
		return s.c_str();
	}

	const char* PageTranslation::operator()(lng::LNG stringId)
	{
		if (!m_translation)
			return error(m_badString, stringId);
		const char* str = m_translation->tr(stringId);
		if (!str)
			return error(m_badString, stringId);
		return str;
	}

	Request::Request(Thread& thread)
		: m_thread(thread)
		, m_headersSent(false)
		, m_alreadyReadSomething(false)
		, m_backend(thread.m_backend->newRequestBackend())
	{
		unpackCookies();
		unpackVariables();
	}

	Request::~Request()
	{
		readAll();
		printHeaders();
	}

#define WS() do { while (isspace((unsigned char)*c) && c < end) ++c; } while(0)
#define LOOK_FOR(ch) do { while (!isspace((unsigned char)*c) && *c != (ch) && c < end) ++c; } while(0)
#define LOOK_FOR2(ch1, ch2) do { while (!isspace((unsigned char)*c) && *c != (ch1) && *c != (ch2) && c < end) ++c; } while(0)
#define IS(ch) (c < end && *c == (ch))

	void Request::unpackCookies()
	{
		param_t HTTP_COOKIE = getParam("HTTP_COOKIE");
		if (!HTTP_COOKIE || !*HTTP_COOKIE)
			return;

		param_t end = HTTP_COOKIE + strlen(HTTP_COOKIE);
		param_t c = HTTP_COOKIE;
		while (c < end)
		{
			WS();
			param_t name_start = c;
			LOOK_FOR('=');
			std::string name(name_start, c);
			WS();
			if (!IS('=')) break;
			++c;
			WS();
			if (IS('"'))
			{
				const char* quot_end = nullptr;
				std::string value = url::quot_parse(c + 1, end - c - 1, &quot_end);
				if (quot_end && *quot_end)
					c = quot_end + 1;
				else
					break;
				if (name[0] != '$')
					m_reqCookies[name] = value;
			}
			else
			{
				param_t value_start = c;
				LOOK_FOR2(';', ',');
				if (name[0] != '$')
					m_reqCookies[name] = std::string(value_start, c);
			}
			WS();
			if (c >= end || (*c != ';' && *c != ',')) break;
			++c;
		};
	}

	void Request::unpackVariables(const char* data, size_t len)
	{
		const char* c = data;
		const char* end = c + len;
		while (c < end)
		{
			WS();
			const char* name_start = c;
			LOOK_FOR2('=', '&');
			std::string name = url::decode(name_start, c - name_start);
			WS();

			if (IS('='))
			{
				++c;
				WS();
				const char* value_start = c;
				LOOK_FOR('&');
				m_reqVars[name] = url::decode(value_start, c - value_start);
				WS();
			}
			else
				m_reqVars[name].erase();

			if (!IS('&')) break;

			++c;
		}
	}

	void Request::unpackVariables()
	{
		param_t CONTENT_TYPE = getParam("CONTENT_TYPE");
		if (CONTENT_TYPE && !strcmp(CONTENT_TYPE, "application/x-www-form-urlencoded"))
		{
			long long length = calcStreamSize();
			if (length != -1)
			{
				if (length > MAX_FORM_BUFFER)
					length = MAX_FORM_BUFFER;

				char * buffer = new (std::nothrow) char[(size_t)length];
				if (buffer)
				{
					if (read(buffer, length) == length)
						unpackVariables(buffer, (size_t)length);
					delete [] buffer;
				}
			}
		}
		else if (CONTENT_TYPE && !strcmp(CONTENT_TYPE, "multipart/form-data"))
		{
			// TODO: add support?
			readAll();
		}

		param_t QUERY_STRING = getParam("QUERY_STRING");
		if (QUERY_STRING && *QUERY_STRING)
		{
			unpackVariables(QUERY_STRING, strlen(QUERY_STRING));
		}
		else
		{
			param_t REQUEST_URI = getParam("REQUEST_URI");
			param_t query = REQUEST_URI ? strchr(REQUEST_URI, '?') : nullptr;
			if (query)
			{
				if (*++query)
					unpackVariables(query, strlen(query));
			}
		}
	}

	void Request::readAll()
	{
		m_alreadyReadSomething = true;
		// ignore() doesn't set the eof bit in some versions of glibc++
		// so use gcount() instead of eof()...
		do m_backend->cin().ignore(1024); while (m_backend->cin().gcount() == 1024);
	}

	long long Request::calcStreamSize()
	{
		param_t sizestr = getParam("CONTENT_LENGTH");
		if (sizestr)
		{
			char* ptr;
			long long ret = strtol(sizestr, &ptr, 10);
			if (*ptr)
			{
				*this << "can't parse \"CONTENT_LENGTH=" << getParam("CONTENT_LENGTH") << "\"\n";
				return -1;
			}
			return ret;
		}
		return -1;
	}

	void Request::ensureInputWasRead()
	{
		if (m_alreadyReadSomething)
			return;
		readAll();
	}

	void Request::buildCookieHeader()
	{
		std::string cookies;

		std::string domAndPath = "; Version=1";
		param_t SERVER_NAME = getParam("SERVER_NAME");
		if (SERVER_NAME && *SERVER_NAME)
		{
			domAndPath += "; Domain=";
			domAndPath += SERVER_NAME;
		}
		domAndPath += "; Path=/; HttpOnly";

		bool first = true;
		for (auto&& cookie: m_respCookies)
		{
			if (first) first = false;
			else cookies += ", ";
			cookies += url::encode(cookie.second.m_name) + "=";
			if (url::isToken(cookie.second.m_value))
				cookies += cookie.second.m_value;
			else
				cookies += "\"" + url::quot_escape(cookie.second.m_value) + "\"";
			cookies += domAndPath;

			if (cookie.second.m_expire != 0)
			{
				char buffer[256];
				tyme::tm_t tm = tyme::gmtime(cookie.second.m_expire);
				tyme::strftime(buffer, "%a, %d-%b-%Y %H:%M:%S GMT", tm );
				cookies += "; Expires=";
				cookies += buffer;
			}
		};

		if (!cookies.empty())
			m_headers["set-cookie"] = "Set-Cookie: " + cookies;
	}

	void Request::printHeaders()
	{
		if (m_headersSent)
			return;

		m_headersSent = true;
		if (m_headers.find("content-type") == m_headers.end())
			m_headers["content-type"] = "Content-Type: text/html; charset=utf-8";

		buildCookieHeader();

		for (auto&& header: m_headers)
		{
			*this << header.second << "\r\n";
		};

		*this << "\r\n";
	}

	void Request::setHeader(const std::string& name, const std::string& value)
	{
		if (m_headersSent)
		{
#if DEBUG_CGI
			*this << "<br/><b>Warning</b>: Cannot set header after sending data to the browser (" << name << ": " << value << ")<br/><br/>";
#endif
			return;
		}
		std::string v(name);
		v += ": ";
		v += value;
		std::string n(name);
		std::transform(n.begin(), n.end(), n.begin(), ::tolower);
		if (n == "set-cookie" || n == "set-cookie2")
			return; //not that API, use setcookie

		m_headers[n] = v;
	}

	void Request::setCookie(const std::string& name, const std::string& value, tyme::time_t expire)
	{
		if (m_headersSent)
		{
#if DEBUG_CGI
			*this << "<br/><b>Warning</b>: Cannot set a cookie after sending data to the browser (" << name << ": " << value << ")<br/><br/>";
#endif
			return;
		}
		std::string n(name);
		std::transform(n.begin(), n.end(), n.begin(), ::tolower);
		m_respCookies[n] = Cookie(name, value, expire);
	}

	std::string Request::serverUri(const std::string& resource, bool withQuery)
	{
		param_t port = getParam("SERVER_PORT");
		param_t server = getParam("SERVER_NAME");
		param_t query = withQuery ? getParam("QUERY_STRING") : nullptr;
		std::string url;

		if (server != nullptr)
		{
			const char* proto = "http";
			if (port != nullptr && strcmp(port, "443") == 0)
				proto = "https";
			url = proto;
			url += "://";
			url += server;
			if (port != nullptr && strcmp(port, "443") != 0 && strcmp(port, "80") != 0)
			{
				url += ":";
				url += port;
			}
		}

		url += resource;

		if (query != nullptr && *query != 0)
		{
			url += "?";
			url += query;
		}
		return url;
	}

	void Request::redirectUrl(const std::string& url)
	{
		setHeader("Location", url);
		*this
			<< "<h1>Redirection</h1>\n"
			<< "<p>The app needs to be <a href='" << url << "'>here</a>.</p>"; 
		die();
	}

	void Request::onLastModified(tyme::time_t lastModified)
	{
		param_t HTTP_IF_MODIFIED_SINCE = getParam("HTTP_IF_MODIFIED_SINCE");
		if (HTTP_IF_MODIFIED_SINCE && *HTTP_IF_MODIFIED_SINCE)
		{
			tyme::tm_t tm;
			tyme::scan(HTTP_IF_MODIFIED_SINCE, tm);
			if (lastModified <= tyme::mktime(tm))
			{
				setHeader("Status", "304 Not Modified");
				die();
			}
		}

		char lm[100];
		tyme::strftime(lm, "%a, %d %b %Y %H:%M:%S GMT", tyme::gmtime(lastModified));
		setHeader("Last-Modified", lm);
	}

	void Request::on400(const char* reason)
	{
		if (!reason)
			setHeader("Status", "400 Bad Request");
		else
		{
			std::string msg = "400 ";
			msg += reason;
			setHeader("Status", msg.c_str());
		}
		setHeader("Content-Type", "text/html; charset=utf-8");

		auto handler = app().getErrorHandler(400);
		if (handler)
			handler->onError(400, *this);
		else
		{
			*this
				<< "<tt>400: Oops! (URL: " << getParam("REQUEST_URI") << ")</tt>";
			if (reason && *reason)
				*this << "<br/>" << reason;
#if DEBUG_CGI
			*this << "<br/>\n<a href='/debug/'>Debug</a>";
			if (!m_icicle.empty())
			{
				*this << ", <a href='/debug/?frozen=" + url::encode(m_icicle) + "'>[F]</a>";
			}
			*this << ".";
#endif
		}
		die();
	}

	void Request::on404()
	{
		setHeader("Status", "404 Not Found");
		setHeader("Content-Type", "text/html; charset=utf-8");

		auto handler = app().getErrorHandler(404);
		if (handler)
			handler->onError(404, *this);
		else
		{
			*this
				<< "<tt>404: Oops! (URL: " << getParam("REQUEST_URI") << ")</tt>";
#if DEBUG_CGI
			*this << "<br/>\n<a href='/debug/'>Debug</a>";
			if (!m_icicle.empty())
			{
				*this << ", <a href='/debug/?frozen=" + url::encode(m_icicle) + "'>[F]</a>";
			}
			*this << ".";
#endif
		}
		die();
	}

	void Request::__on500(const char* file, int line, const std::string& log)
	{
		if (!log.empty())
			FastCGI::ApplicationLog(file, line) << "on500: " << log;

		setHeader("Status", "500 Internal Error");
		setHeader("Content-Type", "text/html; charset=utf-8");

		auto handler = app().getErrorHandler(500);
		if (handler)
			handler->onError(500, *this);
		else
		{
			*this
				<< "<tt>500: Oops! (URL: " << getParam("REQUEST_URI") << ")</tt>";
#if DEBUG_CGI
			*this << "<br/>\n<a href='/debug/'>Debug</a>";
			if (!m_icicle.empty())
			{
				*this << ", <a href='/debug/?frozen=" + url::encode(m_icicle) + "'>[F]</a>";
			}
			*this << ".";
#endif
		}
		die();
	}

	const std::string& Request::getStaticResources()
	{
		return app().getStaticResources();
	}

	SessionPtr Request::getSession(bool require)
	{
		SessionPtr out;
		param_t sessionId = getCookie("reader.login");
		if (sessionId && *sessionId)
			out = app().getSession(*this, sessionId);
		if (require && out.get() == nullptr)
		{
			std::string cont = serverUri(getParam("REQUEST_URI"), false);
			redirect("/auth/login?continue=" + url::encode(cont));
		}

		//if require and session two weeks old: /auth/login?continue=<this-url>&user=<email>

#if DEBUG_CGI
		if (!m_icicle.empty())
		{
			auto ptr = app().frozen(m_icicle);
			if (ptr && out)
				ptr->session_user(out->getLogin());
		}
#endif
		return out;
	}

	SessionPtr Request::startSession(bool long_session, const char* email)
	{
		SessionPtr session = app().startSession(*this, email);
		if (session.get())
		{
			if (long_session)
				setCookie("reader.login", session->getSessionId(), tyme::now() + 30 * 24 * 60 * 60);
			else
				setCookie("reader.login", session->getSessionId());

#if DEBUG_CGI
			if (!m_icicle.empty())
			{
				auto ptr = app().frozen(m_icicle);
				if (ptr)
					ptr->session_user(session->getLogin());
			}
#endif
		}
		return session;
	}

	void Request::endSession(const std::string& sessionId)
	{
		app().endSession(*this, sessionId);
		setCookie("reader.login", "", tyme::now());
	}

	lng::TranslationPtr Request::getTranslation(const std::string& lang)
	{
		return app().getTranslation(lang);
	}

	lng::TranslationPtr Request::httpAcceptLanguage()
	{
		param_t HTTP_ACCEPT_LANGUAGE = getParam("HTTP_ACCEPT_LANGUAGE");
		if (!HTTP_ACCEPT_LANGUAGE) HTTP_ACCEPT_LANGUAGE = "";
		return app().httpAcceptLanguage(HTTP_ACCEPT_LANGUAGE);
	}

	void Request::__sendMail(const char* file, int line, const std::string& preferredLanguage, const MailInfo& info)
	{
		if (info.to.empty())
			__on500(file, line, "Field `To:` empty when trying to send a message " + info.subject);

		wiki::document_ptr doc;

		if (!preferredLanguage.empty())
		{
			auto path = app().getLocalizedFilename(preferredLanguage.c_str(), info.mailFile);
			if (!path.empty())
				doc = wiki::compile(path);
		}

		if (!doc)
		{
			param_t HTTP_ACCEPT_LANGUAGE = getParam("HTTP_ACCEPT_LANGUAGE");
			if (!HTTP_ACCEPT_LANGUAGE) HTTP_ACCEPT_LANGUAGE = "";
			auto path = app().getLocalizedFilename(HTTP_ACCEPT_LANGUAGE, info.mailFile);
			if (!path.empty())
				doc = wiki::compile(path);
		}

		if (!doc)
			__on500(file, line, "Could not load WIKI file from " + info.mailFile.native() + " while trying to send a message \"" + info.subject + "\" to <" + info.to[0].email + ">");

		auto producer = mail::make_wiki_producer(doc, info.variables, app().getDataDir());
		auto message = PostOffice::newMessage(info.subject, producer);

		if (!producer || !message)
			on500("OOM while trying to send a message (" + info.subject + " to " + info.to[0].email + ")");

		message->setSubject(info.subject);
		message->setFrom(info.userName);
		for (auto&& addr : info.to)
			message->addTo(addr.name, addr.email);
		for (auto&& addr : info.cc)
			message->addCc(addr.name, addr.email);

#if 1
		FLOG << "PostOffice::post";
		mail::PostOffice::post(message, true);
		FLOG << "Posted";
#else
		auto filter = std::make_shared<RequestFilter>(*this);
		message->pipe(filter);
		setHeader("Content-Type", "application/octet-stream; charset=utf-8");
		setHeader("Content-Disposition", "inline; filename=reset.eml");

		//RequestStream dbg{ *this };
		//doc->debug(dbg);
		//*this << "\n";

		mail::post(message);
		die();
#endif
	}
}
