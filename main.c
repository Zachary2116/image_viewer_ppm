#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <SDL3/SDL.h>

#define DEFAULT_RESOLUTION_WIDTH  1280
#define DEFAULT_RESOLUTION_HEIGHT 800
#define TEST_COLOR_SHIFT_FACTOR 2

int test_render(SDL_Renderer *renderer) {
    int count = 1;
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    Uint64 start = SDL_GetTicks();
    int duration = 7000;
    int bar_height = 20;
    SDL_Event event;
    while (count) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) return 1;
        }
        int elapsed = (int)(SDL_GetTicks() - start);
        if (elapsed >= duration) break;
        float fraction = 1.0f - (float)elapsed / duration;
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_FRect bar = { 0, 0, 1000 * fraction, bar_height };
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(renderer, &bar);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    return 0;
}

unsigned char *load_ppm(const char *path, int *width, int *height) {
    // Open the file in binary mode to handle both P3 and P6 formats correctly.
    FILE *fp = fopen(path, "rb");
    if (!fp) { perror("Failed to open file"); return NULL; }
    // Read mode either "P3" or "P6"
    char mode[3];
    if (fscanf(fp, "%2s", mode) != 1 || mode[0] != 'P' ||
        (mode[1] != '3' && mode[1] != '6')) {
        // Invalid format
        fprintf(stderr, "Unsupported PPM format (only P3/P6 supported)\n");
        fclose(fp); return NULL;
    }
    int is_p3 = (mode[1] == '3');

    /* skip comment lines */
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#') printf("PPM comment: %s", line);
        else { fseek(fp, -(long)strlen(line), SEEK_CUR); break; }
    }
    // Check for EOF after skipping comments
    if (feof(fp)) {
        fprintf(stderr, "Unexpected end of file while reading header\n");
        fclose(fp); return NULL;
    }
// Read image dimensions and max color value from the header.
    int img_width = 0, img_height = 0, max_val = 0;
    if (fscanf(fp, "%d %d %d", &img_width, &img_height, &max_val) != 3) {
        fprintf(stderr, "Failed to read PPM header\n");
        fclose(fp); return NULL;
    }
    // Validate header values
    if (img_width <= 0 || img_height <= 0 || max_val <= 0 || max_val > 255) {
        fprintf(stderr, "Invalid PPM header values\n");
        fclose(fp); return NULL;
    }   
    if (!is_p3) fgetc(fp); /* consume single whitespace before binary data */

    printf("Image dimensions: %dx%d, max color value: %d\n", img_width, img_height, max_val);

    unsigned char *pixels = malloc(img_width * img_height * 3);
    if (!pixels) { fclose(fp); return NULL; }

    for (int y = 0; y < img_height; y++) {
        // Calculate the pointer to the start of the current row in the pixel buffer.
        for (int x = 0; x < img_width; x++) {
            unsigned char *p = &pixels[(y * img_width + x) * 3];
            // For P3, read ASCII integers; for P6, read binary bytes.
            if (is_p3) {
                int r, g, b;
                if (fscanf(fp, "%d %d %d", &r, &g, &b) != 3) {
                    fprintf(stderr, "Error reading pixel (%d,%d)\n", x, y);
                    free(pixels); fclose(fp); return NULL;
                }
                p[0] = r; p[1] = g; p[2] = b;
            // For P6, we can read all three color components in one call to fread for efficiency.
            } else {
                if (fread(p, 1, 3, fp) != 3) {
                    fprintf(stderr, "Error reading pixel (%d,%d)\n", x, y);
                    free(pixels); fclose(fp); return NULL;
                }
            }
        }
    }
    fclose(fp);
    *width  = img_width;
    *height = img_height;
    return pixels;
}

/* Uploads a packed RGB pixel buffer to a new GPU texture. Returns NULL on failure. */
SDL_Texture *create_texture_from_pixels(SDL_Renderer *renderer,
                                        unsigned char *pixels,
                                        int width, int height) {
// Create an SDL_Texture with the same dimensions as the image, using RGB24 format.
    SDL_Texture *tex = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STATIC,
        width, height);
    if (!tex) return NULL;
    SDL_UpdateTexture(tex, NULL, pixels, width * 3);
    return tex;
}

/* Returns a centered, aspect-correct destination rect.
   zoom=1 means fit-to-window; pan offsets shift the image. */
SDL_FRect fit_rect(int img_w, int img_h, int win_w, int win_h,
                   float zoom, float pan_x, float pan_y) {
    float scale = zoom * fminf((float)win_w / img_w, (float)win_h / img_h);
    float w = img_w * scale;
    float h = img_h * scale;
    // Center the image in the window, applying pan offsets. 
    //Math works by starting with the top-left corner of the image at (0,0),
    // then shifting it to the center of the window, and finally applying pan offsets. 
    return (SDL_FRect){
        (win_w - w) / 2.0f + pan_x,
        (win_h - h) / 2.0f + pan_y,
        w, h
    };
}

/* Render/event loop. Scroll=zoom, middle-drag=pan, 0=reset. */
void run_viewer(SDL_Renderer *renderer, SDL_Texture *texture,
                int img_w, int img_h, int win_w, int win_h) {
    // State for zoom/pan interaction
    float zoom = 1.0f, pan_x = 0, pan_y = 0;
    int running = 1;
    SDL_Event event;
    // Main loop: handle events, then render the texture with the current zoom/pan settings.
    while (running) {
        while (SDL_PollEvent(&event)) {
            // Handle quit, mouse wheel for zoom, middle mouse button for pan, and window resize.
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    running = 0; break;
                case SDL_EVENT_MOUSE_WHEEL:
                    zoom *= (event.wheel.y > 0) ? 1.1f : 0.9f;
                    if (zoom < 0.05f) zoom = 0.05f;
                    break;

                case SDL_EVENT_WINDOW_RESIZED:
                    win_w = event.window.data1;
                    win_h = event.window.data2;
                    break;
            }
        }

        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);
        SDL_FRect dst = fit_rect(img_w, img_h, win_w, win_h, zoom, pan_x, pan_y);
 
        SDL_RenderTexture(renderer, texture, NULL, &dst);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
}

int main(int argc, char *argv[]) {
    /* --- OLD MAIN BODY (replaced by functions above) ---

    int max_color_value = 255;
    int scale = 1;

    if (argc < 2) { ... }

    FILE *fp = fopen(argv[1], "rb");
    ... PPM parsing ...

    SDL_Window *pwindow = SDL_CreateWindow("Image Viewer", img_width * scale, img_height * scale, 0);
    SDL_Renderer *renderer = SDL_CreateRenderer(pwindow, NULL);

    for (y = 0; y < img_height; y++) {
        for (x = 0; x < img_width; x++) {
            ... read pixel ...
            SDL_FRect rect = { x * scale, y * scale, scale, scale };
            SDL_SetRenderDrawColor(renderer, rgb[0], rgb[1], rgb[2], 255);
            SDL_RenderFillRect(renderer, &rect);   // one draw call per pixel
        }
    }

    SDL_RenderPresent(renderer);
    fclose(fp);
    while (SDL_WaitEvent(&event) && event.type != SDL_EVENT_QUIT);
    SDL_DestroyWindow(pwindow);
    SDL_Quit();
    return 0;

    --- END OLD MAIN BODY --- */

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.ppm>\n", argv[0]);
        return 1;
    }

    int img_width = 0, img_height = 0;
    unsigned char *pixels = load_ppm(argv[1], &img_width, &img_height);
    if (!pixels) return 1;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window   *pwindow  = SDL_CreateWindow("Image Viewer",
                                DEFAULT_RESOLUTION_WIDTH, DEFAULT_RESOLUTION_HEIGHT,
                                SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = SDL_CreateRenderer(pwindow, NULL);

    SDL_Texture *texture = create_texture_from_pixels(renderer, pixels, img_width, img_height);
    free(pixels);

    run_viewer(renderer, texture, img_width, img_height,
               DEFAULT_RESOLUTION_WIDTH, DEFAULT_RESOLUTION_HEIGHT);

    SDL_DestroyTexture(texture);
    SDL_DestroyWindow(pwindow);
    SDL_Quit();
    return 0;
}
