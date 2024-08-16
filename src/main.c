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
// Uncomment this to print raw request data
// #define DEBUG_PRINT_RAW_REQ

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include "ip.h"
#include "net.h"

#define die() debug_die(__LINE__)

void debug_die(int line) {
    int error_code = errno;

    perror("Fatal error");
    printf("Line: %d\n", line);
    exit(error_code);
}

int main(int argc, const char * const * const argv) {
    struct protoent * ent = getprotobyname("tcp");

    if (! ent) {
        die();
    }

    struct sockaddr_in my_addr = {
        // TODO: Args
        .sin_addr = {
            .s_addr = 0
        },
        .sin_port = htons(8000),
        .sin_family = AF_INET
    };

    int sock_fd = socket(AF_INET, SOCK_STREAM, ent->p_proto);

    if (sock_fd == -1) {
        die();
    }

    int status = bind(sock_fd, (const struct sockaddr *) &my_addr, sizeof my_addr);

    if (status == -1) {
        die();
    }

    status = listen(sock_fd, 1);

    if (status == -1) {
        die();
    }

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
                if (! threads[i].active) {
                    thread_i = i;
                    goto thread_i_found;
                }
            }

            // We'll wait for a thread to die
            sleep(1);
        }
        thread_i_found:
        threads[thread_i].peer = peer_sock;
        threads[thread_i].peer_fd = peer_sock_fd;
        threads[thread_i].active = 1;

        pthread_create(&threads[thread_i].thread, NULL, start_connection, (void *) thread_i);
    }

    printf("Shutting down...\n");
    close(sock_fd);
    die();
}
