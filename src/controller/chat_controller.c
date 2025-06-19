#include "chat_controller.h"
#include "service/chat_service.h"
#include "http_server/http_utils.h"
#include <cjson/cJSON.h>
#include <string.h>
#include <stdlib.h>

/* ---------- Cookie 헬퍼 ---------- */
static const char *extract_cookie(const struct http_request *req,
                                  const char *key) {
    const char *hdr = http_get_header(req, "Cookie");
    if (!hdr) return NULL;
    size_t klen = strlen(key);
    const char *p = hdr;
    while ((p = strstr(p, key))) {
        if (p[klen] == '=') return p + klen + 1;
        ++p;
    }
    return NULL;
}

/* ---------- 공통 헬퍼 ---------- */
static int build_json_resp(char *buf, size_t sz, enum http_status st, const char *json_body) {
    const char *reason = http_reason_phrase(st);
    int body_len = json_body ? (int) strlen(json_body) : 0;

    int n = snprintf(
        buf, sz,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s",
        st, reason, body_len,
        json_body ? json_body : ""
    );
    return (n < 0 || (size_t) n >= sz) ? -1 : n;
}

/* ---------- 경로에서 {id} 파싱 ---------- */
static int path_id(const char *path,
                   const char *prefix, const char *suffix,
                   uint32_t *out)
{
    /* 1) 쿼리 문자열 제거 ------------------------------------ */
    size_t full_len = strlen(path);
    size_t plen = full_len;
    const char *q = strchr(path, '?');
    if (q) plen = (size_t)(q - path);          /* '?' 앞까지 */

    /* 2) 접두/접미 길이 계산 ---------------------------------- */
    size_t pre = strlen(prefix);
    size_t suf = strlen(suffix);
    if (plen < pre + suf) return -1;

    /* 3) 접두/접미 비교 -------------------------------------- */
    if (strncmp(path,            prefix, pre) != 0) return -1;
    if (strncmp(path + plen - suf, suffix, suf) != 0) return -1;

    /* 4) 중간의 숫자 부분 추출 -------------------------------- */
    size_t num_len = plen - pre - suf;
    if (num_len == 0 || num_len >= 16) return -1;

    char numbuf[16];
    memcpy(numbuf, path + pre, num_len);
    numbuf[num_len] = '\0';

    char *end;
    unsigned long v = strtoul(numbuf, &end, 10);
    if (*end) return -1;

    *out = (uint32_t)v;
    return 0;
}

/* ───── 쿠키 추출 ───── */
static const char *cookie_get(const struct http_request *req, const char *key) {
    const char *hdr = http_get_header(req, "Cookie");
    if (!hdr) return NULL;
    size_t k = strlen(key);
    const char *p = hdr;
    while ((p = strstr(p, key))) {
        if (p[k] == '=') return p + k + 1;
        ++p;
    }
    return NULL;
}

/* ───── 쿼리 ?page= & limit= 해석 (기본 page=0, limit=50) ───── */
static void page_limit_from_path(const char *path, size_t *page, size_t *limit) {
    *page = 0;
    *limit = 50;
    const char *q = strchr(path, '?');
    if (!q) { return; }
    ++q;
    while (*q) {
        if (strncmp(q, "page=", 5) == 0) { *page = (size_t) atoi(q + 5); } else if (strncmp(q, "limit=", 6) == 0) {
            *limit = (size_t) atoi(q + 6);
        }
        q = strchr(q, '&');
        if (!q) break;
        ++q;
    }
    if (*limit == 0 || *limit > 200) *limit = 200;
}

/* ---------- 1. 방 생성 ----------  POST /chat/rooms */
int chat_controller_create_room(const struct http_request *req,
                                char *resp, size_t sz) {
    const char *sid = extract_cookie(req, "KTA_SESSION_ID");
    if (!sid)
        return build_json_resp(resp, sz, HTTP_UNAUTHORIZED,
                               "{\"error\":\"No session\"}");

    if (!req->body)
        return build_json_resp(resp, sz, HTTP_BAD_REQUEST,
                               "{\"error\":\"Missing body\"}");

    cJSON *root = cJSON_Parse(req->body);
    if (!root)
        return build_json_resp(resp, sz, HTTP_BAD_REQUEST,
                               "{\"error\":\"Invalid JSON\"}");

    cJSON *jtitle = cJSON_GetObjectItem(root, "title");
    cJSON *jtype = cJSON_GetObjectItem(root, "room_type");
    cJSON *jmem = cJSON_GetObjectItem(root, "member_ids");

    if (!cJSON_IsString(jtitle) || !cJSON_IsString(jtype)) {
        cJSON_Delete(root);
        return build_json_resp(resp, sz, HTTP_BAD_REQUEST,
                               "{\"error\":\"title/type required\"}");
    }
    enum room_type rt = strcmp(jtype->valuestring, "PRIVATE") == 0
                            ? ROOM_PRIVATE
                            : ROOM_PUBLIC;

    uint32_t mids[50];
    size_t mcnt = 0;
    if (cJSON_IsArray(jmem)) {
        cJSON *e;
        cJSON_ArrayForEach(e, jmem) {
            if (cJSON_IsNumber(e) && mcnt < 50)
                mids[mcnt++] = (uint32_t) e->valuedouble;
        }
    }

    uint32_t room_id;
    int rc = chat_service_create_room(sid, rt,
                                      jtitle->valuestring,
                                      mids, mcnt, &room_id);
    cJSON_Delete(root);

    if (rc == CHAT_ERR_BAD_SESSION)
        return build_json_resp(resp, sz, HTTP_UNAUTHORIZED,
                               "{\"error\":\"Invalid session\"}");
    if (rc != 0)
        return build_json_resp(resp, sz, HTTP_INTERNAL_ERROR,
                               "{\"error\":\"DB error\"}");

    char body[64];
    snprintf(body, sizeof body, "{\"room_id\":%u}", room_id);
    return build_json_resp(resp, sz, HTTP_CREATED, body);
}

/* ---------- 2. 내 로비 ----------  GET /chat/rooms */
int chat_controller_my_rooms(const struct http_request *req,
                             char *resp, size_t sz) {
    const char *sid = extract_cookie(req, "KTA_SESSION_ID");
    if (!sid)
        return build_json_resp(resp, sz, HTTP_UNAUTHORIZED,
                               "{\"error\":\"No session\"}");

    struct lobby_room list[100];
    size_t cnt;
    int rc = chat_service_my_rooms(sid, list, 100, &cnt);

    if (rc == CHAT_ERR_BAD_SESSION)
        return build_json_resp(resp, sz, HTTP_UNAUTHORIZED,
                               "{\"error\":\"Invalid session\"}");
    if (rc != 0)
        return build_json_resp(resp, sz, HTTP_INTERNAL_ERROR,
                               "{\"error\":\"DB error\"}");

    cJSON *arr = cJSON_CreateArray();
    for (size_t i = 0; i < cnt; ++i) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "room_id", list[i].id);
        cJSON_AddStringToObject(o, "title", list[i].title);
        cJSON_AddStringToObject(o, "last_content", list[i].last_content);
        cJSON_AddNumberToObject(o, "last_sender", list[i].last_sender);
        cJSON_AddNumberToObject(o, "last_time", list[i].last_time);
        cJSON_AddNumberToObject(o, "unread", list[i].unread);
        cJSON_AddNumberToObject(o, "member_cnt", list[i].member_cnt);
        cJSON_AddItemToArray(arr, o);
    }
    char *js = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    int ret = build_json_resp(resp, sz, HTTP_OK, js);
    free(js);
    return ret;
}

/* ---------- 3. Public 목록 ----------  GET /chat/rooms/public */
int chat_controller_list_public(const struct http_request *req,
                                char *resp, size_t sz) {
    struct public_room list[50];
    size_t cnt;
    if (chat_service_list_public(list, 50, &cnt) != 0)
        return build_json_resp(resp, sz, HTTP_INTERNAL_ERROR,
                               "{\"error\":\"DB error\"}");

    cJSON *arr = cJSON_CreateArray();
    for (size_t i = 0; i < cnt; ++i) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "room_id", list[i].id);
        cJSON_AddStringToObject(o, "title", list[i].title);
        cJSON_AddNumberToObject(o, "member_cnt", list[i].member_cnt);
        cJSON_AddItemToArray(arr, o);
    }
    char *js = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    int ret = build_json_resp(resp, sz, HTTP_OK, js);
    free(js);
    return ret;
}

/* ---------- 4. 방 참가 ----------  POST /chat/rooms/{id}/join */
int chat_controller_join(const struct http_request *req,
                         char *resp, size_t sz) {
    const char *sid = extract_cookie(req, "KTA_SESSION_ID");
    if (!sid)
        return build_json_resp(resp, sz, HTTP_UNAUTHORIZED,
                               "{\"error\":\"No session\"}");

    uint32_t room;
    if (path_id(req->path, "/chat/rooms/", "/join", &room) != 0)
        return build_json_resp(resp, sz, HTTP_BAD_REQUEST,
                               "{\"error\":\"Bad path\"}");

    int rc = chat_service_join(sid, room);
    if (rc == CHAT_ERR_BAD_SESSION)
        return build_json_resp(resp, sz, HTTP_UNAUTHORIZED,
                               "{\"error\":\"Invalid session\"}");
    if (rc == -1)
        return build_json_resp(resp, sz, HTTP_CONFLICT,
                               "{\"error\":\"Already joined\"}");
    if (rc != 0)
        return build_json_resp(resp, sz, HTTP_INTERNAL_ERROR,
                               "{\"error\":\"DB error\"}");

    return build_json_resp(resp, sz, HTTP_NO_CONTENT,NULL);
}

/* ---------- 5. 방 탈퇴 ----------  DELETE /chat/rooms/{id}/member */
int chat_controller_leave(const struct http_request *req,
                          char *resp, size_t sz) {
    const char *sid = extract_cookie(req, "KTA_SESSION_ID");
    if (!sid)
        return build_json_resp(resp, sz, HTTP_UNAUTHORIZED,
                               "{\"error\":\"No session\"}");

    uint32_t room;
    if (path_id(req->path, "/chat/rooms/", "/member", &room) != 0)
        return build_json_resp(resp, sz, HTTP_BAD_REQUEST,
                               "{\"error\":\"Bad path\"}");

    int rc = chat_service_leave(sid, room);
    if (rc == CHAT_ERR_BAD_SESSION)
        return build_json_resp(resp, sz, HTTP_UNAUTHORIZED,
                               "{\"error\":\"Invalid session\"}");
    if (rc == -1)
        return build_json_resp(resp, sz, HTTP_NOT_FOUND,
                               "{\"error\":\"Not a member\"}");
    if (rc != 0)
        return build_json_resp(resp, sz, HTTP_INTERNAL_ERROR,
                               "{\"error\":\"DB error\"}");

    return build_json_resp(resp, sz, HTTP_NO_CONTENT,NULL);
}

int chat_controller_get_messages(
    const struct http_request *req,
    char *resp, size_t sz
) {
    const char *sid = cookie_get(req, "KTA_SESSION_ID");
    if (!sid)
        return build_json_resp(resp, sz, HTTP_UNAUTHORIZED,
                               "{\"error\":\"No session\"}");

    uint32_t room;
    if (path_id(req->path, "/chat/rooms/", "/messages", &room) != 0)
        return build_json_resp(resp, sz, HTTP_BAD_REQUEST,
                               "{\"error\":\"Bad path\"}");

    size_t page, limit;
    page_limit_from_path(req->path, &page, &limit);

    struct chat_msg_dto msgs[200];
    size_t cnt;
    int rc = chat_service_get_messages(sid, room, page, limit, msgs, &cnt);

    if (rc == CHAT_ERR_BAD_SESSION)
        return build_json_resp(resp, sz, HTTP_UNAUTHORIZED,
                               "{\"error\":\"Invalid session\"}");
    if (rc != 0)
        return build_json_resp(resp, sz, HTTP_INTERNAL_ERROR,
                               "{\"error\":\"DB error\"}");

    cJSON *arr = cJSON_CreateArray();
    for (size_t i = 0; i < cnt; ++i) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", msgs[i].id);
        cJSON_AddNumberToObject(o, "sender", msgs[i].sender_id);
        cJSON_AddStringToObject(o, "sender_nick", msgs[i].sender_nick);
        cJSON_AddStringToObject(o, "content", msgs[i].content);
        cJSON_AddNumberToObject(o, "created_at", msgs[i].created_at);
        cJSON_AddNumberToObject(o, "unread_cnt", msgs[i].unread_cnt);
        cJSON_AddItemToArray(arr, o);
    }
    char *body = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    int ret = build_json_resp(resp, sz, HTTP_OK, body);
    free(body);
    return ret;
}
