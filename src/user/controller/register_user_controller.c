#include "register_user_controller.h"
#include "../../register_user_dto.h"
#include "../../service/register_user_service.h"
#include <cjson/cJSON.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

int handle_register_user(int client_fd, const char *body) {
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

    const char *resp = NULL;

    if (result == 0) {
        resp = "HTTP/1.1 201 Created\r\n"
               "Content-Type: application/json\r\n"
               "Content-Length: 29\r\n\r\n"
               "{\"result\":\"User Registered\"}";
    } else if (result == -1) {
        resp = "HTTP/1.1 409 Conflict\r\n"
               "Content-Type: application/json\r\n"
               "Content-Length: 33\r\n\r\n"
               "{\"error\":\"UserID already exists\"}";
    } else {
        resp = "HTTP/1.1 507 Insufficient Storage\r\n"
               "Content-Type: application/json\r\n"
               "Content-Length: 29\r\n\r\n"
               "{\"error\":\"User limit reached\"}";
    }

    write(client_fd, resp, strlen(resp));
    cJSON_Delete(root);
    return 0;
}
