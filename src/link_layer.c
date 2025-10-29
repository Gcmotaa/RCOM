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

// alarm global variables
int alarmEnabled = FALSE;
int alarmCount = 0;

// Packet values
#define F 0x7E

#define A_T_COMMAND 0x03
#define A_R_COMMAND 0x01

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

    if(!sm) return;
    if(bytes != 1)return;
    switch (sm->state)
    {
    case INITIAL_S:
        if(byte == F) sm->state = RECEIVED_F;
        break;

    case RECEIVED_F:
        if(byte == A_T_COMMAND || byte == A_R_COMMAND) {
            sm->state = RECEIVED_A;
            sm->A = byte;
        }
        else sm->state = INITIAL_S;
        break;

    case RECEIVED_A:
        if(byte == C_SET || byte == C_UA ||
            byte == C_RR || byte == (C_RR | 0x01) ||
            byte == C_REJ || byte == (C_REJ | 0x01) ||
            byte == C_DISC){
            sm->state = RECEIVED_C;
            sm->C = byte;
            }
        else sm->state = INITIAL_S;
        break;

    case RECEIVED_C:
        if(byte == (sm->A ^ sm->C)) 
            sm->state = RECEIVED_BCC1;
        else sm->state = INITIAL_S;
        break;

    case RECEIVED_BCC1:
        if(byte == F) sm->state = END_S;
        else sm->state = INITIAL_S;
        break;
    case END_S:
        break;
    default:
        sm->state = INITIAL_S;
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

int setup_alarm_handler(void) {
    struct sigaction act = {0};
    act.sa_handler = &alarmHandler;
    if (sigaction(SIGALRM, &act, NULL) == -1) 
    {
        perror("sigaction");
        return -1;
    }
    return 0;
}
////////////////////////////////////////////////
// UTILS
////////////////////////////////////////////////

//sets and alarm for timeout seconds. alarm handler must be defined before calling this function
//@return 0 on success, -1 otherwise
//@param A: Expected A byte
//@param C: Expected C byte
//@param max_tries: number of attempts before giving up
//@param timeout: time(microseconds) between attempts
int receive_S(unsigned char A, unsigned char C, int timeout) {
    recieving_S_sm sm;
    sm.state = INITIAL_S;
    sm.A = 0;
    sm.C = 0;

    alarmEnabled = TRUE;
    alarm(timeout);

    while(alarmEnabled && sm.state != END_S) {
        s_statemachine(&sm);
        if(sm.state == RECEIVED_A && sm.A != A) {
            sm.state = INITIAL_S;
        }
        else if(sm.state == RECEIVED_C && sm.C != C) {
            sm.state = INITIAL_S;
        }  
    }

    if(sm.state == END_S) {
        alarmEnabled = FALSE;
        return 0;
    }
    return -1;
}

//@return 0 on success, -1 otherwise
//@param frame: frame to send
//@param frame_len: length of frame to send
//@param A: Expected A byte
//@param C: Expected C byte
//@param max_tries: number of attempts before giving up
//@param timeout: time(microseconds) between attempts
int send_frame_wait_response(unsigned char *frame, int frame_len, unsigned char A, unsigned char C, int max_tries, int timeout) {

    for (int tries = 0; tries < max_tries; tries++) {

        if (writeBytesSerialPort(frame, frame_len) != frame_len)
            return -1;

        if(receive_S(A, C, timeout) == 0) return 0;

    }

    printf("Failed after %i attempts\n", max_tries);
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

    if(setup_alarm_handler() != 0){
        return -1;
    } 

    switch (connectionParameters.role)
    {
    case LlTx:
        unsigned char set[5] = {F, A_T_COMMAND, C_SET, A_T_COMMAND ^ C_SET, F};

        return send_frame_wait_response(set, 5, A_T_COMMAND, C_UA, connectionParameters.nRetransmissions, connectionParameters.timeout);

        break;
    case LlRx:
        if(receive_S(A_T_COMMAND, C_SET, connectionParameters.timeout) != 0) return -1;

        unsigned char ua[5] = {F, A_T_COMMAND, C_UA, A_T_COMMAND ^ C_UA, F};

        //send UA
        if(writeBytesSerialPort(ua, 5) != 5) return -1;
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
    int nextNs = (Ns == 0 ? 1 : 0);

    *bytes = F;
    *(bytes+1) = A_T_COMMAND;
    *(bytes+2) = Ns << 7;
    *(bytes+3) = A_T_COMMAND ^ (Ns << 7);

    for(int i = 0; i < bufSize; i++){
        unsigned char b = buf[i];
        bcc2 = bcc2 ^ b;

        if(b == F || b == ESCAPE_OCTET){ //if we need byte stuffing
            *(bytes+finalSize) = 0x7d;
            *(bytes+finalSize+1) = b ^ 0x20;

            finalSize += 2;
            continue;
        }

        *(bytes+finalSize) = *(buf+i);
        finalSize++;
    }

    //stuffing of bcc2
    if (bcc2 == F || bcc2 == ESCAPE_OCTET){
        *(bytes+finalSize) = ESCAPE_OCTET;
        *(bytes+finalSize+1) = bcc2 ^ 0x20;

        finalSize += 2;
    }
    else{
        *(bytes+finalSize) = bcc2;
        finalSize++;
    }

    *(bytes+finalSize) = F;

    finalSize++;

    // set the alarm function handler
    struct sigaction act = {0};
    act.sa_handler = &alarmHandler;
    if (sigaction(SIGALRM, &act, NULL) == -1)
    {
        perror("sigaction");
        free(bytes);
        return -1;
    }

    alarmCount = 0;

    while (alarmCount < parameters.nRetransmissions)
    {
        if(writeBytesSerialPort(bytes, finalSize) != finalSize) {
            free(bytes);
            fprintf(stderr, "LLWRITE ERROR: write bytes call failed\n");
            return -1;
        }

        recieving_S_sm sm;
        sm.state = INITIAL_S;

        while (alarmEnabled && sm.state != END_S){
            s_statemachine(&sm);
        }

        if (sm.C == (C_RR | nextNs)){ //the frame was correctly recieved
            Ns = nextNs; //will send a new frame with a new ns
            free(bytes);
            return bufSize;
        }
        else printf("resending frame\n");

        //reset the alarm, for if the recieved was C_REJ or C_RR of current Ns, so that the alarm resets
        alarm(parameters.timeout);
        alarmEnabled = TRUE;

    }
    free(bytes);
    fprintf(stderr, "LLWRITE ERROR: Retransmissions exceeded\n");
    return -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    if(packet == NULL) return -1;
    
    recieving_I_state state = INITIAL_I;
    unsigned char header[2]; // header[0] = A, header[1] = C
    int destuffing = FALSE;
    int index = 0;
    unsigned char expected_bcc2 = 0x0;
    unsigned char reply[5];

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
                continue;
            }
            else if (state == BCC1_RECEIVED){ //if bcc1 is incorrect
                state = INITIAL_I;
                continue;
            }

            if(state == RECEIVING_DATA){
                if(byte == ESCAPE_OCTET){
                    destuffing = TRUE; //the next byte will need to be reworked
                    continue;
                }
                
                if(destuffing == TRUE){
                    byte = byte ^ 0x20;
                    destuffing = FALSE;
                }

                if(index > MAX_PAYLOAD_SIZE) {
                    fprintf(stderr, "LLREAD ERROR: index exceeded max payload size\n");
                    return -1;
                }
                *(packet + index) = byte; //put the byte in the packet

                //we do the xor with the prior one so the bcc2 is not included
                if(index > 0)
                    expected_bcc2 = expected_bcc2 ^ *(packet+index-1);

                index++;
            }
        }
    }

    if(state != END_I) {
        fprintf(stderr, "LLREAD ERROR: timeout\n");
        return -1; //got out of the cycle because of timeout
    }

    //we have read all the bytes, we need to confirm bcc2
    if(*(packet+index-1) == expected_bcc2){

        if(*(header+1) == (Ns << 7)){ //check if the ns we received are the one we are expecting
            Ns = (Ns == 1 ? 0 : 1); //change ns

            *(reply+2) = C_RR | Ns;
            *(reply+3) = *(reply+1) ^ *(reply+2);
            *(reply+4) = F;

            writeBytesSerialPort(reply, 5); //reply "send me new frame"
            return index - 1; //minus 1 to not count the bcc2
        }
        else{ //it is not the one we are expecting
            return 0;
        }
       
    }

    if(*(header+1) == (Ns << 7)){ //check if the ns we received are the one we are expecting
        *(reply+2) = C_REJ | Ns;
        *(reply+3) = *(reply+1) ^ *(reply+2);
        *(reply+4) = F;

        writeBytesSerialPort(reply, 5); //send REJ(Ns)
        fprintf(stderr, "LLREAD ERROR: wrong NS\n");
        printf("C = %x, ns = %x\n", header[1], Ns);
        return -1;
    }
    else{ //it is not the one we are expecting
        *(reply+2) = C_RR | Ns;
        *(reply+3) = *(reply+1) ^ *(reply+2);
        *(reply+4) = F;

        writeBytesSerialPort(reply, 5); //reply "send me new frame"
        return 0;
    }
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose()
{
    if(setup_alarm_handler() != 0){
        return -1;
    } 
    switch (parameters.role)
    {
    case LlTx:
        unsigned char discT[5] = {F, A_T_COMMAND, C_DISC, A_T_COMMAND ^ C_DISC, F};
        
        //send and receive DISC
        if(send_frame_wait_response(discT, 5, A_R_COMMAND, C_DISC, parameters.nRetransmissions, parameters.timeout) != 0) return -1;

        unsigned char UA[5] = {F, A_R_COMMAND, C_UA, A_R_COMMAND ^ C_UA, F};
        //send UA
        if(writeBytesSerialPort(UA,5) != 5 ) return -1;

        closeSerialPort();
        break;
    case LlRx:
        //receive the disc
        if(receive_S(A_T_COMMAND, C_DISC, parameters.timeout) != 0) return -1;
  
        unsigned char discR[5] = {F, A_R_COMMAND, C_DISC, A_R_COMMAND ^ C_DISC, F};

        //send DISC receive UA
        if(send_frame_wait_response(discR, 5, A_R_COMMAND, C_UA, parameters.nRetransmissions, parameters.timeout) != 0) return -1;
        closeSerialPort();
        break;
    default:
        printf("llclose error: role not defined\n");
        return -1;
        break;
    }

    return 0;
}
