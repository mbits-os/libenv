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
#include <fast_cgi/session.hpp>
#include <dbconn.hpp>
#include <crypt.hpp>

namespace FastCGI
{
	SessionPtr Session::stlSession()
	{
		return SessionPtr(new (std::nothrow) Session(
			0,
			"stl.test",
			"STL Test",
			"noone@example.com",
			"...",
			tyme::now()
			));
	}

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
			"SELECT user._id AS _id, user.login AS login, user.name AS name, user.email AS email, session.set_on AS set_on "
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
					c->getText(3),
					sessionId,
					c->getTimestamp(4)));
			}
		}
		return SessionPtr();
	}

	SessionPtr Session::startSession(db::ConnectionPtr db, const char* email)
	{
		tyme::time_t now = tyme::now();
		char seed[20];
		Crypt::session_t sessionId;
		Crypt::newSalt(seed);
		Crypt::session(seed, sessionId);

		const char* SQL_USER_BY_EMAIL =
			"SELECT _id, login, name "
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
				std::string login = c->getText(1);
				std::string name = c->getText(2);
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
							login,
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
}
