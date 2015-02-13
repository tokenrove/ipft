/* See the file COPYING for license details. */

#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

/* ridiculous glibc things */
#include <features.h>
#if __GLIBC__ >= 2 && __GLIBC_MINOR >= 1
#include <netpacket/packet.h>
#include <net/ethernet.h>
#else
#include <asm/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ipft.h"


struct private_s {
    int fd, plen;
    unsigned char *pbuf;
};

transmitter_t *linux_tx_open(struct sockaddr_in *destaddr);
receiver_t *linux_rx_open(void);

int linux_tx_send(transmitter_t *tx, char *p, int len);
void linux_tx_close(transmitter_t *tx);

int linux_rx_canrecv(receiver_t *rx);
int linux_rx_recv(receiver_t *rx, char *p, int len);
void linux_rx_close(receiver_t *rx);


transmitter_t *linux_tx_open(struct sockaddr_in *destaddr)
{
    transmitter_t *tx;
    struct private_s *priv;
    int i;

    tx = malloc(sizeof(*tx));
    if(tx == NULL) return NULL;

    priv = malloc(sizeof(*priv));
    if(priv == NULL) return NULL;

    /* Pretend to be fragmented TCP */
    priv->fd = socket(PF_INET, SOCK_RAW, IPPROTO_RAW);
    if(priv->fd < 0)
        bomb("%s: failed: %s\n", __FUNCTION__, strerror(errno));
    i = 1;
    if(setsockopt(priv->fd, SOL_IP, IP_HDRINCL, &i, sizeof(i)) < 0)
        bomb("%s: failed: %s\n", __FUNCTION__, strerror(errno));

    destaddr->sin_port = htons(IPPROTO_TCP);
    i = connect(priv->fd, (const struct sockaddr *)destaddr,
		sizeof(*destaddr));
    if(i < 0)
        bomb("%s: failed: %s\n", __FUNCTION__, strerror(errno));

    priv->plen = 576;
    priv->pbuf = malloc(priv->plen);
    /* Fill the header */
    memset(priv->pbuf, 0, priv->plen);
    /* Version = 4, IHL = 5 */
    priv->pbuf[0] = 5 | (4 << 4);
    /* TOS can stay zero */
    /* Total length will be filled in for us */
    /* ID is 0xbeef */
    priv->pbuf[4] = 0xbe;
    priv->pbuf[5] = 0xef;
    /* Flags are May Fragment, More Fragments, Frag offset is 42 ATM */
    priv->pbuf[6] = 2;
    priv->pbuf[7] = 0;
    /* TTL is 64 for no good reason (FIXME?) */
    priv->pbuf[8] = 64;
    /* Pretend to be a TCP fragment */
    priv->pbuf[9] = IPPROTO_TCP;
    /* Checksum is filled in for us, as is source address */
    /* Destination address is destaddr->sin_addr */
    memcpy(priv->pbuf+16, &destaddr->sin_addr.s_addr, 4);

    tx->h = priv;
    tx->send = linux_tx_send;
    tx->close = linux_tx_close;

    return tx;
}


receiver_t *linux_rx_open(void)
{
    receiver_t *rx;
    struct private_s *priv;

    rx = malloc(sizeof(*rx));
    if(rx == NULL) return NULL;

    priv = malloc(sizeof(*priv));
    if(priv == NULL) return NULL;

    priv->fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    if(priv->fd < 0)
        bomb("%s: failed: %s\n", __FUNCTION__, strerror(errno));

    priv->plen = 576;
    priv->pbuf = malloc(priv->plen);
    memset(priv->pbuf, 0, priv->plen);

    rx->h = priv;
    rx->canrecv = linux_rx_canrecv;
    rx->recv = linux_rx_recv;
    rx->close = linux_rx_close;

    return rx;
}


int linux_tx_send(transmitter_t *tx, char *p, int len)
{
    struct private_s *priv = tx->h;
    int i;

    memcpy(priv->pbuf+5*4, p, len);
    i = send(priv->fd, priv->pbuf, len+5*4, 0);
    if(i < 0)
        bomb("%s: failed: %s\n", __FUNCTION__, strerror(errno));
    return i;
}


void linux_tx_close(transmitter_t *tx)
{
    struct private_s *priv;

    if(!tx) return;

    priv = tx->h;
    if(priv) {
	if(priv->fd != -1)
	    close(priv->fd);
	free(priv);
    }
    memset(tx, 0, sizeof(*tx));
    free(tx);
    return;
}


int linux_rx_canrecv(receiver_t *rx)
{
    fd_set rfds;
    struct timeval tv = { 0, 0 };
    struct private_s *priv = rx->h;

    FD_ZERO(&rfds);
    FD_SET(priv->fd, &rfds);
    return select(priv->fd+1, &rfds, NULL, NULL, &tv) ? 1 : 0;
}


int linux_rx_recv(receiver_t *rx, char *p, int len)
{
    struct private_s *priv = rx->h;
    int i;

    i = read(priv->fd, priv->pbuf, priv->plen);
    /* If the ID is 0xbeef, let it in. */
    if(priv->pbuf[14+4] == 0xbe &&
       priv->pbuf[14+5] == 0xef) {
	i -= 5*4+14;
	if(i > len)
	    i = len;
	memcpy(p, priv->pbuf+5*4+14, i);
    } else
	i = 0;
    return i;
}


void linux_rx_close(receiver_t *rx)
{
    struct private_s *priv;

    if(!rx) return;

    priv = rx->h;
    if(priv) {
	if(priv->fd != -1)
	    close(priv->fd);
	free(priv);
    }
    memset(rx, 0, sizeof(*rx));
    free(rx);
    return;
}
