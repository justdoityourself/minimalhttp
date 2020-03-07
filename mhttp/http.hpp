/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include "tcpserver.hpp"
#include "client.hpp"

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

		HttpConnection(std::string_view host, ConnectionType type = ConnectionType::http)
		{
			sock_t::Connect(host, type);
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

		template < typename ... t_args > void Http404(t_args...headers)
		{
			AsyncWrite(FormatHttpResponse("404 Not Found", gsl::span<uint8_t>(), headers...));
		}

		template < typename ... t_args > void Http200A(void* queue, t_args...headers)
		{
			ActivateWrite(queue,FormatHttpResponse("200 OK", gsl::span<uint8_t>(), headers...));
		}

		template < typename T, typename ... t_args > void ResponseA(void* queue, std::string_view status, const T& contents, t_args...headers)
		{
			ActivateWrite(queue, FormatHttpResponse(status, contents, headers...));
		}

		template < typename ... t_args > void Http400A(void* queue, t_args...headers)
		{
			ActivateWrite(queue, FormatHttpResponse("400 Bad Request", gsl::span<uint8_t>(), headers...));
		}

		template < typename ... t_args > void Http404A(void* queue, t_args...headers)
		{
			ActivateWrite(queue, FormatHttpResponse("404 Not Found", gsl::span<uint8_t>(), headers...));
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

	class EventHttp : public EventClientT < HttpConnection >
	{
	public:
		EventHttp(std::string_view host)
			: EventClientT < HttpConnection >(host, ConnectionType::http)
		{ }

		template < typename T, typename ... t_args > auto RequestWait(std::string_view command, std::string_view path, const T& contents, t_args...headers)
		{
			auto [_response, body] = AsyncWriteWait(FormatHttpRequest(command, path, contents, headers...));

			return (_response.size()) ? Http::ParseResponse(std::move(_response), body) : Http::Response();
		}

		template < typename F, typename T, typename ... t_args > void RequestCallback(F && f, std::string_view command, std::string_view path, const T& contents, t_args...headers)
		{
			AsyncWriteCallback(FormatHttpRequest(command, path, contents, headers...),[f = std::move(f)](auto _response, auto body)
			{
				f((_response.size()) ? Http::ParseResponse(std::move(_response), body) : Http::Response());
			});
		}

		template < typename ... t_args > auto GetWait(std::string_view path, t_args...headers)
		{
			return RequestWait("GET", path, gsl::span<uint8_t>(), headers...);
		}

		template < typename T, typename ... t_args > auto PostWait(std::string_view path, const T& contents, t_args...headers)
		{
			return RequestWait("POST", path, contents, headers...);
		}

		template < typename F, typename ... t_args > void GetCallback(F && f, std::string_view path, t_args...headers)
		{
			return RequestCallback(f, "GET", path, gsl::span<uint8_t>(), headers...);
		}

		template < typename F, typename T, typename ... t_args > void PostCallback(F && f, std::string_view path, const T& contents, t_args...headers)
		{
			return RequestCallback(f,"POST", path, contents, headers...);
		}
	};

	template < typename F > auto make_http_server(const string_view port, F && f, size_t threads = 1, bool mplex = false)
	{
		auto on_connect = [](const auto& c) { return make_pair(true, true); };
		auto on_disconnect = [](const auto& c) {};
		auto on_error = [](const auto& c) {};
		auto on_write = [](const auto& mc, const auto& c) {};

		return TcpServer((uint16_t)stoi(port.data()), ConnectionType::http, on_connect, on_disconnect, [f = std::move(f)](auto* _client, auto&& _request, auto body,auto * mplex)
		{
			//On Request:
			//

			auto& client = *(HttpConnection*)_client;

			try
			{
				f(client, Http::ParseRequest(std::move(_request), body),mplex);
			}
			catch (...)
			{
				client.Http400();
			}

		}, on_error, on_write, mplex,{ threads });
	}


	using on_http_t = std::function < void(HttpConnection&, Http::Request, void*)>;

	class HttpServer : public TcpServer
	{
		on_http_t on_request;
	public:
		HttpServer(on_http_t _req_cb, TcpServer::Options o = TcpServer::Options())
			: on_request(_req_cb)
			, TcpServer([&](auto* _client, auto&& _request, auto body, auto* mplex)
			{
				auto& client = *(HttpConnection*)_client;

				try
				{
					on_request(client, Http::ParseRequest(std::move(_request), body), mplex);
				}
				catch (...)
				{
					client.Http400();
				}

			},o) { }

		HttpServer(uint16_t port, on_http_t _req_cb, bool mplex=false, TcpServer::Options o = TcpServer::Options())
			: HttpServer(_req_cb, o)
		{
			Open(port,"",mplex);
		}

		void Open(uint16_t port, const std::string& options="", bool mplex=false)
		{
			TcpServer::Open(port,options,ConnectionType::http,mplex);
		}
	};
}