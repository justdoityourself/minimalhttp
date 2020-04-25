/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include "tcpserver.hpp"
#include "client.hpp"

#include "d8u/string_switch.hpp"

#include "d8u/buffer.hpp"

#include <ctime>

namespace mhttp
{
	using namespace d8u;
	using namespace d8u::buffer;

	class FtpConnection : public sock_t
	{
		static constexpr std::string_view sv501 = "501 I didn't like what you said\r\n";
		static constexpr std::string_view sv502 = "502 I can't do that yet\r\n";
		static constexpr std::string_view sv331 = "331 Username controls folder root contents\r\n";
		static constexpr std::string_view sv230 = "230 Password evaluated only if required\r\n";
		static constexpr std::string_view sv215 = "215 Why would I tell you that\r\n";
		static constexpr std::string_view sv226 = "226 Transfer Complete\r\n";
		static constexpr std::string_view sv200 = "200 ";
		static constexpr std::string_view sv257b = "257 \"";
		static constexpr std::string_view sv257e = "\" is the current directory\r\n";
		static constexpr std::string_view sv211 = "211-Features:\r\n MDTM\r\n REST STREAM\r\n SIZE\r\n PWD\r\n EPSV\r\n RETR\r\n211 End\r\n";
		static constexpr std::string_view sv_end = "\r\n";
		static constexpr std::string_view sv_error = "Error: ";
		static constexpr std::string_view svc = ",";

	public:
		FtpConnection() {}

		FtpConnection(std::string_view host, ConnectionType type = ConnectionType::ftp)
		{
			sock_t::Connect(host, type);
		}

		//API doesn't need to be this verbose.
		void Ftp501()
		{
			AsyncWrite(join_memory(sv501));
		}

		void Ftp502()
		{
			AsyncWrite(join_memory(sv502));
		}

		void Ftp331()
		{
			AsyncWrite(join_memory(sv331));
		}

		void Ftp230()
		{
			AsyncWrite(join_memory(sv230));
		}

		void Ftp215()
		{
			AsyncWrite(join_memory(sv215));
		}

		void Ftp226()
		{
			AsyncWrite(join_memory(sv226));
		}

		void Ftp211()
		{
			AsyncWrite(join_memory(sv211));
		}

		void Ftp150(std::string_view msg)
		{
			AsyncWrite(join_memory(std::string_view("150 "),msg,sv_end));
		}

		void Ftp250(std::string_view msg)
		{
			AsyncWrite(join_memory(std::string_view("250 "), msg, sv_end));
		}

		void FtpError(std::string_view msg)
		{
			AsyncWrite(join_memory(sv_error,msg,sv_end));
		}

		template < typename ... t_args >  void Ftp200(t_args...msgs)
		{
			AsyncWrite(join_memory(sv200, msgs..., sv_end));
		}

		void FtpPWD(std::string_view cur)
		{
			AsyncWrite(join_memory(sv257b, cur, sv257e));
		}

		void Ftp227(std::string_view _ip, std::string_view _port)
		{
			Helper ip(_ip);
			int q1 = ip.GetWord('.');
			int q2 = ip.GetWord('.');
			int q3 = ip.GetWord('.');
			int q4 = ip.GetWord('.');
			int port = Helper(_port);

			AsyncWrite(join_memory(std::string_view("227 Entering Passive Mode("), std::to_string(q1),svc, std::to_string(q2), svc, std::to_string(q3), svc, std::to_string(q4), svc, std::to_string(port/256),svc,to_string(port%256), std::string_view(")\r\n")));
		}

		FtpConnection* data = nullptr;

		std::string user;
		std::string password;
		std::string cwd = "/";
		std::string type = "I";
	};

	bool FtpBaseCommands(FtpConnection& c, std::string_view _request, std::string_view ip, std::string_view port)
	{
		Helper request(_request);
		Helper cmd = request.GetWord();

		switch (switch_t(cmd))
		{
		case switch_t("PORT"):
		case switch_t("AUTH"):
			c.Ftp502();
			return true;

		case switch_t("USER"):
			c.user = request.GetWord();
			c.Ftp331();
			return true;

		case switch_t("PASS"):
			c.password = request.GetWord();
			c.Ftp230();
			return true;

		case switch_t("SYST"):
			c.Ftp215();
			return true;

		case switch_t("FEAT"):
			c.Ftp211();
			return true;

		case switch_t("PWD"):
			c.FtpPWD(c.cwd);
			return true;

		case switch_t("CWD"):
			c.cwd = request.GetWord();
			c.Ftp250("Changed working directory.");
			return true;

		case switch_t("TYPE"):
			c.type = request.GetWord();
			c.Ftp200(std::string_view("Type set to "), c.type);
			return true;

		case switch_t("PASV"):
			c.Ftp227(ip,port);
			return true;

		default:
			return false;
		}
	}

	template < typename ENUM, typename IO > bool FtpIoCommands(TcpServer<FtpConnection>* server, FtpConnection& client, std::string_view _request, ENUM&& on_enum, IO&& on_io)
	{
		Helper request(_request);
		Helper cmd = request.GetWord();

		switch (switch_t(cmd))
		{
		case switch_t("LIST"):
			client.data = server->NewestConnection(); //This isn't 100%, timing issue possible with multiple clients.

			if (!client.data)
				throw "Where did the data connection go?";

			client.Ftp150("Sending listing to data connection...");
			on_enum(client.cwd, [&](bool dir, size_t size, uint64_t _time, std::string_view name)
			{
				char szYearOrHour[6] = "";

				static const char* pszMonth[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

				struct tm* t = gmtime((time_t*)&_time); // UTC Time
				if (time(NULL) - _time > 180 * 24 * 60 * 60)
					sprintf(szYearOrHour, "%5d", t->tm_year + 1900);
				else
					sprintf(szYearOrHour, "%02d:%02d", t->tm_hour, t->tm_min);

				if (dir)
					client.data->Write(join_memory(std::string_view("dr-r-r 1 Default Anonymous "),
						std::to_string(size),
						std::string_view(" "), std::string_view(pszMonth[t->tm_mon]),
						std::string_view(" "), std::to_string(t->tm_mday),
						std::string_view(" "), std::string_view(szYearOrHour),
						std::string_view(" "), name, std::string_view("/\r\n")));
				else
					client.data->Write(join_memory(std::string_view("-r-r-r- 1 Default Anonymous "),
						std::to_string(size),
						std::string_view(" "), std::string_view(pszMonth[t->tm_mon]),
						std::string_view(" "), std::to_string(t->tm_mday),
						std::string_view(" "), std::string_view(szYearOrHour),
						std::string_view(" "), name, std::string_view("\r\n")));
			});

			client.data->Write(std::string_view("\r\n"));

			client.Ftp226();

			client.data->Close();
			client.data = nullptr;
			return true;
		case switch_t("RETR"):
			client.data = server->NewestConnection(); //This isn't 100%, timing issue possible.

			if (!client.data)
				throw "Where did the data connection go?";

			client.Ftp150("Sending file contents...");

			on_io(client.cwd + "\\" + std::string(request.GetWord()), [&](const auto& block)
			{
				client.data->Write(block);
			});

			client.Ftp226();

			client.data->Close();
			client.data = nullptr;
			return true;
		default:
			return false;
		}
	}

	template < typename ENUM, typename IO > auto make_ftp_server(const string_view ip,const string_view port1, std::string_view port2, ENUM&& on_enum, IO&& on_io, size_t threads = 1, bool mplex = false)
	{
		auto on_connect = [](const auto& c)  { return make_pair(true, true); };
		auto on_disconnect = [](const auto& c) {};
		auto on_error = [](const auto& c) {};
		auto on_write = [](const auto& mc, const auto& c) {};

		return TcpServer<FtpConnection>(std::make_tuple( std::make_pair((uint16_t)stoi(port1.data()), ConnectionType::ftp), std::make_pair((uint16_t)stoi(port2.data()), ConnectionType::ftp_data)), on_connect, on_disconnect, [ip = std::string(ip),port2 = std::string(port2), on_io = std::move(on_io), on_enum = std::move(on_enum)](auto* _server, auto* _client, auto&& _request, auto body, auto* mplex) mutable
		{
			auto server = (TcpServer<FtpConnection> * )_server;
			auto& client = *(FtpConnection*)_client;

			try
			{
				if (!FtpBaseCommands(client, std::string_view((char*)body.data(), body.size()), ip, port2))
				{
					if(!FtpIoCommands(server,client, std::string_view((char*)body.data(), body.size()),on_enum,on_io))
						client.Ftp501();
				}
			}
			catch (...)
			{
				client.Ftp501();
			}
		}, on_error, on_write, mplex, { threads });
	}

	using on_ftp_enum_result = std::function < void(bool,size_t,uint64_t,std::string_view)>;
	using on_ftp_io_result = std::function < void(gsl::span<uint8_t>)>;

	using on_ftp_enum = std::function < void(std::string_view, on_ftp_enum_result)>;
	using on_ftp_io = std::function < void(std::string_view, on_ftp_io_result)>;

	class FtpServer : public TcpServer<>
	{
		on_ftp_enum on_enum;
		on_ftp_io on_io;

		std::string ip;
		std::string port1;
		std::string port2;
	public:
		FtpServer(on_ftp_enum _on_enum, on_ftp_io _on_io, TcpServer::Options o = TcpServer::Options())
			: on_enum(_on_enum)
			, on_io(_on_io)
			, TcpServer([&](auto* _server, auto* _client, auto&& _request, auto body, auto* mplex)
		{
			auto server = (TcpServer<FtpConnection>*)_server;
			auto& client = *(FtpConnection*)_client;

			try
			{
				if (!FtpBaseCommands(client, std::string_view((char*)body.data(), body.size()), ip, port2))
				{
					if (!FtpIoCommands(server, client, std::string_view((char*)body.data(), body.size()), on_enum, on_io))
						client.Ftp501();
				}
			}
			catch (...)
			{
				client.Ftp501();
			}
		}, o) { }

		FtpServer(const std::string_view _ip, const std::string_view _port1, std::string_view _port2, on_ftp_enum _on_enum, on_ftp_io _on_io, bool mplex = false, TcpServer::Options o = TcpServer::Options())
			: FtpServer(_on_enum,_on_io, o)
		{
			Open(_ip,_port1,_port2,"",mplex);
		}

		void Open(const std::string_view _ip, const std::string_view _port1, std::string_view _port2, const std::string& options = "", bool mplex = false)
		{
			ip = _ip;
			port1 = _port1;
			port2 = _port2;

			TcpServer::Open(std::make_tuple(
										std::make_pair((uint16_t)stoi(port1.data()), ConnectionType::ftp), 
										std::make_pair((uint16_t)stoi(port2.data()), ConnectionType::ftp_data)), mplex);
		}
	};
}