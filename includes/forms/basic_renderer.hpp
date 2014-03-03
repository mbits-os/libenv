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
		virtual void render(Request& request, ControlBase* ctrl, bool, const std::string& = std::string()) {}
		virtual void renderCheckbox(Request& request, ControlBase* ctrl, bool hasError) {}
		virtual void renderSimple(Request& request, ControlBase* ctrl)
		{
			ctrl->getSimpleControlString(request, *this);
		}

		virtual void setPlaceholder(ControlBase* ctrl, const std::string& placeholder) {}
		virtual void getLabelString(Request& request, const std::string& name, const std::string& label);
		virtual void selectionLabelString(Request& request, const std::string& name, const std::string& label)
		{
			getLabelString(request, name, label);
		}
		virtual void getErrorString(Request& request, const std::string& error) {}
		virtual void getMessagesString(Request& request, const std::list<std::string>& messages) {}
		virtual void getHintString(Request& request, const std::string& hint) {}
		virtual void getControlString(Request& request, ControlBase* control, const std::string& element, bool hasError) {}
		virtual void getControlString(Request& request, ControlBase* control, const std::string& element, bool hasError, const std::string& content) {}
		virtual void getControlString(Request& request, ControlBase* control, const std::string& element, bool hasError, const ChildrenCallback& op) {}
		virtual void selectionControlString(Request& request, ControlBase* control, bool, const ChildrenCallback& op){}
		virtual void checkboxControlString(Request& request, ControlBase* control, bool hasError) {}
		virtual void checkboxLabelString(Request& request, const std::string& name, const std::string& label){}
		virtual void radioGroupControlString(Request& request, ControlBase* control, const std::string& title, bool hasError) {}
		virtual void radioGroupLabelString(Request& request, const std::string& hint) {}
		virtual void radioControlString(Request& request, ControlBase* control, bool hasError) {}
		virtual void getSectionStart(Request& request, size_t sectionId, const std::string& name) {}
		virtual void getSectionEnd(Request& request, size_t sectionId, const std::string& name) {}
		virtual void getFormStart(Request& request, const std::string& title) {}
		virtual void getFormEnd(Request& request) {}
		virtual void getFieldsetStart(Request& request, const std::string& title);
		virtual void getFieldsetEnd(Request& request);

		virtual void getButtons(Request& request, const Controls& buttons);
	};

} // FastCGI

#endif // __FORMS_BASIC_RENDERER_H__
