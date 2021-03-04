stest.exe: stderr-test.c
	gcc -Wall -Wextra -Werror stderr-test.c -o stest.exe -lws2_32

