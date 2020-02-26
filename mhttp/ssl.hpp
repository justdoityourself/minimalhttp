/* Copyright (C) 2020 D8DATAWORKS - All Rights Reserved */

#pragma once

#include "common.hpp"

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <openssl/x509.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>

#include <iostream>
#include <iterator>

#pragma comment (lib, "crypt32")

/* Reading Material
https://stackoverflow.com/questions/11705815/client-and-server-communication-using-ssl-c-c-ssl-protocol-dont-works
http://simplestcodings.blogspot.com.br/2010/08/secure-server-client-using-openssl-in-c.html
https://wiki.openssl.org/index.php/Simple_TLS_Server

errors:
https://wiki.openssl.org/index.php/Manual:ERR_get_error(3)
https://www.openssl.org/docs/manmaster/man3/ERR_error_string.html
*/

namespace mhttp
{
	void MakeCert(const std::string key, const std::string cert,const std::string & password)
	{
		EVP_PKEY * pkey = nullptr;
		RSA * rsa = nullptr;
		X509 * x509 = nullptr;

		try
		{
			pkey = EVP_PKEY_new();

			if(!pkey) throw "SSL key allocation problem";

			rsa = RSA_generate_key(2048,   RSA_F4, NULL,   NULL    );

			if(!rsa) throw "RSA key generation problem";

			EVP_PKEY_assign_RSA(pkey, rsa);

			x509 = X509_new();

			if(!x509) throw "x509 problem";

			ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);
			X509_gmtime_adj(X509_get_notBefore(x509), 0);
			X509_gmtime_adj(X509_get_notAfter(x509), 31536000L);
			X509_set_pubkey(x509, pkey);
			X509_NAME * name;
			name = X509_get_subject_name(x509);
			X509_NAME_add_entry_by_txt(name, "C",  MBSTRING_ASC,(unsigned char *)"CA", -1, -1, 0);
			X509_NAME_add_entry_by_txt(name, "O",  MBSTRING_ASC,(unsigned char *)"MyCompany Inc.", -1, -1, 0);
			X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,(unsigned char *)"localhost", -1, -1, 0);
			X509_set_issuer_name(x509, name);
			X509_sign(x509, pkey, EVP_sha1());

			FILE * f = fopen(key.c_str(), "wb"); if(!f) throw "Writing key.pem problem.";

			PEM_write_PrivateKey(f,pkey,EVP_des_ede3_cbc(), (unsigned char*)password.c_str(),(uint32_t)password.size(), NULL, NULL );

			fclose(f);

			f = fopen(cert.c_str(), "wb"); if(!f) throw "Writing cert.pem problem.";

			PEM_write_X509(f, x509);

			fclose(f);
		}
		catch(...) {}

		if(pkey) EVP_PKEY_free (pkey);
		//if(rsa) RSA_free(rsa);
		if(x509) X509_free(x509);

	}

	class SSLLibrary
	{
	public:
		SSLLibrary()
		{
			SSL_library_init();
			OpenSSL_add_all_algorithms();
			SSL_load_error_strings();
		}

		~SSLLibrary()
		{
			EVP_cleanup();
			ERR_free_strings();
		}
	};

	void LoadSSLLibrary() { make_singleton<SSLLibrary>(); }

	class SSLContext
	{
	public:
		SSLContext(uint32_t method_t=0)
		{
			LoadSSLLibrary();

			switch(method_t)
			{
			case 0 :
			default:
				method = TLSv1_2_client_method();			
				break;
			case 1:
				//method = TLSv1_2_server_method();
				method = SSLv23_server_method();
				break;
			}

			context = SSL_CTX_new(method);
		}

		~SSLContext()
		{
			SSL_CTX_free(context);
		}
	
		const SSL_METHOD *method;
		SSL_CTX* context;
	};

	SSLContext & GetSSLContext() { return make_singleton<SSLContext>(); }
	SSLContext & GetSSLServer() { return make_singleton2<SSLContext>(1); }

	std::function<std::string()> sslcertpwcb;

	int pem_passwd_cb(char *buf, int size, int rwflag, void *password)
	{
		if(!sslcertpwcb) return 0;
		auto s = sslcertpwcb(); sslcertpwcb = nullptr;
		strncpy(buf, (char *)s.c_str(), size);
		buf[size - 1] = '\0';
		return (int)s.size();
	}

	class SSLConnection
	{
	protected:
		int load_server_certificate(const std::string & cf)
		{
			/*auto s = std::istringstream(cf);
			std::vector<std::string> results((std::istream_iterator<std::string>(s)),std::istream_iterator<std::string>());

			sslcertpwcb = [&](){return results[2];};
			SSL_CTX_set_default_passwd_cb(ssl_context->context, pem_passwd_cb);
			if ( SSL_CTX_use_certificate_file(ssl_context->context, results[1].c_str(), SSL_FILETYPE_PEM) <= 0 ) throw "Failed to load cert file.";
			if ( SSL_CTX_use_PrivateKey_file(ssl_context->context, results[0].c_str(), SSL_FILETYPE_PEM) <= 0 ) throw "Failed to load key file.";
			if ( !SSL_CTX_check_private_key(ssl_context->context) ) throw "Failed to validate key file.";*/

			return 0;
		}

		int init_ssl()
		{
			ssl_connection = SSL_new(ssl_context->context);
			SSL_set_fd(ssl_connection,socket.handle);
			if ( SSL_connect(ssl_connection) == -1 )
			{
				//todo error message
				return -1;
			}

			return 0;
		}

		SSL * ssl_connection = nullptr;
		zed_net_socket_t socket = { 0, 0, 0 };
		SSLContext * ssl_context = nullptr;
	public:

		std::string SSLCipher()
		{
			if(!ssl_connection)
				return "";

			return SSL_get_cipher(ssl_connection);
		}

		std::pair<std::string,std::string> Certificate(SSL* ssl)
		{
			X509 *cert = SSL_get_peer_certificate(ssl);
			if(cert)
			{
				char *_subject = X509_NAME_oneline(X509_get_subject_name(cert), 0,0);
				char * _issuer = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);

				std::string issuer(_issuer);
				std::string subject(_subject);

				free(_subject);
				free(_issuer);
				X509_free(cert);

				return std::make_pair(subject,issuer);
			}
			return std::make_pair("","");
		}

		void SetConnection(const SSLConnection & r)
		{
			ssl_connection = r.ssl_connection;
			ssl_context = r.ssl_context;
			socket.handle = r.socket.handle;
		}

		SSLConnection(){}
		SSLConnection(const std::string & s, int timeout = 0, int buffer = 128*1024){ Connect(s,timeout,buffer);}
		~SSLConnection() { Close(); }

		void Release() { socket = { 0, 0, 0 }; ssl_connection = nullptr; }

		bool Close()
		{
			if(ssl_connection)
			{
				SSL_free(ssl_connection);
				ssl_connection = nullptr;
			}
			if(socket.handle)
			{
				zed_net_socket_close(&socket);
				socket.handle = 0;
				return false;
			}
			return true;
		}

		const char * Error() { return zed_net_get_error(); }

		bool Listen(uint16_t port, std::string certificate_file, int timeout = 0)
		{
			bool use_ssl = certificate_file.size()>0;
			if(use_ssl)ssl_context = &GetSSLServer();
			Close();

			if(ssl_context)
			{
				if(load_server_certificate(certificate_file) == -1)
					return false;
			}

			if (zed_net_tcp_socket_open(&socket,port,0,1) == -1) 
				return false;

			if(timeout)
			{
				if (zed_net_timeout(&socket, timeout) < 0) 
					return false;
			}	

			return true;
		}

		bool Accept(SSLConnection & c,zed_net_address_t * address=nullptr)
		{
			if(0 != zed_net_tcp_accept(&socket,&c.socket,address))
				return false;

			if(ssl_context)
			{
				c.ssl_connection = SSL_new(ssl_context->context);
				SSL_set_fd(c.ssl_connection, c.socket.handle);

				int s = SSL_accept(c.ssl_connection);
				while(0 >= s)
				{
					int e = SSL_get_error(c.ssl_connection,s);
					if(SSL_ERROR_WANT_READ  == e || SSL_ERROR_WANT_WRITE == e)
						std::this_thread::sleep_for(std::chrono::milliseconds(100));
					else
					{
						std::string s = ERR_error_string( e, NULL );
						ERR_print_errors_fp(stderr);
						//printf("SSL State: %s [%d]\n", SSL_state_string_long(c.ssl_connection), SSL_state(c.ssl_connection));
						Close();
						return false;
					}
					s = SSL_accept(c.ssl_connection);
				}
			}

			return true;
		}

		void UseAsync()
		{
			zed_net_async(&socket);
		}

		template < typename F > bool Connect2(const std::string & s, F f, int timeout = 0,int buffer = 128*1024)
		{
			bool result = false;
			uint32_t sleep = 0;
			do
			{
				Threads().Delay(sleep);

				result = Connect(s,timeout,buffer);
				if(result)
					break;

				sleep = f();

			} while( sleep );

			return result;
		}

		bool Connect(const std::string & s, int timeout = 0,int buffer = 128*1024)
		{
			ssl_context = &GetSSLContext();
			if(-1 == zed_net_get_address(s,SOCK_STREAM ,[&](auto s)->bool
			{
				this->Close();
				if (zed_net_tcp_socket_open(&socket) == -1) 
					return false;

				if(timeout)
				{
					if (zed_net_timeout(&socket, timeout) < 0) 
						return false;
				}	

				if(buffer)
				{
					if (zed_net_buffer(&socket, buffer) < 0) 
						return false;
				}

				if (zed_net_tcp_connect(&socket, s) == -1) 
					return false;

				if(this->init_ssl() == -1)
					return false;

				return true;
			})) return false;

			return true;
		}

		template <typename T> int Send(T && t)
		{
			if(ssl_connection)
			{
				int w = SSL_write(ssl_connection, t.data(), t.size());

				if(w<=0)
				{
					if(SSL_ERROR_WANT_WRITE  == SSL_get_error(ssl_connection,w))
						return 0;
				}

				return w;
			}

			return zed_net_tcp_socket_send(&socket,t.data(),t.size());
		}

		template <typename T> int Write(T && t)
		{
			if(ssl_connection)
			{
				auto remaining = t.size();
				uint32_t offset = 0;
				while(remaining)
				{
					auto sent = SSL_write(ssl_connection,t.data()+offset,t.size()-offset);
					if(sent == -1)
						return -1;
					if(!sent)
						std::this_thread::sleep_for(std::chrono::milliseconds(100));
					else
						remaining -= sent;
					offset += sent;
				}

				return (int)offset;
			}

			auto remaining = t.size();
			uint32_t offset = 0;
			while(remaining)
			{
				auto sent = zed_net_tcp_socket_send(&socket,t.data()+offset,t.size()-offset);
				if(sent == -1)
					return -1;
				if(!sent)
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				else
					remaining -= sent;
				offset += sent;
			}

			return (int)offset;
		}

		template <typename T> int Receive( T && t )
		{
			if(ssl_connection)
			{
				int r = SSL_read(ssl_connection, (char*)t.data(), t.size());

				if(r<0)
				{
					if(SSL_ERROR_WANT_READ  == SSL_get_error(ssl_connection,r))
						return 0;
				}
				if(r==0)
				{
					auto v = SSL_get_error(ssl_connection,r);
					return -1;
				}

				return r;
			}

			return zed_net_tcp_socket_receive(&socket, (char*)t.data(), t.size());
		}

		template <typename T> int Read(T && t)
		{
			if(ssl_connection)
			{
				auto remaining = t.size();
				uint32_t offset = 0;
				while(remaining)
				{
					auto read = SSL_read(ssl_connection,t.data()+offset,t.size()-offset);
					if(read == -1)
						return -1;
					else if(!read)
						std::this_thread::sleep_for(std::chrono::milliseconds(100));
					else
						remaining -= read;
					offset += read;
				}

				return offset;
			}

			auto remaining = t.size();
			uint32_t offset = 0;
			while(remaining)
			{
				auto read = zed_net_tcp_socket_receive(&socket,t.data()+offset,t.size()-offset);
				if(read == -1)
					return -1;
				else if(!read)
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				else
					remaining -= read;
				offset += read;
			}

			return offset;
		}
	};
}
