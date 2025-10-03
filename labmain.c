<<<<<<< HEAD
=======
/*
   Toggle this for (d) demo that halts at 1111 (set to 1), or keep 0 for (h)(the full clock with button/switch control):
*/
#define STOP_AFTER_START_SEQUENCE 0 // (d)


/*-----------------------Standard headers--------------------------------*/


#include <stdint.h>   // (a)(b)
#include <stdbool.h>  // (a)(b)


/* ---------------------- Memory-mapped I/O ------------------------- */
#define LEDS_ADDR      0x04000000u  // 10 LEDs base address (c)
#define SWITCHES_ADDR  0x04000010u  // 10 toggle switches base address (f)
#define DISP_BASE      0x04000050u // first 7-segment display base adress (e)
#define DISP_STRIDE    0x10u      // each next display is +0x10 (e)
#define BUTTON_ADDR    0x040000d0u   // push-button #2 adress (g)


#define LEDS     ((volatile unsigned int*) LEDS_ADDR) // volatile tells the compiler: this can change outside the program,-> (Volatile MMIO pointer to LEDs (c))
#define SWITCHES ((volatile unsigned int*) SWITCHES_ADDR) //-> writing/reading through these pointers actually talks to the hardware. (Volatile MMIO pointer to switches (f))
#define BUTTON   ((volatile unsigned int*) BUTTON_ADDR) //-> writing/reading through these pointers actually talks to the hardware. (Volatile MMIO pointer to button (g))






/*----------------------------- Lab1 routines used in Lab3-----------------------------------------*/
/* these functions lives in other files, here we call them */
extern void print(const char*);           //UART prints string from Lab1 (a)(b)
extern void print_dec(unsigned int);      //UART print unsigned integer in decimal form Lab1 (a)(b)
extern void display_string(char*);        //ASCII time output (terminal/console) (a)(b)
extern void time2string(char*, int);      // convert mytime to string (a)(b)
extern void tick(int*);                   // increment mytime by one “second” (a)(b)
extern void delay(int);                   // approximate 1s delay scaling (a)(b)
extern int  nextprime(int);               // not used in A1


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
  //this is a low level and  expects a segment pattern not a digit (e)
}


/* Helper: map 0–9 to active-low 7-seg patterns (bit0=a..bit6=g, bit7=dp).
   Writing 0 lights a segment. These are standard common-anode patterns. */
   //digit--> segment lookup table (active-low) (e)
   //handy table: index 0-9 gives you the 7-seg pattern for that digit, dp =decimal point. 0 turns a segment ON
static const unsigned char SEG_DIGIT[10] = {
  0xC0, /* 0 */
  0xF9, /* 1 */
  0xA4, /* 2 */
  0xB0, /* 3 */
  0x99, /* 4 */
  0x92, /* 5 */
  0x82, /* 6 */
  0xF8, /* 7 */
  0x80, /* 8 */
  0x90  /* 9 */
};


// helper to write a single digit to a display
// validates inputs the calls set_displays with the correct segment value
static void set_display_digit(int display_number, int digit) { // (e)
  if (display_number < 0 || display_number > 5) return; // guard index (e)
  if (digit < 0 || digit > 9) return;                   // guard digit (e)
  set_displays(display_number, SEG_DIGIT[digit]);       // map digit to segments (e)
}




   //(f) read the 10 toggle switches (SW0..SW9)
   // reads the switch register and keeps the lowest 10 bits (sw0..Sw9)
   
int get_sw(void) {
  return (int)(*SWITCHES & 0x3FF); //mask to 10 bits (f)
}


   //(g) read the second push-button
   // reads the button register and keeps bit 0. Returns 1 when pressed
   
int get_btn(void) {
  return (int)(*BUTTON & 0x1); // 1 if pressed, else 0 (g)
}


/*
   (d) startup: 4-bit binary counter on LEDs 0–3
   - (d) says “stop when all first 4 LEDs are 1”. We support that with
     STOP_AFTER_START_SEQUENCE == 1, otherwise clear and continue. For (h) we return and continue.
    */
static void start_sequence(void) { //(d)
  set_leds(0); // clears leds, start at 0000 on LEDs (d)
  for (unsigned i = 0; i < 16; ++i) { // counts from 0-15 on the first 4 leds (d)
    set_leds(i & 0xF);  //show 0000..1111 on LEDs 0–3 (d)
    delay(2);           // each step waits " about a second" (scale from Lab1) (d)
  }


#if STOP_AFTER_START_SEQUENCE
  for (;;); // stop (halt) when 1111 is reached (d)
#else
  /* For (h): continue program after intro */
  set_leds(0); // otherwise, it clears the LEDs and proceed to main loop (h)(d)
#endif
}


/*
   (h) show HH:MM:SS on displays, button+switch to set fields
   - Displays 0..5 are assumed left..right:
       [0][1] hours, [2][3] minutes, [4][5] seconds
   - Field select via SW9..SW8:
       01 -> seconds, 10 -> minutes, 11 -> hours
       00 -> no change
   - Value via SW5..SW0 (0..63)
   - Use SW7 to exit the program (one of the “remaining switches”).
    */
static void show_time_on_displays(int hours, int minutes, int seconds) { // (h)
  /* Rightmost pair (HEX0, HEX1) = seconds */
  set_display_digit(1, (seconds / 10) % 10);  // HEX1 = tens of seconds (h)
  set_display_digit(0,  seconds % 10);        // HEX0 = ones of seconds (h)


  /* Middle pair (HEX2, HEX3) = minutes */
  set_display_digit(3, (minutes / 10) % 10);  // HEX3 = tens of minutes (h)
  set_display_digit(2,  minutes % 10);        // HEX2 = ones of minutes (h)


  /* Left pair (HEX4, HEX5) = hours */
  set_display_digit(5, (hours   / 10) % 10);  // HEX5 = tens of hours (h)
  set_display_digit(4,  hours   % 10);        // HEX4 = ones of hours (h)
}




void labinit(void) { // Optional init hook (a)(b)
  // No hardware init required for A1 (a)(b)
 
}


/*--------------------------------main: integrates (a)-(h)----------------------------------*/


int main(void) { // (a)-(h)
  labinit(); // placeholder for future labs (a)(b)


  /* (d) startup LED binary counter intro */
  start_sequence(); //runs the 4-LED from (d)


#if !STOP_AFTER_START_SEQUENCE // if we didn't halt
  /* (h) running clock with 7-seg + button/switch control */
  int hours = 0, minutes = 0, seconds = 0; // intialize the time to 0, software clock state (h)


  while (1) { // main polling loop (h)
    /* Update 7-seg time view */
    show_time_on_displays(hours, minutes, seconds); // draws HH:MM:SS on the six displays (h)


    // this is the teacher's ASCII clock (Lab1 functionality)
    time2string(textstring, mytime);   // convert time to string (a)(b)
    display_string(textstring);        // print to terminal (a)(b)
    print("\n");                       // newline (a)(b)
   
    /* Approximate 1 “second” tick */
    delay(2); // wait about a second (scale from Lab1) (a)(b)(h)
    tick(&mytime); //increament mytime (Lab1 routine) (a)(b)


    /* Advance HH:MM:SS used on 7-seg by 1 second*/
    if (++seconds >= 60) { // roll seconds (h)
      seconds = 0;
      if (++minutes >= 60) { // roll minutes (h)
        minutes = 0;
        hours = (hours + 1) % 24; // roll hours (h)
      }
    }


    /* If BTN is pressed, read switches and update selected field */
    if (get_btn()) {       // gate edits by button press (g)(h)
      int sw  = get_sw();  // read all switches (f)(h)
      int sel = (sw >> 8) & 0x3;   // reads SW9..SW8 (h)
      int val =  sw & 0x3F; // SW5..SW0: 6-bit value (h)
   /* sel= (sw >> 8)& 0x3:
      01: set seconds
      10: set minutes
      11: set hours
      00: do nothing
   */


      /* Exit using SW7 (bit 7) — one of the remaining switches */
      if (sw & (1 << 7)) { //if sw7 is up we break the loop (h)
        break; // end program (h)
      }




      /*Apply selected edit with bounds*/
      if (sel == 0x1) {          // 01 -> seconds (h)
        seconds = val % 60; // set the chosen fields with bounds (h)
      } else if (sel == 0x2) {   // 10 -> minutes (h)
        minutes = val % 60;
      } else if (sel == 0x3) {   // 11 -> hours (h)
        hours = val % 24;
      }
      // sel == 00: do nothing (h)
    }
  } //After breaking, we turn all LEDs on and spin forever


  /* Optional: signal end — all LEDs on, then halt */
  set_leds(0x3FF);  // turn on all 10 LEDs (h)
  for(;;);      // stop here (h)
#endif


  return 0; // (a)(b)
}


/*1-How many “seconds” in theory have passed after the start sequence developed in part (d)?
  15 seconds. It start at 0 and increament the 4-bit value once per second until it reached 1111


2- In the generated assembly code, in which RISC-V register will the return values from
functions get_btn and get_sw be placed in. You should be able to answer this question
without debugging the generated assembly code.
  a+(x10). Per the standard RISC-V calling convention, a function’s (single word) return value is placed in register a0.
  Since both functions return an int, the value will be in a0 when they return.
*/

>>>>>>> origin/main
