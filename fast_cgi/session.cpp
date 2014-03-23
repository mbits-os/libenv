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
#include <db/conn.hpp>
#include <crypt.hpp>

namespace FastCGI
{
	void reportError(const char* file, int line, db::ErrorReporter* rep, const char* sql)
	{
		const char* error = rep->errorMessage();
		long errorId = rep->errorCode();
		if (!error) error = "(none)";
		FastCGI::ApplicationLog(file, line) << "DB error " << errorId << ": " << error << " (" << sql << ")\n";
	}

#define REPORT_ERROR(rep, sql) reportError(__FILE__, __LINE__, rep, sql)

	ProfilePtr make_profile(const db::ConnectionPtr& db, const std::string& login)
	{
		static const char* SQL_READ_PROFILE =
			"SELECT _id, email, name, family_name, display_name, lang, avatar_engine "
			"FROM profile "
			"WHERE login=?";

		auto profile = db->prepare(SQL_READ_PROFILE);
		if (profile)
		{
			if (profile->bind(0, login))
			{
				auto c = profile->query();
				if (c && c->next())
				{
					long long _id = c->getLongLong(0);
					//std::string login = c->getText();
					std::string email = c->getText(1);
					std::string name = c->getText(2);
					std::string familyName = c->getText(3);
					std::string displayName = c->getText(4);
					std::string preferredLanguage = c->isNull(5) ? std::string() : c->getText(5);
					std::string avatarEngine = c->isNull(6) ? std::string() : c->getText(6);

					return std::make_shared<Profile>(
						_id, login, email, name, familyName, displayName, preferredLanguage, avatarEngine
						);
				}
				else
				{
					REPORT_ERROR(profile.get(), SQL_READ_PROFILE);
				}
			}
			else
			{
				REPORT_ERROR(profile.get(), SQL_READ_PROFILE);
			}
		}
		else
		{
			REPORT_ERROR(db.get(), SQL_READ_PROFILE);
		}
		return nullptr;
	}

	SessionPtr Session::fromDB(const db::ConnectionPtr& db, const UserInfoFactoryPtr& userInfoFactory, const char* sessionId)
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

		const char* SQL_READ_SESSION = "SELECT user_id, set_on FROM session WHERE hash=?";
		auto select = db->prepare(SQL_READ_SESSION);

		if (select)
		{
			if (select->bind(0, sessionId))
			{
				auto c = select->query();
				if (c && c->next())
				{
					auto userId = c->getLongLong(0);
					auto setOn = c->getTimestamp(1);
					c.reset();

					auto userInfo = userInfoFactory->fromId(db, userId);
					if (userInfo)
					{
						auto profile = make_profile(db, userInfo->login());
						if (profile)
						{
							return std::make_shared<Session>(profile, userInfo, sessionId, setOn);
						}
					}
				}
				else
				{
					REPORT_ERROR(select.get(), SQL_READ_SESSION);
				}
			}
			else
			{
				REPORT_ERROR(select.get(), SQL_READ_SESSION);
			}
		}
		else
		{
			REPORT_ERROR(db.get(), SQL_READ_SESSION);
		}
		return nullptr;
	}

	SessionPtr Session::startSession(const db::ConnectionPtr& db, const UserInfoFactoryPtr& userInfoFactory, const char* login)
	{
		tyme::time_t now = tyme::now();
		char seed[20];
		Crypt::session_t sessionId;
		Crypt::newSalt(seed);
		Crypt::session(seed, sessionId);

		const char* SQL_NEW_SESSION = "INSERT INTO session (hash, seed, user_id, set_on) VALUES (?, ?, ?, ?)";

		auto profile = make_profile(db, login);
		if (profile)
		{
			auto userInfo = userInfoFactory->fromLogin(db, profile->login());
			if (userInfo)
			{
				auto insert = db->prepare(SQL_NEW_SESSION);
				if (insert)
				{
					if (
						insert->bind(0, sessionId) &&
						insert->bind(1, seed) &&
						insert->bind(2, userInfo->userId()) &&
						insert->bindTime(3, now) &&
						insert->execute()
						)
					{
						return std::make_shared<Session>(profile, userInfo, sessionId, now);
					}
					else
					{
						REPORT_ERROR(insert.get(), SQL_NEW_SESSION);
					}
				}
				else
				{
					REPORT_ERROR(db.get(), SQL_NEW_SESSION);
				}
			}
		}
		return nullptr;
	}

	void Session::endSession(const db::ConnectionPtr& db, const char* sessionId)
	{
		db::StatementPtr query = db->prepare("DELETE FROM session WHERE hash=?");
		if (query && query->bind(0, sessionId))
			query->execute();
	}

	bool bindTextOrNull(const db::StatementPtr& query, int arg, const std::string& text)
	{
		return text.empty() ? query->bindNull(arg) : query->bind(arg, text);
	}
	void Profile::storeLanguage(const db::ConnectionPtr& db)
	{
		db::StatementPtr query = db->prepare("UPDATE profile SET lang=? WHERE login=?");
		if (query && bindTextOrNull(query, 0, m_preferredLanguage) && query->bind(1, m_login))
			query->execute();
	}

	void Profile::updateData(const db::ConnectionPtr& db, const std::map<std::string, std::string>& changed)
	{
		struct
		{
			const char* dataName;
			const char* sqlName;
			std::string Profile::* prop;
		} props[] = {
			{ "email",        "email",         &Profile::m_email },
			{ "name",         "name",          &Profile::m_name },
			{ "family_name",  "family_name",   &Profile::m_familyName },
			{ "display_name", "display_name",  &Profile::m_displayName },
			{ "avatar",       "avatar_engine", &Profile::m_avatarEngine }
		};

		std::vector<std::string> bindVals;

		std::ostringstream o;
		o << "UPDATE profile SET ";
		for (auto&& pair : changed)
		{
			for (auto&& prop : props)
			{
				if (pair.first == prop.dataName)
				{
					if (!bindVals.empty())
						o << ", ";
					o << prop.sqlName << "=?";
					this->*prop.prop = pair.second;
					bindVals.push_back(pair.second);
					break;
				}
			}
		}

		o << " WHERE _id=?";

		auto sql = o.str();

		auto update = db->prepare(sql.c_str());
		if (!update)
			return;

		int ndx = -1;
		for (auto&& val : bindVals)
		{
			if (!update->bind(++ndx, val))
				return;
		}
		if (!update->bind(++ndx, m_profileId))
			return;

		update->execute();
	}
}
