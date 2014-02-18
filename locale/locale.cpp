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

#include "pch.h"
#include <locale.hpp>
#include <utils.hpp>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef _WIN32
#define _stat stat
#endif

#ifdef _WIN32
#define SEP '\\'
#else
#define SEP '/'
#endif

namespace lng
{
	void Locale::init(const filesystem::path& fileRoot)
	{
		m_fileRoot = fileRoot;
	}

	TranslationPtr Locale::httpAcceptLanguage(const char* header)
	{
		std::list<std::string> langs = url::priorityList(header);
		for (auto& lang: langs)
		{
			std::transform(lang.begin(), lang.end(), lang.begin(), [](char c) { return c == '-' ? '_' : c; });

			Translations::iterator _it = m_translations.find(lang);
			if (_it != m_translations.end())
			{
				auto candidate = _it->second.lock();
				if (candidate)
					return candidate;
			}

			auto candidate = std::make_shared<Translation>();
			if (!candidate)
				return nullptr; //OOM, 500 the page

			if (candidate->open(m_fileRoot / lang / "site_strings.lng"))
			{
				m_translations[lang] = candidate;
				return candidate;
			};
		}

		//falback to en
		Translations::iterator _it = m_translations.find("en");
		if (_it != m_translations.end())
		{
			auto candidate = _it->second.lock();
			if (candidate)
				return candidate;
		}

		auto candidate = std::make_shared<Translation>();
		if (!candidate)
			return nullptr; //OOM, 500 the page

		if (candidate->open(m_fileRoot / "en/site_strings.lng"))
		{
			m_translations["en"] = candidate;
			return candidate;
		};

		return TranslationPtr();
	}

	filesystem::path Locale::getFilename(const char* header, const filesystem::path& filename)
	{
		struct _stat st;
		std::list<std::string> langs = url::priorityList(header);
		for (auto& lang : langs)
		{
			std::transform(lang.begin(), lang.end(), lang.begin(), [](char c) { return c == '-' ? '_' : c; });
			auto path = m_fileRoot / lang / filename;
			if (filesystem::exists(path))
				return path;
		}
		auto path = m_fileRoot / "en" / filename;
		if (filesystem::exists(path))
			return path;

		return filesystem::path();
	}
}