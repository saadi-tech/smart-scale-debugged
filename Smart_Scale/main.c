
#include<avr/io.h>

#define  F_CPU 8000000UL

#define ADMUX_ADCMASK  ((1 << MUX3)|(1 << MUX2)|(1 << MUX1)|(1 << MUX0))
#define ADMUX_REFMASK  ((1 << REFS1)|(1 << REFS0))

#define ADMUX_REF_AREF ((0 << REFS1)|(0 << REFS0))
#define ADMUX_REF_AVCC ((0 << REFS1)|(1 << REFS0))
#define ADMUX_REF_RESV ((1 << REFS1)|(0 << REFS0))
#define ADMUX_REF_VBG  ((1 << REFS1)|(1 << REFS0))

#define ADMUX_ADC_VBG  ((1 << MUX3)|(1 << MUX2)|(1 << MUX1)|(0 << MUX0))

#include "hx711.h"

#include<util/delay.h>
#include <stdio.h>
#include "spi.h"
#include <stdlib.h>
#include "st7735.h"
#include "st7735_gfx.h"
#include "st7735_font.h"
#include <string.h>
#include "logo_bw.h"
#include "free_sans.h"
#include <stdio.h>
#include <stdbool.h>
#include <avr/interrupt.h>
//hx711 lib example


//include uart
#define UART_BAUD_RATE 9600
//#include "uart.h"

#include "hx711.h"

//defines running modes
#define HX711_MODERUNNING 1
#define HX711_MODECALIBRATION1OF2 2
#define HX711_MODECALIBRATION2OF2 3
#define HX711_MODECURRENT HX711_MODERUNNING

//2 step calibration procedure
//set the mode to calibration step 1
//read the calibration offset leaving the load cell with no weight
//set the offset to value read
//put a know weight on the load cell and set calibrationweight value
//run the calibration stes 2 of 2
//read the calibration scale
//set the scale to value read
void clear_screen();
void show_main_screen();
char read_button_press();
void show_button_pressed(char);
uint16_t measure_supply();



//set the gain
int8_t gain = HX711_GAINCHANNELA128;


//set the offset
int32_t offset = 8395583;    // 8395583

//set the scale
double scale = 800.0;


uint32_t last_Reading,difference = 0;
char string[20];
volatile double current_scale =0.0;


char button_pressed;   //1-> Enter... 2-> Back  3-> Next   4-> Previous

char string[20];   //string for writing formatting on the LCD.
char menu[10][16];


float ps,last_ps =0;
unsigned long ms_count=0;
float sgp_m = 0;
float sgp_i = 0;

float sgl = 1.0;


float solid_sg = 1.5;
char enter = 2;
char back = 1;
char next = 4;
char previous = 3;
char current_units = 'm' ; // m -> Metric  i -> Imperial
char measurement_type = '%';  // % solid density    .... s slurry density.
uint16_t sample_point = 0;
char exited = 0 ;
uint8_t menu_index = 0;
bool showing_mainscreen = false;
char printbuff[100];

int main(void) {
	
	//init uart
	uart_init(9600);
	DDRC = DDRC & (0b1000011);
	PORTC |= (0b111100);
	spi_init();
	st7735_init();
	st7735_set_orientation(ST7735_LANDSCAPE);
	clear_screen();
	//init hx711
	hx711_init(gain, scale, offset);
	
	ADMUX = (1 << REFS1) | (1 << REFS0)| (0 << ADLAR)| (0 << MUX3) | (0 << MUX2) | (0 << MUX1) | (0 << MUX0);
	ADCSRA = (1 << ADEN)| (0 << ADSC)| (0 << ADATE)| (1 << ADPS2)|(0 << ADPS1)|(1 << ADPS0);
	ADCSRA |= (1 << ADSC);         // start dummy conversion
	while (ADCSRA & (1 << ADSC));  // wait for dummy to finish
	
	start_clock();
	
	sei();
	showing_mainscreen = true;
	show_main_screen();
		while(1){
			//calculate_percent_solid();
			showing_mainscreen = true;
			button_pressed = read_button_press();
			
			if (button_pressed == enter){
				showing_mainscreen = false;
				//if enter is pressed...
				set_menu(0);
				menu_index = 0;
				show_option(menu_index);
				
				
				while(exited == 0 ){
					button_pressed = read_button_press();
					
					
					
					switch (button_pressed){
						case (4):
						if(menu_index<5){
							menu_index++;
							show_option(menu_index);
						}
						break;
						
						case(3):
						if(menu_index>0){
							menu_index --;
							show_option(menu_index);
						}
						break;
						
						case(1):
						exited = 1;
						break;
						
						case(2):
						
						switch (menu_index){
							case 0:
							//Sample point....
							select_sample_point();
							show_option(menu_index);
							break;
							case 1:
							//Set solid SG
							set_solid_sg();
							show_option(menu_index);
							break;
							
							case 2:
							//Set zero
							set_zero_scale();
							show_option(menu_index);
							break;
							
							case 3:
							//Set measurement type
							set_measurement_type();
							show_option(menu_index);
							break;
							
							case 4:
							//Set UNITS
							set_units();
							show_option(menu_index);
							break;
							
							case 5:
							show_battery_screen();
							show_option(menu_index);
							break;
							
						}
						
						break;
						
						
					}
					
				}
				exited = 0;
				menu_index = 0;
				show_main_screen();
				showing_mainscreen = true;
				//calculate_percent_solid();
				

			}
			
		}

	
}


void clear_screen(){
	st7735_fill_rect(0, 0, 160, 128, ST7735_COLOR_BLACK);
}




void show_main_screen(){
	clear_screen();
	
	
	dtostrf(ps,4, 2, string);
	strcat(string," %");
	
	st7735_draw_text(0, 32, string, &FreeSans, 2, ST7735_COLOR_WHITE);
	
	
	dtostrf(solid_sg,4, 2, string);
	if (current_units == 'm'){
	
		strcat(string,"g/cm3");
	}
	if (current_units == 'i'){
		strcat(string,"lb/gal");
	}
	
	st7735_draw_text(0, 70, "Solid SG :", &FreeSans, 1, ST7735_COLOR_WHITE);
	st7735_draw_text(40, 85, string, &FreeSans, 1, ST7735_COLOR_WHITE);
	
	itoa(sample_point,string,10);
	
	st7735_draw_text(0, 118, "Sample pt :", &FreeSans, 1, ST7735_COLOR_WHITE);
	st7735_draw_text(110, 118, string, &FreeSans, 1, ST7735_COLOR_WHITE);
	
	
	
}

void calculate_percent_solid(double kgs){
	
	
	
	sgp_m = kgs;
	
	if (sgp_m > 1 && sgp_m < solid_sg){
	ps = ((sgl - sgp_m)*solid_sg)/((sgl - solid_sg)*sgp_m) * 100;
	serial_write("PS: % ");
	dtostrf(ps,4, 2, string);
	serial_writeln(string);
	}
	else{
		ps = 0.00;
	}
	
	
	dtostrf(kgs,4, 4, string);
	serial_write("Weight: (KG) ");
	serial_writeln(string);
	
	//if(showing_mainscreen && ps != last_ps){
		if(showing_mainscreen){
		st7735_fill_rect(0, 0, 160, 40, ST7735_COLOR_BLACK);
		dtostrf(ps,4, 2, string);
		strcat(string," %");
	
		last_ps = ps;
	
		st7735_draw_text(0, 32, string, &FreeSans, 2, ST7735_COLOR_WHITE);
	}
	
}

char read_button_press(){
	
	int delay = 70;
	if (  !(PINC & (1 << PINC5))  ){
		//1 pressed...
		_delay_ms(delay);
		return 1;
		
	}
	if (  !(PINC & (1 << PINC4))  ){
		_delay_ms(delay);
		//2 pressed...
		return 2;
		
	}
	if (  !(PINC & (1 << PINC3))  ){
		//3 pressed...
		_delay_ms(delay);
		return 3;
		
	}
	if (  !(PINC & (1 << PINC2))  ){
		//4 pressed...
		_delay_ms(delay);
		return 4;
		
	}
	
	return 0;
	
}

void show_button_pressed(char index){
	
	clear_screen();
	
	switch (index)
	{
		
		case 1:
		st7735_draw_text(20, 75, "BACK", &FreeSans, 2, ST7735_COLOR_WHITE);
		break;
		
		case 2:
		st7735_draw_text(20, 75, "ENTER", &FreeSans, 2, ST7735_COLOR_WHITE);
		break;
		
		case 3:
		st7735_draw_text(20, 75, "PREV.", &FreeSans, 2, ST7735_COLOR_WHITE);
		break;
		
		case 4:
		st7735_draw_text(20, 75, "NEXT", &FreeSans, 2, ST7735_COLOR_WHITE);
		break;
		
		
	}
	_delay_ms(250);
	
}


void set_menu(char index){
	
	switch (index){
		
		case 0:
		strcpy(menu[0],"Sample point");
		strcpy(menu[1],"Set Solid SG");
		strcpy(menu[2],"Zero");
		strcpy(menu[3],"Measurement Type");
		strcpy(menu[4],"Set Units");
		strcpy(menu[5],"Battery Status");
		
		break;
		
		
		
	}
	
	
}

void set_solid_sg(){
		clear_screen();
		dtostrf(solid_sg,4, 2, string);
		st7735_draw_text(0, 15, "Solid SG:", &FreeSans, 1, ST7735_COLOR_WHITE);
		st7735_draw_text(35, 75, string, &FreeSans, 1, ST7735_COLOR_WHITE);
		
		while(1){
			char button_pressed = read_button_press();
			
			switch(button_pressed){
				case 1:
				//back
				return;
				break;
				
				case 2:
				//st7735_draw_text(20, 75, "ENTER", &FreeSans, 2, ST7735_COLOR_WHITE);
				clear_screen();
				sprintf(string, "Solid-sg set OK!", solid_sg);
				st7735_draw_text(0, 75, string , &FreeSans, 1, ST7735_COLOR_WHITE);
				_delay_ms(700);
				return;
				
				break;
				
				case 3:
				if(solid_sg >1.00){
					solid_sg -= 0.05;
				}
				clear_screen();
				dtostrf(solid_sg,4, 2, string);
				st7735_draw_text(0, 15, "Solid SG:", &FreeSans, 1, ST7735_COLOR_WHITE);
				st7735_draw_text(35, 75, string, &FreeSans, 1, ST7735_COLOR_WHITE);
				
				
				break;
				
				case 4:
				if(solid_sg <5.00){
					solid_sg += 0.05;
				}
				clear_screen();
				dtostrf(solid_sg,4, 2, string);
				st7735_draw_text(0, 15, "Solid SG:", &FreeSans, 1, ST7735_COLOR_WHITE);
				st7735_draw_text(35, 75, string, &FreeSans, 1, ST7735_COLOR_WHITE);
				
				break;
				
			}
		}
}


void set_zero_scale(){
clear_screen();

st7735_draw_text(0, 15, "Set to zero:", &FreeSans, 1, ST7735_COLOR_WHITE);
st7735_draw_text(0, 75, "Pls make sure the", &FreeSans, 1, ST7735_COLOR_WHITE);
st7735_draw_text(0, 95, "bucket is empty.", &FreeSans, 1, ST7735_COLOR_WHITE);
while(1){
	char button_pressed = read_button_press();
	
	switch(button_pressed){
		case 1:
		//back
		return;
		break;
		
		case 2:
		//st7735_draw_text(20, 75, "ENTER", &FreeSans, 2, ST7735_COLOR_WHITE);
		clear_screen();
		
		st7735_draw_text(0, 75, "Scale ZERO set." , &FreeSans, 1, ST7735_COLOR_WHITE);
		_delay_ms(700);
		return;
		
		break;
		
		
		
	}
}
}

void set_measurement_type(){
	clear_screen();
	char index =0;
	char buffer[30];
	
		
	char meas_types[2][15];
	strcpy(meas_types[0], "% Solids");
	strcpy(meas_types[1], "Slurry density");
	
	st7735_draw_text(0, 15, "Measurement type:", &FreeSans, 1, ST7735_COLOR_WHITE);
	st7735_draw_text(0, 75, meas_types[index], &FreeSans, 1, ST7735_COLOR_WHITE);
	
	while(1){
		char button_pressed = read_button_press();
		
		switch(button_pressed){
			case 1:
			//back
			return;
			break;
			
			case 2:
			//st7735_draw_text(20, 75, "ENTER", &FreeSans, 2, ST7735_COLOR_WHITE);
			clear_screen();
			st7735_draw_text(0, 15, "Measurement type:", &FreeSans, 1, ST7735_COLOR_WHITE);
			st7735_draw_text(0, 75, meas_types[index], &FreeSans, 1, ST7735_COLOR_WHITE);
			
			if (index ==0){
				measurement_type = '%';
				
			}
			if (index == 1){
				measurement_type = 's';
			}
			_delay_ms(700);
			return;
			
			break;
			
			case 3:
			if(index >0){
				index--;
			}
			clear_screen();
			st7735_draw_text(0, 15, "Measurement type:", &FreeSans, 1, ST7735_COLOR_WHITE);
			st7735_draw_text(0, 75, meas_types[index], &FreeSans, 1, ST7735_COLOR_WHITE);
			
			
			break;
			
			case 4:
			if(index < 1){
				index++;
			}
			clear_screen();
			st7735_draw_text(0, 15, "Measurement type:", &FreeSans, 1, ST7735_COLOR_WHITE);
			st7735_draw_text(0, 75, meas_types[index], &FreeSans, 1, ST7735_COLOR_WHITE);
			
			break;
			
		}
	}
}


void set_units(){
	clear_screen();
	char index =0;
	char buffer[30];
	
	
	char units[2][15];
	strcpy(units[0], "Metric (g/cm3)");
	strcpy(units[1], "Imperial (lb/gal)");
	
	st7735_draw_text(0, 15, "Units:", &FreeSans, 1, ST7735_COLOR_WHITE);
	st7735_draw_text(0, 75, units[index], &FreeSans, 1, ST7735_COLOR_WHITE);
	
	while(1){
		char button_pressed = read_button_press();
		
		switch(button_pressed){
			case 1:
				//back
				return;
				break;
			
			case 2:
				
			
				if (index ==0){
				
					clear_screen();
					st7735_draw_text(0, 15, "Units:", &FreeSans, 1, ST7735_COLOR_WHITE);
					st7735_draw_text(0, 75, "Good choice!", &FreeSans, 1, ST7735_COLOR_WHITE);
					current_units = 'm';
				
				}
				if (index == 1){
				
					clear_screen();
					st7735_draw_text(0, 15, "Units:", &FreeSans, 1, ST7735_COLOR_WHITE);
					st7735_draw_text(0, 75, "Are you sure?", &FreeSans, 1, ST7735_COLOR_WHITE);
					while(1){
						button_pressed = read_button_press();
						if (button_pressed == 2){
							//enter
							clear_screen();
							st7735_draw_text(0, 15, "Units selected:", &FreeSans, 1, ST7735_COLOR_WHITE);
							st7735_draw_text(0, 75, "Imperial units", &FreeSans, 1, ST7735_COLOR_WHITE);
							current_units = 'i';
							break;
						}
						if (button_pressed == 1){
							clear_screen();
							st7735_draw_text(0, 15, "Units selected:", &FreeSans, 1, ST7735_COLOR_WHITE);
							st7735_draw_text(0, 75, "Metric", &FreeSans, 1, ST7735_COLOR_WHITE);
							current_units = 'm';
							break;
						}
				 }
				
				}
				_delay_ms(700);
				return;
			
				break;
			
			case 3:
			if(index >0){
				index--;
			}
			clear_screen();
			st7735_draw_text(0, 15, "Units:", &FreeSans, 1, ST7735_COLOR_WHITE);
			st7735_draw_text(0, 75, units[index], &FreeSans, 1, ST7735_COLOR_WHITE);
			
			
			break;
			
			case 4:
			if(index < 1){
				index++;
			}
			clear_screen();
			st7735_draw_text(0, 15, "Units:", &FreeSans, 1, ST7735_COLOR_WHITE);
			st7735_draw_text(0, 75, units[index], &FreeSans, 1, ST7735_COLOR_WHITE);
			
			break;
			
		}
	}
}
void show_option(char index){
	clear_screen();
	st7735_draw_text(70, 15, "Select", &FreeSans, 1, ST7735_COLOR_WHITE);
	st7735_draw_text(5, 75, menu[index], &FreeSans, 1, ST7735_COLOR_WHITE);
}

void show_sample_point_menu(){
	clear_screen();
	
}

void print_battery(){
	uint16_t battery = measure_supply();
	
	serial_write("Battery:");
	
	itoa(battery, string,10);
	serial_writeln(string);
}

void show_battery_screen(){
	show_battery_status();
	
	while(1){
		button_pressed = read_button_press();
		if (button_pressed == back){
			return;
		}
		
	}
}
void show_battery_status(){
	uint16_t battery = measure_supply();
	
	int percentage = ((battery-3300)/1700.0)*100;
	itoa(percentage, string,10);
	
	
	clear_screen();
	
	
	strcat(string,"%");
	st7735_draw_text(50, 42, string, &FreeSans, 2, ST7735_COLOR_WHITE);
	
	char clr = 0; //0 -> Red, 1 -> Yellow, 2 ->Green
	st7735_fill_rect(50,50,52,25,ST7735_COLOR_WHITE);
	if(percentage<33){
		st7735_fill_rect(51,51,16,23,ST7735_COLOR_RED);
	}
	if (percentage>=33 && percentage<66){
		st7735_fill_rect(51,51,16,23,ST7735_COLOR_YELLOW);
		st7735_fill_rect(68,51,16,23,ST7735_COLOR_YELLOW);
	}
	if (percentage>=66){
		st7735_fill_rect(51,51,16,23,ST7735_COLOR_GREEN);
		st7735_fill_rect(68,51,16,23,ST7735_COLOR_GREEN);
		st7735_fill_rect(85,51,16,23,ST7735_COLOR_GREEN);
	}

}



void select_sample_point(){
	
	clear_screen();
	itoa(sample_point,string,10);
	st7735_draw_text(0, 15, "Sample pt:", &FreeSans, 1, ST7735_COLOR_WHITE);
	st7735_draw_text(35, 75, string, &FreeSans, 1, ST7735_COLOR_WHITE);
	
	while(1){
		char button_pressed = read_button_press();
		
		switch(button_pressed){
		case 1:
		//back
			return;
			break;
		
		case 2:
		//st7735_draw_text(20, 75, "ENTER", &FreeSans, 2, ST7735_COLOR_WHITE);
			clear_screen();
			sprintf(string, "Sp: %d OK!", sample_point);
			st7735_draw_text(0, 75, string , &FreeSans, 1, ST7735_COLOR_WHITE);
			_delay_ms(700);
			return;
		
			break;
		
		case 3:
			if(sample_point >1){
				sample_point--;
			}
			clear_screen();
			itoa(sample_point,string,10);
			st7735_draw_text(0, 15, "Sample pt:", &FreeSans, 1, ST7735_COLOR_WHITE);
			st7735_draw_text(35, 75, string, &FreeSans, 1, ST7735_COLOR_WHITE);
			
			
			break;
		
		case 4:
			if(sample_point <500){
				sample_point++;
			}
			clear_screen();
			itoa(sample_point,string,10);
			st7735_draw_text(0, 15, "Sample pt:", &FreeSans, 1, ST7735_COLOR_WHITE);
			st7735_draw_text(35, 75, string, &FreeSans, 1, ST7735_COLOR_WHITE);
			
			break;
		
		}
	}
}

// measure supply voltage in mV
uint16_t measure_supply(void)
{
	ADMUX &= ~(ADMUX_REFMASK | ADMUX_ADCMASK);
	ADMUX |= ADMUX_REF_AREF;      // select AVCC as reference
	ADMUX |= ADMUX_ADC_VBG;       // measure bandgap reference voltage
	
	_delay_us(500);               // a delay rather than a dummy measurement is needed to give a stable reading!
	ADCSRA |= (1 << ADSC);        // start conversion
	while (ADCSRA & (1 << ADSC)); // wait to finish
	
	return (1100UL*1023/ADC);     // AVcc = Vbg/ADC*1023 = 1.1V*1023/ADC
}




//-------------------------------------------------------------------------
//TIMER Interrupt using TIMER1 for 1ms
void start_clock(){
	TCCR1A = 0x00;
	
	TIMSK1 = 0x01;
	TCNT1 = 64536;
	TCCR1B = 0b00000010;
}

//==============================
void stop_clock(){
	TCNT1 = 63536;
	TCCR1B = 0b00000000;
}

//=============================
ISR (TIMER1_OVF_vect){
	
	//1ms timer interrupt..
	//counting millisecs.
ms_count++;

if(ms_count == 1500 ){

	//get weight
	if(showing_mainscreen){
	double weight = hx711_getweight();
	//dtostrf(weight, 3, 3, printbuff);
	//serial_write("Weight: "); serial_write(printbuff); serial_write("kg"); serial_write("\r\n");

	//serial_write("\r\n");
	calculate_percent_solid(weight);
	}
	ms_count = 0;
	}
	TCNT1 = 64536;
}
//============================