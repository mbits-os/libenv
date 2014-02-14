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

#ifndef __GETOPT_HPP__
#define __GETOPT_HPP__

#include <map>
#include <vector>
#include <string>
#include <memory>
#include <sstream>

namespace getopt
{
	template <typename Char>
	struct reader
	{
		using CSTR = const Char*;
		using STR = Char*;
		using string = std::basic_string<Char, std::char_traits<Char>, std::allocator<Char>>;
		virtual void read(CSTR) {}
		virtual void read() {}
		virtual bool has_arg() const { return true; }

		reader() = default;
		reader(const reader&) = delete;
		reader(reader&&) = delete;
		reader& operator= (const reader&) = delete;
		reader& operator= (reader&&) = delete;
	};

	template <typename Char>
	using string_t = typename reader<Char>::string;

	template <typename Char>
	using reader_ptr = std::shared_ptr<reader<Char>>;

	template <typename Char>
	using readers = std::map<string_t<Char>, reader_ptr<Char>>;

	namespace helpers
	{
		template <typename Char>
		inline const Char* _begin(const Char* e)
		{
			return e;
		}

		template <typename Char>
		inline const Char* _end(const Char* e)
		{
			while (*e) e++;
			return e;
		}

		template <typename Char, typename CharOut>
		inline void copy_strtrans(std::basic_string<CharOut, std::char_traits<CharOut>, std::allocator<CharOut>>& out, const Char* in)
		{
			if (!in)
			{
				out.clear();
				return;
			}
			out.assign(_begin(in), _end(in));
		}

		inline void strtrans(std::string& out, const char* in) { out = in; }
		inline void strtrans(std::wstring& out, const wchar_t* in) { out = in; }
		inline void strtrans(std::string& out, const wchar_t* in) { copy_strtrans(out, in); }
		inline void strtrans(std::wstring& out, const char* in) { copy_strtrans(out, in); }
	}

	template <typename Char, typename T>
	struct conv_impl
	{
		static inline T convert(typename reader<Char>::CSTR arg)
		{
			T out{};
			std::basic_istringstream<Char, std::char_traits<Char>, std::allocator<Char>> is(arg);
			is >> out;
			return out;
		}
	};

	template <typename Char, typename CharOut>
	struct conv_impl<Char, std::basic_string<CharOut, std::char_traits<CharOut>, std::allocator<CharOut>>>
	{
		static inline std::basic_string<CharOut, std::char_traits<CharOut>, std::allocator<CharOut>> convert(typename reader<Char>::CSTR arg)
		{
			std::basic_string<CharOut, std::char_traits<CharOut>, std::allocator<CharOut>> out;
			helpers::strtrans(out, arg);
			return out;
		}
	};

	template <typename Char, typename T>
	struct conv_impl_int
	{
		static inline T convert(typename reader<Char>::CSTR arg)
		{
			if (!arg)
				return 0;

			T out = 0;
			T one = 1;

			if (*arg == '-')
			{
				one = (T)-1;
				arg++;
			}

			if (*arg == '+')
				arg++;

			while (*arg)
			{
				switch (*arg)
				{
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					out *= 10;
					out += *arg - '0';
					break;

				default:
					return out * one;
				}
				++arg;
			}

			return out * one;
		}
	};

	template <typename Char>
	struct conv_impl<Char, int>: conv_impl_int<Char, int>{};
	template <typename Char>
	struct conv_impl<Char, unsigned short>: conv_impl_int<Char, unsigned short>{};

	template <typename Char, typename T>
	class arg_reader : public reader<Char>
	{
		T& ref;
	public:
		arg_reader(T& ref) : ref(ref) {}

		void read(typename reader<Char>::CSTR arg) override
		{
			ref = conv_impl<Char, T>::convert(arg);
		}
	};

	template <typename Char>
	class bool_reader : public reader<Char>
	{
		bool& ref;
	public:
		bool_reader(bool& ref) : ref(ref) {}

		bool has_arg() const { return false; }
		void read() override
		{
			ref = true;
		}
	};

	enum CAUSE
	{
		OK,
		UNEXPECTED,
		UNKNOWN,
		NO_ARGUMENT,
		MISSING
	};

	template <typename Char>
	class options_base
	{
		using CSTR = typename reader<Char>::CSTR;
		using STR = typename reader<Char>::STR;
		using string = typename reader<Char>::string;

		using free_args_t = std::vector<STR>;
		readers<Char> m_readers;
		free_args_t m_free_args;
		CAUSE m_cause = OK;
		bool m_empty = true;
		string m_cause_arg;

		void add(const string& fmt, const reader_ptr<Char>& reader)
		{
			m_readers[fmt] = std::move(reader);
		}

		bool error(CAUSE cause, const string& arg = string())
		{
			m_cause = cause;
			m_cause_arg = arg;
			return false;
		}

		bool free_arg(STR arg)
		{
			m_free_args.push_back(arg);
			return true;
		}

		class action
		{
			int _argc;
			STR* _argv;

		protected:
			options_base<Char>& parent;

		public:
			action(options_base<Char>& parent, int argc, STR argv[]) : _argc(argc), _argv(argv), parent(parent) {}

			int argc() const { return _argc; }
			STR argv(int id) const { return _argv[id]; }

			virtual bool error(CAUSE cause, const string& arg = string())
			{
				return parent.error(cause, arg);
			}

			virtual bool free_arg(STR arg) = 0;
			virtual void value(const string& name, STR value, reader_ptr<Char>& reader) = 0;
			virtual void non_value(const string& name, reader_ptr<Char>& reader) = 0;
		};

		class action_read : public action
		{
		public:
			using action::action;

			bool free_arg(STR arg) override
			{
				action::parent.m_free_args.push_back(arg);
				return true;
			}
			void value(const string& name, STR value, reader_ptr<Char>& reader) override { reader->read(value); }
			void non_value(const string& name, reader_ptr<Char>& reader) override { reader->read(); }

			bool act() { return all(*this); }
		};

		class action_sieve : public action
		{
			std::vector<string>& args;
			CSTR ignorable;

			static bool in(Char c, CSTR ignorable)
			{
				for (; *ignorable; ++ignorable)
				{
					if (*ignorable == c)
						return true;
				}

				return false;
			}

		public:
			action_sieve(options_base<Char>& parent, int argc, STR argv[], std::vector<string>& args, CSTR ignorable) : action(parent, argc, argv), args(args), ignorable(ignorable) {}

			bool free_arg(STR arg) override
			{
				args.push_back(arg);
				return true;
			}
			void value(const string& name, STR value, reader_ptr<Char>& reader) override
			{
				if (!in(name[0], ignorable))
				{
					Char minus[2] = { '-' };
					args.push_back(minus + name);
					args.push_back(value);
				}
			}
			void non_value(const string& name, reader_ptr<Char>& reader) override
			{
				if (!in(name[0], ignorable))
				{
					Char minus[2] = { '-' };
					args.push_back(minus + name);
				}
			}
		};

		bool next(STR arg, int& next, action& act)
		{
			if (!arg)
				return act.error(UNEXPECTED);
			if (arg[0] != '-') // also - empty strings
				return act.free_arg(arg);

			arg++;
			if (*arg == '-') // long opt
				return act.error(UNKNOWN, arg - 1); // not supported...

			while (*arg)
			{
				string name(1, *arg);
				auto it = m_readers.find(name);
				if (it == m_readers.end() || !it->second)
					return act.error(UNKNOWN, name);

				auto reader = it->second;
				if (reader->has_arg())
				{
					STR value = arg + 1;
					if (!*value)
					{
						if (next >= act.argc())
							return act.error(NO_ARGUMENT, name);
						value = act.argv(next++);
					}
					act.value(name, value, reader);
					return true;
				}
				else
					act.non_value(name, reader);

				++arg;
			}

			return true;
		}

		bool all(action& act)
		{
			int index = 1;
			while (index < act.argc())
			{
				m_empty = false;

				auto tmp = index++;
				if (!next(act.argv(tmp), index, act))
					return false;
			}
			return true;

		}

		template <typename T>
		options_base<Char>& add_arg(CSTR fmt, T& ref)
		{
			add(fmt, std::make_shared<arg_reader<Char, T>>(std::ref(ref)));
			return *this;
		}

		options_base<Char>& add(CSTR fmt, bool& ref)
		{
			add(fmt, std::make_shared<bool_reader<Char>>(std::ref(ref)));
			return *this;
		}

		CAUSE cause() const { return m_cause; }
		const string& cause_arg() const { return m_cause_arg; }
		const free_args_t& free_args() const { return m_free_args; }
		bool empty() const { return m_empty; }

	public:
		template <typename Action>
		class active_options
		{
			Action m_action;
			options_base m_options;
		public:
			template <typename... Args>
			active_options(Args&&... args) : m_action(m_options, std::forward<Args>(args)...) {}
			template <typename T>
			active_options<Action>& add_arg(CSTR fmt, T& ref)
			{
				m_options.add_arg(fmt, std::forward<T&>(ref));
				return *this;
			}

			active_options<Action>& add(CSTR fmt, bool& ref)
			{
				m_options.add(fmt, std::forward<bool&>(ref));
				return *this;
			}

			bool act()
			{
				return m_options.all(m_action);
			}

			CAUSE cause() const { return m_options.cause(); }
			const string& cause_arg() const { return m_options.cause_arg(); }
			const free_args_t& free_args() const { return m_options.free_args(); }
			bool empty() const { return m_options.empty(); }
		};

		template <typename Action, typename... Args>
		static active_options<Action> make_active_options(Args&&... args) { return active_options<Action>(std::forward<Args>(args)...); }
		static active_options<action_read> read(int argc, STR argv[])
		{
			return make_active_options<action_read>(argc, argv);
		}

		static active_options<action_sieve> sieve(int argc, STR argv[], std::vector<string>& args, CSTR ignorable)
		{
			return make_active_options<action_sieve>(argc, argv, args, ignorable);
		}
	};

	using options = options_base<char>;
	using woptions = options_base<wchar_t>;
}

#endif // __GETOPT_HPP__

