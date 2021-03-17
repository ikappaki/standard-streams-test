# example output

Below is an example output when the program is instructed to invoke a child process which will write to its stderr (_:to-child-stderr_) 2 '$' characters followed by a new line (_:write-nl 2_)
```
>stest :to-child-stderr :write-nl 2
[CMD*:64340] :to-child-stderr :write-nl 2
[CMD*:36744] :to-stderr :write-nl 2
[RPT*:36744] :writing-bytes 3
$$
[RPT*:36744] :wrote-bytes 3
[RPT*:36744] :exiting...
[RPT*:64340] :child-exited-pid 36744
```

The output starting with _[XXXYZZZ]_ is commentary written (to stdout) by the parent and child processes.

The parent process was started with _CMD_ line arguments _:to-child-stderr :write-nl 2_, its PID was _64340_ and its stderr was attached to the console (_'*'_).

The child process was started with command line arguments _:to-stderr :write-nl 2_, its PID was _36744_ and its standard error was attached to the console (_'*'_), as inherited from its parent process (the default).

_'$$\n'_ was printed to _stderr_ as instructed.

The process with PID _36744_ (i.e. the child) _RPT_'ed that it wrote _3_ bytes to _stderr_.

The stderr FILE-TYPE is reported with the following symbols
* '*' => _stderr_ is attached to the console.
* '|' => _stderr_ has been redirected to a pipe.
* '&' => _stderr_ has been redirected to a socket.
* '+' => _stderr_ has been redirected to a file. 
