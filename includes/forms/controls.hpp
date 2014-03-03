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

#ifndef __FORMS_CONTROLS_H__
#define __FORMS_CONTROLS_H__

#include <forms/control_base.hpp>
#include <forms/basic_renderer.hpp>
#include <vector>

namespace FastCGI {

	class Input;
	class Text;
	class Checkbox;
	class Radio;
	class RadioGroup;
	class Selection;
	class Submit;
	class Reset;
	class Link;

	using InputPtr      = std::shared_ptr<Input>;
	using TextPtr       = std::shared_ptr<Text>;
	using CheckboxPtr   = std::shared_ptr<Checkbox>;
	using RadioPtr      = std::shared_ptr<Radio>;
	using RadioGroupPtr = std::shared_ptr<RadioGroup>;
	using SelectionPtr  = std::shared_ptr<Selection>;
	using SubmitPtr     = std::shared_ptr<Submit>;
	using ResetPtr      = std::shared_ptr<Reset>;
	using LinkPtr       = std::shared_ptr<Link>;

	struct BasicRenderer;

	class Control: public ControlBase
	{
	protected:
		void getControlString(Request& request, BasicRenderer& renderer) override { }
		bool getLabelString(Request& request, BasicRenderer& renderer) override
		{
			if (!m_label.empty())
				renderer.getLabelString(request, m_name, m_label);
			return !m_label.empty();
		}

		bool getHintString(Request& request, BasicRenderer& renderer) override
		{
			if (!m_hint.empty())
				renderer.getHintString(request, m_hint);
			return !m_hint.empty();
		}

	public:
		Control(const std::string& name, const std::string& label, const std::string& hint)
			: ControlBase(name, label, hint)
		{
		}

		void render(Request& request, BasicRenderer& renderer) override
		{
			renderer.render(request, this, m_hasError);
		}
		void renderSimple(Request& request, BasicRenderer& renderer) override
		{
			renderer.renderSimple(request, this);
		}
	};

	class Input: public Control
	{
	public:
		Input(const std::string& type, const std::string& name, const std::string& label, const std::string& hint)
			: Control(name, label, hint)
		{
			setAttr("type", type);
			setAttr("placeholder", label);
		}

		void getControlString(Request& request, BasicRenderer& renderer) override
		{
			renderer.getControlString(request, this, "input", m_hasError);
		}

		void getSimpleControlString(Request& request, BasicRenderer& renderer) override;
	};

	class Text : public Input
	{
		bool m_pass;
	public:
		Text(const std::string& name, const std::string& label, bool pass, const std::string& hint)
			: Input(pass ? "password" : "text", name, label, hint)
			, m_pass(pass)
		{
		}

		void bindUI() override
		{
			if (!m_pass)
				Input::bindUI();
		}
	};

	class CheckboxBase
	{
	protected:
		bool m_checked;
		void bindData(Request& request, const Strings& data, const std::string& name, std::string& value, bool& userValue);
		CheckboxBase() : m_checked(false) {}
	};

	class Checkbox : public Input, protected CheckboxBase
	{
		bool m_checked;
	public:
		Checkbox(const std::string& name, const std::string& label, const std::string& hint)
			: Input("checkbox", name, label, hint)
		{
			removeAttr("placeholder");
		}

		void render(Request& request, BasicRenderer& renderer) override
		{
			renderer.renderCheckbox(request, this, m_hasError);
		}

		void getControlString(Request& request, BasicRenderer& renderer) override
		{
			renderer.checkboxControlString(request, this, m_hasError);
		}

		bool getLabelString(Request& request, BasicRenderer& renderer) override
		{
			if (!m_label.empty())
				renderer.checkboxLabelString(request, getAttr("id"), m_label);
			return !m_label.empty();
		}

		void bindData(Request& request, const Strings& data) override
		{
			CheckboxBase::bindData(request, data, m_name, m_value, m_userValue);
		}

		void bindUI() override
		{
			if (m_checked)
				setAttr("checked", "checked");
		}

		bool isChecked() const { return m_checked; }
	};

	class Radio : public Input, protected CheckboxBase
	{
		bool m_checked;
	public:
		Radio(const std::string& group, const std::string& value, const std::string& label)
			: Input("radio", group, label, std::string())
		{
			removeAttr("placeholder");
			setAttr("id", group + "-" + value);
			setAttr("value", value);
		}

		void render(Request& request, BasicRenderer& renderer) override
		{
			renderer.renderCheckbox(request, this, m_hasError);
		}

		void getControlString(Request& request, BasicRenderer& renderer) override
		{
			renderer.checkboxControlString(request, this, m_hasError);
		}

		bool getLabelString(Request& request, BasicRenderer& renderer) override
		{
			if (!m_label.empty())
				renderer.checkboxLabelString(request, getAttr("id"), m_label);
			return !m_label.empty();
		}

		void bindData(Request& request, const Strings& data) override
		{
			CheckboxBase::bindData(request, data, m_name, m_value, m_userValue);
		}

		void bindUI() override
		{
			if (m_checked)
				setAttr("checked", "checked");
		}

		bool isChecked() const { return m_checked; }
		void check(bool val = true) { m_checked = val; }
	};

	class RadioGroup : public Control
	{
		std::list<RadioPtr> m_buttons;
	public:
		RadioGroup(const std::string& name, const std::string& label)
			: Control(name, label, std::string())
		{
		}

		RadioPtr radio(const std::string& value, const std::string& label)
		{
			auto obj = std::make_shared<Radio>(m_name, value, label);
			if (obj)
				m_buttons.push_back(obj);
			return obj;
		}

		void render(Request& request, BasicRenderer& renderer) override
		{
			renderer.render(request, this, m_hasError, "radio-group");
		}

		void getControlString(Request& request, BasicRenderer& renderer) override
		{
			renderer.radioGroupControlString(request, this, m_label, m_hasError);
			for (auto&& radio : m_buttons)
				radio->render(request, renderer);
		}

		bool getLabelString(Request& request, BasicRenderer& renderer) override
		{
			if (!m_label.empty())
				renderer.radioGroupLabelString(request, m_label);
			return !m_label.empty();
		}

		void bindUI() override
		{
			if (m_value.empty())
				return;

			for (auto&& ctrl: m_buttons)
			{
				ctrl->check(ctrl->getAttr("value") == m_value);
				ctrl->bindUI();
			}
		}
	};

	class Options
	{
	public:
		using value_t = std::pair<std::string, std::string>;
		using options_t = std::vector<value_t>;

		void getControlString(Request& request, const std::string& selected);

		Options& add(const std::string& key, const std::string& value)
		{
			m_options.emplace_back(key, value);
			return *this;
		}

	private:
		options_t m_options;
	};

	class Selection : public Control
	{
		Options m_options;
	public:
		Selection(const std::string& name, const std::string& label, const Options& options, const std::string& hint)
			: Control(name, label, hint)
			, m_options(options)
		{
		}

		bool getLabelString(Request& request, BasicRenderer& renderer) override
		{
			if (!m_label.empty())
				renderer.selectionLabelString(request, m_name, m_label);
			return !m_label.empty();
		}

		void getControlString(Request& request, BasicRenderer& renderer) override
		{
			ChildrenCallback children = [this](Request& request, BasicRenderer& renderer){
				m_options.getControlString(request, m_value);
			};
			renderer.selectionControlString(request, this, m_hasError, children);
		}

		Selection& option(const std::string& key, const std::string& value)
		{
			m_options.add(key, value);
			return *this;
		}
	};

	class Submit : public Input
	{
	public:
		Submit(const std::string& name, const std::string& label, bool narrow, const std::string& hint)
			: Input("submit", name, std::string(), hint)
		{
			if (narrow)
				setAttr("class", "narrow");
			setAttr("value", label);
		}

		void bindUI() override {}
	};

	class Reset : public Input
	{
	public:
		Reset(const std::string& name, const std::string& label, const std::string& hint)
			: Input("reset", name, std::string(), hint)
		{
			setAttr("value", label);
		}

		void bindUI() override {}
	};

	class Link : public Control
	{
		std::string m_text;
	public:
		Link(const std::string& name, const std::string& link, const std::string& text)
			: Control(name, std::string(), std::string())
			, m_text(text)
		{
			setAttr("href", link);
		}
		virtual void getControlString(Request& request, BasicRenderer& renderer) override
		{
			renderer.getControlString(request, this, "a", m_hasError, m_text);
		}
		void bindUI() override {}
	};
} // FastCGI

#endif // __FORMS_CONTROLS_H__
