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

	enum
	{
		UI_VIEW_ONLY_UNREAD  = 1,
		UI_VIEW_ONLY_TITLES = 2,
		UI_VIEW_OLDEST_FIRST = 4
	};

	class Session
	{
		long long m_id;
		std::string m_login;
		std::string m_name;
		std::string m_email;
		std::string m_hash;
		bool m_isAdmin;
		std::string m_preferredLanguage;
		tyme::time_t m_setOn;
		uint32_t m_flags;
		lng::TranslationPtr m_tr;

		void flags(int mask, int value)
		{
			m_flags &= ~mask;
			m_flags |= value;
		}

		bool flags(int mask) const
		{
			return (m_flags & mask) == mask;
		}

		void flags_set(int bit, bool set)
		{
			flags(bit, set ? bit : 0);
		}
	public:
		Session()
		{
		}
		Session(long long id, const std::string& login, const std::string& name, const std::string& email,
			const std::string& hash, bool isAdmin, const std::string& preferredLanguage, uint32_t flags, tyme::time_t setOn)
			: m_id(id)
			, m_login(login)
			, m_name(name)
			, m_email(email)
			, m_hash(hash)
			, m_isAdmin(isAdmin)
			, m_preferredLanguage(preferredLanguage)
			, m_flags(flags)
			, m_setOn(setOn)
		{
		}

		static SessionPtr fromDB(const db::ConnectionPtr& db, const char* sessionId);
		static SessionPtr startSession(const db::ConnectionPtr& db, const char* login);
		static void endSession(const db::ConnectionPtr& db, const char* sessionId);
		long long getId() const { return m_id; }
		const std::string& getLogin() const { return m_login; }
		const std::string& getName() const { return m_name; }
		const std::string& getEmail() const { return m_email; }
		const std::string& getSessionId() const { return m_hash; }
		bool isAdmin() const { return m_isAdmin; }
		const std::string& preferredLanguage() const { return m_preferredLanguage; }
		void preferredLanguage(const std::string& lang) { m_preferredLanguage = lang; }
		void storeLanguage(const db::ConnectionPtr& db);

		bool viewOnlyUnread() const  { return flags(UI_VIEW_ONLY_UNREAD); }
		bool viewOnlyTitles() const  { return flags(UI_VIEW_ONLY_TITLES); }
		bool viewOldestFirst() const { return flags(UI_VIEW_OLDEST_FIRST); }
		void setViewOnlyUnread(bool value = true)  { flags_set(UI_VIEW_ONLY_UNREAD, value); }
		void setViewOnlyTitles(bool value = true)  { flags_set(UI_VIEW_ONLY_TITLES, value); }
		void setViewOldestFirst(bool value = true) { flags_set(UI_VIEW_OLDEST_FIRST, value); }
		void storeFlags(const db::ConnectionPtr& db);

		tyme::time_t getStartTime() const { return m_setOn; }
		lng::TranslationPtr getTranslation() { return m_tr; }
		void setTranslation(const lng::TranslationPtr& tr) { m_tr = tr; }
		long long createFolder(const db::ConnectionPtr& db, const char* name, long long parent = 0);
		long long subscribe(const db::ConnectionPtr& db, const char* url, long long folder = 0);
	};
}

#endif //__SESSION_HPP__