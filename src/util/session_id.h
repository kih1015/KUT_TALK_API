#pragma once
#define SESSION_ID_LEN 64          /* 32 byte → 64 hex */

void kta_generate_session_id(char out[SESSION_ID_LEN + 1]);