/*
 * This file is part of gru-http, an HTTP server.
 * Copyright (C) 2024  Joe Desmond
 *
 * gru-http is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * gru-http is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with gru-http.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef SRC_HTTP_H
#define SRC_HTTP_H
#include <stdlib.h>
#include "status.h"

#define REQ_HEADER_ACCEPT           0
#define REQ_HEADER_CONTENT_TYPE     1
#define REQ_HEADER_CONTENT_LENGTH   2
#define REQ_HEADER_USER_AGENT       3
#define REQ_HEADER_HOST             4
#define REQ_HEADER_MAX              5

#define RES_HEADER_CONTENT_LENGTH   0
#define RES_HEADER_CONTENT_TYPE     1
#define RES_HEADER_MAX              2

#define ARR_SIZE(arr)           ((sizeof (arr)) / sizeof ((arr)[0]))

extern const char * req_header_names[REQ_HEADER_MAX];
extern const char * res_header_names[RES_HEADER_MAX];

struct req_headers {
    char * known[REQ_HEADER_MAX];
    // TODO: Custom headers
};

struct res_headers {
    char * headers[RES_HEADER_MAX];
};

enum http_method {
    Get = 0,
    Head = 1,
    // POST and everything below are unsupported
    Post = 2,
    Put = 3,
    Delete = 4,
    Connect = 5,
    Options = 6,
    Trace = 7,
    Unknown = 64,
};

extern const char * http_method_names[];

struct http_req {
    struct req_headers headers;
    char * target;
    enum http_method method;
    size_t seek;
};
struct http_req create_http_req();
void free_http_req(struct http_req * req);

struct http_res {
    struct res_headers headers;
    char * content;
    http_status_code status;
};
struct http_res create_http_res();
void free_http_res(struct http_res * res);

struct http_res handle_http_req(const char * in_buf, size_t buf_size, struct http_req * req);
void send_http_res(struct http_res * res, int out_sock_fd);

#endif
