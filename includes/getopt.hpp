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
	template <typename C>
	struct reader
	{
		using CSTR = const C*;
		using STR = C*;
		using string = std::basic_string<C, std::char_traits<C>, std::allocator<C>>;
		virtual void read(CSTR) {}
		virtual void read() {}
		virtual bool has_arg() const { return true; }

		reader() = default;
		reader(const reader&) = delete;
		reader(reader&&) = delete;
		reader& operator= (const reader&) = delete;
		reader& operator= (reader&&) = delete;
	};

	template <typename C>
	using string_t = typename reader<C>::string;

	template <typename C>
	using reader_ptr = std::shared_ptr<reader<C>>;

	template <typename C>
	using readers = std::map<string_t<C>, reader_ptr<C>>;

	namespace helpers
	{
		template <typename C>
		inline const C* _begin(const C* e)
		{
			return e;
		}

		template <typename C>
		inline const C* _end(const C* e)
		{
			while (*e) e++;
			return e;
		}

		template <typename C, typename _C>
		inline void copy_strtrans(std::basic_string<_C, std::char_traits<_C>, std::allocator<_C>>& out, const C* in)
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

	template <typename C, typename T>
	struct conv_impl
	{
		static inline T convert(typename reader<C>::CSTR arg)
		{
			T out{};
			std::basic_istringstream<C, std::char_traits<C>, std::allocator<C>> is(arg);
			is >> out;
			return out;
		}
	};

	template <typename C, typename _C>
	struct conv_impl<C, std::basic_string<_C, std::char_traits<_C>, std::allocator<_C>>>
	{
		static inline std::basic_string<_C, std::char_traits<_C>, std::allocator<_C>> convert(typename reader<C>::CSTR arg)
		{
			std::basic_string<_C, std::char_traits<_C>, std::allocator<_C>> out;
			helpers::strtrans(out, arg);
			return out;
		}
	};

	template <typename C, typename T>
	struct conv_impl_int
	{
		static inline T convert(typename reader<C>::CSTR arg)
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

	template <typename C>
	struct conv_impl<C, int>: conv_impl_int<C, int>{};
	template <typename C>
	struct conv_impl<C, unsigned short>: conv_impl_int<C, unsigned short>{};

	template <typename C, typename T>
	class arg_reader : public reader<C>
	{
		T& ref;
	public:
		arg_reader(T& ref) : ref(ref) {}

		void read(CSTR arg) override
		{
			ref = conv_impl<C, T>::convert(arg);
		}
	};

	template <typename C>
	class bool_reader : public reader<C>
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

	template <typename C>
	class options_base
	{
		using CSTR = typename reader<C>::CSTR;
		using STR = typename reader<C>::STR;
		using string = typename reader<C>::string;

		using free_args_t = std::vector<STR>;
		readers<C> m_readers;
		free_args_t m_free_args;
		CAUSE m_cause = OK;
		bool m_empty = true;
		string m_cause_arg;

		void add(const string& fmt, const reader_ptr<C>& reader)
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

		bool read_arg(STR arg, int& next, int argc, STR argv[])
		{
			if (!arg)
				return error(UNEXPECTED);
			if (arg[0] != '-') // also - empty strings
				return free_arg(arg);

			arg++;
			if (*arg == '-') // long opt
				return error(UNKNOWN, arg - 1); // not supported...

			while (*arg)
			{
				string name(1, *arg);
				auto it = m_readers.find(name);
				if (it == m_readers.end() || !it->second)
					return error(UNKNOWN, string(1, *arg));

				auto reader = it->second;
				if (reader->has_arg())
				{
					STR value = arg + 1;
					if (!*value)
					{
						if (next >= argc)
							return error(NO_ARGUMENT, string(1, *arg));
						value = argv[next++];
					}
					reader->read(value);
					return true;
				}
				else
					reader->read();

				++arg;
			}

			return true;
		}

	public:
		template <typename T>
		options_base<C>& add_arg(CSTR fmt, T& ref)
		{
			add(fmt, std::make_shared<arg_reader<C, T>>(std::ref(ref)));
			return *this;
		}

		options_base<C>& add(CSTR fmt, bool& ref)
		{
			add(fmt, std::make_shared<bool_reader<C>>(std::ref(ref)));
			return *this;
		}

		bool read(int argc, STR argv[])
		{
			int index = 1;
			while (index < argc)
			{
				m_empty = false;

				auto tmp = index++;
				if (!read_arg(argv[tmp], index, argc, argv))
					return false;
			}
			return true;
		}

		CAUSE cause() const { return m_cause; }
		const string& cause_arg() const { return m_cause_arg; }
		const free_args_t& free_args() const { return m_free_args; }
		bool empty() const { return m_empty; }
	};

	using options = options_base<char>;
	using woptions = options_base<wchar_t>;
}

#endif // __GETOPT_HPP__

