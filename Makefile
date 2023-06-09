CC = gcc
BINARIES = hangman_client hangman_server

all: ${BINARIES}

hangman_client: hangman_client.c
	${CC} $^ -o $@

hangman_server: hangman_server.c
	${CC} $^ -o $@

clean:
	/bin/rm -f ${BINARIES} *.o 