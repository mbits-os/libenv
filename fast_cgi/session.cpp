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

#define UNREAD_COUNT 10

namespace FastCGI
{
	SessionPtr Session::stlSession()
	{
		return std::make_shared<Session>(
			2,
			"stl.test",
			"STL Test",
			"noone@example.com",
			"...",
			true,
			std::string(),
			0,
			tyme::now()
			);
	}

	SessionPtr Session::fromDB(const db::ConnectionPtr& db, const char* sessionId)
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
			"SELECT user._id AS _id, user.login AS login, user.name AS name, user.email AS email, user.is_admin AS is_admin, user.lang AS lang, user.prefs AS prefs, session.set_on AS set_on "
			"FROM session "
			"LEFT JOIN user ON (user._id = session.user_id) "
			"WHERE session.hash=?"
			);

		if (query && query->bind(0, sessionId))
		{
			db::CursorPtr c = query->query();
			if (c && c->next())
			{
				return std::make_shared<Session>(
					c->getLongLong(0),
					c->getText(1),
					c->getText(2),
					c->getText(3),
					sessionId,
					c->getInt(4) != 0,
					c->isNull(5) ? std::string() : c->getText(5),
					c->getInt(6),
					c->getTimestamp(7));
			}
		}
		return nullptr;
	}

	void reportError(const char* file, int line, db::ErrorReporter* rep, const char* sql)
	{
		const char* error = rep->errorMessage();
		long errorId = rep->errorCode();
		if (!error) error = "(none)";
		FastCGI::ApplicationLog(file, line) << "DB error " << errorId << ": " << error << " (" << sql << ")\n";
	}

#define REPORT_ERROR(rep, sql) reportError(__FILE__, __LINE__, rep, sql)

	SessionPtr Session::startSession(const db::ConnectionPtr& db, const char* email)
	{
		tyme::time_t now = tyme::now();
		char seed[20];
		Crypt::session_t sessionId;
		Crypt::newSalt(seed);
		Crypt::session(seed, sessionId);

		const char* SQL_USER_BY_EMAIL =
			"SELECT _id, login, name, is_admin, lang, prefs "
			"FROM user "
			"WHERE email=?"
			;
		const char* SQL_NEW_SESSION = "INSERT INTO session (hash, seed, user_id, set_on) VALUES (?, ?, ?, ?)";

		db::StatementPtr query = db->prepare(SQL_USER_BY_EMAIL);

		if (query && query->bind(0, email))
		{
			auto c = query->query();
			if (c && c->next())
			{
				long long _id = c->getLongLong(0);
				std::string login = c->getText(1);
				std::string name = c->getText(2);
				bool isAdmin = c->getInt(3) != 0;
				auto lang = c->isNull(4) ? std::string() : c->getText(4);
				uint32_t prefs = (uint32_t)c->getInt(5);

				c.reset();
				query = db->prepare(
					SQL_NEW_SESSION
					);
				if (query &&
					query->bind(0, sessionId) &&
					query->bind(1, seed) &&
					query->bind(2, _id) &&
					query->bindTime(3, now)
					)
				{
					if (query->execute())
					{
						return std::make_shared<Session>(
							_id,
							login,
							name,
							email,
							sessionId,
							isAdmin,
							lang,
							prefs,
							now
							);
					}
					else
					{
						REPORT_ERROR(query.get(), SQL_NEW_SESSION);
					}
				}
				else
				{
					REPORT_ERROR(query.get(), SQL_NEW_SESSION);
				}
			}
		}
		else
		{
			auto rep = query ? (db::ErrorReporter*)query.get() : db.get();
			REPORT_ERROR(rep, SQL_USER_BY_EMAIL);
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
	void Session::storeLanguage(const db::ConnectionPtr& db)
	{
		db::StatementPtr query = db->prepare("UPDATE user SET lang=? WHERE _id=?");
		if (query && bindTextOrNull(query, 0, m_preferredLanguage) && query->bind(1, m_id))
			query->execute();
	}

	void Session::storeFlags(const db::ConnectionPtr& db)
	{
		db::StatementPtr query = db->prepare("UPDATE user SET prefs=? WHERE _id=?");
		if (query && query->bind(0, (int)m_flags) && query->bind(1, m_id))
			query->execute();
	}

	long long Session::createFolder(const db::ConnectionPtr& db, const char* name, long long parent)
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

		if (xhr->wasRedirected())
			feed.m_self = xhr->getFinalLocation();
		else
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
		const char* title = nullifier(entry.m_entry.m_title);

		if (!entryUniqueId)
			return false;

		if (!title)
			title = "";

		auto del = db->prepare("DELETE FROM entry WHERE guid=?");

		if (!del || !del->bind(0, entryUniqueId))
		{
			FLOG << del->errorMessage();
			return false;
		}

		if (!del->execute())
		{
			FLOG << del->errorMessage();
			return false;
		}

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
		if (!insert->bind(0, entry_id)) return false;
		auto cat_it = std::find_if(entry.m_categories.begin(), entry.m_categories.end(), [&insert](const std::string& cat) -> bool
		{
			if (!insert->bind(1, cat.c_str())) { FLOG << insert->errorMessage(); return true; }
			if (!insert->execute()) { FLOG << insert->errorMessage(); return true; }
			return false;
		});
		if (cat_it != entry.m_categories.end()) return false;

		insert = db->prepare("INSERT INTO enclosure(entry_id, url, mime, length) VALUES (?, ?, ?, ?)");
		if (!insert)
			return false;
		if (!insert->bind(0, entry_id)) return false;
		auto enc_it = std::find_if(entry.m_enclosures.begin(), entry.m_enclosures.end(), [&insert](const feed::Enclosure& enc) -> bool
		{
			if (!insert->bind(1, enc.m_url.c_str())) { FLOG << insert->errorMessage(); return true; }
			if (!insert->bind(2, enc.m_type.c_str())) { FLOG << insert->errorMessage(); return true; }
			if (!insert->bind(3, (long long)enc.m_size)) { FLOG << insert->errorMessage(); return true; }
			if (!insert->execute()) { FLOG << insert->errorMessage(); return true; }
			return false;
		});
		if (enc_it != entry.m_enclosures.end()) return false;

		return true;
	}

	inline static bool createEntries(const db::ConnectionPtr& db, long long feed_id, const feed::Entries& entries)
	{
		auto it = std::find_if(entries.rbegin(), entries.rend(), [&](const feed::Entry& entry) -> bool
		{
			return !createEntry(db, feed_id, entry);
		});
		return it == entries.rend();
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

	static inline int alreadySubscribes(const db::ConnectionPtr& db, long long user_id, long long feed_id)
	{
		auto stmt = db->prepare("SELECT count(*) FROM subscription WHERE feed_id=? AND folder_id IN (SELECT _id FROM folder WHERE user_id=?)");
		if (!stmt) { FLOG << db->errorMessage(); return SERR_INTERNAL_ERROR; }
		if (!stmt->bind(0, feed_id)) { FLOG << stmt->errorMessage(); return SERR_INTERNAL_ERROR; }
		if (!stmt->bind(1, user_id)) { FLOG << stmt->errorMessage(); return SERR_INTERNAL_ERROR; }
		auto c = stmt->query();
		if (!c || !c->next()) { FLOG << stmt->errorMessage(); return SERR_INTERNAL_ERROR; }
		return c->getInt(0);
	}

	static inline long long getRootFolder(const db::ConnectionPtr& db, long long user_id)
	{
		auto select = db->prepare("SELECT root_folder FROM user WHERE _id=?");
		if (!select || !select->bind(0, user_id)) return SERR_INTERNAL_ERROR;
		auto c = select->query();
		if (!c || !c->next()) { FLOG << select->errorMessage(); return SERR_INTERNAL_ERROR; }
		return c->getLongLong(0);
	}

	static inline bool doSubscribe(const db::ConnectionPtr& db, long long feed_id, long long folder_id)
	{
		auto max_id = db->prepare("SELECT max(ord) FROM subscription WHERE folder_id=?");
		if (!max_id || !max_id->bind(0, folder_id))
		{
			FLOG << (max_id ? max_id->errorMessage() : db->errorMessage());
			return false;
		}

		long ord = 0;
		auto c = max_id->query();
		if (c && c->next())
		{
			if (!c->isNull(0))
				ord = c->getLong(0) + 1;
		}

		auto subscription = db->prepare("INSERT INTO subscription (feed_id, folder_id, ord) VALUES (?, ?, ?)");
		if (!subscription) { FLOG << subscription->errorMessage(); return false; }
		if (!subscription->bind(0, feed_id)) { FLOG << subscription->errorMessage(); return false; }
		if (!subscription->bind(1, folder_id)) { FLOG << subscription->errorMessage(); return false; }
		if (!subscription->bind(2, ord)) { FLOG << subscription->errorMessage(); return false; }
		if (!subscription->execute()) { FLOG << subscription->errorMessage(); return false; }
		return true;
	}

	static inline bool unreadLatest(const db::ConnectionPtr& db, long long feed_id, long long user_id, int unread_count)
	{
		auto unread = db->prepare("SELECT _id FROM entry WHERE feed_id=? ORDER BY date DESC", 0, unread_count);
		if (!unread || !unread->bind(0, feed_id))
		{
			FLOG << (unread ? unread->errorMessage() : db->errorMessage());
			return false;
		}

		auto state = db->prepare("INSERT INTO state (user_id, type, entry_id) VALUES (?, ?, ?)");
		if (!state)
		{
			FLOG << db->errorMessage();
			return false;
		}
		if (!state->bind(0, user_id)) { FLOG << state->errorMessage(); return false; }
		if (!state->bind(1, ENTRY_UNREAD)) { FLOG << state->errorMessage(); return false; }

		auto c = unread->query();
		if (!c) { FLOG << db->errorMessage(); return false; }
		while (c->next())
		{
			if (!state->bind(2, c->getLongLong(0))) { FLOG << state->errorMessage(); return false; }
			if (!state->execute()) { FLOG << state->errorMessage(); return false; }
		}

		return true;
	}

	long long Session::subscribe(const db::ConnectionPtr& db, const char* url, long long folder)
	{
		int unread_count = UNREAD_COUNT;
		long long feed_id = 0;
		if (!url || !*url)
			return 0;

		db::Transaction transaction(db);
		if (!transaction.begin()) { FLOG << db->errorMessage(); return SERR_INTERNAL_ERROR; }

		auto feed_query = db->prepare("SELECT _id FROM feed WHERE feed.feed=?");
		if (!feed_query || !feed_query->bind(0, url)) return SERR_INTERNAL_ERROR;

		auto c = feed_query->query();
		if (!c)
			return SERR_INTERNAL_ERROR;

		if (!c->next())
		{
			feed::Feed feed;
			int result = getFeed(url, feed);
			if (result)
				return result;

			unread_count = feed.m_entry.size();

			if (!createFeed(db, feed))
				return SERR_INTERNAL_ERROR;
			c = feed_query->query();
			if (!c || !c->next())
				return SERR_INTERNAL_ERROR;
			feed_id = c->getLongLong(0);
			if (!createEntries(db, feed_id, feed.m_entry))
				return SERR_INTERNAL_ERROR;
		}
		else
		{
			feed_id = c->getLongLong(0);
			//could it be double subcription?
			int result = alreadySubscribes(db, m_id, feed_id);
			if (result < 0)
				return result; //an error
			if (result > 0) //already subscribes
				return feed_id;
		}

		if (folder == 0)
		{
			folder = getRootFolder(db, m_id);
			if (folder < 0)
				return folder;
		}

		if (!doSubscribe(db, feed_id, folder)) return SERR_INTERNAL_ERROR;
		if (!unreadLatest(db, feed_id, m_id, unread_count)) return SERR_INTERNAL_ERROR;

		if (!transaction.commit()) { FLOG << db->errorMessage(); return SERR_INTERNAL_ERROR; }

		return feed_id;
	}
}
