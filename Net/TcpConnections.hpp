/* Copyright D8Data 2017 */

#pragma once

namespace atk
{
	template < typename C, typename on_disconnect_t, typename on_error_t > class TcpConnections
	{
		on_disconnect_t OnDisconnect;
	public:
		TcpConnections(on_disconnect_t _on_disconnect_t, ThreadHub& pool = Threads()):OnDisconnect(_on_disconnect_t){Pump(pool);}

		void Pump(ThreadHub& pool = Threads())
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
									OnDisconnect(*i);
								}
								catch(...)
								{
									//std::cout << "Exception in OnDisconnect handler." << std::endl;
								}

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

			connections.emplace_back(*c,a,t,uid);

			return connections.back();
		}

		uint32_t ConnectionCount() { return (uint32_t) connections.size(); }

	private:
		std::recursive_mutex lock;
		std::list<C> connections;
	};
}