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
#include <forms/forms.hpp>
#include <fast_cgi/request.hpp>

namespace FastCGI {
	FormBase::FormBase(const std::string& title, const std::string& method, const std::string& action, const std::string& mime)
		: m_title(title)
		, m_method(method)
		, m_action(action)
		, m_mime(mime)
	{
	}

	void FormBase::formStart(SessionPtr session, Request& request, PageTranslation& tr)
	{
		request << "\r\n<form method='" << m_method << "'";
		if (!m_action.empty()) request << " action='" << m_action << "'";
		if (!m_mime.empty()) request << " enctype='" << m_mime << "'";
		request << ">\r\n  <input type='hidden' name='posted' value='1' />\r\n";

		for (auto&& hidden : m_hidden) {
			param_t var = request.getVariable(hidden.c_str());
			if (var != nullptr)
				request << "  <input type='hidden' name='" << hidden << "' value='" << url::htmlQuotes(var) << "' />\r\n";
		};

		request << "\r\n";
	}

	void FormBase::formEnd(SessionPtr session, Request& request, PageTranslation& tr)
	{
		request << "</form>\r\n";
	}

} // FastCGI
