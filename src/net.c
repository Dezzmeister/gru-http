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


// Uncomment this to suppress stdout for each request
// #define SUPPRESS_REQ_LOGS

#define _GNU_SOURCE
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include "error.h"
#include "http.h"
#include "ip.h"
#include "net.h"

#ifndef SUPPRESS_REQ_LOGS
#include "status.h"
#endif

#define MAX_THREADS     32
#define RECV_BUF_SIZE   8192
#define PRINT_BUF_SIZE  512

struct connection_thread {
    pthread_t thread;
    _Atomic int peer_fd;
    _Atomic int started;
    _Atomic int active;
};

struct connection_thread threads[MAX_THREADS] = { 0 };

#ifndef SUPPRESS_REQ_LOGS
static void print_http_req(struct http_req * req, pid_t tid) {
    if (req->target) {
        printf("[Thread %d] -> %s %s\n", tid, http_method_names[req->method], req->target);

        for (size_t i = 0; i < REQ_HEADER_MAX; i++) {
            if (req->headers.known[i]) {
                printf("\t\t %s: %s\n", req_header_names[i], req->headers.known[i]);
            }
        }
    } else {
        printf("[Thread %d] -> %s (Undefined target)\n", tid, http_method_names[req->method]);
    }
}

static void print_http_res(struct http_res * res, pid_t tid) {
    printf("[Thread %d] <- %d %s\n", tid, res->status, http_status_names[res->status]);

    for (size_t i = 0; i < RES_HEADER_MAX; i++) {
        if (res->headers.headers[i]) {
            printf("\t\t %s: %s\n", res_header_names[i], res->headers.headers[i]);
        }
    }
}
#endif

static int start_connection_impl(struct connection_thread * thread) {
    int peer_fd = thread->peer_fd;
    char buf[RECV_BUF_SIZE];
    int bytes_read;

    char print_buf[PRINT_BUF_SIZE];
    print_buf[PRINT_BUF_SIZE - 1] = 0;
    pid_t tid_for_printing = gettid();

#ifndef SUPPRESS_REQ_LOGS
    printf("[Thread %d] Receiving data\n", tid_for_printing);
#endif

    // TODO: Non-blocking IO so we can disconnect clients that are too slow
    while ((bytes_read = read(peer_fd, buf, RECV_BUF_SIZE - 1)) > 0) {
        buf[bytes_read] = 0;
#ifdef DEBUG_PRINT_RAW_REQ
        write(1, buf, bytes_read);
#endif

        struct http_req req = create_http_req();
        struct http_res res = handle_http_req(buf, bytes_read, &req);
#ifndef SUPPRESS_REQ_LOGS
        print_http_req(&req, tid_for_printing);
#endif
        send_http_res(&res, peer_fd);
#ifndef SUPPRESS_REQ_LOGS
        print_http_res(&res, tid_for_printing);
#endif
        free_http_res(&res);
        free_http_req(&req);
        // Keep-Alive is not supported yet
        break;
    }

    if (bytes_read == -1) {
        snprintf(print_buf, PRINT_BUF_SIZE, "[Thread %d] Failed to read data from socket", tid_for_printing);
        perror(print_buf);
    }

#ifndef SUPPRESS_REQ_LOGS
    printf("[Thread %d] Closing socket\n", tid_for_printing);
#endif
    // TODO: Fix sockets ending up in TIME_WAIT
    int status = shutdown(peer_fd, SHUT_RDWR);

    if (status == -1) {
        perror("Failed to call 'shutdown' on socket");
    }

    status = close(peer_fd);

    if (status == -1) {
        snprintf(print_buf, PRINT_BUF_SIZE, "[Thread %d] Failed to close socket", tid_for_printing);
        perror(print_buf);

        return errno;
    }

    return 0;
}

static void * start_connection(void * thread_index) {
    const size_t thread_i = (size_t) thread_index;
    struct connection_thread * thread = threads + thread_i;

    thread->started = 1;

    int status = start_connection_impl(thread);

    thread->active = 0;

    return (void *) (uint64_t) status;
}

void cancel_all_threads() {
    for (size_t i = 0; i < MAX_THREADS; i++) {
        if (threads[i].active) {
            int status = pthread_cancel(threads[i].thread);

            if (status == -1) {
                perror("Failed to cancel thread");
            }
        }
    }
}

void join_finished_threads() {
    for (size_t i = 0; i < MAX_THREADS; i++) {
        if (threads[i].started) {
            int status = pthread_join(threads[i].thread, NULL);

            if (status == -1) {
                perror("Failed to join thread");
            }

            threads[i].started = 0;
        }
    }
}

void listen_for_connections(const struct sockaddr_in * my_addr) {
    struct protoent * ent = getprotobyname("tcp");

    if (! ent) {
        die();
    }

    int sock_fd = socket(AF_INET, SOCK_STREAM, ent->p_proto);

    if (sock_fd == -1) {
        die();
    }

    endprotoent();

    const int reuse = 1;
    int result = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);

    if (result) {
        perror("Failed to set SO_REUSEADDR on listen socket");
    }

    int status = bind(sock_fd, (const struct sockaddr *) my_addr, sizeof (struct sockaddr_in));

    if (status == -1) {
        die();
    }

    status = listen(sock_fd, 32);

    if (status == -1) {
        die();
    }

    char * ip_str = fmt_ipv4_addr(my_addr->sin_addr);

    printf("Listening on %s:%d\n", ip_str, ntohs(my_addr->sin_port));

    free(ip_str);

    struct sockaddr_in peer_sock;
    socklen_t peer_len = sizeof peer_sock;

    while (1) {
        int peer_sock_fd = accept(sock_fd, (struct sockaddr *) &peer_sock, &peer_len);

        if (peer_sock_fd == -1) {
            perror("Failed to accept connection");
            continue;
        }
#ifndef SUPPRESS_REQ_LOGS
        char * const ip_str = fmt_ipv4_addr(peer_sock.sin_addr);

        if (ip_str) {
            printf("Accepted a connection from %s:%d\n", ip_str, peer_sock.sin_port);
            free(ip_str);
        } else {
            printf("IP string was null\n");
        }
#endif

        size_t thread_i;

        while (1) {
            join_finished_threads();

            for (size_t i = 0; i < MAX_THREADS; i++) {
                if (! threads[i].active) {
                    thread_i = i;
                    goto thread_i_found;
                }
            }

            // We'll wait for a thread to die
            sleep(1);
        }
        thread_i_found:
        threads[thread_i].peer_fd = peer_sock_fd;
        threads[thread_i].active = 1;

        int status = pthread_create(&threads[thread_i].thread, NULL, start_connection, (void *) thread_i);

        if (status == -1) {
            perror("Failed to start thread:");
        }
    }

    printf("Shutting down...\n");
    close(sock_fd);
}
