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
#define MAP_WIDTH 2500
#define MAP_HEIGHT 2500
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
  EnemyTemplate templ;
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

void mvprint_centered(int row, const char *str) {
  int col = (COLS - (int)strlen(str)) / 2;
  if (col < 0)
    col = 0;
  mvprintw(row, col, "%s", str);
}

void mvprint_centered_color(int row, int color_pair, const char *str) {
  int col = (COLS - (int)strlen(str)) / 2;
  if (col < 0)
    col = 0;
  attron(COLOR_PAIR(color_pair));
  mvprintw(row, col, "%s", str);
  attroff(COLOR_PAIR(color_pair));
}

void draw_separator(int row, int width) {
  int col = (COLS - width) / 2;
  if (col < 0)
    col = 0;
  mvhline(row, col, '-', width);
}

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

void show_message(const char *msg, int ms) {
  clear();
  mvprint_centered(LINES / 2, msg);
  refresh();
  sleep_ms(ms);
}

// --- COMBAT ---

void in_fight(char *name, int *hp) {
  int max_hp = active_enemy.templ.hp;

  int block_h = 8;
  int block_w = 30;
  int start_row = (LINES - block_h) / 2;
  int left_col = (COLS - block_w) / 2;

  char log_line1[100] = "";
  char log_line2[100] = "";
  char buf[120];

  while (1) {
    clear();

    int row = start_row;
    snprintf(buf, sizeof(buf), "Enemy: %s", name);
    mvprint_centered_color(row++, COLOR_PAIR_RED, buf);

    draw_hp_bar(row++, left_col, *hp, max_hp);

    draw_separator(row++, block_w);

    snprintf(buf, sizeof(buf), "Player: %s", player1.name);
    mvprint_centered_color(row++, COLOR_PAIR_GREEN, buf);

    draw_hp_bar(row++, left_col, player1.hp, player1.max_hp);

    row++;

    mvprint_centered(row++, log_line1);
    mvprint_centered(row++, log_line2);

    row++;

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

    // Fix: Spawn the enemy relative to the player's position (within a 10-tile
    // radius)
    active_enemy.x = player_x + (rand() % 9) - 4;
    active_enemy.y = player_y + (rand() % 9) - 4;

    // Keep spawn bounds safe within the map boundaries
    if (active_enemy.x < 0)
      active_enemy.x = 0;
    if (active_enemy.x >= MAP_WIDTH)
      active_enemy.x = MAP_WIDTH - 1;
    if (active_enemy.y < 0)
      active_enemy.y = 0;
    if (active_enemy.y >= MAP_HEIGHT)
      active_enemy.y = MAP_HEIGHT - 1;

    // Don't spawn on top of the player
    if (active_enemy.x == player_x && active_enemy.y == player_y)
      active_enemy.x = (player_x + 1) % MAP_WIDTH;
  }
}
// --- MAP / GAME LOOP ---
// Fix the camera to follow player correctly

void map_loop(Character *player) {
  const int VIEW_WIDTH = 10;
  const int VIEW_HEIGHT = 10;
  const int HALF_W = VIEW_WIDTH / 2;
  const int HALF_H = VIEW_HEIGHT / 2;
  const int NUDGE_THRESHOLD = 3;

  const int map_px_w = VIEW_WIDTH * 2 - 1;
  const int hud_rows = 3;
  const int block_h = VIEW_HEIGHT + hud_rows;

  int cam_x = player->x - HALF_W;
  int cam_y = player->y - HALF_H;

  while (1) {
    // --- Camera tracking ---
    int delta_x = player->x - (cam_x + HALF_W);
    int delta_y = player->y - (cam_y + HALF_H);

    if (delta_x > NUDGE_THRESHOLD)
      cam_x++;
    if (delta_x < -NUDGE_THRESHOLD)
      cam_x--;
    if (delta_y > NUDGE_THRESHOLD)
      cam_y++;
    if (delta_y < -NUDGE_THRESHOLD)
      cam_y--;

    if (cam_x < 0)
      cam_x = 0;
    if (cam_y < 0)
      cam_y = 0;
    if (cam_x > MAP_WIDTH - VIEW_WIDTH)
      cam_x = MAP_WIDTH - VIEW_WIDTH;
    if (cam_y > MAP_HEIGHT - VIEW_HEIGHT)
      cam_y = MAP_HEIGHT - VIEW_HEIGHT;

    int start_row = (LINES - block_h) / 2;
    int start_col = (COLS - map_px_w) / 2;
    if (start_row < 0)
      start_row = 0;
    if (start_col < 0)
      start_col = 0;

    // erase() lets ncurses diff against the previous frame instead of
    // forcing a full physical repaint on the next refresh() (which is
    // what clear() does). Same visual result, far less flicker/cost.
    erase();

    draw_hp_bar(start_row, start_col, player->hp,
                player->max_hp); // was player1 (bug)

    // --- Map grid ---
    for (int v_y = VIEW_HEIGHT - 1; v_y >= 0; v_y--) {
      int row = start_row + 1 + (VIEW_HEIGHT - 1 - v_y);
      int world_y = cam_y + v_y;
      for (int v_x = 0; v_x < VIEW_WIDTH; v_x++) {
        int col = start_col + v_x * 2;
        int world_x = cam_x + v_x;

        if (world_x == player->x && world_y == player->y) {
          attron(COLOR_PAIR(COLOR_PAIR_GREEN));
          mvaddch(row, col, 'P');
          attroff(COLOR_PAIR(COLOR_PAIR_GREEN));
        } else if (active_enemy.active && world_x == active_enemy.x &&
                   world_y == active_enemy.y) {
          attron(COLOR_PAIR(COLOR_PAIR_RED));
          mvaddch(row, col, active_enemy.templ.symbol);
          attroff(COLOR_PAIR(COLOR_PAIR_RED));
        } else {
          mvaddch(row, col, '.');
        }
      }
    }

    // --- Position line ---
    mvprintw(start_row + 1 + VIEW_HEIGHT + 1, start_col,
             "Pos: %d | %d  [WASD move, Q quit]", player->x, player->y);

    refresh();

    int input = getch();
    if (input == KEY_RESIZE)
      continue;

    int moved = 0;
    switch (input) {
    case 'w':
    case 'W':
      if (player->y < MAP_HEIGHT - 1) {
        player->y++;
        moved = 1;
      }
      break;
    case 's':
    case 'S':
      if (player->y > 0) {
        player->y--;
        moved = 1;
      }
      break;
    case 'a':
    case 'A':
      if (player->x > 0) {
        player->x--;
        moved = 1;
      }
      break;
    case 'd':
    case 'D':
      if (player->x < MAP_WIDTH - 1) {
        player->x++;
        moved = 1;
      }
      break;
    case 'q':
    case 'Q':
      return;
    default:
      break;
    }

    if (moved) {
      random_event(player->x, player->y);

      if (active_enemy.active && player->x == active_enemy.x &&
          player->y == active_enemy.y) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s has attacked you!",
                 active_enemy.templ.name);
        show_message(buf, 1500);
        in_fight(active_enemy.templ.name, &active_enemy.current_hp);
      }
    }
  }
}

// --- INITIALIZATION ---

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

  int input_col = (COLS - 24) / 2;
  move(row + 3, input_col);

  echo();
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
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);

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
