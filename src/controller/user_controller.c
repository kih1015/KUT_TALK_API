#include "user_controller.h"

#include <stdio.h>

#include "service/user_service.h"
#include "http_server/http_status.h"

int user_controller_register(
    const char *userid,
    const char *nickname,
    const char *password
) {
    int result = user_service_register(
        userid,
        nickname,
        password
    );

    switch (result) {
        case 0: return 201; // Created
        case -1: return 409; // Conflict: UserID exists
        default: return 507; // Insufficient Storage: User limit
    }
}

struct login_ctl_result user_controller_login(const char *uid, const char *pw) {
    struct login_ctl_result r = {HTTP_INTERNAL_ERROR, {0}};
    int rc = user_service_login(uid, pw, r.session_id);
    if (rc == 0) {
        r.status = HTTP_OK;
    }
    else if (rc == -1) {
        r.status = HTTP_UNAUTHORIZED;
    }
    else r.status = HTTP_INTERNAL_ERROR;
    return r;
}
