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

#if defined(_MSC_VER)
#	define DEPRECATED(fun) __declspec(deprecated) fun
#elif defined(__GNUC__)
#	define DEPRECATED(fun) fun __attribute__((deprecated))
#else
#	define DEPRECATED(fun) fun [[deprecated]]
#endif

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

	class Profile
	{
		long long m_profileId = -1;
		std::string m_login;
		std::string m_email;
		std::string m_name;
		std::string m_familyName;
		std::string m_displayName;
		std::string m_preferredLanguage;
		std::string m_avatarEngine;

	public:
		Profile() {}
		Profile(long long id, const std::string& login, const std::string& email,
			const std::string& name, const std::string& familyName, const std::string& displayName,
			const std::string& preferredLanguage, const std::string& avatarEngine)
			: m_profileId(id)
			, m_login(login)
			, m_email(email)
			, m_name(name)
			, m_familyName(familyName)
			, m_displayName(displayName)
			, m_preferredLanguage(preferredLanguage)
			, m_avatarEngine(avatarEngine)
		{
		}

		long long profileId() const { return m_profileId; }
		const std::string& login() const { return m_login; }
		const std::string& email() const { return m_email; }
		const std::string& name() const { return m_name; }
		const std::string& familyName() const { return m_familyName; }
		const std::string& displayName() const { return m_displayName; }
		const std::string& preferredLanguage() const { return m_preferredLanguage; }
		void preferredLanguage(const std::string& lang) { m_preferredLanguage = lang; }
		const std::string& avatarEngine() const { return m_avatarEngine; }

		void storeLanguage(const db::ConnectionPtr& db);
		void updateData(const db::ConnectionPtr& db, const std::map<std::string, std::string>& changed);
	};
	using ProfilePtr = std::shared_ptr<Profile>;

	struct FlagsHelper
	{
		uint32_t m_flags = 0;

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

		FlagsHelper() {}
		FlagsHelper(uint32_t flags) : m_flags(flags) {}
	};

	struct UserInfo
	{
		virtual ~UserInfo() {}
		virtual long long userId() const = 0;
		virtual const std::string& login() const = 0;
	};
	using UserInfoPtr = std::shared_ptr<UserInfo>;

	struct UserInfoFactory
	{
		virtual ~UserInfoFactory() {}
		virtual UserInfoPtr fromId(const db::ConnectionPtr& db, long long id) = 0;
		virtual UserInfoPtr fromLogin(const db::ConnectionPtr& db, const std::string& login) = 0;
	};
	using UserInfoFactoryPtr = std::shared_ptr<UserInfoFactory>;

	class Session
	{
		ProfilePtr m_profile;
		UserInfoPtr m_userInfo;
		std::string m_hash;
		tyme::time_t m_setOn;
		lng::TranslationPtr m_tr;
	public:
		Session()
		{
		}
		Session(const ProfilePtr& profile, const UserInfoPtr& userInfo, const std::string& hash, tyme::time_t setOn)
			: m_profile(profile)
			, m_userInfo(userInfo)
			, m_hash(hash)
			, m_setOn(setOn)
		{
		}

		static SessionPtr fromDB(const db::ConnectionPtr& db, const UserInfoFactoryPtr& userInfoFactory, const char* sessionId);
		static SessionPtr startSession(const db::ConnectionPtr& db, const UserInfoFactoryPtr& userInfoFactory, const char* login);
		static void endSession(const db::ConnectionPtr& db, const char* sessionId);

		const std::string& getSessionId() const { return m_hash; }

		tyme::time_t getStartTime() const { return m_setOn; }
		lng::TranslationPtr getTranslation() { return m_tr; }
		void setTranslation(const lng::TranslationPtr& tr) { m_tr = tr; }

		ProfilePtr profile() const { return m_profile; }
		UserInfoPtr userInfoRaw() const { return m_userInfo; }
		template <typename Impl>
		std::shared_ptr<Impl> userInfo() const { return std::static_pointer_cast<Impl>(m_userInfo); }
	};
}

#endif //__SESSION_HPP__