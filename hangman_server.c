#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define MAX_WORD_LENGTH 8
#define MAX_GUESS_LENGTH 6
#define MAX_CLIENTS 3
#define BUFFER_SIZE 128

// Client struct for storing data about a client
typedef struct {
    int sockfd;
    int remaining_guesses;
    char guessed_letters[MAX_GUESS_LENGTH + 1];
    char incorrect_letters[MAX_GUESS_LENGTH + 1];
} Client;

// Error handling function
void error(char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// Function to read words from a file
int fileExtract(const char* filename, char words[15][MAX_WORD_LENGTH+1]) {
    FILE *fptr = fopen(filename, "r");
    if (fptr == NULL) {
        printf("Not able to open the file.\n");
        return -1;
    }

    int wordCount = 0;
    char temp[MAX_WORD_LENGTH + 2];

    // Read words into the words array until end of file or maximum words are reached
    while (fgets(temp, MAX_WORD_LENGTH + 2, fptr) != NULL && wordCount < 15) {
        temp[strcspn(temp, "\n")] = '\0';  // Remove newline character if present
        strncpy(words[wordCount], temp, MAX_WORD_LENGTH);
        wordCount++;
    }

    fclose(fptr);
    return wordCount;
}

// Function to update the hangman game status
void hangmanUpdate(char c, char* cli_word, char* word, Client* client) {
    int correct = 0;

    // Check if the letter exists in the word
    for (size_t i = 0; i < strlen(word); i++) {
        if (word[i] == c) {
            correct = 1;
            cli_word[i] = c;
        }
    }

    // If letter not found and not already guessed, add it to guessed letters and if not in word, add to incorrect letters
    if (correct == 0 && strchr(client->guessed_letters, c) == NULL) {
        client->guessed_letters[strlen(client->guessed_letters)] = c;
        client->guessed_letters[strlen(client->guessed_letters) + 1] = '\0';

        if (strchr(word, c) == NULL) {
            client->incorrect_letters[strlen(client->incorrect_letters)] = c;
            client->incorrect_letters[strlen(client->incorrect_letters) + 1] = '\0';
            client->remaining_guesses--;
        }
    }
}

int main(int argc, char *argv[]) {
    // Initialize game data and server settings
    char words[15][MAX_WORD_LENGTH + 1];
    int wordCount = fileExtract("hangman_words.txt", words);

    int sockfd = 0;
    int newsockfd = 0;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    int newsockfds[MAX_CLIENTS] = {0};
    int cli_wordnos[MAX_CLIENTS];
    fd_set fds;
    size_t wordlen;

    // Setup and bind server socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { 
        error("ERROR opening socket"); 
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(8080);

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        error("ERROR failed to set socket options");
    }

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR binding");
    }

    if (listen(sockfd, MAX_CLIENTS) < 0) {
        error("ERROR listening");
    }

    // Game data per client
    char buffer[BUFFER_SIZE];
    char cli_words[MAX_CLIENTS][MAX_WORD_LENGTH + 1];
    Client clients[MAX_CLIENTS];

    int totalClients = 0;
    char targets[MAX_CLIENTS][MAX_WORD_LENGTH + 1];

    // Main server loop
    for (;;) {
        // Reset file descriptor set
        FD_ZERO(&fds);
        FD_SET(sockfd, &fds);
        int max_fd = sockfd;

        // Add client sockets to the set
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (newsockfds[i] > 0) {
                FD_SET(newsockfds[i], &fds);
                if (newsockfds[i] > max_fd) {
                    max_fd = newsockfds[i];
                }
            }
        }

        // Select the ready socket descriptor
        if (select(max_fd + 1, &fds, NULL, NULL, NULL) < 0 && errno != EINTR) {
            error("ERROR select");
        }

        // Handle new connections
        if (FD_ISSET(sockfd, &fds)) {
            newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
            if (newsockfd < 0) {
                error("ERROR on accept");
            }

            // If server capacity is not full, accept the client
            if (totalClients < MAX_CLIENTS) {
                // Loop to find a slot for new client
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (newsockfds[i] == 0) {
                        // Setup the new client
                        newsockfds[i] = newsockfd;
                        int wordno = rand() % wordCount;
                        wordlen = strlen(words[wordno]);
                        cli_wordnos[i] = wordno;

                        memset(clients[i].guessed_letters, 0, sizeof(clients[i].guessed_letters));
                        memset(clients[i].incorrect_letters, 0, sizeof(clients[i].incorrect_letters));
                        clients[i].remaining_guesses = MAX_GUESS_LENGTH;

                        memset(cli_words[i], '_', sizeof(cli_words[i]));
                        cli_words[i][wordlen] = '\0';
                        totalClients++;
                        clients[i].sockfd = newsockfd;
                        write(newsockfd, &(int){0}, 1);
                        
                        int word_length = strlen(words[wordno]);
                        write(newsockfd, &word_length, sizeof(int));

                        break;
                    }
                }
            }
            else {  // Server capacity full, refuse the connection
                write(newsockfd, &(int){17}, 1);
                write(newsockfd, "server-overloaded", 17);
                close(newsockfd);
            }
        }

        // Handle client messages
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (newsockfds[i] > 0 && FD_ISSET(newsockfds[i], &fds)) {
                int n = recv(newsockfds[i], buffer, 1, 0);
                if (n < 0) {
                    error("ERROR receiving");
                }
                if (n == 0) {  // Client disconnected
                    printf("Client %d disconnected.\n", i+1);
                    close(newsockfds[i]);
                    newsockfds[i] = 0;
                    totalClients--;
                }
                else {
                    // Handle client guess
                    int msglen = buffer[0];
                    if (msglen == 0) {
                        break;
                    }

                    n = recv(newsockfds[i], buffer, msglen, 0);
                    if (n < 0) {
                        error("ERROR receiving");
                    }

                    // Check for termination message
                    if (strcmp(buffer, "Client terminated") == 0) {
                        printf("Client %d terminated connection.\n", i+1);
                        close(newsockfds[i]);
                        newsockfds[i] = 0;
                        totalClients--;
                        break;
                    }

                    // Update the game status
                    hangmanUpdate(buffer[0], cli_words[i], words[cli_wordnos[i]], &clients[i]);

                    // Game end condition: correct guess!
                    if (strcmp(cli_words[i], words[cli_wordnos[i]]) == 0) {
                        write(newsockfds[i], &(int){33 + strlen(words[cli_wordnos[i]])}, 1);
                        write(newsockfds[i], "The word was ", 13);
                        write(newsockfds[i], words[cli_wordnos[i]], strlen(words[cli_wordnos[i]]));
                        write(newsockfds[i], "\nYou win!\nGame Over!", 21);
                        close(newsockfds[i]);
                        newsockfds[i] = 0;
                        totalClients--;
                    }
                    // Game end condition: no more guesses!
                    else if (clients[i].remaining_guesses == 0) {
                        write(newsockfds[i], &(int){34 + strlen(words[cli_wordnos[i]])}, 1);
                        write(newsockfds[i], "The word was ", 13);
                        write(newsockfds[i], words[cli_wordnos[i]], strlen(words[cli_wordnos[i]]));
                        write(newsockfds[i], "\nYou lose!\nGame Over!", 21);
                        close(newsockfds[i]);
                        newsockfds[i] = 0;
                        totalClients--;
                    }         
                    else {
                        write(newsockfds[i], &(int){0}, 1);
                        int cli_word_len = strlen(cli_words[i]);
                        int cli_guesses_len = strlen(clients[i].guessed_letters);
                        write(newsockfds[i], &cli_word_len, 1);
                        write(newsockfds[i], &cli_guesses_len, 1);
                        write(newsockfds[i], cli_words[i], cli_word_len);
                        write(newsockfds[i], clients[i].guessed_letters, cli_guesses_len);
                    }
                }
            }
        }
    }
    return 0;
}
