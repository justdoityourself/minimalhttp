/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include "async.hpp"
#include "common.hpp"
#include "tcp.hpp"
#include "protocols.hpp"

namespace mhttp
{
	template < typename C, typename on_message_t, typename on_error_t >class TcpWriter
	{
		on_message_t OnMessage;
		on_error_t OnError;

	public:
		TcpWriter(on_message_t _on_message_t, on_error_t _on_error_t, ThreadHub& pool = Threads())
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
					{
						std::lock_guard<std::mutex> _l(lock);

						for(auto _i : connections)
						{
							auto & i = * _i;

							if(i.reader_fault) 
								goto FAULT;

							switch(i.type)
							{
							case ConnectionType::http:
								if(!Http::Write(i,OnMessage,idle)) 
									goto FAULT;
								break;
							case ConnectionType::message:
								if(!Message::Write(i,OnMessage,idle)) 
									goto FAULT;
								break;
							case ConnectionType::map32:
								if (!Map32::Write(i, OnMessage, idle))
									goto FAULT;
								break;
							default:
							FAULT:
								i.writer_fault = true;
								faults++;
								break;
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