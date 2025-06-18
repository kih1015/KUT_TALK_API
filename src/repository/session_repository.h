#pragma once
#include <time.h>

int session_repository_add(const char *sid, const char *uid, time_t exp);
int session_repository_find(const char *sid, char *out_uid, time_t *out_exp);
int session_repository_delete(const char *sid);
