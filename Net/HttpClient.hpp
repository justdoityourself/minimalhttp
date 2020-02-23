/* Copyright D8Data 2019 */

#pragma once

#include <string_view>

#include "Tcp.hpp"
#include "Connection.hpp"

namespace atk
{
	using namespace std;

	class HttpClient
	{
	public:
		template < typename F > static void RequestSync(const string_view host, const Memory m, F f)
		{
			TcpConnection c;
			TcpAddress a;

			EnableNetworking();
			if (!c.Connect(host))
				throw c.Error();

			BufferedConnection<TcpConnection> bc(c, a, ConnectionType::http, 15);

			c.Write(m);

			bc.UseAsync();
			auto reply = Http::ReadBlocking(bc);
			f(reply.first, reply.second);
			reply.first.Cleanup(true);
		}

		template < typename F > static void RequestSync(const string_view host, HttpCommand command, const std::string req,F f)
		{
			RequestSync(host, AtkNet::Common::HttpMessage(command, req, ""),f);
		}

		template <typename F, typename R, typename ... t_args> static void StreamSync2(const string_view host, uint32_t size, F f, HttpCommand cmd, const string_view path, R r, t_args...args)
		{
			RequestSync(host, Common::HttpMessageStream(size, f, cmd, path, args...), r);
		}

		template <typename F, typename R, typename ... t_args> static void StreamSync(const string_view host, uint32_t size, F f, R r, t_args...args)
		{
			StreamSync2(host, size, f, HttpCommand::POST, "/", r, args...);
		}
	};
}