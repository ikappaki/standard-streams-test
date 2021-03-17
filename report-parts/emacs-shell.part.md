## Emacs shell 

The Emacs shell (can be started with _M-x shell_), invokes the _cmdproxy.exe_ utility.

stderr is redirected to a pipe (_|_):
```
>stest :to-stderr :write-nl 1
stest :to-stderr :write-nl 1
[CMD|:69876] :to-stderr :write-nl 1 
[RPT|:69876] :writing-bytes 2
[RPT|:69876] :wrote-bytes 2
[RPT|:69876] :exiting...
$
```

When all output is redirected to a file, stderr is indeed redirected to a file (_+_):
```
>stest :to-stderr :write-nl 1 > test.txt 2>&1
stest :to-stderr :write-nl 1 > test.txt 2>&1
>type test.txt
type test.txt
[CMD+:46324] :to-stderr :write-nl 1 
[RPT+:46324] :writing-bytes 2
[RPT+:46324] :wrote-bytes 2
[RPT+:46324] :exiting...
$
```
child inherits the redirected-to-file _stderr_ (_+_) as expected:
```
>stest :to-child-stderr :write-nl 1 > test.txt 2>&1
stest :to-child-stderr :write-nl 1 > test.txt 2>&1
>type test.txt
type test.txt
[CMD+:63896] :to-child-stderr :write-nl 1 
[CMD+:40500] :to-stderr :write-nl 1 
[RPT+:40500] :writing-bytes 2
[RPT+:40500] :wrote-bytes 2
[RPT+:40500] :exiting...
$
[RPT+:63896] :child-exited-pid 40500
```
