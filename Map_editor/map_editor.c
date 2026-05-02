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
#define RIGHT_DELETE   4

#define DIALOG_NONE          0
#define DIALOG_NEW_MAP       1
#define DIALOG_RENAME_MAP    2
#define DIALOG_RESIZE_MAP    3
#define DIALOG_CONFIRM_DEL   4
#define DIALOG_MUSIC_MAP     5

// ─── Структуры данных ─────────────────────────
typedef struct {
    char name[64];
    int width, height;
    int *tiles;          // слой 0
    int *rot;
    int *mirror_x;
    int *mirror_y;
    int *tiles2;         // слой 1
    int *rot2;
    int *mirror_x2;
    int *mirror_y2;
    char tileset_path[256];
    char music_file[256];
    float music_volume;
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
    float zoom;
    int current_layer;
    bool show_layer1;
    bool show_layer2;
    SDL_Texture *transform_icons[4];

    bool dialog_active;
    int  dialog_type;
    char input_text[64];
    char input_text2[64];
    int dialog_active_field;
    Uint32 dialog_cursor_blink;

    bool save_blink_active;
	char music_fullpath[256];   // хранит полный относительный путь к музыке
	bool dialog_just_closed;   // запрещает рисование сразу после закрытия диалога
    Uint32 save_blink_time;
} Editor;

// Прототипы
Map* current_map(Editor *ed);
void find_first_tileset_path(char *out, size_t out_len);
void find_first_sound_path(char *out, size_t out_len);
void get_relative_path(const char *abs_path, char *out, size_t out_len);

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
	ed->music_fullpath[0] = '\0';
	ed->dialog_just_closed = false;
    ed->save_blink_time = 0;
    ed->dialog_active_field = 0;
    ed->dialog_cursor_blink = 0;
    for (int i = 0; i < 4; i++) ed->type_icons[i] = NULL;
    for (int i = 0; i < 4; i++) ed->transform_icons[i] = NULL;
    ed->map_list.maps = NULL;
    ed->map_list.map_count = 0;
    ed->map_list.current_map = 0;
    ed->current_layer = 0;
    ed->show_layer1 = true;
    ed->show_layer2 = true;
    ed->zoom = 1.0f;
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
        if (ed->type_icons[i])
            SDL_DestroyTexture(ed->type_icons[i]);
        ed->type_icons[i] = NULL;
    }

    const char *filenames[4] = {
        "../assets/icons/passable.png",
        "../assets/icons/block.png",
        "../assets/icons/slow.png",
        "../assets/icons/under.png"
    };

    SDL_Color fallback[4] = {
        {0,   200, 0,   255},
        {200, 0,   0,   255},
        {200, 200, 0,   255},
        {0,   0,   200, 255}
    };

    for (int i = 0; i < 4; i++) {
        SDL_Surface *surf = IMG_Load(filenames[i]);
        if (surf) {
            ed->type_icons[i] = SDL_CreateTextureFromSurface(ed->renderer, surf);
            SDL_FreeSurface(surf);
        } else {
            ed->type_icons[i] = create_color_texture(ed->renderer,
                                     fallback[i].r, fallback[i].g, fallback[i].b,
                                     16, 16);
        }
    }
}

void load_transform_icons(Editor *ed) {
    const char *files[4] = {
        "../assets/icons/rotate.png",
        "../assets/icons/flip_h.png",
        "../assets/icons/flip_v.png",
        "../assets/icons/Delete_Tile.png"
    };
    Uint8 fr[4] = {200, 100, 100, 200};
    Uint8 fg[4] = {100, 200, 100, 80};
    Uint8 fb[4] = {100, 100, 200, 80};
    for (int i = 0; i < 4; i++) {
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

void save_tile_types_for_tileset(const Editor *ed, const char *tileset_path) {
    if (!ed->tileset_loaded || !ed->tile_types) return;

    char safe[256];
    safe_strcpy(safe, sizeof(safe), tileset_path);
    for (char *p = safe; *p; p++) {
        if (*p == '\\' || *p == '/' || *p == ':')
            *p = '_';
    }

    CreateDirectoryA("../data/tile_types", NULL);

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "../data/tile_types/%s.json", safe);

    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < ed->tile_count; i++) {
        cJSON_AddItemToArray(root, cJSON_CreateNumber(ed->tile_types[i]));
    }

    char *str = cJSON_Print(root);
    FILE *f = fopen(filepath, "w");
    if (f) {
        fputs(str, f);
        fclose(f);
    }
    cJSON_Delete(root);
    free(str);
}

void load_tile_types_for_tileset(Editor *ed, const char *tileset_path) {
    if (!ed->tileset_loaded) return;
    if (ed->tile_types) { free(ed->tile_types); ed->tile_types = NULL; }
    ed->tile_types = (int*)calloc(ed->tile_count, sizeof(int));
    char safe[256];
    safe_strcpy(safe, sizeof(safe), tileset_path);
    for (char *p = safe; *p; p++)
        if (*p == '\\' || *p == '/' || *p == ':') *p = '_';
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "../data/tile_types/%s.json", safe);
    FILE *f = fopen(filepath, "r");
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
    char full_tileset[512];

    // Если путь абсолютный (Windows), используем как есть
if (path[0] && path[1] == ':') {
    snprintf(full_tileset, sizeof(full_tileset), "%s", path);
} else {
    snprintf(full_tileset, sizeof(full_tileset), "../%s", path);
}
    SDL_Surface *surface = IMG_Load(full_tileset);
    if (!surface) return 0;   // добавлена точка с запятой

int cols = surface->w / TILE_SIZE;
int rows = surface->h / TILE_SIZE;
int palette_cols = PALETTE_COLS;   // 8
int strips = cols / palette_cols;  // 4 для 32×32

ed->tile_count = cols * rows;
ed->tiles = (SDL_Texture**)malloc(ed->tile_count * sizeof(SDL_Texture*));

int idx = 0;
for (int strip = 0; strip < strips; strip++) {
    int start_col = strip * palette_cols;
    int end_col = start_col + palette_cols;
    for (int r = 0; r < rows; r++) {
        for (int c = start_col; c < end_col; c++) {
            SDL_Rect src = { c * TILE_SIZE, r * TILE_SIZE, TILE_SIZE, TILE_SIZE };
            SDL_Surface *tile_surf = SDL_CreateRGBSurface(0, TILE_SIZE, TILE_SIZE, 32, 0,0,0,0);
            SDL_BlitSurface(surface, &src, tile_surf, NULL);
            ed->tiles[idx++] = SDL_CreateTextureFromSurface(ed->renderer, tile_surf);
            SDL_FreeSurface(tile_surf);
        }
    }
}

    SDL_FreeSurface(surface);
    get_relative_path(path, ed->tileset_path, sizeof(ed->tileset_path));
    ed->tileset_loaded = true;
    ed->palette_scroll = 0;
    ed->selected_tile = 0;
    load_tile_types_for_tileset(ed, path);
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
    ofn.lpstrFilter = "All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn) == TRUE) {
        safe_strcpy(out_path, out_len, ofn.lpstrFile);
        return true;
    }
    return false;
}

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
    get_relative_path(tileset_json->valuestring, map->tileset_path, sizeof(map->tileset_path));

    // Музыка
    cJSON *music_json = cJSON_GetObjectItem(root, "music");
    if (music_json && cJSON_IsObject(music_json)) {
        cJSON *music_file_json = cJSON_GetObjectItem(music_json, "file");
        cJSON *music_vol_json  = cJSON_GetObjectItem(music_json, "volume");
        if (music_file_json && cJSON_IsString(music_file_json))
            safe_strcpy(map->music_file, sizeof(map->music_file), music_file_json->valuestring);
        else
            map->music_file[0] = '\0';
        if (music_vol_json && cJSON_IsNumber(music_vol_json))
            map->music_volume = (float)music_vol_json->valuedouble;
        else
            map->music_volume = 0.8f;
    } else {
        map->music_file[0] = '\0';
        map->music_volume = 0.8f;
    }

    // Первый слой
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

    // Второй слой
    map->tiles2     = (int*)malloc(sz * sizeof(int));
    map->rot2       = (int*)malloc(sz * sizeof(int));
    map->mirror_x2  = (int*)malloc(sz * sizeof(int));
    map->mirror_y2  = (int*)malloc(sz * sizeof(int));
    if (!map->tiles2 || !map->rot2 || !map->mirror_x2 || !map->mirror_y2) {
        free(map->tiles2); free(map->rot2); free(map->mirror_x2); free(map->mirror_y2);
        // Освобождаем уже выделенные массивы первого слоя
        free(map->tiles); free(map->rot); free(map->mirror_x); free(map->mirror_y);
        cJSON_Delete(root); return false;
    }

    cJSON *t2_json = cJSON_GetObjectItem(root, "tiles2");
    cJSON *r2_json = cJSON_GetObjectItem(root, "rot2");
    cJSON *mx2_json = cJSON_GetObjectItem(root, "mirror_x2");
    cJSON *my2_json = cJSON_GetObjectItem(root, "mirror_y2");

    if (t2_json && r2_json && mx2_json && my2_json) {
        for (int x = 0; x < w; x++) {
            cJSON *col_t2  = cJSON_GetArrayItem(t2_json, x);
            cJSON *col_r2  = cJSON_GetArrayItem(r2_json, x);
            cJSON *col_mx2 = cJSON_GetArrayItem(mx2_json, x);
            cJSON *col_my2 = cJSON_GetArrayItem(my2_json, x);
            for (int y = 0; y < h; y++) {
                int idx = x * h + y;
                map->tiles2[idx]     = (col_t2  && cJSON_IsArray(col_t2)  && cJSON_GetArrayItem(col_t2,  y)) ? cJSON_GetArrayItem(col_t2,  y)->valueint : -1;
                map->rot2[idx]       = (col_r2  && cJSON_IsArray(col_r2)  && cJSON_GetArrayItem(col_r2,  y)) ? cJSON_GetArrayItem(col_r2,  y)->valueint : 0;
                map->mirror_x2[idx]  = (col_mx2 && cJSON_IsArray(col_mx2) && cJSON_GetArrayItem(col_mx2, y)) ? cJSON_IsTrue(cJSON_GetArrayItem(col_mx2, y)) : false;
                map->mirror_y2[idx]  = (col_my2 && cJSON_IsArray(col_my2) && cJSON_GetArrayItem(col_my2, y)) ? cJSON_IsTrue(cJSON_GetArrayItem(col_my2, y)) : false;
            }
        }
    } else {
        for (int i = 0; i < sz; i++) map->tiles2[i] = -1;
    }

    cJSON_Delete(root);
    return true;
}

void map_init(Map *map, const char *name, int w, int h, const char *tileset) {
    safe_strcpy(map->name, sizeof(map->name), name);
    map->width = w;
    map->height = h;
    safe_strcpy(map->tileset_path, sizeof(map->tileset_path), tileset);
    map->music_file[0] = '\0';
    map->music_volume = 0.8f;

    int sz = w * h;
    map->tiles     = (int*)calloc(sz, sizeof(int));
    map->rot       = (int*)calloc(sz, sizeof(int));
    map->mirror_x  = (int*)calloc(sz, sizeof(int));
    map->mirror_y  = (int*)calloc(sz, sizeof(int));

    map->tiles2    = (int*)calloc(sz, sizeof(int));
    map->rot2      = (int*)calloc(sz, sizeof(int));
    map->mirror_x2 = (int*)calloc(sz, sizeof(int));
    map->mirror_y2 = (int*)calloc(sz, sizeof(int));

    for (int i = 0; i < sz; i++)
        map->tiles2[i] = -1;
}

void map_free(Map *map) {
    free(map->tiles); free(map->rot); free(map->mirror_x); free(map->mirror_y);
    free(map->tiles2); free(map->rot2); free(map->mirror_x2); free(map->mirror_y2);
}

void find_first_tileset_path(char *out, size_t out_len) {
    out[0] = '\0';
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA("../assets/tilesets/*.png", &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;
    snprintf(out, out_len, "../assets/tilesets/%s", findData.cFileName);
    FindClose(hFind);
}

void get_relative_path(const char *abs_path, char *out, size_t out_len) {
    const char *assets = strstr(abs_path, "assets");
    if (assets) {
        safe_strcpy(out, out_len, assets);
    } else {
        const char *name = strrchr(abs_path, '\\');
        if (!name) name = strrchr(abs_path, '/');
        if (name) name++; else name = abs_path;
        safe_strcpy(out, out_len, name);
    }
}

void find_first_sound_path(char *out, size_t out_len) {
    out[0] = '\0';
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA("../assets/sounds/*.mp3", &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;
    snprintf(out, out_len, "../assets/sounds/%s", findData.cFileName);
    FindClose(hFind);
}

void map_save_to_json(const Map *map, const char *filename) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", map->name);
    cJSON_AddNumberToObject(root, "width", map->width);
    cJSON_AddNumberToObject(root, "height", map->height);
    cJSON_AddStringToObject(root, "tileset", map->tileset_path);

    cJSON *music = cJSON_CreateObject();
    cJSON_AddStringToObject(music, "file", map->music_file);
    cJSON_AddNumberToObject(music, "volume", map->music_volume);
    cJSON_AddItemToObject(root, "music", music);

    // Первый слой (всегда сохраняется)
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

    // Проверяем, есть ли во втором слое хоть один тайл (не -1)
    bool has_layer2 = false;
    int total_tiles = map->width * map->height;
    for (int i = 0; i < total_tiles; i++) {
        if (map->tiles2[i] != -1) {
            has_layer2 = true;
            break;
        }
    }

    // Второй слой сохраняется только если он не пуст
    if (has_layer2) {
        cJSON *t2  = cJSON_AddArrayToObject(root, "tiles2");
        cJSON *r2  = cJSON_AddArrayToObject(root, "rot2");
        cJSON *mx2 = cJSON_AddArrayToObject(root, "mirror_x2");
        cJSON *my2 = cJSON_AddArrayToObject(root, "mirror_y2");

        for (int x = 0; x < map->width; x++) {
            cJSON *col_t2  = cJSON_CreateArray();
            cJSON *col_r2  = cJSON_CreateArray();
            cJSON *col_mx2 = cJSON_CreateArray();
            cJSON *col_my2 = cJSON_CreateArray();
            for (int y = 0; y < map->height; y++) {
                int idx = x * map->height + y;
                cJSON_AddItemToArray(col_t2,  cJSON_CreateNumber(map->tiles2[idx]));
                cJSON_AddItemToArray(col_r2,  cJSON_CreateNumber(map->rot2[idx]));
                cJSON_AddItemToArray(col_mx2, cJSON_CreateBool(map->mirror_x2[idx]));
                cJSON_AddItemToArray(col_my2, cJSON_CreateBool(map->mirror_y2[idx]));
            }
            cJSON_AddItemToArray(t2,  col_t2);
            cJSON_AddItemToArray(r2,  col_r2);
            cJSON_AddItemToArray(mx2, col_mx2);
            cJSON_AddItemToArray(my2, col_my2);
        }
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
    HANDLE hFind = FindFirstFileA("../data/maps/*.json", &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        char path[512];
        snprintf(path, sizeof(path), "../data/maps/%s", findData.cFileName);
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

void create_map(Editor *ed, const char *name, int w, int h, const char *tileset) {
    if (w < MIN_MAP_SIZE) w = MIN_MAP_SIZE;
    if (h < MIN_MAP_SIZE) h = MIN_MAP_SIZE;
    if (w > MAX_MAP_SIZE) w = MAX_MAP_SIZE;
    if (h > MAX_MAP_SIZE) h = MAX_MAP_SIZE;

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
    map_init(&new_map, fullname, w, h, tileset);

    char first_sound[512];
    find_first_sound_path(first_sound, sizeof(first_sound));
    if (first_sound[0] != '\0')
        safe_strcpy(new_map.music_file, sizeof(new_map.music_file), first_sound);

    char path[512]; snprintf(path, sizeof(path), "../data/maps/%s.json", fullname);
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
    char path[512]; snprintf(path, sizeof(path), "../data/maps/%s.json", map->name);
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
    if (!map || strlen(new_name) == 0) return;

    char old_path[128], new_path[128];
    snprintf(old_path, sizeof(old_path), "../data/maps/%s.json", map->name);
    snprintf(new_path, sizeof(new_path), "../data/maps/%s.json", new_name);

    if (rename(old_path, new_path) != 0) return;

    safe_strcpy(map->name, sizeof(map->name), new_name);
    map_save_to_json(map, new_path);

    int old_idx = ed->map_list.current_map;
    load_map_list(ed);

    ed->map_list.current_map = old_idx;
    if (ed->map_list.current_map >= ed->map_list.map_count)
        ed->map_list.current_map = ed->map_list.map_count - 1;

    for (int i = 0; i < ed->map_list.map_count; i++) {
        if (strcmp(ed->map_list.maps[i].name, new_name) == 0) {
            ed->map_list.current_map = i;
            break;
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

    for (int x = 0; x < map->width && x < new_w; x++) {
        for (int y = 0; y < map->height && y < new_h; y++) {
            int old_idx = x * map->height + y;
            int new_idx = x * new_h + y;

            bigger.tiles[new_idx]     = map->tiles[old_idx];
            bigger.rot[new_idx]       = map->rot[old_idx];
            bigger.mirror_x[new_idx]  = map->mirror_x[old_idx];
            bigger.mirror_y[new_idx]  = map->mirror_y[old_idx];

            bigger.tiles2[new_idx]    = map->tiles2[old_idx];
            bigger.rot2[new_idx]      = map->rot2[old_idx];
            bigger.mirror_x2[new_idx] = map->mirror_x2[old_idx];
            bigger.mirror_y2[new_idx] = map->mirror_y2[old_idx];
        }
    }

    map_free(map);
    *map = bigger;

    char path[512];
    snprintf(path, sizeof(path), "../data/maps/%s.json", map->name);
    map_save_to_json(map, path);
}

// ─── Отрисовка текста (вспомогательные) ─────
void draw_text_centered(SDL_Renderer *ren, TTF_Font *font, const char *text, int cx, int cy, SDL_Color color) {
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_Rect dst = { cx - surf->w/2, cy - surf->h/2, surf->w, surf->h };
    SDL_RenderCopy(ren, tex, NULL, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

void draw_input_field(Editor *ed, SDL_Rect field, const char *text, bool active, bool cursor_visible) {
    SDL_SetRenderDrawColor(ed->renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(ed->renderer, &field);
    if (active) {
        SDL_SetRenderDrawColor(ed->renderer, 0, 120, 215, 255);
    } else {
        SDL_SetRenderDrawColor(ed->renderer, 150, 150, 150, 255);
    }
    SDL_RenderDrawRect(ed->renderer, &field);

    SDL_Surface *surf = TTF_RenderUTF8_Blended(ed->font, text, (SDL_Color){0,0,0,255});
    if (surf) {
        SDL_Texture *tex = SDL_CreateTextureFromSurface(ed->renderer, surf);
        SDL_Rect text_rect = { field.x + 4, field.y + (field.h - surf->h)/2, surf->w, surf->h };
        SDL_RenderCopy(ed->renderer, tex, NULL, &text_rect);
        if (active && cursor_visible) {
            int cur_x = text_rect.x + text_rect.w + 1;
            SDL_SetRenderDrawColor(ed->renderer, 0, 0, 0, 255);
            SDL_RenderDrawLine(ed->renderer, cur_x, field.y + 3, cur_x, field.y + field.h - 6);
        }
        SDL_FreeSurface(surf);
        SDL_DestroyTexture(tex);
    } else {
        if (active && cursor_visible) {
            int cur_x = field.x + 4;
            SDL_SetRenderDrawColor(ed->renderer, 0, 0, 0, 255);
            SDL_RenderDrawLine(ed->renderer, cur_x, field.y + 3, cur_x, field.y + field.h - 6);
        }
    }
}

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
            if (ed->type_icons[t])
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
                if (type >= 0 && type < 4 && ed->type_icons[type]) {
                    SDL_Rect icon_dst = { dst.x + PALETTE_TILE_SIZE - 16, dst.y + PALETTE_TILE_SIZE - 16, 16, 16 };
                    SDL_RenderCopy(ed->renderer, ed->type_icons[type], NULL, &icon_dst);
                }
            }

            if (ed->mode == MODE_A && idx == ed->selected_tile && ed->blink_visible) {
            SDL_SetRenderDrawColor(ed->renderer, 255, 255, 0, 255);
            int thickness = 2;   // число пикселей толщины
            for (int t = 0; t < thickness; t++) {
            SDL_Rect r = { frame.x - t, frame.y - t, frame.w + 2*t, frame.h + 2*t };
            SDL_RenderDrawRect(ed->renderer, &r);
                }
            }
        }
    }
}

void render_toolbar(Editor *ed) {
    SDL_Rect bar = { MAP_X, 0, MAP_W, TOOLBAR_H };
    SDL_SetRenderDrawColor(ed->renderer, 70, 70, 70, 255);
    SDL_RenderFillRect(ed->renderer, &bar);

    // 4 кнопки для правой кнопки мыши: поворот, отражение по горизонтали, по вертикали, удаление
    struct { int x; int mode; int icon_index; } btns[] = {
        { MAP_X + 5,   RIGHT_ROTATE, 0 },
        { MAP_X + 45,  RIGHT_FLIP_H, 1 },
        { MAP_X + 85,  RIGHT_FLIP_V, 2 },
        { MAP_X + 125, RIGHT_DELETE, 3 }
    };
    int btn_w = 38, btn_h = TOOLBAR_H - 6;

    for (int i = 0; i < 4; i++) {
        SDL_Rect btn_rect = { btns[i].x, 3, btn_w, btn_h };
        bool active = (ed->right_click_mode == btns[i].mode);
        SDL_SetRenderDrawColor(ed->renderer, active ? 140 : 100, active ? 140 : 100, active ? 140 : 100, 255);
        SDL_RenderFillRect(ed->renderer, &btn_rect);

        if (ed->transform_icons[btns[i].icon_index]) {
            SDL_Rect icon_dst = { btn_rect.x + 3, btn_rect.y + 3, btn_rect.w - 6, btn_rect.h - 6 };
            SDL_RenderCopy(ed->renderer, ed->transform_icons[btns[i].icon_index], NULL, &icon_dst);
        } else {
            const char *labels[] = {"R", "FH", "FV", "Del"};
            draw_text_centered(ed->renderer, ed->font, labels[i], btn_rect.x + btn_rect.w/2, btn_rect.y + btn_rect.h/2, (SDL_Color){255,255,255,255});
        }
    }

    // Кнопки видимости слоёв (сдвинуты вправо)
    int vis_btn_w = 30;
    int vis_btn_h = btn_h;
    int vis_btn_y = 3;
    int vis_btn_x1 = MAP_X + 180;      // было 140 -> 180
    int vis_btn_x2 = vis_btn_x1 + vis_btn_w + 4;

    SDL_Rect vis1 = { vis_btn_x1, vis_btn_y, vis_btn_w, vis_btn_h };
    SDL_Rect vis2 = { vis_btn_x2, vis_btn_y, vis_btn_w, vis_btn_h };

    int mx, my;
    SDL_GetMouseState(&mx, &my);

    // Кнопка L1
    bool hover1 = (mx >= vis1.x && mx < vis1.x+vis1.w && my >= vis1.y && my < vis1.y+vis1.h);
    SDL_Color col1 = ed->show_layer1 ? (SDL_Color){100,255,100,255} : (SDL_Color){100,100,100,255};
    if (hover1) col1 = (SDL_Color){150,255,150,255};
    SDL_SetRenderDrawColor(ed->renderer, col1.r, col1.g, col1.b, 255);
    SDL_RenderFillRect(ed->renderer, &vis1);
    draw_text_centered(ed->renderer, ed->font, "L1", vis1.x + vis1.w/2, vis1.y + vis1.h/2, (SDL_Color){255,255,255,255});

    // Кнопка L2
    bool hover2 = (mx >= vis2.x && mx < vis2.x+vis2.w && my >= vis2.y && my < vis2.y+vis2.h);
    SDL_Color col2 = ed->show_layer2 ? (SDL_Color){100,255,100,255} : (SDL_Color){100,100,100,255};
    if (hover2) col2 = (SDL_Color){150,255,150,255};
    SDL_SetRenderDrawColor(ed->renderer, col2.r, col2.g, col2.b, 255);
    SDL_RenderFillRect(ed->renderer, &vis2);
    draw_text_centered(ed->renderer, ed->font, "L2", vis2.x + vis2.w/2, vis2.y + vis2.h/2, (SDL_Color){255,255,255,255});

    // Кнопка переключения активного слоя
    int active_btn_x = vis_btn_x2 + vis_btn_w + 8;
    SDL_Rect layer_btn = { active_btn_x, 3, btn_w, btn_h };
    bool layer_hover = (mx >= layer_btn.x && mx < layer_btn.x+btn_w && my >= layer_btn.y && my < layer_btn.y+btn_h);

    SDL_Color layer_color;
    if (layer_hover) layer_color = (SDL_Color){180,180,180,255};
    else layer_color = (ed->current_layer == 0) ? (SDL_Color){100,140,100,255} : (SDL_Color){140,100,100,255};

    SDL_SetRenderDrawColor(ed->renderer, layer_color.r, layer_color.g, layer_color.b, 255);
    SDL_RenderFillRect(ed->renderer, &layer_btn);

    char layer_label[2] = { '0' + ed->current_layer + 1, '\0' };
    draw_text_centered(ed->renderer, ed->font, layer_label,
                       layer_btn.x + btn_w/2, layer_btn.y + btn_h/2, (SDL_Color){255,255,255,255});

    // Подсказка "Active Layer" справа от кнопки
    {
        SDL_Color hint_color = {200, 200, 200, 255};
        int hint_x = active_btn_x + btn_w + 12;
        int hint_y = layer_btn.y + (layer_btn.h - FONT_SIZE) / 2;
        SDL_Surface* text_surf = TTF_RenderUTF8_Blended(ed->font, "Active Layer", hint_color);
        if (text_surf) {
            SDL_Texture* text_tex = SDL_CreateTextureFromSurface(ed->renderer, text_surf);
            SDL_Rect text_dst = { hint_x, hint_y, text_surf->w, text_surf->h };
            SDL_RenderCopy(ed->renderer, text_tex, NULL, &text_dst);
            SDL_FreeSurface(text_surf);
            SDL_DestroyTexture(text_tex);
        }
    }
}
	
// Карта
void render_map(Editor *ed) {
    Map *map = current_map(ed);
    if (!map || !ed->tileset_loaded) return;

    float zoom = ed->zoom;
    float scaled_tile = TILE_SIZE * zoom;

    SDL_Rect map_area = { MAP_X, MAP_Y, MAP_W, MAP_H };
    SDL_RenderSetClipRect(ed->renderer, &map_area);

    int start_x = (int)(ed->cam_x / TILE_SIZE);
    int start_y = (int)(ed->cam_y / TILE_SIZE);
    int end_x = start_x + (int)(MAP_W / scaled_tile) + 1;
    int end_y = start_y + (int)(MAP_H / scaled_tile) + 1;

    if (start_x < 0) start_x = 0;
    if (start_y < 0) start_y = 0;
    if (end_x > map->width) end_x = map->width;
    if (end_y > map->height) end_y = map->height;
	
    // Первый слой
    if (ed->show_layer1) {
        for (int x = start_x; x < end_x; x++) {
            for (int y = start_y; y < end_y; y++) {
                int idx = x * map->height + y;
                int tile_id = map->tiles[idx];
                if (tile_id < 0 || tile_id >= ed->tile_count) continue;

                SDL_Texture *tex = ed->tiles[tile_id];
                double angle = map->rot[idx] * 90.0;
                SDL_RendererFlip flip = SDL_FLIP_NONE;
                if (map->mirror_x[idx]) flip |= SDL_FLIP_HORIZONTAL;
                if (map->mirror_y[idx]) flip |= SDL_FLIP_VERTICAL;

                SDL_FRect dst = {
                    MAP_X + (x * TILE_SIZE - ed->cam_x) * zoom,
                    MAP_Y + (y * TILE_SIZE - ed->cam_y) * zoom,
                    scaled_tile,
                    scaled_tile
                };
                SDL_FPoint center = { scaled_tile / 2.0f, scaled_tile / 2.0f };
                SDL_RenderCopyExF(ed->renderer, tex, NULL, &dst, angle, &center, flip);
            }
        }
    }
	
    // Второй слой
if (ed->show_layer2) {
    for (int x = start_x; x < end_x; x++) {
        for (int y = start_y; y < end_y; y++) {
            int idx = x * map->height + y;
            int tile_id = map->tiles2[idx];
            if (tile_id < 0 || tile_id >= ed->tile_count) continue;

            SDL_Texture *tex = ed->tiles[tile_id];
            double angle = map->rot2[idx] * 90.0;
            SDL_RendererFlip flip = SDL_FLIP_NONE;
            if (map->mirror_x2[idx]) flip |= SDL_FLIP_HORIZONTAL;
            if (map->mirror_y2[idx]) flip |= SDL_FLIP_VERTICAL;

            SDL_FRect dst = {
                MAP_X + (x * TILE_SIZE - ed->cam_x) * zoom,
                MAP_Y + (y * TILE_SIZE - ed->cam_y) * zoom,
                scaled_tile,
                scaled_tile
            };
            SDL_FPoint center = { scaled_tile / 2.0f, scaled_tile / 2.0f };

            // Полупрозрачность, только если первый слой тоже видим
            if (ed->show_layer1) {
                SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);   // включаем смешивание
                SDL_SetTextureAlphaMod(tex, 96);                     // % прозрачности
            }
            SDL_RenderCopyExF(ed->renderer, tex, NULL, &dst, angle, &center, flip);
            if (ed->show_layer1) {
                SDL_SetTextureAlphaMod(tex, 255);                     // возвращаем полную непрозрачность
                SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);     // отключаем смешивание
            }
        }
    }
}

    // Подсветка тайла под мышью (рисуем целочисленный прямоугольник)
    int mx, my;
    SDL_GetMouseState(&mx, &my);
    if (mx >= MAP_X && mx < MAP_X + MAP_W && my >= MAP_Y && my < MAP_Y + MAP_H) {
        float world_x = (mx - MAP_X) / zoom + ed->cam_x;
        float world_y = (my - MAP_Y) / zoom + ed->cam_y;
        int tx = (int)(world_x / TILE_SIZE);
        int ty = (int)(world_y / TILE_SIZE);
        if (tx >= 0 && tx < map->width && ty >= 0 && ty < map->height) {
            SDL_Rect hl = {
                (int)(MAP_X + (tx * TILE_SIZE - ed->cam_x) * zoom),
                (int)(MAP_Y + (ty * TILE_SIZE - ed->cam_y) * zoom),
                (int)scaled_tile,
                (int)scaled_tile
            };
            if (ed->current_layer == 0)
                SDL_SetRenderDrawColor(ed->renderer, 255, 255, 0, 255);   // жёлтый для слоя 1
            else
                SDL_SetRenderDrawColor(ed->renderer, 100, 200, 255, 255); // голубой для слоя 2
            SDL_RenderDrawRect(ed->renderer, &hl);
        }
    }

    SDL_RenderSetClipRect(ed->renderer, NULL);
}

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
    int y = WINDOW_H - 180;
    const char *names[] = {"New Map", "Save Map", "Delete Map", "Rename Map", "Resize Map", "Set Music"};
    int mx, my;
    SDL_GetMouseState(&mx, &my);

    for (int i = 0; i < 6; i++) {
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

void open_dialog(Editor *ed, int type) {
    ed->dialog_active = true;
    ed->dialog_type = type;
    ed->dialog_active_field = 0;
    ed->dialog_cursor_blink = SDL_GetTicks();
    SDL_StartTextInput();

    switch (type) {
        case DIALOG_MUSIC_MAP: {
    Map *cur = current_map(ed);
    if (cur) {
        // Сохраняем полный путь в music_fullpath
        safe_strcpy(ed->music_fullpath, sizeof(ed->music_fullpath), cur->music_file);
        // Извлекаем имя файла для отображения
        const char *full = ed->music_fullpath;
        const char *name = strrchr(full, '/');
        if (!name) name = strrchr(full, '\\');
        if (name) name++; else name = full;
        // Обрезаем до 12 символов с троеточием
        char display_name[64];
        size_t len = strlen(name);
        if (len > 12) {
            memcpy(display_name, name, 12);
            memcpy(display_name + 12, "...", 3);
            display_name[15] = '\0';
        } else {
            strcpy(display_name, name);
        }
        snprintf(ed->input_text, sizeof(ed->input_text), "%s", display_name);
        snprintf(ed->input_text2, sizeof(ed->input_text2), "%.2f", cur->music_volume);
    } else {
        ed->music_fullpath[0] = '\0';
        ed->input_text[0] = '\0';
        strcpy(ed->input_text2, "0.80");
    }
    break;
}
        default: break;
    }
}

void close_dialog(Editor *ed) {
    ed->dialog_active = false;
    ed->dialog_type = DIALOG_NONE;
    ed->dialog_active_field = 0;
	ed->dialog_just_closed = true;
    memset(ed->input_text, 0, sizeof(ed->input_text));
    memset(ed->input_text2, 0, sizeof(ed->input_text2));
    SDL_StopTextInput();
}

void draw_dialog(Editor *ed) {
    if (!ed->dialog_active) return;

    SDL_SetRenderDrawBlendMode(ed->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ed->renderer, 0, 0, 0, 180);
    SDL_Rect full = { 0, 0, WINDOW_W, WINDOW_H };
    SDL_RenderFillRect(ed->renderer, &full);
    SDL_SetRenderDrawBlendMode(ed->renderer, SDL_BLENDMODE_NONE);

    SDL_Rect dlg = { WINDOW_W/2 - 180, WINDOW_H/2 - 110, 360, 220 };
    SDL_SetRenderDrawColor(ed->renderer, 230, 230, 230, 255);
    SDL_RenderFillRect(ed->renderer, &dlg);
    SDL_SetRenderDrawColor(ed->renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(ed->renderer, &dlg);

    SDL_Color black = {0,0,0,255};
    SDL_Color white = {255,255,255,255};
    int y_text = dlg.y + 20;

    SDL_Rect field1, field2;
    bool show_cursor = (SDL_GetTicks() - ed->dialog_cursor_blink) < 400;

    switch (ed->dialog_type) {
        case DIALOG_CONFIRM_DEL:
            draw_text_centered(ed->renderer, ed->font, "Delete this map?", dlg.x + 180, y_text, black);
            draw_text_centered(ed->renderer, ed->font, ed->input_text, dlg.x + 180, y_text + 30, black);
            break;

        case DIALOG_NEW_MAP:
            draw_text_centered(ed->renderer, ed->font, "Name:", dlg.x + 50, y_text + 10, black);
            field1 = (SDL_Rect){ dlg.x + 110, y_text - 2, 200, 24 };
            draw_input_field(ed, field1, ed->input_text, ed->dialog_active_field == 0, show_cursor);

            draw_text_centered(ed->renderer, ed->font, "Size (WxH):", dlg.x + 50, y_text + 45, black);
            field2 = (SDL_Rect){ dlg.x + 110, y_text + 33, 200, 24 };
            draw_input_field(ed, field2, ed->input_text2, ed->dialog_active_field == 1, show_cursor);
            break;

        case DIALOG_RENAME_MAP:
            draw_text_centered(ed->renderer, ed->font, "New name:", dlg.x + 50, y_text + 15, black);
            field1 = (SDL_Rect){ dlg.x + 130, y_text + 3, 180, 24 };
            draw_input_field(ed, field1, ed->input_text, true, show_cursor);
            break;

        case DIALOG_RESIZE_MAP:
            draw_text_centered(ed->renderer, ed->font, "Width:", dlg.x + 50, y_text + 10, black);
            field1 = (SDL_Rect){ dlg.x + 110, y_text - 2, 180, 24 };
            draw_input_field(ed, field1, ed->input_text, ed->dialog_active_field == 0, show_cursor);

            draw_text_centered(ed->renderer, ed->font, "Height:", dlg.x + 50, y_text + 45, black);
            field2 = (SDL_Rect){ dlg.x + 110, y_text + 33, 180, 24 };
            draw_input_field(ed, field2, ed->input_text2, ed->dialog_active_field == 1, show_cursor);
            break;

        case DIALOG_MUSIC_MAP:
            draw_text_centered(ed->renderer, ed->font, "Music file:", dlg.x + 50, y_text + 5, black);
            field1 = (SDL_Rect){ dlg.x + 120, y_text - 2, 150, 24 };
            draw_input_field(ed, field1, ed->input_text, ed->dialog_active_field == 0, show_cursor);
            SDL_Rect browse_btn = { dlg.x + 280, y_text - 2, 65, 24 };
            SDL_SetRenderDrawColor(ed->renderer, 150, 150, 150, 255);
            SDL_RenderFillRect(ed->renderer, &browse_btn);
            draw_text_centered(ed->renderer, ed->font, "...", browse_btn.x + browse_btn.w/2, browse_btn.y + browse_btn.h/2, black);

            draw_text_centered(ed->renderer, ed->font, "Volume (0-1):", dlg.x + 50, y_text + 40, black);
            field2 = (SDL_Rect){ dlg.x + 140, y_text + 28, 80, 24 };
            draw_input_field(ed, field2, ed->input_text2, ed->dialog_active_field == 1, show_cursor);
            break;
    }

    SDL_Rect ok = { dlg.x + 30, dlg.y + 180, 110, 28 };
    SDL_Rect cancel = { dlg.x + 200, dlg.y + 180, 110, 28 };
    SDL_SetRenderDrawColor(ed->renderer, 140, 140, 140, 255);
    SDL_RenderFillRect(ed->renderer, &ok);
    SDL_RenderFillRect(ed->renderer, &cancel);

    const char *ok_text = (ed->dialog_type == DIALOG_CONFIRM_DEL) ? "Yes" : "OK";
    draw_text_centered(ed->renderer, ed->font, ok_text, ok.x + ok.w/2, ok.y + ok.h/2, white);
    draw_text_centered(ed->renderer, ed->font, "Cancel", cancel.x + cancel.w/2, cancel.y + cancel.h/2, white);
}

void handle_dialog_click(Editor *ed, int mx, int my) {
    SDL_Rect dlg = { WINDOW_W/2 - 180, WINDOW_H/2 - 110, 360, 220 };
    SDL_Rect ok = { dlg.x + 30, dlg.y + 180, 110, 28 };
    SDL_Rect cancel = { dlg.x + 200, dlg.y + 180, 110, 28 };

    if (ed->dialog_type == DIALOG_MUSIC_MAP) {
    SDL_Rect browse_btn = { dlg.x + 280, dlg.y + 18, 65, 24 };
    if (mx >= browse_btn.x && mx < browse_btn.x+browse_btn.w &&
        my >= browse_btn.y && my < browse_btn.y+browse_btn.h)
    {
        char path[256];
        if (open_file_dialog(path, sizeof(path))) {
            // Сохраняем полный относительный путь
            char rel_music[256];
            get_relative_path(path, rel_music, sizeof(rel_music));
            safe_strcpy(ed->music_fullpath, sizeof(ed->music_fullpath), rel_music);
            // Извлекаем имя файла и обрезаем до 12 символов с троеточием
            const char *bname = strrchr(rel_music, '/');
            if (!bname) bname = strrchr(rel_music, '\\');
            if (bname) bname++; else bname = rel_music;
            size_t blen = strlen(bname);
            if (blen > 12) {
                memcpy(ed->input_text, bname, 12);
                memcpy(ed->input_text + 12, "...", 3);
                ed->input_text[15] = '\0';
            } else {
                snprintf(ed->input_text, sizeof(ed->input_text), "%s", bname);
            }
            ed->dialog_active_field = 0;
        }
        return;
    }
}

    if (mx >= ok.x && mx < ok.x+ok.w && my >= ok.y && my < ok.y+ok.h) {
        switch (ed->dialog_type) {
            case DIALOG_CONFIRM_DEL:
                delete_current_map(ed);
                break;
            case DIALOG_NEW_MAP: {
                int w = 20, h = 15;
                sscanf(ed->input_text2, "%dx%d", &w, &h);
                char first_ts[512];
                find_first_tileset_path(first_ts, sizeof(first_ts));
                create_map(ed, ed->input_text, w, h, first_ts);
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
			
    case DIALOG_MUSIC_MAP: {
    Map *cur = current_map(ed);
    if (cur) {
        // Сохраняем полный путь из music_fullpath (он уже правильный)
        safe_strcpy(cur->music_file, sizeof(cur->music_file), ed->music_fullpath);
        cur->music_volume = (float)atof(ed->input_text2);
        char path[128];
        snprintf(path, sizeof(path), "../data/maps/%s.json", cur->name);
        map_save_to_json(cur, path);
    }
    break;
    }
}
        close_dialog(ed);
    } else if (mx >= cancel.x && mx < cancel.x+cancel.w && my >= cancel.y && my < cancel.y+cancel.h) {
        close_dialog(ed);
    }
}

void handle_input(Editor *ed, bool *running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            *running = false;
            return;
        }

        if (ed->dialog_active) {
            if (SDL_GetTicks() - ed->dialog_cursor_blink > 530) {
                ed->dialog_cursor_blink = SDL_GetTicks();
            }

            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_RETURN:
                        handle_dialog_click(ed, WINDOW_W/2, WINDOW_H/2);
                        break;
                    case SDLK_ESCAPE:
                        close_dialog(ed);
                        break;
                    case SDLK_TAB:
                        if (ed->dialog_type == DIALOG_NEW_MAP ||
                            ed->dialog_type == DIALOG_RESIZE_MAP ||
                            ed->dialog_type == DIALOG_MUSIC_MAP)
                            ed->dialog_active_field = !ed->dialog_active_field;
                        break;
                    case SDLK_BACKSPACE: {
                        if (ed->dialog_type == DIALOG_MUSIC_MAP && ed->dialog_active_field == 0) break;
                        char *str = (ed->dialog_active_field == 0) ? ed->input_text : ed->input_text2;
                        if (str && strlen(str) > 0)
                            str[strlen(str)-1] = '\0';
                        break;
                    }
                }
            } else if (e.type == SDL_TEXTINPUT) {
                if (ed->dialog_type == DIALOG_MUSIC_MAP && ed->dialog_active_field == 0) break;
                char *dest = (ed->dialog_active_field == 0) ? ed->input_text : ed->input_text2;
                int max_len = (ed->dialog_active_field == 1) ? 15 : 63;
                if (dest && strlen(dest) < max_len) {
                    strcat(dest, e.text.text);
                }
            } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                SDL_Rect dlg = { WINDOW_W/2 - 180, WINDOW_H/2 - 110, 360, 220 };
                if (ed->dialog_type == DIALOG_NEW_MAP ||
                    ed->dialog_type == DIALOG_RESIZE_MAP ||
                    ed->dialog_type == DIALOG_MUSIC_MAP)
                {
                    int y_base = dlg.y + 20;
                    SDL_Rect f1, f2;
                    if (ed->dialog_type == DIALOG_NEW_MAP) {
                        f1 = (SDL_Rect){ dlg.x + 110, y_base - 2, 200, 24 };
                        f2 = (SDL_Rect){ dlg.x + 110, y_base + 33, 200, 24 };
                    } else if (ed->dialog_type == DIALOG_RESIZE_MAP) {
                        f1 = (SDL_Rect){ dlg.x + 110, y_base - 2, 180, 24 };
                        f2 = (SDL_Rect){ dlg.x + 110, y_base + 33, 180, 24 };
                    } else {
                        f1 = (SDL_Rect){ dlg.x + 120, y_base - 2, 150, 24 };
                        f2 = (SDL_Rect){ dlg.x + 140, y_base + 28, 80, 24 };
                    }
                    if (e.button.x >= f1.x && e.button.x < f1.x+f1.w &&
                        e.button.y >= f1.y && e.button.y < f1.y+f1.h)
                        ed->dialog_active_field = 0;
                    else if (e.button.x >= f2.x && e.button.x < f2.x+f2.w &&
                             e.button.y >= f2.y && e.button.y < f2.y+f2.h)
                        ed->dialog_active_field = 1;
                    else
                        handle_dialog_click(ed, e.button.x, e.button.y);
                } else {
                    handle_dialog_click(ed, e.button.x, e.button.y);
                }
            }
            continue;
        }

        // Обычный режим редактора
        if (e.type == SDL_MOUSEWHEEL) {
            int mx, my;
            SDL_GetMouseState(&mx, &my);
            if (mx < LEFT_PANEL_W && my >= PALETTE_START_Y && ed->tileset_loaded) {
                ed->palette_scroll -= e.wheel.y;
                int max_scroll = ((ed->tile_count + PALETTE_COLS - 1) / PALETTE_COLS) -
                                 ((WINDOW_H - PALETTE_START_Y) / (PALETTE_TILE_SIZE + 2));
                if (ed->palette_scroll < 0) ed->palette_scroll = 0;
                if (max_scroll > 0 && ed->palette_scroll > max_scroll)
                    ed->palette_scroll = max_scroll;
            } else if (mx >= MAP_X && mx < MAP_X + MAP_W &&
                     my >= MAP_Y && my < MAP_Y + MAP_H) {
                if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LCTRL]) {
                    ed->zoom += e.wheel.y * 0.1f;
                    if (ed->zoom < 0.1f) ed->zoom = 0.1f;
                    if (ed->zoom > 2.0f) ed->zoom = 2.0f;
                }
            }
        }

        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT) {
            if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LCTRL]) {
                ed->panning = 1;
                ed->pan_start_x = e.button.x;
                ed->pan_start_y = e.button.y;
            }
        }
        if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_RIGHT) {
            ed->panning = 0;
        }
        if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
            ed->dialog_just_closed = false;
        }

        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT &&
            !(SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LCTRL]) &&
            ed->right_click_mode != 0)
        {
            int mx = e.button.x, my = e.button.y;
            if (mx >= MAP_X && mx < MAP_X + MAP_W &&
                my >= MAP_Y && my < MAP_Y + MAP_H)
            {
                float world_x = (mx - MAP_X) / ed->zoom + ed->cam_x;
                float world_y = (my - MAP_Y) / ed->zoom + ed->cam_y;
                int tx = world_x / TILE_SIZE, ty = world_y / TILE_SIZE;
                Map *map = current_map(ed);
                if (map && tx >= 0 && tx < map->width && ty >= 0 && ty < map->height)
                {
                    int idx = tx * map->height + ty;
                    int *rot = (ed->current_layer == 0) ? map->rot : map->rot2;
                    int *mirror_x = (ed->current_layer == 0) ? map->mirror_x : map->mirror_x2;
                    int *mirror_y = (ed->current_layer == 0) ? map->mirror_y : map->mirror_y2;

                    switch (ed->right_click_mode) {
                        case RIGHT_ROTATE: rot[idx] = (rot[idx] + 1) % 4; break;
                        case RIGHT_FLIP_H: mirror_x[idx] = !mirror_x[idx]; break;
                        case RIGHT_FLIP_V: mirror_y[idx] = !mirror_y[idx]; break;
                        case RIGHT_DELETE:
                            if (ed->current_layer == 0)
                                map->tiles[idx] = -1;
                            else
                                map->tiles2[idx] = -1;
                            break;
                    }
                }
            }
        }

        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT)
        {
            int mx = e.button.x, my = e.button.y;

            // Левая панель
            if (mx < LEFT_PANEL_W) {
                if (my >= 35 && my < 61) {
                    char path[256];
                    if (open_file_dialog(path, sizeof(path)) &&
                        load_tileset(ed, path))
                    {
                        Map *cur = current_map(ed);
                        if (cur)
                            get_relative_path(path, cur->tileset_path, sizeof(cur->tileset_path));
                    }
                }
                else if (my >= 70 && my < 94) {
                    int half = (LEFT_PANEL_W - 30) / 2;
                    if (mx >= 10 && mx < 10+half)
                        ed->mode = MODE_A;
                    else if (mx >= 10+half+10 && mx < 10+half+10+half)
                        ed->mode = MODE_C;
                }
                else if (ed->mode == MODE_C && my >= 100 && my < 124) {
                    int icon_y = 100, sz = 24, sp = 6;
                    int tw = 4*sz + 3*sp;
                    int sx = (LEFT_PANEL_W - tw) / 2;
                    for (int t = 0; t < 4; t++) {
                        SDL_Rect r = { sx + t*(sz+sp), icon_y, sz, sz };
                        if (mx >= r.x && mx < r.x+r.w &&
                            my >= r.y && my < r.y+r.h) {
                            ed->current_type = t;
                            break;
                        }
                    }
                }
                else if (ed->tileset_loaded && my >= PALETTE_START_Y &&
                         ed->mode == MODE_A)
                {
                    int rx = mx - PALETTE_START_X;
                    int ry = my - PALETTE_START_Y;
                    int step = PALETTE_TILE_SIZE + 2;
                    if (rx >= 0 && ry >= 0) {
                        int col = rx / step;
                        int row = ry / step + ed->palette_scroll;
                        if (col < PALETTE_COLS && (rx % step) < PALETTE_TILE_SIZE) {
                            int idx = row * PALETTE_COLS + col;
                            if (idx >= 0 && idx < ed->tile_count)
                                ed->selected_tile = idx;
                        }
                    }
                }
            }

            // Тулбар
            else if (my < TOOLBAR_H && mx >= MAP_X && mx < MAP_X + MAP_W) {
                // Координаты кнопок (должны совпадать с render_toolbar)
                int vis_btn_w = 30;
                int vis_btn_x1 = MAP_X + 180;
                int vis_btn_x2 = vis_btn_x1 + vis_btn_w + 4;
                int active_btn_x = vis_btn_x2 + vis_btn_w + 8;
                int btn_w = 38;

                if (mx >= MAP_X+5 && mx < MAP_X+43)
                    ed->right_click_mode = RIGHT_ROTATE;
                else if (mx >= MAP_X+45 && mx < MAP_X+83)
                    ed->right_click_mode = RIGHT_FLIP_H;
                else if (mx >= MAP_X+85 && mx < MAP_X+123)
                    ed->right_click_mode = RIGHT_FLIP_V;
                else if (mx >= MAP_X+125 && mx < MAP_X+163)
                    ed->right_click_mode = RIGHT_DELETE;
                else if (mx >= vis_btn_x1 && mx < vis_btn_x1+vis_btn_w)
                    ed->show_layer1 = !ed->show_layer1;
                else if (mx >= vis_btn_x2 && mx < vis_btn_x2+vis_btn_w)
                    ed->show_layer2 = !ed->show_layer2;
                else if (mx >= active_btn_x && mx < active_btn_x+btn_w)
                    ed->current_layer = !ed->current_layer;
                else
                    ed->right_click_mode = 0;
            }

            // Карта
            else if (mx >= MAP_X && mx < MAP_X+MAP_W &&
                     my >= MAP_Y && my < MAP_Y+MAP_H)
            {
                float world_x = (mx - MAP_X) / ed->zoom + ed->cam_x;
                float world_y = (my - MAP_Y) / ed->zoom + ed->cam_y;
                int tx = world_x / TILE_SIZE, ty = world_y / TILE_SIZE;
                Map *map = current_map(ed);
                if (map && tx >= 0 && tx < map->width && ty >= 0 && ty < map->height)
                {
                    int idx = tx * map->height + ty;
                    if (ed->current_layer == 0) {
                        map->tiles[idx]    = ed->selected_tile;
                        map->rot[idx]      = 0;
                        map->mirror_x[idx] = 0;
                        map->mirror_y[idx] = 0;
                    } else {
                        map->tiles2[idx]    = ed->selected_tile;
                        map->rot2[idx]      = 0;
                        map->mirror_x2[idx] = 0;
                        map->mirror_y2[idx] = 0;
                    }
                }
            }

            // Правая панель
            else if (mx >= WINDOW_W - RIGHT_PANEL_W) {
                int list_y = 35;

                for (int i = 0; i < ed->map_list.map_count; i++) {
                    if (my >= list_y + i*20 - 10 && my < list_y + i*20 + 10) {
                        ed->map_list.current_map = i;
                        Map *m = &ed->map_list.maps[i];
                        load_tileset(ed, m->tileset_path);
                        return;
                    }
                }

                int y = WINDOW_H - 180;
                if (my >= y && my < y+24) {
                    open_dialog(ed, DIALOG_NEW_MAP);
                    strcpy(ed->input_text, "map00");
                    strcpy(ed->input_text2, "20x15");
                }
                else if (my >= y+30 && my < y+54) {
                    if (current_map(ed)) {
                        char path[512];
                        snprintf(path, sizeof(path), "../data/maps/%s.json", current_map(ed)->name);
                        map_save_to_json(current_map(ed), path);
                        save_tile_types_for_tileset(ed, current_map(ed)->tileset_path);
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
                else if (my >= y+150 && my < y+174) {
                    open_dialog(ed, DIALOG_MUSIC_MAP);
                }
            }
        }
    }

    // Непрерывные действия
    if (ed->panning && !ed->dialog_active) {
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        float dx = (mx - ed->pan_start_x) / ed->zoom;
        float dy = (my - ed->pan_start_y) / ed->zoom;
        ed->cam_x -= dx;
        ed->cam_y -= dy;
        ed->pan_start_x = mx;
        ed->pan_start_y = my;

        Map *map = current_map(ed);
        if (map) {
            float max_x = map->width * TILE_SIZE - MAP_W / ed->zoom;
            float max_y = map->height * TILE_SIZE - MAP_H / ed->zoom;
            if (max_x < 0) max_x = 0;
            if (max_y < 0) max_y = 0;
            if (ed->cam_x < 0) ed->cam_x = 0;
            if (ed->cam_y < 0) ed->cam_y = 0;
            if (ed->cam_x > max_x) ed->cam_x = max_x;
            if (ed->cam_y > max_y) ed->cam_y = max_y;
        }
    }

    Uint32 mouse_state = SDL_GetMouseState(NULL, NULL);
    if (!ed->dialog_active && (mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT)) && !ed->panning && !ed->dialog_just_closed) {
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        if (mx >= MAP_X && mx < MAP_X + MAP_W && my >= MAP_Y && my < MAP_Y + MAP_H) {
            float world_x = (mx - MAP_X) / ed->zoom + ed->cam_x;
            float world_y = (my - MAP_Y) / ed->zoom + ed->cam_y;
            int tx = world_x / TILE_SIZE, ty = world_y / TILE_SIZE;
            Map *map = current_map(ed);
            if (map && tx >= 0 && tx < map->width && ty >= 0 && ty < map->height) {
                int idx = tx * map->height + ty;
                if (ed->mode == MODE_A) {
                    if (ed->current_layer == 0) {
                        map->tiles[idx]    = ed->selected_tile;
                        map->rot[idx]      = 0;
                        map->mirror_x[idx] = 0;
                        map->mirror_y[idx] = 0;
                    } else {
                        map->tiles2[idx]    = ed->selected_tile;
                        map->rot2[idx]      = 0;
                        map->mirror_x2[idx] = 0;
                        map->mirror_y2[idx] = 0;
                    }
                }
            }
        }
    }

    if (!ed->dialog_active && ed->mode == MODE_C &&
        ed->tileset_loaded && (mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT)))
    {
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        if (mx < LEFT_PANEL_W && my >= PALETTE_START_Y) {
            int rx = mx - PALETTE_START_X;
            int ry = my - PALETTE_START_Y;
            int step = PALETTE_TILE_SIZE + 2;
            if (rx >= 0 && ry >= 0) {
                int col = rx / step;
                int row = ry / step + ed->palette_scroll;
                if (col < PALETTE_COLS && (rx % step) < PALETTE_TILE_SIZE) {
                    int idx = row * PALETTE_COLS + col;
                    if (idx >= 0 && idx < ed->tile_count)
                        ed->tile_types[idx] = ed->current_type;
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

    ed.window = SDL_CreateWindow("Map Editor C SDL2",
                                 SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                 WINDOW_W, WINDOW_H, SDL_WINDOW_SHOWN);
    ed.renderer = SDL_CreateRenderer(ed.window, -1, SDL_RENDERER_ACCELERATED);

    ed.font = TTF_OpenFont("C:/Windows/Fonts/consola.ttf", FONT_SIZE);
    if (!ed.font)
        ed.font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", FONT_SIZE);
    if (!ed.font) {
        printf("No font!\n");
        return 1;
    }

    load_type_icons(&ed);
    load_transform_icons(&ed);

    CreateDirectoryA("../data/maps", NULL);
    load_map_list(&ed);

    if (ed.map_list.map_count == 0) {
        char first_ts[512];
        find_first_tileset_path(first_ts, sizeof(first_ts));
        create_map(&ed, "map00", 20, 15, first_ts);
    } else {
        Map *cur = current_map(&ed);
        if (cur)
            load_tileset(&ed, cur->tileset_path);
    }

    Uint32 last_blink = SDL_GetTicks();
    bool running = true;
    while (running) {
        handle_input(&ed, &running);

        Uint32 now = SDL_GetTicks();
        if (now - last_blink >= 500) {
            ed.blink_visible = !ed.blink_visible;
            last_blink = now;
        }
        if (ed.save_blink_active && now - ed.save_blink_time >= 150)
            ed.save_blink_active = false;

        SDL_SetRenderDrawColor(ed.renderer, 30, 30, 30, 255);
        SDL_RenderClear(ed.renderer);

        render_left_panel(&ed);
        render_toolbar(&ed);
        render_map(&ed);
        render_right_panel(&ed);
        draw_dialog(&ed);

        SDL_RenderPresent(ed.renderer);
        SDL_Delay(16);
    }

    if (current_map(&ed)) {
        char path[512];
        snprintf(path, sizeof(path), "../data/maps/%s.json", current_map(&ed)->name);
        map_save_to_json(current_map(&ed), path);
    }
    if (ed.tileset_loaded) {
        save_tile_types_for_tileset(&ed,
            current_map(&ed) ? current_map(&ed)->tileset_path : ed.tileset_path);
    }

    free_tileset(&ed);
    TTF_CloseFont(ed.font);
    for (int i = 0; i < 4; i++)
        if (ed.type_icons[i]) SDL_DestroyTexture(ed.type_icons[i]);
    for (int i = 0; i < 4; i++)
        if (ed.transform_icons[i]) SDL_DestroyTexture(ed.transform_icons[i]);
    for (int i = 0; i < ed.map_list.map_count; i++)
        map_free(&ed.map_list.maps[i]);
    free(ed.map_list.maps);

    SDL_DestroyRenderer(ed.renderer);
    SDL_DestroyWindow(ed.window);

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}