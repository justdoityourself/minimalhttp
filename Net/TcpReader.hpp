/* Copyright D8Data 2019 */

#pragma once

#include "Helper/Async.hpp"
#include "Common.hpp"
#include "Tcp.hpp"
#include "TcpProtocols.hpp"

namespace atk
{
	template < typename C, typename on_message_t, typename on_error_t >class TcpReader
	{
		on_message_t OnMessage;
		on_error_t OnError;
	public:
		TcpReader(on_message_t _on_message_t, on_error_t _on_error_t, ThreadHub& pool = Threads()):OnError(_on_error_t),OnMessage(_on_message_t){Pump(pool);}

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

							if(i.writer_fault) goto FAULT;

							priority = (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - i.last_message).count();

							if(priority < i.priority)
								continue;

							switch(i.type)
							{
							case ConnectionType::newline_pair:
								if(!Newline::Read(i,OnMessage,connection_idle)) goto FAULT;
								break;
							case ConnectionType::http:
							case ConnectionType::httpb:
								if(!Http::Read(i,OnMessage,connection_idle)) goto FAULT;
								break;
							case ConnectionType::message:
								if(!Message::Read(i,OnMessage,connection_idle)) goto FAULT;
								break;
							case ConnectionType::web:
							case ConnectionType::websz:
								if(!Websocket::Read(i,OnMessage,connection_idle)) goto FAULT;
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