#include "user_controller.h"
#include "service/user_service.h"

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
