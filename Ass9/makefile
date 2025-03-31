all: server client

server: cldp_server.c
	@gcc -o server cldp_server.c 

client: cldp_client.c
	@gcc -o client cldp_client.c

run_server: server
	@./server

run_client: client
	@./client

clean:
	@rm -f server client
