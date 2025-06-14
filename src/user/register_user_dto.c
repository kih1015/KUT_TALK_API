#ifndef REGISTER_USER_DTO_H
#define REGISTER_USER_DTO_H

#define NICKNAME_MAX 64
#define USERID_MAX   64
#define PASSWORD_MAX 64

typedef struct {
    char nickname[NICKNAME_MAX];
    char userid[USERID_MAX];
    char password[PASSWORD_MAX];
} register_user_dto_t;

#endif
