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
#include <forms/vertical_renderer.hpp>
#include <fast_cgi/request.hpp>

namespace FastCGI {

	void VerticalRenderer::render(Request& request, ControlBase* ctrl)
	{
		ctrl->getControlString(request);
		request << "\r\n";
	}

	void VerticalRenderer::setPlaceholder(ControlBase* ctrl, const std::string& placeholder)
	{
		if (!placeholder.empty())
			ctrl->setAttr("placeholder", placeholder);
	}

	void VerticalRenderer::getErrorString(Request& request, const std::string& error)
	{
		request << "<p class='error'>" << error << "</p>\r\n";
	}

	void VerticalRenderer::getMessagesString(Request& request, const std::list<std::string>& messages)
	{
		for (auto&& m : messages)
			request << "<p class='message'>" << m << "</p>\r\n";
	}

	void VerticalRenderer::getHintString(Request& request, const std::string& hint)
	{
		request << "<tr><td></td><td class='hint'>" << hint << "</td></tr>\r\n";
	}

	void VerticalRenderer::getControlString(Request& request, ControlBase* control, const std::string& element, bool hasError)
	{
		if (hasError)
			request << "<li class='error'>";
		else
			request << "<li>";
		control->getElement(request, element);
		request << "</li>";
	}

	void VerticalRenderer::checkboxControlString(Request& request, ControlBase* control, bool hasError)
	{
		if (hasError)
			request << "<li class='error'>";
		else
			request << "<li>";
		control->getElement(request, "input");
		control->getLabelString(request);
		request << "</li>";
	}

	void VerticalRenderer::radioGroupControlString(Request& request, ControlBase* control, const std::string& title, bool hasError)
	{
		if (hasError)
			request << "<li class='error'>";
		else
			request << "<li>";
		control->getElement(request, "span", title);
		request << "</li>";
	}

	void VerticalRenderer::getFormStart(Request& request, const std::string& title)
	{
		request << "\r\n"
			"<div class='form'>\r\n"
			"<h1>" << title << "</h1>\r\n";
	}

	void VerticalRenderer::getFormEnd(Request& request)
	{
		request << "</ul>\r\n";
#if DEBUG_CGI
		request << "\r\n"
			"<div class=\"debug-icons\"><div>\r\n"
			"<a class=\"debug-icon\" href=\"/debug/\"><span>[D]</span></a>\r\n";
		std::string icicle = request.getIcicle();
		if (!icicle.empty())
			request << "<a class=\"frozen-icon\" href=\"/debug/?frozen=" << url::encode(icicle) << "\"><span>[F]</span></a>\r\n";
		request << "</div></div>\r\n";
#endif

		request <<
			"</div>\r\n";
	}

} // FastCGI
