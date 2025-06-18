#include "adapter/user_adapter.h"
#include "controller/user_controller.h"
#include "http_server/http_status.h"
#include "http_server/http_utils.h"

#include <cjson/cJSON.h>
#include <stdio.h>
#include <string.h>

#define COOKIE_MAX 160        /* 쿠키 헤더 임시 버퍼 크기 */
#define BODY_OK    "{\"message\":\"Login success\"}"
#define BODY_FAIL  "{\"error\":\"Invalid credentials\"}"

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
    /* ---------- 1) JSON 파싱 ---------- */
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

    /* ---------- 2) 컨트롤러 호출 ---------- */
    struct login_ctl_result ctl =
            user_controller_login(juid->valuestring, jpw->valuestring);
    cJSON_Delete(root);

    /* ---------- 3) 응답 본문 & 쿠키 준비 ---------- */
    const char *resp_body = (ctl.status == HTTP_OK) ? BODY_OK : BODY_FAIL;
    int body_len = (int) strlen(resp_body);

    char cookie_hdr[COOKIE_MAX] = "";
    if (ctl.status == HTTP_OK) {
        /* 세션 쿠키 헤더 문자열 완성 */
        snprintf(cookie_hdr, sizeof cookie_hdr,
                 "Set-Cookie: KTA_SESSION_ID=%s; "
                 "Path=/; Max-Age=604800; HttpOnly; Secure; SameSite=Lax\r\n",
                 ctl.session_id);
    }

    /* ---------- 4) 최종 HTTP 응답 직렬화 ---------- */
    int n = snprintf(resp_buf, buf_sz,
                     "HTTP/1.1 %d %s\r\n"
                     "Content-Type: application/json\r\n"
                     "Content-Length: %d\r\n"
                     "%s" /* 쿠키 헤더 또는 빈 문자열 */
                     "\r\n"
                     "%s",
                     ctl.status, http_reason_phrase(ctl.status),
                     body_len,
                     cookie_hdr,
                     resp_body);

    return (n < 0 || (size_t) n >= buf_sz) ? -1 : n; /* -1 = 버퍼 부족 */
}

static const char *find_cookie(const char *req, const char *name) {
    const char *p = req;
    size_t nlen = strlen(name);
    while ((p = strcasestr(p, "Cookie:")) != NULL) {
        p += 7;
        while (*p && *p != '\r' && *p != '\n') {
            while (*p == ' ' || *p == '\t') ++p;
            if (strncmp(p, name, nlen) == 0 && p[nlen] == '=') {
                return p + nlen + 1;
            }
            p = strchr(p, ';');
            if (!p) break;
            ++p;
        }
    }
    return NULL;
}

int user_adapter_get_me(const char *req,
                        char *resp_buf, size_t buf_sz) {
    const char *sid = find_cookie(req, "KTA_SESSION_ID");
    if (!sid) /* 쿠키 없으면 401 */
        return build_simple_resp(resp_buf, buf_sz, HTTP_UNAUTHORIZED,
                                 "{\"error\":\"No session\"}");

    char sid_val[65] = {0};
    strncpy(sid_val, sid, 64);

    struct me_ctl_result ctl = user_controller_get_me(sid_val);
    if (ctl.status != HTTP_OK)
        return build_simple_resp(resp_buf, buf_sz, ctl.status,
                                 "{\"error\":\"Unauthorized\"}");

    char body[512];
    int bl = snprintf(body, sizeof body,
                      "{\"userid\":\"%s\",\"nickname\":\"%s\"}",
                      ctl.info.userid, ctl.info.nickname);

    return build_simple_resp(resp_buf, buf_sz, HTTP_OK, body);
}

int user_adapter_logout(const char *req, char *resp_buf, size_t buf_sz) {
    const char *sid = find_cookie(req, "KTA_SESSION_ID");
    if (!sid) /* 쿠키 없음 */
        return build_simple_resp(resp_buf, buf_sz,
                                 HTTP_UNAUTHORIZED,
                                 "{\"error\":\"No session\"}");

    char sid_val[65] = {0};
    strncpy(sid_val, sid, 64);

    enum http_status st = user_controller_logout(sid_val);

    /* 성공 → 쿠키 삭제용 헤더 */
    if (st == HTTP_NO_CONTENT) {
        int n = snprintf(resp_buf, buf_sz,
                         "HTTP/1.1 204 No Content\r\n"
                         "Set-Cookie: KTA_SESSION_ID=deleted; "
                         "Domain=api.kuttalk.kro.kr; Path=/; Max-Age=0; "
                         "HttpOnly; Secure; SameSite=Lax\r\n"
                         "Content-Length: 0\r\n\r\n");
        return (n < 0 || (size_t) n >= buf_sz) ? -1 : n;
    }

    /* 실패 */
    return build_simple_resp(resp_buf, buf_sz, st,
                             "{\"error\":\"Unauthorized\"}");
}
