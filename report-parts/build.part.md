# build

The tool has been tested to compile with the gcc mingw64 compiler bundled with the [MSYS2](https://www.msys2.org/) distribution.

To build, download and install MSYS2. open the [MINGW64 terminal](https://www.msys2.org/docs/terminals/) and run _make_
```
$ make
gcc -Wall -Wextra -Werror stderr-test.c -o stest.exe -lws2_32
```

then open a command prompt and type stest.exe to display the usage message
```
>stest
usage: stest COMMANDS

A utility to probe stderr's behavior on windows.

commands:
  :to-stderr :write|:write-nl COUNT [:unbuf|(:lnbuf|:flbuf BUFFER-SIZE)]
        write COUNT '$' characters to stderr (:write-nl will also write an \n at the end). Optionally change stderr's mode to unbuffered (:unbuf),  line (:lnbuf) or fully (:flbuf) buffered using a new buffer of BUFFER-SIZE.

  :to-child-stderr :write|:write-nl COUNT [:unbuf|(:lnbuf|:flbuf BUFFER-SIZE)]
        Create a child process and have it write to its stderr stream. Takes same options as :to-stderr.

  :pipe :pipe-size SIZE :read RCOUNT :write WCOUNT
        Create a _pipe() of size SIZE, write WCOUNT '$' characters to the pipe's write endpoint and read RCOUNT characters from the pipe's read endpoint.

  :pipe-handle-to-child :pipe-size SIZE :read RCOUNT :write WCOUNT
        Create a _pipe() of size SIZE. Also create a child process passing the write pipe's handle as a command line argument to it. The child will open the handle and write WCOUNT '$' characters to it. The parent process will attempt to read RCOUNT characters from the pipe's read's endpoint.

  :to-handle HANDLE WRITE-COUNT
        helper option to support :pipe-handle-to-child. Attempts to open HANDLE and write WRITE-COUNT '$' characters to it.

  :pipe-to-child-stderr :pipe-size PSIZE :read RCOUNT :write|:write-nl WCOUNT [:unbuf|(:lnbuf|:flbuf BSIZE)]
        Create a _pipe() of size PSIZE. Then create a child process with its stderr redirected to the pipe's write endpoint. The parent process will attempt to read RCOUNT characters from the pipe's read endpoint. The child process will attempt to write to its stderr (see :to-stderr for information on the write and buffering mode options).

  :sock-to-child-stderr :read RCOUNT :write|:write-nl WCOUNT [:unbuf|(:lnbuf|:flbuf BSIZE)]
        Create a pair of read and write sockets. Then create a child process with its stderr redirected to the write socket. The parent process will attempt to read RCOUNT characters from the read socket. The child process will attempt to write to its stderr (see :to-stderr for information on the write and buffering mode options).

```
