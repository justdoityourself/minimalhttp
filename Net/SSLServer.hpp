/* Copyright D8Data 2017 */

#pragma once

#include <functional>
#include <tuple>

#include "TcpListener.hpp"
#include "TcpConnections.hpp"
#include "TcpReader.hpp"
#include "TcpWriter.hpp"
#include "SSL.hpp"
#include "Connection.hpp"

#pragma comment (lib, "crypt32")

namespace AtkNet
{
	template < typename on_accept_t,  typename on_disconnect_t, typename on_message_t, typename on_error_t, typename on_write_t > class SSLServer
		: private TcpWriter<BufferedConnection<SSLConnection>, on_write_t, on_error_t>
		, private TcpReader<BufferedConnection<SSLConnection>,std::function<void(BufferedConnection<SSLConnection>&,Memory&,Memory)>, on_error_t>
		, public TcpConnections<BufferedConnection<SSLConnection>, on_disconnect_t, on_error_t>
		, private EventHandler<std::tuple<BufferedConnection<SSLConnection>*,Memory,Memory>,std::function<void(std::tuple<BufferedConnection<SSLConnection>*,Memory,Memory>)>>
	{
		std::list< TcpListener< SSLConnection, std::function< void(SSLConnection*, TcpAddress, ConnectionType,uint32_t) > > > interfaces;

		on_accept_t OnAccept;
		on_message_t OnMessageEvent;
		ThreadHub& pool;
	public:

		struct Options
		{
			uint32_t event_threads = 1;
		};

		SSLServer(on_accept_t a, on_disconnect_t d, on_message_t r, on_error_t e, on_write_t w, Options o = Options(), ThreadHub& _pool = Threads())
			: OnAccept(a)
			, OnMessageEvent(r)
			, TcpWriter<BufferedConnection<SSLConnection>, on_write_t, on_error_t>(w,e,_pool)
			, TcpReader<BufferedConnection<SSLConnection>,std::function<void(BufferedConnection<SSLConnection>&,Memory&,Memory)>, on_error_t>(std::bind(&SSLServer::RawMessage,this,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3),e,_pool)
			, TcpConnections<BufferedConnection<SSLConnection>, on_disconnect_t, on_error_t>(d,_pool)
			, EventHandler<std::tuple<BufferedConnection<SSLConnection>*,Memory,Memory>,std::function<void(std::tuple<BufferedConnection<SSLConnection>*,Memory,Memory>)>>(o.event_threads,std::bind(&SSLServer::RawEvent,this,std::placeholders::_1),_pool)
			, pool(_pool) {}

		void Open(uint16_t port, const std::string & options, ConnectionType type) { interfaces.emplace_back(port,options,type,std::bind(&SSLServer::DoAccept,this,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4), pool); }

		void Connect(const std::string & host, ConnectionType type, uint32_t uid) 
		{ 
			EnableNetworking();
			SSLConnection c;
			TcpAddress address = { 0, 0 };

			if(!c.Connect(host))
				throw c.Error();

			DoAccept(&c,address,type,uid);
			c.Release();
		}

		template < typename F > void Connect2(const std::string & host, ConnectionType type, uint32_t uid, F f) 
		{ 
			EnableNetworking();
			SSLConnection c;
			TcpAddress address = { 0, 0 };

			if(!c.Connect2(host,f))
				throw c.Error();

			DoAccept(&c,address,type,uid);
			c.Release();
		}
	private:
		void DoAccept(SSLConnection *c, TcpAddress a, ConnectionType t,uint32_t uid) 
		{
			auto & bc = TcpConnections<BufferedConnection<SSLConnection>, on_disconnect_t, on_error_t>::Create(c,a,t,uid);

			if(!Common::Initialize(*c,t))
			{
				c->Close();
				return;
			}

			auto enable = OnAccept(bc);
			bc.UseAsync();
			if(enable.first) TcpWriter<BufferedConnection<SSLConnection>, on_write_t, on_error_t>::Manage(&bc);
			if(enable.second) TcpReader<BufferedConnection<SSLConnection>,std::function<void(BufferedConnection<SSLConnection>&,Memory&,Memory)>, on_error_t>::Manage(&bc);
		}

		void RawMessage(BufferedConnection<SSLConnection> & c,Memory & r, Memory m)
		{
			c.pending++;
			Push(std::make_tuple(&c,r,m));
		}

		void RawEvent(std::tuple<BufferedConnection<SSLConnection> *, Memory, Memory> p)
		{
			OnMessageEvent(*std::get<0>(p),std::get<1>(p),std::get<2>(p));
			std::get<0>(p)->pending --;
			std::get<1>(p).Cleanup(true);
		}
	};
}