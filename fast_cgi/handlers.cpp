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
#include <handlers.hpp>
#include <string.h>

namespace FastCGI { namespace app {

	HandlerPtr Handlers::_handler(Request& request)
	{
		param_t REQUEST_URI = request.getParam("REQUEST_URI");
		if (REQUEST_URI == nullptr) return nullptr;
		param_t query = strchr(REQUEST_URI, '?');

		HandlerMap::iterator _it = m_handlers.find(query ? std::string(REQUEST_URI, query) : REQUEST_URI);
		if (_it == m_handlers.end()) return nullptr;
#if DEBUG_CGI
		return _it->second.ptr;
#else
		return _it->second;
#endif
	}

	void PageHandler::visit(Request& request)
	{
		SessionPtr session = request.getSession(restrictedPage());
		PageTranslation tr;
		if (!tr.init(session, request))
			request.on500("No translation found for the PageHandler");

		bool fromXHR = request.getParam(HTTP_X_AJAX_FRAGMENT);

		prerender(session, request, tr);
#if DEBUG_CGI
		if (fromXHR)
		{
			auto icicle = request.getIcicle();
			if (!icicle.empty())
				request.setHeader("X-Debug-Icicle", icicle);
		}
#endif
		if (!fromXHR)
			header(session, request, tr);
		render(session, request, tr);
		if (!fromXHR)
			footer(session, request, tr);
		postrender(session, request, tr);
	}

	const char* getDTD() { return ""; }

	void PageHandler::header(const SessionPtr& session, Request& request, PageTranslation& tr)
	{
		TopMenu::TopBar menu("topbar", "home", "menu");
		buildTopMenu(menu, session, request, tr);

		request << "<!DOCTYPE html "
			"PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" "
			"\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\r\n"
			"<html>\r\n"
			"  <head>\r\n";
		headElement(session, request, tr);
		menu.echoCSS(request, true);
		request <<
			"  </head>\r\n"
			"  <body>\r\n";
		menu.echoMarkup(request);
		bodyStart(session, request, tr);
	}

	void PageHandler::buildTopMenu(TopMenu::TopBar& menu, const SessionPtr& session, Request& request, PageTranslation& tr)
	{
		std::string display_name;
		std::string icon;
#if DEBUG_CGI
		std::string icicle = request.getIcicle();
		if (!icicle.empty())
		{
			menu.right()
				.item("frozen", -1, "[F]")
				.setItemUrl("frozen", "/debug/?frozen=" + url::encode(icicle));
		}
#endif

		auto user = std::make_shared<TopMenu::UserMenu>("user", "user-menu", 4, std::string(), display_name);
		menu.left().home("home", 0, tr(lng::LNG_GLOBAL_PRODUCT), tr(lng::LNG_GLOBAL_DESCRIPTION));
		menu.right()
			.item("search", 7, std::string(), tr(lng::LNG_NAV_SEARCH))
			.item("new", 1, tr(lng::LNG_NAV_NEW_MENU), tr(lng::LNG_NAV_NEW_MENU_TIP))
			.item("refesh", 3, std::string(), tr(lng::LNG_NAV_REFRESH))
			.add_raw(user);
		user->item("badge", -1, "Here be badger")
			.separator()
			.item("settings", -1, tr(lng::LNG_SETTINGS_TITLE))
			.item("profile", -1, tr(lng::LNG_SETTINGS_PROFILE))
			.item("help", -1, tr(lng::LNG_NAV_HELP))
			.item("logout", -1, tr(lng::LNG_NAV_SIGNOUT));
		menu.right()
			.setItemUrl("search", "javascript:search();")
			.setItemUrl("refesh", "javascript:reload();");
		user->setItemUrl("settings", "/settings/general")
			.setItemUrl("profile", "/settings/profile")
			.setItemUrl("help", "/help/")
			.setItemUrl("logout", "/auth/logout");
	}

	void PageHandler::headElement(const SessionPtr& session, Request& request, PageTranslation& tr)
	{
		request <<
			"    <title>" << getTitle(request, tr) << "</title>\r\n"
			"    <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"/>\r\n"
			"    <style type=\"text/css\">@import url(\"" << static_web << "css/site.css\");</style>\r\n"
			"    <style type=\"text/css\">@import url(\"" << static_web << "css/topbar.css\");</style>\r\n"
			"    <script type=\"text/javascript\" src=\"" << static_web << "code/jquery-1.9.1.js\"></script>\r\n"
			"    <script type=\"text/javascript\" src=\"" << static_web << "code/topbar.js\"></script>\r\n"
			//"    <style type=\"text/css\">@import url(\"" << static_web << "css/topbar_icons.css\");</style>\r\n"
			;
	}

	void PageHandler::bodyStart(const SessionPtr& session, Request& request, PageTranslation& tr)
	{
		request <<
			/*"  <div id=\"wrapper\" class=\"hfeed\">\r\n"
			"    <div id=\"header\">\r\n"
			"        <div id=\"masthead\">\r\n"
			"            <div id=\"branding\">\r\n"
			"                <h1 id=\"site-title\"><span><a href=\"/\">" << tr(lng::LNG_GLOBAL_PRODUCT) << "</a></span></h1>\r\n"
			"                <div id=\"site-description\">" << tr(lng::LNG_GLOBAL_DESCRIPTION) << "</div>\r\n"
			"            </div><!-- #branding -->\r\n"
			"\r\n"
			"            <div id=\"access\">\r\n"
			"                <div class=\"skip-link screen-reader-text\"><a href=\"#content\" title=\"" << tr(lng::LNG_NAV_SKIP) << "\">" << tr(lng::LNG_NAV_SKIP) << "</a></div>\r\n"
			"                <div class=\"menu-header\"><ul id=\"menu-site\" class=\"menu\"></ul></div>\r\n"
			"                </div><!-- #access -->\r\n"
			"        </div><!-- #masthead -->\r\n"
			"    </div><!-- #header -->\r\n"
			"\r\n"*/
			"    <div id=\"main\">\r\n"
			"        <div id=\"container\">\r\n"
			"            <div id=\"content\">\r\n";
	}

	void PageHandler::footer(const SessionPtr& session, Request& request, PageTranslation& tr)
	{
		bodyEnd(session, request, tr);
		request <<
			"  </body>\r\n"
			"</html>\r\n";
	}

	void PageHandler::bodyEnd(const SessionPtr& session, Request& request, PageTranslation& tr)
	{
		request << "\r\n"
			"            </div><!-- #content -->\r\n"
			"        </div><!-- #container -->\r\n"
			"    </div><!-- #main -->\r\n"
			"\r\n"
			"    </div>\r\n"
			;
		topbarUI(session, request, tr);
	}
	void PageHandler::topbarUI(const SessionPtr& session, Request& request, PageTranslation& tr)
	{
		request <<
			"<!-- UI -->\r\n"
			"<div id=\"mouse-catcher\"><div id=\"dimmer\"></div></div>"
			"<div id=\"subscribe\">"
				"<form>"
					"<label for=\"url\">" << tr(lng::LNG_SUBSCRIBE_PROMPT) << "</label><br/>"
					"<input type=\"url\" name=\"url\" id=\"subscribe-url\" />"
					"<div class='subscribe-hint'>" << tr(lng::LNG_SUBSCRIBE_HINT) << "</div>"
					"<div class='subscribe-error'></div>"
					"<div class='buttons'>"
					"<input name='add' type='submit' value='" << tr(lng::LNG_CMD_ADD) << "'/>"
					"<input name='cancel' type='submit' value='" << tr(lng::LNG_CMD_CLOSE) << "'/>"
					"</div>"
				"</form>"
			"</div>";
	}

}} // FastCGI::app
