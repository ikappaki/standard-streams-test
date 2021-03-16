@echo off
setlocal EnableDelayedExpansion

set report=%1

set options=(main example-output build msys2 emacs-shell COMPILE)

for %%o in %options% do if "%%o" == "%report%" goto :%%o

echo Available reports: %options%
goto :EXIT


:MAIN
echo ## console

echo.
echo ### stderr is unbuffered

echo.
echo **Note**: The tool reports with the character _*_ in its commentary that stderr is attached to the console.

echo.
echo When invoked from command prompt (i.e. the console), stderr is unbuffered:
echo ```
set _cmd=stest :to-stderr :write 1& echo ^>!_cmd! & !_cmd!
echo ```

echo.
echo the same is true for a child process inheriting stderr from its parent (the default):
echo ```
set _cmd=stest :to-child-stderr :write 1& echo ^>!_cmd! & !_cmd!
echo ```

echo.
echo ## pipe

echo.
echo The tool uses the [_pipe](https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/pipe) function to create pipes.

echo.
echo A pipe has a buffer whose size can be configured:
echo ```
set _cmd=stest :pipe :pipe-size 2 :read 2 :write 1& echo ^>!_cmd! & !_cmd!
echo ```

echo.
echo A writer will block if it tries to write more bytes that the space available on the pipe buffer:
echo ```
set _cmd=stest :pipe :pipe-size 2 :read 2 :write 3& echo ^>!_cmd! & !_cmd!
echo ```

echo.
echo The writer will unblock once all bytes are written to the pipe:
echo ```
set _cmd=stest :pipe-handle-to-child :pipe-size 1 :read 1 :write 2& echo ^>!_cmd! & !_cmd!
echo ```
echo.
echo In the above, parent reads from pipe of size 1, child attempts to write 2 chars to pipe. Child is blocked waiting for both chars to be written to the pipe. The first char makes it to the pipe buffer, parent consumes it. There is again space for one more char in the pipe's buffer, thus second bytes is written and child is unblocked.

echo.
echo Specifying a pipe-size of 0, creates a pipe with a size of 4096 bytes long (the default size):
echo ```
set _cmd=stest :pipe :pipe-size 0 :read 1 :write 4096& echo ^>!_cmd! & !_cmd!
echo ```
echo.
echo ```
set _cmd=stest :pipe :pipe-size 0 :read 1 :write 4097& echo ^>!_cmd! & !_cmd!
echo ```
echo In the above, process can successfully write 4096 bytes to the pipe, but blocks when tries to write 4097 bytes.

echo.
echo Pipe buffers behave the same across processes:
echo ```
set _cmd=stest :pipe-handle-to-child :pipe-size 0 :read 3 :write 5& echo ^>!_cmd! & !_cmd!
echo ```
echo. 
echo In the above pipe's write handle was passed to the child, the child manages to write 5 bytes successfully into the pipe.

echo.
echo ### pipe redirection to stderr

echo.
echo **Note**: The tool indicates with  _^|_ in its commentary when stderr has been redirected to a pipe.
echo.
echo When spawning a child and redirecting its stderr to a pipe's write endpoint, the child's stderr stream mode becomes fully buffered:
echo ```
set _cmd=stest :pipe-to-child-stderr :pipe-size 0 :read 3 :write 5& echo ^>!_cmd! & !_cmd!
echo ```
echo.
echo In the above, the parent process did not receive any output from the child until the child has exited (and thus its buffers were flushed).

echo.
echo The child's stderr buffer is 4096 bytes long:
echo ```
set _cmd=stest :pipe-to-child-stderr :pipe-size 0 :read 3 :write 4095& echo ^>!_cmd! & !_cmd!
echo ```
echo.
echo ```
set _cmd=stest :pipe-to-child-stderr :pipe-size 0 :read 3 :write 4096& echo ^>!_cmd! & !_cmd!
echo ```
echo.
echo Notice in the above that, in the first case (writing 4095 bytes) the parent only received the output after the child has exited (and thus the stderr buffers were flushed), while in the second case (writing 4096 bytes) the parent received the data immediately with the write, before the child exited.

echo.
echo #### stderr buffer mode

echo.
echo The child can change its stderr buffer mode with [setvbuf](https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/setvbuf).

echo.
echo It can be set to unbuffered mode, in which case characters appear into the pipe's buffer immediately:
echo ```
set _cmd=stest :pipe-to-child-stderr :pipe-size 1 :read 1 :write 1 :unbuf& echo ^>!_cmd! & !_cmd!
echo ```

echo.
echo and as expected, trying to write more bytes to stderr than there is available space on the pipe buffer will cause the write operation to block:
echo ```
set _cmd=stest :pipe-to-child-stderr :pipe-size 1 :read 1 :write 3 :unbuf& echo ^>!_cmd! & !_cmd!
echo ```

echo.
echo It can be set to line buffered mode, but on win32 systems the behavior is the same as fully buffered (see _Remarks_ in [setvbuf](https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/setvbuf)):
echo ```
set _cmd=stest :pipe-to-child-stderr :pipe-size 1 :read 1 :write-nl 2 :lnbuf 10& echo ^>!_cmd! & !_cmd!
echo ```
echo.
echo In the above, sending a new line character to stderr had no effect.

echo stderr can be reset to fully buffered mode, in which case a custom buffer size can be specified:
echo ```
set _cmd=stest :pipe-to-child-stderr :pipe-size 0 :read 2 :write 8 :flbuf 8& echo ^>!_cmd! & !_cmd!
echo ```

echo.
echo and as expected, writing less characters to stderr than its buffer size, does not write anything to the pipe until the child exits and thus stderr is flushed:
echo ```
set _cmd=stest :pipe-to-child-stderr :pipe-size 0 :read 2 :write 7 :flbuf 8& echo ^>!_cmd! & !_cmd!
echo ```

echo While stderr is pushing data to the pipe but there is not enough space in the pipe to accommodate all of the them, the child process is blocked:
echo ```
set _cmd=stest :pipe-to-child-stderr :pipe-size 1 :read 0 :write 2 :flbuf 2& echo ^>!_cmd! & !_cmd!
echo ```

echo.
echo but when enough space is fried on the pipe, the child process is unblocked:
echo ```
set _cmd=stest :pipe-to-child-stderr :pipe-size 2 :read 0 :write 2 :flbuf 2& echo ^>!_cmd! & !_cmd!
echo ```

echo.
echo ## socket redirection to stderr

echo.
echo **Note**: The tool indicates with  _^&_ in its commentary when the stderr has been redirected to a socket.

echo.
echo Some socket types can be redirected to stderr. In the tests below, a pair of read/write sockets are created and connected to each other. The write socket is redirected to stderr.

echo.
echo The write socket has to be of a special type that can be treated as a file handle. It appears that they could be many types of such sockets registered in the system, the tool will select the first reported as being available. 

echo.
echo The below tests were conducted using a write socket type of _MSAFD Tcpip [TCP/IP]_ (the first one reported by the [WSAEnumProtocols](https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaenumprotocolsa) fn).

echo.
echo The redirected stderr stream shares the same characteristics with the pipe redirection, i.e. it is fully buffered with a buffer size of 4096 bytes long.

echo.
echo fully buffered (write reaches the read socket only after the child exits):
echo ```
set _cmd=stest :sock-to-child-stderr :read 1 :write 1& echo ^>!_cmd! & !_cmd!
echo ```

echo.
echo buffer size is 4096 bytes long:
echo ```
set _cmd=stest :sock-to-child-stderr :read 1 :write 4095& echo ^>!_cmd! & !_cmd!
echo ```

echo.
echo ```
set _cmd=stest :sock-to-child-stderr :read 1 :write 4096& echo ^>!_cmd! & !_cmd!
echo ```

echo.
echo stderr buffer mode can be changed to unbuffered:
echo ```
set _cmd=stest :sock-to-child-stderr :read 1 :write 1 :unbuf& echo ^>!_cmd! & !_cmd!
echo ```

echo.
echo # terminals 

echo.
echo The below tests check the type of stderr redirection (if any) that takes places when invoking a process from a few different type of terminals.

echo. 
echo ## command prompt

echo.
echo Invoked from the command prompt, stderr is attached to the console (_*_)
echo ```
set _cmd=stest :to-stderr :write-nl 1& echo ^>!_cmd! & !_cmd!
echo ```

echo When all output is redirected to a file, stderr is indeed redirected to a file (_+_)
echo ```
set _cmd=stest :to-stderr :write-nl 1& echo ^>!_cmd! 1^> test.txt 2^>^&1 & !_cmd! 1> test.txt 2>&1
set _cmd=type test.txt& echo ^>!_cmd! & !_cmd! 
echo ```

echo.
echo child inherits the redirected-to-file _stderr_ (_+_) as expected:
echo ```
set _cmd=stest :to-child-stderr :write-nl 1& echo ^>!_cmd! 1^> test.txt 2^>^&1 & !_cmd! 1> test.txt 2>&1
set _cmd=type test.txt& echo ^>!_cmd! & !_cmd! 
echo ```


goto EXIT

:EXAMPLE-OUTPUT
echo # example output

echo.
echo Below is an example output when the program is instructed to invoke a child process which will write to its stderr (_:to-child-stderr_) 2 '$' characters followed by a new line (_:write-nl 2_)
echo ```
set _cmd=stest :to-child-stderr :write-nl 2& echo ^>!_cmd! & !_cmd!
echo ```

echo.
echo The output starting with _[XXXY:ZZZZZ]_ is commentary written (to stdout) by the parent and child processes.

echo.
echo The PRNT process was started with _CMD_ line arguments _:to-child-stderr :write-nl 2_, its stderr was attached to the console (_'*'_).

echo.
echo The CHILD process was started with command line arguments _:to-stderr :write-nl 2_, its standard error was attached to the console (_'*'_), as inherited from its parent process (the default).

echo.
echo _'$$\n'_ was printed to _stderr_ as instructed.

echo.
echo The CHILD process _RPT_'ed that it wrote _3_ bytes to _stderr_.

echo.
echo The stderr FILE-TYPE is reported with the following symbols
echo * '*' =^> _stderr_ is attached to the console.
echo * '^|' =^> _stderr_ has been redirected to a pipe.
echo * '^&' =^> _stderr_ has been redirected to a socket.
echo * '+' =^> _stderr_ has been redirected to a file. 

goto EXIT

:BUILD
echo # build

echo.
echo The tool has been tested to compile with the gcc mingw64 compiler bundled with the [MSYS2](https://www.msys2.org/) distribution.

echo.
echo To build, download and install MSYS2. open the [MINGW64 terminal](https://www.msys2.org/docs/terminals/) and run _make_
echo ```
del stest.exe
set _cmd=make& echo $ !_cmd! & !_cmd!
echo ```

echo.
echo then open a command prompt and type stest.exe to display the usage message
echo ```
set _cmd=stest& echo ^>!_cmd! & !_cmd!
echo ```

goto EXIT

:MSYS2
echo ## MSYS2 MINGW64

echo.
echo Invoked from the MSYS2 MINGW64 terminal, stderr is redirected to a pipe (_^|_):
echo ```
set _cmd=stest.exe :to-stderr :write-nl 1& echo $ ./!_cmd! & !_cmd!
echo ```

echo.
echo When all output is redirected to a file, stderr is indeed redirected to a file (_+_):
echo ```
set _cmd=stest.exe :to-stderr :write-nl 1& echo $ ./!_cmd! ^> test.txt 2^>^&1 & !_cmd! > test.txt 2>&1
set _cmd=cat test.txt& echo $ !_cmd! & !_cmd! 
echo ```
echo child inherits the redirected-to-file _stderr_ (_+_) as expected:
echo ```
set _cmd=stest.exe :to-child-stderr :write-nl 1& echo $ ./!_cmd! ^> test.txt 2^>^&1 & !_cmd! > test.txt 2>&1
set _cmd=cat test.txt& echo $ !_cmd! & !_cmd! 
echo ```

goto :EXIT

:EMACS-SHELL
echo ## Emacs shell 

echo.
echo The Emacs shell (can be started with _M-x shell_), invokes the _cmdproxy.exe_ utility.

echo.
echo stderr is redirected to a pipe (_^|_):
echo ```
set _cmd=stest :to-stderr :write-nl 1& echo ^>!_cmd! & !_cmd!
echo ```

echo.
echo When all output is redirected to a file, stderr is indeed redirected to a file (_+_):
echo ```
set _cmd=stest :to-stderr :write-nl 1& echo ^>!_cmd! ^> test.txt 2^>^&1 & !_cmd! > test.txt 2>&1
set _cmd=type test.txt& echo ^>!_cmd! & !_cmd! 
echo ```
echo child inherits the redirected-to-file _stderr_ (_+_) as expected:
echo ```
set _cmd=stest :to-child-stderr :write-nl 1& echo ^>!_cmd! ^> test.txt 2^>^&1 & !_cmd! > test.txt 2>&1
set _cmd=type test.txt& echo ^>!_cmd! & !_cmd! 
echo ```

goto :EXIT

:COMPILE
PowerShell -Command "& {(Get-Content report-parts/README.part.md)|Foreach-Object{if($_-match"""@@(.*part.md)@@"""){Get-Content report-parts/$($Matches.1)}else{$_}}|Set-Content README.md}"

:EXIT
