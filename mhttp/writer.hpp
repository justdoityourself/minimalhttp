/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include <chrono>

#include "async.hpp"
#include "common.hpp"
#include "tcp.hpp"
#include "protocols.hpp"

namespace mhttp
{
	class TcpWriter
	{
		using C = sock_t;
		on_write_t OnMessage;
		on_error_t OnError;

	public:
		TcpWriter() {}

		TcpWriter(on_write_t _on_message_t, on_error_t _on_error_t, ThreadHub& pool )
			: OnError(_on_error_t)
			, OnMessage(_on_message_t)
		{
			Pump(pool);
		}

		void Pump(ThreadHub& pool )
		{
			pool.Async([&](bool & run)
			{
				while(run)
				{
					bool idle = true;
					uint32_t faults = 0;
					{
						std::lock_guard<std::mutex> _l(lock);

						auto now = std::chrono::high_resolution_clock::now();

						for(auto _i : connections)
						{
							bool connection_idle = true;
							auto & i = * _i;

							if (i.reader_fault || !DoWrite(i, OnMessage, connection_idle))
							{
								i.writer_fault = true;
								faults++;
							}

							if (!connection_idle)
							{
								idle = false;
								i.last_message = now;
							}
						}

						if(faults)
						{
							for (typename std::list<C*>::iterator i = connections.begin(); i != connections.end(); )
							{
								if ((*i)->writer_fault)
								{
									try
									{
										OnError(*(*i));
									}
									catch(...)
									{
										//std::cout << "Exception in OnError(Write) handler." << std::endl;
									}

									if ((*i)->priority != -1) 
										std::cout << "Writer Dropping Connection ( " << (*i)->uid << " ) " << std::endl;

									i = connections.erase(i);
									if(--faults == 0)
										break;
								}
								else
									++i;
							}
						}
					}

					if(idle)
						pool.Delay(100);
				}
			});
		}

		void Manage(C * c)
		{
			std::lock_guard<std::mutex> _l(lock);

			connections.push_back(c);
		}

	private:
		std::mutex lock;
		std::list<C*> connections;
	};
}