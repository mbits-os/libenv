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

#ifndef __FORMS_FORMS_H__
#define __FORMS_FORMS_H__

#include <fast_cgi.hpp>
#include <forms/controls.hpp>

namespace FastCGI {

	template <typename Renderer> class Section;
	template <typename Renderer> using Sections = std::list<Section<Renderer>>;

	template <typename Renderer> class Fieldset;
	template <typename Renderer> using FieldsetPtr = std::shared_ptr<Fieldset<Renderer>>;

	template <typename Renderer = BasicRenderer>
	class ControlContainer
	{
		Controls<Renderer> m_controls;
	public:
		template <typename Class>
		std::shared_ptr<Class> control(const std::string& name, const std::string& label = std::string(), const std::string& hint = std::string())
		{
			auto obj = std::make_shared<Class>(name, label, hint);
			if (obj)
				m_controls.push_back(obj);
			return obj;
		}

		TextPtr<Renderer> text(const std::string& name, const std::string& label = std::string(), bool pass = false, const std::string& hint = std::string())
		{
			auto obj = std::make_shared<Text<Renderer>>(name, label, pass, hint);
			if (obj)
				m_controls.push_back(obj);
			return obj;
		}

		SelectionPtr<Renderer> selection(const std::string& name, const std::string& label, const Options& options, const std::string& hint = std::string())
		{
			auto obj = std::make_shared<Selection<Renderer>>(name, label, options, hint);
			if (obj)
				m_controls.push_back(obj);
			return obj;
		}

		CheckboxPtr<Renderer> checkbox(const std::string& name, const std::string& label = std::string(), const std::string& hint = std::string())
		{
			return control<Checkbox<Renderer>>(name, label, hint);
		}

		RadioPtr<Renderer> radio(const std::string& group, const std::string& value, const std::string& label = std::string())
		{
			auto obj = std::make_shared<Radio<Renderer>>(group, value, label);
			if (obj)
				m_controls.push_back(obj);
			return obj;
		}

		RadioGroupPtr<Renderer> radio_group(const std::string& group, const std::string& label = std::string())
		{
			auto obj = std::make_shared<RadioGroup<Renderer>>(group, label);
			if (obj)
				m_controls.push_back(obj);
			return obj;
		}

		FieldsetPtr<Renderer> fieldset(const std::string& label = std::string(), const std::string& hint = std::string())
		{
			return control<Fieldset<Renderer>>(std::string(), label, hint);
		}

		LinkPtr<Renderer> ctrl_link(const std::string& name, const std::string& link, const std::string& text)
		{
			auto ptr = std::make_shared<Link<Renderer>>(name, link, text);
			if (ptr)
				m_controls.push_back(ptr);
			return ptr;
		}

		ControlBase* findControl(const std::string& name)
		{
			for (auto&& ctrl : m_controls)
			{
				auto ptr = ctrl->findControl(name);
				if (ptr)
					return ptr;
			}
			return nullptr;
		}

		void renderControls(Request& request)
		{
			for (auto&& ctrl : m_controls)
				ctrl->render(request);
		}

		void renderControlsSimple(Request& request)
		{
			for (auto&& ctrl : m_controls)
				ctrl->renderSimple(request);
		}

		void bind(Request& request, const Strings& data)
		{
			for (auto&& ctrl : m_controls)
				ctrl->bind(request, data);
		}
	};

	template <typename Renderer = BasicRenderer>
	class Fieldset : public Control<Renderer>, public ControlContainer<Renderer>
	{
	public:
		Fieldset(const std::string& name, const std::string& label, const std::string& hint)
			: Control<Renderer>(name, label, hint)
		{
		}

		void getControlString(Request& request)
		{
			Renderer::getFieldsetStart(request, this->m_label);
			this->renderControls(request);
			Renderer::getFieldsetEnd(request);
		}
	};

	template <typename Renderer = BasicRenderer>
	class ButtonContainer
	{
		Controls<Renderer> m_buttons;
	public:
		SubmitPtr<Renderer> submit(const std::string& name, const std::string& label = std::string(), bool narrow = false, const std::string& hint = std::string())
		{
			auto ptr = std::make_shared<Submit<Renderer>>(name, label, narrow, hint);

			if (ptr)
				m_buttons.push_back(ptr);

			return ptr;
		}

		ResetPtr<Renderer> reset(const std::string& name, const std::string& label = std::string(), const std::string& hint = std::string())
		{
			auto ptr = std::make_shared<Reset<Renderer>>(name, label, hint);

			if (ptr)
				m_buttons.push_back(ptr);

			return ptr;
		}

		LinkPtr<Renderer> cmd_link(const std::string& name, const std::string& link, const std::string& text)
		{
			auto ptr = std::make_shared<Link<Renderer>>(name, link, text);
			if (ptr)
				m_buttons.push_back(ptr);
			return ptr;
		}

		ControlBase* findControl(const std::string& name)
		{
			for (auto&& ctrl : m_buttons)
			{
				auto ptr = ctrl->findControl(name);
				if (ptr)
					return ptr;
			}
			return nullptr;
		}

		void renderControls(Request& request)
		{
			Renderer::getButtons(request, m_buttons);
		}

		void bind(Request& request, const Strings& data)
		{
			for (auto&& ctrl : m_buttons)
				ctrl->bind(request, data);
		}
	};

	template <typename Renderer = BasicRenderer>
	class Section : public ControlContainer<Renderer>
	{
		std::string m_name;
	public:
		Section(const std::string& name): m_name(name) {}

		ControlBase* findControl(const std::string& name)
		{
			for (auto&& ctrl : this->m_controls)
			{
				auto ptr = ctrl->findControl(name);
				if (ptr)
					return ptr;
			}
			return nullptr;
		}

		void bind(Request& request, const Strings& data)
		{
			for (auto&& ctrl : this->m_controls)
				ctrl->bind(request, data);
		}

		void render(Request& request, size_t pageId)
		{
			Renderer::getSectionStart(request, pageId, m_name);
			ControlContainer<Renderer>::renderControls(request);
			Renderer::getSectionEnd(request, pageId, m_name);
		}

	};

	template <typename Renderer = BasicRenderer>
	class SectionContainer
	{
		Sections<Renderer> m_sections;
	public:
		Section<Renderer>& section(const std::string& name)
		{
			m_sections.push_back(Section<Renderer>(name));
			return m_sections.back();
		}

		ControlBase* findControl(const std::string& name)
		{
			for (auto&& sec : m_sections)
			{
				auto ptr = sec.findControl(name);
				if (ptr)
					return ptr;
			}
			return nullptr;
		}

		void renderControls(Request& request)
		{
			size_t pageId = 0;
			for (auto&& s : m_sections)
				s.render(request, ++pageId);
		}

		void bind(Request& request, const Strings& data)
		{
			for (auto&& section : m_sections)
				section.bind(request, data);
		}
	};

	class FormBase
	{
	protected:
		std::string   m_title;
		std::string   m_method;
		std::string   m_action;
		std::string   m_mime;
		std::list<std::string> m_hidden;
		std::list<std::string> m_messages;
		std::string   m_error;
	public:
		FormBase(const std::string& title, const std::string& method, const std::string& action, const std::string& mime);
		void formStart(SessionPtr session, Request& request, PageTranslation& tr);
		void formEnd(SessionPtr session, Request& request, PageTranslation& tr);

		void hidden(const std::string& name) { m_hidden.push_back(name); }
		void setError(const std::string& error) { m_error = error; }
		void addMessage(const std::string& message) { m_messages.push_back(message); }
	};

	template <typename Renderer = BasicRenderer, template <typename R> class Controls = ControlContainer, typename Base = Content>
	class FormImpl : public Base, public ButtonContainer<Renderer>, public Controls<Renderer>, public FormBase
	{
	public:
		using RendererT = Renderer;
		using ButtonsT = ButtonContainer<Renderer>;
		using ControlsT = Controls<Renderer>;

		FormImpl(const std::string& title, const std::string& method = "POST", const std::string& action = std::string(), const std::string& mime = std::string())
			: FormBase(title, method, action, mime)
		{
		}

		const char* getPageTitle(PageTranslation& tr) override { return m_title.c_str(); }
		virtual const char* getFormTitle(PageTranslation& tr) { return getPageTitle(tr); }
		void render(const SessionPtr& session, Request& request, PageTranslation& tr) override
		{
			FormBase::formStart(session, request, tr);
			RendererT::getFormStart(request, getFormTitle(tr));

			RendererT::getMessagesString(request, m_messages);

			if (!m_error.empty())
				RendererT::getErrorString(request, m_error);

			ControlsT::renderControls(request);
			ButtonsT::renderControls(request);

			RendererT::getFormEnd(request);
			FormBase::formEnd(session, request, tr);
		}


		ControlBase* findControl(const std::string& name)
		{
			auto ptr = ControlsT::findControl(name);
			if (ptr)
				return ptr;

			return ButtonsT::findControl(name);
		}

		void bind(Request& request, const Strings& data = Strings())
		{
			ControlsT::bind(request, data);
			ButtonsT::bind(request, data);
		}
	};

	template <typename Renderer = BasicRenderer, typename Base = Content>
	using SectionForm = FormImpl<Renderer, SectionContainer, Base>;

	template <typename Renderer = BasicRenderer, typename Base = Content>
	using SimpleForm = FormImpl<Renderer, ControlContainer, Base>;

} // FastCGI

#endif // __FORMS_FORMS_H__
