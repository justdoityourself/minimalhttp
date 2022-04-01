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

#include "d8u/memory.hpp"
#include "d8u/util.hpp"

namespace mhttp
{
	using event_p = std::tuple< void*, sock_t*, d8u::sse_vector, gsl::span<uint8_t>, void* >;

	template < typename CT = sock_t > class TcpServer : public ThreadHub
		, private TcpWriter
		, private TcpReader
		, public TcpConnections<CT>
		, private EventHandler< event_p >
	{
		std::list< TcpListener< TcpConnection > > interfaces;

		on_accept_t OnAccept;
		on_message_t OnMessageEvent;
		on_write_t OnWrite;

		size_t read_buffer = 0;
		size_t write_buffer = 0;
	public:

		struct Options
		{
			size_t event_threads = 1;
		};

		TcpServer(on_accept_t a, on_disconnect_t d, on_message_t r, on_error_t e, on_write_t w, Options o)
			: OnAccept(a)
			, OnMessageEvent(r)
			, OnWrite(w)
			, TcpWriter(std::bind(&TcpServer::RawWrite, this, p1_t, p2_t), e, *this)
			, TcpReader(std::bind(&TcpServer::RawMessage, this, p1_t, p2_t, p3_t), e, *this)
			, TcpConnections<CT>(d, *this)
			, EventHandler< event_p >(o.event_threads, std::bind(&TcpServer::RawEvent, this, p1_t), *this) {}

		TcpServer(on_message_t r, Options o = Options())
			: TcpServer([](const auto& c) { return std::make_pair(true, true); }
				, [](const auto& c) {}, r, [](const auto& c) {}
				, [](const auto& mc, const auto& c) {}, o) {}

		TcpServer(on_message_t r, on_disconnect_t d, Options o = Options())
			: TcpServer([](const auto& c) { return std::make_pair(true, true); }
				, d, r, [](const auto& c) {}
				, [](const auto& mc, const auto& c) {}, o) {}

		TcpServer(uint16_t port, ConnectionType type, on_accept_t a, on_disconnect_t d, on_message_t r, on_error_t e, on_write_t w, bool mplex, Options o)
			: TcpServer(a, d, r, e, w, o)
		{
			Open(port, "", type, mplex);
		}

		template <typename T> TcpServer(const T & ports, on_accept_t a, on_disconnect_t d, on_message_t r, on_error_t e, on_write_t w, bool mplex, Options o)
			: TcpServer(a, d, r, e, w, o)
		{
			Open(ports, mplex);
		}

		TcpServer(uint16_t port, ConnectionType type, on_message_t r, bool mplex = false, Options o = Options())
			: TcpServer(port, type
				, [](const auto& c) { return std::make_pair(true, true); }
				, [](const auto& c) {}, r, [](const auto& c) {}
				, [](const auto& mc, const auto& c) {}, mplex, o) {}

		~TcpServer()
		{
			Shutdown();
		}

		void ReadBuffer(size_t size)
		{
			read_buffer = size;
		}

		void WriteBuffer(size_t size)
		{
			write_buffer = size;
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

		template <typename T> void Open(const T& ports, bool mplex)
		{
			std::apply([&](auto& ...x)
			{
				(Open(x.first, "", x.second, mplex), ...);
			}, ports);
		}

		void Connect(const std::string& host, ConnectionType type)
		{
			TcpConnection c;
			TcpAddress address = { 0, 0 };

			EnableNetworking();
			if (!c.Connect(host))
				throw std::runtime_error(c.Error());

			DoAccept(&c, address, type, 15, false);
			c.Release();
		}

		size_t MessageCount() { return messages; }
		size_t EventsStarted() { return events_started; }
		size_t EventsFinished() { return events_finished; }
		size_t ReplyCount() { return replies; }

	private:

		std::atomic<size_t> messages = 0;
		std::atomic<size_t> events_started = 0;
		std::atomic<size_t> events_finished = 0;
		std::atomic<size_t> replies = 0;

		void DoAccept(TcpConnection* c, TcpAddress a, ConnectionType t, uint32_t uid, bool multiplex)
		{
			auto& bc = TcpConnections<CT>::Create(c, a, t, uid);

			if (!Common::Initialize(*c, t))
			{
				c->Close();
				return;
			}

			if (read_buffer)
				c->ReadBuffer((int)read_buffer);

			if (write_buffer)
				c->WriteBuffer((int)write_buffer);

			d8u::trace("New Connection:", uid);

			auto enable = OnAccept(bc);
			bc.multiplex = multiplex;
			bc.Async();

			if (enable.first)
				TcpWriter::Manage(&bc);

			if (enable.second)
				TcpReader::Manage(&bc);
		}

		void RawWrite(sock_t& c, size_t s)
		{
			replies++;
			OnWrite(c, s);
		}

		void RawMessage(sock_t& c, d8u::sse_vector&& v, gsl::span<uint8_t> s)
		{
			if (!v.size())
			{
				//Empty message indicates desire to disconnect.
				c.priority = -1;
				c.Close();
				return; 
			}

			messages++;

			void* queued_write = nullptr;

			if (!c.Multiplex())
				c.read_lock = true;
			else
				queued_write = (c.type >= ConnectionType::map32) ? (void*)c.QueueMap() : (void*)c.QueueWrite();

			c.pending++;
			EventHandler< event_p >::Push(std::make_tuple((void*)this, &c, std::move(v), s, queued_write));
		}

		void RawEvent(event_p&& p)
		{
			events_started++;

			auto& c = *std::get<1>(p);
			std::apply(OnMessageEvent, p);
			c.pending--;

			if (!c.Multiplex())
				c.read_lock = false;

			events_finished++;
		}
	};

	template < typename F > auto make_tcp_server(const std::string_view port, F f, size_t threads = 1, bool mplex = false, ConnectionType type = ConnectionType::message)
	{
		auto on_connect = [](const auto& c) { return std::make_pair(true, true); };
		auto on_disconnect = [](const auto& c) {};
		auto on_error = [](const auto& c) {};
		auto on_write = [](const auto& mc, const auto& c) {};

		return TcpServer((uint16_t)std::stoi(port.data()), type,on_connect, on_disconnect, [f](void * server,auto* client, auto&& request, auto body, auto * mplex)
		{
			try
			{
				f(*client, std::move(request),mplex);
			}
			catch (...)
			{
				//Something went wrong. Let's shut this socket down.
				client->Close();
			}

		}, on_error, on_write, mplex, { threads });
	}

	template < typename F > auto make_map_server(const std::string_view port, F f, size_t threads = 1)
	{
		return make_tcp_server(port, f, threads, false, ConnectionType::map32);
	}

	template < typename F > auto make_halfmap_server(const std::string_view port, F f, size_t threads = 1)
	{
		return make_tcp_server(port, f, threads, true, ConnectionType::writemap32);
	}
}