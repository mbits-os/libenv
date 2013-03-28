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
#include <dbconn.hpp>
#include <model.hpp>

namespace data
{
	bool Users::getUser(db::ConnectionPtr ptr, const std::string& email, User& out)
	{
		db::StatementPtr select = ptr->prepare("SELECT _id, name, hash FROM user WHERE email=?");
		if (!select.get())
			return false;

		if (!select->bind(0, email.c_str())) return false;

		db::CursorPtr c = select->query();
		if (!c.get() || !c->next())
			return false;

		out.m_valid = true;
		out.m_id = c->getLongLong(0);
		out.m_name = c->getText(1);
		out.m_mail = email;
		out.m_hash = c->getText(2);

		return out.isValid();
	}

	std::string Session::getCurrentLogin()
	{
		return std::string();
	}

	const User& Session::getUser()
	{
		if (!m_looked_user_up)
		{
			m_looked_user_up = true;
			std::string login = getCurrentLogin();
			bool validUser = false;
			if (!login.empty())
				validUser = Users::getUser(m_env->db(), login, m_user);
			if (!validUser)
				m_env->showLogin();
		}
		return m_user;
	}

	Session* Session::getSession()
	{
		return nullptr;
	}
};