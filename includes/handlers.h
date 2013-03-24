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

#ifndef __SERVER_HANDLER_H__
#define __SERVER_HANDLER_H__

#include "fast_cgi.h"

namespace FastCGI { namespace app
{
	class Handler
	{
	public:
		virtual ~Handler() {}
		virtual bool allowsUploads() { return false; }
#if DEBUG_CGI
		virtual std::string name() const = 0;
#endif
		virtual void visit(Request& request) = 0;
	};
	typedef std::tr1::shared_ptr<Handler> HandlerPtr;

	class PageTranslation
	{
		lng::TranslationPtr m_translation;
		std::string m_badString;
	public:
		bool init(SessionPtr session, Request& request);
		const char* operator()(lng::LNG stringId);
	};

	class PageHandler: public Handler
	{
	protected:
		virtual bool restrictedPage() { return true; }
		virtual const char* getPageTitle(PageTranslation& tr) { return nullptr; }
		std::string getTitle(PageTranslation& tr)
		{
			std::string title = tr(lng::LNG_GLOBAL_SITENAME);
    		const char* page = getPageTitle(tr);
    		if (!page) return title;
			title += " &raquo; ";
			title += page;
			return title;
		} 
		//rendering the page
		virtual void prerender(SessionPtr session, Request& request, PageTranslation& tr) {}
		virtual void header(SessionPtr session, Request& request, PageTranslation& tr);
		virtual void render(SessionPtr session, Request& request, PageTranslation& tr) = 0;
		virtual void footer(SessionPtr session, Request& request, PageTranslation& tr);
		virtual void postrender(SessionPtr session, Request& request, PageTranslation& tr) {}
	public:
		void visit(Request& request);
	};

	class RedirectUrlHandler: public Handler
	{
	protected:
		std::string m_url;
	public:
		RedirectUrlHandler(const std::string& url)
			: m_url(url)
		{
		}

#if DEBUG_CGI
		std::string name() const
		{
			return "Location: <a href='" + m_url + "'>" + m_url + "</a>";
		}
#endif

		void visit(Request& request) { request.redirectUrl(m_url); }
	};

	class RedirectHandler: public Handler
	{
		std::string m_service_url;
	public:
		RedirectHandler(const std::string& service_url)
			: m_service_url(service_url)
		{
		}

#if DEBUG_CGI
		std::string name() const
		{
			return "Location: <a href='" + m_service_url + "'>" + m_service_url + "</a>";
		}
#endif

		void visit(Request& request) { request.redirect(m_service_url); }
	};

#if DEBUG_CGI
	struct HandlerDbgInfo
	{
		HandlerPtr ptr;
		const char* file;
		size_t line;
	};

	typedef std::map<std::string, HandlerDbgInfo> HandlerMap;
#else
	typedef std::map<std::string, HandlerPtr> HandlerMap;
#endif
	class Handlers
	{
		static Handlers& get()
		{
			static Handlers instance;
			return instance;
		}

		HandlerMap m_handlers;
		HandlerPtr _handler(Request& request);
		void _register(const std::string& resource, const HandlerPtr& ptr
#if DEBUG_CGI
			, const char* file, size_t line
#endif
			)
		{
#if DEBUG_CGI
			HandlerDbgInfo info;
			info.ptr = ptr;
			info.file = file;
			info.line = line;
			m_handlers[resource] = info;
#else
			m_handlers[resource] = ptr;
#endif
		}
	public:
		static HandlerPtr handler(Request& request)
		{
			return get()._handler(request);
		}

		static void registerRaw(const std::string& resource, Handler* rawPtr
#if DEBUG_CGI
			, const char* file, size_t line
#endif
			)
		{
			if (!rawPtr)
				return;

			HandlerPtr ptr(rawPtr);
			get()._register(resource, ptr
#if DEBUG_CGI
				, file, line
#endif
				);
		}

      	static void redirect(const std::string& resource, const std::string& uri
#if DEBUG_CGI
			, const char* file, size_t line
#endif
			)
		{
			registerRaw(resource, new (std::nothrow) RedirectHandler(uri)
#if DEBUG_CGI
				, file, line
#endif
				);
		}

#if DEBUG_CGI
		static HandlerMap::const_iterator begin() { return get().m_handlers.begin(); }
		static HandlerMap::const_iterator end() { return get().m_handlers.end(); }
#endif

	};

	template <typename PageImpl>
	struct HandlerRegistrar
	{
		HandlerRegistrar(const std::string& resource
#if DEBUG_CGI
			, const char* file, size_t line
#endif
			)
		{
			Handlers::registerRaw(resource, new (std::nothrow) PageImpl()
#if DEBUG_CGI
				, file, line
#endif
				);
		}
	};

	struct RedirectRegistrar
	{
		RedirectRegistrar(const std::string& resource, const std::string& uri
#if DEBUG_CGI
			, const char* file, size_t line
#endif
			)
		{
			Handlers::redirect(resource, uri
#if DEBUG_CGI
				, file, line
#endif
				);
		}
	};
}} //FastCGI::app

#if DEBUG_CGI
#define REGISTRAR_ARGS , __FILE__, __LINE__
#else
#define REGISTRAR_ARGS
#endif

#define REGISTRAR_NAME_2(name, line) name ## _ ## line
#define REGISTRAR_NAME_1(name, line) REGISTRAR_NAME_2(name, line)
#define REGISTRAR_NAME(name) REGISTRAR_NAME_1(name, __LINE__)
#define REGISTER_HANDLER(resource, type) static FastCGI::app::HandlerRegistrar<type> REGISTRAR_NAME(handler) (resource REGISTRAR_ARGS)
#define REGISTER_REDIRECT(resource, uri) static FastCGI::app::RedirectRegistrar REGISTRAR_NAME(redirect) (resource, uri REGISTRAR_ARGS)

#endif