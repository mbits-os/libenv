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
#include <vector>

namespace FastCGI {

	template <typename Renderer> class Input;
	template <typename Renderer> class Text;
	template <typename Renderer> class Checkbox;
	template <typename Renderer> class Radio;
	template <typename Renderer> class RadioGroup;
	template <typename Renderer> class Selection;
	template <typename Renderer> class Submit;
	template <typename Renderer> class Reset;
	template <typename Renderer> class Link;

	template <typename Renderer> using InputPtr      = std::shared_ptr<Input<Renderer>>;
	template <typename Renderer> using TextPtr       = std::shared_ptr<Text<Renderer>>;
	template <typename Renderer> using CheckboxPtr   = std::shared_ptr<Checkbox<Renderer>>;
	template <typename Renderer> using RadioPtr      = std::shared_ptr<Radio<Renderer>>;
	template <typename Renderer> using RadioGroupPtr = std::shared_ptr<RadioGroup<Renderer>>;
	template <typename Renderer> using SelectionPtr  = std::shared_ptr<Selection<Renderer>>;
	template <typename Renderer> using SubmitPtr     = std::shared_ptr<Submit<Renderer>>;
	template <typename Renderer> using ResetPtr      = std::shared_ptr<Reset<Renderer>>;
	template <typename Renderer> using LinkPtr       = std::shared_ptr<Link<Renderer>>;

	struct BasicRenderer;

	template <typename Renderer = BasicRenderer>
	class Control: public ControlBase
	{
	protected:
		void getControlString(Request& request) override { }
		bool getLabelString(Request& request) override
		{
			if (!m_label.empty())
				Renderer::getLabelString(request, m_name, m_label);
			return !m_label.empty();
		}

		bool getHintString(Request& request) override
		{
			if (!m_hint.empty())
				Renderer::getHintString(request, m_hint);
			return !m_hint.empty();
		}

	public:
		Control(const std::string& name, const std::string& label, const std::string& hint)
			: ControlBase(name, label, hint)
		{
		}

		void render(Request& request) override
		{
			Renderer::render(request, this);
		}
		void renderSimple(Request& request) override
		{
			Renderer::renderSimple(request, this);
		}
	};

	template <typename Renderer = BasicRenderer>
	class Input: public Control<Renderer>
	{
	public:
		Input(const std::string& type, const std::string& name, const std::string& label, const std::string& hint)
			: Control<Renderer>(name, label, hint)
		{
			this->setAttr("type", type);
			Renderer::setPlaceholder(this, label);
		}

		void getControlString(Request& request)
		{
			Renderer::getControlString(request, this, "input", this->m_hasError);
		}

		void getSimpleControlString(Request& request)
		{
			if (this->getLabelString(request))
				request << ": ";
			this->getElement(request, "input");
		}
	};

	template <typename Renderer = BasicRenderer>
	class Text : public Input<Renderer>
	{
		bool m_pass;
	public:
		Text(const std::string& name, const std::string& label, bool pass, const std::string& hint)
			: Input<Renderer>(pass ? "password" : "text", name, label, hint)
			, m_pass(pass)
		{
		}

		void bindUI()
		{
			if (!m_pass)
				Input<Renderer>::bindUI();
		}
	};

	class CheckboxBase
	{
	protected:
		bool m_checked;
		void bindData(Request& request, const Strings& data, const std::string& name, std::string& value, bool& userValue);
		CheckboxBase() : m_checked(false) {}
	};

	template <typename Renderer = BasicRenderer>
	class Checkbox : public Input<Renderer>, protected CheckboxBase
	{
		bool m_checked;
	public:
		Checkbox(const std::string& name, const std::string& label, const std::string& hint)
			: Input<Renderer>("checkbox", name, label, hint)
		{
		}
		void getControlString(Request& request)
		{
			Renderer::checkboxControlString(request, this, this->m_hasError);
		}

		void bindData(Request& request, const Strings& data)
		{
			CheckboxBase::bindData(request, data, this->m_name, this->m_value, this->m_userValue);
		}

		void bindUI()
		{
			if (this->m_checked)
				this->setAttr("checked", "checked");
		}

		bool isChecked() const { return this->m_checked; }
	};

	template <typename Renderer = BasicRenderer>
	class Radio : public Input<Renderer>, protected CheckboxBase
	{
		bool m_checked;
	public:
		Radio(const std::string& group, const std::string& value, const std::string& label)
			: Input<Renderer>("radio", group, label, std::string())
		{
			this->setAttr("id", group + "-" + value);
			this->setAttr("value", value);
		}

		void getControlString(Request& request)
		{
			Renderer::checkboxControlString(request, this, this->m_hasError);
		}

		void bindData(Request& request, const Strings& data)
		{
			CheckboxBase::bindData(request, data, this->m_name, this->m_value, this->m_userValue);
		}

		void bindUI()
		{
			if (this->m_checked)
				this->setAttr("checked", "checked");
		}

		bool isChecked() const { return this->m_checked; }
		void check(bool val = true) { this->m_checked = val; }
	};

	template <typename Renderer = BasicRenderer>
	class RadioGroup : public Control<Renderer>
	{
		std::list<RadioPtr<Renderer>> m_buttons;
	public:
		RadioGroup(const std::string& name, const std::string& label)
			: Control<Renderer>(name, label, std::string())
		{
		}

		RadioPtr<Renderer> radio(const std::string& value, const std::string& label)
		{
			auto obj = std::make_shared<Radio<Renderer>>(this->m_name, value, label);
			if (obj)
				m_buttons.push_back(obj);
			return obj;
		}

		void getControlString(Request& request)
		{
			Renderer::radioGroupControlString(request, this, this->m_label, this->m_hasError);
			for (auto&& radio : m_buttons)
				radio->render(request);
		}

		void bindUI() override
		{
			if (this->m_value.empty())
				return;

			for (auto&& ctrl: m_buttons)
			{
				ctrl->check(ctrl->getAttr("value") == this->m_value);
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

	template <typename Renderer = BasicRenderer>
	class Selection : public Control<Renderer>
	{
		Options m_options;
	public:
		Selection(const std::string& name, const std::string& label, const Options& options, const std::string& hint)
			: Control<Renderer>(name, label, hint)
			, m_options(options)
		{
		}

		void getControlString(Request& request)
		{
			Renderer::selectionControlString(request, this, this->m_hasError, [this](Request& request){
				m_options.getControlString(request, this->m_value);
			});
		}

		Selection& option(const std::string& key, const std::string& value)
		{
			m_options.add(key, value);
			return *this;
		}
	};

	template <typename Renderer = BasicRenderer>
	class Submit : public Input<Renderer>
	{
	public:
		Submit(const std::string& name, const std::string& label, bool narrow, const std::string& hint)
			: Input<Renderer>("submit", name, std::string(), hint)
		{
			if (narrow)
				this->setAttr("class", "narrow");
			this->setAttr("value", label);
		}

		void bindUI() {}
	};

	template <typename Renderer = BasicRenderer>
	class Reset : public Input<Renderer>
	{
	public:
		Reset(const std::string& name, const std::string& label, const std::string& hint)
			: Input<Renderer>("reset", name, std::string(), hint)
		{
			this->setAttr("value", label);
		}

		void bindUI() {}
	};

	template <typename Renderer = BasicRenderer>
	class Link : public Control<Renderer>
	{
		std::string m_text;
	public:
		Link(const std::string& name, const std::string& link, const std::string& text)
			: Control<Renderer>(name, std::string(), std::string())
			, m_text(text)
		{
			this->setAttr("href", link);
		}
		virtual void getControlString(Request& request)
		{
			Renderer::getControlString(request, this, "a", this->m_hasError, this->m_text);
		}
		void bindUI() {}
	};
} // FastCGI

#endif // __FORMS_CONTROLS_H__
