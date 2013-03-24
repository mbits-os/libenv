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
#include "forms.h"
#include <utils.h>

namespace FastCGI { namespace app {

	CGIControl::CGIControl(const std::string& name, const std::string& label, const std::string& hint)
		: m_name(name)
		, m_label(label)
		, m_hint(hint)
		, m_userValue(false)
	{
		if (!name.empty())
		{
			setAttr("name", name);
			setAttr("id", name);
		}
	}

	void CGIControl::getLabelString(Request& request)
	{
		if (!m_label.empty())
			request << "<label for='" << m_name << "'>" << m_label << "</label>";
	}

	void CGIControl::getHintString(Request& request)
	{
		if (!m_hint.empty())
			request << "<tr><td></td><td class='hint'>" << m_hint << "</td></tr>\r\n			";
	}

	void CGIControl::getAttributes(Request& request)
	{
		std::for_each(m_attrs.begin(), m_attrs.end(), [&request](const Attributes::value_type& pair) {
			request << " " << pair.first << "='" << url::htmlQuotes(pair.second) << "'";
		});
	}

	void CGIControl::getElement(Request& request, const std::string& name, const std::string& content)
	{
		request << "<" << name;
		getAttributes(request);
		if (!content.empty())
			request << "/>" << content << "</" << name << ">";
		else
			request << "/>";
	}

	void CGIControl::render(Request& request)
	{
		request << "<tr>";
		getControlString(request);
		request << "</tr>\r\n";
		getHintString(request);
	}

	void CGIControl::bindData(Request& request, const Data& data)
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
}} // FastCGI::app
