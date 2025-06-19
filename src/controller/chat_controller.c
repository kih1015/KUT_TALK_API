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
                   uint32_t *out) {
    size_t pre = strlen(prefix);
    if (strncmp(path, prefix, pre) != 0) return -1;
    char *end;
    unsigned long v = strtoul(path + pre, &end, 10);
    if (end == path + pre || strcmp(end, suffix) != 0) return -1;
    *out = (uint32_t) v;
    return 0;
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
