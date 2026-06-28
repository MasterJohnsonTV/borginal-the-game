#include "input.h"

#ifdef _WIN32
#include <conio.h>

char get_keypress(void) { return (char)_getch(); }

#else
#include <termios.h>
#include <unistd.h>

char get_keypress(void) {
  struct termios old_attrs, raw_attrs;
  tcgetattr(STDIN_FILENO, &old_attrs);

  raw_attrs = old_attrs;
  raw_attrs.c_lflag &= ~(ICANON | ECHO);
  raw_attrs.c_cc[VMIN] = 1;
  raw_attrs.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSANOW, &raw_attrs);

  char c;
  read(STDIN_FILENO, &c, 1);

  // If an escape sequence begins (like arrow keys)
  if (c == '\x1b') {
    // Change to non-blocking read to see if more bytes are waiting
    raw_attrs.c_cc[VMIN] = 0;
    raw_attrs.c_cc[VTIME] = 0; // 0 timeout means don't wait if nothing is there
    tcsetattr(STDIN_FILENO, TCSANOW, &raw_attrs);

    char seq[2];
    int n1 = read(STDIN_FILENO, &seq[0], 1);
    int n2 = read(STDIN_FILENO, &seq[1], 1);

    // If we successfully read the rest of the arrow sequence
    if (n1 > 0 && n2 > 0 && seq[0] == '[') {
      switch (seq[1]) {
      case 'A':
        c = 'w';
        break; // Up
      case 'B':
        c = 's';
        break; // Down
      case 'C':
        c = 'd';
        break; // Right
      case 'D':
        c = 'a';
        break; // Left
      default:
        c = 0;
        break;
      }
    } else {
      // It was just a lone Escape key or incomplete sequence
      c = 0;
    }
  }

  tcsetattr(STDIN_FILENO, TCSANOW, &old_attrs);
  return c;
}
#endif
