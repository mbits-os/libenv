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

#ifndef __URI_HPP__
#define __URI_HPP__

#include <stdint.h>
#include <filesystem.hpp>

class Uri
{
	std::string m_uri;

	static const std::string::size_type ncalc = (std::string::size_type)(-2);
	static const std::string::size_type npos = (std::string::size_type)(-1);

	mutable std::string::size_type m_schema = ncalc;
	mutable std::string::size_type m_path = ncalc;
	mutable std::string::size_type m_query = ncalc;
	mutable std::string::size_type m_part = ncalc;

	void ensure_schema() const
	{
		if (m_schema != ncalc)
			return;

		m_schema = npos;
		auto length = m_uri.length();

		auto b = m_uri.data();
		auto c = b;
		auto e = b + length;

		if (c == e || !isalpha((unsigned char)*c))
			return;

		++c;
		while (c != e && (isalnum((unsigned char)*c) || *c == '+' || *c == '-' || *c == '.'))
			++c;

		if (c == e || *c != ':')
			return;

		m_schema = c - b;
	}

	void ensure_path() const
	{
		if (m_path != ncalc)
			return;

		ensure_schema();

		if (m_schema == npos)
		{
			m_path = 0;
			return;
		}

		auto length = m_uri.length();

		auto c = m_uri.data();

		m_path = m_schema + 1;

		if (m_schema + 2 >= length || c[m_schema + 1] != '/' || c[m_schema + 2] != '/')
			return;

		m_path = m_schema + 3;
		while (m_path < length)
		{
			switch (c[m_path])
			{
			case '/': case '?': case '#':
				return;
			}
			++m_path;
		}
	}

	void ensure_query() const
	{
		if (m_query != ncalc)
			return;

		ensure_path();

		auto length = m_uri.length();

		auto c = m_uri.data();

		m_query = m_path;
		while (m_query < length)
		{
			switch (c[m_query])
			{
			case '?': case '#':
				return;
			}
			++m_query;
		}

	}

	void ensure_fragment() const
	{
		if (m_part != ncalc)
			return;

		ensure_query();

		auto length = m_uri.length();

		auto c = m_uri.data();

		m_part = m_query;
		while (m_part < length)
		{
			if (c[m_part] == '#')
				return;
			++m_part;
		}

	}

	void invalidate_fragment()
	{
		m_part = ncalc;
	}

	void invalidate_query()
	{
		invalidate_fragment();
		m_query = ncalc;
	}

	void invalidate_path()
	{
		invalidate_query();
		m_path = ncalc;
	}

	void invalidate_schema()
	{
		invalidate_path();
		m_schema = ncalc;
	}
public:
	Uri() = default;
	Uri(const Uri&) = default;
	Uri(Uri&&) = default;
	Uri& operator=(const Uri&) = default;
	Uri& operator=(Uri&&) = default;

	Uri(const std::string& uri) : m_uri(uri) {}

	bool hierarchical() const
	{
		if (relative())
			return true;

		ensure_path();

		if (m_path - m_schema <= 2)
			return false;

		auto c = m_uri.data();
		return c[m_schema + 1] == '/' && c[m_schema + 2] == '/';
	}
	bool opaque() const { return !hierarchical(); }
	bool relative() const
	{
		ensure_schema();
		return m_schema == npos;
	}
	bool absolute() const { return !relative(); }
	std::string scheme() const
	{
		if (relative())
			return std::string();

		return m_uri.substr(m_schema);
	}
	std::string authority() const
	{
		if (relative() || opaque())
			return std::string();

		auto start = m_schema + 3;
		return m_uri.substr(start, m_path - start);
	}
	std::string path() const
	{
		ensure_query();
		return m_uri.substr(m_path, m_query - m_path);
	}
	std::string query() const
	{
		ensure_fragment();
		return m_uri.substr(m_query, m_part - m_query);
	}
	std::string fragment() const
	{
		ensure_fragment();
		return m_uri.substr(m_part);
	}

	void path(const std::string& value)
	{
		ensure_query();
		m_uri.replace(m_path, m_query - m_path, value);
		invalidate_query();
	}

	void query(const std::string& value)
	{
		ensure_fragment();
		m_uri.replace(m_query, m_part - m_query, value);
		invalidate_fragment();
	}

	void fragment(const std::string& value)
	{
		ensure_fragment();
		m_uri.replace(m_part, m_uri.length() - m_part, value);
	}

	std::string string() const { return m_uri; }

	static Uri make_base(const std::string& document)
	{
		return make_base(Uri{ document });
	}
	static Uri make_base(const Uri& document)
	{
		auto tmp = document;
		if (tmp.relative())
			tmp = "http://" + tmp.string();

		tmp.fragment(std::string());
		tmp.query(std::string());
		tmp.path(filesystem::path{ tmp.path() }.remove_filename().string());
		tmp.ensure_query();
		return tmp;
	}

	static Uri canonical(const Uri& uri, const Uri& base)
	{
		if (uri.absolute())
			return uri;

		auto temp = base;
		temp.fragment(uri.fragment());
		temp.query(uri.query());

		auto path = filesystem::canonical(uri.path(), base.path());
		if (path.has_root_name())
			path = path.string().substr(path.root_name().string().length());
		temp.path(path.string());

		return temp;
	}
};

#endif //__URI_HPP__
