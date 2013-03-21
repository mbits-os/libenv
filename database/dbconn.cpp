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

#include "pch.h"
#include <dbconn.h>
#include <dbconn_driver.h>
#include <utils.h>

namespace db
{
	bool Driver::readProps(const std::string& path, Driver::Props& props)
	{
		std::ifstream ini(path);
		if (!ini.is_open())
			return false;

		std::string line;
		while (!ini.eof())
		{
			ini >> line;
			std::string::size_type enter = line.find('=');
			if (enter != std::string::npos)
				props[line.substr(0, enter)] = line.substr(enter + 1);
		}
		return true;
	}

	ConnectionPtr Connection::open(const char* path)
	{
		Driver::Props props;
		if (!Driver::readProps(path, props))
		{
			std::cerr << "Cannot open " << path << std::endl;
			return nullptr;
		}

		std::string driver_id;
		if (!Driver::getProp(props, "driver", driver_id))
		{
			std::cerr << "Connection configuration is missing the `driver' key\n";
			return nullptr;
		}

		DriverPtr driver = Drivers::driver(driver_id);
		if (driver.get() == nullptr)
		{
			std::cerr << "Cannot find `" << driver_id << "' database driver\n";
			return nullptr;
		}

		return driver->open(path, props);
	}

	//there is a problem with VC and global objects in LIBs...
	namespace mysql
	{
		bool startup_driver();
		void shutdown_driver();
	}

	static struct {
		bool (*startup)();
		void (*shutdown)();
	} info [] = {
		{ mysql::startup_driver, mysql::shutdown_driver }
	};
	static size_t succeeded = array_size(info);

	environment::environment(): failed(false)
	{
		for (size_t i = 0; i < array_size(info); ++i)
		{
			if (!info[i].startup())
			{
				succeeded = i;
				break;
			}
		}
		failed = succeeded != array_size(info);
	}

	environment::~environment()
	{
		for (size_t i = succeeded; i > 0; --i)
			info[i-1].shutdown();
	}
}