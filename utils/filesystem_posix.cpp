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
#include <sys/types.h>
#include <unistd.h>

namespace filesystem
{
	path current_path()
	{
		char buffer[2048];
		return getcwd(buffer, sizeof(buffer));
	}

	status::status(const path& p)
	{
		struct stat st;
		if (stat(p.native().c_str(), &st))
		{
			m_file_size = st.st_size;
			if (S_ISBLK(st.st_mode)) m_type = file_type::block;
			else if (S_ISCHR(st.st_mode)) m_type = file_type::character;
			else if (S_ISDIR(st.st_mode)) m_type = file_type::directory;
			else if (S_ISFIFO(st.st_mode)) m_type = file_type::fifo;
			else if (S_ISREG(st.st_mode)) m_type = file_type::regular;
			else if (S_ISLNK(st.st_mode)) m_type = file_type::symlink;
			else if (S_ISSOCK(st.st_mode)) m_type = file_type::socket;
			else m_type = file_type::unknown;
		}
		else
			m_type = file_type::not_found;
	}
}
