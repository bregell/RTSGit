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
int indices[32] = {0,2,4,0,0,2,4,0,4,5,7,4,5,7,7,9,7,5,4,0,7,9,7,5,4,0,0,-5,0,0,-5,0};
int periods[25] = {2025,1911,1804,1703,1607,1517,1432,1351,1276,1204,1136,1073,1012,956,902,851,804,758,716,676,638,602,568,536,506};
int max_index = 14;
int min_index = -10;
char buffer[20];
char strOut[80];

Serial sci0 = initSerial(SCI_PORT0, &app, reader);

Can can0 = initCan(CAN0BASE, &app, receiver);

void receiver(App *self, int unused) {
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
    SCI_WRITE(&sci0, "Can msg received: ");
    SCI_WRITE(&sci0, msg.buff);
}

void reader(App *self, int c) {
    int periodIndex;
    int i;
    SCI_WRITE(&sci0, "Rcv: \'");
    if (c == 'f'){
	sum = 0;
	counter = 0;
	SCI_WRITE(&sci0, "Reset program");
    }
    else{
	if ((c == 'e') && (counter != 19)){
		sum = atoi(buffer);
		if ((sum <= 5) && (sum >= -5)){
			for(i=0;i<32;i++){
				periodIndex = indices[i] + 10 + sum;
				sprintf(strOut, "The period for indice %i in key %i %is %i us\n", indices[i], sum, periods[periodIndex]);
				SCI_WRITE(&sci0, strOut);
			}
		}
		else{
			sprintf(strOut, "The key %i is out of range ", sum);
			SCI_WRITE(&sci0, strOut);
		};
		counter = 0;	
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
