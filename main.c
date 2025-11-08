#include <stdio.h>
#include <string.h>
#include <sys/_types/_socklen_t.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <unistd.h>

#define PORT 8080

int main(void)
{
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (sock_fd == -1) {
        printf("Failed to create socket\n");
        return 1;
    }

    int opt = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0) {
        printf("Failed to set socket options\n");
        return 1;
    }

    struct sockaddr_in server_addr = { .sin_family = AF_INET,
                                       .sin_addr.s_addr = INADDR_ANY,
                                       .sin_port = htons(PORT) };

    if (bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        printf("Failed to bind to socket\n");
        return 1;
    }

    if (listen(sock_fd, 10) != 0) {
        printf("Failed to listen to socket\n");
        return 1;
    }

    printf("Listening to port %d...\n", PORT);

    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int con_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &client_addr_len);

        if (con_fd == -1) {
            printf("Connection with client failed\n");
            continue;
        }

        const char *response = "HTTP/1.1 200 OK\r\n\r\nHello, World!";
        write(con_fd, response, strlen(response));

        close(con_fd);
    }

    close(sock_fd);

    return 0;
}
