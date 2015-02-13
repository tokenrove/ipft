/*
 * ipft -- IP Fragment Tunnelling
 * Concept by Jamie Gamble <bit@distorted.wiw.org>
 * Implementation by Julian Squires <tek@wiw.org>
 * See the file COPYING for license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>

#include "ipft.h"


void mainloop(transmitter_t *tx, receiver_t *rx);


int main(int argc, char **argv)
{
    transmitter_t *tx;
    receiver_t *rx;
    struct sockaddr_in saddr;
    int i;
    struct hostent *hent;

    /* Deal with arguments. */
    saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    for(i = 1; i < argc; i++) {
	if(argv[i][0] == '-') {
	} else {
	    hent = gethostbyname(argv[i]);
            printf("%s %x\n", hent->h_name, *((uint32_t *)hent->h_addr_list[0]));
	    memcpy(&saddr.sin_addr, hent->h_addr_list[0], hent->h_length);
	}
    }

    /* Open Rx. */
    rx = linux_rx_open();
    if(rx == NULL)
	bomb("Failed to open Rx.\n");

    /* Open Tx. */
    saddr.sin_family = AF_INET;

    tx = linux_tx_open(&saddr);
    if(tx == NULL)
	bomb("Failed to open Tx.\n");

    /* Pass data between local user and Tx/Rx */
    mainloop(tx, rx);

    /* Clean up. */
    tx->close(tx);
    rx->close(rx);

    exit(EXIT_SUCCESS);
}


void mainloop(transmitter_t *tx, receiver_t *rx)
{
    int i, buflen;
    char *buf;
    fd_set rfds;
    struct timeval tv;

    /* FIXME buffer length should be based on path MTU */
    buflen = 255;
    buf = malloc(buflen);
    if(buf == NULL)
	bomb("Failed to allocate base buffer.\n");

    while(1) {
	FD_ZERO(&rfds);
	FD_SET(0, &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = 100;

	select(1, &rfds, NULL, NULL, &tv);

	if(FD_ISSET(0, &rfds)) {
	    i = read(0, buf, buflen);

	    /* Error or EOF? */
	    if(i <= 0)
		break;

	    i = tx->send(tx, buf, i);
	    if(i < 0)
		bomb("Tx send failed with code %d.\n", i);
	}

	if(rx->canrecv(rx)) {
	    i = rx->recv(rx, buf, buflen);
	    if(i < 0)
		bomb("Rx receive failed with code %d.\n", i);
	    write(1, buf, i);
	}
    }

    /* Wipe and clean buffer. */
    memset(buf, 0, buflen);
    free(buf);

    return;
}

/* EOF ipft.c */
