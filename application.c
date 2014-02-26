#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include <machine/hcs12/pim.h>

//#define PTP = (*(char *) 0x0258)
//#define DDRP = (*(char *) 0x025A)
PPIM port_struct = 0x0240;

typedef struct {
	Object super;
	int period;
} Tone;

Tone tone = { initObject(), 500 };

typedef struct {
    Object super;
    int count;
    char c;
} App;

App app = { initObject(), 0, 'X' };

void reader(App*, int);
void receiver(App*, int);

int background_loop_range = 0;
int counter = 0;
int sum = 0;
int indices[32] = {0,2,4,0,0,2,4,0,4,5,7,4,5,7,7,9,7,5,4,0,7,9,7,5,4,0,0,-5,0,0,-5,0};
int periods[25] = {2025,1911,1804,1703,1607,1517,1432,1351,1276,1204,1136,1073,1012,956,902,851,804,758,716,676,638,602,568,536,506};
int max_index = 14;
int min_index = -10;
int timecounter = 0; 
char buffer[20];
char strOut[80];

Serial sci0 = initSerial(SCI_PORT0, &app, reader);
Can can0 = initCan(CAN0BASE, &app, receiver);

void play(Tone *self, int unused){
	port_struct -> ptp = !port_struct -> ptp;
	SEND(USEC(self->period), USEC(100), self, play, 0);
}


void receiver(App *self, int unused) {
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
    SCI_WRITE(&sci0, "Can msg received: ");
    SCI_WRITE(&sci0, msg.buff);
}

void reader(App *self, int c) {
    int periodIndex;
    int i, buff1, buff2;
    Time gb;
    SCI_WRITE(&sci0, "Rcv: \'");
    if (c == 'f'){
	sum = 0;
	counter = 0;
	background_loop_range = sum;
	SCI_WRITE(&sci0, "Reset program");
    }
    else{
	if ((c == 'e') && (counter != 19)){
		sum = atoi(buffer);
		background_loop_range = sum;
		if ((sum <= -10) && (sum >= 14)){
			&tone -> period = periods[indices[sum]];
		}
		else{
			sprintf(strOut, "The key %i is out of range ", sum);
			SCI_WRITE(&sci0, strOut);
		};
		counter = 0;
	} else {
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
    
    //Com port stuffz
    SCI_INIT(&sci0);
    SCI_WRITE(&sci0, "Hello, hello...\n");
    
    //Set direction status of port p
    port_struct -> ddrp = 0xFF; 
    //Start loop
    BEFORE(USEC(100), &tone, play, 0);
    
    //CAN stuffz
    CAN_INIT(&can0);
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
