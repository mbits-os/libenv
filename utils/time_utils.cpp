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
}
