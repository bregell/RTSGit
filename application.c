#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include <machine/hcs12/pim.h>

#define NODE_ID 1

PPIM port_struct = 0x0240;

//Defenition of a Tone
typedef struct {
    Object super;
    int period;
    int state;
} Tone;

Tone tone = { 
    initObject(),
    506,
    1
};

//Defenition of a Song
typedef struct {
    Object super;
    int noteCounter;
    int notes[32];
    int pitch[32];
    Time pitchLenght[3];
    int key;
    int state;
    int mode;
} Song;

Song brotherJohn = { 
    initObject(),
    0,
    {0,2,4,0, 0,2,4,0, 4,5,7,4, 5,7,7,9, 7,5,4,0, 7,9,7,5, 4,0,0,-5, 0,0,-5,0},
    {0,0,0,0, 0,0,0,0, 0,0,1,0, 0,1,2,2, 2,2,0,0, 2,2,2,2, 0,0,0,0, 1,0,0,1},
    //a a a a, a a a a, a a b a, a b c c, c c a a, c c c c, a a a a, b a a b    
    {MSEC(500), MSEC(1000), MSEC(250)}, 
    0,
    0,
    0
};



//Defenition of the base App
typedef struct {
    Object super;
    int count;
    char c;
} App;

App app = { 
    initObject(),
    0, 
    'X'
};

//Prototypes
void reader(App*, int);
void receiver(App*, int);
void playTone(Tone *self, int arg);
void changeTone(Tone *self, int period);
void nextNote(Song *self, int inc);

//Variables for keyboard input
int     counter     = 0;
int     sum         = 0;
char    buffer[20];
char    strOut[80];

//Variable for CAN sync
int leaderId;
int playMode;
int step = 4;

//Tasks
Msg toneTask, songTask, sendTask;


//Periods for generating sound 1/p = f
int periods[25] = {
    2025,1911,1804,1703,1607,1517,
    1432,1351,1276,1204,1136,1073,
    1012,956,902,851,804,758,716,
    676,638,602,568,536,506
};

//Register functions for interupts
Serial sci0 = initSerial(SCI_PORT0, &app, reader);
Can can0 = initCan(CAN0BASE, &app, receiver);

//Function to play one tone forever
void playTone(Tone *self, int arg){
	port_struct -> ptp = !port_struct -> ptp;
	if(self->state){
		toneTask = SEND(USEC(self->period), USEC(50), self, playTone, 0);
	}
}

//Function to change tone
void changeTone(Tone *self, int period) {
    self -> period = period;
    //sprintf(strOut, "Tone %i \n", self -> period);
    //SCI_WRITE(&sci0, strOut);
}

//Turn tone generation on or off
void controlTone(Tone *self, int state){
	self -> state = state;
	port_struct -> ptp = 0;
}

void sendNext(Song *self, int targetNode){
	CANMsg msg;
	msg.msgId = 6;
	msg.nodeId = NODE_ID;
	msg.length = 2;
	msg.buff[0] = (uchar)playMode;
	msg.buff[1] = (uchar)targetNode;
	CAN_SEND(&can0, &msg);
}

//Function to play one song forever
void playSong(Song *self, int arg){
	if(self->state){
		int x = self -> notes[self -> noteCounter] + 10 + self -> key;
		//sprintf(strOut, "Counter: %i \n", self->noteCounter);
		//SCI_WRITE(&sci0, strOut);
		//Change period of tone 
		BEFORE(MSEC(50), &tone, changeTone, periods[x]);
		if (leaderId == NODE_ID){
			if ((playMode == 0) || (playMode == 1)){ //Play Normal and Canon
				//Set time to play next tone
				songTask = AFTER(self -> pitchLenght[self -> pitch[self -> noteCounter]], self, playSong, 0);
				//Send play mesage to other 
				sendTask = AFTER(self -> pitchLenght[self -> pitch[self -> noteCounter]], self, sendNext, 0);
			} else if (playMode == 2) {
				if(!((self->noteCounter)%2) && (self->noteCounter != 0)){
					SYNC(&tone, controlTone, 1);
					toneTask = ASYNC(&tone, playTone, 0);
				} else if (self->noteCounter != 0) {
					SYNC(&tone, controlTone, 0);
				}
				//Set time to play next tone
				songTask = AFTER(self -> pitchLenght[self -> pitch[self -> noteCounter]], self, playSong, 0);
				//Send play mesage to other 
				sendTask = AFTER(self -> pitchLenght[self -> pitch[self -> noteCounter]], self, sendNext, 0);		
			}			
		}	
		//Tone counter
		ASYNC(self, nextNote, 1);
	}
}

void nextNote(Song *self, int inc){
	self -> noteCounter = (self -> noteCounter + inc)%32;
}

void controlSong(Song *self, int state) {
	self->state = state;
	self -> noteCounter = 0;
}

void changeKey(Song *self, int key){
    self -> key = key;
    sprintf(strOut, "Key changed to: %i \n", key);
    SCI_WRITE(&sci0, strOut);
}

void changeTempo(Song *self, int tempo){
    int a,b,c;
    a = tempo;
    b = tempo*2;
    c = tempo/2;
    self -> pitchLenght[0] = MSEC(a);
    self -> pitchLenght[1] = MSEC(b);
    self -> pitchLenght[2] = MSEC(c);
    sprintf(strOut, "Tempo changed to: %i \n", tempo);
    SCI_WRITE(&sci0, strOut);
}

void receiver(App *self, int unused) {
    int delay; 
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
    if((msg.msgId == 0) && (msg.nodeId == leaderId)){ //Play
        leaderId = msg.nodeId;
        playMode = 0;
	SYNC(&brotherJohn, changeKey, (int)(msg.buff[0]-5));  
	SYNC(&tone, controlTone, 1);
	SYNC(&brotherJohn, controlSong, 1);
	ASYNC(&tone, playTone, 0);
	ASYNC(&brotherJohn, playSong, 0);     
        sprintf(strOut, "Play, NodeId: %i ,Key: %i-5 \n", (int)msg.nodeId, (int)msg.buff[0]);
        SCI_WRITE(&sci0, strOut);
    } else if ((msg.msgId == 1) && (msg.nodeId == leaderId)) { //Change Key
        SYNC(&brotherJohn, changeKey, (int)(msg.buff[0]-5));
        sprintf(strOut, "Change Key, NodeId: %i ,Key: %i-5 \n", (int)msg.nodeId, (int)msg.buff[0]);
        SCI_WRITE(&sci0, strOut);
    } else if ((msg.msgId == 2) && (msg.nodeId == leaderId)) { //Change Offset  
        sprintf(strOut, "Change Offset, NodeId: %i ,Key: %i-5 \n", (int)msg.nodeId, (int)msg.buff[0]);
        SCI_WRITE(&sci0, strOut);
    } else if ((msg.msgId == 3) && (msg.nodeId == leaderId)) { //Play Canon
        leaderId = msg.nodeId;
        playMode = 1;    
	SYNC(&brotherJohn, changeKey, (int)(msg.buff[0]-5));
	SYNC(&brotherJohn, changeTempo, (int)(200+4*msg.buff[5]));
	step = (int)(msg.buff[2]); //*msg.buff[5]);	
	delay = (int)(step*(200+4*msg.buff[5]));
	SYNC(&tone, controlTone, 0);
	SYNC(&brotherJohn, controlSong, 0);
	SEND(MSEC(step*MSEC_OF(brotherJohn.pitchLenght[0])), MSEC(10), &tone, controlTone, 1);
	SEND(MSEC(step*MSEC_OF(brotherJohn.pitchLenght[0])), MSEC(10), &brotherJohn, controlSong, 1);		
	SEND(MSEC(step*MSEC_OF(brotherJohn.pitchLenght[0])), MSEC(100), &tone, playTone, 0);
	SEND(MSEC(step*MSEC_OF(brotherJohn.pitchLenght[0])), MSEC(100), &brotherJohn, playSong, 0);
        sprintf(strOut, "Play Canon, NodeId: %i ,Key: %i-5, Offset: %i, Tempo: %i\n", (int)msg.nodeId, (int)msg.buff[0], (int)msg.buff[2], (int)msg.buff[5]);
        SCI_WRITE(&sci0, strOut);
    } else if ((msg.msgId == 4) && (msg.nodeId == leaderId)) { //Play Sequential
        leaderId = msg.nodeId;
        playMode = 2;
	SYNC(&tone, controlTone, 1);
	SYNC(&brotherJohn, controlSong, 1);
	ASYNC(&brotherJohn, playSong, 0);
        SYNC(&brotherJohn, changeKey, (int)(msg.buff[0]-5));
        sprintf(strOut, "Play, NodeId: %i ,Key: %i-5 \n", (int)msg.nodeId, (int)msg.buff[0]);
    } else if ((msg.msgId == 5)) { // && (msg.nodeId == leaderId)) { //Stop song
        //sprintf(strOut, "Stoping Song, NodeId: %i \n", (int)msg.nodeId);
    	//SCI_WRITE(&sci0, strOut);
	leaderId = msg.nodeId;
	SYNC(&brotherJohn, controlSong, 0);
	SYNC(&tone, controlTone, 0);
	sprintf(strOut, "Leadership caimed by NodeId: %i \n", (int)msg.nodeId);
    	SCI_WRITE(&sci0, strOut);
    } else if ((msg.msgId == 6) && (msg.nodeId == leaderId)) { //Next tone message
        //All att once mode
    	if (msg.buff[0] == 0){
    		ASYNC(&brotherJohn, playSong, 0);
    	} 
    	// Canon mode
    	else if (msg.buff[0] == 1) {
    		AFTER(MSEC(step*MSEC_OF(brotherJohn.pitchLenght[0])), &brotherJohn, playSong, 0);
    	}
	// Sequential mode
	else if (msg.buff[0] == 2) {
		if(brotherJohn.noteCounter%2){
			ASYNC(&brotherJohn, playSong, 0);
			SYNC(&tone, controlTone, 1);
			ASYNC(&tone, playTone, 0);
		} else {
			ASYNC(&brotherJohn, playSong, 0);
			SYNC(&tone, controlTone, 0);	
		} 
    	}	
	sprintf(strOut, "Next Tone, NodeId: %i \n", (int)msg.nodeId);
    	SCI_WRITE(&sci0, strOut);
    } else {
	sprintf(
		strOut, 
		"MsgId %i, LeaderId %i, NodeId: %i ,Buf0: %i, Buf1: %i \n", 
		(int)msg.msgId, leaderId, (int)msg.nodeId, (int)msg.buff[0], (int)msg.buff[1]
	);
	SCI_WRITE(&sci0, strOut);
    }
}

void reader(App *self, int c) {
    //Create CAN message
    CANMsg msg;
    //int tempo;
    int i;
    if (c == 'r') {
        sum = 0;
        counter = 0;
        SCI_WRITE(&sci0, "Reset input \n");
    } else if (c == 'p') { 
	if(leaderId == NODE_ID){
		SCI_WRITE(&sci0, "Start song \n");
		//Send start message
		msg.msgId = 0;
		msg.nodeId = NODE_ID;
		msg.length = 1;
		msg.buff[0] = (uchar)(brotherJohn.key+5);
		CAN_SEND(&can0, &msg);
		//Set leader to self
		leaderId = NODE_ID;
		playMode = 0;
		//Start playing song 
		SYNC(&tone, controlTone, 1);
		SYNC(&brotherJohn, controlSong, 1);
		ASYNC(&tone, playTone, 0);
		ASYNC(&brotherJohn, playSong, 0);
	} else {
		SCI_WRITE(&sci0, "Not leader, clain leadership first \n");
	}
    } else if (c == 's') {
	if(leaderId == NODE_ID){
		SCI_WRITE(&sci0, "Start sequential \n");
		//Send start message 
		msg.msgId = 4;
		msg.nodeId = NODE_ID;
		msg.length = 2;
		msg.buff[0] = (uchar)(brotherJohn.key+5);
		msg.buff[1] = (uchar)((MSEC_OF(brotherJohn.pitchLenght[0])-200)/4);
		CAN_SEND(&can0, &msg);
		//Set leader to self
		leaderId = NODE_ID;
		playMode = 2;
		//Start playing song 
		SYNC(&tone, controlTone, 1);
		SYNC(&brotherJohn, controlSong, 1);
		ASYNC(&tone, playTone, 0);
		ASYNC(&brotherJohn, playSong, 0);
		brotherJohn.mode = 1;
	} else {
		SCI_WRITE(&sci0, "Not leader, clain leadership first \n");
	}
    } else if (c == 'c') {
	if(leaderId == NODE_ID){
		//Send start message Canon
		msg.msgId = 3;
		msg.nodeId = NODE_ID;
		msg.length = 6;
		msg.buff[0] = (uchar)brotherJohn.key+5;
		msg.buff[1] = step * 3;
		msg.buff[2] = step;
		msg.buff[3] = step * 1;
		msg.buff[4] = step * 2;
		//a=200+4*["Tempo"]
		msg.buff[5] = (uchar)((MSEC_OF(brotherJohn.pitchLenght[0])-200)/4);
		CAN_SEND(&can0, &msg);
		leaderId = NODE_ID;
		playMode = 1;
		//Start playing song 
		SYNC(&tone, controlTone, 1);
		SYNC(&brotherJohn, controlSong, 1);
		ASYNC(&tone, playTone, 0);
		ASYNC(&brotherJohn, playSong, 0);
		brotherJohn.mode = 1;
		sprintf(strOut, "Start Canon, NodeId: %i ,Key: %i-5, Offset: %i, Tempo: %i\n", NODE_ID, (int)msg.buff[0], (int)msg.buff[5]);
		SCI_WRITE(&sci0, strOut);
	} else {
		SCI_WRITE(&sci0, "Not leader, clain leadership first \n");
	}
    } else if (c == 'h') {
	if(leaderId == NODE_ID){
		SCI_WRITE(&sci0, "Stop song \n");
	} else {
		SCI_WRITE(&sci0, "Claiming leadership.. \nLeadership claimed! \n");
		leaderId = NODE_ID;
	}
	//Send stop message
	msg.msgId = 5;
	msg.nodeId = NODE_ID;
	msg.length = 0;
	//msg.buff = {};
	CAN_SEND(&can0, &msg);
	//Stop playing song
	brotherJohn.mode = 0;
	SYNC(&brotherJohn, controlSong, 0);
	SYNC(&tone, controlTone, 0);
    } else if ((c == 'k') && (counter != 19)){
        sum = atoi(buffer);
        if ((sum >= -5) && (sum <= 5) && (NODE_ID == leaderId)) {
            msg.msgId = 1;
            msg.nodeId = NODE_ID;
            msg.length = 1;
            msg.buff[0] = (uchar)sum+5;
            CAN_SEND(&can0, &msg);
            SYNC(&brotherJohn, changeKey, sum);
            sprintf(strOut, "The key has been set to %i \n", sum);
            SCI_WRITE(&sci0, strOut);
        }
        else{
            sprintf(strOut, "The key %i is out of range \n", sum);
            SCI_WRITE(&sci0, strOut);
        };
        counter = 0;
    } else if ((c == 't') && (counter != 19)){ 
        sum = atoi(buffer);
        if ((sum >= 200) && (sum <= 5000) && (NODE_ID == leaderId)) {
            SYNC(&brotherJohn, changeTempo, sum);
            sprintf(strOut, "The tempo has been set to %i \n", sum);
            SCI_WRITE(&sci0, strOut);
        }
        else{
            sprintf(strOut, "The tempo %i is out of range \n", sum);
            SCI_WRITE(&sci0, strOut);
        };
        counter = 0;
    } else if ((c == 'o') && (counter != 19) && (NODE_ID == leaderId)){ 
        sum = atoi(buffer);
        if ((sum >= 0) && (sum <= 12)) {
		step = sum;
		sprintf(strOut, "The step is set to %i \n", sum);
		SCI_WRITE(&sci0, strOut);
        }
        else{
            sprintf(strOut, "The step %i is out of range \n", sum);
            SCI_WRITE(&sci0, strOut);
        };
        counter = 0;
    } else {
        SCI_WRITECHAR(&sci0, c);
        buffer[counter] = c;
        buffer[counter + 1] = '\0';
        counter = counter + 1;
    };
}

void startApp(App *self, int arg) {
    //Initialize CAN
    CAN_INIT(&can0);
    
    //Com port stuffz
    SCI_INIT(&sci0);
    SCI_WRITE(&sci0, "Hello, hello...\n");
    
    //Set direction status of port p
    port_struct -> ddrp = 0xFF;    
}

int main() {
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
    INSTALL(&can0, can_interrupt, CAN_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}
