.PHONY: all server client dummy1 dummy2 dummy3 dummy4 clean

all: server client dummy1 dummy2 dummy3 dummy4

server: server.c
	gcc -o server server.c 

client: client.c
	gcc -o client client.c

dummy1: bad_client_1.c
	gcc -o dummy1 bad_client_1.c 

dummy2: bad_client_2.c
	gcc -o dummy2 bad_client_2.c 

dummy3: bad_client_3.c
	gcc -o dummy3 bad_client_3.c 

dummy4: bad_client_4.c
	gcc -o dummy4 bad_client_4.c 

run_server:
	./server

run_client:
	./client 5

run_dummy1:
	./dummy1

run_dummy2:
	./dummy2

run_dummy3:
	./dummy3

run_dummy4:
	./dummy4

clean:
	rm -f server client dummy1 dummy2 dummy3 dummy4
