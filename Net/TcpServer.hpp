/* Copyright D8Data 2019 */

#pragma once

#include <functional>
#include <tuple>

#include "TcpListener.hpp"
#include "TcpConnections.hpp"
#include "Tcp.hpp"
#include "Connection.hpp"
#include "TcpReader.hpp"
#include "TcpWriter.hpp"

namespace atk
{
	template < typename on_accept_t,  typename on_disconnect_t, typename on_message_t, typename on_error_t, typename on_write_t > class TcpServer
		: private TcpWriter<BufferedConnection<TcpConnection>, on_write_t, on_error_t>
		, private TcpReader<BufferedConnection<TcpConnection>,std::function<void(BufferedConnection<TcpConnection>&,Memory&,Memory)>, on_error_t>
		, public TcpConnections<BufferedConnection<TcpConnection>, on_disconnect_t, on_error_t>
		, private EventHandler<std::tuple<BufferedConnection<TcpConnection>*,Memory,Memory>,std::function<void(std::tuple<BufferedConnection<TcpConnection>*,Memory,Memory>)>>
	{
		std::list< TcpListener< TcpConnection, std::function< void(TcpConnection*, TcpAddress, ConnectionType,uint32_t) > > > interfaces;

		on_accept_t OnAccept;
		on_message_t OnMessageEvent;
		ThreadHub& pool;
	public:

		struct Options
		{
			uint32_t event_threads = 1;
		};

		TcpServer(on_accept_t a, on_disconnect_t d, on_message_t r, on_error_t e, on_write_t w, Options o = Options(), ThreadHub& _pool = Threads())
			: OnAccept(a)
			, OnMessageEvent(r)
			, TcpWriter<BufferedConnection<TcpConnection>, on_write_t, on_error_t>(w,e,_pool)
			, TcpReader<BufferedConnection<TcpConnection>,std::function<void(BufferedConnection<TcpConnection>&,Memory&,Memory)>, on_error_t>(std::bind(&TcpServer::RawMessage,this,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3),e,_pool)
			, TcpConnections<BufferedConnection<TcpConnection>, on_disconnect_t, on_error_t>(d,_pool)
			, EventHandler<std::tuple<BufferedConnection<TcpConnection>*,Memory,Memory>,std::function<void(std::tuple<BufferedConnection<TcpConnection>*,Memory,Memory>)>>(o.event_threads,std::bind(&TcpServer::RawEvent,this,std::placeholders::_1),_pool)
			, pool(_pool) {}

		void Open(uint16_t port, const std::string & options, ConnectionType type) { interfaces.emplace_back(port,options,type,std::bind(&TcpServer::DoAccept,this,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4), pool); }

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
			auto & bc = TcpConnections<BufferedConnection<TcpConnection>, on_disconnect_t, on_error_t>::Create(c,a,t,uid);

			if(!Common::Initialize(*c,t))
			{
				c->Close();
				return;
			}

			auto enable = OnAccept(bc);
			bc.UseAsync();
			if(enable.first) TcpWriter<BufferedConnection<TcpConnection>, on_write_t, on_error_t>::Manage(&bc);
			if(enable.second) TcpReader<BufferedConnection<TcpConnection>,std::function<void(BufferedConnection<TcpConnection>&,Memory&,Memory)>, on_error_t>::Manage(&bc);
		}

		void RawMessage(BufferedConnection<TcpConnection> & c,Memory & r, Memory m)
		{
			c.pending++;
			Push(std::make_tuple(&c,r,m));
		}

		void RawEvent(std::tuple<BufferedConnection<TcpConnection> *, Memory, Memory> p)
		{
			OnMessageEvent(*std::get<0>(p),std::get<1>(p),std::get<2>(p));
			std::get<0>(p)->pending --;
			std::get<1>(p).Cleanup(true);
		}
	};
}