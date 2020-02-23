/* Copyright D8Data 2017 */

#pragma once

#include "Helper/Async.hpp"
#include "Common.hpp"
#include "Tcp.hpp"
#include "TcpProtocols.hpp"

namespace atk
{
	template < typename C, typename on_message_t, typename on_error_t >class TcpWriter
	{
		on_message_t OnMessage;
		on_error_t OnError;
	public:
		TcpWriter(on_message_t _on_message_t, on_error_t _on_error_t, ThreadHub& pool = Threads()):OnError(_on_error_t),OnMessage(_on_message_t){Pump(pool);}

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

							if(i.reader_fault) goto FAULT;

							switch(i.type)
							{
							case ConnectionType::newline_pair:
								if(!Newline::Write(i,OnMessage,idle)) goto FAULT;
								break;
							case ConnectionType::http:
							case ConnectionType::httpb:
								if(!Http::Write(i,OnMessage,idle)) goto FAULT;
								break;
							case ConnectionType::message:
								if(!Message::Write(i,OnMessage,idle)) goto FAULT;
								break;
							case ConnectionType::web:
							case ConnectionType::websz:
								if(!Websocket::Write(i,OnMessage,idle)) goto FAULT;
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