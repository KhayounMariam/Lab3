/*
  Mystery House - Interactive Fiction (no VGA)
  Built from Lab3 template style:
  - Uses MMIO LEDs, Switches, Button
  - Uses UART print/printc/print_dec (from dtekv-lib)
  - Uses delay(ms) from timetemplate.S
*/

#include <stdint.h>
#include <stdbool.h>

// Required by boot.S even if interrupts are unused
void handle_interrupt(unsigned cause)
{
    (void)cause;
}



/* ---------------------- Memory-mapped I/O (same as your Lab3) ------------------------- */
#define LEDS_ADDR      0x04000000u
#define SWITCHES_ADDR  0x04000010u
#define BUTTON_ADDR    0x040000d0u

#define LEDS     ((volatile unsigned int*) LEDS_ADDR)
#define SWITCHES ((volatile unsigned int*) SWITCHES_ADDR)
#define BUTTON   ((volatile unsigned int*) BUTTON_ADDR)

/* ---------------------- UART + delay from lab files ------------------ */
extern void print(char*);              // JTAG UART string output :contentReference[oaicite:1]{index=1}
extern void printc(char);              // JTAG UART char output :contentReference[oaicite:2]{index=2}
extern void print_dec(unsigned int);   // print decimal number :contentReference[oaicite:3]{index=3}
extern void delay(int);               // approx 1s delay from timetemplate.S :contentReference[oaicite:4]{index=4}


/* ---------------------- Basic I/O helpers (same as Lab3 style) ------------------ */

// (c) LED output
void set_leds(int led_mask) {
  *LEDS = (unsigned int)(led_mask & 0x3FF); // only 10 LEDs exist :contentReference[oaicite:5]{index=5}
}

// (f) read 10 switches
int get_sw(void) {
  return (int)(*SWITCHES & 0x3FF); // SW0..SW9 :contentReference[oaicite:6]{index=6}
}

// (g) read push button
int get_btn(void) {
  return (int)(*BUTTON & 0x1); // BTN pressed = 1 :contentReference[oaicite:7]{index=7}
}


/* ---------------------- UART input (small new part, needed for commands) ------------------ */
/* We reuse the same JTAG UART registers as dtekv-lib.c uses for printc. :contentReference[oaicite:8]{index=8} */
#define JTAG_UART ((volatile unsigned int*) 0x04000040)
#define JTAG_CTRL ((volatile unsigned int*) 0x04000044)

// try to read one char (non-blocking). returns 1 if got char.
static int uart_try_getc(char *out) {
  unsigned int ctrl = *JTAG_CTRL;
  if (ctrl & 0x00008000u) {          // RVALID
    unsigned int data = *JTAG_UART;
    *out = (char)(data & 0xFF);
    return 1;
  }
  return 0;
}

// blocking getc
static char uart_getc_blocking(void) {
  char c;
  while (!uart_try_getc(&c)) { }
  return c;
}

// blocking line read into buf, echoes input
static void uart_getline(char *buf, int maxlen) {
  int i = 0;
  while (1) {
    char c = uart_getc_blocking();

    if (c == '\r' || c == '\n') {   // end line
      print("\n");
      break;
    }

    // backspace
    if ((c == 8 || c == 127) && i > 0) {
      i--;
      print("\b \b");
      continue;
    }

    if (i < maxlen - 1) {
      buf[i++] = c;
      printc(c);                    // echo
    }
  }
  buf[i] = '\0';
}


/* ---------------------- Game data ------------------ */

#define NUM_ROOMS 9
#define MAX_INPUT 64

typedef struct {
  char *name;
  char *desc;
  int north, south, east, west;   // -1 = no exit
  bool dark;                      // needs flashlight ON
  bool locked;                    // door locked
  char *lock_msg;                // printed if locked

  bool item_flashlight;
  bool item_silver_key;
  bool item_brass_key;
} Room;

static Room rooms[NUM_ROOMS];

static int current_room = 0;
static bool has_flashlight = false;
static bool has_silver_key = false;
static bool has_brass_key = false;
static bool flashlight_on = false;

static int warmth = 6; // LEDs show warmth level


/* ---------------------- Small string helpers (minimal C) ------------------ */

static int streq(char *a, char *b) {
  while (*a && *b) {
    if (*a != *b) return 0;
    a++; b++;
  }
  return (*a == 0 && *b == 0);
}

static void tolower_str(char *s) {
  for (; *s; s++) {
    if (*s >= 'A' && *s <= 'Z') *s = *s - 'A' + 'a';
  }
}

// skip spaces
static char* skip_space(char *s) {
  while (*s == ' ' || *s == '\t') s++;
  return s;
}


/* ---------------------- UI printing ------------------ */

static void print_room(int id) {
  Room *r = &rooms[id];

  print("\n== ");
  print(r->name);
  print(" ==\n");
  print(r->desc);
  print("\n");

  // items
  if (r->item_flashlight || r->item_silver_key || r->item_brass_key) {
    print("Items here:");
    if (r->item_flashlight) print(" flashlight");
    if (r->item_silver_key) print(" silver key");
    if (r->item_brass_key) print(" brass key");
    print("\n");
  }

  // exits
  print("Exits:");
  if (r->north != -1) print(" north");
  if (r->south != -1) print(" south");
  if (r->east  != -1) print(" east");
  if (r->west  != -1) print(" west");
  print("\n");

  print("Warmth: ");
  print_dec((unsigned)warmth);
  print("\n");
}

static void enter_room(int id) {
  current_room = id;
  print_room(id);
}

static void update_warmth_leds(void) {
  int mask = 0;
  for (int i = 0; i < warmth; i++) mask |= (1 << i);
  set_leds(mask);
}


/* ---------------------- Game logic ------------------ */

static int can_enter(int to_id) {
  Room *to = &rooms[to_id];

  if (to->locked) {
    print(to->lock_msg);
    print("\n");
    return 0;
  }
  if (to->dark && !(has_flashlight && flashlight_on)) {
    print("It's too dark to go there. Turn flashlight ON.\n");
    return 0;
  }
  return 1;
}

static void handle_go(char *dir) {
  Room *cur = &rooms[current_room];
  int to = -1;

  if (streq(dir, "north")) to = cur->north;
  else if (streq(dir, "south")) to = cur->south;
  else if (streq(dir, "east")) to = cur->east;
  else if (streq(dir, "west")) to = cur->west;
  else {
    print("Go where? north/south/east/west\n");
    return;
  }

  if (to == -1) {
    print("You can't go that way.\n");
    return;
  }

  if (can_enter(to)) enter_room(to);
}

static void handle_take(char *item) {
  Room *cur = &rooms[current_room];

  if (streq(item, "flashlight")) {
    if (cur->item_flashlight) {
      cur->item_flashlight = false;
      has_flashlight = true;
      print("You pick up the flashlight.\n");
    } else print("No flashlight here.\n");
    return;
  }

  if (streq(item, "silver key") || streq(item, "silver") || streq(item, "key")) {
    if (cur->item_silver_key) {
      cur->item_silver_key = false;
      has_silver_key = true;
      print("You take the silver key.\n");
    } else print("No silver key here.\n");
    return;
  }

  if (streq(item, "brass key") || streq(item, "brass")) {
    if (cur->item_brass_key) {
      cur->item_brass_key = false;
      has_brass_key = true;
      print("You take the brass key.\n");
    } else print("No brass key here.\n");
    return;
  }

  print("Take what?\n");
}

static void handle_use(char *item) {
  Room *cur = &rooms[current_room];

  if (streq(item, "flashlight")) {
    if (!has_flashlight) { print("You don't have a flashlight.\n"); return; }
    flashlight_on = !flashlight_on;
    print("Flashlight ");
    print(flashlight_on ? "ON.\n" : "OFF.\n");
    return;
  }

  // unlock Storage Room (room 7) if adjacent
  if (streq(item, "silver key") || streq(item, "silver") || streq(item, "key")) {
    if (!has_silver_key) { print("You don't have the silver key.\n"); return; }

    if (cur->east == 7 || cur->west == 7 || cur->north == 7 || cur->south == 7) {
      rooms[7].locked = false;
      print("You unlock the Storage Room door.\n");
    } else {
      print("Nothing here to unlock with that.\n");
    }
    return;
  }

  // unlock Exit Door (room 8) if adjacent
  if (streq(item, "brass key") || streq(item, "brass")) {
    if (!has_brass_key) { print("You don't have the brass key.\n"); return; }

    if (cur->east == 8 || cur->west == 8 || cur->north == 8 || cur->south == 8) {
      rooms[8].locked = false;
      print("You hear the Exit Door unlock.\n");
    } else {
      print("Nothing here to unlock with that.\n");
    }
    return;
  }

  print("Use what?\n");
}

static void print_inventory(void) {
  print("You are carrying:\n");
  if (has_flashlight) {
    print("- flashlight (");
    print(flashlight_on ? "ON" : "OFF");
    print(")\n");
  }
  if (has_silver_key) print("- silver key\n");
  if (has_brass_key)  print("- brass key\n");
  if (!has_flashlight && !has_silver_key && !has_brass_key)
    print("- nothing\n");
}

static int check_end(void) {
  if (warmth <= 0) {
    print("\nYou collapse from the cold. Game over.\n");
    return 1;
  }
  if (current_room == 8 && rooms[8].locked == false) {
    print("\nYou unlock the door and escape the Mystery House!\n");
    print("Congratulations!\n");
    return 1;
  }
  return 0;
}


/* ---------------------- Command parsing (simple) ------------------ */

static void process_command(char *line) {
  char *s = skip_space(line);
  tolower_str(s);

  // commands:
  // go <dir>
  // take <item>
  // use <item>
  // look
  // inventory
  // help

  if (s[0] == 0) return;

  if (s[0]=='g' && s[1]=='o' && s[2]==' ') {
    handle_go(skip_space(s+3));
    return;
  }

  if (s[0]=='t' && s[1]=='a' && s[2]=='k' && s[3]=='e' && s[4]==' ') {
    handle_take(skip_space(s+5));
    return;
  }

  if (s[0]=='u' && s[1]=='s' && s[2]=='e' && s[3]==' ') {
    handle_use(skip_space(s+4));
    return;
  }

  if (streq(s, "look")) {
    print_room(current_room);
    return;
  }

  if (streq(s, "inventory") || streq(s, "inv")) {
    print_inventory();
    return;
  }

  if (streq(s, "help")) {
    print("Commands:\n");
    print("  go north/south/east/west\n");
    print("  take flashlight | take silver key | take brass key\n");
    print("  use flashlight | use silver key | use brass key\n");
    print("  look, inventory, help\n");
    print("Board movement: set SW1..SW0 then press BTN.\n");
    return;
  }

  print("I don't understand. Type 'help'.\n");
}


/* ---------------------- World setup (9 rooms) ------------------ */

static void init_world(void) {
  // Room indices:
  // 0 Entrance Hall
  // 1 Living Room
  // 2 Kitchen
  // 3 Basement (dark)
  // 4 Upstairs Hall
  // 5 Bedroom
  // 6 Study (silver key)
  // 7 Storage Room (locked, brass key)
  // 8 Exit Door (locked, win)

  rooms[0] = (Room){
    "Entrance Hall",
    "The front door slams shut behind you. The house is silent.",
    1, -1, -1, 8,
    false, false, 0,
    false,false,false
  };

  rooms[1] = (Room){
    "Living Room",
    "A cracked fireplace. Something glints under the sofa.",
    4, 0, 2, -1,
    false, false, 0,
    true,false,false   // flashlight
  };

  rooms[2] = (Room){
    "Kitchen",
    "Dusty plates. A narrow stairwell leads down.",
    -1, 3, 7, 1,
    false, false, 0,
    false,false,false
  };

  rooms[3] = (Room){
    "Basement",
    "Cold concrete. You hear water dripping in the dark.",
    2, -1, -1, -1,
    true, false, 0,     // dark room
    false,false,false
  };

  rooms[4] = (Room){
    "Upstairs Hall",
    "Portraits stare at you. A door to the east is slightly open.",
    6, 1, 5, -1,
    false, false, 0,
    false,false,false
  };

  rooms[5] = (Room){
    "Bedroom",
    "An unmade bed. The window is nailed shut.",
    -1, -1, -1, 4,
    false, false, 0,
    false,false,false
  };

  rooms[6] = (Room){
    "Study",
    "A desk covered in notes. One drawer is ajar.",
    -1, 4, -1, -1,
    false, false, 0,
    false,true,false    // silver key
  };

  rooms[7] = (Room){
    "Storage Room",
    "Old crates. A heavy brass key hangs on a hook.",
    -1, -1, -1, 2,
    false, true, "The Storage Room is locked. You need a silver key.",
    false,false,true    // brass key
  };

  rooms[8] = (Room){
    "Exit Door",
    "A reinforced door with a brass lock. Fresh air seeps through.",
    -1, -1, 0, -1,
    false, true, "The Exit Door is locked. A brass key might fit.",
    false,false,false
  };
}


/* ---------------------- Main (Lab3 polling style) ------------------ */

int main(void) {
  init_world();
  update_warmth_leds();

  print("Mystery House (DTEK-V)\n");
  print("Type 'help' for commands.\n");
  enter_room(0);

  char input[MAX_INPUT];

  while (1) {

    /* -------- board movement shortcut ----------
       Use SW1..SW0 as direction:
         00 north, 01 south, 10 east, 11 west
       Press BTN to commit movement.
    */
    if (get_btn()) {
      int sw = get_sw() & 0x3;

      if (sw == 0) handle_go("north");
      if (sw == 1) handle_go("south");
      if (sw == 2) handle_go("east");
      if (sw == 3) handle_go("west");

      delay(1); // small debounce
    }

    /* -------- cold ticks (simple polling, no interrupts) -------- */
    delay(2); // about 1 second
    warmth--;
    update_warmth_leds();
    if (warmth > 0) {
      print("\nThe house gets colder... Warmth now ");
      print_dec((unsigned)warmth);
      print("\n");
    }

    /* -------- UART command input -------- */
    print("\n> ");
    uart_getline(input, MAX_INPUT);
    process_command(input);

    if (check_end()) break;
  }

  set_leds(0x3FF); // all LEDs on at end
  for(;;);         // halt

  return 0;
}
