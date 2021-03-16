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
