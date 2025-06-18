#include "password_hash.h"
#include <sodium.h>
#include <string.h>

int kta_password_hash(const char *pw, char out[PWD_HASH_LEN]) {
    return crypto_pwhash_str(
               out,
               pw, strlen(pw),
               crypto_pwhash_OPSLIMIT_MODERATE,
               crypto_pwhash_MEMLIMIT_MODERATE) == 0
               ? 0
               : -1;
}

int kta_password_verify(const char *pw, const char stored[PWD_HASH_LEN]) {
    return crypto_pwhash_str_verify(stored, pw, strlen(pw)) == 0 ? 0 : -1;
}
