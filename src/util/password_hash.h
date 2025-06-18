#pragma once

#define PWD_HASH_LEN  crypto_pwhash_STRBYTES   /* 97바이트 */
#include <sodium/crypto_pwhash.h>

int kta_password_hash(const char *pw, char out[PWD_HASH_LEN]);

int verify_password(const char *pw, const char stored[PWD_HASH_LEN]);
