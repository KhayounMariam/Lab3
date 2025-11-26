#include <stdint.h>
#include <stdbool.h>

void handle_interrupt (unsigned cause) {
  (void)cause; 
}

/*Memory-mapped I/O from lab 3 */
#define LEDS_ADDR  0x04000000u 
#define SWITCHES_ADDR 0x040000010u
#define BUTTONS_ADDR 0x040000d0u

#define LEDS ((volatile unsigned int*) LEDS_ADDR)
#define SWITCHES ((volatile unsigned int*) SWITCHES_ADDR)
#define BUTTONS ((volatile unsigned int*) BUTTONS_ADDR)

/* Printing UART logic + delay from dtekv-lib.c, also from lab3*/
extern void print(char*);
extern void printc(char);
extern void print_dec(unsigned int);
extern void delay(int); 

/*Basic I/O helpers from lab 3*/
void set_leds(int led_mask) {
  *LEDS = (unsigned int) (led_mask & 0x3FF); 
}

int get_sw(void) {
  return (int)(*SWITCHES & 0x3FF);
}

int get_btn(void) {
  return (int)(*BUTTONS & 0x1); 
}

/*At this stage, all of the hardware definitions and helpers are set up.
We have the handle_interrupt at the beginning because even though we don't use it
in our code, boot.S expects this symbol. So we make it return nothing.
Then we define where LEDs, switches, and button live in memory, volatile tells the compiler
that this can change due to hardware, so don't optimize reads/writes away. We treat each address as a pointer 
to unsigned int (a positive integer). For the printing logic, they are implemented in other files (dtek.lib, timetemplate.S
but we just declare them. Extern means this exists somewhere else.
). The helper functions are are used later, for set_leds, we are setting the 10 LEDS (only lowest 10 bits used).
get_sw reads the 10 switches (SW0...SW9)
get_btn reads push button, returns 1 if pressed, 0 otherwise.*/


/*DEFINING THE ROOMS AND CORE GAME STATE*/
#define NUM_ROOMS 9 //the compiler knows how big the world is.

struct room { //everywhere in the code we will use struct room instead of room.
  char *name; //a pointer that points to a string literal like "Kitchen"
  char *desc; //description text

  //These fileds tell the user what room they'll go to when they walk in that direction.
  //If they're in the entrance hall (room 0), and going north should take them to Living Room (room 1),
  //then inside the entrance hall struct, it will say: north=1. "from room 0, going north takes you to room 1."
  int north;
  int south;
  int east;
  int west; 

  bool dark; //This room cannot be entered unless flashligth is ON. If dark = true, it will say: room is too dark to go there.
  bool locked; // If locked is true, when the player tries to enter, the game will print: "door is locked, need key"
  char *lock_msg; //message printed if locked.
  bool item_flashlight; //tells us if an item is in the room, if true, player can pick it up.
  bool item_silver_key; //same
  bool item_brass_key; //same
};

static struct room rooms[NUM_ROOMS]; //This creates a room array with 9 objects in memory ex. rooms[0] = entrance hall

//player state
static int current_room = 0; //player starts at room 0. Entrance Hall.
static bool has_flashlight = false; //does player have a flashlight?
static bool has_silver_key = false; //does player have a silver key?
static bool has_brass_key = false; //does player have brass key?
static bool flashlight_on = false; //does player have the flashlight on?

//show things to the player (LEDs + room text)
/*We want the LEDs to show the items the player has in their inventory: 
- LED0 flashlight
- LED1 silver key
- LED2 brass key
*/
static void status_of_leds(void) {
  int mask = 0; //starts with all LEDs OFF (binary 0000000000)

  if (has_flashlight) mask |= (1 << 0); // 1 << 0 = 0001 == LED0 (corresponds to bit 0)
  if (has_silver_key) mask |= (1 << 1); // 1 << 1 = 0010 == LED1 (corresponds to bit 1)
  if (has_brass_key) mask |= (1 << 2); // 1 << 2 = 0100 == LED2 (corresponds to bit 2)

  set_leds (mask); //call the function in step 1 that writes mask into the LED register. sets the number stored in mask to control the LEDs.

}

//print_room is a function that, given a room index (id), prints: 
/*
- room name
- room description
- any items in the room
- the exists (north, south, east, west) that exist 
*/

static void print_room (int id) {
  struct room *r = &rooms[id]; //address of room[some number]

  print("\n== ");
  print(r->name);
  print( "==\n");
  print(r->desc);
  print("\n");

  /* Output will be: == Entrance Hall ==
                      The front door slammed shut behind you..
  */

//Printing items in the room:
if (r->item_flashlight || r->item_silver_key || r->item_brass_key) { //first we check if any oth the item booleans are true (exist in the room)
  print("Items here:"); //if yes: print items here plus the names of the items present.
  if (r->item_flashlight) print(" flashlight");
  if (r->item_silver_key) print(" silver key");
  if (r->item_brass_key) print(" brass key"); 
  print("\n"); 
}

//Printing exists:
print("Exists:");
if (r->north != -1) print(" north");
if (r->south != -1) print (" south");
if (r->east != -1) print (" east");
if (r->west != -1) print (" west");
print ("\n"); 
//when any of them is -1, there is not exist, don't print it.

}

//Change current_room and show the room.
static void enter_room(int id) {
  current_room = id; 
  print_room(id); 
}

/*GAME LOGIC, moving between rooms, picking items, using items etc.*/
static int can_enter(int to_id) {
  struct room *to = &rooms[to_id];

  if (to->locked) {
    print (to->lock_msg);
    print ("\n");
    return 0;
  }

  if (to->dark && !(has_flashlight && flashlight_on)){
    print("It's too dark to go there without flashligh. \n");
    return 0; 
  }

  return 1; //safe to enter

}

//direction map: 0 -> north, 1 -> south, 2 -> east, 3 -> west

static void handle_go (int direction) {
  struct room *cur = &rooms[current_room];
  int to = -1;

  if (direction == 0) to = cur -> north;
  if (direction == 1) to = cur -> south;
  if (direction == 2) to = cur -> east; 
  if (direction == 3) to = cur -> west; 

  if (to == -1) {
    print ("You can't go that way. \n");
    return; 
  }

  if (can_enter(to)) {
    enter_room(to);
  }
}

//item map: 0 -> flashlight, 1 -> silver key, 2 -> brass key
static void handle_take (int item) {
  struct room *cur = &rooms[current_room];

  if (item == 0) { //Did the player select the flashlight on the switches?
    if (cur -> item_flashlight) { //Is the flashlight actually in the room?
      cur->item_flashlight = 0; 
      has_flashlight = 1; 
      print ("You picked up the flashlight. \n"); 
      update_status_leds();
    } else {
      print ("No flashlight here. \n");
    }
    return; 
  }

  if (item == 1) { //silver key
    if (cur -> item_silver_key) {
      cur->item_silver_key = 0;
      has_silver_key = 1; 
      print ("You took the silver key. \n");
      update_status_leds();

    } else {
      print ("No silver key here. \n");
    }
    return; 
  }

  if (item == 2) { //brass key
    if (cur->item_brass_key) {
      cur->item_brass_key = 0; 
      has_brass_key = 1; 
      print ("You took the brass key. \n");
      update_status_leds();
    } else {
      print ("No brass key here. \n");
    }
    return; 
  }
}

static void handle_use(int item) {
  struct room *cur = &rooms[current_room];

  if (item == 0) { //player selected "use flashlight"
    if (!has_flashlight) {
      print ("You don't have a flashlight. \n");
      return;
    }
    flashlight_on = !flashlight_on;
    print("Flashlight ");
    print(flashlight_on ? "ON.\n" : "OFF.\n");
    return; 
  }
//silver key unlocks Storage Room (room 7)
  if (item == 1) {
    if (!has_silver_key) {
      print ("You don't have the silver key.\n");
      return; 
    } 

    if (cur->north == 7 || cur->south == 7 || cur->east == 7 || cur->west == 7) {
    rooms[7].locked = 0; //Go to room array, find room index 7, and set its locked field to false.
    print("You unlock the Storage Room.\n");
  } else {
    print ("Nothing here fits the silver key. \n");
  }
  return;
}

//brass key unlocks Exit Door (room 8)
if (item == 2) {
  if (!has_brass_key) {
    print ("You don't have the brass key. \n");
    return; 
  }

  if (cur->north == 8 || cur->south == 8 || cur->east == 8 || cur->west == 8) {
    rooms[8].locked = 0; 
    print("You unlocked the Exit Door. \n");
  } else {
    print ("Nothing here fits the brass key. \n");
  }
  return; 

} }

//Locked rooms: Room 7 (needs a silver key) and room 8 (needs brass key that player can find in room 7)

static void print_inventory (void) {
  print ("You are carrying: \n");

  if (has_flashlight){
    print("flashlight (");
    print(flashlight_on ? "ON" : "OFF");
    print(")\n");
  }
  if (has_silver_key) print("silver key\n");
  if (has_brass_key) print("brass key\n");
  if (!has_flashlight && !has_silver_key && !has_brass_key)
  print("nothing\n");

}

//win condition
static int check_end(void) {
  if (current_room == 8 && rooms[8].locked == 0) {
    print("\nYou unlock the door and escape the Mystery House HAHAHA!\n");
    print("We hope to see you again...\n");
    return 1; //game ends
  }
  return 0; 
}

/*What is happening in handle_use?
The player uses switches to chose what ACTION to perform (go, take, use, inventory)
Which ITEM or direction (flashlight, keys, north, etc.) then presses the button to confirm. So 
handle_use is called because the player used the switches and pressed the button. It performs the 
"use item" action chosen by the player. If the selected item is the flashlight, toggle ON/OFF. If the
item is the silver key, try to unlock room 7 (storage), If item is the brass key, try to unlock room 8 (exit door).
handle_use does not read the switches, it only reacts to an argument (0, 1, 2) that comes from the switch decoder.*/

/*SWITCH DECODER- what switches trigger different actions
button = "do it now", switches = "what to do". We want a function that reads
the switches, decides what the player meant, calls the right game function (handle_go, handle_take, handle_use,
print_inventory, etc.)

Command Encoding (what the switches mean)
We will use the 4 lowest switches: SW3...SW0.
- SW3..SW2 (2 bits) = command type
- SW1..SW0 (2 bits) = argument 

So: 
Command type (SW3..SW2)
Bits:
- 00 meaning: GO
- 01 meaning: TAKE
- 10 meaning: USE
- 11 meaning: OTHER (look, inventory /help)

Argument (SW1..SW0)
- For go: 
00 direction: north
01 direction: south
10 direction: east
11 direction: west

-For take and use:
00 item: flashlight
01 item: silver key
10 item: brass key
11: unused

-For "other"
00 action: look
01 action: inventory
10 action: unused
11: unused

*/

static void run_switch_command(void) { //this reads the switches, and then calls on an action function
  int sw = get_sw() & 0xF; // we only want the 4 switches
  int cmd = (sw >> 2) & 0xF; //SW3..Sw2
  int arg = sw & 0xF; //SW1..Sw0 so if switches = 1010 then cmd = 10 (use) and arg = 10 (brass key)

  if (cmd == 0) { //00 = movement (go)
    handle_go (arg); //arg: 0= north, 1=south, 2=east, 3=west
    return; 
  }

  if (cmd == 1) { //10: use item
    if (arg <= 2) { //arg: 0=flashlight, 1=silver key, 2= brass key
      handle_use(arg);
    } else {
      print ("No such item to use.\n");
    }
    return; 
  }

  if (cmd == 3) { //11 = other actions
    if (arg == 0) { //look
      print_room(current_room);
    } else if (arg == 1) { //inventory
      print_inventory();
    } else {
      print("No action using this switch combo.\n"); //bc arg==2, 3 is unused
    }
    return; 
  }
  
}

/*The world layout:
- 0 Entrance Hall
- 1 Living Room (flashlight here)
- 2 Kitchen
- 3 Basement (dark)
- 4 Upstairs Hall
- 5 Bedroom
- 6 Study (silver key here)
- 7 Storage Room (locked, brass key here, opens with silver key)
- 8 Exit Door (locked, win room) */

static void init_world(void) {
    // Room 0: Entrance Hall
    rooms[0] = (struct room){
        "Entrance Hall",
        "The front door slams shut behind you. The house is silent.",
        1,  // north -> Living Room
        -1, // south
        -1, // east
        8,  // west -> Exit Door
        false, // dark
        false, // locked
        0,     // lock_msg
        false, // item_flashlight
        false, // item_silver_key
        false  // item_brass_key
    };

    // Room 1: Living Room (has flashlight)
    rooms[1] = (struct room){
        "Living Room",
        "A cracked fireplace. Something glints under the sofa.",
        4,  // north -> Upstairs Hall
        0,  // south -> Entrance Hall
        2,  // east  -> Kitchen
        -1, // west
        false,
        false,
        0,
        true,  // flashlight here
        false,
        false
    };

    // Room 2: Kitchen
    rooms[2] = (struct room){
        "Kitchen",
        "Dusty plates. A narrow stairwell leads down.",
        -1, // north
        3,  // south -> Basement
        7,  // east  -> Storage Room
        1,  // west  -> Living Room
        false,
        false,
        0,
        false,
        false,
        false
    };

    // Room 3: Basement (dark room)
    rooms[3] = (struct room){
        "Basement",
        "Cold concrete. You hear water dripping in the dark.",
        2,  // north -> Kitchen
        -1,
        -1,
        -1,
        true,  // dark = needs flashlight ON
        false,
        0,
        false,
        false,
        false
    };

    // Room 4: Upstairs Hall
    rooms[4] = (struct room){
        "Upstairs Hall",
        "Portraits stare at you. A door to the east is slightly open.",
        6,  // north -> Study
        1,  // south -> Living Room
        5,  // east  -> Bedroom
        -1, // west
        false,
        false,
        0,
        false,
        false,
        false
    };

    // Room 5: Bedroom
    rooms[5] = (struct room){
        "Bedroom",
        "An unmade bed. The window is nailed shut.",
        -1,
        -1,
        -1,
        4,  // west -> Upstairs Hall
        false,
        false,
        0,
        false,
        false,
        false
    };

    // Room 6: Study (silver key here)
    rooms[6] = (struct room){
        "Study",
        "A desk covered in notes. One drawer is ajar.",
        -1,
        4,  // south -> Upstairs Hall
        -1,
        -1,
        false,
        false,
        0,
        false,
        true,  // silver key here
        false
    };

    // Room 7: Storage Room (locked, brass key here)
    rooms[7] = (struct room){
        "Storage Room",
        "Old crates. A heavy brass key hangs on a hook.",
        -1,
        -1,
        -1,
        2,   // west -> Kitchen
        false,
        true,  // locked at start
        "The Storage Room is locked. You need a silver key.",
        false,
        false,
        true   // brass key here
    };

    // Room 8: Exit Door (locked, win room)
    rooms[8] = (struct room){
        "Exit Door",
        "A reinforced door with a brass lock. Fresh air seeps through.",
        -1,
        -1,
        0,   // east -> Entrance Hall
        -1,
        false,
        true,  // locked at start
        "The Exit Door is locked. A brass key might fit.",
        false,
        false,
        false
    };
}


//MAIN LOOP, wire everything togather
/*Main should
- Initialize world data
- clear/update LEDs
- Print intro text
- enter starting room
- Run an infinite loop: wait for button press, read switches & run command, check win condition
- When game is over: turn all LEDs on, halt*/

int main (void) {
  init_world(); //setup world
  update_status_leds(); //no items at starts, so LEDs off

  //Intro text
  print("Mystery House");
  print("Use SW3..Sw0 + BTN to play.\n");
  print("See instruction paper for commands and press button to confirm");

  //start in room 0 (Entrance Hall)
  enter_room(0);

  //Main game loop
  while (1) { //infinie loop game until break
    if (get_btn()) { //wait for button press
      run_switch_command; //perform command based on switches
    

    while (get_btn()) { //wait until button is released
      delay(1); //tiny delay to use clock
    }

    if (check_end()) { //check if game ended (win condition)
      break;
    }
  }
}

//Game over: turn all LEDs on and halt
set_leds(0x3FF); //all 10 LEDs ON
for(;;); //infinite loop (halt CPU) forever.
return 0;

}

