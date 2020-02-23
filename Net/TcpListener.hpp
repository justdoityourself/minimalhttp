/* Copyright D8Data 2017 */

#pragma once

#include "Helper/Async.hpp"
#include "Common.hpp"
#include "Connection.hpp"
#include <atomic>

namespace atk
{
	class Unique
	{
		std::atomic<uint32_t> uid;
	public:
		Unique():uid(1){}

		uint32_t Get() { return uid++; }
	};

	uint32_t ProgramUniqueId() { return make_singleton<Unique>().Get(); }

	template < typename C, typename on_accept_t >class TcpListener
	{
		uint16_t port;
		ConnectionType type;
		on_accept_t OnAccept;
		C server;
		std::string options;
	public:
		TcpListener(on_accept_t _on_accept):OnAccept(_on_accept){}
		TcpListener(uint16_t _port, const std::string & options, ConnectionType _type, on_accept_t _on_accept, ThreadHub& pool = Threads()):OnAccept(_on_accept){Listen(_port,options,_type,pool);}

		void Listen(uint16_t _port,const std::string _options, ConnectionType _type, ThreadHub& pool = Threads())
		{
			EnableNetworking();
			port = _port;
			type = _type;
			options = _options;

			pool.Async([&](bool & run)
			{
				if(!server.Listen(port,options))
					throw server.Error();

				while(run)
				{
					C c;
					TcpAddress address;
					if(!server.Accept(c,&address))
						throw server.Error();

					try
					{
						OnAccept(&c,address,type,ProgramUniqueId());
					}
					catch(...)
					{
						//std::cout << "Exception in OnAccept handler." << std::endl;
					}

					c.Release();
				}
			});
		}
	};
}