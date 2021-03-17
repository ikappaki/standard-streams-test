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

@@example-output.part.md@@

@@build.part.md@@

# tests

All tests were conducted on Windows 10. 

@@console.part.md@@


@@msys2.part.md@@
@@emacs-shell.part.md@@
