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

	class Section;
	using Sections = std::list<Section>;

	class Fieldset;
	using FieldsetPtr = std::shared_ptr<Fieldset>;

	class ControlContainer
	{
	protected:
		Controls m_controls;
	public:
		template <typename Class>
		std::shared_ptr<Class> control(const std::string& name, const std::string& label = std::string(), const std::string& hint = std::string())
		{
			auto obj = std::make_shared<Class>(name, label, hint);
			if (obj)
				m_controls.push_back(obj);
			return obj;
		}

		TextPtr text(const std::string& name, const std::string& label = std::string(), bool pass = false, const std::string& hint = std::string())
		{
			auto obj = std::make_shared<Text>(name, label, pass, hint);
			if (obj)
				m_controls.push_back(obj);
			return obj;
		}

		SelectionPtr selection(const std::string& name, const std::string& label, const Options& options, const std::string& hint = std::string())
		{
			auto obj = std::make_shared<Selection>(name, label, options, hint);
			if (obj)
				m_controls.push_back(obj);
			return obj;
		}

		CheckboxPtr checkbox(const std::string& name, const std::string& label = std::string(), const std::string& hint = std::string())
		{
			return control<Checkbox>(name, label, hint);
		}

		RadioPtr radio(const std::string& group, const std::string& value, const std::string& label = std::string())
		{
			auto obj = std::make_shared<Radio>(group, value, label);
			if (obj)
				m_controls.push_back(obj);
			return obj;
		}

		RadioGroupPtr radio_group(const std::string& group, const std::string& label = std::string())
		{
			auto obj = std::make_shared<RadioGroup>(group, label);
			if (obj)
				m_controls.push_back(obj);
			return obj;
		}

		FieldsetPtr fieldset(const std::string& label = std::string(), const std::string& hint = std::string())
		{
			return control<Fieldset>(std::string(), label, hint);
		}

		LinkPtr link(const std::string& name, const std::string& link, const std::string& text)
		{
			auto ptr = std::make_shared<Link>(name, link, text, false);
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

		void renderControls(Request& request, BasicRenderer& renderer)
		{
			for (auto&& ctrl : m_controls)
				ctrl->render(request, renderer);
		}

		void renderControlsSimple(Request& request, BasicRenderer& renderer)
		{
			for (auto&& ctrl : m_controls)
				ctrl->renderSimple(request, renderer);
		}

		void bind(Request& request, const Strings& data)
		{
			for (auto&& ctrl : m_controls)
				ctrl->bind(request, data);
		}
	};

	class Fieldset : public Control, public ControlContainer
	{
	public:
		Fieldset(const std::string& name, const std::string& label, const std::string& hint)
			: Control(name, label, hint)
		{
		}

		void getControlString(Request& request, BasicRenderer& renderer) override
		{
			renderer.getFieldsetStart(request, m_label);
			renderControls(request, renderer);
			renderer.getFieldsetEnd(request);
		}
	};

	class ButtonContainer
	{
		Controls m_buttons;
	public:
		SubmitPtr submit(const std::string& name, const std::string& label = std::string(), ButtonType buttonType = ButtonType::OK, const std::string& hint = std::string())
		{
			auto ptr = std::make_shared<Submit>(name, label, buttonType, hint);

			if (ptr)
				m_buttons.push_back(ptr);

			return ptr;
		}

		ResetPtr reset(const std::string& name, const std::string& label = std::string(), const std::string& hint = std::string())
		{
			auto ptr = std::make_shared<Reset>(name, label, hint);

			if (ptr)
				m_buttons.push_back(ptr);

			return ptr;
		}

		LinkPtr link(const std::string& name, const std::string& link, const std::string& text)
		{
			auto ptr = std::make_shared<Link>(name, link, text, true);
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

		void renderControls(Request& request, BasicRenderer& renderer)
		{
			renderer.getButtons(request, m_buttons);
		}

		void bind(Request& request, const Strings& data)
		{
			for (auto&& ctrl : m_buttons)
				ctrl->bind(request, data);
		}
	};

	class Section : public ControlContainer
	{
		std::string m_name;
	public:
		Section(const std::string& name): m_name(name) {}

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

		void bind(Request& request, const Strings& data)
		{
			for (auto&& ctrl : m_controls)
				ctrl->bind(request, data);
		}

		void render(Request& request, BasicRenderer& renderer, size_t pageId)
		{
			renderer.getSectionStart(request, pageId, m_name);
			ControlContainer::renderControls(request, renderer);
			renderer.getSectionEnd(request, pageId, m_name);
		}

	};

	class SectionsContainer
	{
		Sections m_sections;
	public:
		Section& section(const std::string& name)
		{
			m_sections.push_back(Section(name));
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

		void renderControls(Request& request, BasicRenderer& renderer)
		{
			size_t pageId = 0;
			for (auto&& s : m_sections)
				s.render(request, renderer, ++pageId);
		}

		void bind(Request& request, const Strings& data)
		{
			for (auto&& section : m_sections)
				section.bind(request, data);
		}
	};

	class ControlsContainer
	{
		ControlContainer m_container;
	public:
		ControlContainer& controls()
		{
			return m_container;
		}

		ControlBase* findControl(const std::string& name)
		{
			return m_container.findControl(name);
		}

		void renderControls(Request& request, BasicRenderer& renderer)
		{
			return m_container.renderControls(request, renderer);
		}

		void bind(Request& request, const Strings& data)
		{
			return m_container.bind(request, data);
		}
	};

	class ButtonsContainer
	{
		ButtonContainer m_container;
	public:
		ButtonContainer& buttons()
		{
			return m_container;
		}

		ControlBase* findControl(const std::string& name)
		{
			return m_container.findControl(name);
		}

		void renderControls(Request& request, BasicRenderer& renderer)
		{
			return m_container.renderControls(request, renderer);
		}

		void bind(Request& request, const Strings& data)
		{
			return m_container.bind(request, data);
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

	template <typename Renderer, class Controls, typename Base>
	class FormImpl : public Base, public ButtonsContainer, public Controls, public FormBase
	{
	public:
		using RendererT = Renderer;
		using ButtonsT = ButtonsContainer;
		using ControlsT = Controls;

		FormImpl(const std::string& title, const std::string& method = "POST", const std::string& action = std::string(), const std::string& mime = std::string())
			: FormBase(title, method, action, mime)
		{
		}

		const char* getPageTitle(PageTranslation& tr) override { return m_title.c_str(); }
		virtual const char* getFormTitle(PageTranslation& tr) { return getPageTitle(tr); }
		void render(const SessionPtr& session, Request& request, PageTranslation& tr) override
		{
			RendererT renderer;
			FormBase::formStart(session, request, tr);
			renderer.getFormStart(request, getFormTitle(tr));

			renderer.getMessagesString(request, m_messages);

			if (!m_error.empty())
				renderer.getErrorString(request, m_error);

			ControlsT::renderControls(request, renderer);
			ButtonsT::renderControls(request, renderer);

			renderer.getFormEnd(request);
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

	template <typename Renderer>
	using SectionForm = FormImpl<Renderer, SectionsContainer, Content>;

	template <typename Renderer>
	using SimpleForm = FormImpl<Renderer, ControlsContainer, Content>;

} // FastCGI

#endif // __FORMS_FORMS_H__
