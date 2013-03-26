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

#ifndef __MYSQL_HPP__
#define __MYSQL_HPP__

#include <dbconn.h>
#include <dbconn_driver.h>

#ifdef _WIN32
#include <WinSock2.h>
#include <mysql.h>
#else
#include <mysql/mysql.h>
#endif

#include <string.h>

namespace db
{
	namespace mysql
	{
		class MySQLBinding
		{
		protected:
			MYSQL *m_mysql;
			MYSQL_STMT* m_stmt;
			MYSQL_BIND *m_bind;
			char **m_buffers;
			size_t m_count;
			MySQLBinding(MYSQL *mysql, MYSQL_STMT* stmt)
				: m_mysql(mysql)
				, m_stmt(stmt)
				, m_bind(nullptr)
				, m_buffers(nullptr)
				, m_count(0)
			{
			}
			~MySQLBinding()
			{
				deleteBind();
			}

			void deleteBind()
			{
				delete [] m_bind;
				m_bind = nullptr;
				for (size_t i = 0 ; i < m_count; ++i)
					delete [] m_buffers[i];
				delete [] m_buffers;

				m_buffers = nullptr;
				m_count = 0;
			}

			bool allocBind(size_t count)
			{
				MYSQL_BIND *bind = new (std::nothrow) MYSQL_BIND[count];
				char **buffers = new (std::nothrow) char*[count];

				if (!bind || !buffers)
				{
					delete [] bind;
					delete [] buffers;
					return false;
				}

				deleteBind();

				memset(bind, 0, sizeof(MYSQL_BIND) * count);
				memset(buffers, 0, sizeof(char*) * count);
				m_bind = bind;
				m_buffers = buffers;
				m_count = count;

				return true;
			}
		};

		class MySQLCursor: public Cursor, MySQLBinding
		{
			unsigned long *m_lengths;
			my_bool	   *m_is_null;
			my_bool	   *m_error;
			bool allocBind(size_t count);
			void deleteBind()
			{
				delete [] m_lengths;
				delete [] m_is_null;
				delete [] m_error;
			}
			static bool bindResult(const MYSQL_FIELD& field, MYSQL_BIND& bind, char*& buffer);
		public:
			MySQLCursor(MYSQL *mysql, MYSQL_STMT *stmt)
				: MySQLBinding(mysql, stmt)
				, m_lengths(nullptr)
				, m_is_null(nullptr)
				, m_error(nullptr)
			{
			}
			~MySQLCursor()
			{
				deleteBind();
			}
			bool prepare();
			bool next();
			size_t columnCount();
			int getInt(int column) { return getLong(column); }
			long getLong(int column);
			long long getLongLong(int column);
			tyme::time_t getTimestamp(int column);
			const char* getText(int column);
			bool isNull(int column);
		};

		class MySQLStatement: public Statement, MySQLBinding
		{
		public:
			MySQLStatement(MYSQL *mysql, MYSQL_STMT *stmt)
				: MySQLBinding(mysql, stmt)
			{
			}
			~MySQLStatement()
			{
				mysql_stmt_close(m_stmt);
				m_stmt = nullptr;

			}
			bool prepare(const char* stmt);
			bool bind(int arg, int value) { return bind(arg, (long)value); }
			bool bind(int arg, short value);
			bool bind(int arg, long value);
			bool bind(int arg, long long value);
			bool bind(int arg, const char* value);
			bool bindTime(int arg, tyme::time_t value);
			template <class T>
			bool bindImpl(int arg, const T& value)
			{
				if (!bindImpl(arg, &value, sizeof(T)))
					return false;
				*((T*)m_buffers[arg]) = value;
				return true;
			}
			bool bindImpl(int arg, const void* value, size_t len);
			bool execute();
			CursorPtr query();
			const char* errorMessage();
		};

		class MySQLConnection: public Connection
		{
			MYSQL m_mysql;
			bool m_connected;
			std::string m_path;
		public:
			MySQLConnection(const std::string& path);
			~MySQLConnection();
			bool connect(const std::string& user, const std::string& password, const std::string& server, const std::string& database);
			bool isStillAlive();
			bool reconnect();
			bool beginTransaction();
			bool rollbackTransaction();
			bool commitTransaction();
			StatementPtr prepare(const char* sql);
			bool exec(const char* sql);
			const char* errorMessage();
		};

		class MySQLDriver: public Driver
		{
			ConnectionPtr open(const std::string& ini_path, const Props& props);
		};
	}
}

#endif //__MYSQL_HPP__
