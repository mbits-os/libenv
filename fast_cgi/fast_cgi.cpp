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

#include "pch.h"
#include "fast_cgi.h"
#include "crypt.hpp"
#include <utils.h>

#ifdef _WIN32
#define INI "..\\conn.ini"
#define LOGFILE "..\\error-%u.log"
#endif

#ifdef USE_POSIX
#define INI "../conn.ini"
#define LOGFILE "../error-%u.log"
#endif

#ifndef INI
#error There is no path to conn.ini defined...
#endif

#define MAX_FORM_BUFFER 10240

namespace FastCGI
{
	SessionPtr Session::fromDB(db::ConnectionPtr db, const char* sessionId)
	{
		/*
		I want:

		query = schema
			.select("user._id AS _id", "user.name AS name", "user.email AS email", "session.set_on AS set_on")
			.leftJoin("session", "user")
			.where("session.has=?");

		query.bind(0, sessionId).run(db);
		if (c.next())
		{
			c[0].as<long long>();
			c["_id"].as<long long>();
		}
		*/
		db::StatementPtr query = db->prepare(
			"SELECT user._id AS _id, user.name AS name, user.email AS email, session.set_on AS set_on "
			"FROM session "
			"LEFT JOIN user ON (user._id = session.user_id) "
			"WHERE session.hash=?"
			);

		if (query.get() && query->bind(0, sessionId))
		{
			db::CursorPtr c = query->query();
			if (c.get() && c->next())
			{
				return SessionPtr(new (std::nothrow) Session(
					c->getLongLong(0),
					c->getText(1),
					c->getText(2),
					sessionId,
					c->getTimestamp(3)));
			}
		}
		return SessionPtr();
	}

	SessionPtr Session::startSession(db::ConnectionPtr db, const char* email)
	{
		tyme::time_t now = tyme::now();
		char seed[20];
		crypt::session_t sessionId;
		crypt::newSalt(seed);
		crypt::session(seed, sessionId);

		const char* SQL_USER_BY_EMAIL =
			"SELECT _id, name "
			"FROM user "
			"WHERE email=?"
			;
		const char* SQL_NEW_SESSION = "INSERT INTO session (hash, seed, user_id, set_on) VALUES (?, ?, ?, ?)";

		db::StatementPtr query = db->prepare(SQL_USER_BY_EMAIL);

		if (query.get() && query->bind(0, email))
		{
			db::CursorPtr c = query->query();
			if (c.get() && c->next())
			{
				long long _id = c->getLongLong(0);
				std::string name = c->getText(1);
				c = db::CursorPtr();
				query = db->prepare(
					SQL_NEW_SESSION
					);
				if (query.get() &&
					query->bind(0, sessionId) &&
					query->bind(1, seed) &&
					query->bind(2, _id) &&
					query->bindTime(3, now)
					)
				{
					if (query->execute())
					{
						return SessionPtr(new (std::nothrow) Session(
							_id,
							name,
							email,
							sessionId,
							now
							));
					}
					else
					{
						const char* error = query->errorMessage();
						if (!error) error = "";
						FLOG << "DB error: " << SQL_NEW_SESSION << " " << error << "\n";
					}
				}
				else
				{
					const char* error = db->errorMessage();
					if (!error) error = "";
					FLOG << "DB error: " << SQL_NEW_SESSION << " " << error << "\n";
				}
			}
		}
		else
		{
			const char* error = query.get() ? query->errorMessage() : db->errorMessage();
			if (!error) error = "";
			FLOG << "DB error: " << SQL_USER_BY_EMAIL << " " << error << "\n";
		}
		return SessionPtr();
	}

	void Session::endSession(db::ConnectionPtr db, const char* sessionId)
	{
		db::StatementPtr query = db->prepare("DELETE FROM session WHERE hash=?");
		if (query.get() && query->bind(0, sessionId))
			query->execute();
	}

	namespace { Application* g_app = nullptr; }
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

		FCGX_InitRequest(&m_request, 0, 0);
		m_locale.init(localeRoot);

		char filename[200];
		sprintf(filename, LOGFILE, m_pid);
		m_log.open(filename);
		return m_log.is_open() ? 0 : 1;
	}

	bool Application::accept()
	{
		bool ret = FCGX_Accept_r(&m_request) == 0;
#if DEBUG_CGI
		if (ret)
		{
			ReqInfo info;
			info.resource    = FCGX_GetParam("REQUEST_URI", m_request.envp);
			info.server      = FCGX_GetParam("SERVER_NAME", m_request.envp);
			info.remote_addr = FCGX_GetParam("REMOTE_ADDR", m_request.envp);
			info.remote_port = FCGX_GetParam("REMOTE_PORT", m_request.envp);
			info.now = tyme::now();
			m_requs.push_back(info);
		}
#endif
		return ret;
	}

	db::ConnectionPtr Application::dbConn(Request& request)
	{
		// synchronized {

		//restart if needed
		if (!m_dbConn.get())
			m_dbConn = db::Connection::open(INI);
		else if (!m_dbConn->isStillAlive())
			m_dbConn->reconnect();

		if (!m_dbConn.get() || !m_dbConn->isStillAlive())
			request.on500();

		return m_dbConn;

		// synchronized }
	}

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

	SessionPtr Application::getSession(Request& request, const std::string& sessionId)
	{
		// synchronized {
		cleanSessionCache();

		SessionPtr out;
		Sessions::iterator _it = m_sessions.find(sessionId);
		if (_it == m_sessions.end() || !_it->second.second.get())
		{
			db::ConnectionPtr db = dbConn(request);
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
		// synchronized }
	}

	SessionPtr Application::startSession(Request& request, const char* email)
	{
		// synchronized {
		cleanSessionCache();

		SessionPtr out;
		db::ConnectionPtr db = dbConn(request);
		if (db.get())
			out = Session::startSession(db, email);
		// TODO: limits needed, or DoS eminent
		if (out.get())
			m_sessions.insert(std::make_pair(out->getSessionId(), std::make_pair(out->getStartTime(), out)));

		return out;
		// synchronized }
	}

	void Application::endSession(Request& request, const std::string& sessionId)
	{
		db::ConnectionPtr db = dbConn(request);
		if (db.get())
			Session::endSession(db, sessionId.c_str());
		Sessions::iterator _it = m_sessions.find(sessionId);
		if (_it != m_sessions.end())
			m_sessions.erase(_it);
	}

	ApplicationLog::ApplicationLog(const char* file, int line)
		: m_log(g_app->log())
	{
		// lock
		m_log << file << ":" << line << " ";
	}

	ApplicationLog::~ApplicationLog()
	{
		m_log << std::endl;
		// unlock
	}

	bool PageTranslation::init(SessionPtr session, Request& request)
	{
		if (session.get() != nullptr)
		{
			m_translation = session->getTranslation();
			if (m_translation.get() != nullptr)
				return true;
		}

		m_translation = request.httpAcceptLanguage();
		if (session.get() != nullptr)
			session->setTranslation(m_translation);

		return m_translation.get() != nullptr;
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
		if (m_translation.get() == nullptr)
			return error(m_badString, stringId);
		const char* str = m_translation->tr(stringId);
		if (!str)
			return error(m_badString, stringId);
		return str;
	}

	Request::Request(Application& app)
		: m_app(app)
		, m_headersSent(false)
		, m_streambufCin(app.m_request.in)
		, m_streambufCout(app.m_request.out)
		, m_cerr(app.m_request.err)
		, m_cin(&m_streambufCin)
		, m_cout(&m_streambufCout)
		, cerr(&m_cerr)
		, m_alreadyReadSomething(false)
	{
		unpackCookies();
		unpackVariables();
	}

	Request::~Request()
	{
		readAll();
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

				char * buffer = (char*)malloc((size_t)length);
				if (buffer)
				{
					if (read(buffer, length) == length)
						unpackVariables(buffer, (size_t)length);
					free(buffer);
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
	}

	void Request::readAll()
	{
		m_alreadyReadSomething = true;
		// ignore() doesn't set the eof bit in some versions of glibc++
		// so use gcount() instead of eof()...
		do m_cin.ignore(1024); while (m_cin.gcount() == 1024);
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

		std::string domAndPath = "; Version=1; Domain=";
		domAndPath += getParam("SERVER_NAME");
		domAndPath += "; Path=/; HttpOnly";

		bool first = true;
		std::for_each(m_respCookies.begin(), m_respCookies.end(), [&first, &domAndPath, &cookies](const ResponseCookies::value_type& cookie)
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
		});

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

		std::for_each(m_headers.begin(), m_headers.end(), [&](const Headers::value_type& header)
		{
			*this << header.second;
		});

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
		fcgi::param_t port = getParam("SERVER_PORT");
		fcgi::param_t server = getParam("SERVER_NAME");
		fcgi::param_t query = withQuery ? getParam("QUERY_STRING") : nullptr;
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

	void Request::on404()
	{
		setHeader("Status", "404 Not Found");
		*this
			<< "<tt>404: Oops! (URL: " << getParam("REQUEST_URI") << ")</tt>";
#if DEBUG_CGI
		*this << "<br/>\n<a href='/debug/'>Debug</a>.";
#endif
		die();
	}

	void Request::on500()
	{
		setHeader("Status", "500 Internal Error");
		*this
			<< "<tt>500: Oops! (URL: " << getParam("REQUEST_URI") << ")</tt>";
#if DEBUG_CGI
		*this << "<br/>\n<a href='/debug/'>Debug</a>.";
#endif
		die();
	}

	SessionPtr Request::getSession(bool require)
	{
		SessionPtr out;
		param_t sessionId = getCookie("reader.login");
		if (sessionId && *sessionId)
			out = m_app.getSession(*this, sessionId);
		if (require && out.get() == nullptr)
		{
			std::string cont = serverUri(getParam("REQUEST_URI"), false);
			redirect("/auth/login?continue=" + url::encode(cont));
		}

		//if require and session two weeks old: /auth/login?continue=<this-url>&user=<email>

		return out;
	}

	SessionPtr Request::startSession(bool long_session, const char* email)
	{
		SessionPtr session = m_app.startSession(*this, email);
		if (session.get())
		{
			if (long_session)
				setCookie("reader.login", session->getSessionId(), tyme::now() + 30 * 24 * 60 * 60);
			else
				setCookie("reader.login", session->getSessionId());
		}
		return session;
	}

	void Request::endSession(const std::string& sessionId)
	{
		m_app.endSession(*this, sessionId);
		setCookie("reader.login", "", tyme::now());
	}

	lng::TranslationPtr Request::httpAcceptLanguage()
	{
		param_t HTTP_ACCEPT_LANGUAGE = getParam("HTTP_ACCEPT_LANGUAGE");
		if (!HTTP_ACCEPT_LANGUAGE) HTTP_ACCEPT_LANGUAGE = "";
		return m_app.httpAcceptLanguage(HTTP_ACCEPT_LANGUAGE);
	}

	void Request::sendMail(const char* mailFile, const char* email)
	{
		param_t HTTP_ACCEPT_LANGUAGE = getParam("HTTP_ACCEPT_LANGUAGE");
		if (!HTTP_ACCEPT_LANGUAGE) HTTP_ACCEPT_LANGUAGE = "";
		std::string path = m_app.getLocalizedFilename(HTTP_ACCEPT_LANGUAGE, mailFile);
	}
}