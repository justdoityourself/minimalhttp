/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include "async.hpp"

#include "d8u/util.hpp"

namespace mhttp
{
	template < typename C = sock_t >class TcpConnections
	{
		on_disconnect_t OnDisconnect;

	public:
		TcpConnections() {}

		TcpConnections(on_disconnect_t _on_disconnect_t, ThreadHub& pool)
			: OnDisconnect(_on_disconnect_t)
		{
			Pump(pool);
		}

		void Pump(ThreadHub& pool)
		{
			pool.Async([&](bool & run)
			{
				while(run)
				{
					{
						std::lock_guard<std::recursive_mutex> _l(lock);

						for (typename std::list<C>::iterator i = connections.begin(); i != connections.end(); )
						{
							if(i->reader_fault && i->writer_fault && i->pending == 0)
							{
								try
								{
									d8u::trace("Lost Connection:", (*i).uid);
									OnDisconnect(*i);
								}
								catch(...)
								{
									//std::cout << "Exception in OnDisconnect handler." << std::endl;
								}

								if ((*i).priority != -1) std::cout << "Finished Dropping Connection ( " << (*i).uid << " ) " << std::endl;

								i = connections.erase(i);
							}
							else
								++i;
						}
					}

					pool.Delay(1000);
				}
			});
		}

		template <typename _S> C & Create(_S *c, TcpAddress a, ConnectionType t,uint32_t uid)
		{
			std::lock_guard<std::recursive_mutex> _l(lock);

			connections.emplace_back();
			connections.back().Init(*c, a, t, uid);

			historic++;

			return connections.back();
		}

		size_t ConnectionCount() { return connections.size(); }

		C* NewestConnection()
		{
			std::lock_guard<std::recursive_mutex> _l(lock);
			if (!connections.size())
				return (C*)nullptr;

			return &connections.back();
		}

		size_t HistoricConnections() { return historic; }

	private:
		std::atomic<size_t> historic = 0;

		std::recursive_mutex lock;
		std::list<C> connections;
	};
}