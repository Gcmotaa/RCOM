// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define F 0x7E

#define A_T_COMMAND 0x03
#define T_R_COMMAND 0x01

#define C_SET 0x03
#define C_UA 0X07
#define C_RR 0XAA
#define C_REJ 0X54
#define C_DISC 0X0B
#define C_IF0 0x00
#define C_IF1 0x80
#define ESCAPE_OCTET 0x7d

typedef enum{
    INITIAL,
    RECEIVED_F,
    RECEIVED_A,
    RECEIVED_C,
    RECEIVED_BCC1,
    END

} recieving_S_state;

typedef enum {
    INITIAL,
    F_RECEIVED,
    A_RECEIVED,
    C_RECEIVED,
    BCC1_RECEIVED,
    RECEIVING_DATA,
    END
} recieving_I_state;

int Ns = 0;

int s_statemachine(recieving_S_state *state) {
    unsigned short tries_counter = 0;
    switch (*state)
    {
    case INITIAL:
        unsigned char byte;
        int bytes = readByteSerialPort(&byte);

        if(bytes == 1) {
            if(byte == F) state = RECEIVED_F;
        }
        break;
    case RECEIVED_F:
        unsigned char byte;
        int bytes = readByteSerialPort(&byte);

        if(bytes == 1) {
            if(byte == A_T_COMMAND) state = RECEIVED_A;
            else state = INITIAL;
        }
        break;
    case RECEIVED_A:

    //CONTINUE HERE
        unsigned char byte;
        int bytes = readByteSerialPort(&byte);

        if(bytes == 1) {
            if(byte == F) state = RECEIVED_F;
        }
        break;
    case RECEIVED_C:
        unsigned char byte;
        int bytes = readByteSerialPort(&byte);

        if(bytes == 1) {
            if(byte == F) state = RECEIVED_F;
        }
        break;
    case RECEIVED_BCC1:
        unsigned char byte;
        int bytes = readByteSerialPort(&byte);

        if(bytes == 1) {
            if(byte == F) state = RECEIVED_F;
        }
        break;
    case END:

        break;
    default:
        break;
    }
}

void updateRecievingIState(recieving_I_state *state, unsigned char byte, unsigned char* header){
    switch (*state)
    {
    case INITIAL:
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
            *state = END;
        }
        break;
    
    default:
        break;
    }
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

        recieving_S_state state = INITIAL;

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
    // TODO: Implement this function

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    recieving_I_state state = INITIAL;
    unsigned char *header;
    int destuffing = 0;
    int index = 0;
    unsigned char expected_bcc2 = 0x0;

    while (state != END){
        unsigned char byte;
        int nBytes = readByteSerialPort(&byte);

        if(nBytes > 0){
            updateRecievingIState(&state, byte, header);

            if(state == BCC1_RECEIVED && byte == (*header ^ *(header+1))){ //check if bcc1 is correct
                if(*(header+1) == Ns << 7){ //check if the ns we received are the one we are expecting
                    state = RECEIVING_DATA;
                }
                else{ //it is not the one we are expecting
                    writeBytesSerialPort(C_RR || Ns, 1);
                    return -1;
                }
            }
            else if (state == BCC1_RECEIVED){ //if bcc1 is incorrect
                state = INITIAL;
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
        Ns = (Ns == 1 ? 0 : 1); //change ns
        writeBytesSerialPort(C_RR || Ns, 1); //reply "send me new frame"
        return index - 1; //minus 1 to not count the bcc2
    }

    writeBytesSerialPort(C_REJ || Ns, 1); //send REJ(Ns)
    return -1;

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose()
{
    // TODO: Implement this function

    return 0;
}
