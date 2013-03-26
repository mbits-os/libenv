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

#ifndef __DBCONN_H__
#define __DBCONN_H__

#include <memory>
#include <utils.h>

namespace db
{
	struct Cursor;
	struct Statement;
	struct Connection;
	typedef std::shared_ptr<Connection> ConnectionPtr;
	typedef std::shared_ptr<Statement> StatementPtr;
	typedef std::shared_ptr<Cursor> CursorPtr;

	struct Cursor
	{
		virtual ~Cursor() {}
		virtual bool next() = 0;
		virtual size_t columnCount() = 0;
		virtual int getInt(int column) = 0;
		virtual long getLong(int column) = 0;
		virtual long long getLongLong(int column) = 0;
		virtual tyme::time_t getTimestamp(int column) = 0;
		virtual const char* getText(int column) = 0;
		virtual bool isNull(int column) = 0;
	};

	struct Statement
	{
		virtual ~Statement() {}
		virtual bool bind(int arg, int value) = 0;
		virtual bool bind(int arg, short value) = 0;
		virtual bool bind(int arg, long value) = 0;
		virtual bool bind(int arg, long long value) = 0;
		virtual bool bind(int arg, const char* value) = 0;
		virtual bool bindTime(int arg, tyme::time_t value) = 0;
		virtual bool execute() = 0;
		virtual CursorPtr query() = 0;
		virtual const char* errorMessage() = 0;
	};

	struct Connection
	{
		virtual ~Connection() {}
		virtual bool isStillAlive() = 0;
		virtual bool beginTransaction() = 0;
		virtual bool rollbackTransaction() = 0;
		virtual bool commitTransaction() = 0;
		virtual bool exec(const char* sql) = 0;
		virtual StatementPtr prepare(const char* sql) = 0;
		virtual const char* errorMessage() = 0;
		virtual bool reconnect() = 0;
		static ConnectionPtr open(const char* path);
	};

	struct Transaction
	{
		enum State
		{
			UNKNOWN,
			BEGAN,
			COMMITED,
			REVERTED
		};

		State m_state;
		ConnectionPtr m_conn;
		Transaction(ConnectionPtr conn): m_state(UNKNOWN), m_conn(conn) {}
		~Transaction()
		{
			if (m_state == BEGAN)
				m_conn->rollbackTransaction();
		}
		bool begin()
		{
			if (m_state != UNKNOWN)
				return false;
			if (!m_conn->beginTransaction())
				return false;
			m_state = BEGAN;
			return true;
		}
		bool commit()
		{
			if (m_state != BEGAN)
				return false;
			m_state = COMMITED;
			return m_conn->commitTransaction();
		}
		bool rollback()
		{
			if (m_state != BEGAN)
				return false;
			m_state = REVERTED;
			return m_conn->rollbackTransaction();
		}
	};

	struct environment
	{
		bool failed;
		environment();
		~environment();
	};
};

#endif //__DBCONN_H__
