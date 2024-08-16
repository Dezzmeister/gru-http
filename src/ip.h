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
#ifndef SRC_IP_H
#define SRC_IP_H

#include <arpa/inet.h>

// Formats an IPv4 address in the familiar readable form (4 dec-octets delimited
// by periods). Returns the string if successful, NULL otherwise. It's the caller's
// responsibility to free the string (with `free()`).
// `addr.s_addr` must be in network byte order.
char * fmt_ipv4_addr(struct in_addr addr);

#endif
