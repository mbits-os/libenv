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

#ifndef __UTILS_H__
#define __UTILS_H__

#include <string>
#include <list>
#include <algorithm>
#include <string.h>
#include <cctype>

template<class T, size_t N> size_t array_size(T (&)[N]){ return N; }

namespace url
{
	bool isToken(const char* in, size_t in_len);
	inline static bool isToken(const std::string& s)
	{
		return isToken(s.c_str(), s.size());
	}

	std::string encode(const char* in, size_t in_len);
	inline static std::string encode(const std::string& s)
	{
		return encode(s.c_str(), s.size());
	}

	std::string decode(const char* in, size_t in_len);
	inline static std::string decode(const std::string& s)
	{
		return decode(s.c_str(), s.size());
	}

	std::string htmlQuotes(const char* in, size_t in_len);
	inline static std::string htmlQuotes(const std::string& s)
	{
		return htmlQuotes(s.c_str(), s.size());
	}

	std::string quot_escape(const char* in, size_t in_len);
	inline static std::string quot_escape(const std::string& s)
	{
		return quot_escape(s.c_str(), s.size());
	}

	std::string quot_parse(const char* in, size_t in_len, const char** closing_quot_p);
	inline static std::string quot_parse(const std::string& s)
	{
		return quot_parse(s.c_str(), s.size(), nullptr);
	}

	// "text/html, application/xml;q=0.9, application/xhtml+xml, image/png, image/webp, image/jpeg, image/gif, image/x-xbitmap, */*;q=0.1"
	// "text/html", "application/xhtml+xml", "image/png", "image/webp", "image/jpeg", "image/gif", "image/x-xbitmap", "application/xml", "*/*"
	std::list<std::string> priorityList(const char* header);
}

namespace tyme
{
#ifdef WIN32
	typedef long long time_t;
#else
	typedef long time_t;
#endif

	struct tm_t
	{
		int tm_sec;     /* seconds after the minute - [0,59] */
		int tm_min;     /* minutes after the hour - [0,59] */
		int tm_hour;    /* hours since midnight - [0,23] */
		int tm_mday;    /* day of the month - [1,31] */
		int tm_mon;     /* months since January - [0,11] */
		int tm_year;    /* years since 1900 */
		int tm_wday;    /* days since Sunday - [0,6] */
		int tm_yday;    /* days since January 1 - [0,365] */
		int tm_isdst;   /* daylight savings time flag */
	};

	// returns "now" in GMT
	time_t now();

	// makes t into GMT time
	time_t mktime(const tm_t& t);

	// expands time to GMT tm_t
	tm_t gmtime(time_t);

	size_t strftime(char * buffer, size_t size, const char * format, const tm_t& tm);
	template<size_t size>
	static inline size_t strftime(char (&buffer)[size], const char * format, const tm_t& tm)
	{
		return strftime(buffer, size, format, tm);
	}

	bool scan(const char* from, const char* to, tm_t& tm);
	inline bool scan(const char* buffer, tm_t& tm)
	{
		if (!buffer)
			return false;
		const char* end = buffer + strlen(buffer);
		return scan(buffer, end, tm);
	}
};

namespace std
{
	static inline void tolower(std::string& inout)
	{
		std::transform(inout.begin(), inout.end(), inout.begin(), ::tolower);
	}

	static inline void toupper(std::string& inout)
	{
		std::transform(inout.begin(), inout.end(), inout.begin(), ::toupper);
	}
	static inline std::string tolower(const std::string& s)
	{
		std::string out = s;
		tolower(out);
		return out;
	}

	static inline std::string toupper(const std::string& s)
	{
		std::string out = s;
		toupper(out);
		return out;
	}

	static inline void trim(std::string& inout)
	{
		auto c = inout.begin();
		auto e = inout.end();
		while (c != e && std::isspace((unsigned char)*c))
			++c;

		if (c == e)
		{
			inout.clear();
			return;
		}

		while (std::isspace((unsigned char) *(e - 1)))
			--e;

		if (c != inout.begin() || e != inout.end())
		{
			std::string tmp{ c, e };
			inout = std::move(tmp);
		}
	}

	static inline std::string trim(const std::string& s)
	{
		std::string out = s;
		trim(out);
		return out;
	}
}

#define SHAREABLE(type) \
	class type; \
	typedef std::shared_ptr<type> type ## Ptr; \
	typedef std::weak_ptr<type> type ## WeakPtr;

#define SHAREABLE_STRUCT(type) \
	struct type; \
	typedef std::shared_ptr<type> type ## Ptr; \
	typedef std::weak_ptr<type> type ## WeakPtr;

#endif //__UTILS_H__
