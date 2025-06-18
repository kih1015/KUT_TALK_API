#pragma once

enum http_status {
    /* 2xx Success */
    HTTP_OK = 200,
    HTTP_CREATED = 201,
    HTTP_NO_CONTENT = 204,

    /* 4xx Client Error */
    HTTP_BAD_REQUEST = 400,
    HTTP_UNAUTHORIZED = 401,
    HTTP_NOT_FOUND = 404,
    HTTP_CONFLICT = 409,

    /* 5xx Server Error */
    HTTP_INTERNAL_ERROR = 500,
    HTTP_INSUFFICIENT_STORAGE = 507
};

static const char *
http_reason_phrase(enum http_status s) {
    switch (s) {
        case HTTP_OK: return "OK";
        case HTTP_CREATED: return "Created";
        case HTTP_NO_CONTENT: return "No Content";

        case HTTP_BAD_REQUEST: return "Bad Request";
        case HTTP_UNAUTHORIZED: return "Unauthorized";
        case HTTP_NOT_FOUND: return "Not Found";
        case HTTP_CONFLICT: return "Conflict";

        case HTTP_INSUFFICIENT_STORAGE: return "Insufficient Storage";
        case HTTP_INTERNAL_ERROR: return "Internal Server Error";

        default: return "Unknown";
    }
}
