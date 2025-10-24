// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

void applicationReciever(){
    //still have to read the filename
    /*
    //open the file in read mode
    FILE* file = fopen(filename, "w");
    if(file == NULL){
        printf("ERROR: Could not open file.\n");
        return;
    }

    //close the file
    if(fclose(file) == -1){
        printf("ERROR: Error closing the file.\n");
    }*/
}

void applicationTransmitter(const char *filename){
    //open the file in read mode
    FILE* file = fopen(filename, "r");
    if(file == NULL){
        printf("ERROR: Could not open file.\n");
        return;
    }

    struct stat st;

    if (stat(filename, &st) != 0){
        printf("ERROR: Error getting file information.\n");
    }

    unsigned char *controlPacket = malloc(9 + strlen(filename));

    *(controlPacket) = 1;
    *(controlPacket + 1) = 0;
    *(controlPacket + 2) = 4;
    *(controlPacket + 3) = st.st_size && 0xff;
    *(controlPacket + 4) = (st.st_size << 8) && 0xff;
    *(controlPacket + 5) = (st.st_size << 16) && 0xff;
    *(controlPacket + 6) = (st.st_size << 24) && 0xff;
    *(controlPacket + 7) = 1;
    *(controlPacket + 8) = strlen(filename);
    *(controlPacket + 9) = filename;

    if(llwrite(controlPacket, 9 + strlen(filename)) != 9 + strlen(filename)){
        printf("ERROR: Error sending start control packet.\n");
        return;
    }

    free(controlPacket);


    //close the file
    if(fclose(file) == -1){
        printf("ERROR: Error closing the file.\n");
    }
}

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer ll;

    strcpy(ll.serialPort, serialPort);
    ll.role = (strcmp(role, "tx") == 0 ? LlTx : LlRx);
    ll.baudRate = baudRate;
    ll.nRetransmissions = nTries;
    ll.timeout = timeout;

    if(llopen(ll) == -1){
        printf("ERROR: Could not open serial port.\n");
        return;
    } //open comunication

    if(ll.role == LlRx){
        applicationReciever();
    }
    else{
        applicationTransmitter(filename);
    }

    if(llclose() == -1){
        printf("ERROR: Could not close serial port.\n");
    }
}
