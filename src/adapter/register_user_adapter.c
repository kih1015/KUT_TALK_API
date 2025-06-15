#include "register_user_adapter.h"

#include <cjson/cJSON.h>

#include "controller/register_user_controller.h"

int register_user_adapter(const char *body) {
    cJSON *root = cJSON_Parse(body);
    if (!root) {
        return 400; // Bad Request: Invalid JSON
    }

    cJSON *jnick = cJSON_GetObjectItem(root, "nickname");
    cJSON *juid = cJSON_GetObjectItem(root, "userid");
    cJSON *jpwd = cJSON_GetObjectItem(root, "password");

    if (!cJSON_IsString(jnick) || !cJSON_IsString(juid) || !cJSON_IsString(jpwd)) {
        cJSON_Delete(root);
        return 400; // Bad Request: Missing or invalid fields
    }

    int status = register_user_controller(
        jnick->valuestring,
        juid->valuestring,
        jpwd->valuestring
    );
    cJSON_Delete(root);

    return status;
}
