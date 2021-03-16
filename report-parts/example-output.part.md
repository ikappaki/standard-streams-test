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
