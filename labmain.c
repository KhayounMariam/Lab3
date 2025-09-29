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

//Globals from lab text
int mytime = 0x5957;
char textstring[] = "text, more text, and even more text!";

/* Below is the function that will be called when an interrupt is triggered. */
void handle_interrupt(unsigned cause) 
{}

/* Add your code here for initializing interrupts. */
void labinit(void)
{}

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
