#include "terminal_sixel.h"

#include <stdlib.h>
#include <string.h>

static int clamp_sixel_int(int value, int low, int high)
{
    if(value < low)
        return low;
    if(value > high)
        return high;
    return value;
}

static void remove_sixel_image(TerminalState *terminal, int index)
{
    SixelImage *image;

    if(terminal == NULL || index < 0 || index >= terminal->sixel_count)
        return;
    image = terminal->sixel_images + index;
    if(image->pixels != NULL) {
        terminal->sixel_total_pixels -= image->width * image->height;
        if(terminal->sixel_total_pixels < 0)
            terminal->sixel_total_pixels = 0;
        free(image->pixels);
    }
    if(index + 1 < terminal->sixel_count)
        memmove(terminal->sixel_images + index,
                terminal->sixel_images + index + 1,
                (size_t)(terminal->sixel_count - index - 1) *
                    sizeof(SixelImage));
    terminal->sixel_count--;
}

void terminal_sixel_clear_images(TerminalState *terminal, int alternate_filter)
{
    int i;

    if(terminal == NULL)
        return;
    for(i = terminal->sixel_count - 1; i >= 0; i--) {
        if(alternate_filter < 0 ||
           terminal->sixel_images[i].alternate_screen == alternate_filter)
            remove_sixel_image(terminal, i);
    }
}

static int sixel_image_intersects_range(const TerminalState *terminal,
                                        const SixelImage *image, int start,
                                        int end)
{
    int rows_used;
    int cols_used;
    int row;

    if(terminal == NULL || image == NULL || terminal->cols <= 0 ||
       image->alternate_screen != terminal->alternate_screen || start >= end)
        return 0;
    rows_used = (image->height + 5) / 6;
    cols_used = (image->width + 5) / 6;
    if(rows_used < 1)
        rows_used = 1;
    if(cols_used < 1)
        cols_used = 1;
    for(row = image->row; row < image->row + rows_used; row++) {
        int row_start;
        int row_end;

        if(row < 0 || row >= terminal->rows)
            continue;
        row_start = row * terminal->cols + image->col;
        row_end = row_start + cols_used;
        if(row_start < row * terminal->cols)
            row_start = row * terminal->cols;
        if(row_end > (row + 1) * terminal->cols)
            row_end = (row + 1) * terminal->cols;
        if(end > row_start && start < row_end)
            return 1;
    }
    return 0;
}

void terminal_sixel_prune_scrollback(TerminalState *terminal)
{
    int i;

    if(terminal == NULL)
        return;
    for(i = terminal->sixel_count - 1; i >= 0; i--) {
        SixelImage *image = terminal->sixel_images + i;

        if(image->alternate_screen || image->row >= 0)
            continue;
        if(image->row < -terminal->scrollback_count)
            remove_sixel_image(terminal, i);
    }
}

void terminal_sixel_clear_range(TerminalState *terminal, int start, int end)
{
    int i;

    if(terminal == NULL)
        return;
    start = clamp_sixel_int(start, 0, terminal->cols * terminal->rows);
    end = clamp_sixel_int(end, 0, terminal->cols * terminal->rows);
    if(start >= end)
        return;
    for(i = terminal->sixel_count - 1; i >= 0; i--) {
        if(sixel_image_intersects_range(terminal, terminal->sixel_images + i,
                                        start, end))
            remove_sixel_image(terminal, i);
    }
}

void terminal_sixel_shift(TerminalState *terminal, int top, int bottom,
                          int delta)
{
    int i;

    if(terminal == NULL || delta == 0)
        return;
    for(i = terminal->sixel_count - 1; i >= 0; i--) {
        SixelImage *image = terminal->sixel_images + i;
        int in_scrollback =
            delta < 0 && top == 0 && !terminal->alternate_screen &&
            !image->alternate_screen && image->row < 0;

        if(image->alternate_screen != terminal->alternate_screen)
            continue;
        if(!in_scrollback && (image->row < top || image->row > bottom))
            continue;
        image->row += delta;
        if(image->row < top || image->row > bottom) {
            if(delta < 0 && top == 0 && !terminal->alternate_screen &&
               !image->alternate_screen && image->row < 0 &&
               image->row >= -terminal->scrollback_count)
                continue;
            remove_sixel_image(terminal, i);
        }
    }
}

static int add_sixel_image(TerminalState *terminal, const int *pixels,
                           int stride, int width, int height,
                           int pixel_aspect_num, int pixel_aspect_den)
{
    SixelImage *images;
    int *copy;
    int row;

    if(terminal == NULL || pixels == NULL || width <= 0 || height <= 0 ||
       stride < width)
        return 0;
    if((size_t)width * (size_t)height > MAX_SIXEL_IMAGE_PIXELS)
        return 0;
    while(terminal->sixel_count >= MAX_SIXEL_IMAGES ||
          terminal->sixel_total_pixels + width * height >
              MAX_SIXEL_TOTAL_PIXELS)
        remove_sixel_image(terminal, 0);
    if(terminal->sixel_count >= terminal->sixel_capacity) {
        int capacity = terminal->sixel_capacity > 0
                           ? terminal->sixel_capacity * 2
                           : 8;

        if(capacity > MAX_SIXEL_IMAGES)
            capacity = MAX_SIXEL_IMAGES;
        images = realloc(terminal->sixel_images,
                         (size_t)capacity * sizeof(SixelImage));
        if(images == NULL)
            return 0;
        terminal->sixel_images = images;
        terminal->sixel_capacity = capacity;
    }
    copy = malloc((size_t)width * (size_t)height * sizeof(int));
    if(copy == NULL)
        return 0;
    for(row = 0; row < height; row++) {
        memcpy(copy + row * width, pixels + row * stride,
               (size_t)width * sizeof(int));
    }
    terminal->sixel_images[terminal->sixel_count].col = terminal->cursor_col;
    terminal->sixel_images[terminal->sixel_count].row = terminal->cursor_row;
    terminal->sixel_images[terminal->sixel_count].alternate_screen =
        terminal->alternate_screen;
    terminal->sixel_images[terminal->sixel_count].width = width;
    terminal->sixel_images[terminal->sixel_count].height = height;
    terminal->sixel_images[terminal->sixel_count].pixel_aspect_num =
        pixel_aspect_num > 0 ? pixel_aspect_num : 1;
    terminal->sixel_images[terminal->sixel_count].pixel_aspect_den =
        pixel_aspect_den > 0 ? pixel_aspect_den : 1;
    terminal->sixel_images[terminal->sixel_count].pixels = copy;
    terminal->sixel_count++;
    terminal->sixel_total_pixels += width * height;
    return 1;
}

int terminal_sixel_finish(TerminalState *terminal, const char *payload)
{
    TerminalPaneSixelImage image;
    int rows_used = 0;

    if(terminal == NULL || payload == NULL)
        return 0;
    rows_used = DecodeTerminalPaneSixel(&image, payload, COLOR_TRUE_RGB, NULL,
                                        NULL);
    if(rows_used > 0 &&
       !add_sixel_image(terminal, image.pixels, image.width, image.width,
                        image.height, image.pixel_aspect_num,
                        image.pixel_aspect_den))
        rows_used = 0;
    FreeTerminalPaneSixelImage(&image);
    return rows_used;
}
