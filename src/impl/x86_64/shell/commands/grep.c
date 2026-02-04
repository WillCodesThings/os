// grep command - filter lines matching pattern

#include <shell/commands.h>
#include <shell/print.h>

// String length
static int grep_strlen(const char *s)
{
    int len = 0;
    while (s[len])
        len++;
    return len;
}

// Substring search
static int str_contains(const char *haystack, const char *needle)
{
    int needle_len = grep_strlen(needle);
    int haystack_len = grep_strlen(haystack);

    for (int i = 0; i <= haystack_len - needle_len; i++)
    {
        int match = 1;
        for (int j = 0; j < needle_len; j++)
        {
            if (haystack[i + j] != needle[j])
            {
                match = 0;
                break;
            }
        }
        if (match)
            return 1;
    }
    return 0;
}

void cmd_grep(int argc, char **argv)
{
    if (argc < 2)
    {
        print_str("Usage: grep <pattern>\n");
        print_str("Filters input lines containing the pattern.\n");
        print_str("Example: ls | grep txt\n");
        return;
    }

    const char *pattern = argv[1];
    const char *input = shell_get_stdin();

    if (!input || shell_get_stdin_len() == 0)
    {
        print_str("No input to filter. Use with pipe: command | grep pattern\n");
        return;
    }

    const char *line_start = input;
    const char *p = input;

    while (*p)
    {
        if (*p == '\n' || *(p + 1) == '\0')
        {
            int line_len = p - line_start;
            if (*p != '\n')
                line_len++;

            char line[256];
            int copy_len = line_len < 255 ? line_len : 255;
            for (int i = 0; i < copy_len; i++)
            {
                line[i] = line_start[i];
            }
            line[copy_len] = '\0';

            if (str_contains(line, pattern))
            {
                print_str(line);
                print_str("\n");
            }

            line_start = p + 1;
        }
        p++;
    }
}
