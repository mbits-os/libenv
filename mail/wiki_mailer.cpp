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
#include <wiki_mailer.hpp>
#include <mail.hpp>
#include <wiki.hpp>
#include <fast_cgi/application.hpp>

#define MACHINE "reedr.net"

namespace mail
{
	struct FilterStream : wiki::stream
	{
		FilterPtr downstream;
		explicit FilterStream(const FilterPtr& downstream) : downstream(downstream) {}
		void write(const char* buffer, size_t size)
		{
			while (size--)
				downstream->put(*buffer++);
		}
	};

	class ImageVault
	{
		using CIDL = std::map<filesystem::path, ImagePartPtr>;

		CIDL cidl;
		MessagePtr message;
		filesystem::path data;
	public:
		ImageVault(const MessagePtr& message, const filesystem::path& data) : message(message), data(data) {}

		std::string image_uri(const filesystem::path& path)
		{
			auto full = filesystem::canonical(path, data);
			// TODO: check if path is inside data

			ImagePartPtr img;
			auto it = cidl.find(full);
			if (it != cidl.end())
				img = it->second;

			if (!img)
			{
				img = message->createInlineImage("image/png", full.native());
				if (!img)
					return std::string();

				cidl[full] = img;
			}

			return "cid:" + img->getCID();
		}
	};

	using ImageVaultPtr = std::shared_ptr<ImageVault>;

	class MailStyler : public wiki::styler
	{
		ImageVaultPtr images;
		bool has_hr = false;


		std::string image_uri(const filesystem::path& path) const
		{
			return images->image_uri(path);
		}
	public:
		MailStyler(const ImageVaultPtr& images) : images(images) {}

		void begin_document(wiki::stream& o) override
		{
			o <<
				"<div style=\"font-family: Helvetica Neu, Calibri, Tahoma, Arial, sans-serif; font-size: 13px; width: 809px; margin: 1em auto; padding: 13px; background: #EEE\">\n"
				"<img style=\"float: left\" src=\"" << image_uri("images/mail_logo.png") << "\" />\n";
		}

		void end_document(wiki::stream& o) override
		{
			if (has_hr)
				o << "<p style=\"padding-left: 109px; font-size: 10px; border: none; border-top: solid 1px #DDD; color: #AAA\">&nbsp;</p>\n\n";
			o << "</div>\n";
		}

		void image(wiki::stream& o, const std::string& path, const std::string& styles, const std::string& alt) const override
		{
			o << "<img";
			if (!styles.empty())
				o << " style=\"" << styles << "\"";
			o << " src=\"" << image_uri(path) << "\"" << alt << " />";
		}

		void begin_block(wiki::stream& o, const std::string& tag, const std::string& additional_style = std::string()) override
		{
			auto left = 109;
			auto left_style = "padding";
			auto style = "";

			auto _tag = tag.c_str();
			if (tag == "sign") { style = "font-style: italic; color: #888"; _tag = "p"; }
			if (tag == "quote") { style = "margin: 1em; padding: 1em; background: #F8F8F8; color: #666"; _tag = "div"; left_style = "margin"; }

			o << '<' << _tag << " style=\"";

			auto font = -1;
			if (_tag == "h1") font = 22;
			else if (_tag == "h2") font = 20;
			else if (_tag == "h3") font = 18;
			else if (_tag == "h4") font = 16;
			else if (_tag == "sign") font = 14;

			if (has_hr)
				font = font == -1 ? 10 : font * 77 / 100;

			bool has_style = false;

			if (font > 0)
			{
				o << "font-size: " << std::to_string(font) << "px";
				has_style = true;
			}

			if (*style)
			{
				if (has_style)
					o << "; ";

				has_style = true;
				o << style;
			}

			if (has_hr)
			{
				if (has_style)
					o << "; ";

				has_style = true;

				has_hr = false;
				o << "margin-top: 1em; padding-top: 1em; border: none; border-top: solid 1px #DDD; color: #AAA";
			}

			if (!additional_style.empty())
			{
				if (has_style)
					o << "; ";

				has_style = true;
				o << additional_style;
			}

			if (has_style)
				o << "; ";
			o << left_style << "-left: " << std::to_string(left) << "px\">";
		}

		void end_block(wiki::stream& o, const std::string& tag) override
		{
			auto _tag = tag.c_str();
			if (tag == "sign") _tag = "p";
			if (tag == "quote") _tag = "div";
			o << "</" << _tag << ">\n\n";
		}

		void hr(wiki::stream&) override
		{
			has_hr = true;
		}
	};

	struct WikiMessageProducer : MessageProducer
	{
		MessageWeakPtr message;

		wiki::document_ptr doc;
		wiki::variables_t vars;
		filesystem::path data;

		WikiMessageProducer(const wiki::document_ptr& doc, const wiki::variables_t& vars, const filesystem::path& data) : doc(doc), vars(vars), data(data) {}

		void echoPlain(const FilterPtr& downstream) override
		{
			FilterStream stream{ downstream };
			wiki::list_ctx ctx;

			doc->text(stream, vars, ctx);
		}

		void echoHtml(const FilterPtr& downstream) override
		{
			auto msg = message.lock();

			if (!msg)
			{
				FLOG << "Error while trying to acquire the message from the text producer";
				return;
			}

			FilterStream stream{ downstream };
			wiki::list_ctx ctx;
			auto vault = std::make_shared<ImageVault>(msg, data);
			auto styler = std::make_shared<MailStyler>(vault);

			doc->markup(stream, vars, styler, ctx);
		}

		void setMessage(const MessageWeakPtr& message) override
		{
			this->message = message;
		}
	};

	MessageProducerPtr make_wiki_producer(const wiki::document_ptr& doc, const wiki::variables_t& vars, const filesystem::path& data)
	{
		return std::make_shared<WikiMessageProducer>(doc, vars, data);
	}
}
