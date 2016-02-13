all:
	gcc client.c -o client
	gcc server.c -o server

clean:
	$(RM) *\~ client server