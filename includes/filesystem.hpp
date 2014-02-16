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

#ifndef __FILESYSTEM_HPP__
#define __FILESYSTEM_HPP__

#include <string>

namespace filesystem
{
	struct slash { static const char value = '/'; };
	struct dot { static const char value = '.'; };
	struct backslash { static const char value = '\\'; };
	struct colon { static const char value = ':'; };

	class path;

	class path_iterator
		: public std::iterator< std::bidirectional_iterator_tag, std::string, std::ptrdiff_t, std::string*, std::string&>
	{
	public:
		typedef const std::string *pointer;
		typedef const std::string& reference;

		path_iterator(): m_ptr(0), m_offset(0) {}

		path_iterator(const path& val, size_t offset) : m_ptr(&val), m_offset(offset) {	get_value(); }

		path_iterator(const path_iterator&) = default;
		path_iterator& operator=(const path_iterator&) = default;
		path_iterator(path_iterator&& oth)
			: m_ptr(oth.m_ptr)
			, m_value(std::move(oth.m_value))
			, m_offset(oth.m_offset)
			, m_needs_value(oth.m_needs_value)
		{
		}
		path_iterator& operator=(const path_iterator&& oth)
		{
			m_ptr = oth.m_ptr;
			m_value = std::move(oth.m_value);
			m_offset = oth.m_offset;
			m_needs_value = oth.m_needs_value;

			return *this;
		}

		reference operator*() const
		{
			if (m_needs_value)
				((path_iterator*)this)->get_value();
			return m_value;
		}

		pointer operator->() const
		{
			return (std::pointer_traits<pointer>::pointer_to(**this));
		}

		inline path_iterator& operator++();

		path_iterator operator++(int)
		{	// postincrement
			path_iterator _Tmp = *this;
			++*this;
			return (_Tmp);
		}

		inline path_iterator& operator--();

		path_iterator operator--(int)
		{	// postdecrement
			path_iterator _Tmp = *this;
			--*this;
			return (_Tmp);
		}

		bool operator==(const path_iterator& _Right) const
		{	// test for iterator equality
			return (m_ptr == _Right.m_ptr && m_offset == _Right.m_offset);
		}

		bool operator!=(const path_iterator& _Right) const
		{	// test for iterator inequality
			return (!(*this == _Right));
		}

		size_t __offset() const { return m_offset; }
	private:
		inline void get_value();

		const path *m_ptr;
		std::string m_value;
		size_t m_offset;
		bool m_needs_value;
	};

	class path
	{
		friend class path_iterator;
		std::string m_path;

#ifdef WIN32
		static void make_universal(std::string& s);
		size_t root_name_end() const;
#else
		static void make_universal(std::string&) {}
		size_t root_name_end() const { return 0; }
#endif
		size_t root_directory_end() const
		{
			auto name = root_name_end();
			if (name < m_path.size() && m_path[name] == slash::value)
				++name;
			return name;
		}

	public:
		using directory_separator = slash;
#ifdef WIN32
		using preferred_separator = backslash;
#else
		using preferred_separator = slash;
#endif

		path() = default;
		path(const path& p) = default;
		path(path&& p) : m_path(std::move(p.m_path)) {}
		path(const std::string& val) : m_path(val) { make_universal(m_path); }
		path(const char* val) : m_path(val) { make_universal(m_path); }
		template <typename It>
		path(It begin, It end) : m_path(begin, end) { make_universal(m_path); }
		~path() = default;

		path& operator=(const path& p) = default;
		path& operator=(path&& p)
		{
			m_path = std::move(p.m_path);
			return *this;
		}
		path& operator=(const std::string& val) { return *this = path(val); }
		path& operator=(const char* val) { return *this = path(val); }

		path& operator/=(const path& p) { return append(p.m_path); }
		path& operator/=(const std::string& val) { return append(val); }
		path& operator/=(const char* val) { return append(val); }
		path& append(const std::string& s) { return append(s.begin(), s.end()); }
		path& append(std::string::const_iterator from, std::string::const_iterator to);


		path& operator+=(const path& val) { return *this += val.m_path; }
		path& operator+=(const std::string& val);
		path& operator+=(const char* val);
		path& operator+=(char val);


		void clear() { m_path.clear(); }
#ifdef WIN32
		path& make_preferred();
#else
		path& make_preferred() { return *this; }
#endif
		path& remove_filename();
		path& replace_filename(const path& replacement) { return remove_filename() /= replacement; }
		path& replace_extension(const path& replacement = path());
		void swap(path& rhs) noexcept{ std::swap(m_path, rhs.m_path); }

		const std::string& string() const { return m_path; }
#ifdef WIN32
		std::string native() const { return path(*this).make_preferred().string(); }
#else
		std::string native() const { return m_path; }
#endif

		int compare(const path& p) const { return m_path.compare(p.string()); }
		int compare(const std::string& s) const { return compare(path(s)); }
		int compare(const char* s) const { return compare(path(s)); }

		path root_name() const;
		path root_directory() const;
		path root_path() const;
		path relative_path() const;
		path parent_path() const;
		path filename() const { return empty() ? std::string() : *--end(); }
		path stem() const;
		path extension() const;

		bool empty() const { return m_path.empty(); }
		bool has_root_name() const { return !root_name().empty(); }
		bool has_root_directory() const { return !root_directory().empty(); }
		bool has_root_path() const { return !root_path().empty(); }
		bool has_relative_path() const { return !relative_path().empty(); }
		bool has_parent_path() const { return !parent_path().empty(); }
		bool has_filename() const { return !filename().empty(); }
		bool has_stem() const { return !stem().empty(); }
		bool has_extension() const { return !extension().empty(); }
#ifdef WIN32
		bool is_absolute() const { return has_root_name() && has_root_directory(); }
#else
		bool is_absolute() const { return has_root_directory(); }
#endif
		bool is_relative() const { return !is_absolute(); }

		using iterator = path_iterator;
		using const_iterator = iterator;

		iterator begin() const
		{
			return (iterator(*this, (size_t)0));
		}

		iterator end() const
		{
			return (iterator(*this, m_path.size()));
		}
	};

	inline path_iterator& path_iterator::operator++()
	{
		size_t root_name_size = m_ptr->root_name_end();
		size_t path_size = m_ptr->m_path.size();
		const char* data = m_ptr->m_path.data();

		if (m_offset < root_name_size)
			m_offset = root_name_size; // move past drive

		else if (m_offset == root_name_size && root_name_size < path_size && data[root_name_size] == slash::value)
		{
			// move past root "/"
			for (++m_offset; m_offset < path_size; ++m_offset)
			{
				if (data[m_offset] != slash::value)
					break;
			}
		}
		else
		{
			// move past slashes followed by a name

			for (; m_offset < path_size; ++m_offset)
			{
				if (data[m_offset] != slash::value)
					break;
			}

			for (; m_offset < path_size; ++m_offset)
			{
				if (data[m_offset] == slash::value)
					break;
			}
		}

		m_needs_value = true;
		return (*this);
	}

	inline path_iterator& path_iterator::operator--()
	{
		size_t offset_save = m_offset;
		size_t offset_prev = 0;

		m_offset = 0;
		do
		{
			offset_prev = m_offset;
			++*this;
		} while (m_offset < offset_save);
		m_offset = offset_prev;

		get_value();
		return *this;
	}

	inline void path_iterator::get_value()
	{
		m_needs_value = false;

		size_t root_name_size = m_ptr->root_name_end();
		size_t path_size = m_ptr->m_path.size();

		m_value.clear();

		// nothing more to enumerate and/or end()
		if (path_size <= m_offset)
			return;

		if (m_offset < root_name_size)
		{
			m_value = m_ptr->m_path.substr(0, root_name_size); // get drive
			return;
		}

		const char* data = m_ptr->m_path.data();

		if (m_offset == root_name_size && root_name_size < path_size && data[root_name_size] == slash::value)
		{
			m_value = slash::value;	// get "/"
			return;
		}

		size_t start = m_offset;
		size_t next_slash = 0;

		for (; start < path_size; ++start)
		{
			if (data[start] != slash::value)
				break;
		}
		for (; start + next_slash < path_size; ++next_slash)
		{
			if (data[start + next_slash] == slash::value)
				break;
		}

		if (next_slash)
		{
			m_value = m_ptr->m_path.substr(start, next_slash);
			return;
		}

		if (start > m_offset) // we moved, but are right after the last slash
			m_value = dot::value;
	}

	inline void swap(path& lhs, path& rhs)
	{
		lhs.swap(rhs);
	}

	inline bool operator< (const path& lhs, const path& rhs)
	{
		return lhs.compare(rhs) < 0;
	}

	inline bool operator<=(const path& lhs, const path& rhs)
	{
		return !(rhs < lhs);
	}

	inline bool operator>(const path& lhs, const path& rhs)
	{
		return rhs < lhs;
	}

	inline bool operator>=(const path& lhs, const path& rhs)
	{
		return !(lhs < rhs);
	}

	inline bool operator==(const path& lhs, const path& rhs)
	{
		return !(lhs < rhs) && !(rhs < lhs);
	}

	inline bool operator!=(const path& lhs, const path& rhs)
	{
		return !(lhs == rhs);
	}

	template <typename T>
	inline path operator/ (const path& lhs, const T& rhs)
	{
		return path(lhs) /= rhs;
	}

	template <typename T>
	inline path operator+ (const path& lhs, const T& rhs)
	{
		return path(lhs) += rhs;
	}

	template <class charT, class traits>
	inline std::basic_ostream<charT, traits>& operator<<(std::basic_ostream<charT, traits>& os, const path& p)
	{
		return os << p.string();
	}

	path current_path();
	inline void current_path(path& p) { p = current_path(); }

	path absolute(const path& p, const path& base = current_path());
	path canonical(const path& p, const path& base = current_path());

	enum class file_type
	{
		not_found = -1,
		none,
		regular,
		directory,
		symlink,
		block,
		character,
		fifo,
		socket,
		unknown
	};

	class status
	{
		file_type m_type = file_type::none;
		uintmax_t m_file_size = 0;
	public:
		status() {}
		status(const status&) = default;
		explicit status(const path&);
		status& operator=(const status&) = default;

		file_type type() const { return m_type; }
		bool exists() const
		{
			switch (type())
			{
			case file_type::none:
			case file_type::not_found:
				return false;
			default:
				break;
			}
			return true;
		}
		uintmax_t file_size() const { return m_file_size; }
		bool is_directory() const { return m_type == file_type::directory; }
		bool is_regular_file() const { return m_type == file_type::regular; }
	};

	inline bool exists(const path& p) { return status(p).exists(); }
	inline uintmax_t file_size(const path& p) { return status(p).file_size(); }
	inline bool is_directory(const path& p) { return status(p).is_directory(); }
	inline bool is_regular_file(const path& p) { return status(p).is_regular_file(); }
};

namespace std
{
	inline void swap(filesystem::path& lhs, filesystem::path& rhs)
	{
		lhs.swap(rhs);
	}
}

#endif // __FILESYSTEM_HPP__
