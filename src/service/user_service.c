#include "service/user_service.h"

#include <time.h>

#include "repository/session_repository.h"
#include "repository/user_repository.h"
#include "util/password_hash.h"

int user_service_register(
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

int user_service_login(const char *uid,
                       const char *pw,
                       char        sid[SESSION_ID_LEN + 1])
{
    /* 1) 저장된 패스워드 해시 조회 */
    char stored[PWD_HASH_LEN] = {0};
    int rc = user_repository_get_hash(uid, stored, sizeof stored);
    if (rc != 0)           /* 1 = userid 없음, 음수 = DB 오류 */
        return (rc == 1) ? -1 : -2;

    /* 2) 비밀번호 검증 */
    if (kta_password_verify(pw, stored) != 0)
        return -1;

    /* 3) 세션 ID 생성 및 저장 */
    kta_generate_session_id(sid);
    time_t exp = time(NULL) + 60 * 60 * 24 * 7;   /* 7일 */

    if (session_repository_add(sid, uid, exp) != 0)
        return -2;

    return 0;                                      /* 성공 */
}
