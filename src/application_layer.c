// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define FRAME_SIZE 100

//@return 0 on success, -1 otherwise
int applicationReciever(){
    FILE *fp = NULL;
    unsigned int file_size = 0;
    unsigned int bytes_written = 0;

    while (TRUE)
    {
        unsigned char packet[MAX_PAYLOAD_SIZE];
        int bytes = llread(packet);
        if (bytes == -1){
            fprintf(stderr, "ERROR:llread failed\n");
            continue;
        } else if(bytes == 0){
            printf("0 bytes read\n");
        }

        unsigned char *ptr = packet;
        printf("%x %x %x\n", *ptr, *(ptr+1), *(ptr+2));
        switch (*ptr) // first byte is C
        {
        case 1: //START
            char filename[256] = {0};
            ++ptr;
            // We loop twice: once for the size of the file and once for the name
            for (int i = 0; i < 2; ++i) { 
                unsigned char T = *(ptr++);
                unsigned char L = *(ptr++);
                unsigned char *value = malloc(L * sizeof(unsigned char));
                memcpy(value, ptr, L);
                ptr += L;
                if (T == 0) { // file size
                    file_size = 0;
                    for (int j = 0; j < L; j++) {
                        file_size |= ((unsigned int)value[j]) << (8 * j);
                    }
                }
                else if (T == 1) { // filename
                    memcpy(filename, value, L);
                    filename[L] = '\0';
                }
                else{
                    fprintf(stderr, "ERROR:Undefined T = %x\n", T);
                    free(value);
                    continue;
                }
                free(value);
            }

            // Create the file
            fp = fopen(filename, "wb");
            if (!fp) {
                perror("ERROR:Could not create file\n");
                return -1;
            }
            break;
        case 2: //DATA
            ++ptr;
            if (fp == NULL) {
                fprintf(stderr, "ERROR:received DATA packet before START\n");
                return -1;
            }

            unsigned char L1 = *ptr++;
            unsigned char L2 = *ptr++;
            unsigned int data_length = L1 + (L2 << 8);

            if(data_length > MAX_PAYLOAD_SIZE) {
                fprintf(stderr, "ERROR:Received more data than supported. received %i\n", data_length);
            }
            // Write data to file
            size_t written = fwrite(ptr, 1, data_length, fp);
            if (written != data_length) {
                fprintf(stderr, "ERROR:Failed to write full data block to file");
                return -1;
            }

            bytes_written += written;
            break;

        case 3: //END
            if(fp != NULL) fclose(fp);
            if (bytes_written != file_size)
                fprintf(stderr, "ERROR:File incomplete, expected %u bytes, got %u\n", file_size, bytes_written);
            return 0;
            break;
        
        default:
            fprintf(stderr, "ERROR:Undefined C = %i\n", *ptr);
            break;
        }
    }
}

void applicationTransmitter(const char *filename){
    //open the file in read mode
    FILE* file = fopen(filename, "r");
    if(file == NULL){
        printf("ERROR: Could not open file.\n");
        return;
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);

    if(filesize == -1L){
        printf("ERROR: ftell failed.\n");
        return;
    }

    rewind(file);

    unsigned char *controlPacket = malloc(9 + strlen(filename));

    *(controlPacket) = 1;
    *(controlPacket + 1) = 0;
    *(controlPacket + 2) = 4;
    *(controlPacket + 3) = filesize & 0xff;
    *(controlPacket + 4) = (filesize << 8) & 0xff;
    *(controlPacket + 5) = (filesize << 16) & 0xff;
    *(controlPacket + 6) = (filesize << 24) & 0xff;
    *(controlPacket + 7) = 1;
    *(controlPacket + 8) = strlen(filename);
    *(controlPacket + 9) = *filename;

    if(llwrite(controlPacket, 9 + strlen(filename)) != 9 + strlen(filename)){
        printf("ERROR: Error sending start control packet.\n");

        if(fclose(file) == -1){
            printf("ERROR: Error closing the file.\n");
        }
        return;
    }
    printf("START control packet sent\n");
    free(controlPacket);

    unsigned char dataPacket[FRAME_SIZE + 3];

    dataPacket[0] = 2;
    dataPacket[1] = FRAME_SIZE & 0xff;
    dataPacket[2] = (FRAME_SIZE << 8) & 0xff;

    while(fread(&dataPacket[3], 1, FRAME_SIZE, file) > 0){
        if(llwrite(dataPacket, FRAME_SIZE+3) != FRAME_SIZE+3){
            printf("ERROR: Error writing a data packet.\n");

            if(fclose(file) == -1){
                printf("ERROR: Error closing the file.\n");
            }

            return;
        }
    }

    unsigned char controlEnd[1];
    controlEnd[0] = 3;

    if(llwrite(controlEnd, 1) != 1){
        printf("ERROR: Error sending the end control packet.\n");
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

    printf("Ports opened with success!\n");
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
