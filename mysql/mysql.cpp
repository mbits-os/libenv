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

#include "dbconn.h"
#include "dbconn/driver.h"

#include <iostream>

#ifdef _WIN32
#include <WinSock2.h>
#endif
#include <mysql.h>

namespace db
{
	namespace mysql
	{
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
			Statement* prepare(const char* sql) { return nullptr; }
			bool exec(const char* sql);
			const char* errorMessage();
		};

		class MySQLDriver: public Driver
		{
			ConnectionPtr open(const std::string& ini_path, const Props& props);
		};
	}
}

namespace db { namespace mysql {
	bool startup_driver()
	{
		REGISTER_DRIVER("mysql", db::mysql::MySQLDriver);
		return mysql_library_init(0, NULL, NULL) == 0;
	}

	void shutdown_driver()
	{
		mysql_library_end();
	}

	struct DriverData
	{
		std::string user;
		std::string password;
		std::string server;
		std::string database;
		bool read(const Driver::Props& props)
		{
			return 
				Driver::getProp(props, "user", user) &&
				Driver::getProp(props, "password", password) &&
				Driver::getProp(props, "server", server) &&
				Driver::getProp(props, "database", database) &&
				!user.empty() && !password.empty() && !server.empty() && !database.empty();
		}
	};

	ConnectionPtr MySQLDriver::open(const std::string& ini_path, const Props& props)
	{
		DriverData data;
		if (!data.read(props))
		{
			std::cerr << "MySQL: invalid configuration";
			return nullptr;
		}

		//std::cerr << "user: " << user << "\npassword: " << password << "\naddress: " << server << std::endl;

		std::tr1::shared_ptr<MySQLConnection> conn(new (std::nothrow) MySQLConnection(ini_path));
		if (conn.get() == nullptr)
			return nullptr;

		if (!conn->connect(data.user, data.password, data.server, data.database))
		{
			std::cerr << "MySQL: cannot connect to " << data.user << "@" << data.server << std::endl;
			return nullptr;
		}

		return std::tr1::static_pointer_cast<Connection>(conn);
	}

	MySQLConnection::MySQLConnection(const std::string& path)
		: m_connected(false)
		, m_path(path)
	{
		mysql_init(&m_mysql);
	}

	MySQLConnection::~MySQLConnection()
	{
		if (m_connected)
			mysql_close(&m_mysql);
	}

	bool MySQLConnection::connect(const std::string& user, const std::string& password, const std::string& server, const std::string& database)
	{
		std::string srvr = server;
		unsigned int port = 0;
		std::string::size_type colon = srvr.rfind(':');
		if (colon != std::string::npos)
		{
			char* ptr;
			port = strtoul(srvr.c_str() + colon + 1, &ptr, 10);
			if (*ptr)
				return m_connected;

			srvr = srvr.substr(0, colon);
		}

		my_bool reconnect = 0;
		mysql_options(&m_mysql, MYSQL_OPT_RECONNECT, &reconnect);

		m_connected = mysql_real_connect(&m_mysql, srvr.c_str(), user.c_str(), password.c_str(), database.c_str(), port, NULL, 0) != NULL;

		return m_connected;
	}

	bool MySQLConnection::reconnect()
	{
		Driver::Props props;
		if (!Driver::readProps(m_path, props))
			return false;

		DriverData data;
		if (!data.read(props))
			return false;

		return connect(data.user, data.password, data.server, data.database);
	}

	bool MySQLConnection::isStillAlive()
	{
		return mysql_ping(&m_mysql) == 0;
	}

	bool MySQLConnection::exec(const char* sql)
	{
		return mysql_query(&m_mysql, sql) == 0;
	}

	const char* MySQLConnection::errorMessage()
	{
		return mysql_error(&m_mysql);
	}
}}
