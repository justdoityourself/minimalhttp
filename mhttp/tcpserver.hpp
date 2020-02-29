/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include <functional>
#include <tuple>

#include "common.hpp"
#include "listener.hpp"
#include "connections.hpp"
#include "tcp.hpp"
#include "connection.hpp"
#include "reader.hpp"
#include "writer.hpp"

#include "../gsl-lite.hpp"

namespace mhttp
{
	using event_p = std::tuple< sock_t*, std::vector<uint8_t>, gsl::span<uint8_t>, void* >;

	class TcpServer : public ThreadHub
		, private TcpWriter
		, private TcpReader
		, public TcpConnections
		, private EventHandler< event_p >
	{
		std::list< TcpListener< TcpConnection > > interfaces;

		on_accept_t OnAccept;
		on_message_t OnMessageEvent;
	public:

		struct Options
		{
			size_t event_threads = 1;
		};

		TcpServer(on_accept_t a, on_disconnect_t d, on_message_t r, on_error_t e, on_write_t w, Options o)
			: OnAccept(a)
			, OnMessageEvent(r)
			, TcpWriter(w, e, *this)
			, TcpReader(std::bind(&TcpServer::RawMessage, this, p1_t, p2_t, p3_t), e, *this)
			, TcpConnections(d, *this)
			, EventHandler< event_p >(o.event_threads, std::bind(&TcpServer::RawEvent, this, p1_t), *this) {}

		TcpServer( on_message_t r, Options o = Options())
			: TcpServer( [](const auto& c) { return make_pair(true, true); }
				, [](const auto& c) {}, r, [](const auto& c) {}
				, [](const auto& mc, const auto& c) {}, o) {}

		TcpServer(uint16_t port, ConnectionType type,on_accept_t a, on_disconnect_t d, on_message_t r, on_error_t e, on_write_t w, bool mplex, Options o)
			: TcpServer(a,d,r,e,w,o) 
		{ 
			Open(port, "", type, mplex); 
		}

		TcpServer(uint16_t port, ConnectionType type, on_message_t r, bool mplex = false, Options o = Options())
			: TcpServer(port, type
				, [](const auto& c) { return make_pair(true, true); }
				, [](const auto& c) {}, r, [](const auto& c) {}
				, [](const auto& mc, const auto& c) {}, mplex, o) {}

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

		void Open(uint16_t port, const std::string& options, ConnectionType type, bool mplex)
		{
			interfaces.emplace_back(port, options, type, std::bind(&TcpServer::DoAccept, this, p1_t, p2_t, p3_t, p4_t, p5_t), mplex, *this);
		}

		void Connect(const std::string& host, ConnectionType type)
		{
			TcpConnection c;
			TcpAddress address;

			EnableNetworking();
			if (!c.Connect(host))
				throw c.Error();

			DoAccept(&c, address, type,15,false);
			c.Release();
		}

	private:

		void DoAccept(TcpConnection* c, TcpAddress a, ConnectionType t, uint32_t uid, bool multiplex)
		{
			auto& bc = TcpConnections::Create(c, a, t, uid);

			if (!Common::Initialize(*c, t))
			{
				c->Close();
				return;
			}

			auto enable = OnAccept(bc);
			bc.multiplex = multiplex;
			bc.Async();

			if (enable.first)
				TcpWriter::Manage(&bc);

			if (enable.second)
				TcpReader::Manage(&bc);
		}

		void RawMessage(sock_t& c, std::vector<uint8_t>&& v, gsl::span<uint8_t> s)
		{
			void* queued_write = nullptr;

			if (!c.Multiplex())
				c.read_lock = true;
			else
				queued_write = (c.type >= ConnectionType::map32) ? (void*)c.QueueMap() : (void*)c.QueueWrite();

			c.pending++;
			EventHandler< event_p >::Push(std::make_tuple(&c, std::move(v), s, queued_write));
		}

		void RawEvent(event_p&& p)
		{
			auto& c = *std::get<0>(p);
			std::apply(OnMessageEvent, p);
			c.pending--;

			if (!c.Multiplex())
				c.read_lock = false;
		}
	};

	template < typename F > auto make_tcp_server(const string_view port, F f, size_t threads = 1, bool mplex = false, ConnectionType type = ConnectionType::message)
	{
		auto on_connect = [](const auto& c) { return make_pair(true, true); };
		auto on_disconnect = [](const auto& c) {};
		auto on_error = [](const auto& c) {};
		auto on_write = [](const auto& mc, const auto& c) {};

		return TcpServer((uint16_t)stoi(port.data()), type,on_connect, on_disconnect, [f](auto* client, auto&& request, auto body, auto * mplex)
		{
			try
			{
				f(*client, std::move(request),mplex);
			}
			catch (...)
			{
				//Something went wrong. Let's shut this socket down.
				client->reader_fault = true;
				client->writer_fault = true;
			}

		}, on_error, on_write, mplex, { threads });
	}

	template < typename F > auto make_map_server(const string_view port, F f, size_t threads = 1)
	{
		return make_tcp_server(port, f, threads, false, ConnectionType::map32);
	}

	template < typename F > auto make_halfmap_server(const string_view port, F f, size_t threads = 1)
	{
		return make_tcp_server(port, f, threads, true, ConnectionType::writemap32);
	}
}