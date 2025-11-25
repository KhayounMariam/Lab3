/*
  Mystery House - Interactive Fiction 
  - Uses MMIO LEDs, Switches, Button
  - Uses UART print/printc/print_dec (from dtekv-lib) for OUTPUT text
  - Uses delay(ms) from timetemplate.S only for small debounce

  Controls (switch-only):
    Use SW3..SW0 as command + argument, then press BTN.

    CMD = SW3..SW2, ARG = SW1..SW0

    CMD=00 (0): go
       ARG=00: north
       ARG=01: south
       ARG=10: east
       ARG=11: west

    CMD=01 (1): take
       ARG=00: flashlight
       ARG=01: silver key
       ARG=10: brass key

    CMD=02 (2): use
       ARG=00: flashlight
       ARG=01: silver key
       ARG=10: brass key

    CMD=03 (3): misc
       ARG=00: look
       ARG=01: inventory
       ARG=10: help

  LEDs:
    LED0 on  -> you have flashlight
    LED1 on  -> you have silver key
    LED2 on  -> you have brass key
*/

#include <stdint.h>
#include <stdbool.h>

/* Required by boot.S even if interrupts are unused. */
void handle_interrupt(unsigned cause)
{
    (void)cause;
}

/* ---------------------- Memory-mapped I/O ------------------------- */
#define LEDS_ADDR      0x04000000u
#define SWITCHES_ADDR  0x04000010u
#define BUTTON_ADDR    0x040000d0u

#define LEDS     ((volatile unsigned int*) LEDS_ADDR)
#define SWITCHES ((volatile unsigned int*) SWITCHES_ADDR)
#define BUTTON   ((volatile unsigned int*) BUTTON_ADDR)

/* ---------------------- UART + delay from lab files ------------------ */
extern void print(char*);
extern void printc(char);
extern void print_dec(unsigned int);
extern void delay(int);

/* ---------------------- Basic I/O helpers ------------------ */

// LED output
void set_leds(int led_mask) {
  *LEDS = (unsigned int)(led_mask & 0x3FF); // only 10 LEDs exist
}

// read 10 switches
int get_sw(void) {
  return (int)(*SWITCHES & 0x3FF);
}

// read push button
int get_btn(void) {
  return (int)(*BUTTON & 0x1); // BTN pressed = 1
}

/* ---------------------- Game data ------------------ */

#define NUM_ROOMS 9

typedef struct {
  char *name;
  char *desc;
  int north, south, east, west;   // -1 = no exit
  bool dark;                      // needs flashlight ON
  bool locked;                    // door locked
  char *lock_msg;                 // printed if locked

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

/* ---------------------- Small string helper ------------------ */

static int streq(char *a, char *b) {
  while (*a && *b) {
    if (*a != *b) return 0;
    a++; b++;
  }
  return (*a == 0 && *b == 0);
}

/* ---------------------- LED status update ------------------ */

static void update_status_leds(void) {
  int mask = 0;

  if (has_flashlight) mask |= (1 << 0);  // LED0
  if (has_silver_key) mask |= (1 << 1);  // LED1
  if (has_brass_key)  mask |= (1 << 2);  // LED2

  set_leds(mask);
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
}

static void enter_room(int id) {
  current_room = id;
  print_room(id);
}

/* ---------------------- Game logic ------------------ */

static int can_enter(int to_id) {
  Room *to = &rooms[to_id];

  if (to->locked) {
    if (to->lock_msg) {
      print(to->lock_msg);
      print("\n");
    }
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
      update_status_leds();
    } else print("No flashlight here.\n");
    return;
  }

  if (streq(item, "silver key") || streq(item, "silver") || streq(item, "key")) {
    if (cur->item_silver_key) {
      cur->item_silver_key = false;
      has_silver_key = true;
      print("You take the silver key.\n");
      update_status_leds();
    } else print("No silver key here.\n");
    return;
  }

  if (streq(item, "brass key") || streq(item, "brass")) {
    if (cur->item_brass_key) {
      cur->item_brass_key = false;
      has_brass_key = true;
      print("You take the brass key.\n");
      update_status_leds();
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
  // Win condition: inside Exit Door room (8) and it is unlocked
  if (current_room == 8 && rooms[8].locked == false) {
    print("\nYou unlock the door and escape the Mystery House!\n");
    print("Congratulations!\n");
    return 1;
  }
  return 0;
}

/* ---------------------- Switch-based command decoder ------------------ */
/*
   Use SW3..SW0 as command + argument, then press BTN:

   CMD = SW3..SW2, ARG = SW1..SW0

   CMD=00 (0): go
      ARG=00: north
      ARG=01: south
      ARG=10: east
      ARG=11: west

   CMD=01 (1): take
      ARG=00: flashlight
      ARG=01: silver key
      ARG=10: brass key

   CMD=02 (2): use
      ARG=00: flashlight
      ARG=01: silver key
      ARG=10: brass key

   CMD=03 (3): misc
      ARG=00: look
      ARG=01: inventory
      ARG=10: help
*/

static void run_switch_command(void) {
  int sw = get_sw() & 0xF;   // use SW3..SW0
  int cmd = (sw >> 2) & 0x3; // SW3..SW2
  int arg = sw & 0x3;        // SW1..SW0

  if (cmd == 0) {
    // ------ go ------
    if (arg == 0)      handle_go("north");
    else if (arg == 1) handle_go("south");
    else if (arg == 2) handle_go("east");
    else if (arg == 3) handle_go("west");
    else               print("Unknown GO selection.\n");
    return;
  }

  if (cmd == 1) {
    // ------ take ------
    if (arg == 0)      handle_take("flashlight");
    else if (arg == 1) handle_take("silver key");
    else if (arg == 2) handle_take("brass key");
    else               print("Unknown TAKE selection.\n");
    return;
  }

  if (cmd == 2) {
    // ------ use ------
    if (arg == 0)      handle_use("flashlight");
    else if (arg == 1) handle_use("silver key");
    else if (arg == 2) handle_use("brass key");
    else               print("Unknown USE selection.\n");
    return;
  }

  if (cmd == 3) {
    // ------ misc ------
    if (arg == 0) {
      // look
      print_room(current_room);
    } else if (arg == 1) {
      // inventory
      print_inventory();
    } else if (arg == 2) {
      // help
      print("Commands via switches:\n");
      print("  CMD=00 go: ARG=00 N, 01 S, 10 E, 11 W\n");
      print("  CMD=01 take: ARG=00 flash, 01 silver, 10 brass\n");
      print("  CMD=02 use:  ARG=00 flash, 01 silver, 10 brass\n");
      print("  CMD=03 misc: ARG=00 look, 01 inventory, 10 help\n");
    } else {
      print("Unknown MISC selection.\n");
    }
    return;
  }

  print("Unknown command selection.\n");
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

/* ---------------------- Main ------------------ */

int main(void) {
  init_world();
  update_status_leds();  // show starting inventory (none)

  print("Mystery House (DTEK-V)\n");
  print("Use switches + button for all commands.\n");
  print("CMD=SW3..SW2, ARG=SW1..SW0, then press BTN.\n");
  print("Set CMD=03 and ARG=10 then press BTN for help.\n");
  enter_room(0);

  while (1) {

    /* -------- switch-based command ---------- */
    if (get_btn()) {
      run_switch_command();
      delay(1); // debounce

      if (check_end()) break;
    }
  }

  // Game ended: turn all LEDs on
  set_leds(0x3FF);
  for(;;);         // halt

  return 0;
}
