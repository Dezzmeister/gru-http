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
#include "ip.h"

char * fmt_ipv4_addr(struct in_addr addr) {
    union {
        uint8_t parts[4];
        uint32_t addr;
    } ip;

    // This is OK because addr.s_addr is in network byte order, so
    // `ip.parts[0]` will always be the most significant byte
    ip.addr = addr.s_addr;

    // 3 periods + 1 null terminator
    size_t total_chars = 3 + 1;

    for (size_t i = 0; i < 4; i++) {
        // Technically UB
        if (ip.parts[i] < 10) {
            total_chars++;
        } else if (ip.parts[i] < 100) {
            total_chars += 2;
        } else {
            total_chars += 3;
        }
    }

    char * out_buf = malloc(total_chars);
    int num_chars_written = snprintf(
            out_buf,
            total_chars,
            "%d.%d.%d.%d",
            ip.parts[0],
            ip.parts[1],
            ip.parts[2],
            ip.parts[3]
    );

    if (num_chars_written > total_chars) {
        free(out_buf);

        return NULL;
    }

    return out_buf;
}
