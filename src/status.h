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
#ifndef SRC_STATUS_H
#define SRC_STATUS_H
#include <stdint.h>

#define HTTP_OK                             200

#define HTTP_BAD_REQUEST                    400
#define HTTP_UNAUTHORIZED                   401
#define HTTP_PAYMENT_REQUIRED               402
#define HTTP_FORBIDDEN                      403
#define HTTP_RESOURCE_NOT_FOUND             404
#define HTTP_METHOD_NOT_ALLOWED             405
#define HTTP_URI_TOO_LONG                   414

#define HTTP_INTERNAL_SERVER_ERROR          500
#define HTTP_METHOD_NOT_IMPLEMENTED         501
#define HTTP_VERSION_NOT_SUPPORTED          505

typedef uint16_t http_status_code;

extern const char * http_status_names[];

#endif
