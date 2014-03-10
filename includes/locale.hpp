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

#ifndef __LOCALE_H__
#define __LOCALE_H__

#include <filesystem.hpp>
#include <vector>

namespace lng
{
	using lng_t = unsigned int;

	struct LangFile;
	class Translation;
	class Locale;

	enum ERR
	{
		ERR_OK,
		ERR_NO_FILE,
		ERR_OOM,
		ERR_WRONG_HEADER,
		ERR_OFFSETS_TRUNCATED,
		ERR_UNEXPECTED_OFFSET,
		ERR_STRING_TRUNCATED,
		ERR_STRING_UNTERMINATED,
		ERR_OTHER
	};
	struct LangFile
	{
		typedef unsigned int offset_t;
		LangFile();
		~LangFile() { close(); }
		ERR open(const filesystem::path& path);
		void close();
		const char* getString(lng_t i);
		offset_t size() const { return m_count; }
	private:
		char* m_content;
		offset_t m_count;
		offset_t* m_offsets;
		char* m_strings;
	};

	class Translation
	{
		LangFile m_file;
		filesystem::path m_path;
		time_t m_mtime = 0;

		time_t mtime() const { return filesystem::status(m_path).mtime(); }
	public:
		bool open(const filesystem::path& path)
		{
			m_path = path;
			m_mtime = mtime();

			return m_file.open(path) == ERR_OK;
		}
		const char* tr(lng_t stringId) { return m_file.getString(stringId); }
		bool fresh() const
		{
			return mtime() == m_mtime;
		}
	};

	typedef std::shared_ptr<Translation> TranslationPtr;
	typedef std::weak_ptr<Translation> TranslationWeakPtr;

	struct LocaleInfo
	{
		std::string m_lang;
		std::string m_name;
	};

	using LocaleInfos = std::vector<LocaleInfo>;

	class Locale
	{
		typedef std::map<std::string, TranslationWeakPtr> Translations;
		filesystem::path m_fileRoot;
		Translations m_translations;
	public:
		void init(const filesystem::path& fileRoot);
		void reload(const filesystem::path& fileRoot);
		TranslationPtr getTranslation(const std::string& language_range);
		TranslationPtr httpAcceptLanguage(const char* header);
		filesystem::path getFilename(const char* header, const filesystem::path& filename);
		LocaleInfos knownLanguages();
	};

	// special "magic" values for the lang id enum
	static inline lng_t Culture() { return 0; }
	static inline lng_t LanguageName() { return 1; }
}

#endif // __LOCALE_H__