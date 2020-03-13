#pragma once

/////////////////////////////////////////////////////////////////////////////////////////
//
// INITIALIZATION AND SHUTDOWN
//

// Get a brief reason for failure
 const char *zed_net_get_error(void);

// Perform platform-specific socket initialization;
// *must* be called before using any other function
//
// Returns 0 on success, -1 otherwise (call 'zed_net_get_error' for more info)
 int zed_net_init(void);

// Perform platform-specific socket de-initialization;
// *must* be called when finished using the other functions
 void zed_net_shutdown(void);

/////////////////////////////////////////////////////////////////////////////////////////
//
// INTERNET ADDRESS API
//

// Represents an internet address usable by sockets
typedef struct {
    unsigned int host;
    unsigned short port;
} zed_net_address_t;

// Obtain an address from a host name and a port
//
// 'host' may contain a decimal formatted IP (such as "127.0.0.1"), a human readable
// name (such as "localhost"), or NULL for the default address
//
// Returns 0 on success, -1 otherwise (call 'zed_net_get_error' for more info)
// int zed_net_get_address(zed_net_address_t *address, const char *host,const char * port);

// Converts an address's host name into a decimal formatted string
//
// Returns NULL on failure (call 'zed_net_get_error' for more info)
 const char *zed_net_host_to_str(unsigned int host);

/////////////////////////////////////////////////////////////////////////////////////////
//
// UDP SOCKETS API
//

// Wraps the system handle for a UDP/TCP socket
typedef struct {
    size_t handle;
    int non_blocking;
    int ready;
} zed_net_socket_t;

// Closes a previously opened socket
 void zed_net_socket_close(zed_net_socket_t *socket);

// Opens a UDP socket and binds it to a specified port
// (use 0 to select a random open port)
//
// Socket will not block if 'non-blocking' is non-zero
//
// Returns 0 on success
// Returns -1 on failure (call 'zed_net_get_error' for more info)
 int zed_net_udp_socket_open(zed_net_socket_t *socket, unsigned int port, int non_blocking);

// Closes a previously opened socket
 void zed_net_socket_close(zed_net_socket_t *socket);

// Sends a specific amount of data to 'destination'
//
// Returns 0 on success, -1 otherwise (call 'zed_net_get_error' for more info)
 int zed_net_udp_socket_send(zed_net_socket_t *socket, zed_net_address_t destination, const void *data, int size);

// Receives a specific amount of data from 'sender'
//
// Returns the number of bytes received, -1 otherwise (call 'zed_net_get_error' for more info)
 int zed_net_udp_socket_receive(zed_net_socket_t *socket, zed_net_address_t *sender, void *data, int size);

/////////////////////////////////////////////////////////////////////////////////////////
//
// TCP SOCKETS API
//

// Opens a TCP socket and binds it to a specified port
// (use 0 to select a random open port)
//
// Socket will not block if 'non-blocking' is non-zero
//
// Returns NULL on failure (call 'zed_net_get_error' for more info)
// Socket will listen for incoming connections if 'listen_socket' is non-zero
// Returns 0 on success
// Returns -1 on failure (call 'zed_net_get_error' for more info)
 int zed_net_tcp_socket_open(zed_net_socket_t *socket, unsigned int port, int non_blocking, int listen_socket);

// Connect to a remote endpoint
// Returns 0 on success.
//  if the socket is non-blocking, then this can return 1 if the socket isn't ready
//  returns -1 otherwise. (call 'zed_net_get_error' for more info)
 int zed_net_tcp_connect(zed_net_socket_t *socket, zed_net_address_t remote_addr);

// Accept connection
// New remote_socket inherits non-blocking from listening_socket
// Returns 0 on success.
//  if the socket is non-blocking, then this can return 1 if the socket isn't ready
//  if the socket is non_blocking and there was no connection to accept, returns 2
//  returns -1 otherwise. (call 'zed_net_get_error' for more info)
 int zed_net_tcp_accept(zed_net_socket_t *listening_socket, zed_net_socket_t *remote_socket, zed_net_address_t *remote_addr=nullptr);

// Returns 0 on success.
//  if the socket is non-blocking, then this can return 1 if the socket isn't ready
//  returns -1 otherwise. (call 'zed_net_get_error' for more info)
 int zed_net_tcp_socket_send(zed_net_socket_t *remote_socket, const void *data, int size);

// Returns 0 on success.
//  if the socket is non-blocking, then this can return 1 if the socket isn't ready
//  returns -1 otherwise. (call 'zed_net_get_error' for more info)
 int zed_net_tcp_socket_receive(zed_net_socket_t *remote_socket, void *data, int size,bool* =nullptr);

// Blocks until the TCP socket is ready. Only makes sense for non-blocking socket.
// Returns 0 on success.
//  returns -1 otherwise. (call 'zed_net_get_error' for more info)
 int zed_net_tcp_make_socket_ready(zed_net_socket_t *socket);

#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <WinSock2.h>
#include <Ws2tcpip.h>
#pragma comment(lib, "wsock32.lib")
#pragma comment(lib, "Ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#endif

static const char *zed_net__g_error;

static int zed_net__error(const char *message) {
    zed_net__g_error = message;

    return -1;
}

 const char *zed_net_get_error(void) {
    return zed_net__g_error;
}

 int zed_net_init(void) {
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        return zed_net__error("Windows Sockets failed to start");
    }

    return 0;
#else
    return 0;
#endif
}

 void zed_net_shutdown(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

 const char *zed_net_host_to_str(unsigned int host) {
    struct in_addr in;
    in.s_addr = host;

    return inet_ntoa(in);
}

 int zed_net_timeout(zed_net_socket_t *sock,int seconds)
 {
#ifdef _WIN32
	DWORD dw = seconds*1000;
	int r = setsockopt(sock->handle, SOL_SOCKET, SO_RCVTIMEO, (char*)&dw, sizeof(DWORD));
	if(r < 0)
		zed_net__error("Failed to set timeout");
	return r;
#else
	struct timeval tv;
	tv.tv_sec = seconds;
	tv.tv_usec = 0;
	int r = setsockopt(sock->handle, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
	if(r < 0)
		zed_net__error("Failed to set timeout");
	return r;
#endif
 }

 int zed_net_buffer(zed_net_socket_t *sock,int bytes)
 {
#ifdef _WIN32
	DWORD dw = bytes;
	int r = setsockopt(sock->handle, SOL_SOCKET, SO_RCVBUF, (char*)&dw, sizeof(DWORD));
	    r = setsockopt(sock->handle, SOL_SOCKET, SO_SNDBUF, (char*)&dw, sizeof(DWORD));
	if(r < 0) zed_net__error("Failed to set buffer");
	return r;
#else
	int s = bytes;
	int r = setsockopt(sock->handle, SOL_SOCKET, SO_RCVBUF, (char*)&s, sizeof(int));
	    r = setsockopt(sock->handle, SOL_SOCKET, SO_SNDBUF, (char*)&s, sizeof(int));
	if(r < 0)
		zed_net__error("Failed to set timeout");
	return r;
#endif
 }

 int zed_net_read_buffer(zed_net_socket_t* sock, int bytes)
 {
#ifdef _WIN32
     DWORD dw = bytes;
     int r = setsockopt(sock->handle, SOL_SOCKET, SO_RCVBUF, (char*)&dw, sizeof(DWORD));
     if (r < 0) zed_net__error("Failed to set buffer");
     return r;
#else
     int s = bytes;
     int r = setsockopt(sock->handle, SOL_SOCKET, SO_RCVBUF, (char*)&s, sizeof(int));
     if (r < 0)
         zed_net__error("Failed to set timeout");
     return r;
#endif
 }

 int zed_net_write_buffer(zed_net_socket_t* sock, int bytes)
 {
#ifdef _WIN32
     DWORD dw = bytes;
     int r = setsockopt(sock->handle, SOL_SOCKET, SO_SNDBUF, (char*)&dw, sizeof(DWORD));
     if (r < 0) zed_net__error("Failed to set buffer");
     return r;
#else
     int s = bytes;
     int r = setsockopt(sock->handle, SOL_SOCKET, SO_SNDBUF, (char*)&s, sizeof(int));
     if (r < 0)
         zed_net__error("Failed to set timeout");
     return r;
#endif
 }

 int zed_net_async(zed_net_socket_t * sock, u_long val = 1)
 {	 
#ifdef _WIN32
	 u_long non_blocking = val;
        if (ioctlsocket(sock->handle, FIONBIO, &non_blocking) != 0)
            return zed_net__error("Failed to set socket to non-blocking");
#else
	 int non_blocking = val;
        if (fcntl(sock->handle, F_SETFL, O_NONBLOCK, non_blocking) != 0)
            return zed_net__error("Failed to set socket to non-blocking");
#endif
		return 0;
 }

 int zed_net_udp_socket_open(zed_net_socket_t *sock/*, unsigned int port, int non_blocking*/) {
    if (!sock)
        return zed_net__error("Socket is NULL");

    // Create the socket
    sock->handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock->handle <= 0) {
        zed_net_socket_close(sock);
        return zed_net__error("Failed to create socket");
    }

    // Bind the socket to the port
    //struct sockaddr_in address;
    //address.sin_family = AF_INET;
    //address.sin_addr.s_addr = INADDR_ANY;
    //address.sin_port = htons(port);

    /*if (bind(sock->handle, (const struct sockaddr *) &address, sizeof(struct sockaddr_in)) != 0) {
        zed_net_socket_close(sock);
        return zed_net__error("Failed to bind socket");
    }

    // Set the socket to non-blocking if neccessary
    if (non_blocking) {
#ifdef _WIN32
        if (ioctlsocket(sock->handle, FIONBIO, &non_blocking) != 0) {
            zed_net_socket_close(sock);
            return zed_net__error("Failed to set socket to non-blocking");
        }
#else
        if (fcntl(sock->handle, F_SETFL, O_NONBLOCK, non_blocking) != 0) {
            zed_net_socket_close(sock);
            return zed_net__error("Failed to set socket to non-blocking");
        }
#endif
    }*/

    sock->non_blocking = 0;

    return 0;
}

 int zed_net_tcp_socket_open(zed_net_socket_t *sock, unsigned int port=0, int non_blocking=0, int listen_socket=0) {
    if (!sock)
        return zed_net__error("Socket is NULL");

    // Create the socket
    sock->handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock->handle <= 0) {
        zed_net_socket_close(sock);
        return zed_net__error("Failed to create socket");
    }

	if(port)
	{
		// Bind the socket to the port
		struct sockaddr_in address;
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY;
		address.sin_port = htons(port);

		if (bind(sock->handle, (const struct sockaddr *) &address, sizeof(struct sockaddr_in)) != 0) {
			zed_net_socket_close(sock);
			return zed_net__error("Failed to bind socket");
		}
	}

    // Set the socket to non-blocking if neccessary
    if (non_blocking) {
#ifdef _WIN32
		u_long l = non_blocking;
        if (ioctlsocket(sock->handle, FIONBIO, &l) != 0) {
            zed_net_socket_close(sock);
            return zed_net__error("Failed to set socket to non-blocking");
        }
#else
        if (fcntl(sock->handle, F_SETFL, O_NONBLOCK, non_blocking) != 0) {
            zed_net_socket_close(sock);
            return zed_net__error("Failed to set socket to non-blocking");
        }
#endif
	sock->ready = 0;
    }

    if (listen_socket) {
#ifndef SOMAXCONN
#define SOMAXCONN 10
#endif
		if (listen(sock->handle, SOMAXCONN) != 0) {
            zed_net_socket_close(sock);
            return zed_net__error("Failed make socket listen");
        }
    }
    sock->non_blocking = non_blocking;

    return 0;
}

// Returns 1 if it would block, <0 if there's an error.
 int zed_net_check_would_block(zed_net_socket_t *socket) 
{
    /*struct timeval timer;
    fd_set writefd;
	int retval;

    if (socket->non_blocking && !socket->ready) {
		writefd.fd_count = 1;
        writefd.fd_array[0] = socket->handle;
        timer.tv_sec = 0;
        timer.tv_usec = 0;
		retval = select(0, NULL, &writefd, NULL, &timer);
        if (retval == 0)
			return 1;
		else if (retval == SOCKET_ERROR) {
			zed_net_socket_close(socket);
			return zed_net__error("Got socket error from select()");
		}
		socket->ready = 1;
    }*/

	return 0;
}

 int zed_net_tcp_make_socket_ready(zed_net_socket_t *socket) 
{
	/*if (!socket->non_blocking)
		return 0;
	if (socket->ready)
		return 0;

    fd_set writefd;
	int retval;

	writefd.fd_count = 1;
	writefd.fd_array[0] = socket->handle;
	retval = select(0, NULL, &writefd, NULL, NULL);
	if (retval != 1)
		return zed_net__error("Failed to make non-blocking socket ready");*/

	socket->ready = 1;

	return 0;
}

 int zed_net_udp_connect(zed_net_socket_t *socket,addrinfo* s)
 {
	int r = connect(socket->handle, s->ai_addr, (int)s->ai_addrlen);

	if(r==-1)
		zed_net__error("Failed to connect");
	
	return r;
 }

 int zed_net_tcp_connect(zed_net_socket_t *socket, addrinfo* s) 
 {
    int retval;

    if (!socket)
        return zed_net__error("Socket is NULL");

	retval = zed_net_check_would_block(socket);
	if (retval == 1)
		return 1;
	else if (retval)
		return -1;

    retval = connect(socket->handle, s->ai_addr, (int)s->ai_addrlen);
	if (retval == SOCKET_ERROR) {
        zed_net_socket_close(socket);
        return zed_net__error("Failed to connect socket");
    }

    return 0;
}

 int zed_net_tcp_accept(zed_net_socket_t *listening_socket, zed_net_socket_t *remote_socket, zed_net_address_t *remote_addr) {
    struct sockaddr_in address;
	int retval, handle;

    if (!listening_socket)
        return zed_net__error("Listening socket is NULL");
    if (!remote_socket)
        return zed_net__error("Remote socket is NULL");
    //if (!remote_addr)
    //    return zed_net__error("Address pointer is NULL");

	retval = zed_net_check_would_block(listening_socket);
	if (retval == 1)
		return 1;
	else if (retval)
		return -1;
#ifdef _WIN32
    typedef int socklen_t;
#endif
	socklen_t addrlen = sizeof(address);
	handle = (int)accept(listening_socket->handle, (struct sockaddr *)&address, &addrlen);

	if (handle == INVALID_SOCKET)
		return 2;

	if(remote_addr)
	{
		remote_addr->host = address.sin_addr.s_addr;
		remote_addr->port = ntohs(address.sin_port);
	}
	remote_socket->non_blocking = listening_socket->non_blocking;
	remote_socket->ready = 0;
	remote_socket->handle = handle;

	return 0;
}

 void zed_net_socket_close(zed_net_socket_t *socket) {
    if (!socket) {
        return;
    }

    if (socket->handle) {
#ifdef _WIN32
        shutdown(socket->handle, SD_BOTH);
        closesocket(socket->handle);
#else
        shutdown(socket->handle, SD_BOTH);
        close(socket->handle);
#endif
    }
}

 int zed_net_udp_socket_send(zed_net_socket_t *socket, zed_net_address_t destination, const void *data, int size) {
    if (!socket) {
        return zed_net__error("Socket is NULL");
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = destination.host;
    address.sin_port = htons(destination.port);

    int sent_bytes = sendto(socket->handle, (const char *) data, size, 0, (const struct sockaddr *) &address, sizeof(struct sockaddr_in));
    if (sent_bytes != size) {
        return zed_net__error("Failed to send data");
    }

    return 0;
}

 int zed_net_udp_socket_receive(zed_net_socket_t *socket, zed_net_address_t *sender, void *data, int size) {
    if (!socket) {
        return zed_net__error("Socket is NULL");
    }

#ifdef _WIN32
    typedef int socklen_t;
#endif

    struct sockaddr_in from;
    socklen_t from_length = sizeof(from);

    int received_bytes = recvfrom(socket->handle, (char *) data, size, 0, (struct sockaddr *) &from, &from_length);
    if (received_bytes <= 0) {
        return 0;
    }

    sender->host = from.sin_addr.s_addr;
    sender->port = ntohs(from.sin_port);

    return received_bytes;
}

 int zed_net_tcp_socket_send(zed_net_socket_t *remote_socket, const void *data, int size) {
	int retval;

    if (!remote_socket) {
        return zed_net__error("Socket is NULL");
    }

	retval = zed_net_check_would_block(remote_socket);
	if (retval == 1)
		return 0;
	else if (retval)
		return -1;

    int sent_bytes = send(remote_socket->handle, (const char *) data, size, 0);
    if (sent_bytes < 0) {
		#ifdef _WIN32
			if(WSAEWOULDBLOCK == WSAGetLastError())
				return 0;
		#else
			if(EAGAIN == errno)
				return 0;
		#endif
        return zed_net__error("Connection has closed on write.");
    }

    return sent_bytes;
}

 int zed_net_tcp_socket_receive(zed_net_socket_t *remote_socket, void *data, int size,bool*timeout) {
	int retval;

    if (!remote_socket) {
        return zed_net__error("Socket is NULL");
    }

	retval = zed_net_check_would_block(remote_socket);
	if (retval == 1)
		return 1;
	else if (retval)
		return -1;

#ifdef _WIN32
    typedef int socklen_t;
#endif

    int received_bytes = recv(remote_socket->handle, (char *) data, size, 0);

	if(timeout) *timeout = false;
	if (received_bytes < 0) 
	{
		#ifdef _WIN32
			auto error = WSAGetLastError();
			if(WSAEWOULDBLOCK == error || WSAETIMEDOUT == error)
			{
				if(timeout) *timeout = true;
				return 0;
			}
		#else
			if(EAGAIN == errno)
			{
				if(timeout) *timeout = true;
				return 0;
			}
		#endif
		return zed_net__error("Connection has closed on read.");
	}

    return received_bytes;
}