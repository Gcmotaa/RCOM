// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

int alarmEnabled = FALSE;
int alarmCount = 0;

// Packet values
#define F 0x7E

#define A_T_COMMAND 0x03
#define T_R_COMMAND 0x01

#define C_SET 0x03
#define C_UA 0X07
#define C_RR 0XAA
#define C_REJ 0X54
#define C_DISC 0X0B
#define ESCAPE_OCTET 0x7d

typedef enum{
    INITIAL_S,
    RECEIVED_F,
    RECEIVED_A,
    RECEIVED_C,
    RECEIVED_BCC1,
    END_S

} recieving_S_state;

typedef enum {
    INITIAL_I,
    F_RECEIVED,
    A_RECEIVED,
    C_RECEIVED,
    BCC1_RECEIVED,
    RECEIVING_DATA,
    END_I
} recieving_I_state;

int Ns = 0;

//////////////////////////////////
/// FRAME RECIEVING STATE MACHINES  
//////////////////////////////////
int s_statemachine(recieving_S_state *state) {
    unsigned short tries_counter = 0;
    unsigned char byte;
    int bytes;

    switch (*state)
    {
    case INITIAL_S:
        bytes = readByteSerialPort(&byte);

        if(bytes == 1) {
            if(byte == F) *state = RECEIVED_F;
        }
        break;
    case RECEIVED_F:
        bytes = readByteSerialPort(&byte);

        if(bytes == 1) {
            if(byte == A_T_COMMAND) *state = RECEIVED_A;
            else *state = INITIAL_S;
        }
        break;
    case RECEIVED_A:

    //CONTINUE HERE
        bytes = readByteSerialPort(&byte);

        if(bytes == 1) {
            if(byte == F) *state = RECEIVED_F;
        }
        break;
    case RECEIVED_C:
        bytes = readByteSerialPort(&byte);

        if(bytes == 1) {
            if(byte == F) *state = RECEIVED_F;
        }
        break;
    case RECEIVED_BCC1:
        bytes = readByteSerialPort(&byte);

        if(bytes == 1) {
            if(byte == F) *state = RECEIVED_F;
        }
        break;
    case END_S:

        break;
    default:
        break;
    }
}

void updateRecievingIState(recieving_I_state *state, unsigned char byte, unsigned char* header){
    switch (*state)
    {
    case INITIAL_I:
        if (byte == F){
            *state = F_RECEIVED;
        }
        break;

    case F_RECEIVED:
        if (byte != F){
            *state = A_RECEIVED;
            *header = byte;
        }
        break;

    case A_RECEIVED:
        *state = C_RECEIVED;
        *(header+1) = byte;
        break;

    case C_RECEIVED:
        *state = BCC1_RECEIVED;
        break;

    case RECEIVING_DATA:
        if(byte == F){
            *state = END_I;
        }
        break;
    
    default:
        break;
    }
}

//////////////////////////////////////////////
/// ALARM HANDLER
//////////////////////////////////////////////
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    if(openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate) <0){
        return -1;
    }

    switch (connectionParameters.role)
    {
    case LlTx:
        unsigned char buf[5] = {F, A_T_COMMAND, C_SET, A_T_COMMAND ^ C_SET, F};

        int bytes = writeBytesSerialPort(buf, 5);

        sleep(1);

        recieving_S_state state = INITIAL_S;

        break;
    case LlRx:
        
        break;
    default:
        return -1;
        break;
    }

    return 0;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    unsigned char bcc2 = 0;
    unsigned char *bytes = malloc((bufSize*2 + 5) * sizeof(unsigned char));
    int finalSize = 4;

    *bytes = F;
    *(bytes+1) = A_T_COMMAND;
    *(bytes+2) = Ns << 7;
    *(bytes+3) = A_T_COMMAND ^ (Ns << 7);

    for(int i = 0; i < bufSize; i++){
        bcc2 = bcc2 ^ *(buf+i);

        if(*(buf+i) == 0x7e || *(buf+i) == 0x7d){ //if we need byte stuffing
            *(bytes+finalSize) = 0x7d;
            *(bytes+finalSize+1) = *(buf+i) ^ 0x20;

            finalSize += 2;
            continue;
        }

        *(bytes+finalSize) = *(buf+i);
        finalSize++;
    }

    *(bytes+finalSize) = bcc2;
    *(bytes+finalSize+1) = F;

    finalSize += 2;

    struct sigaction act = {0};
    act.sa_handler = &alarmHandler;
    if (sigaction(SIGALRM, &act, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }

    if(writeBytesSerialPort(bytes, finalSize) != finalSize) return -1;

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    recieving_I_state state = INITIAL_I;
    unsigned char *header = malloc(2 * sizeof(unsigned char));
    int destuffing = 0;
    int index = 0;
    unsigned char expected_bcc2 = 0x0;
    unsigned char* reply = malloc(4 * sizeof(unsigned char));

    *(reply) = F;
    *(reply+1) = A_T_COMMAND;

    while (state != END_I){
        unsigned char byte;
        int nBytes = readByteSerialPort(&byte);

        if(nBytes > 0){
            updateRecievingIState(&state, byte, header);

            if(state == BCC1_RECEIVED && byte == (*header ^ *(header+1))){ //check if bcc1 is correct
                state = RECEIVING_DATA;
            }
            else if (state == BCC1_RECEIVED){ //if bcc1 is incorrect
                state = INITIAL_I;
            }

            if(state == RECEIVING_DATA){
                if(byte == ESCAPE_OCTET){
                    destuffing = 1; //the next byte will need to be reworked
                    continue;
                }
                
                if(destuffing == 1){
                    byte = byte || 0x20;
                    destuffing = 0;
                }

                *(packet + index) = byte; //put the byte in the packet

                //we do the xor with the prior one so the bcc2 is not included
                expected_bcc2 = expected_bcc2 ^ *(packet+index-1);

                index++;
            }
        }
    }

    //we have read all the bytes, we need to confirm bcc2
    if(*(packet+index-1) == expected_bcc2){

        if(*(header+1) == Ns << 7){ //check if the ns we received are the one we are expecting
            Ns = (Ns == 1 ? 0 : 1); //change ns

            *(reply+2) = C_RR || Ns;
            *(reply+3) = *(reply+1) ^ *(reply+2);

            writeBytesSerialPort(reply, 4); //reply "send me new frame"
            return index - 1; //minus 1 to not count the bcc2
        }
        else{ //it is not the one we are expecting
            return 0;
        }
       
    }

    if(*(header+1) == Ns << 7){ //check if the ns we received are the one we are expecting
        *(reply+2) = C_REJ || Ns;
        *(reply+3) = *(reply+1) ^ *(reply+2);

        writeBytesSerialPort(reply, 4); //send REJ(Ns)
        return -1;
    }
    else{ //it is not the one we are expecting
        *(reply+2) = C_RR || Ns;
        *(reply+3) = *(reply+1) ^ *(reply+2);

        writeBytesSerialPort(reply, 4); //reply "send me new frame"
        return 0;
    }
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose()
{
    // TODO: Implement this function

    return 0;
}
