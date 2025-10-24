// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

void applicationReciever(){
    while (TRUE)
    {
        //int bytes = llread();
    }
    
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
