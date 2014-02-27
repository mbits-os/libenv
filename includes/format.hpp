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

#ifndef __FORMAT_HPP__
#define __FORMAT_HPP__

#include <string>
#include <sstream>
#include <vector>
#include <cctype>

#ifdef __CYGWIN__
namespace std
{
	template <typename T>
	std::string to_string(const T& i);
}
#endif

namespace str
{
	namespace impl
	{
		template <typename T>
		using decay_t = typename std::decay<T>::type;

		inline void pack(std::vector<std::string>& packed) {}

		template <typename T>
		struct conv
		{
			static std::string to_string(const T& cref) { return std::to_string(cref); }
		};

		template <>
		struct conv<std::string>
		{
			static std::string to_string(const std::string& cref) { return cref; }
		};

		template <>
		struct conv<const char*>
		{
			static std::string to_string(const char* cref) { return cref; }
		};

		template <>
		struct conv<char*>
		{
			static std::string to_string(char* cref) { return cref; }
		};

		template <typename T, typename... Args>
		inline void pack(std::vector<std::string>& packed, T&& arg, Args&&... args)
		{
			packed.push_back(std::move(impl::conv<impl::decay_t<T>>::to_string(arg)));
			pack(packed, std::forward<Args>(args)...);
		}
	}

	template <typename... Args>
	inline std::string format(const char* fmt, Args&&... args)
	{
		std::vector<std::string> packed;
		packed.reserve(sizeof...(args));
		impl::pack(packed, std::forward<Args>(args)...);

		std::ostringstream o;

		const char* c = fmt;
		const char* e = c ? c + strlen(c) : c;
		while (c != e)
		{
			if (*c != '%')
			{
				o.put(*c);
			}
			else
			{
				++c; if (c == e) break;
				if (!std::isdigit((unsigned char)*c))
				{
					o.put(*c);
				}
				else
				{
					size_t ndx = 0;
					while (c != e && std::isdigit((unsigned char)*c))
					{
						ndx *= 10;
						ndx += *c - '0';
						++c;
					}

					if (ndx)
					{
						--ndx;
						if (packed.size() > ndx)
							o << packed[ndx];
					}

					continue;
				}
			}
			++c;
		}

		return o.str();
	}

	template <typename... Args>
	inline std::string format(const std::string& fmt, Args&&... args)
	{
		return format(fmt.c_str(), std::forward<Args>(args)...);
	}
}

#endif // __FORMAT_HPP__