#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>

#define SERVER_PORT 21
#define BUFFER_SIZE 2048 //temporary fix, need to add loop to get reply

int get_reply(int socket, char *buffer, int buffer_size) {
    int bytes = read(socket, buffer, buffer_size - 1);
    if (bytes < 0) {
        perror("read");
        exit(1);
    }
    buffer[bytes] = '\0';
    printf("RECEIVED: %s", buffer);
    return bytes;
}

int send_command(int socket, char *cmd) {
    printf("SENT: %s", cmd);
    if(write(socket, cmd, strlen(cmd)) < 0) {
        perror("write");
        exit(1);
    }
}
int main(int argc, char *argv[]) {
    struct hostent *h;
    int a1, a2, a3, a4, p1, p2, data_port;       //PASV Information

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

    char *user = NULL, *password = NULL, *host = NULL;
    char url_path[200];

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
        user = "anonymous";
        password = "anonymous";
        host = url;
    }

    // Extract host and path
    char *slash = strchr(host, '/');
    if (!slash) {
        fprintf(stderr, "Invalid URL: missing /path\n");
        return 1;
    }

    strcpy(url_path, slash);
    *slash = '\0';


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

    int control_socket;
    struct sockaddr_in server_addr;
    char buf[BUFFER_SIZE];
    size_t bytes;

    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    printf("IP Address: %s\n", inet_ntoa(*((struct in_addr *) h->h_addr)));
    server_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *) h->h_addr)));    /*32 bit Internet address network byte ordered*/
    printf("IP Address binary: %u\n", server_addr.sin_addr.s_addr);

    server_addr.sin_port = htons(SERVER_PORT);        /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((control_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
    if (connect(control_socket,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }

    get_reply(control_socket, buf, BUFFER_SIZE);

    snprintf(buf, sizeof(buf), "USER %s\r\n", user);
    send_command(control_socket, buf);
    get_reply(control_socket, buf, BUFFER_SIZE);

    snprintf(buf, sizeof(buf), "PASS %s\r\n", password);
    send_command(control_socket, buf);
    get_reply(control_socket, buf, BUFFER_SIZE);

    send_command(control_socket, "PASV\r\n");
    get_reply(control_socket, buf, BUFFER_SIZE);

    if(sscanf(buf, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &a1,&a2,&a3,&a4,&p1,&p2) < 0){
        perror("sscanf()");
        exit(-1);
    }
    data_port = (p1 * 256) + p2;

    char data_ip[64];
    snprintf(data_ip, sizeof(data_ip), "%d.%d.%d.%d", a1,a2,a3,a4);

    struct sockaddr_in dataServAddr;
    /*server address handling*/
    bzero((char *) &dataServAddr, sizeof(dataServAddr));
    dataServAddr.sin_family = AF_INET;
    dataServAddr.sin_addr.s_addr = inet_addr(data_ip);    /*32 bit Internet address network byte ordered*/

    dataServAddr.sin_port = htons(data_port);        /*server TCP port must be network byte ordered */
    int data_socket;
    if ((data_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }

    /*connect to the server*/
    if (connect(data_socket,
                (struct sockaddr *) &dataServAddr,
                sizeof(dataServAddr)) < 0) {
        perror("connect()");
        exit(-1);
    }
    snprintf(buf, sizeof(buf), "RETR %s\r\n", url_path);
    send_command(control_socket, buf);
    get_reply(control_socket, buf, BUFFER_SIZE);

    char receive_buf[BUFFER_SIZE];

    char *filename = strrchr(url_path, '/');
    if (filename)
        filename++;  // move past '/'
    else
        filename = url_path;  // no '/' found, whole thing is the filename


    FILE *fp = fopen(filename, "wb");  
    if(!fp) {
        perror("fopen()");
        exit(-1);
    }
    do
    {
        bytes = read(data_socket, receive_buf, BUFFER_SIZE - 1);
        if(bytes != -1) {
            fwrite(receive_buf, sizeof(char), bytes, fp);
        }
    } while (bytes > 0);
    
    send_command(control_socket, "QUIT \r\n");

    if (close(control_socket)<0) {
        perror("close()");
        exit(-1);
    }

    if (close(data_socket)<0) {
        perror("close()");
        exit(-1);
    }

    return 0;
}
