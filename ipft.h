
#ifndef IPFT_H
#define IPFT_H

typedef struct receiver_s {
    void *h;
    int (*canrecv)(struct receiver_s *);
    int (*recv)(struct receiver_s *, char *, int);
    void (*close)(struct receiver_s *);
} receiver_t;

typedef struct transmitter_s {
    void *h;
    int (*send)(struct transmitter_s *, char *, int);
    void (*close)(struct transmitter_s *);
} transmitter_t;


#include <netinet/in.h>

extern transmitter_t *linux_tx_open(struct sockaddr_in *);
extern receiver_t *linux_rx_open(void);

extern void bomb(char *, ...);

#endif
