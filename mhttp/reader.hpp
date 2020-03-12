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

		TcpReader(io_t _on_message_t, on_error_t _on_error_t, ThreadHub& pool)
			: OnError(_on_error_t)
			, OnMessage(_on_message_t)
		{
			Pump(pool);
		}

		void Pump(ThreadHub& pool)
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
							{
								i.reader_fault = true;
								faults++;
								continue;
							}

							if (!i.Multiplex() && i.ReadLock())
								continue; //If we do not have multiplexing enabled then stop reading while the current object is being handled.

							priority = (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - i.last_message).count();

							if(priority < i.priority)
								continue;

							if(!DoRead(i,OnMessage,connection_idle))
							{
								i.reader_fault = true;
								faults++;
							}

							if(!connection_idle)
							{
								idle = false;
								i.last_message = now;
							}
							else if(priority > 60 * 1000 && i.Idle())
							{
								if (i.type >= ConnectionType::message)
								{
									//Sending an empty message will confirm that the connection is truely closed.
									//This lets us long poll without timeing out:
									///

									i.last_message = now;
									i.Ping();
								}
								else
								{
									//Drop connection after 60 seconds of idle
									i.priority = -1; //Indicates a timeout
									i.reader_fault = true;
									faults++;
								}
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

									if((*i)->priority != -1) std::cout << "Reader Dropping Connection ( " << (*i)->uid << " ) " << std::endl;

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