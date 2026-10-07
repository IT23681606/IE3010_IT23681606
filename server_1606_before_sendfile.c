#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

#define PORT 7606
#define MAX_CLIENTS 10
#define MAX_ROOMS 20
#define MAX_USERNAME 50
#define MAX_ROOMNAME 50
#define BUFFER_SIZE 2048

typedef struct {
    int socket;
    char username[MAX_USERNAME];
    int registered;
} Client;

typedef struct {
    char name[MAX_ROOMNAME];
    int members[MAX_CLIENTS];
    int member_count;
} Room;

Client clients[MAX_CLIENTS];
Room rooms[MAX_ROOMS];

int client_count = 0;
int room_count = 0;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rooms_mutex = PTHREAD_MUTEX_INITIALIZER;

void log_event(const char *event)
{
    FILE *fp = fopen("netmsg_IT23681606.log", "a");

    if (fp == NULL)
        return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    fprintf(fp,
            "[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
            t->tm_year + 1900,
            t->tm_mon + 1,
            t->tm_mday,
            t->tm_hour,
            t->tm_min,
            t->tm_sec,
            event);

    fclose(fp);
}

void send_response(int socket, const char *message)
{
    char response[BUFFER_SIZE];

    snprintf(response,
             sizeof(response),
             "%s NID:6816\n",
             message);

    send(socket, response, strlen(response), 0);
}

void broadcast_message(const char *message, int exclude_socket)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].registered &&
            clients[i].socket != exclude_socket)
        {
            send(clients[i].socket,
                 message,
                 strlen(message),
                 0);
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

int find_client_by_username(const char *username)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].registered &&
            strcmp(clients[i].username, username) == 0)
        {
            return i;
        }
    }

    return -1;
}

int find_room(const char *room_name)
{
    for (int i = 0; i < room_count; i++)
    {
        if (strcmp(rooms[i].name, room_name) == 0)
            return i;
    }

    return -1;
}

void remove_client_from_rooms(int client_index)
{
    pthread_mutex_lock(&rooms_mutex);

    for (int r = 0; r < room_count; r++)
    {
        for (int m = 0; m < rooms[r].member_count; m++)
        {
            if (rooms[r].members[m] == client_index)
            {
                for (int j = m; j < rooms[r].member_count - 1; j++)
                {
                    rooms[r].members[j] =
                        rooms[r].members[j + 1];
                }

                rooms[r].member_count--;
                m--;
            }
        }
    }

    pthread_mutex_unlock(&rooms_mutex);
}

void *handle_client(void *arg)
{
    int client_index = *(int *)arg;
    free(arg);

    int socket = clients[client_index].socket;

    char buffer[BUFFER_SIZE];

    /*
     * REGISTER must be the first command.
     */
    int bytes = recv(socket,
                     buffer,
                     sizeof(buffer) - 1,
                     0);

    if (bytes <= 0)
    {
        close(socket);
        return NULL;
    }

    buffer[bytes] = '\0';

    char command[BUFFER_SIZE];

    if (sscanf(buffer,
               "REGISTER %49[^\n]",
               command) != 1)
    {
        send_response(socket,
                      "ERR 005 REGISTER_REQUIRED");

        close(socket);
        return NULL;
    }

    /*
     * Check duplicate username.
     */
    pthread_mutex_lock(&clients_mutex);

    if (find_client_by_username(command) != -1)
    {
        pthread_mutex_unlock(&clients_mutex);

        send_response(socket,
                      "ERR 001 USERNAME_TAKEN");

        close(socket);
        return NULL;
    }

    strncpy(clients[client_index].username,
            command,
            MAX_USERNAME - 1);

    clients[client_index].username[MAX_USERNAME - 1] = '\0';

    clients[client_index].registered = 1;

    pthread_mutex_unlock(&clients_mutex);

    char log_message[BUFFER_SIZE];

    snprintf(log_message,
             sizeof(log_message),
             "User connected: %s",
             command);

    log_event(log_message);

    char response[BUFFER_SIZE];

    snprintf(response,
             sizeof(response),
             "OK REGISTERED %s",
             command);

    send_response(socket, response);

    /*
     * Notify other clients.
     */
    snprintf(response,
             sizeof(response),
             "MSG PRESENCE %s JOINED\n",
             command);

    broadcast_message(response, socket);

    /*
     * Main client command loop.
     */
    while (1)
    {
        memset(buffer, 0, sizeof(buffer));

        bytes = recv(socket,
                     buffer,
                     sizeof(buffer) - 1,
                     0);

        if (bytes <= 0)
        {
            break;
        }

        buffer[bytes] = '\0';

        /*
         * Remove newline.
         */
        buffer[strcspn(buffer, "\r\n")] = '\0';

        /*
         * LIST
         */
        if (strcmp(buffer, "LIST") == 0)
        {
            char users[BUFFER_SIZE] = "OK USERS ";

            pthread_mutex_lock(&clients_mutex);

            int first = 1;

            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (clients[i].registered)
                {
                    if (!first)
                        strcat(users, ",");

                    strcat(users,
                           clients[i].username);

                    first = 0;
                }
            }

            pthread_mutex_unlock(&clients_mutex);

            send_response(socket, users);
        }

        /*
         * BCAST
         */
        else if (strncmp(buffer,
                         "BCAST ",
                         6) == 0)
        {
            char *message = buffer + 6;

            if (strlen(message) == 0)
            {
                send_response(socket,
                              "ERR 005 EMPTY_MESSAGE");

                continue;
            }

            snprintf(response,
                     sizeof(response),
                     "OK SENT");

            send_response(socket, response);

            char outgoing[BUFFER_SIZE];

            snprintf(outgoing,
                     sizeof(outgoing),
                     "MSG BCAST %s %s\n",
                     clients[client_index].username,
                     message);

            broadcast_message(outgoing, socket);

            snprintf(log_message,
                     sizeof(log_message),
                     "Broadcast from %s: %s",
                     clients[client_index].username,
                     message);

            log_event(log_message);
        }

        /*
         * PMSG
         */
        else if (strncmp(buffer,
                         "PMSG ",
                         5) == 0)
        {
            char target[MAX_USERNAME];
            char message[BUFFER_SIZE];

            if (sscanf(buffer + 5,
                       "%49s %[^\n]",
                       target,
                       message) != 2)
            {
                send_response(socket,
                              "ERR 005 INVALID_MESSAGE");

                continue;
            }

            pthread_mutex_lock(&clients_mutex);

            int target_index =
                find_client_by_username(target);

            if (target_index == -1)
            {
                pthread_mutex_unlock(&clients_mutex);

                send_response(socket,
                              "ERR 002 USER_NOT_FOUND");

                continue;
            }

            snprintf(response,
                     sizeof(response),
                     "MSG PRIV %s %s\n",
                     clients[client_index].username,
                     message);

            send(clients[target_index].socket,
                 response,
                 strlen(response),
                 0);

            pthread_mutex_unlock(&clients_mutex);

            send_response(socket, "OK SENT");
        }

        /*
         * JOIN room
         */
        else if (strncmp(buffer,
                         "JOIN ",
                         5) == 0)
        {
            char room_name[MAX_ROOMNAME];

            sscanf(buffer + 5,
                   "%49s",
                   room_name);

            pthread_mutex_lock(&rooms_mutex);

            int room_index = find_room(room_name);

            if (room_index == -1)
            {
                if (room_count >= MAX_ROOMS)
                {
                    pthread_mutex_unlock(&rooms_mutex);

                    send_response(socket,
                                  "ERR 005 ROOM_LIMIT");

                    continue;
                }

                room_index = room_count++;

                strcpy(rooms[room_index].name,
                       room_name);

                rooms[room_index].member_count = 0;
            }

            int already_member = 0;

            for (int i = 0;
                 i < rooms[room_index].member_count;
                 i++)
            {
                if (rooms[room_index].members[i] ==
                    client_index)
                {
                    already_member = 1;
                    break;
                }
            }

            if (!already_member)
            {
                rooms[room_index]
                    .members[rooms[room_index].member_count++] =
                    client_index;
            }

            pthread_mutex_unlock(&rooms_mutex);

            snprintf(response,
                     sizeof(response),
                     "OK JOINED %s",
                     room_name);

            send_response(socket, response);
        }

        /*
         * LEAVE room
         */
        else if (strncmp(buffer,
                         "LEAVE ",
                         6) == 0)
        {
            char room_name[MAX_ROOMNAME];

            sscanf(buffer + 6,
                   "%49s",
                   room_name);

            pthread_mutex_lock(&rooms_mutex);

            int room_index = find_room(room_name);

            if (room_index == -1)
            {
                pthread_mutex_unlock(&rooms_mutex);

                send_response(socket,
                              "ERR 003 ROOM_NOT_FOUND");

                continue;
            }

            int found = 0;

            for (int i = 0;
                 i < rooms[room_index].member_count;
                 i++)
            {
                if (rooms[room_index].members[i] ==
                    client_index)
                {
                    for (int j = i;
                         j < rooms[room_index].member_count - 1;
                         j++)
                    {
                        rooms[room_index].members[j] =
                            rooms[room_index].members[j + 1];
                    }

                    rooms[room_index].member_count--;

                    found = 1;
                    break;
                }
            }

            pthread_mutex_unlock(&rooms_mutex);

            if (found)
            {
                snprintf(response,
                         sizeof(response),
                         "OK LEFT %s",
                         room_name);

                send_response(socket, response);
            }
            else
            {
                send_response(socket,
                              "ERR 003 ROOM_NOT_FOUND");
            }
        }

        /*
         * ROOMS
         */
        else if (strcmp(buffer, "ROOMS") == 0)
        {
            char room_list[BUFFER_SIZE] =
                "OK ROOMS ";

            pthread_mutex_lock(&rooms_mutex);

            for (int i = 0; i < room_count; i++)
            {
                if (i > 0)
                    strcat(room_list, ",");

                strcat(room_list,
                       rooms[i].name);
            }

            pthread_mutex_unlock(&rooms_mutex);

            send_response(socket, room_list);
        }

        /*
         * RMSG
         */
        else if (strncmp(buffer,
                         "RMSG ",
                         5) == 0)
        {
            char room_name[MAX_ROOMNAME];
            char message[BUFFER_SIZE];

            if (sscanf(buffer + 5,
                       "%49s %[^\n]",
                       room_name,
                       message) != 2)
            {
                send_response(socket,
                              "ERR 005 INVALID_MESSAGE");

                continue;
            }

            pthread_mutex_lock(&rooms_mutex);

            int room_index = find_room(room_name);

            if (room_index == -1)
            {
                pthread_mutex_unlock(&rooms_mutex);

                send_response(socket,
                              "ERR 003 ROOM_NOT_FOUND");

                continue;
            }

            snprintf(response,
                     sizeof(response),
                     "MSG ROOM %s %s %s\n",
                     room_name,
                     clients[client_index].username,
                     message);

            for (int i = 0;
                 i < rooms[room_index].member_count;
                 i++)
            {
                int member_index =
                    rooms[room_index].members[i];

                if (clients[member_index].socket != socket)
                {
                    send(clients[member_index].socket,
                         response,
                         strlen(response),
                         0);
                }
            }

            pthread_mutex_unlock(&rooms_mutex);

            send_response(socket, "OK SENT");
        }

        /*
         * QUIT
         */
        else if (strcmp(buffer, "QUIT") == 0)
        {
            send_response(socket, "OK BYE");

            break;
        }

        /*
         * Unknown command
         */
        else
        {
            send_response(socket,
                          "ERR 005 UNKNOWN_COMMAND");
        }
    }

    /*
     * Client disconnected.
     */
    char username[MAX_USERNAME];

    strcpy(username,
           clients[client_index].username);

    remove_client_from_rooms(client_index);

    pthread_mutex_lock(&clients_mutex);

    clients[client_index].registered = 0;
    clients[client_index].username[0] = '\0';
    clients[client_index].socket = -1;

    pthread_mutex_unlock(&clients_mutex);

    snprintf(response,
             sizeof(response),
             "MSG PRESENCE %s LEFT\n",
             username);

    broadcast_message(response, socket);

    snprintf(log_message,
             sizeof(log_message),
             "User disconnected: %s",
             username);

    log_event(log_message);

    close(socket);

    return NULL;
}

int main()
{
    int server_fd;

    struct sockaddr_in server_addr;

    printf("====================================\n");
    printf("        NetMessenger Server\n");
    printf("====================================\n");
    printf("Registration : IT23681606\n");
    printf("NID          : 6816\n");
    printf("TCP Port     : 7606\n");
    printf("Status       : Listening...\n");

    server_fd =
        socket(AF_INET,
               SOCK_STREAM,
               0);

    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    int opt = 1;

    setsockopt(server_fd,
               SOL_SOCKET,
               SO_REUSEADDR,
               &opt,
               sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr =
        INADDR_ANY;
    server_addr.sin_port =
        htons(PORT);

    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    log_event("Server started");

    while (1)
    {
        struct sockaddr_in client_addr;

        socklen_t client_len =
            sizeof(client_addr);

        int client_socket =
            accept(server_fd,
                   (struct sockaddr *)&client_addr,
                   &client_len);

        if (client_socket < 0)
        {
            perror("accept");
            continue;
        }

        pthread_mutex_lock(&clients_mutex);

        int index = -1;

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (!clients[i].registered &&
                clients[i].socket == 0)
            {
                index = i;
                break;
            }
        }

        if (index == -1)
        {
            pthread_mutex_unlock(&clients_mutex);

            send(client_socket,
                 "ERR 005 SERVER_FULL NID:6816\n",
                 31,
                 0);

            close(client_socket);

            continue;
        }

        clients[index].socket = client_socket;
        clients[index].registered = 0;

        client_count++;

        pthread_mutex_unlock(&clients_mutex);

        int *arg = malloc(sizeof(int));

        *arg = index;

        pthread_t thread;

        if (pthread_create(&thread,
                           NULL,
                           handle_client,
                           arg) != 0)
        {
            perror("pthread_create");

            close(client_socket);

            free(arg);

            pthread_mutex_lock(&clients_mutex);

            clients[index].socket = 0;

            client_count--;

            pthread_mutex_unlock(&clients_mutex);

            continue;
        }

        pthread_detach(thread);
    }

    close(server_fd);

    return 0;
}
