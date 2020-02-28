/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include "async.hpp"
#include "common.hpp"
#include "tcp.hpp"
#include "protocols.hpp"

namespace mhttp
{
	class TcpReader
	{
		using C = sock_t;
		io_t OnMessage;
		on_error_t OnError;

	public:
		TcpReader() {}

		TcpReader(io_t _on_message_t, on_error_t _on_error_t, ThreadHub& pool = Threads())
			: OnError(_on_error_t)
			, OnMessage(_on_message_t)
		{
			Pump(pool);
		}

		void Pump(ThreadHub& pool = Threads())
		{
			pool.Async([&](bool & run)
			{
				while(run)
				{
					bool idle = true;
					uint32_t faults = 0;

					auto now = std::chrono::high_resolution_clock::now();

					{
						std::lock_guard<std::mutex> _l(lock);

						for(auto _i : connections)
						{
							bool connection_idle = true;
							uint32_t priority;
							auto & i = * _i;

							if(i.writer_fault) 
								goto FAULT;

							if (!i.Multiplex() && i.ReadLock())
								continue; //If we do not have multiplexing enabled then stop reading while the current object is being handled.

							priority = (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - i.last_message).count();

							if(priority < i.priority)
								continue;

							switch(i.type)
							{
							case ConnectionType::http:
								if( !Http::Read(i,OnMessage,connection_idle) ) 
									goto FAULT;
								break;
							case ConnectionType::writemap32:
							case ConnectionType::message:
								if( !Message::Read(i,OnMessage,connection_idle) ) 
									goto FAULT;
								break;
							case ConnectionType::map32:
								if (!Map32::Read(i, OnMessage, connection_idle))
									goto FAULT;
								break;
							default:
							FAULT:
								i.reader_fault = true;
								faults++;
								break;
							}

							if(!connection_idle)
							{
								idle = false;
								i.last_message = now;
							}
						}

						if(faults)
						{
							for (typename std::list<C*>::iterator i = connections.begin(); i != connections.end(); )
							{
								if ((*i)->reader_fault)
								{
									try
									{
										OnError(*(*i));
									}
									catch(...)
									{
										//std::cout << "Exception in OnError(Reader) handler." << std::endl;
									}

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