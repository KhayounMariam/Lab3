/* main.c

   This file written 2024 by Artur Podobas and Pedro Antunes

   For copyright and licensing, see file COPYING */
#define LEDS_ADDR 0x04000000u
#define REG32(a) (*(volatile unsigned int*)(a))

//Turn LEDs on/off: only 10 LSBs matter
void set_leds(int led_mask)
{ REG32(LEDS_ADDR) = led_mask & 0x3FF; }

// Common 7-seg encoding (a..g in bits 0..6), active-high reference
static const unsigned char SEG_ON[16] = {
  0x3F, /*0*/ 0x06, /*1*/ 0x5B, /*2*/ 0x4F, /*3*/ 0x66, /*4*/
  0x6D, /*5*/ 0x7D, /*6*/ 0x07, /*7*/ 0x7F, /*8*/ 0x6F, /*9*/
  0x77, /*A*/ 0x7C, /*b*/ 0x39, /*C*/ 0x5E, /*d*/ 0x79, /*E*/ 0x71 /*F*/
};

// display_number: 0..5, value: 0..15 (we'll use 0..9)
void set_displays(int display_number, int value) {  // part (e)
  if (display_number < 0 || display_number > 5) return;
  if (value < 0) value = 0;
  if (value > 15) value = 15;
  unsigned addr = DISP_BASE + (unsigned)display_number * DISP_STRIDE;

  unsigned char active_low = (~SEG_ON[value]) & 0x7F; // bit=0 lights segment
  REG32(addr) = active_low;
}

static void show_two_digits(int left_disp, int right_disp, int v) {
  if (v < 0) v = 0;
  if (v > 99) v = 99;
  int tens = (v / 10) % 10;
  int ones = v % 10;
  set_displays(left_disp, tens);
  set_displays(right_disp, ones);
}


int get_sw(void) {                       // part (f)
  return REG32(SWITCHES_ADDR) & 0x3FF;
}

int get_btn(void) {                      // part (g)
  return REG32(BTN2_ADDR) & 0x1;         // returns 1 if pressed (adjust if inverted)
}


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
  labinit();

  /* -------- Part (d) start sequence: count 0..15 on LEDs and stop -------- */
  int led_counter = 0;
  set_leds(0);
  while (1) {
    time2string(textstring, mytime);
    display_string(textstring);
    delay(2);
    tick(&mytime);

    led_counter = (led_counter + 1) & 0xF;   // 0..15
    set_leds(led_counter);
    if (led_counter == 0xF) break;          // stop start sequence here
  }

  /* -------- Part (h): continuous clock on 6 displays + input handling ----- */
  int sec = 0, min = 0, hr = 0;             // hh:mm:ss (hours 0..99)
  for(;;) {
    // 1) one "second" tick
    delay(2);               // keep 2 to match your calibration
    tick(&mytime);          // not required for displays, but fine to keep
    sec++;
    if (sec >= 60) { sec = 0; min++; }
    if (min >= 60) { min = 0; hr = (hr + 1) % 100; }

    // 2) show hh:mm:ss on the 6 displays
    //   seconds -> rightmost pair (1 tens, 0 ones)
    show_two_digits(1, 0, sec);
    //   minutes -> middle pair (3 tens, 2 ones)
    show_two_digits(3, 2, min);
    //   hours   -> leftmost pair (5 tens, 4 ones)
    show_two_digits(5, 4, hr);

    // 3) if button pressed, use switches to set counters
    if (get_btn()) {
      int sw = get_sw();

      int sel = (sw >> 8) & 0x3;   // two left-most switches: bits 9..8
      int val = sw & 0x3F;         // six right-most switches: bits 5..0 (0..63)

      // one of the remaining switches exits program: choose SW7 (bit 7)
      if (sw & (1 << 7)) {
        break;                     // exit infinite loop and end program
      }

      // apply selection: 01=seconds, 10=minutes, 11=hours
      switch (sel) {
        case 0x1:  // 01 -> seconds
          sec = val % 60;
          break;
        case 0x2:  // 10 -> minutes
          min = val % 60;
          break;
        case 0x3:  // 11 -> hours
          hr = val % 100;  // two digits; clamp to 0..99
          break;
        default:
          // 00 -> do nothing
          break;
      }

      // reflect immediately on displays after a set
      show_two_digits(1, 0, sec);
      show_two_digits(3, 2, min);
      show_two_digits(5, 4, hr);
    }
  }

  // Halt after exit-switch is used
  for(;;) {}
}


