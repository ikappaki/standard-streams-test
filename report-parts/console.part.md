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
