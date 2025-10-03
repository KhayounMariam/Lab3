/* main.c

   This file written 2024 by Artur Podobas and Pedro Antunes

   For copyright and licensing, see file COPYING */
#define LEDS_ADDR 0x04000000u
#define REG32(a) (*(volatile unsigned int*)(a))

//Turn LEDs on/off: only 10 LSBs matter
void set_leds(int led_mask)
{ REG32(LEDS_ADDR) = led_mask & 0x3FF; }

/* Below functions are external and found in other files. */
extern void print(const char*);
extern void print_dec(unsigned int);
extern void display_string(char*);
extern void time2string(char*,int);
extern void tick(int*);
extern void delay(int);
extern int nextprime( int );

/* ----------------------  globals ------------------ */
int mytime = 0x0000;// is used by time2string and tick to show a text clock, initial time HHMM (a)(b)
char textstring[] = "text, more text, and even more text!"; // UART buffer (a)(b

/* ---------------------- A3 placeholder (empty in A1) -------------- */
void handle_interrupt(unsigned cause)
{
  /* Not used in Assignment 1 */
}


//(c) LED output, control the leds via MMIO 
    
void set_leds(int led_mask) {//write the Led register, each bit turns one LED on/off
  /* Only 10 LEDs exist: keep LSB 10 bits */
  *LEDS = (unsigned int)(led_mask & 0x3FF); // write MMIO to drive LEDs 0..9 (c)
}


   //(e) write raw values to a 7-segement display (active-low segments)
   
void set_displays(int display_number, int value) { // (e)
  if (display_number < 0 || display_number > 5) //display_number 0-5 selects which of the six 7-seg displays (e)
     return;
     //we compute it's adress:base+ index *stride (0x10) (e)
  volatile unsigned int* disp = (volatile unsigned int*)(DISP_BASE + (unsigned)display_number * DISP_STRIDE);
  *disp = (unsigned int)value; // we write value directly. On this board, writing 0 lights a segement (active-low) (e)
  //this is a low level and expects a segment pattern not a digit (e)
}

/* Your code goes into main as well as any needed functions. */
int main(void) {
  // Call labinit()
  labinit();

/* d) start LED counter at 0 on the first 4 LEDs */
  int led_counter = 0;
  set_leds(0);
  
  // Enter a forever loop
  while (1) {
    time2string( textstring, mytime ); // Converts mytime to string
    display_string( textstring ); //Print out the string 'textstring'

    //approx "1 second" delay - keep 2 to match the timetemplate 
    delay( 2 );          // Delays 1 sec (adjust this value)
    tick( &mytime );     // Ticks the clock once

    /* increment 4-bit counter on LEDs 0..3 */
    led_counter = (led_counter + 1) & 0xF;   // 0..15
    set_leds(led_counter);

    /* stop when first 4 LEDs are all on (1111) */
    if (led_counter == 0xF) {
      break;
  }
}

/* halt (leave LEDs as-is) */
  for (;;){}

  return 0;
}
