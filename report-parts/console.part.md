## console

### stderr is unbuffered

**Note**: The tool reports with the character _*_ in its commentary that stderr is attached to the console.

When invoked from command prompt (i.e. the console), stderr is unbuffered:

```
>stest :to-stderr :write 1
[CMD*:32196] :to-stderr :write 1
[RPT*:32196] :writing-bytes 1
$[RPT*:32196] :wrote-bytes 1
[RPT*:32196] :exiting...
```

the same  is true for a child process inheriting stderr from its parent (the default):
```
>stest :to-child-stderr :write 1
[CMD*:50024] :to-child-stderr :write 1
[CMD*:44688] :to-stderr :write 1
[RPT*:44688] :writing-bytes 1
$[RPT*:44688] :wrote-bytes 1
[RPT*:44688] :exiting...
[RPT*:50024] :child-exited-pid 44688
```

## pipe

The tool uses the [_pipe](https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/pipe) function to create pipes.

A pipe has a buffer whose size can be configured:

```
>stest :pipe :pipe-size 2 :read 2 :write 1
[CMD*:51160] :pipe :pipe-size 2 :read 2 :write 1
[RPT*:51160] :pipe-size 2 :writing-bytes 1
[RPT*:51160] :reading-bytes 2
[RPT*:51160] :read-bytes 1 :read-chars $
```

A writer will block if it tries to write more bytes that the space available on the pipe buffer:
```
>stest :pipe :pipe-size 2 :read 2 :write 3
[CMD*:38988] :pipe :pipe-size 2 :read 2 :write 3
[RPT*:38988] :pipe-size 2 :writing-bytes 3
^C
```

The writer will unblock once all bytes are written to the pipe:
```
[CMD*: 7080] :pipe-handle-to-child :pipe-size 1 :read 1 :write 2
[RPT*: 7080] :pipe-size 1 :reading-bytes 1
[CMD*:48540] :to-handle 160 2
[RPT*:48540] :writing-bytes 2
[RPT*:48540] :wrote-bytes 2
[RPT*: 7080] :read-bytes 1 :read-chars $
[RPT*:48540] :exiting...
[RPT*: 7080] :child-exited-pid 48540
```

In the above, parent reads from pipe of size 1, child attempts to write 2 chars to pipe. Child is blocked waiting for both chars to be written to the pipe. The first char makes it to the pipe buffer, parent consumes it. There is again space for one more char in the pipe's buffer, thus second bytes is written and child is unblocked. 

Specifying a pipe-size of 0, creates a pipe with a size of 4096 bytes long (the default size):
```>stest :pipe :pipe-size 0 :read 1 :write 4096
[CMD*:28708] :pipe :pipe-size 0 :read 1 :write 4096
[RPT*:28708] :pipe-size 0 :writing-bytes 4096
[RPT*:28708] :reading-bytes 1
[RPT*:28708] :read-bytes 1 :read-chars $
```

```
>stest :pipe :pipe-size 0 :read 1 :write 4097
[CMD*:23172] :pipe :pipe-size 0 :read 1 :write 4097
[RPT*:23172] :pipe-size 0 :writing-bytes 4097
^C
```

In the above, process can successfully write 4096 bytes to the pipe, but blocks when tries to write 4097 bytes.

Pipe buffers behave the same across processes:
```
>stest :pipe-handle-to-child :pipe-size 0 :read 3 :write 5
[CMD*:47240] :pipe-handle-to-child :pipe-size 0 :read 3 :write 5
[RPT*:47240] :pipe-size 0 :reading-bytes 3
[CMD*:40860] :to-handle 160 5
[RPT*:40860] :writing-bytes 5
[RPT*:40860] :wrote-bytes 5
[RPT*:47240] :read-bytes 3 :read-chars $$$
[RPT*:40860] :exiting...
[RPT*:47240] :child-exited-pid 40860
```

In the above pipe's write handle was passed to the child, the child manages to write 5 bytes successfully into the pipe.

### pipe redirection to stderr

**Note**: The tool indicates with  _|_ in its commentary when stderr has been redirected to a pipe.

When spawning a child and redirecting its stderr to a pipe's write endpoint, the child's stderr stream mode becomes fully buffered:
```
>stest :pipe-to-child-stderr :pipe-size 0 :read 3 :write 5
[CMD*:15924] :pipe-to-child-stderr :pipe-size 0 :read 3 :write 5
[RPT*:15924] :pipe-size 0, :read-req-bytes 3
[CMD|: 9768] :to-stderr :write 5
[RPT|: 9768] :writing-bytes 5
[RPT|: 9768] :wrote-bytes 5
[RPT|: 9768] :exiting...
[RPT*:15924] :read-bytes 3 :read-chars $$$
[RPT*:15924] :child-exited-pid 9768
```

In the above, the parent process did not receive any output from the child until the child has exited (and thus its buffers were flushed).

The child's stderr buffer is 4096 bytes long:
```
>stest :pipe-to-child-stderr :pipe-size 0 :read 3 :write 4095
[CMD*:56228] :pipe-to-child-stderr :pipe-size 0 :read 3 :write 4095
[RPT*:56228] :pipe-size 0, :read-req-bytes 3
[CMD|: 4956] :to-stderr :write 4095
[RPT|: 4956] :writing-bytes 4095
[RPT|: 4956] :wrote-bytes 4095
[RPT|: 4956] :exiting...
[RPT*:56228] :read-bytes 3 :read-chars $$$
[RPT*:56228] :child-exited-pid 4956
```

```
>stest :pipe-to-child-stderr :pipe-size 0 :read 3 :write 4096
[CMD*:50620] :pipe-to-child-stderr :pipe-size 0 :read 3 :write 4096
[RPT*:50620] :pipe-size 0, :read-req-bytes 3
[CMD|:19676] :to-stderr :write 4096
[RPT|:19676] :writing-bytes 4096
[RPT|:19676] :wrote-bytes 4096
[RPT*:50620] :read-bytes 3 :read-chars $$$
[RPT|:19676] :exiting...
[RPT*:50620] :child-exited-pid 19676
```

Notice in the above that, in the first case (writing 4095 bytes) the parent only received the output after the child has exited (and thus the stderr buffers were flushed), while in the second case (writing 4096 bytes) the parent received the data immediately with the write, before the child exited.

#### stderr buffer mode

The child can change its stderr buffer mode with [setvbuf](https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/setvbuf).

It can be set to unbuffered mode, in which case characters appear into the pipe's buffer immediately:
```
>stest :pipe-to-child-stderr :pipe-size 1 :read 1 :write 1 :unbuf
[CMD*:62688] :pipe-to-child-stderr :pipe-size 1 :read 1 :write 1 :unbuf
[RPT*:62688] :pipe-size 1, :read-req-bytes 1
[CMD|:63884] :to-stderr :write 1 :unbuf
[RPT|:63884] :writing-bytes 1
[RPT|:63884] :wrote-bytes 1
[RPT*:62688] :read-bytes 1 :read-chars $
[RPT|:63884] :exiting...
[RPT*:62688] :child-exited-pid 63884
```

and as expected, trying to write more bytes to stderr than there is available space on the pipe buffer will cause the write operation to block:
```
>stest :pipe-to-child-stderr :pipe-size 1 :read 1 :write 3 :unbuf
[CMD*:50436] :pipe-to-child-stderr :pipe-size 1 :read 1 :write 3 :unbuf
[RPT*:50436] :pipe-size 1, :read-req-bytes 1
[CMD|:67896] :to-stderr :write 3 :unbuf
[RPT|:67896] :writing-bytes 3
[RPT*:50436] :read-bytes 1 :read-chars $
^C
```

It can be set to line buffered mode, but on win32 systems the behavior is the same as fully buffered (see _Remarks_ in [setvbuf](https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/setvbuf)):
```
>stest :pipe-to-child-stderr :pipe-size 1 :read 1 :write-nl 2 :lnbuf 10
[CMD*:15024] :pipe-to-child-stderr :pipe-size 1 :read 1 :write-nl 2 :lnbuf 10
[RPT*:15024] :pipe-size 1, :read-req-bytes 1
[CMD|:33728] :to-stderr :write-nl 2 :lnbuf 10
[RPT|:33728] :writing-bytes 3
[RPT|:33728] :wrote-bytes 3
[RPT|:33728] :exiting...
[RPT*:15024] :read-bytes 1 :read-chars $
^C
```

In the above, sending a new line character to stderr had no effect.

stderr can be reset to fully buffered mode, in which case a custom buffer size can be specified:
```
>stest :pipe-to-child-stderr :pipe-size 0 :read 2 :write 8 :flbuf 8
[CMD*:30872] :pipe-to-child-stderr :pipe-size 0 :read 2 :write 8 :flbuf 8
[RPT*:30872] :pipe-size 0, :read-req-bytes 2
[CMD|:16016] :to-stderr :write 8 :flbuf 8
[RPT|:16016] :writing-bytes 8
[RPT|:16016] :wrote-bytes 8
[RPT*:30872] :read-bytes 2 :read-chars $$
[RPT|:16016] :exiting...
[RPT*:30872] :child-exited-pid 16016
```

and as expected, writing less characters to stderr than its buffer size, does not write anything to the pipe until the child exits and thus stderr is flushed:
```
>stest :pipe-to-child-stderr :pipe-size 0 :read 2 :write 7 :flbuf 8
[CMD*: 6348] :pipe-to-child-stderr :pipe-size 0 :read 2 :write 7 :flbuf 8
[RPT*: 6348] :pipe-size 0, :read-req-bytes 2
[CMD|:19744] :to-stderr :write 7 :flbuf 8
[RPT|:19744] :writing-bytes 7
[RPT|:19744] :wrote-bytes 7
[RPT|:19744] :exiting...
[RPT*: 6348] :read-bytes 2 :read-chars $$
[RPT*: 6348] :child-exited-pid 19744
```

While stderr is pushing data to the pipe but there is not enough space in the pipe to accommodate all of the them, the child process is blocked:
```
>stest :pipe-to-child-stderr :pipe-size 1 :read 0 :write 2 :flbuf 2
[CMD*:29380] :pipe-to-child-stderr :pipe-size 1 :read 0 :write 2 :flbuf 2
[RPT*:29380] :pipe-size 1, :read-req-bytes 0
[RPT*:29380] :read-bytes 0 :read-chars
[CMD|:19292] :to-stderr :write 2 :flbuf 2
[RPT|:19292] :writing-bytes 2
^C
```

but when enough space is fried on the pipe, the child process is unblocked:
```
>stest :pipe-to-child-stderr :pipe-size 2 :read 0 :write 2 :flbuf 2
[CMD*:55808] :pipe-to-child-stderr :pipe-size 2 :read 0 :write 2 :flbuf 2
[RPT*:55808] :pipe-size 2, :read-req-bytes 0
[CMD|: 3048] :to-stderr :write 2 :flbuf 2
[RPT*:55808] :read-bytes 0 :read-chars
[RPT|: 3048] :writing-bytes 2
[RPT|: 3048] :wrote-bytes 2
[RPT|: 3048] :exiting...
[RPT*:55808] :child-exited-pid 3048
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
[CMD*:72788] :sock-to-child-stderr :read 1 :write 1
[RPT*:72788] :socket-provider-IFS-selected MSAFD Tcpip [TCP/IP]
[RPT*:72788] :reading-bytes 1
[CMD&:49256] :to-stderr :write 1
[RPT&:49256] :writing-bytes 1
[RPT&:49256] :wrote-bytes 1
[RPT&:49256] :exiting...
[RPT*:72788] :read-bytes 1 :read-chars $
[RPT*:72788] :child-exited-pid 49256
```

buffer size is 4096 bytes long:
```
>stest :sock-to-child-stderr :read 1 :write 4095
[CMD*:71588] :sock-to-child-stderr :read 1 :write 4095
[RPT*:71588] :socket-provider-IFS-selected MSAFD Tcpip [TCP/IP]
[RPT*:71588] :reading-bytes 1
[CMD&:10084] :to-stderr :write 4095
[RPT&:10084] :writing-bytes 4095
[RPT&:10084] :wrote-bytes 4095
[RPT&:10084] :exiting...
[RPT*:71588] :read-bytes 1 :read-chars $
[RPT*:71588] :child-exited-pid 10084
```

```
>stest :sock-to-child-stderr :read 1 :write 4096
[CMD*:47768] :sock-to-child-stderr :read 1 :write 4096
[RPT*:47768] :socket-provider-IFS-selected MSAFD Tcpip [TCP/IP]
[RPT*:47768] :reading-bytes 1
[CMD&: 5204] :to-stderr :write 4096
[RPT&: 5204] :writing-bytes 4096
[RPT*:47768] :read-bytes 1 :read-chars $
[RPT&: 5204] :wrote-bytes 4096
[RPT&: 5204] :exiting...
[RPT*:47768] :child-exited-pid 5204
```

stderr buffer mode can be changed to unbuffered:
```
>stest :sock-to-child-stderr :read 1 :write 1 :unbuf
[CMD*:40196] :sock-to-child-stderr :read 1 :write 1 :unbuf
[RPT*:40196] :socket-provider-IFS-selected MSAFD Tcpip [TCP/IP]
[RPT*:40196] :reading-bytes 1
[CMD&:48588] :to-stderr :write 1 :unbuf
[RPT&:48588] :writing-bytes 1
[RPT*:40196] :read-bytes 1 :read-chars $
[RPT&:48588] :wrote-bytes 1
[RPT&:48588] :exiting...
[RPT*:40196] :child-exited-pid 48588
```

# terminals 

The below tests check the type of stderr redirection (if any) that takes places when invoking a process from a few different type of terminals.

## command prompt

Invoked from the command prompt, stderr is attached to the console (_*_):
```
>stest :to-stderr :write-nl 1
[CMD*:25076] :to-stderr :write-nl 1
[RPT*:25076] :writing-bytes 2
$
[RPT*:25076] :wrote-bytes 2
[RPT*:25076] :exiting...
```

When all output is redirected to a file, stderr is indeed redirected to a file (_+_):
```
>stest :to-stderr :write-nl 1 1> test.txt 2>&1
>type test.txt
[CMD+:60832] :to-stderr :write-nl 1
[RPT+:60832] :writing-bytes 2
[RPT+:60832] :wrote-bytes 2
[RPT+:60832] :exiting...
$
```
child inherits the redirected-to-file _stderr_ (_+_) as expected:
```
>stest :to-child-stderr :write-nl 1 1> test.txt 2>&1
>type test.txt
[CMD+:70088] :to-child-stderr :write-nl 1
[CMD+:19884] :to-stderr :write-nl 1
[RPT+:19884] :writing-bytes 2
[RPT+:19884] :wrote-bytes 2
[RPT+:19884] :exiting...
$
[RPT+:70088] :child-exited-pid 19884
```
