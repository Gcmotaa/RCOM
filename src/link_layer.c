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

// alarm globaçl variables
int alarmEnabled = FALSE;
int alarmCount = 0;

// Packet values
#define F 0x7E

#define A_T_COMMAND 0x03
#define A_R_COMMAND 0x01

#define C_SET 0x03
#define C_UA 0X07
#define C_RR0 0XAA
#define C_RR1 0XAB
#define C_REJ0 0X54
#define C_REJ1 0X55
#define C_DISC 0X0B
#define ESCAPE_OCTET 0x7d

typedef enum{
    INITIAL_S,
    RECEIVED_F,
    RECEIVED_A,
    RECEIVED_C,
    RECEIVED_BCC1,
    END_S
} S_States;

typedef struct{
    S_States state;
    unsigned char A;
    unsigned char C;
} recieving_S_sm;

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

// connection parameters
LinkLayer parameters;

//////////////////////////////////
/// FRAME RECIEVING STATE MACHINES  
//////////////////////////////////
void s_statemachine(recieving_S_sm *sm) {
    unsigned char byte;
    int bytes;
    bytes = readByteSerialPort(&byte);

    switch (sm->state)
    {
    case INITIAL_S:
        if(bytes == 1) {
            if(byte == F) sm->state = RECEIVED_F;
        }
        break;

    case RECEIVED_F:
        if(bytes == 1) {
            if(byte == A_T_COMMAND || byte == A_R_COMMAND) {
                sm->state = RECEIVED_A;
                sm->A = byte;
            }
            else sm->state = INITIAL_S;
        }
        break;

    case RECEIVED_A:
        if(bytes == 1) {
            if(byte == C_SET || byte == C_UA ||
                byte == C_RR0 || byte == C_RR1 ||
                byte == C_REJ0 || byte == C_REJ1 ||
                byte == C_DISC){
                sm->state = RECEIVED_C;
                sm->C = byte;
                }
        }
        break;

    case RECEIVED_C:
        if(bytes == 1) {
            if(byte == (sm->A ^ sm->C)) 
                sm->state = RECEIVED_BCC1;
        }
        break;

    case RECEIVED_BCC1:
        if(bytes == 1) {
            if(byte == F) sm->state = END_S;
        }
        break;
    case END_S:
        break;
    default:
        printf("state is NULL\n");
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
// UTILS
////////////////////////////////////////////////

//@return 0 on success, -1 otherwise
//@param max_tries: number of attempts before giving up
//@param timeout: time(microseconds) between attempts
int receive_S(unsigned char A, unsigned char C, int max_tries, int timeout) {
    recieving_S_sm sm;
    sm.state = INITIAL_S;
    S_States previous_state = INITIAL_S;

    int current_tries = 0;
    while (current_tries < max_tries) {
        s_statemachine(&sm);
        if(sm.state == RECEIVED_A && sm.A != A) {
            sm.state = INITIAL_S;
        }
        else if(sm.state == RECEIVED_C && sm.C != C) {
            sm.state = INITIAL_S;
        }
        else if(sm.state == END_S){
            return 0;
        }
        if(previous_state == sm.state) {
            ++current_tries;
            usleep(timeout);
        }
    }
    return -1;
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    parameters = connectionParameters;

    if(openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate) <0){
        return -1;
    }

    switch (connectionParameters.role)
    {
    case LlTx:
        unsigned char buf[5] = {F, A_T_COMMAND, C_SET, A_T_COMMAND ^ C_SET, F};

        //should i sleep before this?
        int bytes = writeBytesSerialPort(buf, 5);

        //wait a second before advancing
        sleep(1);

        //receive UA
        return receive_S(A_T_COMMAND, C_UA, 4, 250);

        break;
    case LlRx:
        
        receive_S(A_R_COMMAND, C_UA, 10, 500);

        unsigned char buf[5] = {F, A_T_COMMAND, C_UA, A_T_COMMAND ^ C_UA, F};

        int bytes = writeBytesSerialPort(buf, 5);
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

    // set the alarm function handler
    struct sigaction act = {0};
    act.sa_handler = &alarmHandler;
    if (sigaction(SIGALRM, &act, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }

    while (alarmCount < parameters.nRetransmissions)
    {
        if(writeBytesSerialPort(bytes, finalSize) != finalSize) return -1;
        if (alarmEnabled == FALSE)
        {
            alarm(parameters.timeout); // Set alarm to be triggered in timeout
            alarmEnabled = TRUE;
        }
    }

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    recieving_I_state state = INITIAL_I;
    unsigned char *header = malloc(2 * sizeof(unsigned char));
    int destuffing = FALSE;
    int index = 0;
    unsigned char expected_bcc2 = 0x0;
    unsigned char* reply = malloc(4 * sizeof(unsigned char));

    *(reply) = F;
    *(reply+1) = A_T_COMMAND;

    struct sigaction act = {0};
    act.sa_handler = &alarmHandler;
    if (sigaction(SIGALRM, &act, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }

    alarmEnabled = TRUE;
    alarm(parameters.timeout);

    while (state != END_I && alarmEnabled){
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
                    destuffing = TRUE; //the next byte will need to be reworked
                    continue;
                }
                
                if(destuffing == TRUE){
                    byte = byte || 0x20;
                    destuffing = FALSE;
                }

                *(packet + index) = byte; //put the byte in the packet

                //we do the xor with the prior one so the bcc2 is not included
                expected_bcc2 = expected_bcc2 ^ *(packet+index-1);

                index++;
            }
        }
    }

    if(state != END_I) return -1; //got out of the cycle because of timeout

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
    switch (parameters.role)
    {
    case LlTx:
        unsigned char disc[5] = {F, A_T_COMMAND, C_DISC, A_T_COMMAND ^ C_DISC, F};
        
        //send disc
        writeBytesSerialPort(disc, 5);

        usleep(500);

        //receive disc
        receive_S(A_R_COMMAND, C_DISC, 10, 500);

        unsigned char UA[5] = {F, A_R_COMMAND, C_UA, A_R_COMMAND ^ C_UA, F};
        //send UA
        writeBytesSerialPort(UA,5);

        closeSerialPort();
        break;
    case LlRx:
        //receive the disc
        receive_S(A_T_COMMAND, C_DISC, 10, 500);
  
        unsigned char dics[5] = {F, A_R_COMMAND, C_DISC, A_R_COMMAND ^ C_DISC, F};

        //send disc
        writeBytesSerialPort(disc,5);

        usleep(500);
        //receive UA
        receive_S(A_R_COMMAND, C_UA, 4, 250);
        closeSerialPort();
        break;
    default:
        printf("llclose error: role not defined\n");
        return -1;
        break;
    }

    return 0;
}
