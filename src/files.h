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
#include <sys/types.h>
#include <dirent.h>

struct file {
    char path[256];
    char * content;
    size_t content_length;
    struct file * next;
};

struct http_static_dir {
    char * root;
    struct file * files;
};

extern struct http_static_dir static_files;

void load_static_dir(const char * dir);
void free_static_dir();
int reload_static_file(struct file * file_entry);
