/*
 * borginal_ncurses.c
 * Compile: gcc borginal_ncurses.c -o borginal -lncurses
 *
 * All UI is centered on the terminal at runtime using LINES / COLS.
 * Resize the window and the layout follows automatically.
 */

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// --- CROSS-PLATFORM SLEEP ---
#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

// --- CONSTANTS ---
#define MAP_WIDTH 10
#define MAP_HEIGHT 10
#define ENEMY_POOL_SIZE 3

// ncurses color pair IDs
#define COLOR_PAIR_RED 1
#define COLOR_PAIR_GREEN 2
#define COLOR_PAIR_YELLOW 3
#define COLOR_PAIR_NORMAL 4

// --- HP BAR ---
#define BAR_WIDTH 10
static const char *bar_fill = "##########";
static const char *bar_empty = "----------";

// --- STRUCTURES ---

typedef struct {
  char name[25];
  int level;
  int strength;
  int x, y;
  float exp;
  int hp;
  int max_hp;
} Character;

typedef struct {
  char name[25];
  char symbol;
  int hp;
  int dmg;
  char rank;
  int level;
} EnemyTemplate;

typedef struct {
  EnemyTemplate templ; // "template" is a C++ keyword; renamed to avoid clashes
  int x, y;
  int current_hp;
  int active;
} Enemy;

// --- GLOBAL STATE ---

Character player1;
Enemy active_enemy;

EnemyTemplate enemy_pool[ENEMY_POOL_SIZE] = {
    {"Goblin", 'G', 15, 3, 0, 1},
    {"Orc", 'O', 35, 7, 0, 1},
    {"Skeleton", 'S', 20, 4, 0, 1},
};

// --- UTILITY ---

// Print a string centered horizontally at the given row.
void mvprint_centered(int row, const char *str) {
  int col = (COLS - (int)strlen(str)) / 2;
  if (col < 0)
    col = 0;
  mvprintw(row, col, "%s", str);
}

// Print a colored string centered at the given row.
void mvprint_centered_color(int row, int color_pair, const char *str) {
  int col = (COLS - (int)strlen(str)) / 2;
  if (col < 0)
    col = 0;
  attron(COLOR_PAIR(color_pair));
  mvprintw(row, col, "%s", str);
  attroff(COLOR_PAIR(color_pair));
}

// Draw a horizontal separator centered at row, 'width' chars wide.
void draw_separator(int row, int width) {
  int col = (COLS - width) / 2;
  if (col < 0)
    col = 0;
  mvhline(row, col, '-', width);
}

// Build and print an HP bar string at (row, left_col).
// Returns the column after the bar (for chaining).
void draw_hp_bar(int row, int left_col, int current, int max) {
  int segments = (int)(((float)current / max) * BAR_WIDTH);
  if (segments < 0)
    segments = 0;
  if (segments > BAR_WIDTH)
    segments = BAR_WIDTH;
  mvprintw(row, left_col, "HP: [%.*s%.*s] %d/%d", segments, bar_fill,
           BAR_WIDTH - segments, bar_empty, current, max);
}

int calculate_damage(Character player) {
  return player.strength + (rand() % 5);
}

int calculate_enemy_damage(Enemy enemy) {
  return enemy.templ.dmg + enemy.templ.rank + enemy.templ.level + (rand() % 5);
}

// --- TIMED MESSAGE SCREEN ---

// Clear screen, show a centered message, pause, then return.
void show_message(const char *msg, int ms) {
  clear();
  mvprint_centered(LINES / 2, msg);
  refresh();
  sleep_ms(ms);
}

// --- COMBAT ---

void in_fight(char *name, int *hp) {
  int max_hp = active_enemy.templ.hp;

  // UI block is ~8 rows tall; center it vertically
  int block_h = 8;
  int block_w = 30; // approximate width of the widest line
  int start_row = (LINES - block_h) / 2;
  int left_col = (COLS - block_w) / 2;

  char log_line1[100] = "";
  char log_line2[100] = "";
  char buf[120];

  while (1) {
    clear();

    // --- Enemy header ---
    int row = start_row;
    snprintf(buf, sizeof(buf), "Enemy: %s", name);
    mvprint_centered_color(row++, COLOR_PAIR_RED, buf);

    draw_hp_bar(row++, left_col, *hp, max_hp);

    draw_separator(row++, block_w);

    // --- Player header ---
    snprintf(buf, sizeof(buf), "Player: %s", player1.name);
    mvprint_centered_color(row++, COLOR_PAIR_GREEN, buf);

    draw_hp_bar(row++, left_col, player1.hp, player1.max_hp);

    row++; // blank line

    // --- Combat log ---
    mvprint_centered(row++, log_line1);
    mvprint_centered(row++, log_line2);

    row++; // blank line

    // --- Actions ---
    mvprint_centered_color(row++, COLOR_PAIR_YELLOW, "1. Attack    2. Run");

    refresh();

    int choice = getch();

    if (choice == '1') {
      int dmg = calculate_damage(player1);
      int enemy_dmg = calculate_enemy_damage(active_enemy);

      *hp -= dmg;
      player1.hp -= enemy_dmg;

      snprintf(log_line1, sizeof(log_line1), "You hit the %s for %d damage!",
               name, dmg);
      snprintf(log_line2, sizeof(log_line2), "The %s hit you for %d damage!",
               name, enemy_dmg);

      if (player1.hp <= 0) {
        player1.hp = 0;
        snprintf(buf, sizeof(buf), "You were defeated by the %s...", name);
        show_message(buf, 2000);
        break;
      }
      if (*hp <= 0) {
        active_enemy.active = 0;
        snprintf(buf, sizeof(buf), "You defeated the %s!", name);
        show_message(buf, 1500);
        break;
      }

    } else if (choice == '2') {
      active_enemy.active = 0;
      show_message("You ran away!", 1500);
      break;
    }
    // Any other key: redraw loop (no-op)
  }
}

// --- RANDOM ENCOUNTERS ---

void random_event(int player_x, int player_y) {
  int random_int = (rand() % 20) + 1; // 5% chance per tile moved

  if (random_int == 1) {
    int idx = rand() % ENEMY_POOL_SIZE;
    EnemyTemplate selected = enemy_pool[idx];

    active_enemy.templ = selected;
    active_enemy.current_hp = selected.hp;
    active_enemy.active = 1;

    active_enemy.x = rand() % MAP_WIDTH;
    active_enemy.y = rand() % MAP_HEIGHT;

    // Don't spawn on top of the player
    if (active_enemy.x == player_x && active_enemy.y == player_y)
      active_enemy.x = (player_x + 1) % MAP_WIDTH;
  }
}

// --- MAP / GAME LOOP ---
// TODO: Expand map to zone-based 10k x 10k layout
// TODO: Show player stats and XP bar in UI

void map_loop(Character *player) {
  // The map block is MAP_HEIGHT rows of cells (2 chars each) + 4 rows for HUD
  // We center the whole block on screen.
  int map_px_w = MAP_WIDTH * 2 - 1; // "P . . ..." — width in chars
  int hud_rows = 3;                 // hp bar + blank + position line
  int block_h = MAP_HEIGHT + hud_rows;

  while (1) {
    // Recalculate center each frame to handle terminal resize
    int start_row = (LINES - block_h) / 2;
    int start_col = (COLS - map_px_w) / 2;
    if (start_row < 0)
      start_row = 0;
    if (start_col < 0)
      start_col = 0;

    clear();

    // --- HUD: HP bar ---
    draw_hp_bar(start_row, start_col, player1.hp, player1.max_hp);

    // --- Map grid ---
    for (int y = MAP_HEIGHT - 1; y >= 0; y--) {
      int row = start_row + 1 + (MAP_HEIGHT - 1 - y); // top-to-bottom render
      for (int x = 0; x < MAP_WIDTH; x++) {
        int col = start_col + x * 2;
        if (x == player->x && y == player->y) {
          attron(COLOR_PAIR(COLOR_PAIR_GREEN));
          mvaddch(row, col, 'P');
          attroff(COLOR_PAIR(COLOR_PAIR_GREEN));
        } else if (active_enemy.active && x == active_enemy.x &&
                   y == active_enemy.y) {
          attron(COLOR_PAIR(COLOR_PAIR_RED));
          mvaddch(row, col, active_enemy.templ.symbol);
          attroff(COLOR_PAIR(COLOR_PAIR_RED));
        } else {
          mvaddch(row, col, '.');
        }
      }
    }

    // --- Position line ---
    char pos_buf[32];
    snprintf(pos_buf, sizeof(pos_buf), "Pos: %d | %d  [WASD move, Q quit]",
             player->x, player->y);
    mvprintw(start_row + 1 + MAP_HEIGHT + 1, start_col, "%s", pos_buf);

    refresh();

    int input = getch();

    // Handle terminal resize
    if (input == KEY_RESIZE)
      continue;

    switch (input) {
    case 'w':
    case 'W':
      if (player->y < MAP_HEIGHT - 1) {
        player->y++;
        random_event(player->x, player->y);
      }
      break;
    case 's':
    case 'S':
      if (player->y > 0) {
        player->y--;
        random_event(player->x, player->y);
      }
      break;
    case 'a':
    case 'A':
      if (player->x > 0) {
        player->x--;
        random_event(player->x, player->y);
      }
      break;
    case 'd':
    case 'D':
      if (player->x < MAP_WIDTH - 1) {
        player->x++;
        random_event(player->x, player->y);
      }
      break;
    case 'q':
    case 'Q':
      return;
    default:
      break;
    }

    // Trigger combat when player steps onto the enemy
    if (active_enemy.active && player->x == active_enemy.x &&
        player->y == active_enemy.y) {
      // TODO: Vary message depending on who initiated combat
      char buf[64];
      snprintf(buf, sizeof(buf), "%s has attacked you!",
               active_enemy.templ.name);
      show_message(buf, 1500);
      in_fight(active_enemy.templ.name, &active_enemy.current_hp);
    }
  }
}

// --- INITIALIZATION ---
// TODO: Load from save file

void game() {
  player1.exp = 0;
  player1.x = 0;
  player1.y = 0;
  player1.level = 1;
  player1.strength = 10;
  player1.max_hp = 100;
  player1.hp = 100;

  map_loop(&player1);
}

// --- CHARACTER CREATION ---

void character_creation() {
  clear();

  int row = LINES / 2 - 2;
  mvprint_centered(row, "=== Character Creation ===");
  mvprint_centered(row + 2, "Enter your name (max 24 chars):");

  // Position cursor on the line below the prompt, centered
  int input_col = (COLS - 24) / 2;
  move(row + 3, input_col);

  echo(); // show typed characters
  curs_set(1);
  getnstr(player1.name, 24);
  noecho();
  curs_set(0);

  char welcome[50];
  snprintf(welcome, sizeof(welcome), "Welcome, %s!", player1.name);
  show_message(welcome, 1000);

  game();
}

// --- NCURSES SETUP / TEARDOWN ---

void init_ncurses() {
  initscr();
  cbreak();             // raw input, no line buffering
  noecho();             // don't echo keypresses
  keypad(stdscr, TRUE); // enable arrow keys and KEY_RESIZE
  curs_set(0);          // hide cursor during gameplay

  if (has_colors()) {
    start_color();
    init_pair(COLOR_PAIR_RED, COLOR_RED, COLOR_BLACK);
    init_pair(COLOR_PAIR_GREEN, COLOR_GREEN, COLOR_BLACK);
    init_pair(COLOR_PAIR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
    init_pair(COLOR_PAIR_NORMAL, COLOR_WHITE, COLOR_BLACK);
  }
}

void cleanup_ncurses() { endwin(); }

// --- ENTRY POINT ---

int main() {
  srand(time(NULL));
  init_ncurses();

  // Main menu
  while (1) {
    clear();
    int row = LINES / 2 - 2;
    mvprint_centered(row, "=== Borginal ===");
    mvprint_centered_color(row + 2, COLOR_PAIR_GREEN, "1. Play!");
    mvprint_centered(row + 3, "2. Credits");
    mvprint_centered(row + 4, "Q. Quit");
    refresh();

    int option = getch();

    if (option == '1') {
      character_creation();
    } else if (option == '2') {
      show_message("Credits: MasterJohnson", 2000);
    } else if (option == 'q' || option == 'Q') {
      break;
    }
  }

  cleanup_ncurses();
  return 0;
}
