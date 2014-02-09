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

#ifndef __SESSION_HPP__
#define __SESSION_HPP__

#include <mt.hpp>
#include <utils.hpp>

namespace db
{
	struct Connection;
	typedef std::shared_ptr<Connection> ConnectionPtr;
}

namespace lng
{
	class Translation;
	typedef std::shared_ptr<Translation> TranslationPtr;
};

namespace FastCGI
{
	class Session;
	typedef std::shared_ptr<Session> SessionPtr;

	enum SUBSCRIBE_ERROR
	{
		SERR_INTERNAL_ERROR = -500,
		SERR_4xx_ANSWER = -4,
		SERR_5xx_ANSWER = -5,
		SERR_OTHER_ANSWER = -6,
		SERR_NOT_A_FEED = -7
	};

	enum ENTRY_STATE
	{
		ENTRY_UNREAD,
		ENTRY_STARRED,
		ENTRY_IMPORTANT
	};

	class Session
	{
		long long m_id;
		std::string m_login;
		std::string m_name;
		std::string m_email;
		std::string m_hash;
		tyme::time_t m_setOn;
		lng::TranslationPtr m_tr;
	public:
		Session()
		{
		}
		Session(long long id, const std::string& login, const std::string& name, const std::string& email, const std::string& hash, tyme::time_t setOn)
			: m_id(id)
			, m_login(login)
			, m_name(name)
			, m_email(email)
			, m_hash(hash)
			, m_setOn(setOn)
		{
		}

		static SessionPtr stlSession();
		static SessionPtr fromDB(db::ConnectionPtr db, const char* sessionId);
		static SessionPtr startSession(db::ConnectionPtr db, const char* email);
		static void endSession(db::ConnectionPtr db, const char* sessionId);
		long long getId() const { return m_id; }
		const std::string& getLogin() const { return m_login; }
		const std::string& getName() const { return m_name; }
		const std::string& getEmail() const { return m_email; }
		const std::string& getSessionId() const { return m_hash; }
		tyme::time_t getStartTime() const { return m_setOn; }
		lng::TranslationPtr getTranslation() { return m_tr; }
		void setTranslation(lng::TranslationPtr tr) { m_tr = tr; }
		long long createFolder(db::ConnectionPtr db, const char* name, long long parent = 0);
		long long subscribe(db::ConnectionPtr db, const char* url, long long folder = 0);
	};
}

#endif //__SESSION_HPP__