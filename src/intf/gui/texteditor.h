#pragma once
#include <stdint.h>
#include <stddef.h>
#include "gui/window.h"

// Text editor constants
#define TEXTEDITOR_MAX_LINES 64
#define TEXTEDITOR_MAX_LINE_LEN 80
#define TEXTEDITOR_WIDTH 400
#define TEXTEDITOR_HEIGHT 250

// Text editor structure
typedef struct text_editor {
    window_t *window;
    char lines[TEXTEDITOR_MAX_LINES][TEXTEDITOR_MAX_LINE_LEN];
    int line_count;
    int cursor_line;
    int cursor_col;
    int scroll_offset;
    char filename[64];
    int modified;
} text_editor_t;

// Create a new text editor window
text_editor_t *texteditor_create(const char *title, int32_t x, int32_t y);

// Close the text editor
void texteditor_close(text_editor_t *editor);

// Handle keyboard input
void texteditor_handle_key(text_editor_t *editor, char c);

// Load file into editor
int texteditor_load(text_editor_t *editor, const char *path);

// Save editor content to file
int texteditor_save(text_editor_t *editor, const char *path);

// Refresh the display
void texteditor_refresh(text_editor_t *editor);
