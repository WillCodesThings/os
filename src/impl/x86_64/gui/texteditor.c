#include "gui/texteditor.h"
#include "gui/window.h"
#include "memory/heap.h"
#include "utils/memory.h"
#include "utils/string.h"
#include "graphics/font.h"
#include "fs/vfs.h"

// Colors
#define COLOR_BG        0x1E1E1E
#define COLOR_TEXT      0xD4D4D4
#define COLOR_CURSOR    0xFFFFFF
#define COLOR_LINE_NUM  0x858585
#define COLOR_SELECTION 0x264F78

// Layout
#define LINE_NUM_WIDTH  40
#define CHAR_WIDTH      8
#define CHAR_HEIGHT     8
#define LINE_HEIGHT     10

// Paint callback
static void texteditor_paint(window_t *win) {
    text_editor_t *editor = (text_editor_t *)win->user_data;
    if (!editor) return;

    // Clear background
    window_clear(win, COLOR_BG);

    // Draw line number gutter
    window_fill_rect(win, 0, 0, LINE_NUM_WIDTH - 4, win->content_height, 0x252526);

    // Calculate visible lines
    int visible_lines = win->content_height / LINE_HEIGHT;
    int start_line = editor->scroll_offset;
    int end_line = start_line + visible_lines;
    if (end_line > editor->line_count) end_line = editor->line_count;

    // Draw each visible line
    for (int i = start_line; i < end_line; i++) {
        int y = (i - start_line) * LINE_HEIGHT + 2;

        // Draw line number
        char num_buf[8];
        int n = i + 1;
        int pos = 0;
        if (n >= 100) num_buf[pos++] = '0' + (n / 100) % 10;
        if (n >= 10) num_buf[pos++] = '0' + (n / 10) % 10;
        num_buf[pos++] = '0' + n % 10;
        num_buf[pos] = '\0';

        // Draw line number chars manually to framebuffer
        int num_x = LINE_NUM_WIDTH - 12 - pos * CHAR_WIDTH;
        for (int c = 0; c < pos; c++) {
            char ch = num_buf[c];
            for (int cy = 0; cy < 8; cy++) {
                for (int cx = 0; cx < 8; cx++) {
                    if (font_8x8[(int)ch][cy] & (1 << (7 - cx))) {
                        window_put_pixel(win, num_x + c * CHAR_WIDTH + cx, y + cy, COLOR_LINE_NUM);
                    }
                }
            }
        }

        // Draw text content
        char *line = editor->lines[i];
        int x = LINE_NUM_WIDTH;
        for (int j = 0; line[j] && x < (int)win->content_width; j++) {
            char ch = line[j];
            if (ch >= 32 && ch < 127) {
                for (int cy = 0; cy < 8; cy++) {
                    for (int cx = 0; cx < 8; cx++) {
                        if (font_8x8[(int)ch][cy] & (1 << (7 - cx))) {
                            window_put_pixel(win, x + cx, y + cy, COLOR_TEXT);
                        }
                    }
                }
            }
            x += CHAR_WIDTH;
        }

        // Draw cursor on current line
        if (i == editor->cursor_line) {
            int cursor_x = LINE_NUM_WIDTH + editor->cursor_col * CHAR_WIDTH;
            window_fill_rect(win, cursor_x, y, 2, CHAR_HEIGHT, COLOR_CURSOR);
        }
    }

    // Status bar
    int status_y = win->content_height - LINE_HEIGHT - 2;
    window_fill_rect(win, 0, status_y, win->content_width, LINE_HEIGHT + 2, 0x007ACC);

    // Draw status text
    char status[80];
    int sp = 0;
    // "Ln X, Col Y"
    const char *ln_text = "Ln ";
    for (int i = 0; ln_text[i]; i++) status[sp++] = ln_text[i];
    int ln = editor->cursor_line + 1;
    if (ln >= 100) status[sp++] = '0' + (ln / 100) % 10;
    if (ln >= 10) status[sp++] = '0' + (ln / 10) % 10;
    status[sp++] = '0' + ln % 10;
    status[sp++] = ',';
    status[sp++] = ' ';
    const char *col_text = "Col ";
    for (int i = 0; col_text[i]; i++) status[sp++] = col_text[i];
    int col = editor->cursor_col + 1;
    if (col >= 100) status[sp++] = '0' + (col / 100) % 10;
    if (col >= 10) status[sp++] = '0' + (col / 10) % 10;
    status[sp++] = '0' + col % 10;
    if (editor->modified) {
        status[sp++] = ' ';
        status[sp++] = '[';
        status[sp++] = 'M';
        status[sp++] = 'o';
        status[sp++] = 'd';
        status[sp++] = 'i';
        status[sp++] = 'f';
        status[sp++] = 'i';
        status[sp++] = 'e';
        status[sp++] = 'd';
        status[sp++] = ']';
    }
    status[sp] = '\0';

    int status_x = 4;
    for (int i = 0; status[i] && status_x < (int)win->content_width; i++) {
        char ch = status[i];
        if (ch >= 32 && ch < 127) {
            for (int cy = 0; cy < 8; cy++) {
                for (int cx = 0; cx < 8; cx++) {
                    if (font_8x8[(int)ch][cy] & (1 << (7 - cx))) {
                        window_put_pixel(win, status_x + cx, status_y + 2 + cy, 0xFFFFFF);
                    }
                }
            }
        }
        status_x += CHAR_WIDTH;
    }
}

// Close callback
static void texteditor_on_close(window_t *win) {
    text_editor_t *editor = (text_editor_t *)win->user_data;
    if (editor) {
        kfree(editor);
    }
}

// Create text editor
text_editor_t *texteditor_create(const char *title, int32_t x, int32_t y) {
    text_editor_t *editor = (text_editor_t *)kcalloc(1, sizeof(text_editor_t));
    if (!editor) return 0;

    // Create window
    editor->window = window_create(title, x, y,
                                   TEXTEDITOR_WIDTH, TEXTEDITOR_HEIGHT,
                                   WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE |
                                   WINDOW_FLAG_CLOSABLE);

    if (!editor->window) {
        kfree(editor);
        return 0;
    }

    // Set callbacks and user data
    editor->window->user_data = editor;
    editor->window->on_paint = texteditor_paint;
    editor->window->on_close = texteditor_on_close;

    // Initialize with one empty line
    editor->line_count = 1;
    editor->cursor_line = 0;
    editor->cursor_col = 0;
    editor->scroll_offset = 0;
    editor->modified = 0;

    return editor;
}

// Handle keyboard input
void texteditor_handle_key(text_editor_t *editor, char c) {
    if (!editor) return;

    // Ctrl+S to save (ASCII 0x13)
    if (c == 0x13) {
        if (editor->filename[0]) {
            // Save to existing filename
            texteditor_save(editor, editor->filename);
            window_set_title(editor->window, editor->filename);
        } else {
            // No filename - save as "untitled.txt"
            texteditor_save(editor, "untitled.txt");
            memcpy(editor->filename, "untitled.txt", 13);
            window_set_title(editor->window, "untitled.txt (saved)");
        }
        window_invalidate(editor->window);
        return;
    }

    char *current_line = editor->lines[editor->cursor_line];
    int line_len = strlen(current_line);

    if (c == '\n') {
        // Insert new line
        if (editor->line_count < TEXTEDITOR_MAX_LINES - 1) {
            // Move lines down
            for (int i = editor->line_count; i > editor->cursor_line + 1; i--) {
                memcpy(editor->lines[i], editor->lines[i - 1], TEXTEDITOR_MAX_LINE_LEN);
            }

            // Split current line at cursor
            char *new_line = editor->lines[editor->cursor_line + 1];
            memcpy(new_line, current_line + editor->cursor_col, line_len - editor->cursor_col + 1);
            current_line[editor->cursor_col] = '\0';

            editor->line_count++;
            editor->cursor_line++;
            editor->cursor_col = 0;
            editor->modified = 1;
        }
    } else if (c == '\b') {
        // Backspace
        if (editor->cursor_col > 0) {
            // Delete character before cursor
            for (int i = editor->cursor_col - 1; i < line_len; i++) {
                current_line[i] = current_line[i + 1];
            }
            editor->cursor_col--;
            editor->modified = 1;
        } else if (editor->cursor_line > 0) {
            // Merge with previous line
            char *prev_line = editor->lines[editor->cursor_line - 1];
            int prev_len = strlen(prev_line);

            if (prev_len + line_len < TEXTEDITOR_MAX_LINE_LEN) {
                // Append current line to previous
                memcpy(prev_line + prev_len, current_line, line_len + 1);

                // Move lines up
                for (int i = editor->cursor_line; i < editor->line_count - 1; i++) {
                    memcpy(editor->lines[i], editor->lines[i + 1], TEXTEDITOR_MAX_LINE_LEN);
                }
                editor->lines[editor->line_count - 1][0] = '\0';

                editor->line_count--;
                editor->cursor_line--;
                editor->cursor_col = prev_len;
                editor->modified = 1;
            }
        }
    } else if (c >= 32 && c < 127) {
        // Insert printable character
        if (line_len < TEXTEDITOR_MAX_LINE_LEN - 1) {
            // Shift characters right
            for (int i = line_len + 1; i > editor->cursor_col; i--) {
                current_line[i] = current_line[i - 1];
            }
            current_line[editor->cursor_col] = c;
            editor->cursor_col++;
            editor->modified = 1;
        }
    }

    // Ensure cursor is visible (scroll if needed)
    int visible_lines = (editor->window->content_height - LINE_HEIGHT - 4) / LINE_HEIGHT;
    if (editor->cursor_line < editor->scroll_offset) {
        editor->scroll_offset = editor->cursor_line;
    } else if (editor->cursor_line >= editor->scroll_offset + visible_lines) {
        editor->scroll_offset = editor->cursor_line - visible_lines + 1;
    }

    window_invalidate(editor->window);
}

// Load file into editor
int texteditor_load(text_editor_t *editor, const char *path) {
    if (!editor || !path) return -1;

    vfs_node_t *file = vfs_open(path, VFS_READ);
    if (!file) return -1;

    // Read file content
    uint8_t buffer[4096];
    int bytes = vfs_read(file, 0, 4096, buffer);
    vfs_close(file);

    if (bytes <= 0) return -1;

    // Parse into lines
    editor->line_count = 0;
    editor->cursor_line = 0;
    editor->cursor_col = 0;
    int line_pos = 0;

    for (int i = 0; i < bytes && editor->line_count < TEXTEDITOR_MAX_LINES; i++) {
        if (buffer[i] == '\n') {
            editor->lines[editor->line_count][line_pos] = '\0';
            editor->line_count++;
            line_pos = 0;
        } else if (buffer[i] >= 32 && buffer[i] < 127) {
            if (line_pos < TEXTEDITOR_MAX_LINE_LEN - 1) {
                editor->lines[editor->line_count][line_pos++] = buffer[i];
            }
        }
    }

    // Handle last line without newline
    if (line_pos > 0) {
        editor->lines[editor->line_count][line_pos] = '\0';
        editor->line_count++;
    }

    if (editor->line_count == 0) {
        editor->line_count = 1;
    }

    // Copy filename
    const char *filename = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/') filename = p + 1;
    }
    int fn_len = strlen(filename);
    if (fn_len > 63) fn_len = 63;
    memcpy(editor->filename, filename, fn_len);
    editor->filename[fn_len] = '\0';

    window_set_title(editor->window, filename);
    editor->modified = 0;
    window_invalidate(editor->window);

    return 0;
}

// Save editor content to file
int texteditor_save(text_editor_t *editor, const char *path) {
    if (!editor || !path) return -1;

    vfs_node_t *file = vfs_open(path, VFS_WRITE);
    if (!file) {
        // Try to create the file
        vfs_node_t *root = vfs_get_root();
        if (root && root->create) {
            const char *filename = path;
            for (const char *p = path; *p; p++) {
                if (*p == '/') filename = p + 1;
            }
            root->create(root, filename, VFS_FILE);
            file = vfs_open(path, VFS_WRITE);
        }
    }

    if (!file) return -1;

    // Build content buffer
    uint8_t buffer[4096];
    int pos = 0;

    for (int i = 0; i < editor->line_count && pos < 4000; i++) {
        char *line = editor->lines[i];
        int len = strlen(line);
        if (pos + len + 1 < 4096) {
            memcpy(buffer + pos, line, len);
            pos += len;
            buffer[pos++] = '\n';
        }
    }

    int written = vfs_write(file, 0, pos, buffer);
    vfs_close(file);

    if (written > 0) {
        editor->modified = 0;
        window_invalidate(editor->window);
    }

    return written > 0 ? 0 : -1;
}

// Close text editor
void texteditor_close(text_editor_t *editor) {
    if (editor && editor->window) {
        window_destroy(editor->window);
    }
}

// Refresh display
void texteditor_refresh(text_editor_t *editor) {
    if (editor && editor->window) {
        window_invalidate(editor->window);
    }
}
