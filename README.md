A test program to investigate the buffering behavior of stderr on MS-Windows.

The tool can
1. write chars to stderr and optionally change the stream's buffer mode.
2. Spawn a child. The child can write to its _stderr_.
3. Create a pipe and read and write to its read and write endpoints respectively.
4. Spawn a child and pass the pipe's write handle as a command line argument. The child can write to the handle and the parent can read from the pipe's read endpoint.
5. Spawn a child and redirect its stderr to the pipe's write handle. The parent can read from the pipe's read endpoint, while the child can write to its stderr and optionally change its buffering mode.
6. Create a read/write socket pair, and spawn a child with its stderr to the pair's write socket. The parent can read from the read socket, while the child can write to its stderr and optionally change its buffering mode.
   
# analysis 

**Disclaimer:** the results and analysis have not been independently verified and are open to review, let me know if you spot any inconsistencies or errors.

The test results suggest that stderr is unbuffered when attached to the console, though is fully buffered when it is redirected to a pipe by the parent process.

It is a common practice for programs to use pipes to interact with the input/output of a child process.

`(PARENT PROCESS) READER <= [PIPE] <= [STDERR] <= WRITER (CHILD PROCESS)`

A pipe has a buffer whose default size is 4096 bytes log. The writer will block if there is not enough space to write in the buffer.

When a child's stderr is redirected to a pipe's write endpoint, the stderr stream is fully buffered with a buffer size of 4096 bytes long. This is independent of the pipe buffer. The stream buffer contents will only be written to the pipe buffer when the stream buffer is full.

`(PARENT PROCESS) READER <= [PIPE BUFFER] <= [STDERR STREAM BUFFER] <= WRITER (CHILD PROCESS)`

In the common use case where the default stream mode (fully buffered) and buffer size (4096) are used, it can lead to the parent process experiencing long delays in receiving output data from the child when the latter does not flush its stderr buffer. 

This could be very problematic when the parent and child process are different applications and the parent has no control over the child. 

Consider a scenario where the parent process is a terminal emulator running bash. Output from the child process will not reach the parent terminal (and thus the user) until either the child exits or the stream buffer is full. In both cases the user's experience will be very poor.

Consider an even worse scenario where the parent process invokes a child process and expects to read from standard error the address/port the child is listening to, for the purpose of connecting to it and requesting some service. The child process writes an informational message to stderr with the address/port it is bound to, but the message being far less that 4096 chars long does not reach the parent process (not until it exists at least, which defeats the purpose). The parent process will not be able to connect to the child process.

Possible general solutions to the above issue could be (list not exhaustive)
1. If you are the developer of the parent program, you might want to consider switching from using pipes to the [Windows Pseudo Console (ConPTY)](https://devblogs.microsoft.com/commandline/windows-command-line-introducing-the-windows-pseudo-console-conpty/)?
2. If you are the developer of the child program, you might want to consider checking stderr's file type with [GetFileType](https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfiletype) and if it is of __FILE_TYPE_PIPE__
   1. [fflush](https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/fflush) stderr generously (perhaps after every newline character -- this will make the stream behave as if it was line buffered), or
   2. reset the stderr buffer mode to unbuffered with [setvbuf](https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/setvbuf) (unfortunately, switching to line buffered does not have the desired effect but it is the same as fully buffered).
   3. (Note: sockets are also reported as _FILE_TYPE_PIPE_, to distinguish them from real pipes you can use the [GetNamedPipeInfo](https://docs.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-getnamedpipeinfo) function)

# example output

Below is an example output when the program is instructed to invoke a child process which will write to its stderr (_:to-child-stderr_) 2 '$' characters followed by a new line (_:write-nl 2_)
```
>stest :to-child-stderr :write-nl 2
[CMD*:PARNT] :to-child-stderr :write-nl 2
[CMD*:CHILD] :to-stderr :write-nl 2
[RPT*:CHILD] :writing-bytes 3
$$
[RPT*:CHILD] :wrote-bytes 3
[RPT*:CHILD] :exiting...
[RPT*:PARNT] :child-exited
```

The output starting with _[XXXY:ZZZZZ]_ is commentary written (to stdout) by the parent and child processes.

The PRNT process was started with _CMD_ line arguments _:to-child-stderr :write-nl 2_, its stderr was attached to the console (_'*'_).

The CHILD process was started with command line arguments _:to-stderr :write-nl 2_, its standard error was attached to the console (_'*'_), as inherited from its parent process (the default).

_'$$\n'_ was printed to _stderr_ as instructed.

The CHILD process _RPT_'ed that it wrote _3_ bytes to _stderr_.

The stderr FILE-TYPE is reported with the following symbols
* '*' => _stderr_ is attached to the console.
* '|' => _stderr_ has been redirected to a pipe.
* '&' => _stderr_ has been redirected to a socket.
* '+' => _stderr_ has been redirected to a file.

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

# tests

All tests were conducted on Windows 10. 

## console

### stderr is unbuffered

**Note**: The tool reports with the character _*_ in its commentary that stderr is attached to the console.

When invoked from command prompt (i.e. the console), stderr is unbuffered:
```
>stest :to-stderr :write 1
[CMD*:PARNT] :to-stderr :write 1
[RPT*:PARNT] :writing-bytes 1
$[RPT*:PARNT] :wrote-bytes 1
[RPT*:PARNT] :exiting...
```

the same is true for a child process inheriting stderr from its parent (the default):
```
>stest :to-child-stderr :write 1
[CMD*:PARNT] :to-child-stderr :write 1
[CMD*:CHILD] :to-stderr :write 1
[RPT*:CHILD] :writing-bytes 1
$[RPT*:CHILD] :wrote-bytes 1
[RPT*:CHILD] :exiting...
[RPT*:PARNT] :child-exited
```

## pipe

The tool uses the [_pipe](https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/pipe) function to create pipes.

A pipe has a buffer whose size can be configured:
```
>stest :pipe :pipe-size 2 :read 2 :write 1
[CMD*:PARNT] :pipe :pipe-size 2 :read 2 :write 1
[RPT*:PARNT] :pipe-size 2 :writing-bytes 1
[RPT*:PARNT] :reading-bytes 2
[RPT*:PARNT] :read-bytes 1 :read-chars $
```

A writer will block if it tries to write more bytes that the space available on the pipe buffer:
```
>stest :pipe :pipe-size 2 :read 2 :write 3
[CMD*:PARNT] :pipe :pipe-size 2 :read 2 :write 3
[RPT*:PARNT] :pipe-size 2 :writing-bytes 3
[RPT*:PARNT] :killing-after-inactivity-secs 2
```

The writer will unblock once all bytes are written to the pipe:
```
>stest :pipe-handle-to-child :pipe-size 1 :read 1 :write 2
[CMD*:PARNT] :pipe-handle-to-child :pipe-size 1 :read 1 :write 2
[RPT*:PARNT] :pipe-size 1 :reading-bytes 1
[CMD*:CHILD] :to-handle 164 2
[RPT*:CHILD] :writing-bytes 2
[RPT*:CHILD] :wrote-bytes 2
[RPT*:PARNT] :read-bytes 1 :read-chars $
[RPT*:CHILD] :exiting...
[RPT*:PARNT] :child-exited
```

In the above, parent reads from pipe of size 1, child attempts to write 2 chars to pipe. Child is blocked waiting for both chars to be written to the pipe. The first char makes it to the pipe buffer, parent consumes it. There is again space for one more char in the pipe's buffer, thus second bytes is written and child is unblocked.

Specifying a pipe-size of 0, creates a pipe with a size of 4096 bytes long (the default size):
```
>stest :pipe :pipe-size 0 :read 1 :write 4096
[CMD*:PARNT] :pipe :pipe-size 0 :read 1 :write 4096
[RPT*:PARNT] :pipe-size 0 :writing-bytes 4096
[RPT*:PARNT] :reading-bytes 1
[RPT*:PARNT] :read-bytes 1 :read-chars $
```

```
>stest :pipe :pipe-size 0 :read 1 :write 4097
[CMD*:PARNT] :pipe :pipe-size 0 :read 1 :write 4097
[RPT*:PARNT] :pipe-size 0 :writing-bytes 4097
[RPT*:PARNT] :killing-after-inactivity-secs 2
```
In the above, process can successfully write 4096 bytes to the pipe, but blocks when tries to write 4097 bytes.

Pipe buffers behave the same across processes:
```
>stest :pipe-handle-to-child :pipe-size 0 :read 3 :write 5
[CMD*:PARNT] :pipe-handle-to-child :pipe-size 0 :read 3 :write 5
[RPT*:PARNT] :pipe-size 0 :reading-bytes 3
[CMD*:CHILD] :to-handle 164 5
[RPT*:CHILD] :writing-bytes 5
[RPT*:CHILD] :wrote-bytes 5
[RPT*:PARNT] :read-bytes 3 :read-chars $$$
[RPT*:CHILD] :exiting...
[RPT*:PARNT] :child-exited
```

In the above pipe's write handle was passed to the child, the child manages to write 5 bytes successfully into the pipe.

### pipe redirection to stderr

**Note**: The tool indicates with  _|_ in its commentary when stderr has been redirected to a pipe.

When spawning a child and redirecting its stderr to a pipe's write endpoint, the child's stderr stream mode becomes fully buffered:
```
>stest :pipe-to-child-stderr :pipe-size 0 :read 3 :write 5
[CMD*:PARNT] :pipe-to-child-stderr :pipe-size 0 :read 3 :write 5
[RPT*:PARNT] :pipe-size 0, :read-req-bytes 3
[CMD|:CHILD] :to-stderr :write 5
[RPT|:CHILD] :writing-bytes 5
[RPT|:CHILD] :wrote-bytes 5
[RPT|:CHILD] :exiting...
[RPT*:PARNT] :read-bytes 3 :read-chars $$$
[RPT*:PARNT] :child-exited
```

In the above, the parent process did not receive any output from the child until the child has exited (and thus its buffers were flushed).

The child's stderr buffer is 4096 bytes long:
```
>stest :pipe-to-child-stderr :pipe-size 0 :read 3 :write 4095
[CMD*:PARNT] :pipe-to-child-stderr :pipe-size 0 :read 3 :write 4095
[RPT*:PARNT] :pipe-size 0, :read-req-bytes 3
[CMD|:CHILD] :to-stderr :write 4095
[RPT|:CHILD] :writing-bytes 4095
[RPT|:CHILD] :wrote-bytes 4095
[RPT|:CHILD] :exiting...
[RPT*:PARNT] :read-bytes 3 :read-chars $$$
[RPT*:PARNT] :child-exited
```

```
>stest :pipe-to-child-stderr :pipe-size 0 :read 3 :write 4096
[CMD*:PARNT] :pipe-to-child-stderr :pipe-size 0 :read 3 :write 4096
[RPT*:PARNT] :pipe-size 0, :read-req-bytes 3
[CMD|:CHILD] :to-stderr :write 4096
[RPT|:CHILD] :writing-bytes 4096
[RPT|:CHILD] :wrote-bytes 4096
[RPT*:PARNT] :read-bytes 3 :read-chars $$$
[RPT|:CHILD] :exiting...
[RPT*:PARNT] :child-exited
```

Notice in the above that, in the first case (writing 4095 bytes) the parent only received the output after the child has exited (and thus the stderr buffers were flushed), while in the second case (writing 4096 bytes) the parent received the data immediately with the write, before the child exited.

#### stderr buffer mode

The child can change its stderr buffer mode with [setvbuf](https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/setvbuf).

It can be set to unbuffered mode, in which case characters appear into the pipe's buffer immediately:
```
>stest :pipe-to-child-stderr :pipe-size 1 :read 1 :write 1 :unbuf
[CMD*:PARNT] :pipe-to-child-stderr :pipe-size 1 :read 1 :write 1 :unbuf
[RPT*:PARNT] :pipe-size 1, :read-req-bytes 1
[CMD|:CHILD] :to-stderr :write 1 :unbuf
[RPT|:CHILD] :writing-bytes 1
[RPT*:PARNT] :read-bytes 1 :read-chars $
[RPT|:CHILD] :wrote-bytes 1
[RPT|:CHILD] :exiting...
[RPT*:PARNT] :child-exited
```

and as expected, trying to write more bytes to stderr than there is available space on the pipe buffer will cause the write operation to block:
```
>stest :pipe-to-child-stderr :pipe-size 1 :read 1 :write 3 :unbuf
[CMD*:PARNT] :pipe-to-child-stderr :pipe-size 1 :read 1 :write 3 :unbuf
[RPT*:PARNT] :pipe-size 1, :read-req-bytes 1
[CMD|:CHILD] :to-stderr :write 3 :unbuf
[RPT|:CHILD] :writing-bytes 3
[RPT*:PARNT] :read-bytes 1 :read-chars $
[RPT|:CHILD] :killing-after-inactivity-secs 1
[RPT*:PARNT] :child-exited
```

It can be set to line buffered mode, but on win32 systems the behavior is the same as fully buffered (see _Remarks_ in [setvbuf](https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/setvbuf)):
```
>stest :pipe-to-child-stderr :pipe-size 1 :read 1 :write-nl 2 :lnbuf 10
[CMD*:PARNT] :pipe-to-child-stderr :pipe-size 1 :read 1 :write-nl 2 :lnbuf 10
[RPT*:PARNT] :pipe-size 1, :read-req-bytes 1
[CMD|:CHILD] :to-stderr :write-nl 2 :lnbuf 10
[RPT|:CHILD] :writing-bytes 3
[RPT|:CHILD] :wrote-bytes 3
[RPT|:CHILD] :exiting...
[RPT*:PARNT] :read-bytes 1 :read-chars $
[RPT*:PARNT] :killing-after-inactivity-secs 2
```

In the above, sending a new line character to stderr had no effect.
stderr can be reset to fully buffered mode, in which case a custom buffer size can be specified:
```
>stest :pipe-to-child-stderr :pipe-size 0 :read 2 :write 8 :flbuf 8
[CMD*:PARNT] :pipe-to-child-stderr :pipe-size 0 :read 2 :write 8 :flbuf 8
[RPT*:PARNT] :pipe-size 0, :read-req-bytes 2
[CMD|:CHILD] :to-stderr :write 8 :flbuf 8
[RPT|:CHILD] :writing-bytes 8
[RPT|:CHILD] :wrote-bytes 8
[RPT*:PARNT] :read-bytes 2 :read-chars $$
[RPT|:CHILD] :exiting...
[RPT*:PARNT] :child-exited
```

and as expected, writing less characters to stderr than its buffer size, does not write anything to the pipe until the child exits and thus stderr is flushed:
```
>stest :pipe-to-child-stderr :pipe-size 0 :read 2 :write 7 :flbuf 8
[CMD*:PARNT] :pipe-to-child-stderr :pipe-size 0 :read 2 :write 7 :flbuf 8
[RPT*:PARNT] :pipe-size 0, :read-req-bytes 2
[CMD|:CHILD] :to-stderr :write 7 :flbuf 8
[RPT|:CHILD] :writing-bytes 7
[RPT|:CHILD] :wrote-bytes 7
[RPT|:CHILD] :exiting...
[RPT*:PARNT] :read-bytes 2 :read-chars $$
[RPT*:PARNT] :child-exited
```
While stderr is pushing data to the pipe but there is not enough space in the pipe to accommodate all of the them, the child process is blocked:
```
>stest :pipe-to-child-stderr :pipe-size 1 :read 0 :write 2 :flbuf 2
[CMD*:PARNT] :pipe-to-child-stderr :pipe-size 1 :read 0 :write 2 :flbuf 2
[RPT*:PARNT] :pipe-size 1, :read-req-bytes 0
[RPT*:PARNT] :read-bytes 0 :read-chars
[CMD|:CHILD] :to-stderr :write 2 :flbuf 2
[RPT|:CHILD] :writing-bytes 2
[RPT|:CHILD] :killing-after-inactivity-secs 1
[RPT*:PARNT] :child-exited
```

but when enough space is fried on the pipe, the child process is unblocked:
```
>stest :pipe-to-child-stderr :pipe-size 2 :read 0 :write 2 :flbuf 2
[CMD*:PARNT] :pipe-to-child-stderr :pipe-size 2 :read 0 :write 2 :flbuf 2
[RPT*:PARNT] :pipe-size 2, :read-req-bytes 0
[RPT*:PARNT] :read-bytes 0 :read-chars
[CMD|:CHILD] :to-stderr :write 2 :flbuf 2
[RPT|:CHILD] :writing-bytes 2
[RPT|:CHILD] :wrote-bytes 2
[RPT|:CHILD] :exiting...
[RPT*:PARNT] :child-exited
```

## socket redirection to stderr

**Note**: The tool indicates with  _&_ in its commentary when the stderr has been redirected to a socket.

Some socket types can be redirected to stderr. In the tests below, a pair of read/write sockets are created and connected to each other. The write socket is redirected to stderr.

The write socket has to be of a special type that can be treated as a file handle. It appears that they could be many types of such sockets registered in the system, the tool will select the first reported as being available.

The below tests were conducted using a write socket type of _MSAFD Tcpip [TCP/IP]_ (the first one reported by the [WSAEnumProtocols](https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaenumprotocolsa) fn).

The redirected stderr stream shares the same characteristics with the pipe redirection, i.e. it is fully buffered with a buffer size of 4096 bytes long.

fully buffered (write reaches the read socket only after the child exits):
```
>stest :sock-to-child-stderr :read 1 :write 1
[CMD*:PARNT] :sock-to-child-stderr :read 1 :write 1
[RPT*:PARNT] :socket-provider-IFS-selected MSAFD Tcpip [TCP/IP]
[RPT*:PARNT] :reading-bytes 1
[CMD&:CHILD] :to-stderr :write 1
[RPT&:CHILD] :writing-bytes 1
[RPT&:CHILD] :wrote-bytes 1
[RPT&:CHILD] :exiting...
[RPT*:PARNT] :read-bytes 1 :read-chars $
[RPT*:PARNT] :child-exited
```

buffer size is 4096 bytes long:
```
>stest :sock-to-child-stderr :read 1 :write 4095
[CMD*:PARNT] :sock-to-child-stderr :read 1 :write 4095
[RPT*:PARNT] :socket-provider-IFS-selected MSAFD Tcpip [TCP/IP]
[RPT*:PARNT] :reading-bytes 1
[CMD&:CHILD] :to-stderr :write 4095
[RPT&:CHILD] :writing-bytes 4095
[RPT&:CHILD] :wrote-bytes 4095
[RPT&:CHILD] :exiting...
[RPT*:PARNT] :read-bytes 1 :read-chars $
[RPT*:PARNT] :child-exited
```

```
>stest :sock-to-child-stderr :read 1 :write 4096
[CMD*:PARNT] :sock-to-child-stderr :read 1 :write 4096
[RPT*:PARNT] :socket-provider-IFS-selected MSAFD Tcpip [TCP/IP]
[RPT*:PARNT] :reading-bytes 1
[CMD&:CHILD] :to-stderr :write 4096
[RPT&:CHILD] :writing-bytes 4096
[RPT*:PARNT] :read-bytes 1 :read-chars $
[RPT&:CHILD] :wrote-bytes 4096
[RPT&:CHILD] :exiting...
[RPT*:PARNT] :child-exited
```

stderr buffer mode can be changed to unbuffered:
```
>stest :sock-to-child-stderr :read 1 :write 1 :unbuf
[CMD*:PARNT] :sock-to-child-stderr :read 1 :write 1 :unbuf
[RPT*:PARNT] :socket-provider-IFS-selected MSAFD Tcpip [TCP/IP]
[RPT*:PARNT] :reading-bytes 1
[CMD&:CHILD] :to-stderr :write 1 :unbuf
[RPT&:CHILD] :writing-bytes 1
[RPT*:PARNT] :read-bytes 1 :read-chars $
[RPT&:CHILD] :wrote-bytes 1
[RPT&:CHILD] :exiting...
[RPT*:PARNT] :child-exited
```

# terminals

The below tests check the type of stderr redirection (if any) that takes places when invoking a process from a few different type of terminals.

## command prompt

Invoked from the command prompt, stderr is attached to the console (_*_)
```
>stest :to-stderr :write-nl 1
[CMD*:PARNT] :to-stderr :write-nl 1
[RPT*:PARNT] :writing-bytes 2
$
[RPT*:PARNT] :wrote-bytes 2
[RPT*:PARNT] :exiting...
```
When all output is redirected to a file, stderr is indeed redirected to a file (_+_)
```
>stest :to-stderr :write-nl 1 1> test.txt 2>&1
>type test.txt
[CMD+:PARNT] :to-stderr :write-nl 1
[RPT+:PARNT] :writing-bytes 2
[RPT+:PARNT] :wrote-bytes 2
[RPT+:PARNT] :exiting...
$
```

child inherits the redirected-to-file _stderr_ (_+_) as expected:
```
>stest :to-child-stderr :write-nl 1 1> test.txt 2>&1
>type test.txt
[CMD+:PARNT] :to-child-stderr :write-nl 1
[CMD+:CHILD] :to-stderr :write-nl 1
[RPT+:CHILD] :writing-bytes 2
[RPT+:CHILD] :wrote-bytes 2
[RPT+:CHILD] :exiting...
$
[RPT+:PARNT] :child-exited
```


## MSYS2 MINGW64

Invoked from the MSYS2 MINGW64 terminal, stderr is redirected to a pipe (_|_):
```
$ ./stest.exe :to-stderr :write-nl 1
[CMD|:PARNT] :to-stderr :write-nl 1
[RPT|:PARNT] :writing-bytes 2
[RPT|:PARNT] :wrote-bytes 2
[RPT|:PARNT] :exiting...
$
```

When all output is redirected to a file, stderr is indeed redirected to a file (_+_):
```
$ ./stest.exe :to-stderr :write-nl 1 > test.txt 2>&1
$ cat test.txt
[CMD+:PARNT] :to-stderr :write-nl 1
[RPT+:PARNT] :writing-bytes 2
[RPT+:PARNT] :wrote-bytes 2
[RPT+:PARNT] :exiting...
$
```
child inherits the redirected-to-file _stderr_ (_+_) as expected:
```
$ ./stest.exe :to-child-stderr :write-nl 1 > test.txt 2>&1
$ cat test.txt
[CMD+:PARNT] :to-child-stderr :write-nl 1
[CMD+:CHILD] :to-stderr :write-nl 1
[RPT+:CHILD] :writing-bytes 2
[RPT+:CHILD] :wrote-bytes 2
[RPT+:CHILD] :exiting...
$
[RPT+:PARNT] :child-exited
```
## Emacs shell 

The Emacs shell (can be started with _M-x shell_), invokes the _cmdproxy.exe_ utility.

stderr is redirected to a pipe (_|_):
```
>stest :to-stderr :write-nl 1 
[CMD|:PARNT] :to-stderr :write-nl 1 
[RPT|:PARNT] :writing-bytes 2
[RPT|:PARNT] :wrote-bytes 2
[RPT|:PARNT] :exiting...
$
```

When all output is redirected to a file, stderr is indeed redirected to a file (_+_):
```
>stest :to-stderr :write-nl 1 > test.txt 2>&1 
>type test.txt 
[CMD+:PARNT] :to-stderr :write-nl 1 
[RPT+:PARNT] :writing-bytes 2
[RPT+:PARNT] :wrote-bytes 2
[RPT+:PARNT] :exiting...
$
```
child inherits the redirected-to-file _stderr_ (_+_) as expected:
```
>stest :to-child-stderr :write-nl 1 > test.txt 2>&1 
>type test.txt 
[CMD+:PARNT] :to-child-stderr :write-nl 1 
[CMD+:CHILD] :to-stderr :write-nl 1 
[RPT+:CHILD] :writing-bytes 2
[RPT+:CHILD] :wrote-bytes 2
[RPT+:CHILD] :exiting...
$
[RPT+:PARNT] :child-exited
```
