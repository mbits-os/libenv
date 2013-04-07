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
#include <utils.hpp>
#include <time.h>

#ifdef _WIN32
#define ts_gmtime(tm, value) _gmtime64_s(&(tm), &(value))
#else
#define ts_gmtime(tm, value) gmtime_r(&(value), &(tm))
#endif

#ifdef _WIN32
#define ts_localtime(tm, value) _localtime64_s(&(tm), &(value))
#else
#define ts_localtime(tm, value) localtime_r(&(value), &(tm))
#endif

namespace tyme
{
#define adjust(x) (x)
	time_t __adjust(time_t local)
	{
		struct ::tm tm;
		ts_gmtime(tm, local);
		return ::mktime(&tm);
	}

	time_t now()
	{
		time_t t;
		return __adjust(::time(&t));
	}

	// makes t into GMT time
	time_t mktime(const tm_t& tm)
	{
		struct ::tm _tm = {
			tm.tm_sec,
			tm.tm_min,
			tm.tm_hour,
			tm.tm_mday,
			tm.tm_mon,
			tm.tm_year,
			tm.tm_wday,
			tm.tm_yday,
			tm.tm_isdst
		};
		return adjust(::mktime(&_tm));
	}

	// expands time to GMT tm_t
	tm_t gmtime(time_t value)
	{
		struct ::tm tm;
		ts_localtime(tm, value);
		tm_t _tm = {
			tm.tm_sec,
			tm.tm_min,
			tm.tm_hour,
			tm.tm_mday,
			tm.tm_mon,
			tm.tm_year,
			tm.tm_wday,
			tm.tm_yday,
			tm.tm_isdst
		};
		return _tm;
	}

	size_t strftime(char * buffer, size_t size, const char * format, const tm_t& tm)
	{
		struct ::tm _tm = {
			tm.tm_sec,
			tm.tm_min,
			tm.tm_hour,
			tm.tm_mday,
			tm.tm_mon,
			tm.tm_year,
			tm.tm_wday,
			tm.tm_yday,
			tm.tm_isdst
		};
		return ::strftime(buffer, size, format, &_tm);
	}

	const char* parseInt(const char* from, const char* to, int& component)
	{
		while (from < to && isdigit((unsigned char)*from))
		{
			component *= 10;
			component += *from++ - '0';
		}
		return from;
	}

	inline bool parse(const char*& from, const char* to, int& component, size_t digits)
	{
		const char* ptr = parseInt(from, to, component);
		if (ptr - from != digits)
			return false;
		from = ptr;
		return true;
	}

	inline bool parse(const char*& from, const char* to, int& component, size_t digits, char next)
	{
		bool ret = parse(from, to, component, digits) && from < to && *from == next;
		if (ret) ++from;
		return ret;
	}

	bool rfc3339(const char* from, const char* to, tm_t& tm)
	{
		memset(&tm, 0, sizeof(tm));

		if (!parse(from, to, tm.tm_year, 4, '-')) return false;
		if (!parse(from, to, tm.tm_mon, 2, '-')) return false;
		if (!parse(from, to, tm.tm_mday, 2, 'T')) return false;
		if (!parse(from, to, tm.tm_hour, 2, ':')) return false;
		if (!parse(from, to, tm.tm_min, 2, ':')) return false;
		if (!parse(from, to, tm.tm_sec, 2)) return false;

		int tz_sign = -1;
		int tz = 0;

		if (from >= to) return false;
		if (*from == '.')
		{
			int ignore;
			++from;
			from = parseInt(from, to, ignore);
			if (from >= to) return false;
		}

		if (*from != 'Z' && *from != '+' && *from != '-') return false;

		if (*from != 'Z')
		{
			if (*from == '-')
				tz_sign = 1;

			++from;
			if (!parse(from, to, tz, 2, ':')) return false;
			tz *= 60;
			if (!parse(from, to, tz, 2)) return false;
			tz *= tz_sign;
			tz *= 60;
		}

		tm.tm_mon --;
		tm.tm_year -= 1900;

		tm = gmtime(mktime(tm) + tz);

		return true;
	}

#define WS while (from < to && isspace((unsigned char)*from)) ++from;
#define NWS while (from < to && !isspace((unsigned char)*from)) ++from;

	template <size_t size>
	static inline int locate(const char* (&table)[size], const char*& from, const char* to, char next)
	{
		if (from + 4 >= to)
			return -1;
		if (next && from[3] != next)
			return -1;
		if (!next && !isspace((unsigned char)from[3]))
			return -1;

		int i = 0;
		for (; i < size; ++i)
		{
			if (!strncmp(from, table[i], 3))
			{
				from += 4;
				WS;
				return i;
			}
		}
		return -1;
	}

	bool rfc822(const char* from, const char* to, tm_t& tm)
	{
		static const char* day[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
		static const char* month[] =  { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
		static const struct {
			const char* name;
			int offset;
		} zones[] = {
			{ "UT",  0}, { "GMT", 0},

			{ "EST", 5*60 }, { "EDT", 4*60 },
			{ "CST", 6*60 }, { "CDT", 5*60 },
			{ "MST", 7*60 }, { "MDT", 6*60 },
			{ "PST", 8*60 }, { "PDT", 7*60 },

			{ "Z", 0 },
			{ "A",  1*60 }, { "B",  2*60 }, { "C",  3*60 }, { "D",  4*60 }, { "E",  5*60 }, { "F",  6*60 }, { "G",  7*60 }, { "H",  8*60 }, { "I",  9*60 }, { "K",  10*60 }, { "L",  11*60 }, { "M",  12*60 },
			{ "N", -1*60 }, { "O", -2*60 }, { "P", -3*60 }, { "Q", -4*60 }, { "R", -5*60 }, { "S", -6*60 }, { "T", -7*60 }, { "U", -8*60 }, { "V", -9*60 }, { "W", -10*60 }, { "X", -11*60 }, { "Y", -12*60 }
		};

		memset(&tm, 0, sizeof(tm));

		//is there a place for the weekday?
		if (from + 4 >= to) return false;
		if (from[3] == ',')
		{
			tm.tm_wday = locate(day, from, to, ',');
			if (tm.tm_wday == -1)
				return false;
		}

		if (!parse(from, to, tm.tm_mday, 2)) return false;
		WS;

		tm.tm_mon = locate(month, from, to, 0);
		if (tm.tm_mon == -1)
			return false;

		const char* ptr = parseInt(from, to, tm.tm_year);
		if (ptr - from == 2) ; //OK
		else if (ptr - from == 4) tm.tm_year -= 1900;
		else return false;
		from = ptr;
		WS;

		if (!parse(from, to, tm.tm_hour, 2, ':')) return false;
		if (!parse(from, to, tm.tm_min, 2)) return false;
		if (from < to && *from == ':')
		{
			++from;
			if (!parse(from, to, tm.tm_sec, 2)) return false;
		}

		WS;

		if (from >= to)
			return false;

		int tz = 0;
		if (*from == '-' || *from == '+')
		{
			++from;
			if (!parse(from, to, tz, 4)) return false;
			tz = tz % 100 + (tz/100 * 60); // unpack HHMM into HH*60 + MM
			if (*from == '+')
				tz = -tz;
		}
		else
		{
			const char* ptr = from;
			NWS;
			size_t len = from - ptr;

			int i = 0;
			for (; i < sizeof(zones)/sizeof(zones[0]); ++i)
			{
				if (!strncmp(from, zones[i].name, len) && strlen(zones[i].name) == len)
				{
					tz = zones[i].offset;
					break;
				}
			}
			if (i == sizeof(zones)/sizeof(zones[0]))
				return false;
		}

		tm = gmtime(mktime(tm) + tz);
		return true;
	}

	bool scan(const char* from, const char* to, tm_t& tm)
	{
		if (rfc3339(from, to, tm))
			return true;

		if (rfc822(from, to, tm))
			return true;

		memset(&tm, 0, sizeof(tm));
		return false;
	}
}
