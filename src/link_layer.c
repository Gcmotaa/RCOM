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
#define C_RR0 0XAA
#define C_RR1 0XAB
#define C_REJ0 0X54
#define C_REJ1 0X5
#define C_DISC 0X0B
#define C_IF0 0x00
#define C_IF1 0x80

typedef enum{
    INITIAL,
    RECEIVED_F,
    RECEIVED_A,
    RECEIVED_C,
    RECEIVED_BCC1,
    END

} recieving_S_state;

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

                }
            }
            else if (state == BCC1_RECEIVED){
                state = INITIAL;
            }
        }
    }

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
