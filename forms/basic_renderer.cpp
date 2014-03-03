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
#include <forms/basic_renderer.hpp>
#include <forms/controls.hpp>
#include <fast_cgi/request.hpp>

namespace FastCGI {

	void BasicRenderer::getLabelString(Request& request, const std::string& name, const std::string& label)
	{
		request << "<label for='" << name << "'>" << label << "</label>";
	}

	void BasicRenderer::getFieldsetStart(Request& request, const std::string& title)
	{
		request << "<fieldset>\r\n";
		if (!title.empty())
			request << "<legend>" << title << "</legend>\r\n";
	}

	void BasicRenderer::getFieldsetEnd(Request& request)
	{
		request << "</fieldset>\r\n";
	}

	void BasicRenderer::getButtons(Request& request, const Controls& buttons)
	{
		for (auto&& ctrl : buttons)
			ctrl->renderSimple(request, *this);
	}
} // FastCGI
