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
#include "status.h"

const char * http_status_names[] = {
    [HTTP_OK] = "OK",

    [HTTP_BAD_REQUEST] = "Bad Request",
    [HTTP_UNAUTHORIZED] = "Unauthorized",
    [HTTP_PAYMENT_REQUIRED] = "Payment Required",
    [HTTP_FORBIDDEN] = "Forbidden",
    [HTTP_RESOURCE_NOT_FOUND] = "Resource Not Found",
    [HTTP_METHOD_NOT_ALLOWED] = "Method Not Allowed",
    [HTTP_URI_TOO_LONG] = "URI Too Long",

    [HTTP_INTERNAL_SERVER_ERROR] = "Internal Server Error",
    [HTTP_METHOD_NOT_IMPLEMENTED] = "Method Not Implemented",
    [HTTP_VERSION_NOT_SUPPORTED] = "HTTP Version Not Supported"
};
