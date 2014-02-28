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

#ifndef __FORMS_BASIC_RENDERER_H__
#define __FORMS_BASIC_RENDERER_H__

#include <forms/control_base.hpp>

namespace FastCGI {

	struct BasicRenderer
	{
		static void render(Request& request, ControlBase* ctrl) {}
		static void renderSimple(Request& request, ControlBase* ctrl)
		{
			ctrl->getSimpleControlString(request);
		}

		static void setPlaceholder(ControlBase* ctrl, const std::string& placeholder) {}
		static void getLabelString(Request& request, const std::string& name, const std::string& label);
		static void getErrorString(Request& request, const std::string& error) {}
		static void getMessagesString(Request& request, const std::list<std::string>& messages) {}
		static void getHintString(Request& request, const std::string& hint) {}
		static void getControlString(Request& request, ControlBase* control, const std::string& element, bool hasError) {}
		template <typename Arg>
		static void getControlString(Request& request, ControlBase* control, bool hasError, Arg&& arg) {}
		static void checkboxControlString(Request& request, ControlBase* control, bool hasError) {}
		static void radioGroupControlString(Request& request, ControlBase* control, const std::string& title, bool hasError) {}
		static void radioControlString(Request& request, ControlBase* control, bool hasError) {}
		static void getSectionStart(Request& request, size_t sectionId, const std::string& name) {}
		static void getSectionEnd(Request& request, size_t sectionId, const std::string& name) {}
		static void getFormStart(Request& request, const std::string& title) {}
		static void getFormEnd(Request& request) {}
		static void getFieldsetStart(Request& request, const std::string& title);
		static void getFieldsetEnd(Request& request);

		template <typename Renderer>
		static void getButtons(Request& request, const Controls<Renderer>& buttons)
		{
			for (auto&& ctrl : buttons)
				ctrl->renderSimple(request);
		}
	};

} // FastCGI

#endif // __FORMS_BASIC_RENDERER_H__
