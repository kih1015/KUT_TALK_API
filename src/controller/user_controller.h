#pragma once
#include "http_server//http_status.h"
#include "util/session_id.h"

int user_controller_register(
    const char *userid,
    const char *nickname,
    const char *password
);

struct login_ctl_result {
    enum http_status status;
    char session_id[SESSION_ID_LEN + 1];
};

struct login_ctl_result user_controller_login(const char *uid, const char *pw);
