#ifndef _SHA256_H_
#define _SHA256_H_

#include <string.h>

typedef struct sha256_state_struct {
    unsigned long long length;
    unsigned long state[8], curlen;
    unsigned char buf[64];
} sha256_state;

void sha256_init(sha256_state * md);
void sha256_process (sha256_state * md, const unsigned char *in, unsigned long inlen);
void sha256_done(sha256_state * md, unsigned char *out);
#endif
