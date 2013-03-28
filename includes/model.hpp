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

#ifndef __DBCONN_MODEL_H__
#define __DBCONN_MODEL_H__

#include <string>
#include <dbconn.hpp>

namespace data
{
	typedef long long key_t;

	class Feed;
	class Entry;
	class User;
	struct Users;
	class Session;

	class Entry
	{
		friend class Feed;
		Entry(key_t feed_id);

		// hiding
		Entry() {}
		Entry& operator=(const Entry&) { return *this; }
	public:
		Entry(const Entry&) {}
		bool store();
	};

	class Feed
	{
	public:
		Entry newEntry();
	};

	class User
	{
		friend struct Users;
		bool m_valid;
		long long m_id;
		std::string m_name;
		std::string m_mail;
		std::string m_hash;
	public:
		User(): m_valid(false), m_id(0) {}
		bool isValid() const { return m_valid; }
		long long id() const { return m_id; }
		const std::string& name() const { return m_name; }
		const std::string& mail() const { return m_mail; }
		const std::string& hash() const { return m_hash; }
		bool isPasswordValid(const char* password);
	};

	struct Environment
	{
		virtual db::ConnectionPtr db() = 0;
		virtual void showLogin() = 0;
		virtual bool setCookie(const std::string& name, const std::string& value, time_t expire = 0) = 0;
		virtual std::string getCookie(const std::string& name) = 0;
	};

	struct Users
	{
		static bool getUser(db::ConnectionPtr ptr, const std::string& email, User& out);
	};

	class Session
	{
		Environment* m_env;
		bool m_looked_user_up;
		User m_user;

		std::string getCurrentLogin();
	public:
		Session(Environment* env)
			: m_env(env)
			, m_looked_user_up(false)
		{}
		const User& getUser();
		static Session* getSession();
	};

};

#endif //__DBCONN_MODEL_H__
