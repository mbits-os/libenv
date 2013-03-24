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

#ifndef __FORMS_H__
#define __FORMS_H__

#include <fast_cgi.h>

namespace FastCGI { namespace app {

	class CGIControl;
	typedef std::tr1::shared_ptr<CGIControl> CGIControlPtr;

	class CGIControl
	{
	protected:
		typedef std::map<std::string, std::string> Attributes;

		std::string m_name;
		std::string m_label;
		std::string m_hint;
		std::string m_value;
		bool        m_userValue;
		Attributes  m_attrs;

		virtual void getControlString(Request& request) { }
		virtual void getLabelString(Request& request);
		virtual void getHintString(Request& request);

		void getAttributes(Request& request);
		void getElement(Request& request, const std::string& name, const std::string& content = std::string());
	public:
		CGIControl(const std::string& name, const std::string& label, const std::string& hint = std::string());
		virtual ~CGIControl() {}

		virtual void render(Request& request);

		virtual CGIControl* findControl(const std::string& name)
		{
			if (m_name == name) return this;
			return nullptr;
		}

		void setAttr(const std::string& key, const std::string& value)
		{
			m_attrs[key] = value;
		}

		typedef std::map<std::string, std::string> Data;
		virtual void bindData(Request& request, const Data& data);
		virtual void bindUI()
		{
			if (!m_value.empty())
				setAttr("value", m_value);
		}
		void bind(Request& request, const Data& data = Data())
		{
			m_userValue = false;
			m_value.erase();
			bindData(request, data);
			bindUI();
		}
		bool hasUserData() const { return m_userValue && !m_value.empty(); }
		const std::string& getData() const { return m_value; }
	};

	class CGIForm
	{
	};

}} // FastCGI::app

#endif // __FORMS_H__
