#pragma once
#include <stdint.h>
#include <time.h>

int session_repository_add(const char *sid, uint32_t user_id, time_t exp);

int session_repository_find_id(const char *sid,
                               uint32_t *out_user_id,
                               time_t *out_exp);

int session_repository_delete(const char *sid);
