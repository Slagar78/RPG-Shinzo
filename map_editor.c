#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <windows.h>
#include <commdlg.h>
#include "cJSON.h"

// ─── Геометрия окна ───────────────────────────
#define WINDOW_W 1280
#define WINDOW_H 720
#define TOOLBAR_H 34

#define LEFT_PANEL_W 300
#define RIGHT_PANEL_W 200
#define MAP_X LEFT_PANEL_W
#define MAP_Y TOOLBAR_H
#define MAP_W (WINDOW_W - LEFT_PANEL_W - RIGHT_PANEL_W)
#define MAP_H (WINDOW_H - TOOLBAR_H)

#define TILE_SIZE 48
#define PALETTE_TILE_SIZE 32
#define PALETTE_COLS 8
#define PALETTE_START_X 10
#define PALETTE_START_Y 140

#define FONT_SIZE 16
#define MODE_A 0
#define MODE_C 1
#define MIN_MAP_SIZE 2
#define MAX_MAP_SIZE 999

#define RIGHT_ROTATE   1
#define RIGHT_FLIP_H   2
#define RIGHT_FLIP_V   3

#define DIALOG_NONE          0
#define DIALOG_NEW_MAP       1
#define DIALOG_RENAME_MAP    2
#define DIALOG_RESIZE_MAP    3
#define DIALOG_CONFIRM_DEL   4

// ─── Структуры данных ─────────────────────────
typedef struct {
    char name[64];
    int width, height;
    int *tiles;          // x * height + y
    int *rot;
    int *mirror_x;
    int *mirror_y;
    char tileset_path[256];
} Map;

typedef struct {
    Map *maps;
    int map_count;
    int current_map;
} MapList;

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    TTF_Font     *font;

    SDL_Texture **tiles;
    int           tile_count;
    char          tileset_path[256];

    int          *tile_types;
    SDL_Texture  *type_icons[4];
    int           current_type;

    int palette_scroll;
    int selected_tile;
    int mode;
    bool tileset_loaded;

    Uint32 blink_time;
    bool   blink_visible;

    MapList map_list;

    float cam_x, cam_y;
    int   panning;
    Sint32 pan_start_x, pan_start_y;

    int right_click_mode;
    SDL_Texture *transform_icons[3];

    bool dialog_active;
    int  dialog_type;
    char input_text[64];
    char input_text2[64];
	
	int dialog_active_field;   // 0 = первое поле, 1 = второе поле
    Uint32 dialog_cursor_blink;

    // Мигание кнопки Save
    bool   save_blink_active;
    Uint32 save_blink_time;
} Editor;

// Прототип
Map* current_map(Editor *ed);

// Безопасное копирование строк
void safe_strcpy(char *dest, size_t dest_size, const char *src) {
    if (dest_size > 0) snprintf(dest, dest_size, "%s", src);
}

void editor_init(Editor *ed) {
    memset(ed, 0, sizeof(Editor));
    ed->mode = MODE_A;
    ed->selected_tile = 0;
    ed->current_type = 0;
    ed->tileset_loaded = false;
    ed->blink_time = SDL_GetTicks();
    ed->blink_visible = true;
    ed->dialog_active = false;
    ed->dialog_type = DIALOG_NONE;
    ed->right_click_mode = 0;
    ed->cam_x = ed->cam_y = 0;
    ed->panning = 0;
    ed->save_blink_active = false;
    ed->save_blink_time = 0;	
	ed->dialog_active_field = 0;
    ed->dialog_cursor_blink = 0;
	
    for (int i = 0; i < 4; i++) ed->type_icons[i] = NULL;
    for (int i = 0; i < 3; i++) ed->transform_icons[i] = NULL;
    ed->map_list.maps = NULL;
    ed->map_list.map_count = 0;
    ed->map_list.current_map = 0;
}

void free_tileset(Editor *ed) {
    if (ed->tiles) {
        for (int i = 0; i < ed->tile_count; i++) SDL_DestroyTexture(ed->tiles[i]);
        free(ed->tiles); ed->tiles = NULL;
    }
    if (ed->tile_types) { free(ed->tile_types); ed->tile_types = NULL; }
    ed->tile_count = 0;
    ed->tileset_loaded = false;
}

SDL_Texture* create_color_texture(SDL_Renderer *ren, Uint8 r, Uint8 g, Uint8 b, int w, int h) {
    SDL_Surface *surf = SDL_CreateRGBSurface(0, w, h, 32, 0,0,0,0);
    SDL_FillRect(surf, NULL, SDL_MapRGBA(surf->format, r, g, b, 255));
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_FreeSurface(surf);
    return tex;
}

void load_type_icons(Editor *ed) {
    for (int i = 0; i < 4; i++) {
        if (ed->type_icons[i]) SDL_DestroyTexture(ed->type_icons[i]);
        ed->type_icons[i] = NULL;
    }
    const char *filenames[4] = {
        "assets/icons/passable.png", "assets/icons/block.png",
        "assets/icons/slow.png", "assets/icons/under.png"
    };
    SDL_Color fallback[4] = {
        {0, 200, 0, 255}, {200, 0, 0, 255}, {200, 200, 0, 255}, {0, 0, 200, 255}
    };
    for (int i = 0; i < 4; i++) {
        SDL_Surface *surf = IMG_Load(filenames[i]);
        if (surf) {
            ed->type_icons[i] = SDL_CreateTextureFromSurface(ed->renderer, surf);
            SDL_FreeSurface(surf);
        } else {
            ed->type_icons[i] = create_color_texture(ed->renderer, fallback[i].r, fallback[i].g, fallback[i].b, 16, 16);
        }
    }
}

void load_transform_icons(Editor *ed) {
    const char *files[3] = {
        "assets/icons/rotate.png",
        "assets/icons/flip_h.png",
        "assets/icons/flip_v.png"
    };
    Uint8 fr[3] = {200, 100, 100};
    Uint8 fg[3] = {100, 200, 100};
    Uint8 fb[3] = {100, 100, 200};
    for (int i = 0; i < 3; i++) {
        if (ed->transform_icons[i]) SDL_DestroyTexture(ed->transform_icons[i]);
        ed->transform_icons[i] = NULL;
        SDL_Surface *surf = IMG_Load(files[i]);
        if (surf) {
            ed->transform_icons[i] = SDL_CreateTextureFromSurface(ed->renderer, surf);
            SDL_FreeSurface(surf);
        } else {
            ed->transform_icons[i] = create_color_texture(ed->renderer, fr[i], fg[i], fb[i], 32, 32);
        }
    }
}

void save_tile_types(const Editor *ed) {
    if (!ed->tileset_loaded || !ed->tile_types) return;
    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < ed->tile_count; i++) cJSON_AddItemToArray(root, cJSON_CreateNumber(ed->tile_types[i]));
    char *str = cJSON_Print(root);
    FILE *f = fopen("data/tile_types.json", "w");
    if (f) { fputs(str, f); fclose(f); }
    cJSON_Delete(root); free(str);
}

void load_tile_types(Editor *ed) {
    if (!ed->tileset_loaded) return;
    if (ed->tile_types) { free(ed->tile_types); ed->tile_types = NULL; }
    ed->tile_types = (int*)malloc(ed->tile_count * sizeof(int));
    memset(ed->tile_types, 0, ed->tile_count * sizeof(int));
    FILE *f = fopen("data/tile_types.json", "r");
    if (!f) return;
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    char *data = (char*)malloc(len+1);
    fread(data, 1, len, f); fclose(f); data[len] = '\0';
    cJSON *arr = cJSON_Parse(data); free(data);
    if (!arr) return;
    int size = cJSON_GetArraySize(arr);
    for (int i = 0; i < ed->tile_count && i < size; i++)
        ed->tile_types[i] = cJSON_GetArrayItem(arr, i)->valueint;
    cJSON_Delete(arr);
}

int load_tileset(Editor *ed, const char *path) {
    free_tileset(ed);
    SDL_Surface *surface = IMG_Load(path);
    if (!surface) return 0;
    int cols = surface->w / TILE_SIZE;
    int rows = surface->h / TILE_SIZE;
    ed->tile_count = cols * rows;
    ed->tiles = (SDL_Texture**)malloc(ed->tile_count * sizeof(SDL_Texture*));
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            SDL_Rect src = { c * TILE_SIZE, r * TILE_SIZE, TILE_SIZE, TILE_SIZE };
            SDL_Surface *tile_surf = SDL_CreateRGBSurface(0, TILE_SIZE, TILE_SIZE, 32, 0,0,0,0);
            SDL_BlitSurface(surface, &src, tile_surf, NULL);
            ed->tiles[r * cols + c] = SDL_CreateTextureFromSurface(ed->renderer, tile_surf);
            SDL_FreeSurface(tile_surf);
        }
    }
    SDL_FreeSurface(surface);
    safe_strcpy(ed->tileset_path, sizeof(ed->tileset_path), path);
    ed->tileset_loaded = true;
    ed->palette_scroll = 0;
    ed->selected_tile = 0;
    load_tile_types(ed);
    return 1;
}

bool open_file_dialog(char *out_path, size_t out_len) {
    OPENFILENAMEA ofn;
    char szFile[260] = {0};
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "PNG Files\0*.png\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn) == TRUE) {
        safe_strcpy(out_path, out_len, ofn.lpstrFile);
        return true;
    }
    return false;
}

// ─── Карты ───────────────────────────────────
bool map_load_from_json(Map *map, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return false;
    fseek(f, 0, SEEK_END); long len = ftell(f);
    if (len <= 0) { fclose(f); return false; }
    fseek(f, 0, SEEK_SET);
    char *data = (char*)malloc(len+1);
    if (!data) { fclose(f); return false; }
    fread(data, 1, len, f); data[len] = '\0'; fclose(f);

    cJSON *root = cJSON_Parse(data); free(data);
    if (!root) return false;

    cJSON *name_json = cJSON_GetObjectItem(root, "name");
    cJSON *width_json = cJSON_GetObjectItem(root, "width");
    cJSON *height_json = cJSON_GetObjectItem(root, "height");
    cJSON *tileset_json = cJSON_GetObjectItem(root, "tileset");
    cJSON *t_json = cJSON_GetObjectItem(root, "tiles");
    cJSON *r_json = cJSON_GetObjectItem(root, "rot");
    cJSON *mx_json = cJSON_GetObjectItem(root, "mirror_x");
    cJSON *my_json = cJSON_GetObjectItem(root, "mirror_y");

    if (!name_json || !width_json || !height_json || !tileset_json || !t_json || !r_json || !mx_json || !my_json) {
        cJSON_Delete(root); return false;
    }

    int w = width_json->valueint, h = height_json->valueint;
    if (w < MIN_MAP_SIZE || w > MAX_MAP_SIZE || h < MIN_MAP_SIZE || h > MAX_MAP_SIZE) {
        cJSON_Delete(root); return false;
    }
    int sz = w * h;
    if (sz <= 0) { cJSON_Delete(root); return false; }

    map->width = w; map->height = h;
    safe_strcpy(map->name, sizeof(map->name), name_json->valuestring);
    safe_strcpy(map->tileset_path, sizeof(map->tileset_path), tileset_json->valuestring);

    map->tiles     = (int*)malloc(sz * sizeof(int));
    map->rot       = (int*)malloc(sz * sizeof(int));
    map->mirror_x  = (int*)malloc(sz * sizeof(int));
    map->mirror_y  = (int*)malloc(sz * sizeof(int));
    if (!map->tiles || !map->rot || !map->mirror_x || !map->mirror_y) {
        free(map->tiles); free(map->rot); free(map->mirror_x); free(map->mirror_y);
        cJSON_Delete(root); return false;
    }

    for (int x = 0; x < w; x++) {
        cJSON *col_t  = cJSON_GetArrayItem(t_json, x);
        cJSON *col_r  = cJSON_GetArrayItem(r_json, x);
        cJSON *col_mx = cJSON_GetArrayItem(mx_json, x);
        cJSON *col_my = cJSON_GetArrayItem(my_json, x);

        for (int y = 0; y < h; y++) {
            int idx = x * h + y;
            map->tiles[idx]     = (col_t  && cJSON_IsArray(col_t)  && cJSON_GetArrayItem(col_t,  y)) ? cJSON_GetArrayItem(col_t,  y)->valueint : 0;
            map->rot[idx]       = (col_r  && cJSON_IsArray(col_r)  && cJSON_GetArrayItem(col_r,  y)) ? cJSON_GetArrayItem(col_r,  y)->valueint : 0;
            map->mirror_x[idx]  = (col_mx && cJSON_IsArray(col_mx) && cJSON_GetArrayItem(col_mx, y)) ? cJSON_IsTrue(cJSON_GetArrayItem(col_mx, y)) : false;
            map->mirror_y[idx]  = (col_my && cJSON_IsArray(col_my) && cJSON_GetArrayItem(col_my, y)) ? cJSON_IsTrue(cJSON_GetArrayItem(col_my, y)) : false;
        }
    }

    cJSON_Delete(root);
    return true;
}

void map_init(Map *map, const char *name, int w, int h, const char *tileset) {
    safe_strcpy(map->name, sizeof(map->name), name);
    map->width = w; map->height = h;
    safe_strcpy(map->tileset_path, sizeof(map->tileset_path), tileset);
    int sz = w * h;
    map->tiles = (int*)calloc(sz, sizeof(int));
    map->rot = (int*)calloc(sz, sizeof(int));
    map->mirror_x = (int*)calloc(sz, sizeof(int));
    map->mirror_y = (int*)calloc(sz, sizeof(int));
}

void map_free(Map *map) {
    free(map->tiles); free(map->rot); free(map->mirror_x); free(map->mirror_y);
}

void map_save_to_json(const Map *map, const char *filename) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", map->name);
    cJSON_AddNumberToObject(root, "width", map->width);
    cJSON_AddNumberToObject(root, "height", map->height);
    cJSON_AddStringToObject(root, "tileset", map->tileset_path);

    cJSON *t  = cJSON_AddArrayToObject(root, "tiles");
    cJSON *r  = cJSON_AddArrayToObject(root, "rot");
    cJSON *mx = cJSON_AddArrayToObject(root, "mirror_x");
    cJSON *my = cJSON_AddArrayToObject(root, "mirror_y");

    for (int x = 0; x < map->width; x++) {
        cJSON *col_t  = cJSON_CreateArray();
        cJSON *col_r  = cJSON_CreateArray();
        cJSON *col_mx = cJSON_CreateArray();
        cJSON *col_my = cJSON_CreateArray();

        for (int y = 0; y < map->height; y++) {
            int idx = x * map->height + y;
            cJSON_AddItemToArray(col_t,  cJSON_CreateNumber(map->tiles[idx]));
            cJSON_AddItemToArray(col_r,  cJSON_CreateNumber(map->rot[idx]));
            cJSON_AddItemToArray(col_mx, cJSON_CreateBool(map->mirror_x[idx]));
            cJSON_AddItemToArray(col_my, cJSON_CreateBool(map->mirror_y[idx]));
        }

        cJSON_AddItemToArray(t,  col_t);
        cJSON_AddItemToArray(r,  col_r);
        cJSON_AddItemToArray(mx, col_mx);
        cJSON_AddItemToArray(my, col_my);
    }

    char *str = cJSON_Print(root);
    FILE *f = fopen(filename, "w");
    if (f) { fputs(str, f); fclose(f); }
    cJSON_Delete(root);
    free(str);
}

Map* current_map(Editor *ed) {
    if (ed->map_list.map_count == 0) return NULL;
    return &ed->map_list.maps[ed->map_list.current_map];
}

void load_map_list(Editor *ed) {
    for (int i = 0; i < ed->map_list.map_count; i++) map_free(&ed->map_list.maps[i]);
    free(ed->map_list.maps);
    ed->map_list.maps = NULL;
    ed->map_list.map_count = 0;
    ed->map_list.current_map = 0;

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA("data/maps/*.json", &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        char path[256];
        snprintf(path, sizeof(path), "data/maps/%s", findData.cFileName);
        Map temp;
        if (map_load_from_json(&temp, path)) {
            ed->map_list.map_count++;
            ed->map_list.maps = (Map*)realloc(ed->map_list.maps, ed->map_list.map_count * sizeof(Map));
            if (!ed->map_list.maps) exit(1);
            ed->map_list.maps[ed->map_list.map_count - 1] = temp;
        }
    } while (FindNextFileA(hFind, &findData));
    FindClose(hFind);
}

void create_map(Editor *ed, const char *name, int w, int h) {
    if (w < MIN_MAP_SIZE) w = MIN_MAP_SIZE; if (h < MIN_MAP_SIZE) h = MIN_MAP_SIZE;
    if (w > MAX_MAP_SIZE) w = MAX_MAP_SIZE; if (h > MAX_MAP_SIZE) h = MAX_MAP_SIZE;

    char fullname[64];
    safe_strcpy(fullname, sizeof(fullname), name);
    int counter = 0;
    while (1) {
        bool exists = false;
        for (int i = 0; i < ed->map_list.map_count; i++)
            if (strcmp(ed->map_list.maps[i].name, fullname) == 0) { exists = true; break; }
        if (!exists) break;
        counter++;
        snprintf(fullname, sizeof(fullname), "%s%d", name, counter);
    }

    Map new_map;
    map_init(&new_map, fullname, w, h, ed->tileset_path);
    char path[128]; snprintf(path, sizeof(path), "data/maps/%s.json", fullname);
    map_save_to_json(&new_map, path);

    ed->map_list.map_count++;
    ed->map_list.maps = (Map*)realloc(ed->map_list.maps, ed->map_list.map_count * sizeof(Map));
    if (!ed->map_list.maps) exit(1);
    ed->map_list.maps[ed->map_list.map_count - 1] = new_map;
    ed->map_list.current_map = ed->map_list.map_count - 1;

    load_tileset(ed, new_map.tileset_path);
}

void delete_current_map(Editor *ed) {
    if (ed->map_list.map_count <= 1) return;
    Map *map = &ed->map_list.maps[ed->map_list.current_map];
    char path[128]; snprintf(path, sizeof(path), "data/maps/%s.json", map->name);
    remove(path);
    map_free(map);
    for (int i = ed->map_list.current_map; i < ed->map_list.map_count - 1; i++)
        ed->map_list.maps[i] = ed->map_list.maps[i+1];
    ed->map_list.map_count--;
    if (ed->map_list.current_map >= ed->map_list.map_count) ed->map_list.current_map = ed->map_list.map_count - 1;
    Map *cur = current_map(ed);
    if (cur) load_tileset(ed, cur->tileset_path);
}

void rename_current_map(Editor *ed, const char *new_name) {
    Map *map = current_map(ed);
    if (!map || strlen(new_name)==0) return;
    char old_path[128], new_path[128];
    snprintf(old_path, sizeof(old_path), "data/maps/%s.json", map->name);
    snprintf(new_path, sizeof(new_path), "data/maps/%s.json", new_name);
    if (rename(old_path, new_path) == 0) {
        safe_strcpy(map->name, sizeof(map->name), new_name);
        map_save_to_json(map, new_path);
        int old_idx = ed->map_list.current_map;
        load_map_list(ed);
        for (int i = 0; i < ed->map_list.map_count; i++) {
            if (strcmp(ed->map_list.maps[i].name, new_name) == 0) {
                ed->map_list.current_map = i;
                break;
            }
        }
    }
}

void resize_current_map(Editor *ed, int new_w, int new_h) {
    Map *map = current_map(ed);
    if (!map) return;
    new_w = (new_w < MIN_MAP_SIZE) ? MIN_MAP_SIZE : (new_w > MAX_MAP_SIZE) ? MAX_MAP_SIZE : new_w;
    new_h = (new_h < MIN_MAP_SIZE) ? MIN_MAP_SIZE : (new_h > MAX_MAP_SIZE) ? MAX_MAP_SIZE : new_h;

    Map bigger;
    map_init(&bigger, map->name, new_w, new_h, map->tileset_path);
    for (int x = 0; x < map->width && x < new_w; x++)
        for (int y = 0; y < map->height && y < new_h; y++) {
            int old_idx = x * map->height + y;
            int new_idx = x * new_h + y;
            bigger.tiles[new_idx] = map->tiles[old_idx];
            bigger.rot[new_idx] = map->rot[old_idx];
            bigger.mirror_x[new_idx] = map->mirror_x[old_idx];
            bigger.mirror_y[new_idx] = map->mirror_y[old_idx];
        }
    map_free(map);
    *map = bigger;
    char path[128]; snprintf(path, sizeof(path), "data/maps/%s.json", map->name);
    map_save_to_json(map, path);
}

// ─── Отрисовка текста ────────────────
void draw_text_centered(SDL_Renderer *ren, TTF_Font *font, const char *text, int cx, int cy, SDL_Color color) {
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_Rect dst = { cx - surf->w/2, cy - surf->h/2, surf->w, surf->h };
    SDL_RenderCopy(ren, tex, NULL, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

// ─── Левая панель ──────────────────────
void render_left_panel(Editor *ed) {
    SDL_Rect bg = { 0, 0, LEFT_PANEL_W, WINDOW_H };
    SDL_SetRenderDrawColor(ed->renderer, 50, 50, 50, 255);
    SDL_RenderFillRect(ed->renderer, &bg);
    draw_text_centered(ed->renderer, ed->font, "TILESET", LEFT_PANEL_W/2, 15, (SDL_Color){255,255,255,255});

    SDL_Rect load_btn = { 10, 35, LEFT_PANEL_W - 20, 26 };
    SDL_SetRenderDrawColor(ed->renderer, 80, 80, 80, 255);
    SDL_RenderFillRect(ed->renderer, &load_btn);
    draw_text_centered(ed->renderer, ed->font, "Load Tileset", load_btn.x + load_btn.w/2, load_btn.y + load_btn.h/2, (SDL_Color){255,255,255,255});

    SDL_Rect btnA = { 10, 70, (LEFT_PANEL_W - 30)/2, 24 };
    SDL_Rect btnC = { btnA.x + btnA.w + 10, 70, btnA.w, 24 };
    if (ed->mode == MODE_A) {
        SDL_SetRenderDrawColor(ed->renderer, 100, 160, 100, 255);
        SDL_RenderFillRect(ed->renderer, &btnA);
        SDL_SetRenderDrawColor(ed->renderer, 140, 140, 140, 255);
        SDL_RenderFillRect(ed->renderer, &btnC);
    } else {
        SDL_SetRenderDrawColor(ed->renderer, 140, 140, 140, 255);
        SDL_RenderFillRect(ed->renderer, &btnA);
        SDL_SetRenderDrawColor(ed->renderer, 100, 160, 100, 255);
        SDL_RenderFillRect(ed->renderer, &btnC);
    }
    draw_text_centered(ed->renderer, ed->font, "A", btnA.x + btnA.w/2, btnA.y + btnA.h/2, (SDL_Color){255,255,255,255});
    draw_text_centered(ed->renderer, ed->font, "C", btnC.x + btnC.w/2, btnC.y + btnC.h/2, (SDL_Color){255,255,255,255});

    if (ed->mode == MODE_C) {
        int icon_y = 100, icon_size = 24, spacing = 6;
        int total_w = 4 * icon_size + 3 * spacing;
        int start_x = (LEFT_PANEL_W - total_w) / 2;
        for (int t = 0; t < 4; t++) {
            SDL_Rect r = { start_x + t * (icon_size + spacing), icon_y, icon_size, icon_size };
            SDL_RenderCopy(ed->renderer, ed->type_icons[t], NULL, &r);
            if (t == ed->current_type) {
                SDL_SetRenderDrawColor(ed->renderer, 255, 255, 0, 255);
                SDL_RenderDrawRect(ed->renderer, &r);
            }
        }
    }

    SDL_SetRenderDrawColor(ed->renderer, 100, 100, 100, 255);
    SDL_RenderDrawLine(ed->renderer, 10, 130, LEFT_PANEL_W-10, 130);

    if (!ed->tileset_loaded) {
        draw_text_centered(ed->renderer, ed->font, "No tileset", LEFT_PANEL_W/2, 250, (SDL_Color){255,100,100,255});
        return;
    }

    int visible_rows = (WINDOW_H - PALETTE_START_Y) / (PALETTE_TILE_SIZE + 2);
    int total_rows = (ed->tile_count + PALETTE_COLS - 1) / PALETTE_COLS;

    for (int row = ed->palette_scroll; row < ed->palette_scroll + visible_rows && row < total_rows; row++) {
        for (int col = 0; col < PALETTE_COLS; col++) {
            int idx = row * PALETTE_COLS + col;
            if (idx >= ed->tile_count) break;

            SDL_Rect dst = {
                PALETTE_START_X + col * (PALETTE_TILE_SIZE + 2),
                PALETTE_START_Y + (row - ed->palette_scroll) * (PALETTE_TILE_SIZE + 2),
                PALETTE_TILE_SIZE, PALETTE_TILE_SIZE
            };
            SDL_RenderCopy(ed->renderer, ed->tiles[idx], NULL, &dst);

            SDL_SetRenderDrawColor(ed->renderer, 70, 70, 70, 255);
            SDL_Rect frame = { dst.x - 1, dst.y - 1, PALETTE_TILE_SIZE + 2, PALETTE_TILE_SIZE + 2 };
            SDL_RenderDrawRect(ed->renderer, &frame);

            if (ed->mode == MODE_C && ed->tile_types) {
                int type = ed->tile_types[idx];
                if (type >= 0 && type < 4) {
                    SDL_Rect icon_dst = { dst.x + PALETTE_TILE_SIZE - 12, dst.y + PALETTE_TILE_SIZE - 12, 12, 12 };
                    SDL_RenderCopy(ed->renderer, ed->type_icons[type], NULL, &icon_dst);
                }
            }

            if (ed->mode == MODE_A && idx == ed->selected_tile && ed->blink_visible) {
                SDL_SetRenderDrawColor(ed->renderer, 255, 255, 0, 255);
                SDL_RenderDrawRect(ed->renderer, &frame);
            }
        }
    }
}

// ─── Тулебар ───────────────────────────
void render_toolbar(Editor *ed) {
    SDL_Rect bar = { MAP_X, 0, MAP_W, TOOLBAR_H };
    SDL_SetRenderDrawColor(ed->renderer, 70, 70, 70, 255);
    SDL_RenderFillRect(ed->renderer, &bar);

    struct { int x; int mode; int icon_index; } btns[] = {
        { MAP_X + 5, RIGHT_ROTATE, 0 },
        { MAP_X + 45, RIGHT_FLIP_H, 1 },
        { MAP_X + 85, RIGHT_FLIP_V, 2 }
    };
    int btn_w = 38, btn_h = TOOLBAR_H - 6;

    for (int i = 0; i < 3; i++) {
        SDL_Rect btn_rect = { btns[i].x, 3, btn_w, btn_h };
        bool active = (ed->right_click_mode == btns[i].mode);
        SDL_SetRenderDrawColor(ed->renderer, active ? 140 : 100, active ? 140 : 100, active ? 140 : 100, 255);
        SDL_RenderFillRect(ed->renderer, &btn_rect);

        if (ed->transform_icons[btns[i].icon_index]) {
            SDL_Rect icon_dst = { btn_rect.x + 3, btn_rect.y + 3, btn_rect.w - 6, btn_rect.h - 6 };
            SDL_RenderCopy(ed->renderer, ed->transform_icons[btns[i].icon_index], NULL, &icon_dst);
        } else {
            const char *labels[] = {"R", "FH", "FV"};
            draw_text_centered(ed->renderer, ed->font, labels[i], btn_rect.x + btn_rect.w/2, btn_rect.y + btn_rect.h/2, (SDL_Color){255,255,255,255});
        }
    }
}

// ─── Карта ─────────────────────────────
void render_map(Editor *ed) {
    Map *map = current_map(ed);
    if (!map || !ed->tileset_loaded) return;

    SDL_Rect map_area = { MAP_X, MAP_Y, MAP_W, MAP_H };
    SDL_RenderSetClipRect(ed->renderer, &map_area);

    for (int x = 0; x < map->width; x++) {
        for (int y = 0; y < map->height; y++) {
            int idx = x * map->height + y;
            int tile_id = map->tiles[idx];
            if (tile_id < 0 || tile_id >= ed->tile_count) continue;

            SDL_Texture *tex = ed->tiles[tile_id];
            double angle = map->rot[idx] * 90.0;
            SDL_RendererFlip flip = SDL_FLIP_NONE;
            if (map->mirror_x[idx]) flip |= SDL_FLIP_HORIZONTAL;
            if (map->mirror_y[idx]) flip |= SDL_FLIP_VERTICAL;

            SDL_Rect dst = {
                (int)(MAP_X + x * TILE_SIZE - ed->cam_x),
                (int)(MAP_Y + y * TILE_SIZE - ed->cam_y),
                TILE_SIZE, TILE_SIZE
            };
            SDL_Point center = { TILE_SIZE/2, TILE_SIZE/2 };
            SDL_RenderCopyEx(ed->renderer, tex, NULL, &dst, angle, &center, flip);
        }
    }

    int mx, my;
    SDL_GetMouseState(&mx, &my);
    if (mx >= MAP_X && mx < MAP_X + MAP_W && my >= MAP_Y && my < MAP_Y + MAP_H) {
        int world_x = (int)(mx - MAP_X + ed->cam_x);
        int world_y = (int)(my - MAP_Y + ed->cam_y);
        int tx = world_x / TILE_SIZE;
        int ty = world_y / TILE_SIZE;
        if (tx >= 0 && tx < map->width && ty >= 0 && ty < map->height) {
            SDL_Rect hl = {
                (int)(MAP_X + tx * TILE_SIZE - ed->cam_x),
                (int)(MAP_Y + ty * TILE_SIZE - ed->cam_y),
                TILE_SIZE, TILE_SIZE
            };
            SDL_SetRenderDrawColor(ed->renderer, 255, 255, 0, 255);
            SDL_RenderDrawRect(ed->renderer, &hl);
        }
    }

    SDL_RenderSetClipRect(ed->renderer, NULL);
}

// ─── Правая панель ─────────────────────
void render_right_panel(Editor *ed) {
    int pan_x = WINDOW_W - RIGHT_PANEL_W;
    SDL_Rect bg = { pan_x, 0, RIGHT_PANEL_W, WINDOW_H };
    SDL_SetRenderDrawColor(ed->renderer, 60, 60, 60, 255);
    SDL_RenderFillRect(ed->renderer, &bg);

    draw_text_centered(ed->renderer, ed->font, "MAPS", pan_x + RIGHT_PANEL_W/2, 15, (SDL_Color){255,255,255,255});

    int list_y = 35;
    for (int i = 0; i < ed->map_list.map_count; i++) {
        SDL_Color col = (i == ed->map_list.current_map) ? (SDL_Color){0,255,0,255} : (SDL_Color){255,255,255,255};
        draw_text_centered(ed->renderer, ed->font, ed->map_list.maps[i].name, pan_x + RIGHT_PANEL_W/2, list_y + i * 20, col);
    }

    int btn_x = pan_x + 5;
    int btn_w = RIGHT_PANEL_W - 10;
    int y = WINDOW_H - 150;
    const char *names[] = {"New Map", "Save Map", "Delete Map", "Rename Map", "Resize Map"};
    int mx, my;
    SDL_GetMouseState(&mx, &my);

    for (int i = 0; i < 5; i++) {
        SDL_Rect btn = { btn_x, y + i*30, btn_w, 24 };
        bool hover = (mx >= btn.x && mx < btn.x+btn.w && my >= btn.y && my < btn.y+btn.h);
        SDL_Color bg_col = hover ? (SDL_Color){110,110,110,255} : (SDL_Color){90,90,90,255};

        if (i == 1 && ed->save_blink_active) {
            bg_col = (SDL_Color){220, 80, 80, 255};
        }

        SDL_SetRenderDrawColor(ed->renderer, bg_col.r, bg_col.g, bg_col.b, 255);
        SDL_RenderFillRect(ed->renderer, &btn);
        draw_text_centered(ed->renderer, ed->font, names[i], btn.x + btn.w/2, btn.y + btn.h/2, (SDL_Color){255,255,255,255});
    }
}

// ─── Модальный диалог ────────────────────
void open_dialog(Editor *ed, int type) {
    ed->dialog_active = true;
    ed->dialog_type = type;
    ed->dialog_active_field = 0;          // начинаем с первого поля
    ed->dialog_cursor_blink = SDL_GetTicks();
    SDL_StartTextInput();
}

void close_dialog(Editor *ed) {
    ed->dialog_active = false;
    ed->dialog_type = DIALOG_NONE;
    ed->dialog_active_field = 0;
    memset(ed->input_text, 0, sizeof(ed->input_text));
    memset(ed->input_text2, 0, sizeof(ed->input_text2));
    SDL_StopTextInput();
}

void draw_dialog(Editor *ed) {
    if (!ed->dialog_active) return;

    // Затемнённый фон
    SDL_SetRenderDrawBlendMode(ed->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ed->renderer, 0, 0, 0, 180);
    SDL_Rect full = { 0, 0, WINDOW_W, WINDOW_H };
    SDL_RenderFillRect(ed->renderer, &full);
    SDL_SetRenderDrawBlendMode(ed->renderer, SDL_BLENDMODE_NONE);

    SDL_Rect dlg = { WINDOW_W/2 - 160, WINDOW_H/2 - 100, 320, 200 };
    SDL_SetRenderDrawColor(ed->renderer, 230, 230, 230, 255);
    SDL_RenderFillRect(ed->renderer, &dlg);
    SDL_SetRenderDrawColor(ed->renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(ed->renderer, &dlg);

    SDL_Color black = {0,0,0,255};
    SDL_Color white = {255,255,255,255};
    int y_text = dlg.y + 20;

    switch (ed->dialog_type) {
        case DIALOG_CONFIRM_DEL:
            draw_text_centered(ed->renderer, ed->font, "Delete map?", dlg.x + 160, y_text, black);
            draw_text_centered(ed->renderer, ed->font, ed->input_text, dlg.x + 160, y_text + 30, black);
            break;
        case DIALOG_NEW_MAP:
            draw_text_centered(ed->renderer, ed->font, "Name:", dlg.x + 50, y_text + 20, black);
            draw_text_centered(ed->renderer, ed->font, ed->input_text, dlg.x + 150, y_text + 20, black);
            draw_text_centered(ed->renderer, ed->font, "Size (WxH):", dlg.x + 50, y_text + 50, black);
            draw_text_centered(ed->renderer, ed->font, ed->input_text2, dlg.x + 150, y_text + 50, black);
            break;
        case DIALOG_RENAME_MAP:
            draw_text_centered(ed->renderer, ed->font, "New name:", dlg.x + 50, y_text + 30, black);
            draw_text_centered(ed->renderer, ed->font, ed->input_text, dlg.x + 160, y_text + 30, black);
            break;
        case DIALOG_RESIZE_MAP:
            draw_text_centered(ed->renderer, ed->font, "Width:", dlg.x + 50, y_text + 20, black);
            draw_text_centered(ed->renderer, ed->font, ed->input_text, dlg.x + 150, y_text + 20, black);
            draw_text_centered(ed->renderer, ed->font, "Height:", dlg.x + 50, y_text + 50, black);
            draw_text_centered(ed->renderer, ed->font, ed->input_text2, dlg.x + 150, y_text + 50, black);
            break;
    }

    // Подсветка активного поля
    if (ed->dialog_type == DIALOG_NEW_MAP || ed->dialog_type == DIALOG_RESIZE_MAP) {
        SDL_Rect field1 = { dlg.x + 120, dlg.y + 35, 150, 20 };
        SDL_Rect field2 = { dlg.x + 120, dlg.y + 65, 150, 20 };
        if (ed->dialog_active_field == 0) {
            SDL_SetRenderDrawColor(ed->renderer, 0, 120, 215, 255);
            SDL_RenderDrawRect(ed->renderer, &field1);
        } else {
            SDL_SetRenderDrawColor(ed->renderer, 0, 120, 215, 255);
            SDL_RenderDrawRect(ed->renderer, &field2);
        }
        // Мигающий курсор
        bool show_cursor = (SDL_GetTicks() - ed->dialog_cursor_blink) < 400;
        if (show_cursor) {
            SDL_Rect *active_rect = (ed->dialog_active_field == 0) ? &field1 : &field2;
            int text_w = 0;
            char *str = (ed->dialog_active_field == 0) ? ed->input_text : ed->input_text2;
            SDL_Surface *surf = TTF_RenderUTF8_Blended(ed->font, str, black);
            if (surf) { text_w = surf->w; SDL_FreeSurface(surf); }
            SDL_SetRenderDrawColor(ed->renderer, 0, 0, 0, 255);
            SDL_RenderDrawLine(ed->renderer,
                active_rect->x + text_w + 2, active_rect->y + 2,
                active_rect->x + text_w + 2, active_rect->y + active_rect->h - 4);
        }
    }

    SDL_Rect ok = { dlg.x + 30, dlg.y + 160, 110, 28 };
    SDL_Rect cancel = { dlg.x + 180, dlg.y + 160, 110, 28 };
    SDL_SetRenderDrawColor(ed->renderer, 140, 140, 140, 255);
    SDL_RenderFillRect(ed->renderer, &ok);
    SDL_RenderFillRect(ed->renderer, &cancel);

    const char *ok_text = (ed->dialog_type == DIALOG_CONFIRM_DEL) ? "Yes" : "OK";
    draw_text_centered(ed->renderer, ed->font, ok_text, ok.x + ok.w/2, ok.y + ok.h/2, white);
    draw_text_centered(ed->renderer, ed->font, "Cancel", cancel.x + cancel.w/2, cancel.y + cancel.h/2, white);
}

void handle_dialog_click(Editor *ed, int mx, int my) {
    SDL_Rect dlg = { WINDOW_W/2 - 160, WINDOW_H/2 - 100, 320, 200 };
    SDL_Rect ok = { dlg.x + 30, dlg.y + 160, 110, 28 };
    SDL_Rect cancel = { dlg.x + 180, dlg.y + 160, 110, 28 };

    if (mx >= ok.x && mx < ok.x+ok.w && my >= ok.y && my < ok.y+ok.h) {
        switch (ed->dialog_type) {
            case DIALOG_CONFIRM_DEL:
                delete_current_map(ed);
                break;
            case DIALOG_NEW_MAP: {
                int w = 20, h = 15;
                sscanf(ed->input_text2, "%dx%d", &w, &h);
                create_map(ed, ed->input_text, w, h);
                break;
            }
            case DIALOG_RENAME_MAP:
                rename_current_map(ed, ed->input_text);
                break;
            case DIALOG_RESIZE_MAP: {
                int w = atoi(ed->input_text);
                int h = atoi(ed->input_text2);
                resize_current_map(ed, w, h);
                break;
            }
        }
        close_dialog(ed);
    } else if (mx >= cancel.x && mx < cancel.x+cancel.w && my >= cancel.y && my < cancel.y+cancel.h) {
        close_dialog(ed);
    }
}

// ─── Обработка ввода ──────────────────────
void handle_input(Editor *ed, bool *running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { *running = false; return; }

        // Модальный диалог – только его события
        if (ed->dialog_active) {
            // Мигание курсора
            if (SDL_GetTicks() - ed->dialog_cursor_blink > 530) {
                ed->dialog_cursor_blink = SDL_GetTicks();
            }

            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_RETURN) {
                    handle_dialog_click(ed, WINDOW_W/2, WINDOW_H/2); // Enter = OK
                } else if (e.key.keysym.sym == SDLK_ESCAPE) {
                    close_dialog(ed);
                } else if (e.key.keysym.sym == SDLK_TAB) {
                    // Переключение поля
                    if (ed->dialog_type == DIALOG_NEW_MAP || ed->dialog_type == DIALOG_RESIZE_MAP)
                        ed->dialog_active_field = !ed->dialog_active_field;
                } else if (e.key.keysym.sym == SDLK_BACKSPACE) {
                    char *str = (ed->dialog_active_field == 0) ? ed->input_text : ed->input_text2;
                    if (str && strlen(str) > 0)
                        str[strlen(str)-1] = '\0';
                }
            } else if (e.type == SDL_TEXTINPUT) {
                char *dest = (ed->dialog_active_field == 0) ? ed->input_text : ed->input_text2;
                int max_len = 63;
                if ((ed->dialog_type == DIALOG_NEW_MAP || ed->dialog_type == DIALOG_RESIZE_MAP) && ed->dialog_active_field == 1)
                    max_len = 15;   // поле размера/высоты короче
                if (dest && strlen(dest) < max_len) {
                    strcat(dest, e.text.text);
                }
            } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                // Клик внутри диалога – проверяем, не попали ли в поле ввода
                SDL_Rect dlg = { WINDOW_W/2 - 160, WINDOW_H/2 - 100, 320, 200 };
                if (ed->dialog_type == DIALOG_NEW_MAP || ed->dialog_type == DIALOG_RESIZE_MAP) {
                    // Области текстовых полей (примерно)
                    SDL_Rect field1 = { dlg.x + 120, dlg.y + 35, 150, 20 };
                    SDL_Rect field2 = { dlg.x + 120, dlg.y + 65, 150, 20 };
                    if (e.button.x >= field1.x && e.button.x < field1.x+field1.w &&
                        e.button.y >= field1.y && e.button.y < field1.y+field1.h) {
                        ed->dialog_active_field = 0;
                    } else if (e.button.x >= field2.x && e.button.x < field2.x+field2.w &&
                               e.button.y >= field2.y && e.button.y < field2.y+field2.h) {
                        ed->dialog_active_field = 1;
                    } else {
                        handle_dialog_click(ed, e.button.x, e.button.y);
                    }
                } else {
                    handle_dialog_click(ed, e.button.x, e.button.y);
                }
            }
            continue;
        }

        // Скролл палитры
        if (e.type == SDL_MOUSEWHEEL) {
            int mx, my;
            SDL_GetMouseState(&mx, &my);
            if (mx < LEFT_PANEL_W && my >= PALETTE_START_Y && ed->tileset_loaded) {
                ed->palette_scroll -= e.wheel.y;
                int max_scroll = ((ed->tile_count + PALETTE_COLS - 1) / PALETTE_COLS) -
                                 ((WINDOW_H - PALETTE_START_Y) / (PALETTE_TILE_SIZE + 2));
                if (ed->palette_scroll < 0) ed->palette_scroll = 0;
                if (max_scroll > 0 && ed->palette_scroll > max_scroll) ed->palette_scroll = max_scroll;
            }
        }

        // Панорамирование
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT) {
            if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LCTRL]) {
                ed->panning = 1;
                ed->pan_start_x = e.button.x;
                ed->pan_start_y = e.button.y;
            }
        }
        if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_RIGHT) ed->panning = 0;

        // Transform
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT &&
            !(SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LCTRL]) && ed->right_click_mode != 0) {
            int mx = e.button.x, my = e.button.y;
            if (mx >= MAP_X && mx < MAP_X + MAP_W && my >= MAP_Y && my < MAP_Y + MAP_H) {
                int world_x = (int)(mx - MAP_X + ed->cam_x);
                int world_y = (int)(my - MAP_Y + ed->cam_y);
                int tx = world_x / TILE_SIZE, ty = world_y / TILE_SIZE;
                Map *map = current_map(ed);
                if (map && tx >= 0 && tx < map->width && ty >= 0 && ty < map->height) {
                    int idx = tx * map->height + ty;
                    switch (ed->right_click_mode) {
                        case RIGHT_ROTATE: map->rot[idx] = (map->rot[idx] + 1) % 4; break;
                        case RIGHT_FLIP_H: map->mirror_x[idx] = !map->mirror_x[idx]; break;
                        case RIGHT_FLIP_V: map->mirror_y[idx] = !map->mirror_y[idx]; break;
                    }
                }
            }
        }

        // Левая кнопка
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx = e.button.x, my = e.button.y;

            if (mx < LEFT_PANEL_W) {
                if (my >= 35 && my < 61) { char path[256]; if (open_file_dialog(path, sizeof(path))) load_tileset(ed, path); }
                else if (my >= 70 && my < 94) { int half = (LEFT_PANEL_W - 30)/2; if (mx >= 10 && mx < 10+half) ed->mode = MODE_A; else if (mx >= 10+half+10 && mx < 10+half+10+half) ed->mode = MODE_C; }
                else if (ed->mode == MODE_C && my >= 100 && my < 124) { /* выбор типа */ int icon_y=100,sz=24,sp=6; int tw=4*sz+3*sp; int sx=(LEFT_PANEL_W-tw)/2; for(int t=0;t<4;t++){ SDL_Rect r={sx+t*(sz+sp),icon_y,sz,sz}; if(mx>=r.x&&mx<r.x+r.w&&my>=r.y&&my<r.y+r.h){ed->current_type=t;break;} } }
                else if (ed->tileset_loaded && my >= PALETTE_START_Y && ed->mode == MODE_A) { /* выбор тайла */ int rx=mx-PALETTE_START_X,ry=my-PALETTE_START_Y; int step=PALETTE_TILE_SIZE+2; if(rx>=0&&ry>=0){int col=rx/step,row=ry/step+ed->palette_scroll; if(col<PALETTE_COLS&&rx%step<PALETTE_TILE_SIZE){int idx=row*PALETTE_COLS+col; if(idx>=0&&idx<ed->tile_count)ed->selected_tile=idx;}}}
            }
            else if (my < TOOLBAR_H && mx >= MAP_X && mx < MAP_X+MAP_W) {
                if (mx >= MAP_X+5 && mx < MAP_X+43) ed->right_click_mode = RIGHT_ROTATE;
                else if (mx >= MAP_X+45 && mx < MAP_X+83) ed->right_click_mode = RIGHT_FLIP_H;
                else if (mx >= MAP_X+85 && mx < MAP_X+123) ed->right_click_mode = RIGHT_FLIP_V;
                else ed->right_click_mode = 0;
            }
            else if (mx >= MAP_X && mx < MAP_X+MAP_W && my >= MAP_Y && my < MAP_Y+MAP_H) {
                int world_x = (int)(mx - MAP_X + ed->cam_x);
                int world_y = (int)(my - MAP_Y + ed->cam_y);
                int tx = world_x / TILE_SIZE, ty = world_y / TILE_SIZE;
                Map *map = current_map(ed);
                if (map && tx >= 0 && tx < map->width && ty >= 0 && ty < map->height) {
                    int idx = tx * map->height + ty;
                    map->tiles[idx] = ed->selected_tile;
                    map->rot[idx] = 0;
                    map->mirror_x[idx] = 0;
                    map->mirror_y[idx] = 0;
                }
            }
            else if (mx >= WINDOW_W - RIGHT_PANEL_W) {
                int pan_x = WINDOW_W - RIGHT_PANEL_W;
                int list_y = 35;
                for (int i = 0; i < ed->map_list.map_count; i++) {
                    if (my >= list_y + i*20 - 10 && my < list_y + i*20 + 10) {
                        ed->map_list.current_map = i;
                        Map *m = &ed->map_list.maps[i];
                        load_tileset(ed, m->tileset_path);
                        return;
                    }
                }
                int y = WINDOW_H - 150;
                if (my >= y && my < y+24) {
                    strcpy(ed->input_text, "map00");
                    strcpy(ed->input_text2, "20x15");
                    open_dialog(ed, DIALOG_NEW_MAP);
                }
                else if (my >= y+30 && my < y+54) {
                    if (current_map(ed)) {
                        char path[128];
                        snprintf(path, sizeof(path), "data/maps/%s.json", current_map(ed)->name);
                        map_save_to_json(current_map(ed), path);
                        save_tile_types(ed);
                        ed->save_blink_active = true;
                        ed->save_blink_time = SDL_GetTicks();
                    }
                }
                else if (my >= y+60 && my < y+84) {
                    if (ed->map_list.map_count > 1) {
                        safe_strcpy(ed->input_text, sizeof(ed->input_text), current_map(ed)->name);
                        open_dialog(ed, DIALOG_CONFIRM_DEL);
                    }
                }
                else if (my >= y+90 && my < y+114) {
                    safe_strcpy(ed->input_text, sizeof(ed->input_text), current_map(ed) ? current_map(ed)->name : "");
                    open_dialog(ed, DIALOG_RENAME_MAP);
                }
                else if (my >= y+120 && my < y+144) {
                    if (current_map(ed)) {
                        snprintf(ed->input_text, sizeof(ed->input_text), "%d", current_map(ed)->width);
                        snprintf(ed->input_text2, sizeof(ed->input_text2), "%d", current_map(ed)->height);
                    }
                    open_dialog(ed, DIALOG_RESIZE_MAP);
                }
            }
        }
    }

    // Панорамирование
    if (ed->panning && !ed->dialog_active) {
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        ed->cam_x -= (mx - ed->pan_start_x);
        ed->cam_y -= (my - ed->pan_start_y);
        ed->pan_start_x = mx; ed->pan_start_y = my;
        Map *map = current_map(ed);
        if (map) {
            float max_x = map->width * TILE_SIZE - MAP_W;
            float max_y = map->height * TILE_SIZE - MAP_H;
            if (ed->cam_x < 0) ed->cam_x = 0;
            if (ed->cam_y < 0) ed->cam_y = 0;
            if (ed->cam_x > max_x) ed->cam_x = max_x;
            if (ed->cam_y > max_y) ed->cam_y = max_y;
        }
    }

    // Непрерывное рисование (только если диалог не активен)
    Uint32 mouse_state = SDL_GetMouseState(NULL, NULL);
    if (!ed->dialog_active && (mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT)) && !ed->panning) {
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        if (mx >= MAP_X && mx < MAP_X + MAP_W && my >= MAP_Y && my < MAP_Y + MAP_H) {
            int world_x = (int)(mx - MAP_X + ed->cam_x);
            int world_y = (int)(my - MAP_Y + ed->cam_y);
            int tx = world_x / TILE_SIZE, ty = world_y / TILE_SIZE;
            Map *map = current_map(ed);
            if (map && tx >= 0 && tx < map->width && ty >= 0 && ty < map->height) {
                int idx = tx * map->height + ty;
                if (ed->mode == MODE_A) {
                    map->tiles[idx] = ed->selected_tile;
                    map->rot[idx] = 0;
                    map->mirror_x[idx] = 0;
                    map->mirror_y[idx] = 0;
                }
            }
        }
    }

    // Режим C (только если диалог не активен)
    if (!ed->dialog_active && ed->mode == MODE_C && ed->tileset_loaded && (mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT))) {
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        if (mx < LEFT_PANEL_W && my >= PALETTE_START_Y) {
            int rx = mx - PALETTE_START_X, ry = my - PALETTE_START_Y;
            int step = PALETTE_TILE_SIZE + 2;
            if (rx >= 0 && ry >= 0) {
                int col = rx / step, row = ry / step + ed->palette_scroll;
                if (col < PALETTE_COLS && rx % step < PALETTE_TILE_SIZE) {
                    int idx = row * PALETTE_COLS + col;
                    if (idx >= 0 && idx < ed->tile_count) ed->tile_types[idx] = ed->current_type;
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    Editor ed;
    editor_init(&ed);

    ed.window = SDL_CreateWindow("Map Editor C SDL2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                 WINDOW_W, WINDOW_H, SDL_WINDOW_SHOWN);
    ed.renderer = SDL_CreateRenderer(ed.window, -1, SDL_RENDERER_ACCELERATED);
    ed.font = TTF_OpenFont("C:/Windows/Fonts/consola.ttf", FONT_SIZE);
    if (!ed.font) ed.font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", FONT_SIZE);
    if (!ed.font) { printf("No font!\n"); return 1; }

    load_type_icons(&ed);
    load_transform_icons(&ed);

    CreateDirectoryA("data/maps", NULL);
    load_map_list(&ed);
    if (ed.map_list.map_count == 0) {
        create_map(&ed, "map00", 20, 15);
    } else {
        Map *cur = current_map(&ed);
        if (cur) load_tileset(&ed, cur->tileset_path);
    }

    Uint32 last_blink = SDL_GetTicks();
    bool running = true;
    while (running) {
        handle_input(&ed, &running);

        Uint32 now = SDL_GetTicks();
        if (now - last_blink >= 500) { ed.blink_visible = !ed.blink_visible; last_blink = now; }
        if (ed.save_blink_active && now - ed.save_blink_time >= 150) ed.save_blink_active = false;

        SDL_SetRenderDrawColor(ed.renderer, 30, 30, 30, 255);
        SDL_RenderClear(ed.renderer);

        render_left_panel(&ed);
        render_toolbar(&ed);
        render_map(&ed);
        render_right_panel(&ed);

        draw_dialog(&ed);  // Диалог поверх всего

        SDL_RenderPresent(ed.renderer);
        SDL_Delay(16);
    }

    if (current_map(&ed)) {
        char path[128];
        snprintf(path, sizeof(path), "data/maps/%s.json", current_map(&ed)->name);
        map_save_to_json(current_map(&ed), path);
    }
    save_tile_types(&ed);
    free_tileset(&ed);
    TTF_CloseFont(ed.font);
    for (int i = 0; i < 4; i++) if (ed.type_icons[i]) SDL_DestroyTexture(ed.type_icons[i]);
    for (int i = 0; i < 3; i++) if (ed.transform_icons[i]) SDL_DestroyTexture(ed.transform_icons[i]);
    for (int i = 0; i < ed.map_list.map_count; i++) map_free(&ed.map_list.maps[i]);
    free(ed.map_list.maps);
    SDL_DestroyRenderer(ed.renderer);
    SDL_DestroyWindow(ed.window);
    TTF_Quit(); IMG_Quit(); SDL_Quit();
    return 0;
}