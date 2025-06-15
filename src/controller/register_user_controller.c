#include "register_user_controller.h"
#include "service/register_user_service.h"
#include <cjson/cJSON.h>

int register_user_controller(
    const char *userid,
    const char *nickname,
    const char *password
) {
    int result = register_user_service(
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

/**
 * Route handler: parses JSON, calls controller
 * Returns only HTTP status code (int) without http_response_t
 */

