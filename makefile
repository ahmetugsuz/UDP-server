CFLAGS = -std=gnu11 -g -Wall -Wextra
VFLAGS = --leak-check=full --show-leak-kinds=all --track-origins=yes --malloc-fill=0x40 --free-fill=0x23
all: upush_server upush_client

upush_client: upush_client.o send_packet.o send_packet.h
	gcc $(CFLAGS) upush_client.o send_packet.o -o upush_client 

upush_server: upush_server.o send_packet.o send_packet.h
	gcc $(CFLAGS) upush_server.o send_packet.o -o upush_server 

upush_client.o: upush_client.c 
	gcc $(CFLAGS) -c upush_client.c 

upush_server.o: upush_server.c 
	gcc $(CFLAGS) -c upush_server.c 

send_packet.o: send_packet.c 
	gcc $(CFLAGS) -c send_packet.c 


check_server: upush_server
	valgrind $(VFLAGS) ./upush_server 2233 0

check_client_ole: upush_client
	valgrind $(VFLAGS) ./upush_client ole 127.0.0.1 2233 2 0

check_client_ahmet: upush_client
	valgrind $(VFLAGS) ./upush_client ahmet 127.0.0.1 2233 2 0

check_client_ida: upush_client
	valgrind $(VFLAGS) ./upush_client ida 127.0.0.1 2233 2 0

ificp:
	scp upush_server.c upush_client.c send_packet.c send_packet.h Makefile ahmettu@login.ifi.uio.no:~/Documents/IN2140/eksamen

clean:
	rm -f $(BIN) *.o 