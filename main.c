#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <sys/syslimits.h>
#include <unistd.h>

enum request_parsing_state { PARSING_REQUEST_LINE, PARSING_REQUEST_HEADERS };

#define PORT 8080
#define MAX_REQUEST_BYTES 4096
#define FILE_BUFFER_SIZE 4096
#define MAX_HEADER_SIZE 256
#define HTTP_403 "HTTP/1.1 403 Forbidden\r\n\r\n"
#define HTTP_404 "HTTP/1.1 404 Not Found\r\n\r\n"
#define HTTP_500 "HTTP/1.1 500 Internal Server Error\r\n\r\n"

struct request_headers {
    char *content_type;
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
void parse_request(struct request *req, const char *request_buffer);
void free_request(struct request *req);

const char *get_mime_type(const char *ext);
void send_response(int con_fd, char *path);

const char *get_mime_type(const char *ext)
{
    if (strcmp(ext, "html") == 0) {
        return "text/html";
    }

    if (strcmp(ext, "css") == 0) {
        return "text/css";
    }

    if (strcmp(ext, "js") == 0) {
        return "application/javascript";
    }

    if (strcmp(ext, "png") == 0) {
        return "image/png";
    }

    if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0) {
        return "image/jpeg";
    }

    return "text/plain";
}

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
}

void print_parsed_request(struct request *req)
{
    printf("REQUEST LINE:\n");
    printf("METHOD = %s\n", req->method);
    printf("PATH = %s\n", req->path);
    printf("VERSION = %s\n", req->version);
    printf("\n");

    printf("REQUEST HEADERS:\n");

    if (req->headers.content_type) {
        printf("Content-Type: %s\n", req->headers.content_type);
    }

    printf("\n");
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

void parse_request(struct request *req, const char *request_buffer)
{
    enum request_parsing_state state = PARSING_REQUEST_LINE;
    size_t buffer_index = 0;
    size_t start = 0;

    while (request_buffer[buffer_index] != '\0' && buffer_index < MAX_REQUEST_BYTES) {
        switch (state) {
        case PARSING_REQUEST_LINE: {
            if (buffer_index > 0 && request_buffer[buffer_index - 1] == '\r' &&
                request_buffer[buffer_index] == '\n') {
                char *copied_buffer = strndup(request_buffer + start, buffer_index - start);

                if (!copied_buffer) {
                    printf("Failed to copy buffer string\n");
                    return;
                }

                parse_request_line(req, copied_buffer);
                state = PARSING_REQUEST_HEADERS;

                free(copied_buffer);

                start = buffer_index + 1;
            }
        } break;

        case PARSING_REQUEST_HEADERS: {
            if (buffer_index > 0 && request_buffer[buffer_index - 1] == '\r' &&
                request_buffer[buffer_index] == '\n') {
                char *copied_buffer = strndup(request_buffer + start, buffer_index - start - 1);

                if (!copied_buffer) {
                    printf("Failed to copy buffer string\n");
                    return;
                }

                parse_request_header(&req->headers, copied_buffer);

                free(copied_buffer);

                start = buffer_index + 1;
            }
        }

        break;

        default: {
            printf("Unknown parsing state\n");
            return;
        }
        }

        buffer_index += 1;
    }
}

void send_response(int con_fd, char *path)
{
    if (strcmp(path, "/") == 0) {
        path = "/index.html";
    }

    char file_path[PATH_MAX];
    snprintf(file_path, sizeof(file_path), "web/%s", path + 1);

    char resolved_path[PATH_MAX];

    if (!realpath(file_path, resolved_path)) {
        write(con_fd, HTTP_404, strlen(HTTP_404));
        return;
    }

    char web_path[PATH_MAX];

    if (!realpath("web", web_path)) {
        write(con_fd, HTTP_500, strlen(HTTP_500));
        return;
    }

    if (strncmp(resolved_path, web_path, strlen(web_path)) != 0) {
        write(con_fd, HTTP_403, strlen(HTTP_403));
        return;
    }

    FILE *file_ptr = fopen(resolved_path, "rb");

    if (!file_ptr) {
        write(con_fd, HTTP_404, strlen(HTTP_404));
        perror("fopen failed");
        return;
    }

    char *ext = strrchr(path, '.');
    ext = ext ? ext + 1 : "html";
    const char *mime_type = get_mime_type(ext);

    char header[MAX_HEADER_SIZE];
    size_t header_size = snprintf(header, sizeof(header),
                                  "HTTP/1.1 200 OK\r\n"
                                  "Content-Type: %s\r\n"
                                  "\r\n",
                                  mime_type);

    write(con_fd, header, header_size);

    char html[FILE_BUFFER_SIZE];

    size_t bytes_read = 0;
    while ((bytes_read = fread(html, sizeof(char), FILE_BUFFER_SIZE, file_ptr)) > 0) {
        ssize_t total_written = 0;

        while (total_written < bytes_read) {
            ssize_t bytes_written = write(con_fd, html + total_written, bytes_read - total_written);

            if (bytes_written == -1) {
                perror("write failed");
                fclose(file_ptr);
                return;
            }

            total_written += bytes_written;
        }
    }

    if (ferror(file_ptr)) {
        perror("fread failed");
        fclose(file_ptr);
        return;
    }

    fclose(file_ptr);
}

int main(void)
{
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (sock_fd == -1) {
        perror("socket failled");
        return 1;
    }

    int opt = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0) {
        perror("setsockopt failled");
        return 1;
    }

    struct sockaddr_in server_addr = { .sin_family = AF_INET,
                                       .sin_addr.s_addr = INADDR_ANY,
                                       .sin_port = htons(PORT) };

    if (bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        perror("bind failled");
        return 1;
    }

    if (listen(sock_fd, 10) != 0) {
        perror("listen failled");
        return 1;
    }

    printf("Listening to port %d...\n", PORT);

    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int con_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &client_addr_len);

        if (con_fd == -1) {
            perror("accept failed");
            continue;
        }

        char request_buffer[MAX_REQUEST_BYTES];
        ssize_t bytes_read = read(con_fd, request_buffer, sizeof(request_buffer) - 1);

        if (bytes_read == -1) {
            perror("read failed");
            continue;
        }

        request_buffer[bytes_read] = '\0';

        struct request req = { 0 };
        parse_request(&req, request_buffer);

        print_parsed_request(&req);
        send_response(con_fd, req.path);

        free_request(&req);
        close(con_fd);
    }

    close(sock_fd);

    return 0;
}
