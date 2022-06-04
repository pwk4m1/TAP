/*
 * BSD 3-Clause License
 * 
 * Copyright (c) 2022, k4m1
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, 
 *    this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIA LDAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVE RCAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * 
 * Don't delete this time, mmkay?
 *
 * This file contains code related to networking input/output.
 * we'll read in and send out data here.
 * 
 * Multi-processing related stuff shall _NOT_ be done here.
 * The wait-for inbound should be done with
 * 	other file:
 * 		while (still_alive) {
 * 			try_to_receive(sock, timeout, dst);
 * 			...
 * 		}
 * 	this file:
 * 		try_to_receive(sock, timeout, dst) {
 * 			dst = recv(sock, ...);
 * 			if (timed out) {
 * 				return error;
 * 			} else {
 * 				return ok;
 * 			}
 * 		}
 *
 */
#include <sys/types.h>

#include <sys/time.h>
#include <sys/socket.h>
#include <sys/errno.h>

#include <arpa/inet.h>
#include <netinet/ip.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <log.h>
#include <intercept_helpers.h>

static int running = 1;

#define WAIT_DIR_OUT 1
#define WAIT_DIR_IN  2
#define SOCK_OP_CONN 1
#define SOCK_OP_BIND 0

/*
 * This function simply binds socket based on options provided OR
 * 		 	connects socket to remote host based
 *
 * Requires:
 * 	char *dst, 			where to bind/connect
 * 	short port, 			which tcp port to bind/connect
 * 	struct sockaddr_in *s_addr 	ptr to uninitialized sockaddr_in
 * 	int op 				1 to connect, 0 to bind
 * Modifies:
 * 	struct sockaddr_in is populated for the user.
 * Returns:
 * 	int socket on success or -1 on error
 */
int
sock_op_do(char *dst, short port, struct sockaddr_in *saddr, int op)
{
	int sock;
	int stat;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (!sock) {
		return sock;
	}

	saddr->sin_family 	= AF_INET;
	saddr->sin_addr.s_addr 	= inet_addr(dst);
	saddr->sin_port 	= htons(port);

	if (op == SOCK_OP_BIND) {
		stat = bind(sock, (struct sockaddr*)saddr, sizeof(*saddr));
	} else {
		stat = connect(sock, (struct sockaddr *)saddr, sizeof(*saddr));
	}
	if (stat < 0) {
		close(sock);
		sock = -1;
	}
	return sock;
}

/*
 * Wait for a socket to be readable or writable.
 *
 * Requires:
 * 	int sock, 			socket to waitfor
 * 	int dir, 			0 for send, 1 for recv
 * 	int s_timeout 			how many seconds until timeout
 * 	int u_timeout 			how many microseconds until timeout
 * Returns:
 * 	1 if socket is operable
 * 	0 if timed out
 * 	-1 on error
 */
int
waitfor(int sock, int dir, int s_timeout, int u_timeout)
{
	fd_set fds;
	struct timeval timeout;
	int stat;

	FD_ZERO(&fds);
	FD_SET(sock++, &fds);

	timeout.tv_sec  = (time_t)s_timeout;
	timeout.tv_usec = (suseconds_t)u_timeout;
	if (dir == WAIT_DIR_IN) {
		stat = select(FD_SETSIZE, &fds, 0, 0, &timeout);
	} else {
		stat = select(FD_SETSIZE, 0, &fds, 0, &timeout);
	}
	return stat;
}

/* 
 * Helper to read data from socket, waits until socket becomes
 * readable & reads data from socket to *dstptr
 *
 * Requires:
 * 	int sock, 			socket to read from
 * 	size_t size, 			amount of bytes to read
 * 	unsigned char *dst 		where to read to
 * Returns:
 * 	-1 on error, 0 on timeout, or amount of bytes read.
 */
size_t
rx(int sock, size_t size, unsigned char *dst)
{
	int stat;

	stat = waitfor(sock, WAIT_DIR_IN, 5, 0);
	if (stat <= 0) {
		return stat;
	}
	return recv(sock, dst, size, 0);
}

/*
 * Helper to send data over socket, waits until socket becomes
 * writable & sends data from *srcptr to socket
 *
 * Requires:
 * 	int sock, 			socket to write to
 * 	size_t size, 			amount of bytes to write
 * 	unsigned char *src, 		what to write
 * Returns:
 * 	-1 on error, 0 on timeout, or amounts of bytes written
 */
size_t
tx(int sock, size_t size, unsigned char *src)
{
	int stat;

	stat = waitfor(sock, WAIT_DIR_OUT, 5, 0);
	if (stat <= 0) {
		return stat;
	}
	return send(sock, src, size, 0);
}

/*
 * Wait indefinitely until data is readable from socket
 *
 * Requires:
 * 	int sock, 			socket to read from
 * 	size_t size, 			amount of bytes to read
 * 	unsigned char *dst, 		where to write data from socket
 * Returns:
 * 	-1 on unrecoverable error or amount of bytes recvd
 */
size_t
infinite_rx(int sock, size_t size, unsigned char *dst)
{
	size_t stat;

	stat = 0;
	do {
		stat = rx(sock, size, dst);
	} while (stat == 0);
	return stat;
}

/*
 * Receive data from peer A, operate on it based on user-provided
 * ruleset, and pass it on to peer B.
 *
 * Requires:
 * 	int sock_src, 			Peer A socket
 * 	int sock_dst,  			Peer B socket
 * 	size_t tx_size, 		Amount of bytes for transmitting 1 
 * 					packet from A -> B
 *	int (*callback)(unsigned char*) callback tamperer for txbuf
 *
 * Returns:
 * 	-1 on unrecoverable error, or amount of bytes transmitted
 */
size_t
sink_a_to_b(int sock_src, int sock_dst, size_t tx_size,
		void (*callback)(unsigned char*))
{
	size_t stat;
	unsigned char *txbuf;

	txbuf = (unsigned char *)malloc(tx_size);
	if (!txbuf) {
		LOG("malloc(%zu) failed\n", tx_size);
		return -1;
	}
	memset(txbuf, 0, tx_size);

	stat = infinite_rx(sock_src, tx_size, txbuf);
	if (stat == -1) {
		return stat;
	}
	if (callback != 0) {
		callback(txbuf);
	}
	stat = tx(sock_dst, tx_size, txbuf);
	
	memset(txbuf, 0, tx_size);
	free(txbuf);
	return stat;
}

/*
 * Waitfor A or B socket becoming readable
 * 
 * Requires:
 * 	int sin 			- client socket
 * 	int sout 			- server socket
 * Returns:
 *  	WAIT_DIR_IN if sin is readable
 *  	WAIT_DIR_OUT if sout is readable
 *  	WAIT_DIR_IN | WAIT_DIR_OUT if both (UB/tcp error)
 *  	-1 on error
 */
int
waitfor_any_readable(int sin, int sout)
{
	struct timeval tv;
	fd_set fds;
	int stat;
	int waiting;

	FD_ZERO(&fds);
	FD_SET(sin, &fds);
	FD_SET(sout, &fds);
	
	tv.tv_sec = 10;
	tv.tv_usec = 0;

	waiting = 1;
	while (waiting) {
		stat = select(sin+sout, &fds, 0, 0, &tv);
		switch (stat) {
		case(-1):
			ERR("select() errored with errno: %d\n", errno);
			waiting = 0;
			break;
		case (0):
			/* Time out, repeat */
			break;
		default:
			waiting = 0;
		}
	}
	if (stat == -1) {
		return stat;
	}
	stat = 0;
	if (FD_ISSET(sin, &fds)) {
		stat |= WAIT_DIR_IN;
	}
	if (FD_ISSET(sout, &fds)) {
		stat |= WAIT_DIR_OUT;
	}
	if (!stat)
		return -1;
	return stat;
}

/*
 * Sink A <-> B forever/until connection is closed
 *
 * Requires:
 * 	int sin 				- client socket connected to us
 * 	int sout 				- dest. socket to relay to
 * 	size_t tx_size 				- transmission buffer size
 * 	void (*cb)(unsigned char*, size_t) 	- callback for interception
 * Returns:
 * 	None
 */
void
sink_a_and_b_forever(int sin, int sout, size_t tx_size, 
		void (*cb)(unsigned char *, size_t))
{
	size_t stat;
	int stat_dir;
	unsigned char *txbuf;

	txbuf = (unsigned char *)malloc(tx_size);
	if (!txbuf) {
		LOG("malloc(%zu) failed\n", tx_size);
		return;
	}
	for (;;) {
		memset(txbuf, 0, tx_size);

		/* Wait for either direction to write */
		stat_dir = waitfor_any_readable(sin, sout);
		if (stat_dir < 0) {
			break;
		}
		/* Receive data */
		if (stat_dir & WAIT_DIR_IN) {
			stat = rx(sin, tx_size, txbuf);
		} else if (stat_dir & WAIT_DIR_OUT) { 
			stat = rx(sout, tx_size, txbuf);
		}
		if (stat <= 0) {
			ERR("tx failed, socket disconnected?\n");
			goto end;
		}
		/* If callback, do it */
		if (cb != 0) {
			cb(txbuf, tx_size);
		}
		/* Relay data */
		if (stat_dir & WAIT_DIR_IN) {
			stat = tx(sout, tx_size, txbuf);
		} else if (stat_dir & WAIT_DIR_OUT) {
			stat = tx(sin, tx_size, txbuf);
		}
		if (stat < 0) {
			ERR("tx failed\n");
			goto end;
		}
	}
end:
	memset(txbuf, 0, tx_size);
	free(txbuf);

}

/*
 * Start sink for specified source/destination pair with
 * fixed size transmit buffers
 *
 * Requires:
 * 	char *addrin 				- address to bind
 * 	short lport 				- port to listen to
 * 	char *addrout 				- address to forward inbound data to
 * 	short dport 				- port to send to
 * 	size_t tx_size 				- amount of bytes to transmit
 * 	void (*cb)(unsigned char*, size_t) 	- callback for interception
 * Returns:
 * 	None
 */
void
start_sink(char *addrin, short lport, char *addrout, short dport, 
		size_t tx_size, void (*cb)(unsigned char *, size_t))
{
	struct sockaddr_in saddr_peer_in;
	struct sockaddr_in saddr_peer_out;
	socklen_t saddr_size;
	int peer_in_sock;
	int peer_out_sock;
	int nsock;
	int stat;
	int retries;

	/*
	 * Initialise listener socket, retries, ...
	 */
	retries = 0;
	peer_in_sock = sock_op_do(addrin, lport, &saddr_peer_in,
			SOCK_OP_BIND);
	if (peer_in_sock <= 0) {
		ERR("Unable to start sink, sock_op_do() errored\n");
		return;
	}

	/*
	 * Listen for inbound traffic
	 */
	listen(peer_in_sock, 1);
	saddr_size = sizeof(saddr_peer_in);

	do {
		/*
		 * Accept inbound connection, nsock <- new socket 
		 */
		nsock = accept(peer_in_sock,
				(struct sockaddr *)&saddr_peer_in,
				&saddr_size);

		if (nsock == -1) {
			ERR("Error: %s", "Failed to accept() from socket\n");
			goto end;
		}
		/* 
		 * Loop until we can connect to remote host
		 */
		do {
			peer_out_sock = sock_op_do(addrout, dport,
							&saddr_peer_out,
							SOCK_OP_CONN);
			if (peer_out_sock <= 0) {
				retries++;
			}
		} while ((peer_out_sock <= 0) && (retries < 10));

		/* 
		 * If max retries happened, then quit
		 */
		if (retries == 10) {
			ERR("Failed to connect to %s\n", addrout);
			goto end;
		}
		/* Reset retries as we succeeded */
		retries = 0;
		/* Sink 1 tranmission from A -> B */
		/* Sink A <-> B */
		//sink_a_to_b(nsock, peer_out_sock, tx_size, cb);
		sink_a_and_b_forever(nsock, peer_out_sock, tx_size, cb);
		/* close outbound & temp socks, init back to 0 */
		close(nsock);
		close(peer_out_sock);
		nsock = 0;
		peer_out_sock = 0;

	/* If no longer running, quit */
	} while (running);
end:
	if (nsock)
		close(nsock);
	if (peer_out_sock)
		close(peer_out_sock);
	if (peer_in_sock)
		close(peer_in_sock);
}


/* TESTS HERE */

/*
 * callback/intercepting functionality here for testcases 
 */
void
test_cb(unsigned char *buf, size_t buf_size)
{
	void *off;

	off = findseq(buf, "TEST", buf_size, strlen("TEST"));
	if (off) {
		replace_str_of_equal_size(buf, buf_size, strlen("TEST"),
				(unsigned char *)&"TEST", 
				(unsigned char *)&"LMAO");
	}
}

/* TESTS END */

int
main()
{
	void (*cb)(unsigned char*, size_t) = &test_cb;
	//test_bind_wait_rx_tx();
	start_sink("0.0.0.0", 1337, "127.0.0.1", 1338, 256, cb);
	return -1;
}

