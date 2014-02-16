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
#include <filesystem.hpp>
#include <sys/stat.h>

namespace filesystem
{

	void path::make_universal(std::string& s)
	{
		for (char& c : s)
		{
			if (c == backslash::value)
				c = slash::value;
		}
	}

	size_t path::root_name_end() const
	{
		auto pos = m_path.find(colon::value);
		if (pos == std::string::npos)
			return 0;

		return pos + 1;
	}

	path& path::make_preferred()
	{
		for (char& c : m_path)
		{
			if (c == slash::value)
				c = backslash::value;
		}

		return *this;
	}

	path current_path()
	{
		char buffer[2048];
		GetCurrentDirectoryA(sizeof(buffer), buffer);
		return buffer;
	}

	path app_directory()
	{
		char buffer[2048];
		GetModuleFileNameA(nullptr, buffer, sizeof(buffer));
		return path(buffer).parent_path();
	}

	status::status(const path& p)
	{
		auto native = p.native();
		DWORD atts = GetFileAttributesA(native.c_str());
		if (atts == INVALID_FILE_ATTRIBUTES)
		{
			m_type = file_type::not_found;
			return;
		}

		if (atts && FILE_ATTRIBUTE_DIRECTORY)
			m_type = file_type::directory;
		else
			m_type = file_type::regular;

		struct stat st;
		if (stat(native.c_str(), &st))
			m_file_size = st.st_size;
	}
}
