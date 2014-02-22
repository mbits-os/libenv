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
#include <vector>
#include <cstdlib>

namespace filesystem
{
#ifdef WIN32
	static std::string::const_iterator skip_drive(std::string::const_iterator from, std::string::const_iterator to)
	{
		auto save = from;
		while (from != to && *from != colon::value) ++from;
		if (from != to)
		{
			++from;
			if (from != to && (*from == slash::value) || (*from == backslash::value))
				return from;
		}

		return save;
	}
#endif

	path& path::append(std::string::const_iterator from, std::string::const_iterator to)
	{
#ifdef WIN32
		from = skip_drive(from, to);
#endif
		bool has_left_slash = !empty() && (*--m_path.end() == slash::value);
		bool has_right_slash = from != to && ((*from == directory_separator::value) || (*from == preferred_separator::value));

		if (has_left_slash && has_right_slash)
			++from;
		else if (!has_left_slash && !has_right_slash)
			m_path.push_back(slash::value);

		for (; from != to; ++from)
		{
			if (*from == preferred_separator::value)
				m_path.push_back(directory_separator::value);
			else
				m_path.push_back(*from);
		}

		return *this;
	}

	path& path::operator+=(const std::string& val)
	{
#ifdef WIN32
		std::string tmp{ val };
		make_universal(tmp);
		m_path += tmp;
#else
		m_path += val;
#endif
		return *this;
	}

	path& path::operator+=(const char* val)
	{
#ifdef WIN32
		std::string tmp{ val };
		make_universal(tmp);
		m_path += tmp;
#else
		m_path += val;
#endif
		return *this;
	}

	path& path::operator+=(char val)
	{
		if (val == preferred_separator::value)
			m_path.push_back(directory_separator::value);
		else
			m_path.push_back(val);
		return *this;
	}

	path& path::remove_filename()
	{
		if (!empty() && begin() != --end())
		{
			// leaf to remove, back up over it
			size_t path_start = root_directory_end();
			size_t end = m_path.size();

			for (; path_start < end; --end)
			{
				if (m_path[end - 1] == slash::value)
					break;
			}
			for (; path_start < end; --end)
			{
				if (m_path[end - 1] != slash::value)
					break;
			}
			m_path.erase(end);
		}
		return *this;
	}

	path& path::replace_extension(const path& replacement)
	{
		if (replacement.empty() || replacement.m_path[0] == dot::value)
			return parent_path() /= (stem() + replacement);

		return parent_path() /= (stem() + "." + replacement);
	}

	path path::root_name() const
	{
		return m_path.substr(0, root_name_end());
	}

	path path::root_directory() const
	{
		auto pos = root_name_end();
		if (pos < m_path.size() && m_path[pos] == slash::value)
			return std::string(1, slash::value);

		return std::string();
	}

	path path::root_path() const
	{
		return m_path.substr(0, root_directory_end());
	}

	path path::relative_path() const
	{
		size_t pos = root_directory_end();

		while (pos < m_path.size() && m_path[pos] == slash::value)
			++pos;	// skip extra '/' after root

		return path(m_path.c_str() + pos, m_path.c_str() + m_path.size());
	}

	path path::parent_path() const
	{
		if (empty())
			return path();

		auto _end = --end();
		size_t off = _end.__offset();
		if (off && m_path[off - 1] == slash::value)
			--off;

		return m_path.substr(0, off);
	}

	path path::stem() const
	{
		auto fname = filename().string();
		auto pos = fname.find(dot::value);
		return fname.substr(0, pos);
	}

	path path::extension() const
	{
		auto fname = filename().string();
		auto pos = fname.find(dot::value);
		return fname.substr(pos);
	}


	path absolute(const path& p, const path& base)
	{
		if (p.is_absolute())
			return p;

		path a_base = base.is_absolute() ? base : absolute(base);

		if (p.has_root_name())
			return p.root_name() / a_base.root_directory() / a_base.relative_path() / p.relative_path();

		if (p.has_root_directory())
			return a_base.root_name() / p;

		return a_base / p;
	}

	path canonical(const path& p, const path& base)
	{
		path tmp = absolute(p, base);
		auto out = tmp.root_path();
		tmp = tmp.relative_path();

		std::vector<std::string> parts;
		bool last_dot = false;
		for (auto&& part : tmp)
		{
			last_dot = false;
			if (part == ".")
			{
				last_dot = true;
				continue;
			}

			if (part == "..")
			{
				if (!parts.empty())
					parts.pop_back();
				continue;
			}

			parts.push_back(part);
		}

		for (auto&& part : parts)
			out /= part;

		if (last_dot)
			out /= "";

		return out;
	}

	void remove(const path& p)
	{
		std::remove(p.native().c_str());
	}
}
