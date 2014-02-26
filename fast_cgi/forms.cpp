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
#include <forms.hpp>
#include <utils.hpp>

namespace FastCGI {

	Control::Control(const std::string& name, const std::string& label, const std::string& hint)
		: m_name(name)
		, m_label(label)
		, m_hint(hint)
		, m_userValue(false)
		, m_hasError(false)
	{
		if (!name.empty())
		{
			setAttr("name", name);
			setAttr("id", name);
		}
	}

	bool Control::getLabelString(Request& request)
	{
		if (!m_label.empty())
			request << "<label for='" << m_name << "'>" << m_label << "</label>";
		return !m_label.empty();
	}

	bool Control::getHintString(Request& request)
	{
		if (!m_hint.empty())
			request << "<tr><td></td><td class='hint'>" << m_hint << "</td></tr>\r\n			";
		return !m_hint.empty();
	}

	void Control::getAttributes(Request& request)
	{
		for (auto&& pair: m_attrs) {
			request << " " << pair.first << "='" << url::htmlQuotes(pair.second) << "'";
		};
	}

	void Control::getElement(Request& request, const std::string& name, const std::string& content)
	{
		request << "<" << name;
		getAttributes(request);
		if (!content.empty())
			request << "/>" << content << "</" << name << ">";
		else
			request << "/>";
	}

	void Control::render(Request& request)
	{
		request << "<tr>";
		getControlString(request);
		request << "</tr>\r\n";
		getHintString(request);
	}

	void Control::renderSimple(Request& request)
	{
		getSimpleControlString(request);
	}

	void Control::bindData(Request& request, const Strings& data)
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

	Control* Section::findControl(const std::string& name)
	{
		Control* ptr = nullptr;
		std::find_if(m_controls.begin(), m_controls.end(), [&ptr, &name](ControlPtr& ctrl) -> bool
		{
			ptr = ctrl->findControl(name);
			return ptr != nullptr;
		});
		return ptr;
	}

	void Checkbox::bindData(Request& request, const Strings& data)
	{
		if (m_name.empty()) return;

		m_userValue = false;
		m_value.erase();

		if (request.getVariable("posted") != nullptr)
		{
			param_t value = request.getVariable(m_name.c_str());
			m_userValue = true;
			if (value)
				m_value = value;
			else
				m_value.erase();
			m_checked = value != nullptr;
			return;
		}

		auto _it = data.find(m_name);
		if (_it != data.end())
		{
			m_value = _it->second;
			m_checked = m_value == "1" || m_value == "true";
			return;
		}
	}

	void Section::bind(Request& request, const Strings& data)
	{
		for (auto&& ctrl: m_controls)
		{
			ctrl->bind(request, data);
		};
	}

	void Section::render(Request& request, size_t pageId)
	{
		if (!m_name.empty())
			request << "<tr><td colspan='2' class='header'><h3 name='page" << pageId << "' id='page" << pageId <<"'>" << m_name << "</h3></td></tr>\r\n";
		for (auto&& ctrl: m_controls)
		{
			ctrl->render(request);
		};
	}

	Form::Form(const std::string& title, const std::string& method, const std::string& action, const std::string& mime)
		: m_title(title)
		, m_method(method)
		, m_action(action)
		, m_mime(mime)
	{
	}

	void Form::render(SessionPtr session, Request& request, PageTranslation& tr)
	{
		request << "\r\n<form method='" << m_method << "'";
		if (!m_action.empty()) request << " action='" << m_action << "'";
		if (!m_mime.empty()) request << " enctype='" << m_mime << "'";
		request << ">\r\n<input type='hidden' name='posted' value='1' />\r\n";

		for (auto&& hidden: m_hidden) {
			param_t var = request.getVariable(hidden.c_str());
			if (var != nullptr)
				request << "<input type='hidden' name='" << hidden << "' value='" << url::htmlQuotes(var) << "' />\r\n";
		};

		request << "\r\n"
			"<div class='form'>\r\n"
			"<table class='form-table'>\r\n";

		if (!m_error.empty())
			request
				<< "      <tr><td colspan='2' class='error'>" << m_error << "</td></tr>\r\n";

		size_t pageId = 0;
		for (auto&& s: m_sections) {
			s.render(request, ++pageId);
		};

		if (!m_buttons.empty())
		{
			request << "\r\n"
				"<tr><td colspan='2' class='buttons'>\r\n";

			for (auto&& ctrl: m_buttons)
			{
				ctrl->renderSimple(request);
			};
			request << "\r\n</td></tr>\r\n";
		}
		request << "\r\n"
			"</table>\r\n"
			"</div>\r\n"
			"</form>\r\n";
	}

	Control* Form::findControl(const std::string& name)
	{
		Control* ptr = nullptr;

		std::find_if(m_sections.begin(), m_sections.end(), [&ptr, &name](Section& section) -> bool
		{
			ptr = section.findControl(name);
			return ptr != nullptr;
		});
		if (ptr != nullptr) return ptr;

		std::find_if(m_buttons.begin(), m_buttons.end(), [&ptr, &name](ControlPtr& ctrl) -> bool
		{
			ptr = ctrl->findControl(name);
			return ptr != nullptr;
		});
		return ptr;
	}

	void Form::bind(Request& request, const Strings& data)
	{
		for (auto&& section: m_sections)
		{
			section.bind(request, data);
		};

		for (auto&& ctrl: m_buttons)
		{
			ctrl->bind(request, data);
		};
	}
} // FastCGI
