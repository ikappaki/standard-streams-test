## MSYS2 MINGW64

Invoked from the MSYS2 MINGW64 terminal, stderr is redirected to a pipe (_|_):
```
$ ./stest.exe :to-stderr :write-nl 1
[CMD|:67224] :to-stderr :write-nl 1
[RPT|:67224] :writing-bytes 2
[RPT|:67224] :wrote-bytes 2
[RPT|:67224] :exiting...
$
```

When all output is redirected to a file, stderr is indeed redirected to a file (_+_):
```
$ ./stest.exe :to-stderr :write-nl 1 > test.txt 2>&1
$ cat test.txt
[CMD+:18708] :to-stderr :write-nl 1
[RPT+:18708] :writing-bytes 2
[RPT+:18708] :wrote-bytes 2
[RPT+:18708] :exiting...
$
```
child inherits the redirected-to-file _stderr_ (_+_) as expected:
```
$ ./stest.exe :to-child-stderr :write-nl 1 > test.txt 2>&1
$ cat test.txt
[CMD+:18168] :to-child-stderr :write-nl 1
[CMD+:15484] :to-stderr :write-nl 1
[RPT+:15484] :writing-bytes 2
[RPT+:15484] :wrote-bytes 2
[RPT+:15484] :exiting...
$
[RPT+:18168] :child-exited-pid 15484
```
