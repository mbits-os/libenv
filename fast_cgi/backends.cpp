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
#include <fast_cgi/request.hpp>
#include <fast_cgi/backends.hpp>

#include <string.h>

namespace FastCGI
{
	namespace impl
	{
		LibFCGIRequest::LibFCGIRequest(FCGX_Request& request)
			: m_streambufCin(request.in)
			, m_streambufCout(request.out)
			, m_streambufCerr(request.err)
			, m_cin(&m_streambufCin)
			, m_cout(&m_streambufCout)
			, m_cerr(&m_streambufCerr)
		{
		}

		static inline char* dup(const char* src)
		{
			size_t len = strlen(src) + 1;
			char* dst = (char*)malloc(len);
			if (!dst)
				return dst;
			memcpy(dst, src, len);
			return dst;
		}

		STLThread::STLThread(const char* uri)
		{
			REQUEST_URI = (char*)malloc(strlen(uri) + sizeof("REQUEST_URI="));
			strcat(strcpy(REQUEST_URI, "REQUEST_URI="), uri);
			const char* query = strchr(uri, '?');
			if (query)
			{
				++query;
				QUERY_STRING = (char*)malloc(strlen(query) + sizeof("QUERY_STRING="));
				strcat(strcpy(QUERY_STRING, "QUERY_STRING="), query);
			}
			else QUERY_STRING = dup("QUERY_STRING=");

			environment[0] = "FCGI_ROLE=RESPONDER";
			environment[1] = QUERY_STRING;
			environment[2] = "REQUEST_METHOD=GET";
			environment[3] = REQUEST_URI;
			environment[4] = "SERVER_PORT=80";
			environment[5] = "SERVER_NAME=www.reedr.net";
			environment[6] = "HTTP_COOKIE=reader.login=...";
			environment[7] = NULL;
		}

		STLThread::~STLThread()
		{
			free(REQUEST_URI);
			free(QUERY_STRING);
		}
	};
}
