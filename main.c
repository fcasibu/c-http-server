#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_types/_socklen_t.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <unistd.h>

enum request_parsing_state { PARSING_REQUEST_LINE, PARSING_REQUEST_HEADERS };

#define PORT 8080
#define MAX_REQUEST_BYTES 4096

struct request_headers {
    char *content_type;

    char *host;
    char *user_agent;
};

struct request {
    struct request_headers headers;
    char *method;
    char *path;
    char *version;
};

void print_parsed_request(struct request *req);
void parse_request_header(struct request_headers *headers, char *read_buffer);
void parse_request_line(struct request *req, char *read_buffer);
void parse_request(struct request *req, const char *request_buffer, size_t request_buffer_size);
void free_request(struct request *req);

void free_request(struct request *req)
{
    if (req->method) {
        free(req->method);
    }

    if (req->path) {
        free(req->path);
    }

    if (req->version) {
        free(req->version);
    }

    if (req->headers.content_type) {
        free(req->headers.content_type);
    }

    if (req->headers.host) {
        free(req->headers.host);
    }

    if (req->headers.user_agent) {
        free(req->headers.user_agent);
    }
}

void print_parsed_request(struct request *req)
{
    printf("REQUEST LINE:\n");
    printf("METHOD = %s\n", req->method);
    printf("PATH = %s\n", req->path);
    printf("VERSION = %s\n\n", req->version);
    printf("REQUEST HEADERS:\n");

    if (req->headers.content_type) {
        printf("Content-Type: %s\n", req->headers.content_type);
    }

    if (req->headers.host) {
        printf("Host: %s\n", req->headers.host);
    }

    if (req->headers.user_agent) {
        printf("User-Agent: %s\n", req->headers.user_agent);
    }
}

void parse_request_header(struct request_headers *headers, char *read_buffer)
{
    if (!read_buffer) {
        printf("No tokens to read in request headers\n");
        return;
    }

    const char *delimiter = ": ";
    char *tok = NULL;

    tok = strtok(read_buffer, delimiter);
    if (!tok) {
        printf("Invalid header name in headers\n");
        return;
    }

    const char *header_name_tok = tok;

    tok = strtok(0, delimiter);

    if (strcmp(header_name_tok, "Content-Type") == 0) {
        headers->content_type = strdup(tok);
    } else if (strcmp(header_name_tok, "Host") == 0) {
        headers->host = strdup(tok);
    } else if (strcmp(header_name_tok, "User-Agent") == 0) {
        headers->user_agent = strdup(tok);
    }
}

void parse_request_line(struct request *req, char *read_buffer)
{
    if (!read_buffer) {
        printf("No tokens to read in request line\n");
        return;
    }

    const char *delimiter = " ";
    char *tok = NULL;

    tok = strtok(read_buffer, delimiter);
    if (!tok) {
        printf("Invalid method in request line\n");
        return;
    }

    req->method = strdup(tok);

    tok = strtok(0, delimiter);

    if (!tok) {
        printf("Invalid path in request line\n");
        return;
    }

    req->path = strdup(tok);

    tok = strtok(0, delimiter);

    if (!tok) {
        printf("Invalid HTTP version in request line\n");
        return;
    }

    req->version = strdup(tok);
}

void parse_request(struct request *req, const char *request_buffer, size_t request_buffer_size)
{
    enum request_parsing_state state = PARSING_REQUEST_LINE;
    char buffer[request_buffer_size];
    size_t buffer_index = 0;
    size_t start = 0;

    while (true) {
        if (buffer_index >= request_buffer_size) {
            break;
        }

        buffer[buffer_index] = request_buffer[buffer_index];

        switch (state) {
        case PARSING_REQUEST_LINE: {
            if (buffer[buffer_index - 1] == '\r' && buffer[buffer_index] == '\n') {
                char *copied_buffer = strndup(buffer + start, buffer_index - start);

                if (!copied_buffer) {
                    printf("Failed to copy buffer string\n");
                    break;
                }

                parse_request_line(req, copied_buffer);
                state = PARSING_REQUEST_HEADERS;

                free(copied_buffer);

                start = buffer_index;
            }
        } break;

        case PARSING_REQUEST_HEADERS: {
            if (buffer[buffer_index - 1] == '\r' && buffer[buffer_index] == '\n') {
                char *copied_buffer = strndup(buffer + start + 1, buffer_index - start - 1);

                if (!copied_buffer) {
                    printf("Failed to copy buffer string\n");
                    break;
                }

                parse_request_header(&req->headers, copied_buffer);

                free(copied_buffer);

                start = buffer_index;
            }
        }

        break;

        default: {
            printf("Unknown parsing state\n");
            break;
        }
        }

        buffer_index += 1;
    }
}

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

        char request_buffer[MAX_REQUEST_BYTES];
        size_t bytes_read = read(con_fd, request_buffer, sizeof(request_buffer) - 1);
        if (bytes_read == -1) {
            printf("Failed to read the request\n");
            continue;
        }

        struct request req = { 0 };
        parse_request(&req, request_buffer, bytes_read);

        print_parsed_request(&req);

        free_request(&req);

        const char *response = "HTTP/1.1 200 OK\r\n\r\nHello, World!";
        write(con_fd, response, strlen(response));

        close(con_fd);
    }

    close(sock_fd);

    return 0;
}
