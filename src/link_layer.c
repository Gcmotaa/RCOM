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
    // TODO: Implement this function

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
