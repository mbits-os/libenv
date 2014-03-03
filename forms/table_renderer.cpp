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
#include <forms/table_renderer.hpp>
#include <forms/controls.hpp>
#include <fast_cgi/request.hpp>

namespace FastCGI {
	void TableRenderer::render(Request& request, ControlBase* ctrl, bool, const std::string&)
	{
		request << "<tr>";
		ctrl->getControlString(request, *this);
		request << "</tr>\r\n";
		ctrl->getHintString(request, *this);
	}

	void TableRenderer::getErrorString(Request& request, const std::string& error)
	{
		request << "<tr><td colspan='2' class='error'>" << error << "</td></tr>\r\n";
	}

	void TableRenderer::getHintString(Request& request, const std::string& hint)
	{
		request << "<tr><td></td><td class='hint'>" << hint << "</td></tr>\r\n";
	}

	void TableRenderer::getControlString(Request& request, ControlBase* control, const std::string& element, bool hasError)
	{
		request << "<td>";
		control->getLabelString(request, *this);
		request << "</td><td>";
		control->getElement(request, *this, element);
		request << "</td>";
	}

	void TableRenderer::getControlString(Request& request, ControlBase* control, const std::string& element, bool hasError, const std::string& arg)
	{
		request << "<td>";
		control->getLabelString(request, *this);
		request << "</td><td>";
		control->getElement(request, *this, element, arg);
		request << "</td>";
	}

	void TableRenderer::getControlString(Request& request, ControlBase* control, const std::string& element, bool hasError, const ChildrenCallback& arg)
	{
		request << "<td>";
		control->getLabelString(request, *this);
		request << "</td><td>";
		control->getElement(request, *this, element, arg);
		request << "</td>";
	}

	void TableRenderer::checkboxControlString(Request& request, ControlBase* control, bool hasError)
	{
		request << "<td>";
		control->getElement(request, *this, "input");
		request << "</td><td>";
		control->getLabelString(request, *this);
		request << "</td>";
	}

	void TableRenderer::selectionControlString(Request& request, ControlBase* control, bool hasError, const ChildrenCallback& op)
	{
		request << "<td>";
		control->getLabelString(request, *this);
		request << "</td><td>";
		control->getElement(request, *this, "select", op);
		request << "</td>";
	}

	void TableRenderer::getSectionStart(Request& request, size_t sectionId, const std::string& name)
	{
		if (!name.empty())
			request << "<tr><td colspan='2' class='header'><h3 name='page" << sectionId << "' id='page" << sectionId << "'>" << name << "</h3></td></tr>\r\n";
	}

	void TableRenderer::getFormStart(Request& request, const std::string& title)
	{
		request <<
			"<div class='form'>\r\n"
			"<table class='form-table'>\r\n";
	}

	void TableRenderer::getFormEnd(Request& request)
	{
		request << "\r\n"
			"</table>\r\n"
			"</div>\r\n";
	}

	void TableRenderer::getButtons(Request& request, const Controls& buttons)
	{

		if (!buttons.empty())
		{
			request << "\r\n"
				"<tr><td colspan='2' class='buttons'>\r\n";

			for (auto&& ctrl : buttons)
				ctrl->renderSimple(request, *this);

			request << "\r\n</td></tr>\r\n";
		}
	}

} // FastCGI
