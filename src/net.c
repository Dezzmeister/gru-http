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
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "http.h"
#include "net.h"
#include "status.h"

#define RECV_BUF_SIZE   8192
#define PRINT_BUF_SIZE  512

struct connection_thread threads[MAX_THREADS] = {};

static void print_http_req(struct http_req * req, pid_t tid) {
    if (req->target) {
        printf("[Thread %d] -> %s %s\n", tid, http_method_names[req->method], req->target);
    } else {
        printf("[Thread %d] -> %s (Undefined target)\n", tid, http_method_names[req->method]);
    }
}

static void print_http_res(struct http_res * res, pid_t tid) {
    printf("[Thread %d] <- %d %s\n", tid, res->status, http_status_names[res->status]);
}

void * start_connection(void * thread_index) {
    const size_t thread_i = (size_t) thread_index;
    struct connection_thread * thread = threads + thread_i;

    char buf[RECV_BUF_SIZE];
    int bytes_read;

    char print_buf[PRINT_BUF_SIZE];
    print_buf[PRINT_BUF_SIZE - 1] = 0;
    pid_t tid_for_printing = gettid();

    printf("[Thread %d] Receiving data\n", tid_for_printing);

    struct http_req req = create_http_req();
    char out_tmp[RECV_BUF_SIZE];
    // TODO: Non-blocking IO so we can disconnect clients that are too slow
    while ((bytes_read = recv(thread->peer_fd, buf, RECV_BUF_SIZE - 1, 0))) {
        if (bytes_read == -1) {
            snprintf(print_buf, PRINT_BUF_SIZE, "[Thread %d] Failed to read data from socket", tid_for_printing);
            perror(print_buf);
            break;
        } else {
            buf[bytes_read] = 0;
#ifdef DEBUG_PRINT_RAW_REQ
            write(1, buf, bytes_read);
#endif

            struct http_res res = handle_http_req(buf, out_tmp, bytes_read, &req);
            print_http_req(&req, tid_for_printing);
            send_http_res(&res, thread->peer_fd);
            print_http_res(&res, tid_for_printing);
            // TODO: Keep-Alive
            break;
        }
    }

    free_http_req(&req);

    printf("[Thread %d] Closing socket\n", tid_for_printing);
    int status = close(thread->peer_fd);

    if (status == -1) {
        snprintf(print_buf, PRINT_BUF_SIZE, "[Thread %d] Failed to close socket", tid_for_printing);
        perror(print_buf);

        thread->active = 0;
        return (void *) (uint64_t) errno;
    }

    thread->active = 0;
    return 0;
}
