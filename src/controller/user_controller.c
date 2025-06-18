#include "controller/user_controller.h"
#include "service/user_service.h"
#include "http_server/http_utils.h"

#include <cjson/cJSON.h>
#include <string.h>

/* --------------------------------------------------------- */
/* 공통 헬퍼                                                 */
/* --------------------------------------------------------- */
static int build_json_resp(char        *buf,
                           size_t       sz,
                           enum http_status st,
                           const char  *json_body)               /* NULL 가능 */
{
    const char *reason = http_reason_phrase(st);
    int body_len      = json_body ? (int)strlen(json_body) : 0;

    int n = snprintf(buf, sz,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s",
        st, reason, body_len,
        json_body ? json_body : "");

    return (n < 0 || (size_t)n >= sz) ? -1 : n;
}

/* 쿠키 헤더에서 값 찾기 (간단 구현) */
static const char *find_cookie(const char *req, const char *name) {
    const size_t nlen = strlen(name);
    const char  *p    = strcasestr(req, "Cookie:");
    while (p) {
        p += 7;
        while (*p && *p != '\r' && *p != '\n') {
            while(*p==' '||*p=='\t') ++p;
            if (strncmp(p,name,nlen)==0 && p[nlen]=='=') return p+nlen+1;
            p = strchr(p,';');
            if (!p) break;
            ++p;
        }
        p = strcasestr(p, "Cookie:");
    }
    return NULL;
}

/* --------------------------------------------------------- */
/* 회원가입                                                  */
/* --------------------------------------------------------- */
int user_controller_register(const char *body,
                             char *resp_buf, size_t buf_sz)
{
    cJSON *root = cJSON_Parse(body);
    if (!root)
        return build_json_resp(resp_buf, buf_sz,
                               HTTP_BAD_REQUEST,
                               "{\"error\":\"Invalid JSON\"}");

    cJSON *jnick = cJSON_GetObjectItem(root,"nickname");
    cJSON *juid  = cJSON_GetObjectItem(root,"userid");
    cJSON *jpwd  = cJSON_GetObjectItem(root,"password");

    if (!cJSON_IsString(jnick)||!cJSON_IsString(juid)||!cJSON_IsString(jpwd)){
        cJSON_Delete(root);
        return build_json_resp(resp_buf, buf_sz,
                               HTTP_BAD_REQUEST,
                               "{\"error\":\"Missing fields\"}");
    }

    int rc = user_service_register(juid->valuestring,
                                   jnick->valuestring,
                                   jpwd->valuestring);
    cJSON_Delete(root);

    switch (rc) {
        case 0:  /* 201 Created, 바디 없음 */
            return build_json_resp(resp_buf, buf_sz, HTTP_CREATED, NULL);
        case -1: /* 중복 */
            return build_json_resp(resp_buf, buf_sz, HTTP_CONFLICT,
                                   "{\"error\":\"Duplicate userid/nickname\"}");
        default: /* 내부 오류 */
            return build_json_resp(resp_buf, buf_sz,
                                   HTTP_INTERNAL_ERROR,
                                   "{\"error\":\"Internal error\"}");
    }
}

/* --------------------------------------------------------- */
/* 로그인                                                    */
/* --------------------------------------------------------- */
#define BODY_OK   "{\"message\":\"Login success\"}"
#define BODY_FAIL "{\"error\":\"Invalid credentials\"}"

int user_controller_login(const char *body,
                          char *resp_buf, size_t buf_sz)
{
    cJSON *root = cJSON_Parse(body);
    if (!root)
        return build_json_resp(resp_buf, buf_sz,
                               HTTP_BAD_REQUEST,
                               "{\"error\":\"Invalid JSON\"}");

    cJSON *juid = cJSON_GetObjectItem(root,"userid");
    cJSON *jpw  = cJSON_GetObjectItem(root,"password");
    if (!cJSON_IsString(juid)||!cJSON_IsString(jpw)){
        cJSON_Delete(root);
        return build_json_resp(resp_buf, buf_sz,
                               HTTP_BAD_REQUEST,
                               "{\"error\":\"Missing fields\"}");
    }

    char sid[SESSION_ID_LEN+1] = {0};
    int  rc  = user_service_login(juid->valuestring,
                                  jpw->valuestring,
                                  sid);
    cJSON_Delete(root);

    if (rc == 0) { /* 성공 → 200 + 세션쿠키 */
        int n = snprintf(resp_buf, buf_sz,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Set-Cookie: KTA_SESSION_ID=%s; Path=/; Max-Age=604800; "
            "HttpOnly; Secure; SameSite=Lax\r\n"
            "Content-Length: %zu\r\n"
            "\r\n"
            "%s",
            sid, strlen(BODY_OK), BODY_OK);
        return (n < 0 || (size_t)n >= buf_sz) ? -1 : n;
    }
    /* 실패 */
    return build_json_resp(resp_buf, buf_sz,
                           (rc == -1) ? HTTP_UNAUTHORIZED : HTTP_INTERNAL_ERROR,
                           BODY_FAIL);
}

/* --------------------------------------------------------- */
/* 내 정보 조회                                              */
/* --------------------------------------------------------- */
int user_controller_get_me(const char *raw_req,
                           char *resp_buf, size_t buf_sz)
{
    const char *sid = find_cookie(raw_req, "KTA_SESSION_ID");
    if (!sid)
        return build_json_resp(resp_buf, buf_sz,
                               HTTP_UNAUTHORIZED,
                               "{\"error\":\"No session\"}");

    char sid_val[SESSION_ID_LEN+1]={0};
    strncpy(sid_val, sid, SESSION_ID_LEN);

    struct user_info info;
    int rc = user_service_get_me(sid_val, &info);
    if (rc != 0)
        return build_json_resp(resp_buf, buf_sz,
                               (rc == -1)?HTTP_UNAUTHORIZED:HTTP_INTERNAL_ERROR,
                               "{\"error\":\"Unauthorized\"}");

    char body[256];
    snprintf(body,sizeof body,
             "{\"userid\":\"%s\",\"nickname\":\"%s\"}",
             info.userid, info.nickname);

    return build_json_resp(resp_buf, buf_sz, HTTP_OK, body);
}

/* --------------------------------------------------------- */
/* 로그아웃                                                  */
/* --------------------------------------------------------- */
int user_controller_logout(const char *raw_req,
                           char *resp_buf, size_t buf_sz)
{
    const char *sid = find_cookie(raw_req, "KTA_SESSION_ID");
    if (!sid)
        return build_json_resp(resp_buf, buf_sz,
                               HTTP_UNAUTHORIZED,
                               "{\"error\":\"No session\"}");

    char sid_val[SESSION_ID_LEN+1]={0};
    strncpy(sid_val, sid, SESSION_ID_LEN);

    int rc = user_service_logout(sid_val);
    if (rc != 0)
        return build_json_resp(resp_buf, buf_sz,
                               (rc == -1) ? HTTP_UNAUTHORIZED
                                          : HTTP_INTERNAL_ERROR,
                               "{\"error\":\"Unauthorized\"}");

    /* 204 + 쿠키 삭제 */
    int n = snprintf(resp_buf, buf_sz,
        "HTTP/1.1 204 No Content\r\n"
        "Set-Cookie: KTA_SESSION_ID=deleted; Path=/; Max-Age=0; "
        "HttpOnly; Secure; SameSite=Lax\r\n"
        "Content-Length: 0\r\n\r\n");
    return (n < 0 || (size_t)n >= buf_sz) ? -1 : n;
}
