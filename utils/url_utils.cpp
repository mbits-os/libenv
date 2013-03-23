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

#include "pch.h"
#include <utils.h>
#include <vector>
#include <iterator>

namespace url
{
	inline bool isToken(char c)
	{
		// as per RFC2616
		switch(c)
		{
		case  0: case  1: case  2: case  3: case  4:
		case  5: case  6: case  7: case  8: case  9:
		case 10: case 11: case 12: case 13: case 14:
		case 15: case 16: case 17: case 18: case 19:
		case 20: case 21: case 22: case 23: case 24:
		case 25: case 26: case 27: case 28: case 29:
		case 30: case 31: case 32: case 127:
		case '(': case ')': case '<': case '>': case '@':
		case ',': case ';': case ':': case '\\': case '"':
		case '/': case '[': case ']': case '?': case '=':
		case '{': case '}':
			return false;
		}
		return true;
	}

	bool isToken(const char* in, size_t in_len)
	{
		for (size_t i = 0; i < in_len; ++i)
		{
			if (isToken(in[i]))
				return false;
		}
		return true;
	}

	std::string encode(const char* in, size_t in_len)
	{
		static char hexes[] = "0123456789ABCDEF";
		std::string out;
		out.reserve(in_len * 11 / 10);

		for (size_t i = 0; i < in_len; ++i)
		{
			unsigned char c = in[i];
			if (isalnum(c) || c == '-' || c == '-' || c == '.' || c == '_' || c == '~')
			{
				out += c;
				continue;
			}
			out += '%';
			out += hexes[(c >> 4) & 0xF];
			out += hexes[(c     ) & 0xF];
		}
		return out;
	}

	static inline char hex(char c)
	{
		switch(c)
		{
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			return c - '0';
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
			return c - 'a' + 10;
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
			return c - 'A' + 10;
		}
		return 0;
	}
	std::string decode(const char* in, size_t in_len)
	{
		std::string out;
		out.reserve(in_len);

		for (size_t i = 0; i < in_len; ++i)
		{
			// go inside only, if there is enough space
			if (in[i] == '%' && (i < in_len - 3) &&
				isxdigit(in[i+1]) && isxdigit(in[i+2]))
			{
				unsigned char c = (hex(in[i+1]) << 4) | hex(in[i+2]);
				out += c;
				continue;
			}
			out += in[i];
		}
		return out;
	}

	std::string quot_escape(const char* in, size_t in_len)
	{
		std::string out;
		out.reserve(in_len * 3 / 2);

		for (size_t i = 0; i < in_len; ++i)
		{
			switch (in[i])
			{
			case '\\': out += "\\\\"; break;
			case '\a': out += "\\a"; break;
			case '\b': out += "\\b"; break;
			case '\r': out += "\\r"; break;
			case '\n': out += "\\n"; break;
			case '\t': out += "\\t"; break;
			case '"':  out += "\\\""; break;
			case '\'': out += "\\'"; break;
			default:
				out.push_back(in[i]);
			}
		}

		return out;
	}

	std::string quot_parse(const char* in, size_t in_len, const char** closing_quot_p)
	{
		std::string out;
		out.reserve(in_len);

		for (size_t i = 0; i < in_len; ++i)
		{
			if (in[i] == '"')
			{
				if (closing_quot_p)
					*closing_quot_p = in + i;
				break;
			}

			if (in[i] == '\\')
			{
				++i;

				if (i < in_len)
					switch(in[i])
					{
					case '\\': out += "\\"; break;
					case 'a':  out += "\a"; break;
					case 'b':  out += "\b"; break;
					case 'r':  out += "\r"; break;
					case 'n':  out += "\n"; break;
					case 't':  out += "\t"; break;
					case '"':  out += "\""; break;
					case '\'': out += "'"; break;
					};

				continue;
			}
			else out += in[i];
		}

		return out;
	}

	struct ListItem
	{
		std::string m_value;
		size_t m_pos;
		size_t m_q;

		// SORT q DESC, pos ASC
		bool operator < (const ListItem& right) const
		{
			if (m_q != right.m_q) return m_q > right.m_q;
			return m_pos < right.m_pos;
		}

#define WS do { while (isspace(*c) && c < end) c++; } while(0)
#define LOOK_FOR(ch) do { while (!isspace(*c) && *c != ',' && *c != (ch) && c < end) c++; } while(0)

		const char* read(size_t pos, const char* c, const char* end)
		{
			m_pos = pos;
			m_q = 1000;

			WS;
			const char* token = c;
			LOOK_FOR(';');
			m_value.assign(token, c);
			WS;
			while (*c == ';')
			{
				c++;
				WS;
				token = c;
				LOOK_FOR('=');
				if (c - token == 1 && *token == 'q')
				{
					WS;
					if (*c == '=')
					{
						c++;
						WS;
						token = c;
						LOOK_FOR(';');
						m_q = quality(token, c);
						WS;
					}
				}
				else LOOK_FOR(';');
			}
			return ++c;
		}
		static size_t quality(const char* c, const char* end)
		{
			size_t q = 0;
			size_t pow = 1000;
			bool seen_dot = false;
			while (c != end)
			{
				switch(*c)
				{
				case '0': case '1': case '2': case '3': case '4':
				case '5': case '6': case '7': case '8': case '9':
					q *= 10;
					q += *c - '0';
					if (seen_dot)
					{
						pow /= 10;
						if (pow == 1)
							return q;
					}
					break;
				case '.':
					seen_dot = true;
					break;
				default:
					return q * pow;
				};
				++c;
			}
			return q * pow;
		}
	};

	std::list<std::string> priorityList(const char* header)
	{
		std::vector<ListItem> items;

		const char* c = header;
		const char* end = c + strlen(c);
		size_t pos = 0;
		while (c < end)
		{
			ListItem it;
			c = it.read(pos++, c, end);
			items.push_back(it);
		}

		std::stable_sort(items.begin(), items.end());
		std::list<std::string> out;
		std::transform(items.begin(), items.end(), std::back_inserter(out), [](const ListItem& item) { return item.m_value; });
		return out;
	}
}
