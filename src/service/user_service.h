#pragma once
#include "util/session_id.h"
#include "repository/user_repository.h"

int user_service_register(
    const char *userid,
    const char *nickname,
    const char *password
);

int user_service_login(const char *uid, const char *pw, char sid[SESSION_ID_LEN + 1]);

int user_service_get_me(const char *session_id, struct user_info *out);

int user_service_logout(const char *session_id);