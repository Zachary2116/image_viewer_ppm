#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>

int test_render(SDL_Renderer *renderer) {
    int count = 1;
    /* SDL_SetRenderDrawColor: sets the color used for drawing ops (r, g, b, alpha) */
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); /* red */

    /* SDL_RenderClear: fills the entire screen with the current draw color */
    SDL_RenderClear(renderer);

    /* SDL_RenderPresent: swaps the back buffer to the screen (like glSwapBuffers) */
    SDL_RenderPresent(renderer);
    /* shrinking bar countdown over 10 seconds */
    Uint64 start = SDL_GetTicks();
    int duration = 7000; /* ms */
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

int main(int argc, char *argv[]){
    int max_color_value = 255;
    int scale = 1;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.ppm>\n", argv[0]);
        return 1;
    }

    /* Open the file */
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("Failed to open file");
        return 1;
    }

    /* detect P3 or P6 */
    char mode[3];
    if (fscanf(fp, "%2s", mode) != 1 || (mode[0] != 'P') || (mode[1] != '3' && mode[1] != '6')) {
        fprintf(stderr, "Unsupported PPM format (only P3/P6 supported)\n");
        fclose(fp);
        return 1;
    }
    int is_p3 = (mode[1] == '3');

    /* read header comments (lines starting with '#') */
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#') {
            printf("PPM comment: %s", line);
        } else {
            fseek(fp, -strlen(line), SEEK_CUR);
            break;
        }
    }

    if (feof(fp)) {
        fprintf(stderr, "Unexpected end of file while reading header\n");
        fclose(fp);
        return 1;
    }

    /* read width, height, max color value from header */
    int img_width = 0, img_height = 0;
    if (fscanf(fp, "%d %d", &img_width, &img_height) != 2) {
        fprintf(stderr, "Failed to read PPM header\n");
        fclose(fp);
        return 1;
    } else {
        printf("Image dimensions: %dx%d, max color value: %d\n", img_width, img_height, max_color_value);
    }

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *pwindow = SDL_CreateWindow("Image Viewer", img_width * scale, img_height * scale, 0);
    SDL_Renderer *renderer = SDL_CreateRenderer(pwindow, NULL);

    int x, y;
    unsigned char rgb[3];

    for (y = 0; y < img_height; y++) {
        for (x = 0; x < img_width; x++) {
            if (is_p3) {
                int r, g, b;
                if (fscanf(fp, "%d %d %d", &r, &g, &b) != 3) {
                    printf("Error: failed to read pixel at (%d, %d)\n", x, y);
                    fclose(fp);
                    return 1;
                }
                rgb[0] = r; rgb[1] = g; rgb[2] = b;
            } else {
                if (fread(rgb, 1, 3, fp) != 3) {
                    printf("Error: failed to read pixel at (%d, %d)\n", x, y);
                    fclose(fp);
                    return 1;
                }
            }

            SDL_FRect rect = { x * scale, y * scale, scale, scale };
            SDL_SetRenderDrawColor(renderer, rgb[0], rgb[1], rgb[2], 255);
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    SDL_RenderPresent(renderer);
    fclose(fp);

    SDL_Event event;
    while (SDL_WaitEvent(&event) && event.type != SDL_EVENT_QUIT);

    SDL_DestroyWindow(pwindow);
    SDL_Quit();
    return 0;
}

/* ascii_to_binary: later support for P3 */
int ascii_to_binary(const char *input_path, const char *output_path);
