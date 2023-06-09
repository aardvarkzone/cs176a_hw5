/*
skeleton code for TCP connection sourced from:
https://stackoverflow.com/questions/11405819/does-struct-hostent-have-a-field-h-addr
https://www.cs.rpi.edu/~moorthy/Courses/os98/Pgms/client.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define h_addr h_addr_list[0] /* for backward compatibility */
#define BUFFER_SIZE 128

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// Function to connect to the server
int connectToServer(const char *hostname, int port) {
    struct hostent *server;
    struct sockaddr_in serv_addr;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // Check if socket creation was successful 
    if (sockfd < 0) {
        error("ERROR opening socket");
    }

    // Get the host information 
    server = gethostbyname(hostname);
    if (server == NULL) {
        error("ERROR unknown host");
    }

    // Setup the server details 
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);

    // Connect to the server 
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR connecting");
    }

    return sockfd;
}

// Function to play hangman game 
void playHangman(int sockfd) {
    char buffer[BUFFER_SIZE];
    char letter;

    read(sockfd, buffer, 1);
    if (buffer[0] > 0) {
        read(sockfd, buffer, buffer[0]);
        buffer[strlen(buffer)] = '\0';
        printf("%s", buffer);
        return;
    }

    printf(">>>Ready to start game? (y/n): ");
    fflush(stdout);
    fgets(buffer, BUFFER_SIZE, stdin);
    if (buffer[0] != 'y') {
        close(sockfd);
        return;
    }

    send(sockfd, &(int){0}, 1, 0);

    int word_length;

    // Receive the word length from the server
    read(sockfd, &word_length, sizeof(int));

    printf(">>>");
    for (int i = 0; i < word_length; i++) {
        if (i != (word_length - 1)) {
            printf("_ ");
        } else {
            printf("_\n");
        }
    }
    printf(">>>Incorrect Guesses: \n>>>\n");


    for (;;) {
        printf(">>>Letter to guess: ");
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {  // Check for EOF
            if (feof(stdin)) {
                printf("\n");
                // Send termination message to server 
                char term_msg[] = "Client terminated";
                write(sockfd, term_msg, sizeof(term_msg));

                close(sockfd);
                exit(EXIT_SUCCESS);
            }
        }

        buffer[strcspn(buffer, "\n")] = '\0';

        letter = buffer[0];
        if (strlen(buffer) != 1 || !isalpha(letter)) {
            printf("Error! Please guess one letter.\n");
        } else {
            buffer[0] = 1;
            buffer[1] = tolower(letter);
            write(sockfd, buffer, 2);

            read(sockfd, buffer, 1);
            if (buffer[0] == 0) {
                int wordlen, guesslen;
                read(sockfd, buffer, 1);
                wordlen = buffer[0];
                read(sockfd, buffer, 1);
                guesslen = buffer[0];

                memset(buffer, 0, BUFFER_SIZE);
                read(sockfd, buffer, wordlen);

                printf(">>>");
                for (int i = 0; i < wordlen; i++) {
                    if (i != wordlen - 1){
                        printf("%c ", buffer[i]);
                    } else {
                        printf("%c\n", buffer[i]);
                    }   
                }

                printf(">>>Incorrect Guesses: ");

                if (guesslen > 0) {
                    memset(buffer, 0, BUFFER_SIZE);
                    read(sockfd, buffer, guesslen);

                    for (int i = 0; i < guesslen; i++) {
                       if (i != guesslen - 1){
                            printf("%c ", buffer[i]);
                        } else {
                            printf("%c\n", buffer[i]);
                        }   
                    }
                }
                printf("\n");
            } else {
                int msglen = buffer[0];
                memset(buffer, 0, BUFFER_SIZE);
                usleep(8000);
                read(sockfd, buffer, msglen);
                printf("%s\n", buffer);
                return;
            }
        }
    }
}



int main(int argc, char *argv[]) {
    // ./hangman_client [IP] [port number... use 8080] 
    if (argc != 3) {
        fprintf(stderr, "usage %s hostname port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Connect to server 
    int sockfd = connectToServer(argv[1], atoi(argv[2]));
    
    // Play hangman game 
    playHangman(sockfd);

    // Close the socket connection 
    close(sockfd);

    return 0;
}