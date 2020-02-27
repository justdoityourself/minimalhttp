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
	using io_t = std::function< void( sock_t&, std::vector<uint8_t>&&, gsl::span<uint8_t>) >;
	using event_p = std::tuple< sock_t*, std::vector<uint8_t> , gsl::span<uint8_t>, void * >;
	using event_f = std::function< void(event_p &&) >;
	using listen_t = std::function< void( TcpConnection*, TcpAddress, ConnectionType, uint32_t, bool ) >;

	auto p1_t = std::placeholders::_1;
	auto p2_t = std::placeholders::_2;
	auto p3_t = std::placeholders::_3;
	auto p4_t = std::placeholders::_4;
	auto p5_t = std::placeholders::_5;

	template <	  typename on_accept_t
				, typename on_disconnect_t
				, typename on_message_t
				, typename on_error_t
				, typename on_write_t > class TcpServer

		: public ThreadHub
		, private TcpWriter< sock_t, on_write_t, on_error_t >
		, private TcpReader< sock_t, io_t, on_error_t >
		, public TcpConnections< sock_t, on_disconnect_t >
		, private EventHandler< event_p, event_f >
	{
		std::list< TcpListener< TcpConnection, listen_t > > interfaces;

		on_accept_t OnAccept;
		on_message_t OnMessageEvent;
	public:

		struct Options
		{
			size_t event_threads = 1;
		};

		TcpServer(on_accept_t a, on_disconnect_t d, on_message_t r, on_error_t e, on_write_t w, Options o = Options())
			: OnAccept ( a )
			, OnMessageEvent ( r )
			, TcpWriter< sock_t, on_write_t, on_error_t > ( w, e, *this)
			, TcpReader< sock_t, io_t, on_error_t> ( std::bind( &TcpServer::RawMessage, this, p1_t, p2_t, p3_t), e, *this)
			, TcpConnections< sock_t, on_disconnect_t > ( d, *this)
			, EventHandler< event_p, event_f >( o.event_threads, std::bind( &TcpServer::RawEvent, this, p1_t), *this) {}

		~TcpServer()
		{
			Shutdown();
		}

		void Shutdown()
		{
			for (auto& e : interfaces)
				e.Shutdown();

			ThreadHub::Stop();
		}

		void Join()
		{
			ThreadHub::Join();
		}

		void Open(uint16_t port, const std::string & options, ConnectionType type, bool mplex) 
		{ 
			interfaces.emplace_back( port, options,type, std::bind( &TcpServer::DoAccept, this, p1_t, p2_t, p3_t, p4_t, p5_t), mplex, *this );
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

		void DoAccept(TcpConnection *c, TcpAddress a, ConnectionType t,uint32_t uid, bool multiplex) 
		{
			auto & bc = TcpConnections<sock_t, on_disconnect_t>::Create(c,a,t,uid);

			if(!Common::Initialize(*c,t))
			{
				c->Close();
				return;
			}

			auto enable = OnAccept(bc);
			bc.multiplex = multiplex;
			bc.Async();

			if(enable.first) 
				TcpWriter<sock_t, on_write_t, on_error_t>::Manage(&bc);

			if(enable.second) 
				TcpReader<sock_t,io_t, on_error_t>::Manage(&bc);
		}

		void RawMessage(sock_t & c, std::vector<uint8_t> && v, gsl::span<uint8_t> s)
		{
			void* queued_write = nullptr;

			if (!c.Multiplex())
				c.read_lock = true;
			else
				queued_write = (c.type >= ConnectionType::map32) ? (void*)c.QueueMap() : (void*)c.QueueWrite();

			c.pending++;
			EventHandler< event_p, event_f >::Push( std::make_tuple( &c, std::move(v), s, queued_write ) );
		}

		void RawEvent(event_p && p)
		{
			auto& c = *std::get<0>(p);
			std::apply( OnMessageEvent, p );
			c.pending --;

			if (!c.Multiplex())
				c.read_lock = false;	
		}
	};

	template < typename F > auto& make_tcp_server(const string_view port, F f, size_t threads = 1, ConnectionType type = ConnectionType::message, bool mplex = false)
	{
		auto on_connect = [&](const auto& c) { return make_pair(true, true); };
		auto on_disconnect = [&](const auto& c) {};
		auto on_error = [&](const auto& c) {};
		auto on_write = [&](const auto& mc, const auto& c) {};

		static TcpServer tcp(on_connect, on_disconnect, [&](auto* client, auto&& request, auto body, auto * mplex)
		{
			try
			{
				f(*client, std::move(request));
			}
			catch (...)
			{
				//Something went wrong. Let's shut this socket down.
				client->reader_fault = true;
				client->writer_fault = true;
			}

		}, on_error, on_write, { threads });

		tcp.Open( (uint16_t) stoi(port.data()), "", type, mplex);

		return tcp;
	}

	template < typename F > auto& make_map_server(const string_view port, F f, size_t threads = 1)
	{
		return make_tcp_server(port, f, threads, ConnectionType::map32, false);
	}

	template < typename F > auto& make_writemap_server(const string_view port, F f, size_t threads = 1)
	{
		return make_tcp_server(port, f, threads, ConnectionType::writemap32, true);
	}
}