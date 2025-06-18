#pragma once

#include <stddef.h>

/// @param body       요청 JSON 페이로드
/// @param resp_buf   응답용 버퍼 (헤더+바디가 여기에 쓰임)
/// @param buf_size   resp_buf의 크기
/// @return           HTTP 응답 전체 길이 (bytes), 또는 음수 on error
int register_user_adapter(
    const char *body,
    char       *resp_buf,
    size_t      buf_size
);
