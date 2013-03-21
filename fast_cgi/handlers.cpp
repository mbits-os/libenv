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

#include "pch.h"
#include "handlers.h"

namespace app
{
	HandlerPtr Handlers::_handler(Request& request)
	{
		fcgi::param_t REQUEST_URI = request.getParam("REQUEST_URI");
		if (REQUEST_URI == NULL) return HandlerPtr();
		fcgi::param_t query = strchr(REQUEST_URI, '?');

		HandlerMap::iterator _it = m_handlers.find(query ? std::string(REQUEST_URI, query) : REQUEST_URI);
		if (_it == m_handlers.end()) return HandlerPtr();
#if DEBUG_CGI
		return _it->second.ptr;
#else
		return _it->second;
#endif
	}
}