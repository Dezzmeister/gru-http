# gru-http

This is a basic HTTP 1.1 (RFC 9112) server. It's not a full implementation of RFC 9112;
it just supports enough for me to host simple static websites.

## Building

The server uses Linux syscalls, so it will only build on Linux. To make a release build:

```sh
make release
```

To make a debug build:

```sh
make debug
```

You can pass `CC` to use a different C compiler:

```sh
make release CC=clang
```

## Developing

I use [YouCompleteMe](https://github.com/ycm-core/YouCompleteMe) for code completion with 
[Bear](https://github.com/rizsotto/Bear/tree/master) to generate a compilation database. 
To generate the compilation database for yourself, install Bear and run

```sh
bear -- make <something>
```

## License

gru-http is licensed under the GNU Affero Public License 3 or any later version at your choice. See
[COPYING](https://github.com/Dezzmeister/gru-http/blob/master/COPYING) for details.
