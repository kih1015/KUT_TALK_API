#include "register_user_controller.h"
#include "user/register_user_dto.h"
#include "user/service/register_user_service.h"
#include "http_response.h"
#include <cjson/cJSON.h>
#include <string.h>

int register_user_controller(const char *body, http_response_t *out_res) {
    cJSON *root = cJSON_Parse(body);
    if (!root) return 400;

    cJSON *jnick = cJSON_GetObjectItem(root, "nickname");
    cJSON *juid  = cJSON_GetObjectItem(root, "userid");
    cJSON *jpwd  = cJSON_GetObjectItem(root, "password");

    if (!cJSON_IsString(jnick) || !cJSON_IsString(juid) || !cJSON_IsString(jpwd)) {
        cJSON_Delete(root);
        return 400;
    }

    register_user_dto_t dto;
    strncpy(dto.nickname, jnick->valuestring, NICKNAME_MAX - 1);
    strncpy(dto.userid,   juid->valuestring,   USERID_MAX - 1);
    strncpy(dto.password, jpwd->valuestring, PASSWORD_MAX - 1);
    dto.nickname[NICKNAME_MAX-1] = '\0';
    dto.userid[USERID_MAX-1]     = '\0';
    dto.password[PASSWORD_MAX-1] = '\0';

    int result = register_user_service(&dto);

    cJSON_Delete(root);

    // 응답 구성
    if (result == 0) {
        http_response_init(out_res, 201, "Created");
        http_response_set_header(out_res, "Content-Type", "application/json");
        http_response_set_body(out_res, "{\"result\":\"User Registered\"}", strlen("{\"result\":\"User Registered\"}"));
    }
    else if (result == -1) {
        http_response_init(out_res, 409, "Conflict");
        http_response_set_header(out_res, "Content-Type", "application/json");
        http_response_set_body(out_res, "{\"error\":\"UserID already exists\"}", strlen("{\"error\":\"UserID already exists\"}"));
    }
    else {
        http_response_init(out_res, 507, "Insufficient Storage");
        http_response_set_header(out_res, "Content-Type", "application/json");
        http_response_set_body(out_res, "{\"error\":\"User limit reached\"}", strlen("{\"error\":\"User limit reached\"}"));
    }

    return 0;
}
