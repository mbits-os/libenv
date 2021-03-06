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
#include <forms/controls.hpp>
#include <fast_cgi/request.hpp>

namespace FastCGI {

	void VerticalRenderer::render(Request& request, ControlBase* ctrl, bool hasError, const std::string& additional_classes)
	{
		request <<
			"      <div class='item";
		if (!additional_classes.empty())
			request << ' ' << additional_classes;
		if (hasError)
			request << " error";
		request << "'>\r\n";
		ctrl->getLabelString(request, *this);
		ctrl->getControlString(request, *this);
		request <<
			"      </div>\r\n";
	}

	void VerticalRenderer::renderCheckbox(Request& request, ControlBase* ctrl, bool hasError)
	{
		request <<
			"      <div class='item";
		if (hasError)
			request << " error";
		request << "'>\r\n";
		ctrl->getControlString(request, *this);
		request <<
			"      </div>\r\n";
	}

	void VerticalRenderer::setPlaceholder(ControlBase* ctrl, const std::string& placeholder)
	{
		if (!placeholder.empty())
			ctrl->setAttr("placeholder", placeholder);
	}

	void VerticalRenderer::getLabelString(Request& request, const std::string& name, const std::string& label)
	{
		request << "        <div class='label'><label for='" << name << "'>" << label << ":</label></div>\r\n";
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
		request << "<br/><span class='hint'>" << hint << "</span>";
	}

	void VerticalRenderer::getControlString(Request& request, ControlBase* control, const std::string& element, bool)
	{
		request << "        <div class='control'>";
		control->getElement(request, *this, element);
		control->getHintString(request, *this);
		request << "</div>\r\n";
	}

	void VerticalRenderer::getControlString(Request& request, ControlBase* control, const std::string& element, bool hasError, const std::string& arg)
	{
		request << "        <div class='control'>";
		control->getElement(request, *this, element, arg);
		control->getHintString(request, *this);
		request << "</div>\r\n";
	}

	void VerticalRenderer::getControlString(Request& request, ControlBase* control, const std::string& element, bool hasError, const ChildrenCallback& arg)
	{
		request << "        <div class='control'>";
		control->getElement(request, *this, element, arg);
		control->getHintString(request, *this);
		request << "</div>\r\n";
	}


	void VerticalRenderer::linkControlString(Request& request, ControlBase* control, bool hasError, bool isButton, const std::string& content)
	{
		if (!isButton)
			return getControlString(request, control, "a", hasError, content);

		request << "<div class='control link'>";
		control->getElement(request, *this, "a", content);
		control->getHintString(request, *this);
		request << "</div>";
	}

	void VerticalRenderer::checkboxControlString(Request& request, ControlBase* control, bool hasError)
	{
		request << "        <div class='control'>";
		control->getElement(request, *this, "input");
		control->getLabelString(request, *this);
		control->getHintString(request, *this);
		request << "</div>\r\n";
	}

	void VerticalRenderer::checkboxLabelString(Request& request, const std::string& name, const std::string& label)
	{
		request << "<label for='" << name << "'>" << label << "</label>";
	}

	void VerticalRenderer::radioGroupControlString(Request& request, ControlBase* control, const std::string& title, bool hasError)
	{
	}

	void VerticalRenderer::radioGroupLabelString(Request& request, const std::string& label)
	{
		request << "        <div class='label'>" << label << ":</div>\r\n";
	}

	void VerticalRenderer::selectionLabelString(Request& request, const std::string& name, const std::string& label)
	{
		request << "        <div class='label retain'><label for='" << name << "'>" << label << ":</label></div>\r\n";
	}

	void VerticalRenderer::selectionControlString(Request& request, ControlBase* control, bool, const ChildrenCallback& op)
	{
		request << "        <div class='control'>";
		control->getElement(request, *this, "select", op);
		control->getHintString(request, *this);
		request << "</div>\r\n";
	}

	void VerticalRenderer::getSectionStart(Request& request, size_t sectionId, const std::string& name)
	{
		request <<
			"    <div class='section' id='sec-" << sectionId << "'>\r\n"
			"      <h2>" << name << "</h2>\r\n";
	}

	void VerticalRenderer::getSectionEnd(Request& request, size_t sectionId, const std::string& name)
	{
		request <<
			"    </div> <!-- sec-" << sectionId << " -->\r\n";
	}

	void VerticalRenderer::getFormStart(Request& request, const std::string& title)
	{
		request << "\r\n"
			"  <div class='form'>\r\n"
			"    <h1>" << title << "</h1>\r\n";
	}

	void VerticalRenderer::getFormEnd(Request& request)
	{
#if DEBUG_CGI
		request <<
			"\r\n"
			"    <div class=\"debug-icons\">\r\n"
			"      <div>\r\n"
			"        <a class=\"debug-icon\" href=\"/debug/\"><span>[D]</span></a>\r\n";
		std::string icicle = request.getIcicle();
		if (!icicle.empty())
			request << "        <a class=\"frozen-icon\" href=\"/debug/?frozen=" << url::encode(icicle) << "\"><span>[F]</span></a>\r\n";
		request <<
			"      </div>\r\n"
			"    </div>\r\n";
#endif

		request <<
			"  </div> <!-- class='form' -->\r\n";
	}

	void VerticalRenderer::getButtons(Request& request, const Controls& buttons)
	{

		if (!buttons.empty())
		{
			request << "\r\n"
				"    <div class='item buttons'>\r\n";

			for (auto&& ctrl : buttons)
			{
				request << "      ";
				ctrl->renderSimple(request, *this);
				request << "\r\n";
			}

			request << "    </div> <!-- buttons -->\r\n";
		}
	}

} // FastCGI
