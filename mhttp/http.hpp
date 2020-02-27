/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include "tcpserver.hpp"

#include "d8u/buffer.hpp"

namespace mhttp
{
	using namespace d8u::buffer;

	class HttpConnection : public sock_t
	{
		static constexpr std::string_view _proto = "HTTP/1.1";
		static constexpr std::string_view _space = " ";
		static constexpr std::string_view _content_length = "Content-Length: ";
		static constexpr std::string_view _newline = "\r\n";
		static constexpr std::string_view _eof = "\r\n\r\n";

	public:
		HttpConnection() {}

		HttpConnection(std::string_view host)
		{
			Connect(host);
		}

		void Connect(std::string_view host)
		{
			TcpConnection c;
			TcpAddress a;

			EnableNetworking();
			if (!c.Connect(host))
				return;

			sock_t::Init(c, a, ConnectionType::http, 15);
			c.Release();
		}

		template < typename T, typename ... t_args > static std::vector<uint8_t> FormatHttpResponse(std::string_view status, const T & contents, t_args...headers)
		{
			return join_memory(_proto, _space, status, _newline, headers..., _content_length, std::to_string(contents.size()), _eof, contents);
		}

		template < typename T, typename ... t_args > static std::vector<uint8_t> FormatHttpRequest(std::string_view command, const string_view path, const T & contents, t_args...headers)
		{
			return join_memory(command, _space, path, _space, _proto, _newline, headers..., _content_length, std::to_string(contents.size()), _eof, contents);
		}

		//Server Functions:
		//

		template < typename ... t_args > void Http200(t_args...headers)
		{
			AsyncWrite(FormatHttpResponse("200 OK", gsl::span<uint8_t>(), headers...));
		}

		template < typename T, typename ... t_args > void Response(std::string_view status, const T & contents, t_args...headers)
		{
			AsyncWrite(FormatHttpResponse(status, contents, headers...));
		}

		template < typename ... t_args > void Http400(t_args...headers)
		{
			AsyncWrite(FormatHttpResponse("400 Bad Request", gsl::span<uint8_t>(), headers...));
		}


		//Client Functions:
		//

		template < typename T, typename ... t_args > auto Request(std::string_view command, std::string_view path, const T & contents, t_args...headers)
		{
			sock_t::Write(FormatHttpRequest(command, path, contents, headers...));

			sock_t::Async(1);

			auto [_response, body] = Http::ReadResponse(*this);

			sock_t::Async(0);

			return ( _response.size() ) ? Http::ParseResponse(std::move(_response), body) : Http::Response();
		}

		template < typename ... t_args > auto Get(std::string_view path, t_args...headers)
		{
			return Request("GET", path, gsl::span<uint8_t>(), headers...);
		}

		template < typename T, typename ... t_args > auto Post(std::string_view path, const T & contents, t_args...headers)
		{
			return Request("POST", path, contents, headers...);
		}
	};

	template < typename F > auto& make_http_server(const string_view port, F f, size_t threads = 1)
	{
		auto on_connect = [&](const auto& c) { return make_pair(true, true); };
		auto on_disconnect = [&](const auto& c) {};
		auto on_error = [&](const auto& c) {};
		auto on_write = [&](const auto& mc, const auto& c) {};

		static TcpServer http(on_connect, on_disconnect, [&](auto& _client, auto&& _request, const auto& body)
		{
			//On Request:
			//

			auto& client = *(HttpConnection*)&_client;

			try
			{
				f(client, Http::ParseRequest(std::move(_request), body));
			}
			catch (...)
			{
				client.Http400();
			}

		}, on_error, on_write, { threads });

		http.Open((uint16_t)stoi(port.data()), "", ConnectionType::http);

		return http;
	}
}