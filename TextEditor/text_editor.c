#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <string.h>
#include <stdbool.h>
#include <windows.h>
#include <commdlg.h>

#define MAX_VISIBLE_LINES 3
#define MAX_LINE_BYTES    1024
#define PANEL_WIDTH       480
#define PANEL_HEIGHT      128
#define PANEL_X           ((576 - PANEL_WIDTH) / 2)
#define PANEL_Y           (480 - PANEL_HEIGHT - 24)

#define MARGIN_LEFT       20
#define MARGIN_RIGHT      20
#define MARGIN_TOP        15
#define LINE_SPACING      2

// ---------- UTF-8 helpers ----------
static int utf8_char_len(const char *s) {
    unsigned char c = (unsigned char)*s;
    if (c < 0x80) return 1;
    if (c < 0xC0) return 1;
    if (c < 0xE0) return 2;
    if (c < 0xF0) return 3;
    return 4;
}

static int utf8_backspace(char *buf, int byte_len) {
    if (byte_len <= 0) return 0;
    int pos = byte_len - 1;
    while (pos > 0 && (buf[pos] & 0xC0) == 0x80) pos--;
    buf[pos] = '\0';
    return pos;
}

// ---------- File dialog helpers (Win32) ----------
static bool open_file_dialog(char *out_path, size_t out_len) {
    OPENFILENAMEA ofn;
    char szFile[260] = {0};
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Text Files\0*.TXT\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn) == TRUE) {
        strncpy(out_path, ofn.lpstrFile, out_len);
        return true;
    }
    return false;
}

static bool save_file_dialog(char *out_path, size_t out_len) {
    OPENFILENAMEA ofn;
    char szFile[260] = "message.txt";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Text Files\0*.TXT\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileNameA(&ofn) == TRUE) {
        strncpy(out_path, ofn.lpstrFile, out_len);
        return true;
    }
    return false;
}

// ---------- Dynamic line array ----------
typedef struct {
    char **data;
    int count;
    int capacity;
} LineArray;

static void la_init(LineArray *la) {
    la->data = NULL;
    la->count = la->capacity = 0;
}

static void la_add_empty(LineArray *la) {
    if (la->count >= la->capacity) {
        la->capacity = la->capacity ? la->capacity * 2 : 4;
        la->data = (char**)realloc(la->data, la->capacity * sizeof(char*));
    }
    la->data[la->count] = (char*)calloc(1, MAX_LINE_BYTES);
    la->count++;
}

static void la_free(LineArray *la) {
    for (int i = 0; i < la->count; i++) free(la->data[i]);
    free(la->data);
    la->data = NULL;
    la->count = la->capacity = 0;
}

// ---------- Main program ----------
int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    SDL_Window* window = SDL_CreateWindow(
        "Shinzo Text Editor",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        576, 480, SDL_WINDOW_SHOWN
    );
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    SDL_Texture* panelTexture = IMG_LoadTexture(renderer, "../assets/ui/message_panel.png");
    if (!panelTexture) SDL_Log("Failed to load panel: %s", IMG_GetError());

    TTF_Font* font = TTF_OpenFont("../assets/ui/fonts/main.ttf", 25);
    if (!font) SDL_Log("Failed to load font: %s", TTF_GetError());

    LineArray lines;
    la_init(&lines);
    la_add_empty(&lines);

    int first_visible = 0;
    bool show_cursor = true;
    Uint32 blink_timer = SDL_GetTicks();

    SDL_Rect import_btn = { 10, 10, 90, 28 };
    SDL_Rect export_btn = { 110, 10, 90, 28 };
    bool import_hover = false, export_hover = false;

    SDL_StartTextInput();

    int running = 1;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = 0;
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        int mx = event.button.x, my = event.button.y;
                        if (mx >= import_btn.x && mx < import_btn.x+import_btn.w &&
                            my >= import_btn.y && my < import_btn.y+import_btn.h) {
                            char path[260];
                            if (open_file_dialog(path, sizeof(path))) {
                                FILE *f = fopen(path, "r");
                                if (f) {
                                    la_free(&lines);
                                    la_init(&lines);
                                    char buffer[MAX_LINE_BYTES];
                                    while (fgets(buffer, sizeof(buffer), f)) {
                                        size_t len = strlen(buffer);
                                        if (len > 0 && buffer[len-1] == '\n') buffer[len-1] = '\0';
                                        la_add_empty(&lines);
                                        strncpy(lines.data[lines.count-1], buffer, MAX_LINE_BYTES-1);
                                    }
                                    fclose(f);
                                    if (lines.count == 0) la_add_empty(&lines);
                                    if (lines.count > MAX_VISIBLE_LINES)
                                        first_visible = lines.count - MAX_VISIBLE_LINES;
                                    else
                                        first_visible = 0;
                                }
                            }
                        }
                        else if (mx >= export_btn.x && mx < export_btn.x+export_btn.w &&
                                 my >= export_btn.y && my < export_btn.y+export_btn.h) {
                            char path[260];
                            if (save_file_dialog(path, sizeof(path))) {
                                FILE *f = fopen(path, "w");
                                if (f) {
                                    for (int i = 0; i < lines.count; i++)
                                        fprintf(f, "%s\n", lines.data[i]);
                                    fclose(f);
                                }
                            }
                        }
                    }
                    break;

                case SDL_TEXTINPUT: {
                    size_t add_len = strlen(event.text.text);
                    char *last = lines.data[lines.count-1];
                    int cur_len = strlen(last);
                    if (cur_len + add_len < MAX_LINE_BYTES-2) {
                        strcat(last, event.text.text);
                        int w, h;
                        TTF_SizeUTF8(font, last, &w, &h);
                        const int max_width = PANEL_WIDTH - MARGIN_LEFT - MARGIN_RIGHT;
                        if (w > max_width) {
                            const char *p = last;
                            const char *safe_end = p;
                            while (*p) {
                                int ch_len = utf8_char_len(p);
                                char temp[MAX_LINE_BYTES];
                                int prefix_bytes = p - last + ch_len;
                                if (prefix_bytes >= (int)sizeof(temp)) break;
                                memcpy(temp, last, prefix_bytes);
                                temp[prefix_bytes] = '\0';
                                TTF_SizeUTF8(font, temp, &w, &h);
                                if (w > max_width) break;
                                p += ch_len;
                                safe_end = p;
                            }
                            char new_line[MAX_LINE_BYTES] = {0};
                            if (*safe_end) {
                                strncpy(new_line, safe_end, MAX_LINE_BYTES-1);
                                *((char*)safe_end) = '\0';
                            }
                            la_add_empty(&lines);
                            strncpy(lines.data[lines.count-1], new_line, MAX_LINE_BYTES-1);
                            if (lines.count - first_visible > MAX_VISIBLE_LINES)
                                first_visible = lines.count - MAX_VISIBLE_LINES;
                        }
                    }
                    break;
                }

                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_BACKSPACE) {
                        char *last = lines.data[lines.count-1];
                        int len = strlen(last);
                        if (len > 0) {
                            utf8_backspace(last, len);
                        } else if (lines.count > 1) {
                            free(lines.data[lines.count-1]);
                            lines.count--;
                            if (first_visible > 0 && first_visible >= lines.count)
                                first_visible = lines.count - MAX_VISIBLE_LINES;
                            if (first_visible < 0) first_visible = 0;
                        }
                    }
                    else if (event.key.keysym.sym == SDLK_RETURN) {
                        la_add_empty(&lines);
                        if (lines.count - first_visible > MAX_VISIBLE_LINES)
                            first_visible = lines.count - MAX_VISIBLE_LINES;
                    }
                    else if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = 0;
                    }
                    else if (event.key.keysym.sym == SDLK_UP) {
                        if (first_visible > 0) first_visible--;
                    }
                    else if (event.key.keysym.sym == SDLK_DOWN) {
                        if (first_visible < lines.count - MAX_VISIBLE_LINES)
                            first_visible++;
                    }
                    break;
            }
        }

        // Мигание курсора
        if (SDL_GetTicks() - blink_timer >= 530) {
            show_cursor = !show_cursor;
            blink_timer = SDL_GetTicks();
        }

        int mx, my;
        SDL_GetMouseState(&mx, &my);
        import_hover = (mx >= import_btn.x && mx < import_btn.x+import_btn.w &&
                        my >= import_btn.y && my < import_btn.y+import_btn.h);
        export_hover = (mx >= export_btn.x && mx < export_btn.x+export_btn.w &&
                        my >= export_btn.y && my < export_btn.y+export_btn.h);

        // Отрисовка
        SDL_SetRenderDrawColor(renderer, 10, 10, 40, 255);   // тёмно-синий фон
        SDL_RenderClear(renderer);

        SDL_Color btn_color = {100, 100, 100, 255};
        SDL_Color hover_color = {150, 150, 150, 255};
        SDL_SetRenderDrawColor(renderer, import_hover ? hover_color.r : btn_color.r,
                               import_hover ? hover_color.g : btn_color.g,
                               import_hover ? hover_color.b : btn_color.b, 255);
        SDL_RenderFillRect(renderer, &import_btn);
        SDL_SetRenderDrawColor(renderer, export_hover ? hover_color.r : btn_color.r,
                               export_hover ? hover_color.g : btn_color.g,
                               export_hover ? hover_color.b : btn_color.b, 255);
        SDL_RenderFillRect(renderer, &export_btn);

        SDL_Surface *surf = TTF_RenderUTF8_Blended(font, "Import", (SDL_Color){255,255,255,255});
        if (surf) {
            SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_Rect d = { import_btn.x+5, import_btn.y+2, surf->w, surf->h };
            SDL_RenderCopy(renderer, tex, NULL, &d);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(surf);
        }
        surf = TTF_RenderUTF8_Blended(font, "Export", (SDL_Color){255,255,255,255});
        if (surf) {
            SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_Rect d = { export_btn.x+5, export_btn.y+2, surf->w, surf->h };
            SDL_RenderCopy(renderer, tex, NULL, &d);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(surf);
        }

        if (panelTexture) {
            SDL_Rect r = { PANEL_X, PANEL_Y, PANEL_WIDTH, PANEL_HEIGHT };
            SDL_RenderCopy(renderer, panelTexture, NULL, &r);

            // Проверка границ скролла (без насильного возврата вниз!)
            if (first_visible < 0) first_visible = 0;
            if (lines.count > MAX_VISIBLE_LINES && first_visible > lines.count - MAX_VISIBLE_LINES)
                first_visible = lines.count - MAX_VISIBLE_LINES;

            int start = first_visible;
            int visible_count = lines.count - start;
            if (visible_count > MAX_VISIBLE_LINES)
                visible_count = MAX_VISIBLE_LINES;

            int line_y = PANEL_Y + MARGIN_TOP;
            for (int i = start; i < start + visible_count; i++) {
                if (line_y + TTF_FontHeight(font) > PANEL_Y + PANEL_HEIGHT - MARGIN_TOP)
                    break;
                SDL_Surface *s = TTF_RenderUTF8_Blended(font, lines.data[i], (SDL_Color){255,255,255,255});
                if (s) {
                    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, s);
                    SDL_Rect d = { PANEL_X + MARGIN_LEFT, line_y, s->w, s->h };
                    SDL_RenderCopy(renderer, tex, NULL, &d);
                    SDL_DestroyTexture(tex);
                    SDL_FreeSurface(s);
                }
                line_y += TTF_FontHeight(font) + LINE_SPACING;
            }

            // Красная стрелка вверх
            if (start > 0) {
                int ax = PANEL_X + PANEL_WIDTH - MARGIN_RIGHT - 12;
                int ay = PANEL_Y + MARGIN_TOP;
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                SDL_RenderDrawLine(renderer, ax, ay, ax+8, ay+10);
                SDL_RenderDrawLine(renderer, ax, ay, ax-8, ay+10);
                SDL_RenderDrawLine(renderer, ax-8, ay+10, ax+8, ay+10);
            }

            // Красная стрелка вниз
            if (start + visible_count < lines.count) {
                int ax = PANEL_X + PANEL_WIDTH - MARGIN_RIGHT - 12;
                int ay = PANEL_Y + PANEL_HEIGHT - MARGIN_TOP;
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                SDL_RenderDrawLine(renderer, ax, ay, ax+8, ay-10);
                SDL_RenderDrawLine(renderer, ax, ay, ax-8, ay-10);
                SDL_RenderDrawLine(renderer, ax-8, ay-10, ax+8, ay-10);
            }

            // Курсор
            if (show_cursor && lines.count > 0) {
                int last_idx = (start + visible_count - 1);
                if (last_idx >= 0 && last_idx < lines.count) {
                    char *cur_line = lines.data[last_idx];
                    int w, h;
                    TTF_SizeUTF8(font, cur_line, &w, &h);
                    int cx = PANEL_X + MARGIN_LEFT + w;
                    int cy = PANEL_Y + MARGIN_TOP + (last_idx - start) * (TTF_FontHeight(font) + LINE_SPACING);
                    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                    SDL_RenderDrawLine(renderer, cx, cy, cx, cy + TTF_FontHeight(font));
                }
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_StopTextInput();
    if (panelTexture) SDL_DestroyTexture(panelTexture);
    if (font) TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    la_free(&lines);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}