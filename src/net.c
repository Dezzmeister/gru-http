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
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "params.h"
#include "error.h"
#include "http.h"
#include "ip.h"
#include "net.h"

#ifndef SUPPRESS_REQ_LOGS
#include "status.h"
#endif

#define RECV_BUF_SIZE   8192
#define PRINT_BUF_SIZE  512

struct locked_thread_fields {
    int peer_fd;
    int started;
    int active;
};

struct connection_thread {
    pthread_t thread;
    struct http_req req;
    struct http_res res;

    pthread_mutex_t lock;
    struct locked_thread_fields locked;
};

struct connection_thread threads[MAX_CONNECTION_THREADS] = { 0 };

enum user_command {
    None = 0,
    Quit = 1
};

#ifdef DEBUG_LOCKS
#define checked_lock(lock) { \
    int result = pthread_mutex_lock((lock)); \
    switch (result) { \
        case EINVAL: { \
            printf("Error locking mutex: EINVAL, %s, %d\n", __FILE__, __LINE__); \
            break; \
        } \
        case EDEADLK: { \
            printf("Error locking mutex: EDEADLK, %s, %d\n", __FILE__, __LINE__); \
        } \
    } \
}

#define checked_unlock(lock) { \
    int result = pthread_mutex_unlock((lock)); \
    switch (result) { \
        case EINVAL: { \
            printf("Error unlocking mutex: EINVAL, %s, %d\n", __FILE__, __LINE__); \
            break; \
        } \
        case EPERM: { \
            printf("Error unlocking mutex: EDEADLK, %s, %d\n", __FILE__, __LINE__); \
        } \
    } \
}
#else
#define checked_lock pthread_mutex_lock
#define checked_unlock pthread_mutex_unlock
#endif

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
    checked_lock(&thread->lock);
    int peer_fd = thread->locked.peer_fd;
    checked_unlock(&thread->lock);
    char buf[RECV_BUF_SIZE];

    char print_buf[PRINT_BUF_SIZE];
    print_buf[PRINT_BUF_SIZE - 1] = 0;
    pid_t tid_for_printing = gettid();

#ifndef SUPPRESS_REQ_LOGS
    printf("[Thread %d] Receiving data\n", tid_for_printing);
#endif

    struct pollfd poll_arg = {
        .fd = peer_fd,
        .events = POLLIN
    };

    int status = poll(&poll_arg, 1, POLL_TIMEOUT_MS);

    if (status == -1) {
        snprintf(print_buf, PRINT_BUF_SIZE, "[Thread %d] Failed to poll socket", tid_for_printing);
        perror(print_buf);
    } else if (status == 0) {
        snprintf(print_buf, PRINT_BUF_SIZE, "[Thread %d] Timed out while polling socket", tid_for_printing);
    } else {
        // TODO: Call `poll` and `read` in a loop, and support Keep-Alive
        int bytes_read = read(peer_fd, buf, RECV_BUF_SIZE - 1);

        if (bytes_read > 0) {
            buf[bytes_read] = 0;
#ifdef DEBUG_PRINT_RAW_REQ
            write(1, buf, bytes_read);
#endif

            handle_http_req(buf, bytes_read, &thread->req, &thread->res);
#ifndef SUPPRESS_REQ_LOGS
            print_http_req(&thread->req, tid_for_printing);
#endif
            send_http_res(&thread->res, peer_fd);
#ifndef SUPPRESS_REQ_LOGS
            print_http_res(&thread->res, tid_for_printing);
#endif
        } else if (bytes_read == -1) {
            snprintf(print_buf, PRINT_BUF_SIZE, "[Thread %d] Failed to read data from socket", tid_for_printing);
            perror(print_buf);
        }
    }

#ifndef SUPPRESS_REQ_LOGS
    printf("[Thread %d] Closing socket\n", tid_for_printing);
#endif
    status = shutdown(peer_fd, SHUT_RDWR);

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

    checked_lock(&thread->lock);
    thread->locked.started = 1;
    checked_unlock(&thread->lock);

    char thread_name[16];

    snprintf(thread_name, 16, "handler %d", (uint8_t) thread_i);
    int setname_result = pthread_setname_np(thread->thread, thread_name);

    if (setname_result) {
        perror("Failed to set handler thread name");
    }

    int status = start_connection_impl(thread);

    checked_lock(&thread->lock);
    thread->locked.active = 0;
    checked_unlock(&thread->lock);

    return (void *) (uint64_t) status;
}

void join_finished_threads() {
    for (size_t i = 0; i < MAX_CONNECTION_THREADS; i++) {
        checked_lock(&threads[i].lock);
        if (threads[i].locked.started && ! threads[i].locked.active) {
            int status = pthread_join(threads[i].thread, NULL);

            if (status) {
                perror("Failed to join thread");
            }

            threads[i].locked.started = 0;
            reset_http_req(&threads[i].req);
            reset_http_res(&threads[i].res);
        }
        checked_unlock(&threads[i].lock);
    }
}

void init_shared_memory() {
    pthread_mutexattr_t mutexattr;

    pthread_mutexattr_init(&mutexattr);
#ifdef DEBUG_LOCKS
    int result = pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_ERRORCHECK_NP);
#else
    int result = pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_FAST_NP);
#endif

    if (result) {
        die();
    }

    for (size_t i = 0; i < MAX_CONNECTION_THREADS; i++) {
        pthread_mutex_init(&threads[i].lock, &mutexattr);

        threads[i].req = create_http_req();
        threads[i].res = create_http_res();
    }

    pthread_mutexattr_destroy(&mutexattr);
}

enum user_command get_user_command() {
    static char buf[256];

    int bytes_read = read(STDIN_FILENO, buf, ((sizeof buf) / sizeof(char)) - 1);
    buf[bytes_read] = 0;

    if (! strcmp(buf, "q\n")) {
        return Quit;
    }

    return None;
}

void listen_for_connections(const struct sockaddr_in * my_addr) {
    init_shared_memory();

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
    printf("Send 'q' to quit\n");

    free(ip_str);

    struct sockaddr_in peer_sock;
    socklen_t peer_len = sizeof peer_sock;

    struct pollfd poll_arg[2] = {
        {
            .fd = sock_fd,
            .events = POLLIN
        },
        {
            .fd = STDIN_FILENO,
            .events = POLLIN
        }
    };

    while (1) {
        int status = poll(poll_arg, sizeof(poll_arg) / sizeof(struct pollfd), -1);

        if (status == -1) {
            perror("Failed to poll stdin and listen socket");
            continue;
        }

        if (status == 0) {
            perror("Timed out while polling stdin and listen socket");
            continue;
        }

        if (poll_arg[1].revents) {
            if (poll_arg[1].revents & POLLIN) {
                enum user_command cmd = get_user_command();

                switch (cmd) {
                    case Quit: {
                        goto shutdown;
                    };
                    case None: {
                        break;
                    };
                }
            } else {
                printf("Poll error event on stdin: %d\n", poll_arg[1].revents);
            }
        }

        if (poll_arg[0].revents) {
            if (poll_arg[0].revents & POLLIN) {
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

                    for (size_t i = 0; i < MAX_CONNECTION_THREADS; i++) {
                        checked_lock(&threads[i].lock);
                        if (! threads[i].locked.started && ! threads[i].locked.active) {
                            thread_i = i;
                            checked_unlock(&threads[i].lock);
                            goto thread_i_found;
                        }
                        checked_unlock(&threads[i].lock);
                    }

                    // We'll wait for a thread to die
                    sleep(1);
                }
                thread_i_found:
                checked_lock(&threads[thread_i].lock);
                threads[thread_i].locked.peer_fd = peer_sock_fd;
                threads[thread_i].locked.active = 1;
                checked_unlock(&threads[thread_i].lock);

                int status = pthread_create(&threads[thread_i].thread, NULL, start_connection, (void *) thread_i);

                if (status) {
                    perror("Failed to start thread");
                }
            } else {
                printf("Poll error event on listen socket: %d\n", poll_arg[1].revents);
            }
        }
    }

shutdown:
    printf("Shutting down...\n");
    close(sock_fd);

    for (size_t i = 0; i < MAX_CONNECTION_THREADS; i++) {
        int should_join = 0;
        checked_lock(&threads[i].lock);
        should_join = threads[i].locked.started || threads[i].locked.active;
        checked_unlock(&threads[i].lock);

        if (should_join) {
            int status = pthread_join(threads[i].thread, NULL);

            if (status) {
                perror("Failed to join thread");
            }

            reset_http_req(&threads[i].req);
            reset_http_res(&threads[i].res);
        }
    }
}
