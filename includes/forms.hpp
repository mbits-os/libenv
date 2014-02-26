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

#ifndef __FORMS_H__
#define __FORMS_H__

#include <fast_cgi.hpp>

namespace FastCGI {

	class Control;
	class Input;
	class Text;
	class Checkbox;
	class Submit;
	class Reset;
	class Section;
	class Form;

	typedef std::shared_ptr<Control>  ControlPtr;
	typedef std::shared_ptr<Section>  SectionPtr;
	typedef std::shared_ptr<Input>    InputPtr;
	typedef std::shared_ptr<Text>     TextPtr;
	typedef std::shared_ptr<Checkbox> CheckboxPtr;
	typedef std::shared_ptr<Submit>   SubmitPtr;
	typedef std::shared_ptr<Reset>    ResetPtr;
	typedef std::shared_ptr<Form>     FormPtr;

	typedef std::list<Section> Sections;
	typedef std::list<ControlPtr> Controls;

	typedef std::map<std::string, std::string> Strings;

	class Control
	{
	protected:
		std::string m_name;
		std::string m_label;
		std::string m_hint;
		std::string m_value;
		bool        m_userValue;
		Strings     m_attrs;
		bool        m_hasError;

		virtual void getControlString(Request& request) { }
		virtual void getSimpleControlString(Request& request) { getControlString(request); }
		virtual bool getLabelString(Request& request);
		virtual bool getHintString(Request& request);

		void getAttributes(Request& request);
		void getElement(Request& request, const std::string& name, const std::string& content = std::string());
	public:
		Control(const std::string& name, const std::string& label, const std::string& hint);
		virtual ~Control() {}

		virtual void render(Request& request);
		virtual void renderSimple(Request& request);

		virtual Control* findControl(const std::string& name)
		{
			if (m_name == name) return this;
			return nullptr;
		}

		void setAttr(const std::string& key, const std::string& value)
		{
			m_attrs[key] = value;
		}

		virtual void bindData(Request& request, const Strings& data);
		virtual void bindUI()
		{
			if (!m_value.empty())
				setAttr("value", m_value);
		}
		void bind(Request& request, const Strings& data)
		{
			m_userValue = false;
			m_value.erase();
			bindData(request, data);
			bindUI();
		}
		bool hasUserData() const { return m_userValue && !m_value.empty(); }
		const std::string& getData() const { return m_value; }
		void setError(bool val = true) { m_hasError = val; }
	};

	class Input: public Control
	{
	public:
		Input(const std::string& type, const std::string& name, const std::string& label, const std::string& hint)
			: Control(name, label, hint)
		{
			setAttr("type", type);
		}

		void getControlString(Request& request)
		{
			request << "<td>";
			getLabelString(request);
			request << "</td><td>";
			getElement(request, "input");
			request << "</td>";
		}

		void getSimpleControlString(Request& request)
		{
			if (getLabelString(request))
				request << ": ";
			getElement(request, "input");
		}
	};

	class Text: public Input
	{
		bool m_pass;
	public:
		Text(const std::string& name, const std::string& label, bool pass, const std::string& hint)
			: Input(pass ? "password" : "text", name, label, hint)
			, m_pass(pass)
		{
		}

		void bindUI()
		{
			if (!m_pass)
				Input::bindUI();
		}
	};

	class Checkbox: public Input
	{
		bool m_checked;
	public:
		Checkbox(const std::string& name, const std::string& label, const std::string& hint)
			: Input("checkbox", name, label, hint)
			, m_checked(false)
		{
		}
		void getControlString(Request& request)
		{
			request << "<td>";
			getElement(request, "input");
			request << "</td><td>";
			getLabelString(request);
			request << "</td>";
		}

		void bindData(Request& request, const Strings& data);

		void bindUI()
		{
			if (m_checked)
				setAttr("checked", "checked");
		}

		bool isChecked() const { return m_checked; }
	};

	class Submit: public Input
	{
	public:
		Submit(const std::string& name, const std::string& label, const std::string& hint)
			: Input("submit", name, std::string(), hint)
		{
			setAttr("value", label);
		}

		void bindUI() {}
	};

	class Reset: public Input
	{
	public:
		Reset(const std::string& name, const std::string& label, const std::string& hint)
			: Input("reset", name, std::string(), hint)
		{
			setAttr("value", label);
		}

		void bindUI() {}
	};

	class Section
	{
		friend class From;
		Controls m_controls;
		std::string m_name;
	public:
		Section(const std::string& name): m_name(name) {}
		Control* findControl(const std::string& name);
		void bind(Request& request, const Strings& data);
		void render(Request& request, size_t pageId);

		template <typename Class>
		std::shared_ptr<Class> control(const std::string& name, const std::string& label = std::string(), const std::string& hint = std::string())
		{
			auto obj = std::make_shared<Class>(name, label, hint);
			if (obj.get())
				m_controls.push_back(obj);
			return obj;
		}

		TextPtr text(const std::string& name, const std::string& label = std::string(), bool pass = false, const std::string& hint = std::string())
		{
			auto obj = std::make_shared<Text>(name, label, pass, hint);
			if (obj.get())
				m_controls.push_back(obj);
			return obj;
		}

		CheckboxPtr checkbox(const std::string& name, const std::string& label = std::string(), const std::string& hint = std::string())
		{
			return control<Checkbox>(name, label, hint);
		}
	};

	class Form: public Content
	{
		std::string   m_title;
		std::string   m_method;
		std::string   m_action;
		std::string   m_mime;
		std::list<std::string> m_hidden;
		std::string   m_error;
		Sections      m_sections;
		Controls      m_buttons;
	public:
		Form(const std::string& title, const std::string& method = "POST", const std::string& action = std::string(), const std::string& mime = std::string());

		const char* getPageTitle(PageTranslation& tr) { return m_title.c_str(); }
		void render(SessionPtr session, Request& request, PageTranslation& tr);

		Form& hidden(const std::string& name)
		{
			m_hidden.push_back(name);
			return *this;
		}

		Section& section(const std::string& name)
		{
			m_sections.push_back(Section(name));
			return m_sections.back();
		}

		SubmitPtr submit(const std::string& name, const std::string& label = std::string(), const std::string& hint = std::string())
		{
			auto ptr = std::make_shared<Submit>(name, label, hint);

			if (ptr.get())
				m_buttons.push_back(ptr);

			return ptr;
		}

		ResetPtr reset(const std::string& name, const std::string& label = std::string(), const std::string& hint = std::string())
		{
			auto ptr = std::make_shared<Reset>(name, label, hint);

			if (ptr.get())
				m_buttons.push_back(ptr);

			return ptr;
		}

		Control* findControl(const std::string& name);
		void bind(Request& request, const Strings& data = Strings());
		void setError(const std::string& error) { m_error = error; }
	};

} // FastCGI

#endif // __FORMS_H__
