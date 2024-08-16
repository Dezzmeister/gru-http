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
#ifndef SRC_NET_H
#define SRC_NET_H

#include <arpa/inet.h>
#include <pthread.h>

#define MAX_THREADS     8

struct connection_thread {
    pthread_t thread;
    struct sockaddr_in peer;
    int peer_fd;
    int active;
};

extern struct connection_thread threads[MAX_THREADS];

void * start_connection(void * thread_index);

#endif
