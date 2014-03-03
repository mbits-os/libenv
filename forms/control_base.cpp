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
#include <forms/control_base.hpp>
#include <fast_cgi/request.hpp>

namespace FastCGI {
	void ControlBase::getAttributes(Request& request, BasicRenderer& renderer)
	{
		for (auto&& pair : m_attrs)
			request << " " << pair.first << "='" << url::htmlQuotes(pair.second) << "'";
	}

	void ControlBase::getElement(Request& request, BasicRenderer& renderer, const std::string& name)
	{
		request << "<" << name;
		getAttributes(request, renderer);
		request << " />";
	}

	void ControlBase::getElement(Request& request, BasicRenderer& renderer, const std::string& name, const std::string& content)
	{
		request << "<" << name;
		getAttributes(request, renderer);
		if (!content.empty())
			request << " >" << content << "</" << name << ">";
		else
			request << " />";
	}

	void ControlBase::getElement(Request& request, BasicRenderer& renderer, const std::string& name, const ChildrenCallback& op)
	{
		request << "<" << name;
		getAttributes(request, renderer);
		request << " >\r\n";
		op(request, renderer);
		request << "</" << name << ">";
	}

	void ControlBase::bindData(Request& request, const Strings& data)
	{
		if (m_name.empty()) return;

		m_userValue = false;
		m_value.erase();

		param_t value = request.getVariable(m_name.c_str());
		if (value != nullptr)
		{
			m_userValue = true;
			m_value = value;
			return;
		}

		auto _it = data.find(m_name);
		if (_it != data.end())
		{
			m_value = _it->second;
			return;
		}
	}
} // FastCGI
