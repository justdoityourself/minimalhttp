/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include <atomic>

#include "async.hpp"
#include "common.hpp"
#include "connection.hpp"

namespace mhttp
{
	class Unique
	{
		std::atomic<uint32_t> uid;
	public:
		Unique():uid(1){}

		uint32_t Get() { return uid++; }
	};

	uint32_t ProgramUniqueId() { static Unique u; return u.Get(); }

	template < typename C >class TcpListener
	{
		uint16_t port;
		ConnectionType type;
		listen_t OnAccept;
		C server;
		std::string options;

	public:
		TcpListener(listen_t _on_accept)
			: OnAccept(_on_accept) {}

		TcpListener(uint16_t _port, const std::string & options, ConnectionType _type, listen_t _on_accept, bool multiplex, ThreadHub& pool )
			: OnAccept(_on_accept)
		{
			Listen(_port,options,_type,multiplex,pool);
		}

		void Shutdown()
		{
			server.Close();
		}

		void Listen(uint16_t _port,const std::string _options, ConnectionType _type, bool multiplex, ThreadHub& pool)
		{
			EnableNetworking();
			port = _port;
			type = _type;
			options = _options;

			pool.Async([&,multiplex](bool & run)
			{
				if(!server.Listen(port,options))
				{
					std::cout << "Failed to listen: " << server.Error() << std::endl;
					throw std::runtime_error(server.Error());
				}

				while(run)
				{
					C c;
					TcpAddress address;
					if (!server.Accept(c, &address))
					{
						if (!run || !server.Valid())
							break; //Socket was closed gracefully.
						else
						{
							auto msg = server.Error();
							if(msg) std::cout << "Failed to accept: " << msg << std::endl;
							throw std::runtime_error(server.Error());
						}
					}


					try
					{
						OnAccept(&c,address,type,ProgramUniqueId(),multiplex);
					}
					catch(...)
					{
						std::cout << "Exception in OnAccept handler." << std::endl;
					}

					c.Release();
				}
			});
		}
	};
}