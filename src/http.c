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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "http.h"

const char * header_names[HEADER_MAX] = {
    [HEADER_ACCEPT] = "accept",
    [HEADER_CONTENT_TYPE] = "content-type",
    [HEADER_CONTENT_LENGTH] = "content-length",
    [HEADER_USER_AGENT] = "user-agent",
    [HEADER_HOST] = "host"
};

const char * http_method_names[] = {
    [Get] = "GET",
    [Head] = "HEAD",
    [Post] = "POST",
    [Put] = "PUT",
    [Delete] = "DELETE",
    [Connect] = "CONNECT",
    [Options] = "OPTIONS",
    [Trace] = "TRACE",
    [Unknown] = "(Unknown method)"
};
static const size_t http_method_max = ARR_SIZE(http_method_names);

static const char http_version[] = "HTTP/1.1";

// Parses an HTTP request line into an `http_req` object and returns either a status code or
// zero. If a status code is returned, then that should be sent back to the client immediately.
// Otherwise, the remainder of the request should be processed.
static http_status_code parse_req_line(const char * restrict in_buf, char * restrict out_buf, size_t buf_size, struct http_req * req) {
    size_t out_pos = 0;

    while (in_buf[req->seek] != ' ') {
        if (req->seek >= buf_size) {
            return HTTP_BAD_REQUEST;
        }

        req->seek++;
    }

    for (int i = 0; i < http_method_max; i++) {
        if (! strncmp(in_buf, http_method_names[i], req->seek)) {
            req->method = i;
            break;
        }
    }

    if (req->method == Unknown || req->method >= Post) {
        return HTTP_METHOD_NOT_IMPLEMENTED;
    }

    // Consume the space
    req->seek++;

    // A request target must not contain whitespace. For now, we'll just keep reading until we hit
    // a space.
    while (in_buf[req->seek] != ' ') {
        if (req->seek >= buf_size) {
            return HTTP_URI_TOO_LONG;
        }

        out_buf[out_pos++] = in_buf[req->seek++];
    }

    out_buf[out_pos++] = 0;

    req->target = malloc(out_pos);
    memcpy(req->target, out_buf, out_pos);

    // Consume the space
    req->seek++;

    // At this point we can just compare the remaining few characters to the expected HTTP
    // version string and fail if they don't match
    if (strncmp(in_buf + req->seek, http_version, ARR_SIZE(http_version) - 1)) {
        return HTTP_VERSION_NOT_SUPPORTED;
    }

    req->seek += ARR_SIZE(http_version) - 1;

    if (req->seek >= (buf_size - 2)) {
        return HTTP_URI_TOO_LONG;
    }

    if (in_buf[req->seek++] != '\r') {
        return HTTP_BAD_REQUEST;
    }

    if (in_buf[req->seek++] != '\n') {
        return HTTP_BAD_REQUEST;
    }

    return 0;
}

struct http_req create_http_req() {
    struct http_req out = {
        .target = NULL,
        .method = Unknown,
        .seek = 0
    };

    return out;
}

void free_http_req(struct http_req * req) {
    // TODO: Free headers

    if (req->target) {
        free(req->target);
    }
}

struct http_res handle_http_req(const char * restrict in_buf, char * restrict out_tmp, size_t buf_size, struct http_req * req) {
    http_status_code req_line_status = parse_req_line(in_buf, out_tmp, buf_size, req);

    // TODO: Read headers

    if (req_line_status) {
        struct http_res out = {
            .status = req_line_status
        };

        return out;
    }

    struct http_res out = {
        .status = HTTP_OK
    };

    return out;
}

static void status_to_str(http_status_code status, char out[4]) {
    int d0 = status / 100;
    int d1 = (status % 100) / 10;
    int d2 = status % 10;

    out[0] = d0 + '0';
    out[1] = d1 + '0';
    out[2] = d2 + '0';
    out[3] = 0;
}

void send_http_res(struct http_res * res, int out_sock_fd) {
    // TODO: Buffered write

    write(out_sock_fd, http_version, ARR_SIZE(http_version) - 1);
    write(out_sock_fd, " ", 1);

    char status[4];
    status_to_str(res->status, status);

    write(out_sock_fd, status, 3);
    write(out_sock_fd, " \r\n\r\n", 5);
}
