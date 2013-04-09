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
#include <dom.hpp>
#include <http.hpp>
#include <feed_parser.hpp>

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

	long long Session::createFolder(db::ConnectionPtr db, const char* name, long long parent)
	{
		return SERR_INTERNAL_ERROR;
	}

	int getFeed(const char* url, feed::Feed& feed)
	{
		auto xhr = http::XmlHttpRequest::Create();
		if (!xhr)
			return SERR_INTERNAL_ERROR;

		xhr->open(http::HTTP_GET, url, false);
		xhr->send();

		int status = xhr->getStatus() / 100;
		if (status == 4)      return SERR_4xx_ANSWER;
		else if (status == 5) return SERR_5xx_ANSWER;
		else if (status != 2) return SERR_OTHER_ANSWER;

		// TODO: link with @rel='alternate home' or @rel='alternate' and @type='application/rss+xml' or @type='application/atom+xml' discovery

		dom::XmlDocumentPtr doc = xhr->getResponseXml();
		if (!doc || !feed::parse(doc, feed))
			return SERR_NOT_A_FEED;

		feed.m_self = url;
		feed.m_etag = xhr->getResponseHeader("Etag");
		feed.m_lastModified = xhr->getResponseHeader("Last-Modified");
		return 0;
	}

	inline static const char* nullifier(const std::string& s)
	{
		return s.empty() ? nullptr : s.c_str();
	}

	inline static bool createEntry(const db::ConnectionPtr& db, long long feed_id, const feed::Entry& entry)
	{
		const char* entryUniqueId = nullifier(entry.m_entryUniqueId);
		if (!entryUniqueId)
			return false;

		const char* title = nullifier(entry.m_entry.m_title);
		if (!title)
			title = "";

		auto del = db->prepare("DELETE FROM entry WHERE guid=?");
		if (del || !del->bind(0, entryUniqueId)) return false;
		if (!del->execute()) return false;

		auto insert = db->prepare("INSERT INTO entry (feed_id, guid, title, url, date, author, authorLink, description, contents) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
		if (!insert)
			return false;

		tyme::time_t date = entry.m_dateTime == -1 ? tyme::now() : entry.m_dateTime;

		if (!insert->bind(0, feed_id)) return false;
		if (!insert->bind(1, entryUniqueId)) return false;
		if (!insert->bind(2, nullifier(entry.m_entry.m_title))) return false;
		if (!insert->bind(3, nullifier(entry.m_entry.m_url))) return false;
		if (!insert->bindTime(4, date)) return false;
		if (!insert->bind(5, nullifier(entry.m_author.m_name))) return false;
		if (!insert->bind(6, nullifier(entry.m_author.m_email))) return false;
		if (!insert->bind(7, nullifier(entry.m_description))) return false;
		if (!insert->bind(8, nullifier(entry.m_content))) return false;

		if (!insert->execute()) return false;
		if (entry.m_categories.empty() && entry.m_enclosures.empty())
			return true;
		
		auto guid = db->prepare("SELECT _id FROM entry WHERE guid=?");
		if (!guid || !guid->bind(0, entryUniqueId)) return false;

		auto c = guid->query();
		if (!c || !c->next()) return false;
		long long entry_id = c->getLongLong(0);

		insert = db->prepare("INSERT INTO categories(entry_id, cat) VALUES (?, ?)");
		if (!insert)
			return false;
		if (!insert->bind(0, feed_id)) return false;
		auto cat_it = std::find_if(entry.m_categories.begin(), entry.m_categories.end(), [&insert](const std::string& cat) -> bool
		{
			if (!insert->bind(1, cat.c_str())) return true;
			return !insert->execute();
		});
		if (cat_it != entry.m_categories.end()) return false;

		insert = db->prepare("INSERT INTO enclosure(entry_id, url, mime, length) VALUES (?, ?, ?, ?)");
		if (!insert)
			return false;
		if (!insert->bind(0, feed_id)) return false;
		auto enc_it = std::find_if(entry.m_enclosures.begin(), entry.m_enclosures.end(), [&insert](const feed::Enclosure& enc) -> bool
		{
			if (!insert->bind(1, enc.m_url.c_str())) return true;
			if (!insert->bind(2, enc.m_type.c_str())) return true;
			if (!insert->bind(3, (long long)enc.m_size)) return true;
			return !insert->execute();
		});
		if (enc_it != entry.m_enclosures.end()) return false;
		return true;
	}

	inline static bool createEntries(const db::ConnectionPtr& db, long long feed_id, const feed::Entries& entries)
	{
		auto it = std::find_if(entries.begin(), entries.end(), [&](const feed::Entry& entry) -> bool
		{
			return !createEntry(db, feed_id, entry);
		});
		return it == entries.end();
	}

	inline static bool createFeed(const db::ConnectionPtr& db, const feed::Feed& feed)
	{
		tyme::time_t last_updated = tyme::now();
		auto insert = db->prepare("INSERT INTO feed (title, site, feed, last_update, author, authorLink, etag, last_modified) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
		if (!insert)
			return false;

		if (!insert->bind(0, nullifier(feed.m_feed.m_title))) return false;
		if (!insert->bind(1, nullifier(feed.m_feed.m_url))) return false;
		if (!insert->bind(2, nullifier(feed.m_self))) return false;
		if (!insert->bindTime(3, last_updated)) return false;
		if (!insert->bind(4, nullifier(feed.m_author.m_name))) return false;
		if (!insert->bind(5, nullifier(feed.m_author.m_email))) return false;
		if (!insert->bind(6, nullifier(feed.m_etag))) return false;
		if (!insert->bind(7, nullifier(feed.m_lastModified))) return false;

		return insert->execute();
	}

	long long Session::subscribe(db::ConnectionPtr db, const char* url, long long folder)
	{
		long long feed_id = 0;
		if (!url || !*url)
			return 0;

		db::Transaction transaction(db);

		auto feed_query = db->prepare("SELECT _id FROM feed WHERE feed.feed=?");
		if (!feed_query || !feed_query->bind(0, url)) return SERR_INTERNAL_ERROR;

		auto c = feed_query->query();
		if (!c)
			return SERR_INTERNAL_ERROR;

		if (!c->next())
		{
			//it's a new URL
			feed::Feed feed;
			int result = getFeed(url, feed);
			if (result)
				return result;
			createFeed(db, feed);
			c = feed_query->query();
			if (!c || !c->next())
				return SERR_INTERNAL_ERROR;
			feed_id = c->getLongLong(0);
			createEntries(db, feed_id, feed.m_entry);
		}
		else
			feed_id = c->getLongLong(0);

		if (folder == 0)
		{
			auto select = db->prepare("SELECT root_folder FROM user WHERE _id=?");
			if (!select || !select->bind(0, m_id)) return SERR_INTERNAL_ERROR;
			c = select->query();
			if (!c || !c->next()) return SERR_INTERNAL_ERROR;
			folder = c->getLongLong(0);
		}

		auto max_id = db->prepare("SELECT max(folder_id) FROM subscription WHERE folder_id=?");
		if (!max_id || !max_id->bind(0, folder))
			return SERR_INTERNAL_ERROR;

		long ord = 0;
		c = max_id->query();
		if (c && c->next())
		{
			if (!c->isNull(0))
				ord = c->getLong(0) + 1;
		}

		auto subscription = db->prepare("INSERT INTO subscription (feed_id, folder_id, ord) VALUES (?, ?, ?)");
		if (!subscription) return SERR_INTERNAL_ERROR;
		if (!subscription->bind(0, feed_id)) return SERR_INTERNAL_ERROR;
		if (!subscription->bind(1, folder)) return SERR_INTERNAL_ERROR;
		if (!subscription->bind(2, ord)) return SERR_INTERNAL_ERROR;
		if (!subscription->execute()) return SERR_INTERNAL_ERROR;

		if (!transaction.commit())
			return SERR_INTERNAL_ERROR;

		return feed_id;
	}
}
