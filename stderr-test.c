/* A tool to investigate the buffering behavior of stderr on MS-Windows.

 MIT License

 Copyright (c) 2021 Ioannis Kappas

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE. */
     
#include <assert.h>
#include <io.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <winsock2.h>
#include <windows.h>


#define _DEBUG_DO 0

enum e_args {
  e_S=INT_MIN, /* start of commands barrier */
  eTO_STDERR, eTO_CHILD_STDERR, ePIPE,
  ePIPE_HANDLE_TO_CHILD, eTO_HANDLE,
  ePIPE_TO_CHILD_STDERR, eSOCK_TO_CHILD_STDERR,
  e_E,         /* end of commands barrier */
  eWRITE, eWRITE_NL,
  ePIPE_SIZE, eREAD,
  eUNBUF, eLNBUF, eFLBUF,
  e_I,         /* end of identifiers barrier */
};

static char const * usage[] =
  {"A utility to probe stderr's behavior on windows.",

   /* The order of entries below should match the order of commands in `e_args' */
   ":to-stderr :write|:write-nl COUNT [:unbuf|(:lnbuf|:flbuf BUFFER-SIZE)]"
     "\n\twrite COUNT '$' characters to stderr (:write-nl will also write an \\n at the end). Optionally change stderr's mode to unbuffered (:unbuf),  line (:lnbuf) or fully (:flbuf) buffered using a new buffer of BUFFER-SIZE.",
   ":to-child-stderr :write|:write-nl COUNT [:unbuf|(:lnbuf|:flbuf BUFFER-SIZE)]"
     "\n\tCreate a child process and have it write to its stderr stream. Takes same options as :to-stderr.",
   ":pipe :pipe-size SIZE :read RCOUNT :write WCOUNT"
     "\n\tCreate a _pipe() of size SIZE, write WCOUNT '$' characters to the pipe's write endpoint and read RCOUNT characters from the pipe's read endpoint.",
   ":pipe-handle-to-child :pipe-size SIZE :read RCOUNT :write WCOUNT"
     "\n\tCreate a _pipe() of size SIZE. Also create a child process passing the write pipe's handle as a command line argument to it. The child will open the handle and write WCOUNT '$' characters to it. The parent process will attempt to read RCOUNT characters from the pipe's read's endpoint.",
   ":to-handle HANDLE WRITE-COUNT"
     "\n\thelper option to support :pipe-handle-to-child. Attempts to open HANDLE and write WRITE-COUNT '$' characters to it.",
   ":pipe-to-child-stderr :pipe-size PSIZE :read RCOUNT :write|:write-nl WCOUNT [:unbuf|(:lnbuf|:flbuf BSIZE)]"
     "\n\tCreate a _pipe() of size PSIZE. Then create a child process with its stderr redirected to the pipe's write endpoint. The parent process will attempt to read RCOUNT characters from the pipe's read endpoint. The child process will attempt to write to its stderr (see :to-stderr for information on the write and buffering mode options).",
   ":sock-to-child-stderr :read RCOUNT :write|:write-nl WCOUNT [:unbuf|(:lnbuf|:flbuf BSIZE)]"
     "\n\tCreate a pair of read and write sockets. Then create a child process with its stderr redirected to the write socket. The parent process will attempt to read RCOUNT characters from the read socket. The child process will attempt to write to its stderr (see :to-stderr for information on the write and buffering mode options)."
  };

/* helper macros to assist with safe e_args indexing */
#define _CMD_INFO(C) usage[(C)-e_S]
#define _IDN_ASRT(C) assert((C)>e_E&&(C)<e_I);

/* variables and utility macros to assist with synchronizing logging
   output (to stdout) between parent and child by using a named mutex
   whose ID (i.e. name) is passed over to the child. */
static DWORD _PID=-1;static char _CM=' ';static HANDLE _OUTMX=NULL;static char* _MX_ID=NULL;
static char const * _RL="PARNT";
#define RPT(FS, ...) {DWORD wr=WaitForSingleObject(_OUTMX, INFINITE);                \
                      assert(wr==WAIT_OBJECT_0);                                     \
		      printf("[RPT%c:%s] " FS,_CM,_RL  __VA_OPT__(,) __VA_ARGS__); \
		      fflush(stdout);					             \
		      DWORD ro=ReleaseMutex(_OUTMX); assert(ro);}
#define _RPT_D(...) if (_DEBUG_DO) RPT(__VA_ARGS__)

/* logging version of assert */
#define ASSERT(COND) {bool cond=COND;     \
    if (!cond) {RPT(":ASSERTION-FAILED"   \
                    " %s :FILE %s, :LINE %d\n", #COND, __FILE__, __LINE__); abort(); }}

bool args_PARSE(int argc, char const * argv[], int args[]);
void log_SETUP(int argc, char const* argv[]);
int strings_JOIN(int aistart, int total, char const * prefix,
		 char const * argv[], char* buffer, int buffer_size);
void pipe_test(int pipe_size, int write_count, int read_count);
void pipe_handle_to_child(int pipe_size, int write_count, int read_count);
void pipe_to_child_stderr(int pipe_size, int read_count, char const * cmdargs);
void socket_to_child_stderr(int read_count, char const * cmdargs);



int main(int argc, char const * argv[])
{
  int args[argc];
  if (!args_PARSE(argc, argv, args)) return 1;
  ASSERT(args[0] == argc-1);

  log_SETUP(argc,argv);
  
  int ailast=-1; /* the index of the last argument considered */
  const int argslen = args[++ailast];
  
  switch(args[++ailast])
    {
    case eTO_STDERR:
      {
	enum e_args msg_type = args[++ailast]; _IDN_ASRT(msg_type);
	switch(msg_type) { case eWRITE: case eWRITE_NL: break; default: ASSERT(0); };

	int write_count = args[++ailast];
	
	ASSERT(write_count<5000);

	enum e_args mode=0; int buffer_size=1;
	if (ailast<argslen)
	  {
	    mode=args[++ailast]; _IDN_ASRT(mode);
	    switch (mode)
	      {
	      case eLNBUF: case eFLBUF:
		ASSERT(ailast<argslen); buffer_size=args[++ailast]; break;
	      case eUNBUF: break;
	      default: ASSERT(0);
	      }
	    ASSERT(buffer_size<4096);
	  }

	ASSERT( ailast == argslen );

        // must be created on the heap since it might be still used
        // past this block when the program is exited and stderr is
        // flushed.
	char* buffer = calloc(buffer_size+1, sizeof(char)); 
	if (mode)
	  {
	    int ret = setvbuf(stderr, buffer,
			      mode==eUNBUF ? _IONBF :
			      mode==eLNBUF ? _IOLBF :
			      _IOFBF,
			      buffer_size);
	    ASSERT( !ret );
	  }

	int msg_len=write_count;
	if (msg_type==eWRITE_NL) ++msg_len;
	char msg[msg_len];
	memset(msg, 0, sizeof(msg));
	memset(msg, '$', msg_len);
	if (msg_type==eWRITE_NL) msg[msg_len-1] = '\n';
	
	RPT(":writing-bytes %d\n", msg_len);
	int wrote = fwrite(msg, sizeof(char), msg_len, stderr);
	RPT(":wrote-bytes %d\n", wrote);

	RPT(":exiting...\n");

	return 0;
      }
    case eTO_CHILD_STDERR:
      {
	ASSERT( ailast+1 < argc );
	char subcmd[] = ":to-stderr";
	int cmdargs_size = strings_JOIN(++ailast, argc, subcmd, argv, NULL, 0);
	ASSERT(cmdargs_size<64);
	char cmdargs[cmdargs_size];
	strings_JOIN(ailast, argc, subcmd, argv, cmdargs, cmdargs_size);
      
	HANDLE child_SPAWN(char const * cmdargs, HANDLE stderr_handle);
	HANDLE child = child_SPAWN(cmdargs, NULL);

	WaitForSingleObject( child, INFINITE );
	RPT(":child-exited\n");

	return 0;
      }
    case ePIPE:
      {
	ASSERT( args[++ailast] == ePIPE_SIZE );
	int pipe_size = args[++ailast];
	ASSERT( args[++ailast] == eREAD );
	int read_count = args[++ailast];
	ASSERT( args[++ailast] == eWRITE );
	int write_count = args[++ailast];
	ASSERT(write_count < 5000);
      
	ASSERT( ailast == argslen );
	
	pipe_test(pipe_size, write_count, read_count);
	return 0; 
      }
    case ePIPE_HANDLE_TO_CHILD:
      {
	ASSERT( args[++ailast] == ePIPE_SIZE );
	int pipe_size = args[++ailast];
	ASSERT( args[++ailast] == eREAD );
	int read_count = args[++ailast];
	ASSERT( args[++ailast] == eWRITE );
	int write_count = args[++ailast];
	ASSERT(write_count < 5000);
      
	ASSERT( ailast == argslen );
      
	pipe_handle_to_child(pipe_size, write_count, read_count);
	return 0;
      }
    case eTO_HANDLE:
      {
	int arg_handle = args[++ailast];
	DWORD write_count = args[++ailast];

	ASSERT( ailast == argslen );
	

	HANDLE handle = (HANDLE)(UINT_PTR) arg_handle;

	char msg[write_count+1];
	memset(msg, 0, sizeof(msg));
	memset(msg, '$', sizeof(char)*write_count);

	RPT(":writing-bytes %ld\n", write_count);
	DWORD wrote;
	int retval = WriteFile(handle, msg, write_count, &wrote, NULL);
	ASSERT( retval != 0 );
	RPT(":wrote-bytes %ld\n", wrote);

	RPT(":exiting...\n");

	return 0;
      }
    case ePIPE_TO_CHILD_STDERR:
      {
	ASSERT( args[++ailast] == ePIPE_SIZE );
	int pipe_size = args[++ailast];
	ASSERT( args[++ailast] == eREAD );
	int read_count = args[++ailast];

	ASSERT( ailast+1 < argc );
	
	char subcmd[] = ":to-stderr";
	int cmdargs_size=strings_JOIN(++ailast, argc, subcmd, argv, NULL, 0);
	ASSERT(cmdargs_size<64);
	char cmdargs[cmdargs_size];
	strings_JOIN(ailast, argc, subcmd, argv, cmdargs, cmdargs_size);
      
	pipe_to_child_stderr(pipe_size, read_count, cmdargs);
	return 0;
      }
    case eSOCK_TO_CHILD_STDERR:
      {
	ASSERT( args[++ailast] == eREAD );
	int read_count = args[++ailast];

	ASSERT( ailast+1 < argc );
	
	char subcmd[] = ":to-stderr";
	int cmdargs_size=strings_JOIN(++ailast, argc, subcmd, argv, NULL, 0);
	ASSERT(cmdargs_size<64);
	char cmdargs[cmdargs_size];
	strings_JOIN(ailast, argc, subcmd, argv, cmdargs, cmdargs_size);
      
	socket_to_child_stderr(read_count, cmdargs);
	return 0;
      }
    default:
      ASSERT(0);
    }

  return 0;

}

HANDLE child_SPAWN(char const * cmdargs, HANDLE err_handle)
/* Spawn a new instance of the program with command line arguments
   CMDARGS. Optionally redirect the new program's stderr to ERR_HANDLE
   when set.
   
   Return the handle of the new process.
*/
{
  STARTUPINFO start;
  PROCESS_INFORMATION pi;

  memset (&start, 0, sizeof (start));
  start.cb = sizeof (start);
  start.dwFlags = SW_HIDE;

  /* A hack to pass the ID (i.e. name) of the logging
     synchronization mutex to the child using the Reserved2 field.
       
     See "Pass arbitrary data to a child process!" in
     http://www.catch22.net/tuts/undocumented-createprocess
  */
  int id_size = sizeof(DWORD)+strlen(_MX_ID)+1;
  char buf[id_size]; memset(buf, 0, id_size);
  strncpy(sizeof(DWORD)+buf, _MX_ID, id_size);
  start.cbReserved2 = sizeof(buf);
  start.lpReserved2 = (LPBYTE)buf;
    
  if (err_handle)
    {
      start.dwFlags |= STARTF_USESTDHANDLES;
      start.hStdInput = GetStdHandle (STD_INPUT_HANDLE);
      start.hStdOutput = GetStdHandle (STD_OUTPUT_HANDLE);

      start.hStdError = err_handle;
    }

  TCHAR cmd[MAX_PATH];
  GetModuleFileName(NULL, cmd, MAX_PATH);
  int cmdline_size = 1;
  cmdline_size+=snprintf(NULL, 0, "%s %s", cmd, cmdargs);
  ASSERT(cmdline_size < 128);
  char cmdline[cmdline_size];
  snprintf(cmdline, cmdline_size, "%s %s", cmd, cmdargs);
  _RPT_D(":parent/child-cmd %s\n", cmdline);
  
  int rc = CreateProcessA (NULL, cmdline, NULL, NULL, TRUE /* inherit handles? */,
			   0,
			   NULL, NULL, &start, &pi);
  ASSERT( rc != 0 );

  return pi.hProcess;
}

void pipe_test(int pipe_size, int write_count, int read_count)
/* Create a _pipe() of PIPE_SIZE. Write WRITE-COUNT '$' chars to
   pipe's write endpoint and then read READ-COUNT chars from pipe's
   read endpoint. 
*/
{
  char msg[write_count];
  memset(msg, '$', sizeof(char)*write_count);
  
  enum { READ, WRITE };
  int pfds[2];
  int rc = _pipe (pfds, pipe_size, _O_NOINHERIT | _O_BINARY);
  ASSERT( rc == 0 );

  
  RPT(":pipe-size %d :writing-bytes %d\n",
	pipe_size, write_count);
  int wrote_count = _write(pfds[WRITE], msg, write_count);
  ASSERT( wrote_count == write_count);

  char read_buffer[ read_count+1 ];
  memset(read_buffer, 0, read_count+1);
    
  RPT(":reading-bytes %d\n",
	read_count);
  int read = _read(pfds[READ], read_buffer, read_count);

  RPT(":read-bytes %d :read-chars %s\n",
	read, read_buffer);

  _close(pfds[READ]);
  _close(pfds[WRITE]);
}
      
void pipe_handle_to_child(int pipe_size, int write_count, int read_count)
/* Create a _pipe() of PIPE_SIZE. Spawn a child passing the pipe's
   write handle as command line argument to the child. The child
   writes WRITE-COUNT '$' chars to the write handle while the parent
   reads READ-COUNT chars. 
*/
{
  enum { READ, WRITE };
  int pfds[2];
  int rc = _pipe (pfds, pipe_size, _O_NOINHERIT | _O_BINARY);
  ASSERT( rc == 0 );
  HANDLE write_handle = (HANDLE) _get_osfhandle (pfds[WRITE]);
  {
    HANDLE parent = GetCurrentProcess ();
    int retval = DuplicateHandle (parent,
				  (HANDLE) _get_osfhandle (pfds[WRITE]),
				  parent,
				  &write_handle,
				  0,
				  TRUE,
				  DUPLICATE_SAME_ACCESS);
    ASSERT( retval != 0 );
    _close(pfds[WRITE]);
  }

      
  int cmdargs_size = 1;
  cmdargs_size+=snprintf(NULL, 0, ":to-handle %lld %d",
			 (UINT_PTR)write_handle, write_count);
  ASSERT(cmdargs_size < 32);
  char cmdargs[cmdargs_size];
  snprintf(cmdargs, cmdargs_size, ":to-handle %lld %d",
	   (UINT_PTR)write_handle, write_count);

  HANDLE child = child_SPAWN(cmdargs, NULL); ASSERT ( child );
  
  char read_buffer[ read_count+1 ];
  memset(read_buffer, 0, read_count+1);
    
  RPT(":pipe-size %d :reading-bytes %d\n",
	pipe_size,  read_count);
  int read = _read(pfds[READ], read_buffer, read_count);

  RPT(":read-bytes %d :read-chars %s\n",
	read, read_buffer); fflush(stdout);
 
  WaitForSingleObject(child, INFINITE);
  RPT(":child-exited\n");

  _close(pfds[READ]);
    
}


void pipe_to_child_stderr(int pipe_size, int read_count, char const * cmdargs)
/* Create a _pipe() of PIPE-SIZE. Spawn a new child process with
   command line arguments CMDARGS, redirecting its stderr to the
   pipe's write endpoint. The parent reads READ-COUNT chars from the
   pipe's read endpoint. 
*/
{
  enum { READ, WRITE };
  int pfds[2];
  int rc = _pipe (pfds, pipe_size, _O_NOINHERIT | _O_BINARY);
  ASSERT( rc == 0 );
  HANDLE write_handle = (HANDLE) _get_osfhandle (pfds[WRITE]);
  {
    HANDLE parent = GetCurrentProcess ();
    int retval = DuplicateHandle (parent,
				  (HANDLE) _get_osfhandle (pfds[WRITE]),
				  parent,
				  &write_handle,
				  0,
				  TRUE,
				  DUPLICATE_SAME_ACCESS);
    ASSERT( retval != 0 );
    _close(pfds[WRITE]);	  
  }

  HANDLE child = child_SPAWN(cmdargs, write_handle); ASSERT(child);

  char read_buffer[ read_count+1 ];
  memset(read_buffer, 0, read_count+1);
    
  RPT(":pipe-size %d, :read-req-bytes %d\n",
	pipe_size, read_count);
  int read = _read(pfds[READ], read_buffer, read_count);

  RPT(":read-bytes %d :read-chars %s\n",
	read, read_buffer); fflush(stdout);
  
  // wait for child to exit
  WaitForSingleObject(child, INFINITE );
  RPT(":child-exited\n");

  _close(pfds[READ]);
}

struct THREAD_ARGS {
  SOCKET socket_read;
  int read_count;
};

DWORD socket_READ(LPVOID _args)
/* read _ARGS.read_count chars from socket _ARGS.socket_read. _ARGS is
   of type `THREAD_ARGS'. 
*/
{
  struct THREAD_ARGS* args = (struct THREAD_ARGS*) _args;
  ASSERT(args->read_count<5000);
  
  _RPT_D(":socket-read :listening...\n");
  listen(args->socket_read , 1);

  struct sockaddr_in client;
  
  int size = sizeof(struct sockaddr_in);
  _RPT_D(":socket-read :accepting...\n");
  SOCKET socket = accept(args->socket_read , (struct sockaddr *)&client, &size);
  ASSERT(socket!=INVALID_SOCKET);
	
  int buffer_size = 1+args->read_count; 
  char read_buffer[buffer_size]; memset(read_buffer, 0, buffer_size);
  RPT(":reading-bytes %d\n", args->read_count);
  int recv_size = recv(socket, read_buffer, args->read_count, 0);
  ASSERT(recv_size != SOCKET_ERROR);
  
  RPT(":read-bytes %d :read-chars %s\n", recv_size, read_buffer);
  
  _RPT_D(":thread-exiting...\n");

  return 0;
}

void socket_to_child_stderr(int read_count, char const * cmdargs)
/* Create a pair of read/write TCP sockets. The write socket is
   created using the first available WSA TCP "protocol" which can make
   the socket also act as a file handle. The write socket is
   connected to the read socket.

   Spawns a child process with command line arguments CMDARGS. The
   child stderr is redirected to the write socket. 

   It reads READ-COUNT characters from the read socket.
*/
{
  WSADATA wsa;
  int rc = WSAStartup(MAKEWORD(2,2),&wsa);
  ASSERT(rc == 0);
  
  WSAPROTOCOL_INFO provider; memset(&provider, 0, sizeof(provider));
  {
    // locate the first TCP provider that can create a socket which
    // can act as a file.
    //
    // see https://stackoverflow.com/questions/58324162/are-wsasockets-able-to-take-process-i-o-without-the-use-of-pipes
    int protocol[] = { IPPROTO_TCP, 0 };

    unsigned int buffer_size = 4096; /* a guess of an upper memory requirements */
    BYTE buffer[buffer_size]; memset(&buffer, 0, sizeof(buffer));
    WSAPROTOCOL_INFO* pinfo = (WSAPROTOCOL_INFO*)&buffer;
    DWORD pinfo_size = sizeof(buffer);
    _RPT_D(":tcp-providers-getting...\n");
    int count = WSAEnumProtocols(protocol, pinfo, &pinfo_size);
    ASSERT( pinfo_size<=buffer_size );
    ASSERT(count > 0);
    int iprovider = -1;
    for (int i=0;i<count;i++)
      {
	_RPT_D(":provider %d :address-family %d :protocol %s :ifs? %s\n",
	       i, pinfo[0].iAddressFamily, pinfo[i].szProtocol,
	       pinfo[i].dwServiceFlags1 & XP1_IFS_HANDLES ? "true" : "false");
	// keep a reference of the first one found
	if (iprovider==-1
	    && pinfo[i].dwServiceFlags1 & XP1_IFS_HANDLES) iprovider = i;
      }

    ASSERT(iprovider!=-1);
    
    provider=pinfo[iprovider];
  }

  RPT(":socket-provider-IFS-selected %s\n", provider.szProtocol);
  SOCKET socket_write = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, &provider, 0, 0 );
  ASSERT(socket_write  != INVALID_SOCKET);

  SOCKET socket_read = socket(AF_INET, SOCK_STREAM, 0 );
  ASSERT(socket_read != INVALID_SOCKET);
  
  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  server.sin_port = 0; // any available port
  
  _RPT_D(":socket-read :binding...\n");
  rc = bind(socket_read ,(struct sockaddr *)&server , sizeof(server));
  ASSERT( rc != SOCKET_ERROR);
	
  // create a thread to listen for connections
  HANDLE thread;
  {
    struct THREAD_ARGS args;
    args.socket_read = socket_read;
    args.read_count = read_count;
    DWORD threadID;
    thread = CreateThread(NULL, 0, socket_READ, &args, 0,&threadID); 
  
    ASSERT(thread != NULL);
  }
  
  {
    int addrlen = sizeof(server);
    rc = getsockname(socket_read, (struct sockaddr *)&server, &addrlen); ASSERT(!rc);
    _RPT_D(":socket-read :port-opened %d\n", ntohs(server.sin_port));
  }

  // connect write socket to read socket 
  _RPT_D(":socket-write :connecting...\n");
  rc = connect(socket_write , (struct sockaddr *)&server , sizeof(server));
  ASSERT(rc != SOCKET_ERROR);
  
  HANDLE write_handle = (HANDLE)socket_write;
  HANDLE child = child_SPAWN(cmdargs, write_handle); ASSERT(child);

  WaitForSingleObject(child, INFINITE );
  RPT(":child-exited\n");
  
  WaitForSingleObject(thread, INFINITE );
  _RPT_D(":thread-exited\n");         
  closesocket(socket_read);
  closesocket(socket_write);	  
  WSACleanup();
}


bool args_PARSE(int argc, char const * argv[], int args[])
/* Parse ARGC number of arguments from ARGV, and on success place
   results in ARGS. First entry of ARGV is program name/path. ARGS
   must have enough space to accommodate ARGC number of entries.

   The arguments are parsed according to the descriptions in `usage'
   global variable.

   The first entry in ARGS is the count of arguments succesfully parsed. 

   Each other entry can be
   1. an e_args command or identifier, or
   2. an integer number

   It prints out `usage' in case of no arguments provided, or an error
   with an indicator/description where the parsing has gone wrong.

   It returns true on success, false otherwise.
*/
{
  assert(e_E-e_S == sizeof(usage)/sizeof(char*));
    
  memset(args, 0, sizeof(int)*argc);
  
  int c = 0;
  if (argc > 1)
    {
      for (int v=1;v<argc;v++)
	{
	  args[++c] = strtol(argv[v], NULL, 10);
	  assert(args[c]>e_I && "negative integer arg overlaps with reserved enum range");
	  if (args[c] < e_I
	      || (args[c] == 0 && strcmp("0", argv[v])))
	    {
	      enum e_args e = args[c] =
		!strcmp(":to-stderr"           , argv[v]) ? eTO_STDERR            :
		!strcmp(":to-child-stderr"     , argv[v]) ? eTO_CHILD_STDERR      :
		!strcmp(":pipe"                , argv[v]) ? ePIPE                 :
		!strcmp(":pipe-handle-to-child", argv[v]) ? ePIPE_HANDLE_TO_CHILD :
		!strcmp(":to-handle"           , argv[v]) ? eTO_HANDLE            :
		!strcmp(":pipe-to-child-stderr", argv[v]) ? ePIPE_TO_CHILD_STDERR :
		!strcmp(":sock-to-child-stderr", argv[v]) ? eSOCK_TO_CHILD_STDERR :
		!strcmp(":write"               , argv[v]) ? eWRITE                :
		!strcmp(":write-nl"            , argv[v]) ? eWRITE_NL             :
		!strcmp(":pipe-size"           , argv[v]) ? ePIPE_SIZE            :
		!strcmp(":read"                , argv[v]) ? eREAD                 :
		!strcmp(":unbuf"               , argv[v]) ? eUNBUF                :
		!strcmp(":lnbuf"               , argv[v]) ? eLNBUF                :
		!strcmp(":flbuf"               , argv[v]) ? eFLBUF                :
		e_S;

	      if (e==e_S) break;
	    }
	};
    }
  args[0] = c;
  
#define _USAGE() {printf("usage: %s COMMANDS\n\n%s\n\ncommands:\n",argv[0],usage[0]); \
                  for(int i=1;i<e_E-e_S;i++)printf("  %s\n\n",usage[i]);return false;}
  if (!args[0]) _USAGE();

  
  int x = 1;
#define _OPTIONS(C) {printf("%s",argv[0]);for(int i=1;i<x;i++)printf(" %s",argv[i]);\
                     printf(" ::error::\n\noptions:\n\t%s\t\n",usage[C-e_S]); return false;}
  enum e_args cmd = args[x];
  switch (cmd)
    {
    case eTO_STDERR: case eTO_CHILD_STDERR:
      if (++x > args[0]) _OPTIONS(cmd);
      switch (args[x])
	{
	case eWRITE: case eWRITE_NL:
	  /* WRITE-COUNT */
	  if (++x > args[0]) _OPTIONS(cmd);
	  if (args[x] <= 0) _OPTIONS(cmd);
	  break;
	default: _OPTIONS(cmd);
	}
      if (x < args[0]) switch(args[++x])
			 {
			 case eUNBUF: break;
			 case eLNBUF: case eFLBUF:
			   /* BUFFER-SIZE */
			   if (++x > args[0]) _OPTIONS(cmd);
			   if (args[x] <= 1) _OPTIONS(cmd);
			   break;
			 default: _OPTIONS(cmd);
			 }
      break;
    case ePIPE: case ePIPE_HANDLE_TO_CHILD:
      if (++x > args[0]) _OPTIONS(cmd);
      if (args[x] != ePIPE_SIZE) _OPTIONS(cmd);
      if (++x > args[0]) _OPTIONS(cmd);
      if (args[x] < 0) _OPTIONS(cmd);      
      
      if (++x > args[0]) _OPTIONS(cmd);
      if (args[x] != eREAD) _OPTIONS(cmd);
      if (++x > args[0]) _OPTIONS(cmd);
      if (args[x] < 0) _OPTIONS(cmd);      

      if (++x > args[0]) _OPTIONS(cmd);
      if (args[x] != eWRITE) _OPTIONS(cmd);
      if (++x > args[0]) _OPTIONS(cmd);
      if (args[x] <= 0) _OPTIONS(cmd);      
      break;
    case eTO_HANDLE:
      /* HANDLE */
      if (++x > args[0]) _OPTIONS(cmd);
      if (args[x] < 0) _OPTIONS(cmd);      
      /* WRITE-COUNT */
      if (++x > args[0]) _OPTIONS(cmd);
      if (args[x] < 0) _OPTIONS(cmd);      
      break;
    case ePIPE_TO_CHILD_STDERR:
      if (++x > args[0]) _OPTIONS(cmd);
      if (args[x] != ePIPE_SIZE) _OPTIONS(cmd);
      if (++x > args[0]) _OPTIONS(cmd);
      if (args[x] < 0) _OPTIONS(cmd);      

      if (++x > args[0]) _OPTIONS(cmd);
      if (args[x] != eREAD) _OPTIONS(cmd);
      if (++x > args[0]) _OPTIONS(cmd);
      if (args[x] < 0) _OPTIONS(cmd);      

      if (++x > args[0]) _OPTIONS(cmd);
      switch (args[x])
	{
	case eWRITE: case eWRITE_NL:
	  /* WRITE-COUNT */
	  if (++x > args[0]) _OPTIONS(cmd);
	  if (args[x] <= 0) _OPTIONS(cmd);
	  break;
	default: _OPTIONS(cmd);
	}
      if (x < args[0]) switch(args[++x])
			 {
			 case eUNBUF: break;
			 case eLNBUF: case eFLBUF:
			   /* BUFFER-SIZE */
			   if (++x > args[0]) _OPTIONS(cmd);
			   if (args[x] <= 1) _OPTIONS(cmd);
			   break;
			 default: _OPTIONS(cmd);
			 }
      break;
    case eSOCK_TO_CHILD_STDERR:
      if (++x > args[0]) _OPTIONS(cmd);
      if (args[x] != eREAD) _OPTIONS(cmd);
      if (++x > args[0]) _OPTIONS(cmd);
      if (args[x] < 0) _OPTIONS(cmd);      

      if (++x > args[0]) _OPTIONS(cmd);
      switch (args[x])
	{
	case eWRITE: case eWRITE_NL:
	  /* WRITE-COUNT */
	  if (++x > args[0]) _OPTIONS(cmd);
	  if (args[x] <= 0) _OPTIONS(cmd);
	  break;
	default: _OPTIONS(cmd);
	}
      if (x < args[0]) switch(args[++x])
			 {
			 case eUNBUF: break;
			 case eLNBUF: case eFLBUF:
			   /* BUFFER-SIZE */
			   if (++x > args[0]) _OPTIONS(cmd);
			   if (args[x] <= 1) _OPTIONS(cmd);
			   break;
			 default: _OPTIONS(cmd);
			 }
      break;
    default: _USAGE();
    }
  if (++x<=args[0])_OPTIONS(cmd);
  
  if (0) {printf(":args");for(int i=0;i<=c;i++){printf(" %d",args[i]);};printf("\n");};
  return true;
    
}

int strings_JOIN(int aistart, int total, char const * prefix,
		 char const * argv[], char* buffer, int buffer_size)
/* Join the strings in ARGV using space as a separator, starting from
   AISTART index and using PREFIX as the first string. ARGV has TOTAL
   number of entries.

 The resulting string is written in BUFFER of BUFFER-SIZE.

 When BUFFER is NULL or BUFFER-SIZE is 0, it returns the size of the
 buffer required to write the result, otherwise it returns
 BUFFER-SIZE.
*/
{
  if (buffer==NULL || buffer_size==0)
    {
      int argc = total;
      char const * subcmd = prefix;
      int cmdargs_size = 1+strlen(subcmd);
      for(int i=aistart;i<argc;i++) cmdargs_size+=1+strlen(argv[i]);
      return cmdargs_size;      
    }
  else
    {
      int argc = total;
      char const * subcmd = prefix;
      char* cmdargs = buffer;
      int cmdargs_size = buffer_size;
      
      memset(cmdargs,0,cmdargs_size);
      int copied=0;
      {
	const int subcmd_size = strlen(subcmd);
	assert(cmdargs_size>subcmd_size);
	strncpy(cmdargs, subcmd, subcmd_size);
	copied+=subcmd_size;
      }
      char const * separator = " ";
      const int sep_len = strlen(separator);
      for(int i=aistart;i<argc;i++)
	{
	  assert(cmdargs_size>copied+sep_len);
	  strncat(cmdargs,separator, cmdargs_size-copied);
	  copied+=sep_len;
	  const int str_size = strlen(argv[i]);
	  assert(cmdargs_size>copied+str_size);
	  strncat(cmdargs,argv[i],cmdargs_size-copied);
	  copied+=str_size;
	}
      assert(copied==cmdargs_size-1);
      return cmdargs_size;
    }
}

DWORD _EXIT(LPVOID _exit_ms)
{
  DWORD* exit_ms = (DWORD*)_exit_ms;
  Sleep(*exit_ms);
  RPT(":killing-after-inactivity-secs %ld\n", *exit_ms/1000);
  exit(99);
}

void log_SETUP(int argc, char const* argv[])
/* Setup global logging variables and print out the ARGC number of
   command line arguments found in ARGV that were used to start this
   process.

   When printing something out using this logging facility the file
   type of standard error will be indicated by the following symbols:

   '*' => char device or console  - `FILE_TYPE_CHAR'
   '+' => file                    - `FILE_TYPE_DISK'
   '|' => anonymous or named pipe - `FILE_TYPE_PIPE' but not a socket
   '&' => socket                  - `FILE_TYPE_PIPE' and is a socket
   '!' => remote                  - `FILE_TYPE_REMOTE'
   '?' => unknown
*/
{
  // time before the program exits forcefully. Has to be static since
  // it is used an arg to the thread
  static DWORD exit_ms=2000;

  _PID = GetProcessId(GetCurrentProcess());
  {
    HANDLE sh=(HANDLE)_get_osfhandle(_fileno(stderr)); assert(sh);
    DWORD ft=GetFileType(sh);
    _CM =
      ft==FILE_TYPE_CHAR   ? '*' :
      ft==FILE_TYPE_DISK   ? '+' :
      ft==FILE_TYPE_PIPE   ? '|' :
      ft==FILE_TYPE_REMOTE ? '!' : '?';

    if (_CM=='|' &&
	!GetNamedPipeInfo(sh, NULL, NULL, NULL, NULL))
      _CM = '&';
  }

  // name of mutex is simply prefix/PID
  char const * prefix = "pipe-test/";
  int prefix_len = strlen(prefix);
  
  // check if the parent process has passed in the name of the mutex
  // (thus this process is a child process) or create a new mutex for
  // sync'ing logging.
  STARTUPINFO si; memset(&si, 0, sizeof(si));
  GetStartupInfo(&si);
  if (si.cbReserved2>prefix_len
      && !strncmp(prefix, (char*)si.lpReserved2+sizeof(DWORD), strlen(prefix)))
    {
      _RL="CHILD";      
      exit_ms=1000;
      _MX_ID = calloc(si.cbReserved2, sizeof(char));
      strncpy(_MX_ID, (char*)si.lpReserved2+sizeof(DWORD), si.cbReserved2);
      _OUTMX = OpenMutex(MUTEX_ALL_ACCESS, FALSE, _MX_ID); assert(_OUTMX);
    }
  else
    {
      int id_size = 1;
      id_size+=snprintf(NULL, 0, "%s%ld", prefix, _PID);
      _MX_ID = calloc(id_size, sizeof(char));
      snprintf(_MX_ID, id_size, "%s%ld", prefix, _PID);
      _OUTMX = CreateMutex(NULL, FALSE, _MX_ID); assert(_OUTMX);
    }

  
  {
    DWORD wr=WaitForSingleObject(_OUTMX, INFINITE); assert(wr==WAIT_OBJECT_0);
    printf("[CMD%c:%s] ",_CM,_RL);
    for(int i=1;i<argc;i++){printf("%s ",argv[i]);} printf("\n");
    fflush(stdout);
    DWORD ro=ReleaseMutex(_OUTMX); assert(ro);
  }

  {
    DWORD threadID;
    HANDLE thread = CreateThread(NULL, 0, _EXIT, &exit_ms, 0,&threadID);
    ASSERT(thread != NULL);
  }
  
}
