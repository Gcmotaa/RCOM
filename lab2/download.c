#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>

#define SERVER_PORT 80

int main(int argc, char *argv[]) {
    struct hostent *h;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server IP address>\n", argv[0]);
        exit(-1);
    }

    char *url = argv[1];

    // Check prefix
    const char *prefix = "ftp://";
    if (strncmp(url, prefix, strlen(prefix)) != 0) {
        fprintf(stderr, "Invalid URL: must start with ftp://\n");
        return 1;
    }

    url += strlen(prefix);  // move pointer past ftp://

    char *user = NULL, *password = NULL, *host = NULL, *url_path = NULL;

    // Check if credentials are present
    char *at = strchr(url, '@');
    if (at) {
        // Credentials exist
        *at = '\0';  // split credentials from the rest

        char *colon = strchr(url, ':');
        if (!colon) {
            fprintf(stderr, "Invalid URL: user:password format required\n");
            return 1;
        }

        *colon = '\0';
        user = url;
        password = colon + 1;

        host = at + 1;  // part after '@'
    } else {
        // No credentials
        user = NULL;
        password = NULL;
        host = url;
    }

    // Extract host and path
    char *slash = strchr(host, '/');
    if (!slash) {
        fprintf(stderr, "Invalid URL: missing /path\n");
        return 1;
    }

    *slash = '\0';
    url_path = slash + 1;

    printf("Parsed values:\n");
    printf("User:      %s\n", user ? user : "(none)");
    printf("Password:  %s\n", password ? password : "(none)");
    printf("Host:      %s\n", host);
    printf("URL Path:  %s\n", url_path);
/**
 * The struct hostent (host entry) with its terms documented

    struct hostent {
        char *h_name;    // Official name of the host.
        char **h_aliases;    // A NULL-terminated array of alternate names for the host.
        int h_addrtype;    // The type of address being returned; usually AF_INET.
        int h_length;    // The length of the address in bytes.
        char **h_addr_list;    // A zero-terminated array of network addresses for the host.
        // Host addresses are in Network Byte Order.
    };

    #define h_addr h_addr_list[0]	The first address in h_addr_list.
*/
    if ((h = gethostbyname(host)) == NULL) {
        herror("gethostbyname()");
        exit(-1);
    }

    int sockfd;
    struct sockaddr_in server_addr;
    char buf[] = "Mensagem de teste na travessia da pilha TCP/IP\n";
    size_t bytes;

    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    printf("IP Address: %s\n", inet_ntoa(*((struct in_addr *) h->h_addr)));
    server_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *) h->h_addr)));    /*32 bit Internet address network byte ordered*/
    printf("IP Address binary: %d\n", server_addr.sin_addr.s_addr);

    server_addr.sin_port = htons(SERVER_PORT);        /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }
    /*send a string to the server*/
    bytes = write(sockfd, buf, strlen(buf));
    if (bytes > 0)
        printf("Bytes escritos %ld\n", bytes);
    else {
        perror("write()");
        exit(-1);
    }

    if (close(sockfd)<0) {
        perror("close()");
        exit(-1);
    }

    return 0;
}
