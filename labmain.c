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

void get_sw(void) {
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
    rooms[7].locked = 0; 
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

/*Switch Commands- what switches trigger different actions
button = "do it now", switches = "what to do" */
