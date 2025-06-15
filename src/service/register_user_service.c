#include "service/register_user_service.h"
#include "repository/user_repository.h"

int register_user_service(
    const char *userid,
    const char *nickname,
    const char *password
) {
    // 1) userid 중복 검사
    int uexists = user_repository_exists(userid);
    if (uexists < 0) {
        // DB 에러
        return -2;
    }
    if (uexists > 0) {
        // userid 중복
        return -1;
    }

    // 2) nickname 중복 검사
    int nexists = user_repository_nickname_exists(nickname);
    if (nexists < 0) {
        // DB 에러
        return -2;
    }
    if (nexists > 0) {
        // nickname 중복 — 같은 에러 코드로 처리하거나, 구분하고 싶다면 별도 음수 코드 사용
        return -1;
    }

    // 3) 정상 저장
    int rc = user_repository_add(userid, nickname, password);
    return (rc == 0 ? 0 : -2);
}
