#pragma once
#include "util/session_id.h"

int user_service_register(
    const char *userid,
    const char *nickname,
    const char *password
);

int user_service_login(const char *uid, const char *pw, char sid[SESSION_ID_LEN + 1]);
