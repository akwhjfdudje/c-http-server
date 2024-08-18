#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <regex.h>
#define PORT "3490"
#define BACKLOG 10
#define HLIMIT 50
#define BUFFER_SIZE 4096
#define MAX_HEADERS 100
#define MAX_HEADER_SIZE 256

// define request struct
struct request{
	char method[8];
	char version[16];
	char body[256];
	char *headers[MAX_HEADERS];
	char url[256];
};

// define response struct
struct response{
	char *headers[MAX_HEADERS];
	char status[32];
	char body[256];
	int code;
	
};

struct response* generateResp(int code, char *statusMessage, char *body, char *customHeaders[], int customHeaderCount) {
    struct response *resp = (struct response *)malloc(sizeof(struct response));
    if (resp == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    // Set the code and status message
    resp->code = code;
    strncpy(resp->status, statusMessage, sizeof(resp->status) - 1);
    resp->status[sizeof(resp->status) - 1] = '\0';

    // Set the body
    strncpy(resp->body, body, sizeof(resp->body) - 1);
    resp->body[sizeof(resp->body) - 1] = '\0';

    // Initialize headers
    resp->headers[0] = (char *)malloc(MAX_HEADER_SIZE);
	
    // Add custom headers
	int headerIndex = 0;
    for (int i = 0; i < customHeaderCount && headerIndex < MAX_HEADERS; i++) {
        resp->headers[headerIndex] = (char *)malloc(MAX_HEADER_SIZE);
        if (resp->headers[headerIndex] == NULL) {
            fprintf(stderr, "Memory allocation for headers failed\n");
            for (int j = 0; j < headerIndex; j++) {
                free(resp->headers[j]);
            }
            free(resp);
            exit(EXIT_FAILURE);
        }
        strncpy(resp->headers[headerIndex++], customHeaders[i], MAX_HEADER_SIZE - 1);
        resp->headers[headerIndex - 1][MAX_HEADER_SIZE - 1] = '\0';
    }

    return resp; // Return the pointer to the response struct
}

struct request* parseReq(char *req) {
	int header_count = 0;

	struct request *r = malloc(sizeof(struct request));
	if (r == NULL) {
		fprintf(stderr, "Memory allocation failed\n");
		exit(1);
	}
	//Parse request line
	sscanf(req, "%s %s %s", r->method, r->url, r->version);
	// Parse headers
    const char *header_start = strstr(req, "\r\n") + 2; // Move past the request line
    const char *header_end;
    while ((header_end = strstr(header_start, "\r\n")) != NULL && header_end != header_start) {
        if (header_count >= MAX_HEADERS) {
            fprintf(stderr, "Exceeded maximum number of headers\n");
            break;
        }

        // Allocate memory for the header	
        r->headers[header_count] = (char *)malloc(MAX_HEADER_SIZE);
        if (r->headers[header_count] == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            exit(EXIT_FAILURE);
        }
	

        // Copy the header into the allocated memory
        strncpy(r->headers[header_count], header_start, header_end - header_start);
        r->headers[header_count][header_end - header_start] = '\0'; // Null-terminate the header string

        // Move to the next header
        header_start = header_end + 2; // Move past the current header
        header_count++;
    }

	return r;
}

char* responseToString(struct response *resp) {
    // Calculate the size needed for the response string
    size_t totalSize = 0;
    totalSize += snprintf(NULL, 0, "HTTP/1.1 %d %s\r\n", resp->code, resp->status); // Status line

    for (int i = 0; i < MAX_HEADERS; i++) {
        if (resp->headers[i] != NULL) {
            totalSize += snprintf(NULL, 0, "%s\r\n", resp->headers[i]); // Headers
        }
    }

    totalSize += snprintf(NULL, 0, "\r\n"); // End of headers
    totalSize += strlen(resp->body); // Body

    // Allocate memory for the final response string
    char *responseString = (char *)malloc(totalSize + 1);
    if (responseString == NULL) {
        fprintf(stderr, "Memory allocation for response string failed\n");
        exit(EXIT_FAILURE);
    }

    // Construct the response string
    char *ptr = responseString;
    ptr += sprintf(ptr, "HTTP/2 %d %s\r\n", resp->code, resp->status); // Status line

    for (int i = 0; i < MAX_HEADERS; i++) {
        if (resp->headers[i] != NULL) {
            ptr += sprintf(ptr, "%s\r\n", resp->headers[i]); // Headers
        }
    }

    ptr += sprintf(ptr, "\r\n"); // End of headers
    strcpy(ptr, resp->body); // Body

    return responseString; // Return the response string
}
int main() {
	int status;
	struct addrinfo hints;
	struct addrinfo *servinfo;
	struct sockaddr_storage their_addr;
	socklen_t addr_size;
	int s;
	int bstatus;
	int lstatus;
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;

	if ((status = getaddrinfo("127.0.0.1", PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
		exit(1);
	}

	//starting the socket
	s = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	
	//binding to an ip	
	if ((bstatus = bind(s, servinfo->ai_addr, servinfo->ai_addrlen)) != 0) {
		fprintf(stderr, "bind error: %s\n", gai_strerror(bstatus));
		exit(1);
	}	
	if ((lstatus = listen(s, BACKLOG)) != 0) {
		fprintf(stderr, "listen error: %s\n", gai_strerror(lstatus));
		exit(1);
	}	
	addr_size = sizeof their_addr;
	while (1){
		int a = accept(s, (struct sockaddr *)&their_addr, &addr_size);
		char *body = "<html><body><h1>this page exists</h1></body></html>";
		char *customHeaders[] = {
			"Host: whatever:8000",
			"Server: randomsequenceofchars",
			"Content-type: text/html",
			"Connection: close"};
		int customHeaderCount = sizeof(customHeaders) / sizeof(customHeaders[0]);

		struct response *resp = generateResp(200, "Random Status Message", body, customHeaders, customHeaderCount);
		char *msg = responseToString(resp);
		int len, bytes_sent;
		len = strlen(msg);
		bytes_sent = send(a, msg, len, 0);
		char* buf = (char*)malloc(1024);
		int bytes_recv;
		if ((bytes_recv = recv(a, buf, 2048, 0)) == -1) {
			fprintf(stderr, "recv error: %s\n", gai_strerror(bytes_recv));
			exit(1);
		}
		char* req = malloc(sizeof(struct request));
		strcpy(req, buf);
		free(buf);	
		struct request *r = parseReq(req);
		if (r==NULL){
			printf("Failed to parse request, exiting...\n");
			return 1;
		}
		printf("%s %s %s\n", r->method, r->url, r->version);
		int shutd;
		if ((shutd = shutdown(a, 2)) != 0) {
			fprintf(stderr, "shutdown error: %s\n", gai_strerror(shutd));
			exit(1);
		}
	}
		freeaddrinfo(servinfo);
}

