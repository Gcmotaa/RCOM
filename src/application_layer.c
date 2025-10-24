// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

#include <stdio.h>
#include <string.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer ll;

    strcpy(ll.serialPort, serialPort);
    ll.role = (strcmp(role, "tx") == 0 ? LlTx : LlRx);
    ll.baudRate = baudRate;
    ll.nRetransmissions = nTries;
    ll.timeout = timeout;

    //open the file
    FILE* readFile = fopen(filename, "r");
    if(readFile == NULL){
        printf("ERROR: Could not open file.");
        return;
    }

    if(llopen(ll) == -1){
        printf("ERROR: Could not open serial port.");
        return;
    } //open comunication
}
