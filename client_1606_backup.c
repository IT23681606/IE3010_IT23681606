#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_IP "127.0.0.1"
#define PORT 7606
#define BUFFER_SIZE 1024

int sock_fd;

void *receive_messages(void *arg)
{
    char buffer[BUFFER_SIZE];

    while (1)
    {
        memset(buffer, 0, sizeof(buffer));

        int bytes_received = recv(sock_fd, buffer,
                                  sizeof(buffer) - 1, 0);

        if (bytes_received <= 0)
        {
            printf("\nServer disconnected.\n");
            break;
        }

        buffer[bytes_received] = '\0';

        printf("\n%s", buffer);
        printf("Message: ");
        fflush(stdout);
    }

    return NULL;
}

int main(void)
{
    struct sockaddr_in server_addr;
    pthread_t receiver_thread;
    char message[BUFFER_SIZE];

    printf("====================================\n");
    printf("        NetMessenger Client\n");
    printf("====================================\n");

    printf("Connecting to NetMessenger Server...\n");
    printf("Server: %s\n", SERVER_IP);
    printf("Port: %d\n", PORT);

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (sock_fd < 0)
    {
        perror("socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP,
                  &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton");
        close(sock_fd);
        return 1;
    }

    if (connect(sock_fd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(sock_fd);
        return 1;
    }

    printf("Connected successfully to the server.\n");
    printf("You can now send messages.\n");
    printf("Type /quit to disconnect.\n\n");

    if (pthread_create(&receiver_thread,
                       NULL,
                       receive_messages,
                       NULL) != 0)
    {
        perror("pthread_create");
        close(sock_fd);
        return 1;
    }

    while (1)
    {
        printf("Message: ");
        fflush(stdout);

        if (fgets(message, sizeof(message), stdin) == NULL)
        {
            break;
        }

        if (strcmp(message, "/quit\n") == 0)
        {
            break;
        }

        if (send(sock_fd,
                 message,
                 strlen(message),
                 0) < 0)
        {
            perror("send");
            break;
        }
    }

    close(sock_fd);

    pthread_cancel(receiver_thread);
    pthread_join(receiver_thread, NULL);

    printf("\nDisconnected from server.\n");

    return 0;
}
