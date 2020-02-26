/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include <functional>
#include <tuple>

#include "listener.hpp"
#include "connections.hpp"
#include "tcp.hpp"
#include "connection.hpp"
#include "reader.hpp"
#include "writer.hpp"

#include "../gsl-lite.hpp"

namespace mhttp
{
	using sock_t = BufferedConnection< TcpConnection >;
	using io_t = std::function< void( sock_t&, std::vector<uint8_t> , gsl::span<uint8_t>) >;
	using event_p = std::tuple< sock_t*, std::vector<uint8_t>, gsl::span<uint8_t> >;
	using event_f = std::function< void(event_p) >;
	using listen_t = std::function< void( TcpConnection*, TcpAddress, ConnectionType, uint32_t ) >;

	auto p1_t = std::placeholders::_1;
	auto p2_t = std::placeholders::_2;
	auto p3_t = std::placeholders::_3;
	auto p4_t = std::placeholders::_4;

	template <	  typename on_accept_t
				, typename on_disconnect_t
				, typename on_message_t
				, typename on_error_t
				, typename on_write_t > class TcpServer

		: private TcpWriter< sock_t, on_write_t, on_error_t >
		, private TcpReader< sock_t, io_t, on_error_t >
		, public TcpConnections< sock_t, on_disconnect_t >
		, private EventHandler< event_p, event_f >
	{
		std::list< TcpListener< TcpConnection, listen_t > > interfaces;

		on_accept_t OnAccept;
		on_message_t OnMessageEvent;
		ThreadHub& pool;
	public:

		struct Options
		{
			uint32_t event_threads = 1;
		};

		TcpServer(on_accept_t a, on_disconnect_t d, on_message_t r, on_error_t e, on_write_t w, Options o = Options(), ThreadHub& _pool = Threads())
			: OnAccept ( a )
			, OnMessageEvent ( r )
			, TcpWriter< sock_t, on_write_t, on_error_t > ( w, e, _pool )
			, TcpReader< sock_t, io_t, on_error_t> ( std::bind( &TcpServer::RawMessage, this, p1_t, p2_t, p3_t), e, _pool )
			, TcpConnections< sock_t, on_disconnect_t > ( d,_pool )
			, EventHandler< event_p, event_f >( o.event_threads, std::bind( &TcpServer::RawEvent, this, p1_t), _pool )
			, pool(_pool) {}

		void Open(uint16_t port, const std::string & options, ConnectionType type) 
		{ 
			interfaces.emplace_back( port, options,type, std::bind( &TcpServer::DoAccept, this, p1_t, p2_t, p3_t, p4_t), pool );
		}

		void Connect(const std::string & host, ConnectionType type) 
		{ 
			TcpConnection c;
			TcpAddress address;

			EnableNetworking();
			if(!c.Connect(host))
				throw c.Error();

			DoAccept(&c,address,type);
			c.Release();
		}

	private:

		void DoAccept(TcpConnection *c, TcpAddress a, ConnectionType t,uint32_t uid) 
		{
			auto & bc = TcpConnections<sock_t, on_disconnect_t>::Create(c,a,t,uid);

			if(!Common::Initialize(*c,t))
			{
				c->Close();
				return;
			}

			auto enable = OnAccept(bc);
			bc.UseAsync();

			if(enable.first) 
				TcpWriter<sock_t, on_write_t, on_error_t>::Manage(&bc);

			if(enable.second) 
				TcpReader<sock_t,io_t, on_error_t>::Manage(&bc);
		}

		void RawMessage(sock_t & c, std::vector<uint8_t> v, gsl::span<uint8_t> s)
		{
			c.pending++;
			EventHandler< event_p, event_f >::Push( std::make_tuple( &c, std::move(v), s ) );
		}

		void RawEvent(event_p p)
		{
			OnMessageEvent(*std::get<0>(p),std::get<1>(p),std::get<2>(p));
			std::get<0>(p)->pending --;
		}
	};


	template < typename F > int make_http_server(const string_view port, F f)
	{
		auto on_connect = [&](const auto& c) { return make_pair(true, true); };
		auto on_disconnect = [&](const auto& c) {};
		auto on_error = [&](const auto& c) {};
		auto on_write = [&](const auto& mc, const auto& c) {};

		TcpServer http(on_connect, on_disconnect, [&](auto& client, const auto& request, const auto& body)
		{
			//On Request:
			//

			try
			{
				f(client, request, body);
			}
			catch (...)
			{
				//client.Http400();
			}

		}, on_error, on_write);

		http.Open((uint16_t)stoi(port.data()), "", ConnectionType::http);

		Threads().Wait();

		return 0;
	}
}