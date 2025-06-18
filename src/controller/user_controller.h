#pragma once
#include "http_server//http_status.h"
#include "repository/user_repository.h"
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

struct me_ctl_result {
    enum http_status status;
    struct user_info info;
};

struct me_ctl_result user_controller_get_me(const char *session_id);

enum http_status user_controller_logout(const char *session_id);