CC := gcc
send_ip := 127.0.0.1
send_port := 8080
receive_ip := 127.0.0.1
receive_port := 9090
inputfile := input.txt
outputfile := output.txt

libname := ksocket

all: libksocket.a
	$(CC) -o user1 user1.c -L. -lksocket 
	$(CC) -o user2 user2.c -L. -lksocket 
libksocket.a: k_socket.c k_socket.h
	$(CC) -Wall -c k_socket.c
	ar rcs libksocket.a k_socket.o

run_user2: 
	./user2 $(receive_ip) $(receive_port) $(send_ip) $(send_port) $(outputfile)

run_user1:
	./user1 $(send_ip) $(send_port) $(receive_ip) $(receive_port) $(inputfile)

clean:
	rm user1 user2 $(outputfile) libksocket.a k_socket.o