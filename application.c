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
} Song;

Song brotherJohn = { 
    initObject(),
    0,
    {0,2,4,0, 0,2,4,0, 4,5,7,4, 5,7,7,9, 7,5,4,0, 7,9,7,5, 4,0,0,-5, 0,0,-5,0},
    {0,0,0,0, 0,0,0,0, 0,0,1,0, 0,1,2,2, 2,2,0,0, 2,2,2,2, 0,0,0,0, 1,0,0,1},
    //a a a a, a a a a, a a b a, a b c c, c c a a, c c c c, a a a a, b a a b    
    {MSEC(500), MSEC(1000), MSEC(250)}, 
    0,
    1
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

//Variables for keyboard input
int     counter     = 0;
int     sum         = 0;
char    buffer[20];
char    strOut[80];

//Variable for CAN sync
int leaderId;
int playMode;
int step;

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
    if(arg == 1){
	self -> state = 1;
    }
    if (self -> state == 1) {
        port_struct -> ptp = !port_struct -> ptp;
        SEND(USEC(self->period), USEC(50), self, playTone, 0);
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
}

void sendNext(Song *self, int targetNode){
	CANMsg msg;
	msg.msgId = 6;
	msg.nodeId = NODE_ID;
	msg.length = 4;
	msg.buff[0] = (uchar)targetNode;
	CAN_SEND(&can0, &msg);
}

//Function to play one song forever
void playSong(Song *self, int arg){
    Time pitchL, stepVal;
    int x = self -> notes[self -> noteCounter] + 10 + self -> key;
    if(arg == 1){
	self -> state = 1;
    }
    if (self -> state == 1){
	if (leaderId == NODE_ID){
		if(playMode == 0){ //Play Normal
			//Set time to play next tone
			SEND(self -> pitchLenght[self -> pitch[self -> noteCounter]], USEC(50), self, playSong, 0);
			//Send play mesage to other 
			SEND(self -> pitchLenght[self -> pitch[self -> noteCounter]], USEC(50), self, sendNext, 0);
		} else if(playMode == 1){ //Play Canon
			//Time to next tone
			pitchL = self -> pitchLenght[self -> pitch[self -> noteCounter]];
			//Set time to play next tone
			SEND(pitchL, USEC(50), self, playSong, 0);
			//Step * a length
			stepVal = MSEC(MSEC_OF(self->pitchLenght[0])*step);
			//First offset 
			pitchL = pitchL + stepVal;
			SEND(pitchL, USEC(50), self, sendNext, (NODE_ID+1)%4);
			//Second offset
			pitchL = pitchL + stepVal;
			SEND(pitchL, USEC(50), self, sendNext, (NODE_ID+2)%4);
			//Third offset
			pitchL = pitchL + stepVal;
			SEND(pitchL, USEC(50), self, sendNext, (NODE_ID+3)%4);		
		} else if(playMode == 2){ //Play Seq
            //Set new leader
			leaderId = (NODE_ID+1)%4; 
            //Send messsage to new leader
			SEND(self -> pitchLenght[self -> pitch[self -> noteCounter]], USEC(50), self, sendNext, leaderId);
		}
	}
	//Change period of tone 
        BEFORE(MSEC(50), &tone, changeTone, periods[x]);
	//Tone counter
	if(playMode == 2){
		self -> noteCounter = (self -> noteCounter + 4)%32;
	} else {
		self -> noteCounter = (self -> noteCounter + 1)%32;
	}
	sprintf(strOut, "Counter: %i \n", self -> noteCounter);
	SCI_WRITE(&sci0, strOut);
	//Old shit
        /* if (self -> noteCounter < 31) {
		if(playMode = 2){
			self -> noteCounter = self -> noteCounter + 4;
		} else {
			self -> noteCounter = self -> noteCounter + 1;
		}
		sprintf(strOut, "NC: %i T: %i \n", self -> noteCounter, self -> pitchLenght[self -> pitch[self -> noteCounter]]);
		SCI_WRITE(&sci0, strOut);
        } else {
		self -> noteCounter = 0;
		sprintf(strOut, "AGAIN \n");
		SCI_WRITE(&sci0, strOut);
        } */
    }
}

void controlSong(Song *self, int state) {
    self -> state = state;
    self -> noteCounter = 0;
}

void changeKey(Song *self, int key){
    self -> key = key;
}

void changeTempo(Song *self, int tempo){
    int a,b,c;
    a = tempo;
    b = tempo*2;
    c = tempo/2;
    self -> pitchLenght[0] = MSEC(a);
    self -> pitchLenght[1] = MSEC(b);
    self -> pitchLenght[2] = MSEC(c);
}

void receiver(App *self, int unused) {
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
    if(msg.msgId == 0){ //Play
        leaderId = msg.nodeId;
        playMode = 0;
        BEFORE(USEC(100), &tone, playTone, 0);
        BEFORE(USEC(100), &brotherJohn, playSong, 0);
        BEFORE(USEC(50), &brotherJohn, changeKey, msg.buff[0]-5);
        BEFORE(USEC(100), &tone, playTone, 1);
        BEFORE(USEC(100), &brotherJohn, playSong, 1);
        sprintf(strOut, "Play, NodeId: %i ,Key: %i+5 \n", (int)msg.nodeId, (int)msg.buff[0]);
        SCI_WRITE(&sci0, strOut);
    } else if (msg.msgId == 1 && msg.nodeId == leaderId) { //Change Key
        BEFORE(USEC(50), &brotherJohn, changeKey, msg.buff[0]-5);
        sprintf(strOut, "Change Key, NodeId: %i ,Key: %i+5 \n", (int)msg.nodeId, (int)msg.buff[0]);
        SCI_WRITE(&sci0, strOut);
    } else if (msg.msgId == 2) {
        //BEFORE(USEC(50), &brotherJohn, changeTempo,(200+4*(int)msg.buff[1]));
    } else if (msg.msgId == 3) { //Play Canon
        leaderId = msg.nodeId;
        playMode = 1;
        BEFORE(USEC(100), &tone, playTone, 0);
        BEFORE(USEC(100), &brotherJohn, playSong, 0);
        BEFORE(USEC(50), &brotherJohn, changeKey, msg.buff[0]-5);
        sprintf(strOut, "Play Canon, NodeId: %i ,Key: %i+5, Step: %i \n", (int)msg.nodeId, (int)msg.buff[0], (int)msg.buff[1]);
        SCI_WRITE(&sci0, strOut);
    } else if (msg.msgId == 4) { // Play Sequential
        leaderId = msg.nodeId;
    	playMode = 2;
    	BEFORE(USEC(100), &tone, playTone, 0);
    	BEFORE(USEC(100), &brotherJohn, playSong, 0);
    	BEFORE(USEC(50), &brotherJohn, changeKey, msg.buff[0]-5);
    	sprintf(strOut, "Play Canon, NodeId: %i ,Key: %i+5, Step: %i \n", (int)msg.nodeId, (int)msg.buff[0], (int)msg.buff[1]);
    	SCI_WRITE(&sci0, strOut);
    } else if (msg.msgId == 5 && msg.nodeId == leaderId) { //Stop song
        BEFORE(USEC(100), &tone, playTone, 0);
        BEFORE(USEC(100), &brotherJohn, playSong, 0);
    } else if (msg.msgId == 6 && (msg.nodeId == leaderId)) { //Next tone message
        //All att once mode
    	if (msg.buff[0] == 0){ 
    		BEFORE(USEC(100), &brotherJohn, playSong, 1);
    	} 
    	// Canon mode
    	else if ((msg.buff[0] == 1) && (msg.buff[1] == NODE_ID)) {
    		BEFORE(USEC(100), &brotherJohn, playSong, 1);
    	} 
    	//Sequential mode
    	else if ((msg.buff[0] == 2)) {
            if((msg.buff[1] == NODE_ID)) {
                BEFORE(USEC(100), &brotherJohn, playSong, 1);
            } else {
                leaderId = (leaderId+1)%4;
            }
    	}
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
        SCI_WRITE(&sci0, "Start song \n");
        //Send start message
        msg.msgId = 0;
        msg.nodeId = NODE_ID;
        msg.length = 4;
        msg.buff[0] = (uchar)brotherJohn.key+5;
        CAN_SEND(&can0, &msg);
        //Set leader to self
        leaderId = NODE_ID;
        playMode = 0;
        //Start playing song 
        BEFORE(USEC(100), &tone, playTone, 1);
        BEFORE(USEC(100), &brotherJohn, playSong, 1);
    } else if ((c == 'c') && (counter != 4)) { 
        sum = atoi(buffer);
        if ((sum >= 0) && (sum <= 127)) {
            SCI_WRITE(&sci0, "Start canon \n");
            //Send start message
            msg.msgId = 3;
            msg.nodeId = NODE_ID;
            msg.length = 4;
            //msg.buff = {song.key, song.tempo, sum};
            CAN_SEND(&can0, &msg);
            leaderId = NODE_ID;
            playMode = 1;
            //Start playing song 
            BEFORE(USEC(100), &tone, playTone, 1);
            BEFORE(USEC(100), &brotherJohn, playSong, 1);
        } else {
            sprintf(strOut, "The step %i is out of range \n", sum);
            SCI_WRITE(&sci0, strOut);
        };
        counter = 0;
    } else if ((c == 's') && (counter != 4)) { 
        sum = atoi(buffer);
        if ((sum >= 0) && (sum <= 127)) {
            SCI_WRITE(&sci0, "Start sequential \n");
            //Send start message
            msg.msgId = 0;
            msg.nodeId = NODE_ID;
            msg.length = 4;
            //msg.buff = {song.key, song.tempo};
            CAN_SEND(&can0, &msg);
            leaderId = NODE_ID;
            playMode = 2;
            //Start playing song 
            BEFORE(USEC(100), &tone, playTone, 1);
            BEFORE(USEC(100), &brotherJohn, playSong, 1);
        } else {
            sprintf(strOut, "The step %i is out of range \n", sum);
            SCI_WRITE(&sci0, strOut);
        };
        counter = 0;
    } else if (c == 'h') {
        SCI_WRITE(&sci0, "Stop song \n");
        //Send stop message
        msg.msgId = 5;
        msg.nodeId = NODE_ID;
        msg.length = 4;
        //msg.buff = {};
        CAN_SEND(&can0, &msg);
        //Stop playing song
        BEFORE(USEC(100), &tone, controlTone, 0);
        BEFORE(USEC(100), &brotherJohn, controlSong, 0);
    } else if ((c == 'k') && (counter != 19)){
        sum = atoi(buffer);
        if ((sum >= -5) && (sum <= 5)) {
            msg.msgId = 1;
            msg.nodeId = NODE_ID;
            msg.length = 4;
            msg.buff[0] = (uchar)sum+5;
            CAN_SEND(&can0, &msg);
            BEFORE(USEC(100), &brotherJohn, changeKey, sum);
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
        if ((sum >= 20) && (sum <= 5000)) {
            BEFORE(USEC(100), &brotherJohn, changeTempo, sum);
            sprintf(strOut, "The tempo has been set to %i \n", sum);
            SCI_WRITE(&sci0, strOut);
        }
        else{
            sprintf(strOut, "The tempo %i is out of range \n", sum);
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
