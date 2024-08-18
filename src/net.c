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
#include <netdb.h>
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>
#include "error.h"
#include "http.h"
#include "ip.h"
#include "net.h"
#include "status.h"

#define RECV_BUF_SIZE   8192
#define PRINT_BUF_SIZE  512

struct connection_thread threads[MAX_THREADS] = {};

static void print_http_req(struct http_req * req, pid_t tid) {
    if (req->target) {
        printf("[Thread %d] -> %s %s\n", tid, http_method_names[req->method], req->target);

        for (int i = 0; i < REQ_HEADER_MAX; i++) {
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
}

static int start_connection_impl(struct connection_thread * thread) {
    int peer_fd = atomic_load_explicit(&thread->peer_fd, memory_order_acquire);
    char buf[RECV_BUF_SIZE];
    int bytes_read;

    char print_buf[PRINT_BUF_SIZE];
    print_buf[PRINT_BUF_SIZE - 1] = 0;
    pid_t tid_for_printing = gettid();

    printf("[Thread %d] Receiving data\n", tid_for_printing);

    struct http_req req = create_http_req();
    // TODO: Non-blocking IO so we can disconnect clients that are too slow
    while ((bytes_read = read(peer_fd, buf, RECV_BUF_SIZE - 1))) {
        if (bytes_read == -1) {
            snprintf(print_buf, PRINT_BUF_SIZE, "[Thread %d] Failed to read data from socket", tid_for_printing);
            perror(print_buf);
            break;
        } else {
            buf[bytes_read] = 0;
#ifdef DEBUG_PRINT_RAW_REQ
            write(1, buf, bytes_read);
#endif

            struct http_res res = handle_http_req(buf, bytes_read, &req);
            print_http_req(&req, tid_for_printing);
            send_http_res(&res, peer_fd);
            print_http_res(&res, tid_for_printing);
            free_http_res(&res);
            // Keep-Alive is not supported
            break;
        }
    }

    free_http_req(&req);

    printf("[Thread %d] Closing socket\n", tid_for_printing);
    shutdown(peer_fd, SHUT_WR);
    int status = close(peer_fd);

    if (status == -1) {
        snprintf(print_buf, PRINT_BUF_SIZE, "[Thread %d] Failed to close socket", tid_for_printing);
        perror(print_buf);

        return errno;
    }

    return 0;
}

static void connection_handler_cleanup(void * arg) {
    struct connection_thread * thread = arg;

    atomic_store_explicit(&thread->active, 0, memory_order_release);
}

static void * start_connection(void * thread_index) {
    const size_t thread_i = (size_t) thread_index;
    struct connection_thread * thread = threads + thread_i;
    int status;

    pthread_cleanup_push(connection_handler_cleanup, thread);
    status = start_connection_impl(thread);
    pthread_cleanup_pop(1);

    return (void *) (uint64_t) status;
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

    int status = bind(sock_fd, (const struct sockaddr *) my_addr, sizeof (struct sockaddr_in));

    if (status == -1) {
        die();
    }

    status = listen(sock_fd, 1);

    if (status == -1) {
        die();
    }

    char * ip_str = fmt_ipv4_addr(my_addr->sin_addr);

    printf("Listening on %s:%d\n", ip_str, ntohs(my_addr->sin_port));

    free(ip_str);

    struct sockaddr_in peer_sock;
    int peer_sock_fd;
    socklen_t peer_len = sizeof peer_sock;

    while ((peer_sock_fd = accept(sock_fd, (struct sockaddr *) &peer_sock, &peer_len)) != -1) {
        char * const ip_str = fmt_ipv4_addr(peer_sock.sin_addr);

        if (ip_str) {
            printf("Accepted a connection from %s:%d\n", ip_str, peer_sock.sin_port);
            free(ip_str);
        } else {
            printf("IP string was null\n");
        }

        size_t thread_i;

        while (1) {
            for (size_t i = 0; i < MAX_THREADS; i++) {
                if (! atomic_load_explicit(&threads[i].active, memory_order_acquire)) {
                    thread_i = i;
                    goto thread_i_found;
                }
            }

            // We'll wait for a thread to die
            sleep(1);
        }
        thread_i_found:
        atomic_store_explicit(&threads[thread_i].peer_fd, peer_sock_fd, memory_order_release);
        atomic_store_explicit(&threads[thread_i].active, 1, memory_order_release);

        pthread_create(&threads[thread_i].thread, NULL, start_connection, (void *) thread_i);
    }

    printf("Shutting down...\n");
    close(sock_fd);
}
