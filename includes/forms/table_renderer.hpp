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

#ifndef __FORMS_TABLE_RENDERER_H__
#define __FORMS_TABLE_RENDERER_H__

#include <forms/basic_renderer.hpp>

namespace FastCGI {
	struct TableRenderer : BasicRenderer
	{
		void render(Request& request, ControlBase* ctrl, bool, const std::string& = std::string()) override;
		void getErrorString(Request& request, const std::string& error) override;
		void getHintString(Request& request, const std::string& hint) override;
		void getControlString(Request& request, ControlBase* control, const std::string& element, bool hasError) override;
		void getControlString(Request& request, ControlBase* control, const std::string& element, bool hasError, const std::string& arg) override;
		void getControlString(Request& request, ControlBase* control, const std::string& element, bool hasError, const ChildrenCallback& arg) override;

		void checkboxControlString(Request& request, ControlBase* control, bool hasError) override;
		void radioControlString(Request& request, ControlBase* control, bool hasError) override;

		void selectionControlString(Request& request, ControlBase* control, bool hasError, const ChildrenCallback& op) override;

		void getSectionStart(Request& request, size_t sectionId, const std::string& name) override;
		void getFormStart(Request& request, const std::string& title) override;
		void getFormEnd(Request& request) override;

		void getButtons(Request& request, const Controls& buttons) override;
	};
} // FastCGI

#endif // __FORMS_TABLE_RENDERER_H__
