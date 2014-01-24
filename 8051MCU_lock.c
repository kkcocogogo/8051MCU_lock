/* 
feature:
	optimized for portability and performance
	keyboard_scan() function performance very stable
further notice:

	I never learn the true osc frequency of my current MCU board, I tried 12, 11.0592, and 10.969Mhz but they all failed to create a delay
	precise enougn. so keep in mind that all delays that says microseconds or seconds are roughly precise, I'm not makin a clock anyway.

	due to the flawed design of this TX-1C MCU board(check out TX-1C manual for more), keyboard inputs will affect lcd display, not a bug.
	send_data() will just work, but may come with some glitches, haven't got time to redesign.
*/

#include<reg52.h>
#include<string.h>

#define keyboard_bus P3
#define lcd_bus P0
#define led_debug P1 // using led light for debugging purpose
#define password_length 20
sbit beep=P2^3;// buzzer fires at beep = 0
sbit lcden=P3^4;// lcd enable bit
sbit lcdrs=P3^5;// lcd data/command selection bit

/*in this cast, we'll have to turn off 7-segment led to provide proper voltage for lcd
chip_select and segment_select*/
sbit chip_select=P2^6;
sbit segment_select=P2^7;

/*the following declarations are fundamental functions/variables related to the MCU board I/O*/
void send_command(unsigned char);
void send_data(unsigned char frDisplay_string[]);
void delay_ms(unsigned char);
void delay_s(unsigned char);
void beep_ms(unsigned char);
unsigned char delay_counter;
volatile unsigned char keyboard_scan();
/*all functions related to password logic goes here*/
void input(unsigned char password_temp[]); // input() captures user input and atores them in given array
volatile bit compare(unsigned char password_temp1[],unsigned char password_temp2[]);
unsigned char xdata password[password_length];// if you have battery-backed external RAM, password may be preserved after reset/power off

void main() {

	void initialize_MCU();// applicable to any 8051 MCU with 1602 lcd
	void initialize_password();
	volatile bit verify();
	unsigned char keystroke=0;
	volatile bit verification=0;

	initialize_MCU();
	initialize_password();

	while(1) {

		send_data("Y-UNLOCK        N-CHANGE        ");// there are better ways to manipulate lcd displays but I haven't got time
		while(keystroke!='y'&&keystroke!='n') keystroke=keyboard_scan();
		verification=verify();// if failed, verify() will never return

		switch(keystroke) {

			case 'n':
			keystroke='\0';
			initialize_password();
			break;

			case 'y':
			keystroke='\0';
			send_data("\aUNLOCKED!");// \a character causes a clear screen command, check out send_data() for detail
			delay_s(5);
			break;
		}
	}
}

volatile bit verify() {

	unsigned char strikeout_counter=0,password_temp[password_length];
	unsigned char xdata warning_message[]="\aFAIL!  ";
	send_data("\aPLEASE VERIFY");
	input(password_temp);

	while(~(compare(password_temp,password))) {

		strikeout_counter++;
		warning_message[strlen(warning_message)-1]=(unsigned char)(((unsigned char)'0')+strikeout_counter);
		send_data(warning_message);

		/* if verification failed, the program then goes into a endless loop, stands for permanent locked state*/
		if(strikeout_counter>=3) {
			send_data("\aFOREVER LOCKED, LOSER.");
			led_debug=0x00;
			while(1);
		}
		input(password_temp);
	}
	send_data("\aVERIFIED");
	return 1;
}

/*if two passwords agree, return 1*/
volatile bit compare(unsigned char password_temp1[],unsigned char password_temp2[]) {

	unsigned char password_counter;

	for (password_counter=0;password_counter<password_length-1;password_counter++)	{
		if(password_temp1[password_counter]!=password_temp2[password_counter]) return 0;
	}

	return 1;
}

void initialize_password() {

	/*user have to enter a new password twice to intialize/reset the password*/
	unsigned char xdata password_temp1[password_length],password_temp2[password_length];
	volatile bit idata password_set_flag=0;
	send_data("\aINITIALIZE PW");
	do {
		/*capture user input to password_temp[1], consider the following for loop as a nested function to capture function*/
		input(password_temp1);
		send_data("\aRE-ENTER");
		input(password_temp2);
		password_set_flag=compare(password_temp1,password_temp2);

		if(password_set_flag==1) {
			password[0]='\0';
			memcpy(password,password_temp2,password_length);
			send_data("\aPASSWORD SET!  CONGRATULATION!");
			delay_s(5);
		}

		else {
			/*clear both temp passwords for security reasons*/
			memset(password_temp1,'\0',password_length);
			memset(password_temp2,'\0',password_length);
			send_data("\aMISTYPE!");
		}

	} while(password_set_flag==0);

}

void input(unsigned char password_temp[]) {

	unsigned char password_counter,keystroke;
	bit user_confirm_flag=0;
	memset(password_temp,'\0',password_length);
	send_data("\nN-CLR Y-CONFIRM");

	while(user_confirm_flag==0)
	{ 
		for(password_counter=0;password_counter<password_length;password_counter++) {

			keystroke=keyboard_scan();

			switch(keystroke) {
				
				case 'y':
				user_confirm_flag=1;// input finished
				break;
				
				case 'n':
				memset(password_temp,0,password_length);
				password_counter=0;
				send_data("\aCLEARED!");
				send_data("\nN-CLR Y-CONFIRM");
				
				case 0xff:// 0xff stands for null, maybe I should use \0 as null
				password_counter--;
				break;

				default:
				password_temp[password_counter]=keystroke;
				beep_ms(50);
				break;
			}
			if(user_confirm_flag==1) break;
		}
	}
}


void initialize_MCU() { 

	/* initializing timer 0 */
	EA=1;// enable interrupt
	ET0=1;//enable timer 0 interrupt
	TMOD=0x02;//set timer 0 to mode 2
	TH0=0x49;
	TL0=0x49;//set TL0 to 0x49=256-183
	TR0=1;//starts timer 0
	/* Oscillator frequency is 10.969MHz, that makes a 914083Hz system clock. 
	Notice that 914083Hz/183 is 4994.99, So 5 overflows make up 1ms*/
	
	/* First, disable the seven-segment display, we'll use 1602 LCD display */
	chip_select=0;
	segment_select=0;
	
	/* Initializing LCD */
	send_command(0x38);//set display mode
	send_command(0x0c);//turn on display, define cursor behavior
	send_command(0x06);//further define cursor behavior
	send_command(0x01);//clear screen
}

/*send command according to 1602 manualL*/
void send_command(unsigned char command) {
	lcdrs=0;
	lcd_bus=command;
	delay_ms(5);
	lcden=1;
	delay_ms(5);
	lcden=0;
}

/*send_data() displays the input string display_data on lcd, notice that string cannot contain more than 32 characters*/
void send_data(unsigned char display_data[]) {
	
	unsigned char counter;
	send_command(0x80+0x00);
	for(counter=0;counter<strlen(display_data);counter++) {

		// control typography
		if(counter==16) send_command(0x80+0x40);
		if(display_data[counter]=='\n') {
			counter++;
			send_command(0x80+0x40);
		}
		if(display_data[counter]=='\a') {
			counter++;
			send_command(0x01);
		}

		lcdrs=1;
		lcd_bus=display_data[counter];
		delay_ms(1);
		lcden=1;
		delay_ms(1);
		lcden=0;
		lcdrs=0;
	}
}

volatile unsigned char keyboard_scan() {

	/*keyboard_scan() function may interfere lcd during display*/	
	unsigned char keyboard_bus_temp;

	for(keyboard_bus=0x0f;;keyboard_bus=0x0f) {// infinite loop until a proper return
		
		keyboard_bus_temp=keyboard_bus;//capture keyboard_bus value to temp variable
		delay_ms(10);// wait for 10ms to ensure we're reading a stable voltage

		if(keyboard_bus_temp!=0x0f) {
			keyboard_bus=0xf0;
			delay_ms(10);
			if(keyboard_bus!=0xf0) {
				keyboard_bus_temp=keyboard_bus_temp+keyboard_bus;
			/*the way this function scans keyboard, adding keyboard_bus_temp and keyboard_bus won't cause carry bit
			so that each one of the single key has a unique sum value, this is very, very good*/			}
			else continue;
			keyboard_bus=0x0f;
			delay_ms(10);
			while(keyboard_bus!=0x0f);// wait until user release the button

			switch(keyboard_bus_temp)
			{
				/*this function runs an infinite loop until keystroke agree with a proper return.
				so as a single keyboard scan program you should put the definitions of the keys
				that you're insterested below*/
				case 0x10:return 7;
				case 0x20:return 8;
				case 0x40:return 9;
				case 0x80:return 'y';
				case 0xee:return 4;
				case 0xde:return 5;
				case 0xbe:return 6;
				case 0x7e:return 'n';
				case 0xec:return 1;
				case 0xdc:return 2;
				case 0xbc:return 3;
				case 0xd8:return 0;
				// case 0xe8:return 'n';// you don't want to mess with P3^4 and P3^5
				// case 0xb8:return 'y';
			}
			continue;
		}
		else continue;
	}
}

/*generate a delay of maximum 63s*/
void delay_s(unsigned char lag_s) {
	lag_s=5*lag_s;
	for(;lag_s>0;lag_s--) {
		delay_ms(50);
	}	
}
/*max 50ms*/
void delay_ms(unsigned char lag_ms) {
	unsigned char overflow_counter;
	overflow_counter=5*lag_ms;
	delay_counter=0;
	while(delay_counter!=overflow_counter);
}

void timer0_delay() interrupt 1 {
	delay_counter++;	
}

void beep_ms(unsigned char lag_ms) {
	beep=0;
	delay_ms(lag_ms);
	beep=1;
}