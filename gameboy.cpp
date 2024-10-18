unsigned char memoryRead(int);
void memoryWrite(int,unsigned char);


#include "Z80.h"
#include <stdio.h>
#include <iostream>
#include <fstream>

#include <sys/select.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>


using namespace std;

//char rom[]={0x06,0x06,0x3e,0x00,0x80,0x05,0xc2,0x04,0x00,0x76};
Z80* z80;
char* rom;
int romSize;

unsigned char graphicsRAM[8192];
int palette[4];
int tileset, tilemap, scrollx, scrolly;

int Screen[160][144];


unsigned char memoryRead(int address)
{
	return rom[address];
}

void memoryWrite(int address, unsigned char b)
{
}

void renderScreen()
{
	for(int xpixel=0; xpixel<160; xpixel++)
	{
		for(int ypixel=0; ypixel<144; ypixel++)
		{
			//your code here: figure out color in terms of xpixel,ypixel...
			int color=0;	

			Screen[xpixel][ypixel]=color;
		}
	}

    //TODO: STEP 2 HERE

	//demo: turn on pixels 2,3 ==> 3 (black)
	//demo: 100,100 ==> 2 (gray)
//	Screen[2][3]=3;
//	Screen[100][100]=2;

	//this clears the terminal screen
	printf("\033c");
	for(int y=0; y<144; y++)
	{
		for(int x=0; x<160; x++)
		{
			if(Screen[x][y]==3)
				printf("*");
			if(Screen[x][y]==2)
				printf("+");
			if(Screen[x][y]==1)
				printf(".");
			if(Screen[x][y]==0)
				printf(" ");
		}
		printf("\n");
	}
}


//don't use this until keyboard step
//https://stackoverflow.com/questions/448944/c-non-blocking-keyboard-input
struct termios origterm;
void reset_onexit()
{
	tcsetattr(0,TCSANOW,&origterm);
}
//when ready for keyboard, call this at start of main
void set_conio_mode()
{
	struct termios newterm;
	//backup terminal attributes
	tcgetattr(0,&origterm);
	memcpy(&newterm,&origterm,sizeof(newterm));

	atexit(reset_onexit);
	cfmakeraw(&newterm);
	tcsetattr(0,TCSANOW,&newterm);
}
//call this to find out 1: there's a key, 0: no key
int kbhit()
{
	struct timeval tv={0L,0L};
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(0,&fds);
	return select(1,&fds,NULL,NULL,&tv)>0;
}
//call this to get the identity of the key.  you can do this on each frame
//once  you call this, then you can write a keydown and keyup function to handle key press

//if you use this, check for a return value of "3".  this is ctrl-c.  if you see a 3, call return from main to stop program
int getch()
{
	int r;
	unsigned char c;
	if((r=read(0,&c,sizeof(c)))<0)
		return r;
	else
		return c;
}



int main(int argc, char* argv[])
{
//	ifstream romfile(argv[1], ios::in|ios::binary|ios::ate);
	ifstream romfile("testrom.gb", ios::in|ios::binary|ios::ate);
	streampos size=romfile.tellg();

	rom=new char[size];
	romSize=size;
	romfile.seekg(0,ios::beg);
	romfile.read(rom,size);
	romfile.close();

	z80 = new Z80(memoryRead,memoryWrite);

	z80->reset();

    //ToDO: STEP 3 HERE
//	z80->PC=0;

//	while(!z80->halted)
//	{
//		z80->doInstruction();
//		printf("PC=%x A=%d B=%d\n",z80->PC,z80->A,z80->B);
//	}

	ifstream vidfile("screendump.txt",ios::in);
	for(int i=0; i<8192; i++)
	{
		int n;
		vidfile>>n;
		graphicsRAM[i]=(unsigned char)n;
	}
	vidfile >> tileset;
	vidfile >> tilemap;
	vidfile >> scrollx;
	vidfile >> scrolly;
	vidfile >> palette[0];
	vidfile >> palette[1];
	vidfile >> palette[2];
	vidfile >> palette[3];

	renderScreen();

}
