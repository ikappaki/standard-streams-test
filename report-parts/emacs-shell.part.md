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
