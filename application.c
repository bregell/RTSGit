#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"


typedef struct {
    Object super;
    int count;
    char c;
} App;

App app = { initObject(), 0, 'X' };

void reader(App*, int);
void receiver(App*, int);

int counter = 0;
int sum = 0;
char buffer[20];
char strOut[40];

Serial sci0 = initSerial(SCI_PORT0, &app, reader);

Can can0 = initCan(CAN0BASE, &app, receiver);

void receiver(App *self, int unused) {
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
    SCI_WRITE(&sci0, "Can msg received: ");
    SCI_WRITE(&sci0, msg.buff);
}

void reader(App *self, int c) {
    
    SCI_WRITE(&sci0, "Rcv: \'");
    if (c == 'f'){
	sum = 0;
	counter = 0;
	SCI_WRITE(&sci0, "Reset program");
    }
    else{
	if ((c == 'e') && (counter != 19)){
		sum = sum + atoi(buffer);
		counter = 0;
		sprintf(strOut,"The value is %i \n The running sum is %i", atoi(buffer), sum);
		SCI_WRITE(&sci0, strOut);
	}
	else{
		SCI_WRITECHAR(&sci0, c);
		buffer[counter] = c;
		buffer[counter+1] = '\0';
		counter = counter + 1;
		sprintf(strOut,"%i", counter);
		SCI_WRITE(&sci0, strOut);
	};
    };
    SCI_WRITE(&sci0, "\'\n");
}

void startApp(App *self, int arg) {
    CANMsg msg;
    SCI_INIT(&sci0);
    CAN_INIT(&can0);
    SCI_WRITE(&sci0, "Hello, hello...\n");
    msg.msgId = 1;
    msg.nodeId = 1;
    msg.length = 6;
    msg.buff[0] = 'H';
    msg.buff[1] = 'e';
    msg.buff[2] = 'l';
    msg.buff[3] = 'l';
    msg.buff[4] = 'o';
    msg.buff[5] = 0;
    CAN_SEND(&can0, &msg);
}

int main() {
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
    INSTALL(&can0, can_interrupt, CAN_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}
