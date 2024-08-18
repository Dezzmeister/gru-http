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
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "error.h"
#include "files.h"

struct http_static_dir static_files;

struct dir_stack {
    char path[PATH_MAX];
    size_t path_len;
    struct dir_stack * next;
};

static size_t path_join(char * restrict first, char * restrict second, size_t first_len) {
    size_t second_len = strlen(second);
    size_t memcpy_dst = 0;
    size_t memcpy_src = 0;
    size_t out_len = 0;

    if (first[first_len - 1] == '/') {
        if (second[0] == '/') {
            out_len = first_len + second_len - 1;
            memcpy_dst = first_len;
            memcpy_src = 1;
        } else {
            out_len = first_len + second_len;
            memcpy_dst = first_len;
            memcpy_src = 0;
        }
    } else {
        if (second[0] == '/') {
            out_len = first_len + second_len;
            memcpy_dst = first_len;
            memcpy_src = 0;
        } else {
            out_len = first_len + second_len + 1;
            memcpy_dst = first_len + 1;
            memcpy_src = 0;

            if (out_len < PATH_MAX) {
                first[first_len] = '/';
            }
        }
    }

    if (out_len >= PATH_MAX) {
        printf("Result of joining %s and %s would be %ld, max length is %d\n", first, second, out_len, PATH_MAX);
        exit(1);
    }

    memcpy(first + memcpy_dst, second + memcpy_src, second_len - memcpy_src);
    first[out_len] = 0;

    return out_len;
}

struct file * read_full_file(const char * path, size_t root_offset) {
    int fd = open(path, O_RDONLY);

    if (fd == -1) {
        die();
    }

    struct stat statbuf;

    int status = fstat(fd, &statbuf);

    if (status == -1) {
        close(fd);
        die();
    }

    char * bytes = malloc(statbuf.st_size);
    size_t pos = 0;

    while ((status = read(fd, bytes + pos, statbuf.st_size - pos))) {
        if (status == -1) {
            printf("Error reading from %s\n", path);
            die();
        }
    }

    status = close(fd);

    if (status == -1) {
        printf("Error closing %s\n", path);
        die();
    }

    struct file * out = malloc(sizeof(struct file));

    out->content = bytes;
    out->next = NULL;
    memcpy(out->path, path + root_offset, strlen(path) + 1 - root_offset);

    return out;
}

static struct file * read_full_dir(char * root_dir_name) {
    struct dir_stack * top_dir = malloc(sizeof(struct dir_stack));
    struct dir_stack * last_dir = top_dir;

    last_dir->next = NULL;
    last_dir->path_len = strlen(root_dir_name);

    if (last_dir->path_len > PATH_MAX) {
        printf("Path must not be longer than %d characters: %s\n", PATH_MAX, root_dir_name);
        exit(1);
    }

    memcpy(last_dir->path, root_dir_name, last_dir->path_len);

    if (last_dir->path[last_dir->path_len] == '/') {
        last_dir->path_len--;
    }

    last_dir->path[last_dir->path_len] = 0;

    struct file * files = NULL;
    struct file * top_file = NULL;

    char tmp_buf[256];

    while (last_dir) {
        DIR * dir = opendir(last_dir->path);

        if (! dir) {
            printf("Failed to open %s\n", last_dir->path);
            die();
        }

        int dir_fd = dirfd(dir);

        if (dir_fd == -1) {
            printf("Failed to get file descriptor for dir %s\n", last_dir->path);
            closedir(dir);
            free(last_dir);
            // TODO: Free everything else when dying
            die();
        }

        struct dirent * ent;

        while ((ent = readdir(dir)) && strcmp(ent->d_name, ".") && strcmp(ent->d_name, "..")) {
            struct stat statbuf;
            int status = fstatat(dir_fd, ent->d_name, &statbuf, 0);

            if (status == -1) {
                printf("Failed to stat %s in %s\n", ent->d_name, last_dir->path);
                closedir(dir);
                free(last_dir);
                die();
            }

            if (S_ISDIR(statbuf.st_mode)) {
                top_dir->next = malloc(sizeof(struct dir_stack));
                top_dir = top_dir->next;
                top_dir->next = NULL;
                memcpy(top_dir->path, last_dir->path, last_dir->path_len + 1);
                top_dir->path_len = path_join(top_dir->path, ent->d_name, last_dir->path_len);
            } else if (S_ISREG(statbuf.st_mode)) {
                memcpy(tmp_buf, last_dir->path, last_dir->path_len + 1);
                path_join(tmp_buf, ent->d_name, last_dir->path_len);

                struct file * next_file = read_full_file(tmp_buf, last_dir->path_len);

                if (! files) {
                    files = next_file;
                    top_file = files;
                } else {
                    top_file->next = next_file;
                    top_file = top_file->next;
                }
            }
 
            errno = 0;
        }

        if (errno) {
            closedir(dir);
            free(last_dir);
            die();
        }

        int close_result = closedir(dir);

        struct dir_stack * next_dir = last_dir->next;
        free(last_dir);
        last_dir = next_dir;

        if (close_result == -1) {
            printf("Failed to close %s\n", top_dir->path);
            die();
        }
    }

    return files;
}

void load_static_dir(const char * dir) {
    size_t dir_len = strlen(dir);

    struct http_static_dir out = {
        .root = malloc(dir_len + 1),
        .files = NULL
    };

    memcpy(out.root, dir, dir_len + 1);

    out.files = read_full_dir(out.root);

    static_files = out;
}
