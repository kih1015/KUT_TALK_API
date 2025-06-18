#include "adapter/user_adapter.h"
#include "controller/user_controller.h"
#include "http_server/http_status.h"
#include "http_server/http_utils.h"

#include <cjson/cJSON.h>
#include <stdio.h>
#include <string.h>

int user_adapter_register(
    const char *body,
    char *resp_buf,
    size_t buf_size
) {
    int code;
    const char *resp_body = NULL;

    // 1) JSON 파싱
    cJSON *root = cJSON_Parse(body);
    if (!root) {
        code = 400;
        resp_body = "{\"error\":\"Invalid JSON\"}";
    } else {
        cJSON *jnick = cJSON_GetObjectItem(root, "nickname");
        cJSON *juid = cJSON_GetObjectItem(root, "userid");
        cJSON *jpwd = cJSON_GetObjectItem(root, "password");
        if (!cJSON_IsString(jnick) ||
            !cJSON_IsString(juid) ||
            !cJSON_IsString(jpwd)) {
            code = 400;
            resp_body = "{\"error\":\"Missing or invalid fields\"}";
        } else {
            // 2) 컨트롤러 호출: HTTP 상태 코드를 그대로 리턴받음
            int svc = user_controller_register(
                juid->valuestring,
                jnick->valuestring,
                jpwd->valuestring
            );
            code = svc;
            // 3) 상태 코드에 따른 응답 바디 선택
            if (code == 201) {
                resp_body = NULL;
            } else if (code == 409) {
                resp_body = "{\"error\":\"Duplicate userid\"}";
            } else if (code == 507) {
                resp_body = "{\"error\":\"Storage full\"}";
            } else {
                code = 500;
                resp_body = "{\"error\":\"Internal Error\"}";
            }
        }
        cJSON_Delete(root);
    }

    // 4) Reason phrase 결정
    const char *reason =
    (code == 201
         ? "Created"
         : code == 400
               ? "Bad Request"
               : code == 404
                     ? "Not Found"
                     : code == 409
                           ? "Conflict"
                           : code == 507
                                 ? "Insufficient Storage"
                                 : "Internal Server Error");

    // 5) 헤더 + 바디를 resp_buf에 작성
    int body_len = resp_body ? (int) strlen(resp_body) : 0;
    int total = snprintf(
        resp_buf, buf_size,
        "HTTP/1.1 %d %s\r\n"
        "Content-Length: %d\r\n"
        "Content-Type: application/json\r\n"
        "\r\n"
        "%s",
        code, reason, body_len,
        resp_body ? resp_body : ""
    );

    // 6) 버퍼 오버플로우 체크
    if (total < 0 || (size_t) total >= buf_size) {
        return -1;
    }
    return total;
}

int user_adapter_login(const char *body,
                       char *resp_buf, size_t buf_sz) {
    /* 1) JSON 파싱 (userid,password) */
    cJSON *root = cJSON_Parse(body);
    if (!root)
        return build_simple_resp(resp_buf, buf_sz, HTTP_BAD_REQUEST,
                                 "{\"error\":\"Invalid JSON\"}");

    cJSON *juid = cJSON_GetObjectItem(root, "userid");
    cJSON *jpw = cJSON_GetObjectItem(root, "password");
    if (!cJSON_IsString(juid) || !cJSON_IsString(jpw)) {
        cJSON_Delete(root);
        return build_simple_resp(resp_buf, buf_sz, HTTP_BAD_REQUEST,
                                 "{\"error\":\"Missing fields\"}");
    }

    struct login_ctl_result ctl =
            user_controller_login(juid->valuestring, jpw->valuestring);
    cJSON_Delete(root);

    const char *phrase = http_reason_phrase(ctl.status);
    const char *body_ok = "{\"message\":\"Login success\"}";
    const char *body_fail = "{\"error\":\"Invalid credentials\"}";
    const char *resp_body = (ctl.status == HTTP_OK) ? body_ok : body_fail;
    int body_len = strlen(resp_body);

    int n = snprintf(resp_buf, buf_sz,
                     "HTTP/1.1 %d %s\r\n"
                     "Content-Type: application/json\r\n"
                     "Content-Length: %d\r\n"
                     "%s"
                     "\r\n"
                     "%s",
                     ctl.status, phrase, body_len,
                     ctl.status == HTTP_OK
                         ? "Set-Cookie: KTA_SESSION_ID=%s; Path=/; Max-Age=604800;"
                         " HttpOnly; Secure; SameSite=Strict\r\n"
                         : "",
                     resp_body);

    if (ctl.status == HTTP_OK)
        n = snprintf(resp_buf, buf_sz, resp_buf, ctl.session_id);

    return (n < 0 || n >= (int) buf_sz) ? -1 : n;
}
