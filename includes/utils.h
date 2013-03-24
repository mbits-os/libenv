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

#ifndef __UTILS_H__
#define __UTILS_H__

#include <string>
#include <list>

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
#ifdef _WIN32
	typedef long long time_t;
#else
#error tyme::time_t is not defined
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
};

#endif //__UTILS_H__
