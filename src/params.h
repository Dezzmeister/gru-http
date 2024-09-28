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
#ifndef SRC_PARAMS_H
#define SRC_PARAMS_H

// This file contains some #defines that can be used to enable or disable
// program features, as well as global parameters that a user may want to
// set.


// This controls how many connection threads the program is allowed to have at
// any one time. This count does not include other threads, such as the main
// thread.
#define MAX_CONNECTION_THREADS      32

// The number of milliseconds to wait for a client to send data before disconnecting
// them.
#define POLL_TIMEOUT_MS             10000

// Uncomment this to suppress printing details of every request and response to
// stdout. Doing this can greatly increase the program's ability to handle many
// requests per second.
// #define SUPPRESS_REQ_LOGS

// Uncomment this to print raw request data to stdout
// #define DEBUG_PRINT_RAW_REQ

// Uncomment this to use error-checking locks instead of fast locks
#define DEBUG_LOCKS

#endif
