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
#include <locale.hpp>
#include <utils.h>
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
	void Locale::init(const char* fileRoot)
	{
		m_fileRoot = fileRoot;
		if (!m_fileRoot.empty() && m_fileRoot[m_fileRoot.length() - 1] != SEP)
			m_fileRoot.push_back(SEP);
	}

	TranslationPtr Locale::httpAcceptLanguage(const char* header)
	{
		TranslationPtr candidate(new (std::nothrow) Translation());
		if (!candidate.get())
			return candidate; //OOM, 500 the page

		std::list<std::string> langs = url::priorityList(header);
		auto _lang = langs.begin(), _end = langs.end();
		for (; _lang != _end; ++_lang)
		{
			std::string& lang = *_lang;
			std::transform(lang.begin(), lang.end(), lang.begin(), [](char c) { return c == '-' ? '_' : c; });

			Translations::iterator _it = m_translations.find(lang);
			if (_it != m_translations.end() && !_it->second.expired())
				return _it->second.lock();

			std::string path = m_fileRoot + lang + SEP + "site_strings.lng";
			if (candidate->open(path.c_str()))
			{
				m_translations[lang] = candidate;
				return candidate;
			};
		}

		//falback to en
		Translations::iterator _it = m_translations.find("en");
		if (_it != m_translations.end() && !_it->second.expired())
			return _it->second.lock();

		std::string path = m_fileRoot + "en" + SEP + "site_strings.lng";
		if (candidate->open(path.c_str()))
		{
			m_translations["en"] = candidate;
			return candidate;
		};

		return TranslationPtr();
	}

	std::string Locale::getFilename(const char* header, const char* filename)
	{
		struct _stat st;
		std::list<std::string> langs = url::priorityList(header);
		auto _lang = langs.begin(), _end = langs.end();
		for (; _lang != _end; ++_lang)
		{
			std::string& lang = *_lang;
			std::transform(lang.begin(), lang.end(), lang.begin(), [](char c) { return c == '-' ? '_' : c; });
			std::string path = m_fileRoot + lang + SEP + filename;
			if (!_stat(path.c_str(), &st))
				return path;
		}
		std::string path = m_fileRoot + "en" + SEP + filename;
		if (!_stat(path.c_str(), &st))
			return path;

		return std::string();
	}
}