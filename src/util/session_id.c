#include "util/session_id.h"
#include <sodium.h>

static const char HEX[] = "0123456789abcdef";

void kta_generate_session_id(char out[SESSION_ID_LEN + 1])
{
    unsigned char rnd[32];
    randombytes_buf(rnd, sizeof rnd);
    for (int i = 0; i < 32; ++i) {
        out[i*2]     = HEX[rnd[i] >> 4];
        out[i*2 + 1] = HEX[rnd[i] & 0xF];
    }
    out[SESSION_ID_LEN] = '\0';
}