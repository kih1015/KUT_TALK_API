# REST API Reference

## Authentication

### `POST /users/login`

로그인하여 세션을 생성합니다.
**Request**

```json
{
  "userid": "string",
  "password": "string"
}
```

**Response**

* **200 OK**

    * Set-Cookie 헤더에 세션 ID (`KTA_SESSION_ID`)
    * JSON body:

      ```json
      { "sid": "SESSION_ID" }
      ```
* **401 Unauthorized**

  ```json
  { "error": "Invalid credentials" }
  ```
* **400 Bad Request**
* **500 Internal Server Error**

---

### `POST /users`

새 사용자(회원) 등록
**Request**

```json
{
  "userid":   "string",
  "nickname": "string",
  "password": "string"
}
```

**Response**

* **201 Created**
* **409 Conflict**

  ```json
  { "error": "Duplicate userid/nickname" }
  ```
* **400 Bad Request**
* **500 Internal Server Error**

---

### `GET /users/me`

내 프로필 정보 조회
**Request**

* Cookie: `KTA_SESSION_ID`
  **Response**
* **200 OK**

  ```json
  {
    "userid":   "string",
    "nickname": "string"
  }
  ```
* **401 Unauthorized**

---

### `POST /users/logout`

로그아웃
**Request**

* Cookie: `KTA_SESSION_ID`
  **Response**
* **204 No Content**
* **401 Unauthorized**

---

## Chat Rooms

### `POST /chat/rooms`

새 채팅방 생성
**Request**

* Cookie: `KTA_SESSION_ID`
* JSON body:

  ```json
  {
    "room_type": "PUBLIC" | "PRIVATE",
    "title":     "string",
    "member_ids":[ /* PRIVATE 방에 추가할 user_id 리스트 */ ]
  }
  ```

**Response**

* **201 Created**

  ```json
  { "room_id": number }
  ```
* **401 Unauthorized**
* **400 Bad Request**
* **500 Internal Server Error**

---

### `GET /chat/rooms/me`

내가 속한 채팅방 목록 조회
**Request**

* Cookie: `KTA_SESSION_ID`
  **Response**
* **200 OK**

  ```json
  [
    {
      "room_id":     number,
      "title":       "string",
      "last_content":"string",
      "last_sender": number,
      "last_time":   unix_timestamp,
      "unread":      number,
      "member_cnt":  number
    },
    …
  ]
  ```
* **401 Unauthorized**
* **500 Internal Server Error**

---

### `GET /chat/rooms/public`

공개 채팅방 목록 조회
**Request**

* Cookie: `KTA_SESSION_ID`
  **Response**
* **200 OK**

  ```json
  [
    { "room_id": number, "title": "string", "member_cnt": number },
    …
  ]
  ```
* **500 Internal Server Error**

---

### `POST /chat/rooms/{id}/join`

채팅방 참가
**Request**

* Cookie: `KTA_SESSION_ID`
* Path parameter `id`: room\_id
  **Response**
* **204 No Content**
* **401 Unauthorized**
* **409 Conflict**
* **400 Bad Request**
* **500 Internal Server Error**

---

### `DELETE /chat/rooms/{id}/member`

채팅방 나가기
**Request**

* Cookie: `KTA_SESSION_ID`
* Path parameter `id`: room\_id
  **Response**
* **204 No Content**
* **401 Unauthorized**
* **404 Not Found**
* **400 Bad Request**
* **500 Internal Server Error**

---

### `GET /chat/rooms/{id}/messages?page={n}&limit={m}`

특정 채팅방 메시지 조회 (페이징)
**Request**

* Cookie: `KTA_SESSION_ID`
* Path parameter `id`: room\_id
* Query parameters:

    * `page`  (0-based, 기본 0)
    * `limit` (기본 50, 최대 200)
      **Response**
* **200 OK**

  ```json
  [
    {
      "id":          number,
      "sender":      number,
      "sender_nick": "string",
      "content":     "string",
      "created_at":  unix_timestamp,
      "unread_cnt":  number
    },
    …
  ]
  ```
* **401 Unauthorized**
* **400 Bad Request**
* **500 Internal Server Error**
