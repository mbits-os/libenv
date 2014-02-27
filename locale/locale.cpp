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
#include <fast_cgi/application.hpp>
#include <iterator>

#ifdef _WIN32
#include "dirent_win32.hpp"
#else
#include <dirent.h>
#endif

#ifndef _WIN32
#define _stat stat
#endif

#ifdef _WIN32
#define SEP '\\'
#else
#define SEP '/'
#endif

class dir_handle
{
	DIR* dir = nullptr;
public:
	~dir_handle() { close(); }
	void close()
	{
		if (!dir)
			return;

		closedir(dir);
		dir = nullptr;
	}

	bool open(const filesystem::path& path)
	{
		close();
		dir = opendir(path.native().c_str());
		return dir != nullptr;
	}

	dirent* read() { return dir ? readdir(dir) : nullptr; }
};

namespace lng
{
	void Locale::init(const filesystem::path& fileRoot)
	{
		m_fileRoot = fileRoot;
	}

	inline bool inside(const std::list<std::string>& langs, const std::string& key)
	{
		for (auto&& lang : langs)
		{
			if (lang == key)
				return true;
		}
		return false;
	}

	bool expandList(std::list<std::string>& langs)
	{
		for (auto&& lang : langs)
		{
			auto pos = lang.find_last_of('-');
			if (pos == std::string::npos)
				continue; // no tags in the language range

			auto sub = lang.substr(0, pos);
			if (inside(langs, sub))
				continue; // sub-range already in list

			auto sub_len = sub.length();
			auto insert = langs.end();
			auto cur = langs.begin(), end = langs.end();
			for (; cur != end; ++cur)
			{
				if (
					cur->compare(0, sub_len, sub) == 0 &&
					cur->length() > sub_len &&
					cur->at(sub_len) == '-'
					)
				{
					insert = cur;
				}
			}

			if (insert != langs.end())
				++insert;

			langs.insert(insert, sub);
			return true;
		}
		return false;
	}

	TranslationPtr Locale::httpAcceptLanguage(const char* header)
	{
		std::list<std::string> langs = url::priorityList(header);

		while (expandList(langs));

		for (auto& lang : langs)
		{
			auto candidate = getTranslation(lang);
			if (candidate)
				return candidate;
		}

		//falback to en
		return getTranslation("en");
	}

	TranslationPtr Locale::getTranslation(const std::string& language_range)
	{
		auto lang = language_range;
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

		return nullptr;
	}

	filesystem::path Locale::getFilename(const char* header, const filesystem::path& filename)
	{
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

	LocaleInfos Locale::knownLanguages()
	{
		dir_handle handle;
		if (!handle.open(m_fileRoot))
			return LocaleInfos();

		LocaleInfos out;
		dirent* entry = nullptr;
		while ((entry = handle.read()) != nullptr)
		{
			char * lang = entry->d_name;
			if (lang && *lang == '.') // ., .., and "hidden" files
				continue;

			filesystem::path candidate = m_fileRoot / lang;
			if (!filesystem::is_directory(candidate))
				continue;

			TranslationPtr translation;
			//std::transform(lang.begin(), lang.end(), lang.begin(), [](char c) { return c == '-' ? '_' : c; });

			Translations::iterator _it = m_translations.find(lang);
			if (_it != m_translations.end())
				translation = _it->second.lock();

			if (!translation)
			{
				translation = std::make_shared<Translation>();
				if (!translation->open(candidate / "site_strings.lng"))
					continue;
			}

			LocaleInfo nfo = { translation->tr(lng::CULTURE), translation->tr(lng::LANGUAGE_NAME) };
			out.emplace_back(nfo);
		}

		return out;
	}
}