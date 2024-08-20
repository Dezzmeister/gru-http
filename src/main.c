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
#include <argp.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "error.h"
#include "files.h"
#include "http.h"
#include "net.h"

const char * argp_program_version = "gru-http 1.0";
const char * argp_program_bug_address = "dezzmeister16@gmail.com";
static const char doc[] = 
"gru-http is a simple HTTP 1.1 server capable of serving static files. "
"The source code is available at https://github.com/Dezzmeister/gru-http."
"\v"
"The first argument is an IPv4 address, formatted as 4 decimal octets separated "
"by periods. The second argument is a TCP port (in the range [1, 65535]). The "
"server will bind to the given IP address and listen on the given port. "
"The last argument is a directory containing an index.html file (and other "
"necessary files). The server will treat this as the root directory, "
"so that a resource named in a GET request will correspond to a file in this "
"directory. The server will respond with index.html to a GET request for the root "
"directory.";
static struct argp_option argp_options[] = {
    {
        .name = "cache",
        .key = 'c',
        .arg = "never|always",
        .flags = 0,
        .doc = "Overrides default caching behavior for responses. Pass \"never\" "
            "to force the server to load the requested resource from the filesystem "
            "on every request. Pass \"always\" to force the server to ignore "
            "\"Cache-Control: no-cache\" and return cached resources on every "
            "request. The default behavior is for the server to return a cached "
            "resource, unless the \"Cache-Control\" header is present and specifies "
            "\"no-cache\".",
        .group = 0

    },
    { 0 }
};

static char * ip_str;
static char * port_str;
static char * static_dir;

static error_t arg_parser(int key, char * arg, struct argp_state * state) {
    static int arg_index = 0;

    switch (key) {
        case ARGP_KEY_ARG: {
            switch (arg_index) {
                case 0: {
                    ip_str = arg;
                    break;
                }
                case 1: {
                    port_str = arg;
                    break;
                }
                case 2: {
                    static_dir = arg;
                    break;
                }
                default: {
                    argp_usage(state);
                    break;
                }
            }
            arg_index++;
            return 0;
        }
        case ARGP_KEY_END: {
            if (arg_index < 3) {
                argp_usage(state);
            }
            break;
        }
        case 'c': {
            if (! strcmp(arg, "never")) {
                global_options.cache_option = NeverUseCache;
            } else if (! strcmp(arg, "always")) {
                global_options.cache_option = AlwaysUseCache;
            } else {
                printf("Invalid --cache option\n");
                argp_usage(state);
            }

            break;
        }
        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

int main(int argc, char ** const argv) {
    struct argp parser = {
        .options = argp_options,
        .parser = arg_parser,
        .args_doc = "IPV4 PORT DIR",
        .doc = doc,
        .children = NULL,
        .help_filter = NULL,
        .argp_domain = NULL
    };

    error_t argp_result = argp_parse(&parser, argc, argv, 0, NULL, NULL);

    if (argp_result) {
        die();
    }

    switch (global_options.cache_option) {
        case NeverUseCache: {
            printf("Caching is disabled\n");
            break;
        }
        case AlwaysUseCache: {
            printf("Caching is forced\n");
            break;
        }
        case DefaultUseCache: {}
    };

    int port = atoi(port_str);

    if (! port || port > 65535) {
        printf("Port is not valid\n");
        exit(1);
    }

    struct sockaddr_in my_addr = {
        .sin_addr = {
            .s_addr = 0
        },
        .sin_port = htons(port),
        .sin_family = AF_INET
    };

    int ip_str_result = inet_aton(ip_str, &my_addr.sin_addr);

    if (! ip_str_result) {
        printf("IP address is not valid\n");
        exit(1);
    }

    printf("Loading static files from %s\n", static_dir);
    load_static_dir(static_dir);

    listen_for_connections(&my_addr);

    free_static_dir();

    return 0;
}
