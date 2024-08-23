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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "files.h"
#include "http.h"

#define write_sock(...)     { \
    int result = send(__VA_ARGS__, MSG_NOSIGNAL); \
    if (result == -1) { \
        perror("Failed to write to socket"); \
        return; \
    } \
} noop()

static void noop() {}

struct server_options global_options = {
    .cache_option = DefaultUseCache
};

const char * req_header_names[REQ_HEADER_MAX] = {
    [REQ_HEADER_ACCEPT] = "Accept",
    [REQ_HEADER_CACHE_CONTROL] = "Cache-Control",
    [REQ_HEADER_CONTENT_TYPE] = "Content-Type",
    [REQ_HEADER_CONTENT_LENGTH] = "Content-Length",
    [REQ_HEADER_HOST] = "Host",
    [REQ_HEADER_USER_AGENT] = "User-Agent"
};

const char * res_header_names[RES_HEADER_MAX] = {
    [RES_HEADER_CONTENT_LENGTH] = "Content-Length",
    [RES_HEADER_CONTENT_TYPE] = "Content-Type"
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

// We (loosely) support 1.0 and 1.1
static const char * http_versions[] = {
    "HTTP/1.0",
    "HTTP/1.1"
};

static const char http_version_out[] = "HTTP/1.1";

// Parses an HTTP request line into an `http_req` object and returns either a status code or
// zero. If a status code is returned, then that should be sent back to the client immediately.
// Otherwise, the remainder of the request should be processed.
static http_status_code parse_req_line(const char * restrict in_buf, size_t buf_size, struct http_req * req) {
    while (in_buf[req->seek] != ' ') {
        if (req->seek >= buf_size - 1) {
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
    size_t seek_end = req->seek;

    // A request target must not contain whitespace. For now, we'll just keep reading until we hit
    // a space.
    while (in_buf[seek_end] != ' ') {
        if (seek_end >= buf_size - 1) {
            return HTTP_URI_TOO_LONG;
        }

        seek_end++;
    }

    req->target = malloc(seek_end - req->seek + 1);
    memcpy(req->target, in_buf + req->seek, seek_end - req->seek);
    req->target[seek_end - req->seek] = 0;

    // Consume the space
    req->seek = seek_end + 1;

    // At this point we can just compare the remaining few characters to the expected HTTP
    // version string and fail if they don't match
    for (size_t i = 0; i < ARR_SIZE(http_versions); i++) {
        size_t version_len = strlen(http_versions[i]);

        if (! strncmp(in_buf + req->seek, http_versions[i], version_len)) {
            req->seek += version_len;
            goto version_found;
        }
    }

    return HTTP_VERSION_NOT_SUPPORTED;

version_found:

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

static char lowercase(char in) {
    if ('A' <= in && in <= 'Z') {
        return (in - 'A') + 'a';
    }

    return in;
}

static int is_whitespace(char in) {
    return in == ' ' || in == '\t';
}

static int is_known_header(const char * restrict header_in, size_t known_header_i, size_t header_in_len) {
    size_t i = 0;

    const char * restrict known_header = req_header_names[known_header_i];
    const size_t known_header_len = strlen(known_header);

    if (known_header_len != header_in_len) {
        return 0;
    }

    const size_t n = header_in_len;

    while (i < n && header_in[i] && known_header[i]) {
        if (lowercase(header_in[i]) != lowercase(known_header[i])) {
            break;
        }

        i++;
    }

    if (i < n) {
        return ((unsigned char) lowercase(header_in[i])) == ((unsigned char) lowercase(known_header[i]));
    }

    return 1;
}

static http_status_code parse_field_line(const char * in_buf, size_t buf_size, struct http_req * req) {
    size_t seek_end = req->seek;

    while (in_buf[seek_end] != ':') {
        if (seek_end >= buf_size - 1) {
            return HTTP_BAD_REQUEST;
        }

        seek_end++;
    }

    int req_header = -1;

    for (int i = 0; i < REQ_HEADER_MAX; i++) {
        if (is_known_header(in_buf + req->seek, i, seek_end - req->seek)) {
            req_header = i;
            break;
        }
    }

    req->seek = seek_end + 1;

    // TODO: Custom headers. No idea what we would even do with these

    while (is_whitespace(in_buf[req->seek])) {
        if (req->seek >= buf_size - 1) {
            return HTTP_BAD_REQUEST;
        }

        req->seek++;
    }

    seek_end = req->seek;

    while (in_buf[seek_end] != '\r') {
        if (seek_end >= buf_size - 3) {
            return HTTP_BAD_REQUEST;
        }

        seek_end++;
    }

    if (in_buf[seek_end + 1] != '\n') {
        return HTTP_BAD_REQUEST;
    }

    while (is_whitespace(in_buf[seek_end])) {
        seek_end--;
    }

    if (req_header == -1) {
        req->seek = seek_end + 2;
        return 0;
    }

    req->headers.known[req_header] = malloc(seek_end - req->seek + 1);
    memcpy(req->headers.known[req_header], in_buf + req->seek, seek_end - req->seek);
    req->headers.known[req_header][seek_end - req->seek] = 0;

    req->seek = seek_end + 2;

    return 0;
}

static http_status_code parse_field_lines(const char * in_buf, size_t buf_size, struct http_req * req) {
    http_status_code last_line_result = 0;

    if (req->seek >= buf_size) {
        return HTTP_BAD_REQUEST;
    }

    while (in_buf[req->seek] != '\r' && ! last_line_result) {
        last_line_result = parse_field_line(in_buf, buf_size, req);
    }

    if (last_line_result) {
        return last_line_result;
    }

    if (req->seek >= buf_size - 1) {
        return HTTP_BAD_REQUEST;
    }

    if (in_buf[++req->seek] != '\n') {
        return HTTP_BAD_REQUEST;
    }

    return 0;
}

struct http_req create_http_req() {
    struct http_req out = {
        .headers = {
            .known = {}
        },
        .target = NULL,
        .method = Unknown,
        .seek = 0
    };

    for (size_t i = 0; i < REQ_HEADER_MAX; i++) {
        out.headers.known[i] = NULL;
    }

    return out;
}

void reset_http_req(struct http_req * req) {
    for (size_t i = 0; i < REQ_HEADER_MAX; i++) {
        if (req->headers.known[i]) {
            free(req->headers.known[i]);
        }
    }

    if (req->target) {
        free(req->target);
    }

    *req = create_http_req();
}

struct http_res create_http_res() {
    struct http_res out = {
        .headers = {
            .headers = {}
        },
        .status = HTTP_INTERNAL_SERVER_ERROR,
        .content = NULL
    };

    for (size_t i = 0; i < RES_HEADER_MAX; i++) {
        out.headers.headers[i] = NULL;
    }

    return out;
}

void reset_http_res(struct http_res * res) {
    for (size_t i = 0; i < RES_HEADER_MAX; i++) {
        if (res->headers.headers[i]) {
            free(res->headers.headers[i]);
        }
    }

    *res = create_http_res();
}

static void set_http_status(struct http_res * res, http_status_code status) {
    res->status = status;

    if (res->content) {
        return;
    }

    // We'll try to send a default message in the response body
    int len = strlen(http_status_names[status]);

    res->content = http_status_names[status];
    res->content_length = len;

    size_t buf_size = sizeof(long) * 8 + 1;
    char * len_str = malloc(buf_size);
    snprintf(len_str, buf_size, "%d", len);
    res->headers.headers[RES_HEADER_CONTENT_LENGTH] = len_str;

    static const char default_content_type[] = "text/plain; charset=us-ascii";
    char * type_str = malloc(ARR_SIZE(default_content_type) + 1);
    memcpy(type_str, default_content_type, ARR_SIZE(default_content_type));
    type_str[ARR_SIZE(default_content_type)] = 0;
    res->headers.headers[RES_HEADER_CONTENT_TYPE] = type_str;
}

static int strcmp_ignore_case(const char * a, const char * b) {
    int len_a = strlen(a);
    int len_b = strlen(b);

    if (len_a < len_b) {
        return -b[len_a];
    } else if (len_a > len_b) {
        return a[len_b];
    }

    size_t i = 0;
    while (i < len_a) {
        if (lowercase(a[i]) != lowercase(b[i])) {
            return a[i] - b[i];
        }

        i++;
    }

    return 0;
}

static char * get_content_type(char * filename) {
    size_t end = strlen(filename);
    size_t start = end;

    while (start != 0 && filename[start] != '.' && filename[start] != '/') {
        start--;
    }

    const char * content_type = NULL;

    if (filename[start] == '/' || start == end) {
        content_type = "application/octet-stream";
    } else {
        start++;

        if (! strcmp_ignore_case(filename + start, "html")) {
            content_type = "text/html";
        } else if (! strcmp_ignore_case(filename + start, "js")) {
            content_type = "text/javascript";
        } else if (! strcmp_ignore_case(filename + start, "css")) {
            content_type = "text/css";
        } else if (! strcmp_ignore_case(filename + start, "txt")) {
            content_type = "text/plain";
        } else if (! strcmp_ignore_case(filename + start, "png")) {
            content_type = "image/png";
        } else if (! strcmp_ignore_case(filename + start, "jpg")) {
            content_type = "image/jpeg";
        } else if (! strcmp_ignore_case(filename + start, "jpeg")) {
            content_type = "image/jpeg";
        } else {
            content_type = "application/octet-stream";
        }
    }

    size_t len = strlen(content_type);
    char * out = malloc(len + 1);

    memcpy(out, content_type, len + 1);

    return out;
}

static http_status_code try_get_resource(struct http_res * res, struct http_req * req) {
    struct file * resource = static_files.files;
    int is_index_query = ! strcmp(req->target, "/");

    while (resource) {
        if (is_index_query && ! strcmp("/index.html", resource->path)) {
            break;
        }

        if (! strcmp(resource->path, req->target)) {
            break;
        }

        resource = resource->next;
    }

    if (! resource) {
        return HTTP_RESOURCE_NOT_FOUND;
    }

    int never_use_cache = global_options.cache_option == NeverUseCache;
    int must_use_cache = global_options.cache_option == AlwaysUseCache;
    int will_use_cache = never_use_cache ||
        (! must_use_cache &&
         req->headers.known[REQ_HEADER_CACHE_CONTROL] &&
         ! strcmp(req->headers.known[REQ_HEADER_CACHE_CONTROL], "no-cache")
        );

    if (will_use_cache) {
        reload_static_file(resource);
    }

    size_t len_str_size = sizeof(long) * 8 + 1;
    char * len_str = malloc(len_str_size);
    snprintf(len_str, len_str_size, "%ld", resource->content_length);
    res->headers.headers[RES_HEADER_CONTENT_LENGTH] = len_str;
    res->headers.headers[RES_HEADER_CONTENT_TYPE] = get_content_type(resource->path);

    if (req->method == Head) {
        return 0;
    }

    res->content = resource->content;
    res->content_length = resource->content_length;

    return 0;
}

void handle_http_req(const char * in_buf, size_t buf_size, struct http_req * req, struct http_res * res) {
    http_status_code req_line_status = parse_req_line(in_buf, buf_size, req);

    if (req_line_status) {
        set_http_status(res, req_line_status);

        return;
    }

    http_status_code field_lines_status = parse_field_lines(in_buf, buf_size, req);

    if (field_lines_status) {
        set_http_status(res, field_lines_status);

        return;
    }

    http_status_code get_resource_status = try_get_resource(res, req);

    if (get_resource_status) {
        set_http_status(res, get_resource_status);

        return;
    }

    set_http_status(res, HTTP_OK);
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

    write_sock(out_sock_fd, http_version_out, ARR_SIZE(http_version_out) - 1);
    write_sock(out_sock_fd, " ", 1);

    char status[4];
    status_to_str(res->status, status);

    write_sock(out_sock_fd, status, 3);
    write_sock(out_sock_fd, " \r\n", 3);

    for (size_t i = 0; i < RES_HEADER_MAX; i++) {
        if (res->headers.headers[i]) {
            int name_len = strlen(res_header_names[i]);
            int val_len = strlen(res->headers.headers[i]);

            write_sock(out_sock_fd, res_header_names[i], name_len);
            write_sock(out_sock_fd, ": ", 2);
            write_sock(out_sock_fd, res->headers.headers[i], val_len);
            write_sock(out_sock_fd, "\r\n", 2);
        }
    }

    write_sock(out_sock_fd, "\r\n", 2);

    if (res->content) {
        write_sock(out_sock_fd, res->content, res->content_length);
    }
}
