#include "input.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
// --- CROSS-PLATFORM SLEEP CONFIGURATION ---
#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms) // Windows uses Sleep() in milliseconds
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000) // Unix uses usleep() in microseconds
#endif

// --- GAME BALANCING CONSTANTS ---
#define MAP_WIDTH 10
#define MAP_HEIGHT 10
#define ENEMY_POOL_SIZE 3

// --- STRUCTURES (DATA MODELS) ---
// PLAYER
typedef struct {
  char name[25]; // Player name (+1 for null terminator)
  int level;
  int strength; // Base value used for damage scaling
  int x;        // Grid horizontal coordinate
  int y;        // Grid vertical coordinate
  float exp;
  int hp;     // <-- Added
  int max_hp; // <-- Added
} Character;

// MOB
typedef struct {
  char name[25];
  char symbol; // Map icon representation (e.g., 'G')
  int hp;      // Default maximum health pool
  int dmg;     // Base attack damage
  char rank;
  int level;
} EnemyTemplate;

typedef struct {
  EnemyTemplate template; // Inherits name, symbol, max hp, dmg, rank, level
  int x, y;               // Map position
  int current_hp;         // Tracks actual HP remaining in combat
  int active;             // 1 or 0
} Enemy;

// --- GLOBAL STATE VARIABLES ---

Character player1;  // Tracks global state of the user character
Enemy active_enemy; // Stores state of the single active combat opponent

// Fixed database of spawnable templates
EnemyTemplate enemy_pool[ENEMY_POOL_SIZE] = {
    {"Goblin", 'G', 15, 3}, {"Orc", 'O', 35, 7}, {"Skeleton", 'S', 20, 4}};

// --- UTILITY FUNCTIONS ---

// Clears terminal buffer depending on OS platform
void clear_screen() {
#ifdef _WIN32
  system("cls");
#else
  system("clear");
#endif
}

// Calculates dynamic attack output: Base Strength + Random variance (0 to 4)
int calculate_damage(Character player) {
  return player.strength + (rand() % 5);
}
int calculate_enemy_damage(Enemy enemy) {
  return enemy.template.dmg + enemy.template.rank + enemy.template.level +
         (rand() % 5);
}
// --- COMBAT CORE ---
// FIX: When a mob attacks you, theres a newline in the first frame of the
// fight.

//  Gotta fix it!

// Executes turn-based combat loop until enemy dies or player flees
void in_fight(char *name, char symbol, int hp, int x, int y, int strength) {
  clear_screen();
  printf("%s has attacked you!\n", name);
  sleep_ms(1000);

  int max_hp = active_enemy.template.hp;
  char choice;
  int bar_width = 10; // Total text character length of UI bar
  const char *filling = "##########";
  const char *empty = "----------";

  while (1) {
    clear_screen();

    // 1. Calculate Enemy HP Bar
    int enemy_segments = (int)(((float)hp / max_hp) * bar_width);
    if (enemy_segments < 0)
      enemy_segments = 0;
    if (enemy_segments > bar_width)
      enemy_segments = bar_width;

    // 2. Calculate Player HP Bar (Global)
    int player_segments =
        (int)(((float)player1.hp / player1.max_hp) * bar_width);
    if (player_segments < 0)
      player_segments = 0;
    if (player_segments > bar_width)
      player_segments = bar_width;

    // --- RENDER UI ---
    // Enemy UI
    printf("\nEnemy: %s (%c)\n", name, symbol);
    printf("HP: [%.*s%.*s] %d/%d\n", enemy_segments, filling,
           bar_width - enemy_segments, empty, hp, max_hp);

    printf("---------------------\n"); // Separator

    // Player UI (Always visible below)
    printf("Player: %s\n", player1.name);
    printf("HP: [%.*s%.*s] %d/%d\n\n", player_segments, filling,
           bar_width - player_segments, empty, player1.hp, player1.max_hp);

    printf("1. Attack\n2. Run\nChoose action: ");

    choice = get_keypress();

    if (choice == '1') {
      int dmg = calculate_damage(player1);
      int enemy_dmg = calculate_enemy_damage(active_enemy);

      hp -= dmg;
      player1.hp -= enemy_dmg;

      clear_screen();
      printf("You hit the %s for %d damage!\n", name, dmg);
      printf("The %s hit you for %d damage!\n", name, enemy_dmg);

      if (player1.hp <= 0) {
        player1.hp = 0;
        clear_screen();
        printf("You were defeated by the %s...\n", name);
        sleep_ms(2000);
        break;
      }
      if (hp <= 0) {
        active_enemy.active = 0;
        clear_screen();
        printf("You defeated the %s!\n", name);
        sleep_ms(1500);
        break;
      }
    } else if (choice == '2') {
      clear_screen();
      printf("You ran away!\n");
      active_enemy.active = 0;
      sleep_ms(1500);
      break;
    } else {
      continue; // Ignore arrows, movement keys, anything unrecognized
    }
  }
}

// Evaluates RNG on step to determine if an encounter initializes
void random_event(int *n_tiles, int player_x, int player_y, int strength) {
  // Only try to spawn an enemy if one isn't already alive on the map
  // if (active_enemy.active)
  //   return;

  int random_int = (rand() % 20) + 1; // 5% execution probability per tile move

  if (random_int == 1) {
    int random_index = rand() % ENEMY_POOL_SIZE;
    EnemyTemplate selected = enemy_pool[random_index];

    // Correctly assign to the nested template struct
    active_enemy.template = selected;
    active_enemy.current_hp = selected.hp; // Set current health to max health
    active_enemy.active = 1;

    // Spawn them somewhere random nearby, or right in front of the player
    // For now, let's place them at a random spot on the map so they don't
    // immediately trap you
    active_enemy.x = rand() % MAP_WIDTH;
    active_enemy.y = rand() % MAP_HEIGHT;

    // Safety check: Don't spawn directly on top of the player
    if (active_enemy.x == player_x && active_enemy.y == player_y) {
      active_enemy.x = (player_x + 1) % MAP_WIDTH;
    }
  }
}
// --- GAME LOOP AND MAP RENDER ---
// Controls map rendering array and player input polling

// TODO: Make the map bigger (should be 10kx10k with zones, should think
// over it)
// TODO: Show player stats in UI and xp bar
void map(Character *player) {
  char input;
  int n_tiles =
      1; // Tracks total spaces navigated (currently unused internally)

  while (1) {
    clear_screen();

    // Render Map
    for (int y = MAP_HEIGHT - 1; y >= 0; y--) {
      for (int x = 0; x < MAP_WIDTH; x++) {
        if (x == player->x && y == player->y) {
          printf("P "); // Renders player pointer position
        } else if (active_enemy.active && x == active_enemy.x &&
                   y == active_enemy.y) {
          printf(
              "%c ",
              active_enemy.template.symbol); // Renders enemy symbol if active
        } else {
          printf(". "); // Renders ambient coordinate space
        }
      }
      printf("\n");
    }
    printf("\nPosition: %d | %d\n", player->x, player->y);

    input = get_keypress();

    // Coordinate validation boundaries prevent player index overflow
    // out-of-bounds
    if ((input == 'w' || input == 'W') && player->y < MAP_HEIGHT - 1) {
      player->y += 1;
      n_tiles++;
      random_event(&n_tiles, player->x, player->y, player->strength);
    } else if ((input == 's' || input == 'S') && player->y > 0) {
      player->y -= 1;
      n_tiles++;
      random_event(&n_tiles, player->x, player->y, player->strength);
    } else if ((input == 'a' || input == 'A') && player->x > 0) {
      player->x -= 1;
      n_tiles++;
      random_event(&n_tiles, player->x, player->y, player->strength);
    } else if ((input == 'd' || input == 'D') && player->x < MAP_WIDTH - 1) {
      player->x += 1;
      n_tiles++;
      random_event(&n_tiles, player->x, player->y, player->strength);
    } else if (input == 'q' || input == 'Q') {
      return; // Returns to main program block execution sequence
    }
    if (active_enemy.active && player->x == active_enemy.x &&
        player->y == active_enemy.y) {
      in_fight(active_enemy.template.name, active_enemy.template.symbol,
               active_enemy.current_hp, active_enemy.x, active_enemy.y,
               player->strength);
    }
  }
}

// Sets initial structural values for player configuration profiles
// TODO: READ FROM SAVE
void game() {
  player1.exp = 0;
  player1.x = 0;
  player1.y = 0;
  player1.level = 1;
  player1.strength = 10;
  player1.max_hp = 100; // <-- Added
  player1.hp = 100;     // <-- Added

  map(&player1); // Starts main game rendering workflow loop
}

// Standard naming parser with fixed array overflow safety checks
void character_creation() {
  clear_screen();
  printf("Character creation!\n");

  while (1) {
    printf("Enter your name (max 24 chars): ");
    // Explicit format width limit (%24s) leaves room for final string \0
    scanf("%24s", player1.name);
    break;
  }

  printf("Welcome, %s!\n", player1.name);
  fflush(stdout); // Instantly clears standard output buffer before starting
                  // game thread
  sleep_ms(2000);

  game();
}

// Program Entry Point
int main() {
  clear_screen();
  srand(time(NULL)); // Hardcoded seed ensures predictable, repeatable
                     // pseudorandom generations
  char option;

  printf("Hello in Borginal!\n");
  printf("1. Play!\n");
  printf("2. Credits!\n");

  option = get_keypress();

  if (option == '1') {
    character_creation();
  } else if (option == '2') {
    clear_screen();
    printf("Credits: MasterJohnson\n");
  } else {
    clear_screen();
    printf("Unknown option!\n");
  }

  return 0;
}
