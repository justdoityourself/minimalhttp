/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include "common.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <map>
#include <string_view>
#include <string>
#include <memory>
#include <sstream>

#include "../gsl-lite.hpp"
#include "d8u/buffer.hpp"
#include "d8u/encode.hpp"
#include "d8u/hash.hpp"

namespace mhttp
{
	using namespace d8u::buffer;

	class FTP;
	class Websocket;

	class Common
	{
	public:
		template < typename C, typename T > static bool Initialize(C& c, T type) 
		{ 
			switch (type)
			{
			case ConnectionType::ftp:
				return ftp_init(c);
			case ConnectionType::websocket_sz:
			case ConnectionType::websocket: 
				return ws_init(c);
			default:
				return true;
			}
		} //Protocol based on connect details.

		template < typename C, typename M > static bool WriteMessage(C & c, M m,bool & idle, uint32_t bias = 0)
		{
			while(true)
			{
				if(c.write_buffer.size())
				{
					auto r = c.Send( gsl::span<uint8_t>( c.write_buffer.data() + c.write_offset,c.write_buffer.size() - c.write_offset ) );

					if(!r) break;

					if(r == -1)
						return false;

					idle = false;

					c.write_offset += r;
					if(c.write_offset != c.write_buffer.size())
						return true;

					m(c, c.write_count);

					c.write_count++;
					c.write_bytes += c.write_buffer.size();

					c.write_buffer = d8u::sse_vector();
					c.write_offset = 0;
				}
				
				if(!c.TryWrite(c.write_buffer))
					break;

				uint32_t size = (uint32_t)c.write_buffer.size() - bias;
				if (sizeof(uint32_t) != c.Write(gsl::span<uint8_t>((uint8_t*)&size, sizeof(uint32_t))))
					return false;
				if (!size)
					c.priority = 0;
			}

			return true;
		}

		template < typename C, typename M > static bool WriteRaw(C& c, M m, bool& idle)
		{
			while (true)
			{
				if (c.write_buffer.size())
				{
					auto r = c.Send(gsl::span<uint8_t>(c.write_buffer.data() + c.write_offset, c.write_buffer.size() - c.write_offset));

					if (!r) break;

					if (r == -1)
						return false;

					idle = false;

					c.write_offset += r;
					if (c.write_offset != c.write_buffer.size())
						return true;

					m(c, c.write_count);

					c.write_count++;
					c.write_bytes += c.write_buffer.size();

					c.write_buffer = d8u::sse_vector();
					c.write_offset = 0;
				}

				if (!c.TryWrite(c.write_buffer))
					break;
			}

			return true;
		}
	};

	class Http
	{
	public:

		struct Request
		{
			std::string_view type;
			std::string_view path;
			std::string_view proto;
			std::string_view command;

			d8u::sse_vector raw;
			gsl::span<uint8_t> body;

			std::map<std::string_view, std::string_view> headers;
			std::map<std::string_view, std::string_view> parameters;
		};

		static Request ParseRequest(d8u::sse_vector && _m, gsl::span<uint8_t> body)
		{
			Request result;

			Helper m(_m);
			auto cm = m.GetLine();
	
			result.type = cm.GetWord(); 
			Helper path = cm.GetWord(), command;
			result.proto = cm.GetWord(); 
			
			std::tie(path,command) = path.Divide('?');

			result.path = path;
			result.command = command;
			result.body = body;

			Helper line = m.GetLine();
			while(line.size())
			{
				auto k = line.GetWord(), v = line.GetWord();
				result.headers[k] = v;
				line = m.GetLine();
			}

			while (command.size())
			{
				auto k = command.GetWord('='), v = command.GetWord('&');
				result.parameters[k] = v;
			}

			result.raw = std::move(_m);

			return result;
		}

		struct Response
		{
			std::string_view proto;
			std::string_view _status;
			std::string_view msg;

			d8u::sse_vector raw;
			gsl::span<uint8_t> body;

			int status = 0;

			std::map<std::string_view, std::string_view> headers;
		};

		static Response ParseResponse(d8u::sse_vector && _m, gsl::span<uint8_t> body)
		{
			Response result;

			Helper m(_m);
			Helper cm = m.GetLine();
			Helper proto = cm.GetWord(), status = cm.GetWord(), msg= cm.GetWord();

			result.proto = proto;
			result._status = status;
			result.msg = msg;
			result.body = body;

			result.status = status;

			Helper line = m.GetLine();
			while(line.size())
			{
				auto k = line.GetWord(), v = line.GetWord();
				result.headers[k] = v;
				line = m.GetLine();
			}

			result.raw = std::move(_m);

			return result;
		}

		template <typename C > static std::pair<d8u::sse_vector, gsl::span<uint8_t> > ReadResponse(C & c)
		{
			std::pair<d8u::sse_vector, gsl::span<uint8_t> > result;
			bool finished = false;

			//auto start = std::chrono::high_resolution_clock::now();

			while(!finished)
			{
				bool idle = false;

				//auto now = std::chrono::high_resolution_clock::now();

				//auto count = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
				//if(10 < count)
				//	throw std::runtime_error("timeout");

				if(!Read(c,[&](auto & c, d8u::sse_vector && r, gsl::span<uint8_t> m)
				{
					finished = true;
					result = std::make_pair(std::move(r),m);
				},idle,true)) 
					return result;

				if(idle)
					std::this_thread::sleep_for(std::chrono::milliseconds(300));
			}

			return result;
		}

		template < typename C, typename M > static bool Read(C & c, M m,bool & idle,bool talk=false)
		{
			constexpr auto buffer_size = 16 * 1024;

			auto reset = [&](size_t end)
			{
				if (c.read_offset > end)
				{
					/*
						This condition occurs only when the connection is being used for multiplexing.
					*/

					d8u::sse_vector remainder; remainder.reserve(buffer_size);

					remainder.insert(remainder.end(), c.read_buffer.data() + end, c.read_buffer.data() + c.read_offset);

					auto result = std::move(c.read_buffer); 
					result.resize(end);

					c.read_offset = remainder.size();
					c.read_buffer = std::move(remainder);

					return std::move(result);
				}
				else
				{
					c.read_offset = 0;
					c.read_buffer.resize(end);
					return std::move(c.read_buffer);
				}
			};	

			while(true)
			{
				if (!c.read_buffer.size())
					c.read_buffer.resize(buffer_size);

CONTINUE:

				auto rem = (int32_t) c.read_buffer.size() - (int32_t) c.read_offset;

				if(rem <= 0)
				{
					c.read_buffer.resize(c.read_buffer.size() * 2);
					continue;
				}

				int r = c.Receive( gsl::span<uint8_t> ( c.read_buffer.data() + c.read_offset,rem ) );

				if(!r) 
					break;

				if(r == -1)
					return false;

				idle = false;

				c.read_offset += r;
				size_t end = c.read_offset;

RETRY:
				size_t start = 0;

				uint8_t * ptr = c.read_buffer.data();

				for (; start + 3 < end; start++)
				{
					auto t = *ptr++;
					if (  t == '\r' || t == '\n' )
					{
						if( (*ptr == '\r' || *ptr == '\n') && (*(ptr+1) == '\r' || *(ptr+1) == '\n') && (*(ptr+2) == '\r' || *(ptr+2) == '\n') )
						{
							size_t al = 0;

							static std::string_view target = "content-length:";
							auto it = std::search( c.read_buffer.data(), c.read_buffer.data() + start, target.begin(), target.end(), [](char ch1, char ch2) { return std::tolower(ch1) == ch2; } );

							if (it != c.read_buffer.data() + start)
							{
								auto off = std::distance(c.read_buffer.data(), it) + 15;
								al = std::stoi((const char*)(c.read_buffer.data() + off));
								if (al > 1024 * 1024 * 8)
									return false;
							}

							size_t message_size = start + 4 + al;

							if(message_size > end)
							{
								if (message_size > c.read_buffer.size())
									c.read_buffer.resize(message_size);

								goto CONTINUE; //This read is not complete.
							}
							else
							{
								auto single = reset(message_size);
								gsl::span<uint8_t> body(single.data() + start + 4, al);

								m(c,std::move(single),body);

								c.read_count++;
								c.read_bytes += single.size();

								if (c.read_buffer.size())
									goto RETRY; //Another request has been read and is waiting.

								break;
							}
						}
					}
						
				}
			}

			return true;
		}

		template < typename C, typename M > static bool Write(C & c, M m,bool & idle)
		{
			return Common::WriteRaw(c,m,idle);
		}
	};

	class Websocket
	{
	public:
		static constexpr uint8_t ws_text = 129;
		static constexpr uint8_t ws_binary = 130;
		static constexpr uint8_t ws_disconnect = 136;

		static constexpr uint8_t ws_small_message = 126;
		static constexpr uint8_t ws_large_message = 127;

		template < typename C > static bool Initialize(C& c)
		{
			bool valid = true;
			try
			{
				std::array<uint8_t, 8 * 1024> data;
				int length = 0;

				while (true)
				{
					length += c.Receive(gsl::span<uint8_t>(data.data() + length, (uint32_t)data.size() - length));

					if (length > 4)
					{
						if (*((data.data() + length) - 4) == '\r' 
								&& *((data.data() + length) - 3) == '\n' 
								&& *((data.data() + length) - 2) == '\r' 
								&& *((data.data() + length) - 1) == '\n')
							break;
					}

					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}

				std::string response;

				std::stringstream stream((char*)&(data)[0]);
				std::string line;
				std::map<std::string, std::string> lookup;

				while (std::getline(stream, line, '\n'))
				{
					size_t pos = line.find(":", 0);
					std::string key, value;
					key = line.substr(0, pos);
					value = line.substr(pos + 2, -1);
					lookup[key] = value;
				}

				std::string security = lookup["Sec-WebSocket-Key"];
				security.resize(security.find('\r'));
				security += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

				std::array<uint8_t, 20> sha1_hash;
				d8u::hash::sha1(security,sha1_hash);

				response +=
					"HTTP/1.1 101 Web Socket Protocol Handshake\r\n" \
					"Upgrade: Websocket\r\n" \
					"Connection: Upgrade\r\n" \
					"Sec-WebSocket-Accept: ";
				response += d8u::encode::base64(sha1_hash);
				response += "\r\n\r\n";

				c.Write(response);
			}
			catch (...)
			{
				valid = false;
			}

			return valid;
		}

		template < typename C, typename M > static bool Read(C& c, M m, bool& idle)
		{
			while (true)
			{
				if (c.read_buffer.size())
				{
					int r = c.Receive(gsl::span<uint8_t>(c.read_buffer.data() + c.read_offset, c.read_buffer.size() - c.read_offset));
					if (!r) break;

					if (r == -1)
					{
						std::cout << "Socket Fault. (6)" << std::endl;
						return false;
					}

					idle = false;

					c.read_offset += r;
					if (c.read_offset != c.read_buffer.size())
						return true;

					uint8_t control_code = c.read_buffer.data()[0];
					bool final_frame = ((control_code >> 7) == 0x1);
					uint8_t opcode = control_code & 0xf;

					int ml = (int)c.read_buffer.data()[1] & ws_large_message;

					uint8_t* mask;
					uint32_t message_offset;

					if (ml == ws_small_message)
					{
						mask = c.read_buffer.data() + 4;
						message_offset = 8;
					}

					else if (ml == ws_large_message)
					{
						mask = c.read_buffer.data() + 10;
						message_offset = 14;
					}
					else
					{
						mask = c.read_buffer.data() + 2;
						message_offset = 6;
					}

					ml = (int)(c.read_buffer.size() - message_offset);

					if (*mask)
						for (int j = 0; j < ml; j++)
							c.read_buffer.data()[j + message_offset] ^= mask[j % 4];

					c.read_count++;
					c.read_bytes += c.read_buffer.size();
					m(c, std::move(c.read_buffer), gsl::span<uint8_t>(c.read_buffer.data() + message_offset, c.read_buffer.size() - message_offset));

					c.read_offset = 0;
				}
				else
				{
					uint16_t header;
					uint16_t sm;
					uint64_t lg;

					int control_length = c.Receive(gsl::span<uint8_t>((uint8_t*)&header, sizeof(uint16_t)));
					if (!control_length) break;

					if (control_length != 2)
					{
						std::cout << "Header Fault. (5)" << std::endl;
						return false;
					}

					uint8_t control_code = ((uint8_t*)(&header))[0];
					bool final_frame = ((control_code >> 7) == 0x1);
					uint8_t opcode = control_code & 0xf;

					if (8 == opcode)
					{
						std::cout << "Client Side Termination. (4)" << std::endl;
						return false;
					}

					//if (9 == opcode); //Unsupported websocket ping

					int ml = (int)((uint8_t*)(&header))[1] & ws_large_message;

					if (ml == ws_small_message)
					{
						control_length += c.Receive(gsl::span<uint8_t>((uint8_t*)&sm, sizeof(uint16_t)));

						if (control_length != 4)
						{
							std::cout << "Header Fragmented. (1)" << std::endl;
							return false;
						}

						uint8_t* i = (uint8_t*)&sm;
						ml = 0;

						for (uint32_t j = 0; j < 2; j++)
							ml += i[j] << (8 * (1 - j));
					}

					else if (ml == ws_large_message)
					{
						control_length += c.Receive(gsl::span<uint8_t>((uint8_t*)&lg, sizeof(uint64_t)));

						if (control_length != 10)
						{
							std::cout << "Header Fragmented. (2)" << std::endl;
							return false;
						}

						uint8_t* i = (uint8_t*)&lg;
						ml = 0;

						for (uint32_t j = 0; j < 8; j++)
							ml += i[j] << (8 * (7 - j));
					}

					if (ml > 10 * 1024 * 1024) //Max message segment
					{
						std::cout << "Message Exceeded Maximum. (3)" << std::endl;
						return false;
					}

					idle = false;

					c.read_buffer.resize(ml + 4);
					*((uint16_t*)c.read_buffer.data()) = header;
					if (control_length == 4) *((uint16_t*)(c.read_buffer.data()+2)) = sm;
					else if (control_length == 10) *((uint64_t*)(c.read_buffer.data() + 2)) = lg;
					c.read_offset = control_length;
				}
			}

			return true;
		}

		template < typename C, typename M > static bool Write(C& c, M m, bool& idle)
		{
			return Common::WriteRaw(c, m, idle);
		}
	};

	class FTP
	{
	public:

		template < typename C > static bool Initialize(C& c)
		{
			c.Write(std::string_view("220 Template FTP Server Core Ready.\r\n"));
			return true;
		}

		template < typename C, typename M > static bool Read(C& c, M m, bool& idle, bool talk = false)
		{
			constexpr auto buffer_size = 16 * 1024;

			auto reset = [&](size_t end)
			{
				if (c.read_offset > end)
				{
					/*
						This condition occurs only when the connection is being used for multiplexing.
					*/

					d8u::sse_vector remainder; remainder.reserve(buffer_size);

					remainder.insert(remainder.end(), c.read_buffer.data() + end, c.read_buffer.data() + c.read_offset);

					auto result = std::move(c.read_buffer);
					result.resize(end);

					c.read_offset = remainder.size();
					c.read_buffer = std::move(remainder);

					return std::move(result);
				}
				else
				{
					c.read_offset = 0;
					c.read_buffer.resize(end);
					return std::move(c.read_buffer);
				}
			};

			while (true)
			{
				if (!c.read_buffer.size())
					c.read_buffer.resize(buffer_size);

				auto rem = (int32_t)c.read_buffer.size() - (int32_t)c.read_offset;

				if (rem <= 0)
				{
					c.read_buffer.resize(c.read_buffer.size() * 2);
					continue;
				}

				int r = c.Receive(gsl::span<uint8_t>(c.read_buffer.data() + c.read_offset, rem));

				if (!r)
					break;

				if (r == -1)
					return false;

				idle = false;

				c.read_offset += r;
				size_t end = c.read_offset;

			RETRY:
				size_t start = 0;

				uint8_t* ptr = c.read_buffer.data();

				for (; start + 1 < end; start++)
				{
					if (*ptr++ == '\r' && *(ptr) == '\n')
					{
						auto message_size = start + 2;

						auto single = reset(message_size);
						gsl::span<uint8_t> body(single.data(),start);

						m(c, std::move(single), body);

						c.read_count++;
						c.read_bytes += single.size();

						if (c.read_buffer.size())
							goto RETRY; //Another request has been read and is waiting.

						break;
					}
				}
			}

			return true;
		}

		template < typename C, typename M > static bool Write(C& c, M m, bool& idle)
		{
			return Common::WriteRaw(c, m, idle);
		}
	};

	class Message
	{
	public:
		template < typename C, typename M > static bool Read(C & c, M m,bool & idle)
		{
			while(true)
			{
				if(c.read_buffer.size())
				{
					auto r = c.Receive( gsl::span<uint8_t>(c.read_buffer.data() + c.read_offset, c.read_buffer.size() - c.read_offset) );

					if(!r) 
						break;

					if(r == -1)
						return false; //error

					idle = false;

					c.read_offset += r;

					if(c.read_offset != c.read_buffer.size())
						return true;

					m( c, std::move(c.read_buffer) , gsl::span<uint8_t>(c.read_buffer) );

					c.read_count++;
					c.read_bytes += c.read_buffer.size();

					c.read_offset = 0;

					if(!c.Multiplex()) break;
				}
				else
				{
					uint32_t ml=0;
					int header_length = c.ReadIf( gsl::span<uint8_t>( (uint8_t*) &ml, sizeof(uint32_t) ) );

					if(!header_length) 
						break;

					if(header_length != 4)
						return false;

					if (!ml)
						m(c, d8u::sse_vector(), gsl::span<uint8_t>()); //Ping
					else if(ml > 8*1024*1024)
						return false;

					idle = false;

					c.read_buffer.resize(ml);
				}
			}

			return true;
		}

		template < typename C, typename M > static bool Write(C & c, M m,bool & idle)
		{
			return Common::WriteMessage(c,m,idle);
		}
	};

	class ISCSI
	{
	public:
		template < typename C, typename M > static bool Read(C& c, M m, bool& idle)
		{
			while (true)
			{
				if (c.read_buffer.size())
				{
					auto r = c.Receive(gsl::span<uint8_t>(c.read_buffer.data() + c.read_offset, c.read_buffer.size() - c.read_offset));

					if (!r)
						break;

					if (r == -1)
						return false; //error

					idle = false;

					c.read_offset += r;

					if (c.read_offset != c.read_buffer.size())
						return true;

					m(c, std::move(c.read_buffer), gsl::span<uint8_t>(c.read_buffer));

					c.read_count++;
					c.read_bytes += c.read_buffer.size();

					c.read_offset = 0;

					if (!c.Multiplex()) break;
				}
				else
				{
					static constexpr size_t header_size = 48;

					uint64_t _peek = 0;
					int header_length = c.ReadIf(gsl::span<uint8_t>((uint8_t*)&_peek, sizeof(uint64_t)));

					if (!header_length)
						break;

					if (header_length != 8)
						return false;

					gsl::span<uint8_t> peek((uint8_t*)&_peek,8);
					uint8_t ahsl = peek[4];
					int dataseg = peek[5] << 16 | peek[6] << 8 | peek[7];

					size_t ml = header_size + ahsl + dataseg;
					if(ml % 4)
						ml += 4 - ml % 4;

					if (ml > 8 * 1024 * 1024)
						return false;

					idle = false;

					c.read_buffer.resize(ml);
					c.read_offset = 8;
					std::copy(peek.begin(),peek.end(),c.read_buffer.begin());
				}
			}

			return true;
		}

		template < typename C, typename M > static bool Write(C& c, M m, bool& idle)
		{
			return Common::WriteRaw(c, m, idle);
		}
	};

	class Map32
	{
	public:

		template < typename T > static std::pair<uint32_t, gsl::span<uint8_t>> DecodeHeader(const T& t)
		{
			return std::make_pair(*((uint32_t*)t.data()), gsl::span<uint8_t>(((uint8_t*)t.data()) + sizeof(uint32_t), t.size() - sizeof(uint32_t)));
		}

		/*template < typename T > static d8u::sse_vector MakeHeader(const T& t, size_t sz)
		{
			return std::make_pair(*((uint32_t*)t.data()), gsl::span<uint8_t>(((uint8_t*)t.data()) + sizeof(uint32_t), t.size() - sizeof(uint32_t)));
		}*/

		template < typename C, typename M > static bool Read(C& c, M m, bool& idle)
		{
			while (true)
			{
				if (c.read_buffer.size())
				{
					auto r = c.Receive(gsl::span<uint8_t>(c.read_buffer.data() + c.read_offset, c.read_buffer.size() - c.read_offset));

					if (!r)
						break;

					if (r == -1)
						return false; //error

					idle = false;

					c.read_offset += r;

					if (c.read_offset == 4)
					{
						if (*((uint32_t*)c.read_buffer.data()) == 0)
						{
							m(c, d8u::sse_vector(), gsl::span<uint8_t>());
							c.read_offset = 0;
							c.read_buffer = d8u::sse_vector();
							return true;
						}
					}

					if (c.read_offset != c.read_buffer.size())
						return true;

					m(c, std::move(c.read_buffer), gsl::span<uint8_t>(c.read_buffer));

					c.read_count++;
					c.read_bytes += c.read_buffer.size();

					c.read_offset = 0;

					if (!c.Multiplex()) break;
				}
				else
					c.read_buffer.resize(32 + sizeof(uint32_t));
			}

			return true;
		}

		template < typename C, typename M > static bool Write(C& c, M m, bool& idle)
		{
			while (true)
			{
				if (c.map.size())
				{
					auto r = c.Send(gsl::span<uint8_t>(c.map.data() + c.write_offset, c.map.size() - c.write_offset));

					if (!r) break;

					if (r == -1)
						return false;

					idle = false;

					c.write_offset += r;
					if (c.write_offset != c.map.size())
						return true;

					m(c, c.write_count);

					c.write_count++;
					c.write_bytes += c.map.size();

					c.map = gsl::span<uint8_t>();
					c.write_offset = 0;
				}

				if (!c.TryMap(c.map))
					break;

				uint32_t size = (uint32_t)c.map.size();
				if (sizeof(uint32_t) != c.Write(gsl::span<uint8_t>((uint8_t*)&size, sizeof(uint32_t))))
					return false;
			}

			return true;
		}

		template < typename C, typename M > static bool ClientWrite(C& c, M m, bool& idle)
		{
			return Common::WriteMessage(c, m, idle, 32);
		}
	};


	template <typename C, typename M> bool DoWrite(C &i, M m, bool & idle)
	{
		switch (i.type)
		{
		case ConnectionType::websocket_sz:
		case ConnectionType::websocket:
			return Websocket::Write(i, m, idle);
		case ConnectionType::iscsi:
			return ISCSI::Write(i, m, idle);
		case ConnectionType::ftp:
		case ConnectionType::ftp_data:
			return FTP::Write(i, m, idle);
		case ConnectionType::http:
			return Http::Write(i, m, idle);

		case ConnectionType::readmap32:
		case ConnectionType::message:
			return Message::Write(i, m, idle);

		case ConnectionType::writemap32:
		case ConnectionType::map32:
			return Map32::Write(i, m, idle);

		case ConnectionType::map32client:
			return Map32::ClientWrite(i, m, idle);

		default:
			return false;
		}
	}

	template <typename C, typename M> bool DoRead(C& i, M m, bool& idle)
	{
		switch (i.type)
		{
		case ConnectionType::websocket_sz:
		case ConnectionType::websocket:
			return Websocket::Read(i, m, idle);
		case ConnectionType::iscsi:
			return ISCSI::Read(i, m, idle);
		case ConnectionType::ftp:
		case ConnectionType::ftp_data:
			return FTP::Read(i, m, idle);
		case ConnectionType::http:
			return Http::Read(i, m, idle);

		case ConnectionType::map32client:
		case ConnectionType::writemap32:
		case ConnectionType::message:
			return Message::Read(i, m, idle);

		case ConnectionType::readmap32:
		case ConnectionType::map32:
			return Map32::Read(i, m, idle);

		default:
			return false;
		}
	}

	template < typename C > bool ftp_init(C& c)
	{
		return FTP::Initialize(c);
	}

	template < typename C > bool ws_init(C& c)
	{
		return Websocket::Initialize(c);
	}
};