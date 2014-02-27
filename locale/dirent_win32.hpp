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

#ifndef __DIRENT_WIN32_HPP__
#define __DIRENT_WIN32_HPP__

#include <errno.h>
#include <io.h> /* _findfirst and _findnext set errno iff they return -1 */
#include <stdlib.h>
#include <string.h>
#include <string>

class DIR;

struct dirent
{
	char *d_name;
};

inline DIR           *opendir(const char *);
inline int           closedir(DIR *);
inline struct dirent *readdir(DIR *);
inline void          rewinddir(DIR *);

class DIR
{
	intptr_t    handle = -1;
	_finddata_t info;
	dirent      result;
	std::string name;
public:
	DIR(const std::string& _name) : name(_name)
	{
		const char *all = /* search pattern must end with suitable wildcard */
			strchr("/\\", name[name.length() - 1]) ? "*" : "/*";
		name += all;
	}

	bool findfirst()
	{
		handle = _findfirst(name.c_str(), &info);
		result.d_name = nullptr;
		return handle != -1;
	}

	int findclose()
	{
		if (handle == -1)
			return -1;
		return _findclose(handle);
	}

	dirent* readdir()
	{
		if (handle != -1)
		{
			if (!result.d_name || _findnext(handle, &info) != -1)
			{
				result.d_name = info.name;
				return &result;
			}
		}

		return nullptr;
	}
};

DIR *opendir(const char *name)
{
	DIR *dir = 0;

	if (name && name[0])
	{
		dir = new DIR{name};
		if (!dir->findfirst())
		{
			delete dir;
			dir = nullptr;
		}
	}
	else
	{
		errno = EINVAL;
	}

	return dir;
}

int closedir(DIR *dir)
{
	int result = -1;

	if (dir)
	{
		result = dir->findclose();
		delete dir;
	}

	if (result == -1) /* map all errors to EBADF */
	{
		errno = EBADF;
	}

	return result;
}

dirent *readdir(DIR *dir)
{
	dirent *result = nullptr;

	if (dir)
	{
		result = dir->readdir();
	}
	else
	{
		errno = EBADF;
	}

	return result;
}

#endif // __DIRENT_WIN32_HPP__
