// actors_editor.c
#include "actors_editor.h"
#include <stdio.h>
#include <string.h>
#include <SDL_ttf.h>
#include <ctype.h>
#include <windows.h>
#include <commdlg.h>

// Внешние глобальные переменные (из DatabaseEditor.c)
extern cJSON *classes_json;       // массив классов
extern int classes_count;
extern cJSON *actors_json;        // массив актёров
extern int actors_count;
extern TTF_Font *g_font;
extern int g_font_ok;
extern char error_msg[256];
int draw_text_ext(SDL_Renderer *r, int x, int y, const char *text, SDL_Color color);
char* open_file_dialog();

// ---------- ЛОКАЛЬНЫЕ ДАННЫЕ МОДУЛЯ ----------
static cJSON *actors = NULL;
static int actor_count = 0;
static int selected_actor = -1;

// поля редактирования
static char  name_buf[11] = {0};       // до 10 символов
static int   class_id_val = 0;
static char  portrait_buf[64] = {0};
static char  mapsprite_buf[64] = {0};
static int   level_val = 1;

// вспомогательные списки классов
typedef struct {
    int id;
    char name[64];
} ClassEntry;
static ClassEntry class_list[200];
static int class_list_count = 0;
static int selected_class_idx = -1;   // индекс в class_list

// кнопки
static int save_timer = 0;
#define SAVE_BLINK_DURATION 42
static int actor_scroll = 0;
static int g_window_height = 680;   // высота окна по умолчанию

// поля ввода (EditField)
#define MAX_ACTOR_FIELDS 10
typedef struct {
    char text[256];
    int cursor;
    int active;
    cJSON *json_obj;
    const char *json_key;
    int is_numeric;
    int max_len;
    int array_index;
    SDL_Rect rect;
    int is_special; // 0-обычное, 1-class, 2-portrait, 3-mapsprite
} ActorField;
static ActorField actor_fields[MAX_ACTOR_FIELDS];
static int actor_field_count = 0;
static int actor_active_field = -1;

// Прототипы локальных функций
static void build_class_list(void);
static void commit_actor_changes(void);
static void load_actor_fields(void);
static void open_actor_fields(void);
static void commit_actor_field(int idx);
static int  handle_actor_input(SDL_Event *evt, int idx);
static void draw_actor_field(SDL_Renderer *r, int x, int y, int w, int h, int idx, const char *label, const char *fallback_text);
static void add_new_actor(void);
static void delete_actor(void);

// ---------- РЕАЛИЗАЦИИ ----------

static void build_class_list(void) {
    class_list_count = 0;
    for (int i = 0; i < classes_count; i++) {
        cJSON *cls = cJSON_GetArrayItem(classes_json, i);
        int id = cJSON_GetObjectItem(cls, "id")->valueint;
        const char *name = cJSON_GetObjectItem(cls, "name")->valuestring;
        if (class_list_count < 200) {
            class_list[class_list_count].id = id;
            strncpy(class_list[class_list_count].name, name, 63);
            class_list[class_list_count].name[63] = '\0';
            class_list_count++;
        }
    }
}

static void commit_actor_changes(void) {
    if (selected_actor < 0) return;
    cJSON *act = cJSON_GetArrayItem(actors, selected_actor);
    cJSON *nm = cJSON_GetObjectItem(act, "name");
    if (nm && cJSON_IsString(nm)) cJSON_SetValuestring(nm, name_buf);
    cJSON *cid = cJSON_GetObjectItem(act, "class_id");
    if (cid && cJSON_IsNumber(cid)) { cid->valueint = class_id_val; cid->valuedouble = class_id_val; }
    cJSON *prt = cJSON_GetObjectItem(act, "portrait");
    if (prt && cJSON_IsString(prt)) cJSON_SetValuestring(prt, portrait_buf);
    cJSON *ms = cJSON_GetObjectItem(act, "mapsprite");
    if (ms && cJSON_IsString(ms)) cJSON_SetValuestring(ms, mapsprite_buf);
    cJSON *lv = cJSON_GetObjectItem(act, "level");
    if (lv && cJSON_IsNumber(lv)) { lv->valueint = level_val; lv->valuedouble = level_val; }
}

static void load_actor_fields(void) {
    if (selected_actor < 0) return;
    cJSON *act = cJSON_GetArrayItem(actors, selected_actor);
    const char *s = cJSON_GetObjectItem(act, "name")->valuestring;
    strncpy(name_buf, s ? s : "", 10); name_buf[10] = '\0';
    class_id_val = cJSON_GetObjectItem(act, "class_id")->valueint;
    const char *p = cJSON_GetObjectItem(act, "portrait")->valuestring;
    strncpy(portrait_buf, p ? p : "", 63); portrait_buf[63] = '\0';
    const char *m = cJSON_GetObjectItem(act, "mapsprite")->valuestring;
    strncpy(mapsprite_buf, m ? m : "", 63); mapsprite_buf[63] = '\0';
    level_val = cJSON_GetObjectItem(act, "level")->valueint;

    // синхронизация индекса класса
    selected_class_idx = -1;
    for (int i = 0; i < class_list_count; i++) {
        if (class_list[i].id == class_id_val) { selected_class_idx = i; break; }
    }
    open_actor_fields();
}

static void open_actor_fields(void) {
    actor_field_count = 0;
    actor_active_field = -1;
    int base_x = 360, base_y = 80, off = 100;

    // 0 Name
    snprintf(actor_fields[0].text, sizeof(actor_fields[0].text), "%s", name_buf);
    actor_fields[0].cursor = strlen(actor_fields[0].text);
    actor_fields[0].active = 0;
    actor_fields[0].json_obj = cJSON_GetArrayItem(actors, selected_actor);
    actor_fields[0].json_key = "name";
    actor_fields[0].is_numeric = 0; actor_fields[0].max_len = 10; actor_fields[0].array_index = -1; actor_fields[0].is_special = 0;
    actor_fields[0].rect = (SDL_Rect){base_x+off, base_y + 0*35, 150, 22};

    // 1 Class (специальное)
    snprintf(actor_fields[1].text, sizeof(actor_fields[1].text), "%d", class_id_val);
    actor_fields[1].cursor = 0;
    actor_fields[1].active = 0;
    actor_fields[1].json_obj = NULL; actor_fields[1].json_key = "class_id";
    actor_fields[1].is_numeric = 1; actor_fields[1].max_len = 0; actor_fields[1].array_index = -1;
    actor_fields[1].is_special = 1;
    actor_fields[1].rect = (SDL_Rect){base_x+off, base_y + 1*35, 150, 22};

    // 2 Level
    snprintf(actor_fields[2].text, sizeof(actor_fields[2].text), "%d", level_val);
    actor_fields[2].cursor = strlen(actor_fields[2].text);
    actor_fields[2].active = 0;
    actor_fields[2].json_obj = NULL; actor_fields[2].json_key = "level";
    actor_fields[2].is_numeric = 1; actor_fields[2].max_len = 4; actor_fields[2].array_index = -1; actor_fields[2].is_special = 0;
    actor_fields[2].rect = (SDL_Rect){base_x+off, base_y + 2*35, 150, 22};

    // 3 Portrait (специальное)
    snprintf(actor_fields[3].text, sizeof(actor_fields[3].text), "%s", portrait_buf);
    actor_fields[3].cursor = strlen(actor_fields[3].text);
    actor_fields[3].active = 0;
    actor_fields[3].json_obj = NULL; actor_fields[3].json_key = "portrait";
    actor_fields[3].is_numeric = 0; actor_fields[3].max_len = 0; actor_fields[3].array_index = -1;
    actor_fields[3].is_special = 2;
    actor_fields[3].rect = (SDL_Rect){base_x+off, base_y + 3*35, 150, 22};

    // 4 Mapsprite (специальное)
    snprintf(actor_fields[4].text, sizeof(actor_fields[4].text), "%s", mapsprite_buf);
    actor_fields[4].cursor = strlen(actor_fields[4].text);
    actor_fields[4].active = 0;
    actor_fields[4].json_obj = NULL; actor_fields[4].json_key = "mapsprite";
    actor_fields[4].is_numeric = 0; actor_fields[4].max_len = 0; actor_fields[4].array_index = -1;
    actor_fields[4].is_special = 3;
    actor_fields[4].rect = (SDL_Rect){base_x+off, base_y + 4*35, 150, 22};

    actor_field_count = 5;
}

static void commit_actor_field(int idx) {
    if (idx < 0 || idx >= actor_field_count) return;
    ActorField *f = &actor_fields[idx];
    if (f->is_special == 1) { // class
        class_id_val = atoi(f->text);
        selected_class_idx = -1;
        for (int i = 0; i < class_list_count; i++)
            if (class_list[i].id == class_id_val) { selected_class_idx = i; break; }
    } else if (f->is_special == 2) { // portrait
        strncpy(portrait_buf, f->text, 63); portrait_buf[63] = '\0';
    } else if (f->is_special == 3) { // mapsprite
        strncpy(mapsprite_buf, f->text, 63); mapsprite_buf[63] = '\0';
    } else {
        if (f->json_obj && f->json_key) {
            if (f->is_numeric) {
                int v = atoi(f->text);
                cJSON *num = cJSON_GetObjectItem(f->json_obj, f->json_key);
                if (num && cJSON_IsNumber(num)) { num->valueint = v; num->valuedouble = v; }
            } else {
                cJSON *str = cJSON_GetObjectItem(f->json_obj, f->json_key);
                if (str && cJSON_IsString(str)) cJSON_SetValuestring(str, f->text);
            }
        }
    }
    if (idx == 0) { strncpy(name_buf, f->text, 10); name_buf[10] = '\0'; }
    else if (idx == 2) level_val = atoi(f->text);
    f->active = 0;
}

static int handle_actor_input(SDL_Event *evt, int idx) {
    ActorField *f = &actor_fields[idx];
    SDL_Rect r = f->rect;
    if (evt->type == SDL_MOUSEBUTTONDOWN && evt->button.button == SDL_BUTTON_LEFT) {
        int mx = evt->button.x, my = evt->button.y;
        if (mx >= r.x && mx < r.x+r.w && my >= r.y && my < r.y+r.h) {
            if (actor_active_field >= 0 && actor_active_field != idx) commit_actor_field(actor_active_field);
            actor_active_field = idx;
            f->active = 1;
            f->cursor = strlen(f->text);
            return 1;
        }
        if (f->active) {
            commit_actor_field(idx);
            int best = -1, bestDist = 1000;
            for (int i = 0; i < actor_field_count; i++) {
                if (i == 3 || i == 4) continue; // пропускаем портрет и спрайт
                SDL_Rect fr = actor_fields[i].rect;
                int cy = fr.y + fr.h/2;
                int dist = abs(my - cy);
                if (dist < bestDist && mx >= fr.x - 20 && mx < fr.x + fr.w + 20) { bestDist = dist; best = i; }
            }
            if (best >= 0) {
                actor_fields[best].active = 1;
                actor_fields[best].cursor = strlen(actor_fields[best].text);
                actor_active_field = best;
            } else actor_active_field = -1;
            return 1;
        } else {
            int best = -1, bestDist = 1000;
            for (int i = 0; i < actor_field_count; i++) {
                if (i == 3 || i == 4) continue;
                SDL_Rect fr = actor_fields[i].rect;
                int cy = fr.y + fr.h/2;
                int dist = abs(my - cy);
                if (dist < bestDist && mx >= fr.x - 20 && mx < fr.x + fr.w + 20) { bestDist = dist; best = i; }
            }
            if (best >= 0) {
                if (actor_active_field >= 0) commit_actor_field(actor_active_field);
                actor_fields[best].active = 1;
                actor_fields[best].cursor = strlen(actor_fields[best].text);
                actor_active_field = best;
                return 1;
            }
        }
    } else if (evt->type == SDL_KEYDOWN && f->active && f->is_special == 0) {
        if (evt->key.keysym.sym == SDLK_UP) {
            if (idx == 0) return 1;
            commit_actor_field(idx);
            int new_idx = idx - 1;
            actor_fields[new_idx].active = 1;
            actor_fields[new_idx].cursor = strlen(actor_fields[new_idx].text);
            actor_active_field = new_idx;
            return 1;
        } else if (evt->key.keysym.sym == SDLK_DOWN) {
            if (idx >= 2) return 1; // останавливаемся на Level (индекс 2)
            commit_actor_field(idx);
            int new_idx = idx + 1;
            actor_fields[new_idx].active = 1;
            actor_fields[new_idx].cursor = strlen(actor_fields[new_idx].text);
            actor_active_field = new_idx;
            return 1;
        } else if (evt->key.keysym.sym == SDLK_BACKSPACE) {
            if (f->cursor > 0) {
                memmove(f->text + f->cursor - 1, f->text + f->cursor, strlen(f->text) - f->cursor + 1);
                f->cursor--;
            }
            return 1;
        } else if (evt->key.keysym.sym == SDLK_RETURN || evt->key.keysym.sym == SDLK_KP_ENTER) {
            commit_actor_field(idx);
            return 1;
        } else if (evt->key.keysym.sym == SDLK_LEFT && f->cursor > 0) { f->cursor--; return 1; }
        else if (evt->key.keysym.sym == SDLK_RIGHT && f->cursor < strlen(f->text)) { f->cursor++; return 1; }
    } else if (evt->type == SDL_TEXTINPUT && f->active && f->is_special == 0) {
        char ch = evt->text.text[0];
        if (f->is_numeric) {
            if (isdigit(ch) || (ch == '-' && f->cursor == 0 && f->text[0] == '\0')) {
                if (f->max_len > 0 && strlen(f->text) >= f->max_len) return 1;
                if (strlen(f->text) < 255) {
                    memmove(f->text + f->cursor + 1, f->text + f->cursor, strlen(f->text) - f->cursor + 1);
                    f->text[f->cursor++] = ch;
                }
            }
        } else {
            if (ch >= 32 && ch <= 126 && strlen(f->text) < 255) {
                if (f->max_len > 0 && strlen(f->text) >= f->max_len) return 1;
                memmove(f->text + f->cursor + 1, f->text + f->cursor, strlen(f->text) - f->cursor + 1);
                f->text[f->cursor++] = ch;
            }
        }
        return 1;
    }
    return 0;
}

static void draw_actor_field(SDL_Renderer *r, int x, int y, int w, int h, int idx, const char *label, const char *fallback_text) {
    SDL_Color white = {255,255,255}, black = {0,0,0}, gray = {100,100,100};
    draw_text_ext(r, x, y + 3, label, white);
    int fx = x + 100;
    SDL_Rect rect = {fx, y, w, h};
    SDL_SetRenderDrawColor(r, gray.r, gray.g, gray.b, 255); SDL_RenderFillRect(r, &rect);
    SDL_SetRenderDrawColor(r, white.r, white.g, white.b, 255); SDL_RenderDrawRect(r, &rect);

    const char *text_to_draw = fallback_text;
    if (idx >= 0 && idx < actor_field_count && actor_fields[idx].active)
        text_to_draw = actor_fields[idx].text;

    draw_text_ext(r, fx + 5, y + 3, text_to_draw, black);

    if (idx >= 0 && idx < actor_field_count && actor_fields[idx].active) {
        ActorField *f = &actor_fields[idx];
        char before[256] = {0};
        if (f->cursor > 0) {
            strncpy(before, f->text, f->cursor);
            before[f->cursor] = '\0';
        }
        SDL_Surface *s = TTF_RenderUTF8_Solid(g_font, before, black);
        int offset = fx + 5 + (s ? s->w : 0);
        if (s) SDL_FreeSurface(s);
        draw_text_ext(r, offset, y + 3, "|", black);
    }
}

static void browse_asset(char *buffer, int buf_size) {
    char *path = open_file_dialog();
    if (!path) return;
    const char *fname = strrchr(path, '\\');
    if (!fname) fname = path; else fname++;
    // удаляем расширение .png
    char tmp[128];
    strncpy(tmp, fname, 127); tmp[127] = '\0';
    char *dot = strrchr(tmp, '.');
    if (dot) *dot = '\0';
    strncpy(buffer, tmp, buf_size - 1);
    buffer[buf_size - 1] = '\0';
    free(path);
}

static void add_new_actor(void) {
    if (!actors) return;
    int new_id = 0;
    for (int i = 0; i < actor_count; i++) {
        int id = cJSON_GetObjectItem(cJSON_GetArrayItem(actors, i), "id")->valueint;
        if (id >= new_id) new_id = id + 1;
    }
    char new_name[64]; int idx = 0;
    while (1) {
        snprintf(new_name, sizeof(new_name), "Actor%02d", idx);
        int found = 0;
        for (int i = 0; i < actor_count; i++)
            if (strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(actors, i), "name")->valuestring, new_name) == 0) found = 1;
        if (!found) break;
        idx++;
    }
    cJSON *act = cJSON_CreateObject();
    cJSON_AddNumberToObject(act, "id", new_id);
    cJSON_AddStringToObject(act, "name", new_name);
    cJSON_AddNumberToObject(act, "class_id", 0);
    cJSON_AddStringToObject(act, "portrait", "Mushra");
    cJSON_AddStringToObject(act, "mapsprite", "mapsprite000");
    cJSON_AddNumberToObject(act, "level", 1);
    cJSON_AddNumberToObject(act, "exp", 0);
    cJSON_AddNumberToObject(act, "kills", 0);
    cJSON_AddNumberToObject(act, "defeats", 0);
    cJSON_AddItemToArray(actors, act);
    actor_count++;
    if (selected_actor >= 0) commit_actor_changes();
    selected_actor = actor_count - 1;
    load_actor_fields();
}

static void delete_actor(void) {
    if (selected_actor < 0 || !actors) return;
    cJSON *new_arr = cJSON_CreateArray();
    for (int i = 0; i < actor_count; i++) {
        if (i == selected_actor) continue;
        cJSON_AddItemToArray(new_arr, cJSON_Duplicate(cJSON_GetArrayItem(actors, i), 1));
    }
    cJSON_Delete(actors);
    actors = new_arr;
    actor_count--;
    if (actor_count == 0) {
        selected_actor = -1;
        actor_active_field = -1;
        actor_field_count = 0;
    } else {
        if (selected_actor >= actor_count) selected_actor = actor_count - 1;
        if (selected_actor >= 0) load_actor_fields();
    }
}

void actors_reload(void) {
    if (actors) { cJSON_Delete(actors); actors = NULL; }
    FILE *f = fopen("../data/actors/actors.json", "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(len+1); fread(buf, 1, len, f); buf[len] = '\0'; fclose(f);
    cJSON *root = cJSON_Parse(buf); free(buf);
    if (root) {
        cJSON *arr = cJSON_GetObjectItem(root, "actors");
        if (arr && cJSON_IsArray(arr)) {
            actors_json = arr;
            actors_count = cJSON_GetArraySize(arr);
            actors = actors_json;
            actor_count = actors_count;
            selected_actor = -1;
            actor_scroll = 0;
            save_timer = 0;
            build_class_list();
            actor_field_count = 0;
            actor_active_field = -1;
            if (actor_count > 0) { selected_actor = 0; load_actor_fields(); }
        } else cJSON_Delete(root);
    }
}

// ---------- ПУБЛИЧНЫЕ ФУНКЦИИ ----------
void actors_init(cJSON *json_array, int count) {
    actors = json_array;
    actor_count = count;
    selected_actor = -1;
    actor_scroll = 0;
    save_timer = 0;
    build_class_list();
    actor_field_count = 0;
    actor_active_field = -1;
    if (actor_count > 0) { selected_actor = 0; load_actor_fields(); }
}

void actors_save_to_file(void) {
    if (!actors) return;
    cJSON *root = cJSON_CreateObject();
    cJSON *dup = cJSON_Duplicate(actors, 1);
    cJSON_AddItemToObject(root, "actors", dup);
    char *js = cJSON_PrintBuffered(root, 0, 1);
    FILE *f = fopen("../data/actors/actors.json", "w");
    if (f) { fputs(js, f); fclose(f); error_msg[0] = '\0'; }
    else snprintf(error_msg, sizeof(error_msg), "Failed to save actors.json");
    free(js); cJSON_Delete(root);
    save_timer = SAVE_BLINK_DURATION;
}

int actors_is_edit_active(void) { return actor_active_field >= 0; }

void actors_update_timer(void) {
    if (save_timer > 0) save_timer--;
}

void actors_reset_selection(void) {
    if (actor_count > 0) { selected_actor = 0; load_actor_fields(); }
    else { selected_actor = -1; actor_active_field = -1; actor_field_count = 0; }
}

void actors_adjust_scroll(int delta) {
    actor_scroll -= delta;
    // Не выше первого элемента
    if (actor_scroll < 0) actor_scroll = 0;
    // Не ниже последнего элемента
    int total_h = actors_get_total_height();
    int visible_h = g_window_height - 35; // 35 = tab_h + 5 (высота вкладок и отступ)
    int max_scroll = total_h > visible_h ? total_h - visible_h : 0;
    if (actor_scroll > max_scroll) actor_scroll = max_scroll;
}
int actors_get_scroll(void) { return actor_scroll; }

void actors_draw_list(SDL_Renderer *renderer, int y_offset, int scroll) {
    if (!actors) return;
    int y = y_offset - scroll;
    SDL_Color white = {255,255,255};
    for (int i = 0; i < actor_count; i++) {
        if (y + 20 < y_offset || y > y_offset + 600) { y += 20; continue; }
        cJSON *act = cJSON_GetArrayItem(actors, i);
        int id = cJSON_GetObjectItem(act, "id")->valueint;
        const char *name = cJSON_GetObjectItem(act, "name")->valuestring;
        int cid = cJSON_GetObjectItem(act, "class_id")->valueint;
        // Найдём название класса
        const char *cname = "???";
        for (int k = 0; k < class_list_count; k++) {
            if (class_list[k].id == cid) { cname = class_list[k].name; break; }
        }
        char buf[256];
        snprintf(buf, sizeof(buf), "%d: %s (Class %d: %s)", id, name, cid, cname);
        SDL_Rect rr = {10, y, 290, 20};
        if (i == selected_actor) { SDL_SetRenderDrawColor(renderer, 100,100,255,128); SDL_RenderFillRect(renderer, &rr); }
        draw_text_ext(renderer, 10, y, buf, white);
        y += 20;
    }
}

int actors_get_total_height(void) { return actor_count * 20; }

void actors_handle_click(int mx, int my, int y_offset, int scroll) {
    int y = y_offset - scroll;
    for (int i = 0; i < actor_count; i++) {
        if (mx >= 10 && mx < 300 && my >= y && my < y+20) {
            if (selected_actor != i) {
                if (selected_actor >= 0) commit_actor_changes();
                selected_actor = i;
                load_actor_fields();
            }
            return;
        }
        y += 20;
    }
}

void actors_draw_edit_panel(SDL_Renderer *renderer, int px, int py) {
    if (selected_actor < 0) return;
    SDL_Color black = {0,0,0,255}, white = {255,255,255}, blue = {70,70,120};
    SDL_Rect panel = {px, py, 580, 500};
    SDL_SetRenderDrawColor(renderer, 60,60,60,255); SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &panel);

    int y = py + 10;
    // Name
    draw_actor_field(renderer, px+10, y, 150, 22, 0, "Name:", name_buf);
    // покажем id актёра рядом
    cJSON *act = cJSON_GetArrayItem(actors, selected_actor);
    int actor_id = cJSON_GetObjectItem(act, "id")->valueint;
    char idstr[16]; snprintf(idstr, sizeof(idstr), "ID:%d", actor_id);
    draw_text_ext(renderer, px+270, y+3, idstr, white);
    y += 35;

    // Class (с кнопками < >)
    draw_text_ext(renderer, px+10, y+3, "Class:", white);
    const char *cname = "???";
    int cid_disp = class_id_val;
    for (int i = 0; i < class_list_count; i++) {
        if (class_list[i].id == class_id_val) { cname = class_list[i].name; break; }
    }
    char class_label[128]; snprintf(class_label, sizeof(class_label), "%d: %s", cid_disp, cname);
    draw_text_ext(renderer, px+110, y+3, class_label, white);
    SDL_Rect prev_c = {px+270, y, 20, 22}; SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, 255); SDL_RenderFillRect(renderer, &prev_c);
    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &prev_c);
    draw_text_ext(renderer, px+275, y+3, "<", white);
    SDL_Rect next_c = {px+295, y, 20, 22}; SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, 255); SDL_RenderFillRect(renderer, &next_c);
    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &next_c);
    draw_text_ext(renderer, px+300, y+3, ">", white);
    y += 35;

    // Level
    char lvlstr[8]; snprintf(lvlstr, sizeof(lvlstr), "%d", level_val);
    draw_actor_field(renderer, px+10, y, 150, 22, 2, "Level:", lvlstr);
    y += 35;

    // Portrait
    draw_text_ext(renderer, px+10, y+3, "Portrait:", white);
    draw_text_ext(renderer, px+110, y+3, portrait_buf, white);
    SDL_Rect brow1 = {px+10+100+150+5, y, 70, 22};
    SDL_SetRenderDrawColor(renderer, 100,100,200,255); SDL_RenderFillRect(renderer, &brow1);
    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &brow1);
    draw_text_ext(renderer, brow1.x+5, brow1.y+3, "Browse", white);
    y += 35;

    // Mapsprite
    draw_text_ext(renderer, px+10, y+3, "Mapsprite:", white);
    draw_text_ext(renderer, px+110, y+3, mapsprite_buf, white);
    SDL_Rect brow2 = {px+10+100+150+5, y, 70, 22};
    SDL_SetRenderDrawColor(renderer, 100,100,200,255); SDL_RenderFillRect(renderer, &brow2);
    SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &brow2);
    draw_text_ext(renderer, brow2.x+5, brow2.y+3, "Browse", white);
    y += 40;

    // Кнопки: Del Actor | SAVE | Refresh | Add Actor
    int btn_y = y + 5;
    SDL_Rect del_btn = {px+10, btn_y, 85, 30};
    SDL_SetRenderDrawColor(renderer, 200,80,80,255); SDL_RenderFillRect(renderer, &del_btn);
    SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &del_btn);
    draw_text_ext(renderer, del_btn.x+10, del_btn.y+5, "Del Actor", white);

    SDL_Rect save_btn = {px+100, btn_y, 70, 30};
    SDL_Color save_col = (save_timer > 0) ? (SDL_Color){0,255,0,255} : (SDL_Color){255,255,0,255};
    SDL_SetRenderDrawColor(renderer, save_col.r, save_col.g, save_col.b, 255); SDL_RenderFillRect(renderer, &save_btn);
    SDL_SetRenderDrawColor(renderer, 0,0,0,255); SDL_RenderDrawRect(renderer, &save_btn);
    draw_text_ext(renderer, save_btn.x+12, save_btn.y+5, "SAVE", black);

    SDL_Rect refresh_btn = {px+175, btn_y, 80, 30};
    SDL_SetRenderDrawColor(renderer, 180,180,255,255); SDL_RenderFillRect(renderer, &refresh_btn);
    SDL_SetRenderDrawColor(renderer, 0,0,0,255); SDL_RenderDrawRect(renderer, &refresh_btn);
    draw_text_ext(renderer, refresh_btn.x+12, refresh_btn.y+5, "Refresh", black);

    SDL_Rect add_btn = {px+260, btn_y, 95, 30};
    SDL_SetRenderDrawColor(renderer, 100,200,100,255); SDL_RenderFillRect(renderer, &add_btn);
    SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &add_btn);
    draw_text_ext(renderer, add_btn.x+5, add_btn.y+5, "Add Actor", white);
}

void actors_handle_input(SDL_Event *evt) {
    for (int i = 0; i < actor_field_count; i++) {
        if (handle_actor_input(evt, i)) break;
    }
    if (actor_active_field >= 0 && (evt->type == SDL_KEYDOWN || evt->type == SDL_TEXTINPUT))
        return;

    if (evt->type == SDL_MOUSEBUTTONDOWN && evt->button.button == SDL_BUTTON_LEFT) {
        int mx = evt->button.x, my = evt->button.y;
        if (selected_actor < 0) return;
        int px = 360, py = 50;

        // Browse Portrait
        SDL_Rect brow1 = {px+10+100+150+5, py+10 + 3*35, 70, 22};
        if (mx >= brow1.x && mx < brow1.x+brow1.w && my >= brow1.y && my < brow1.y+brow1.h) {
            browse_asset(portrait_buf, sizeof(portrait_buf));
            commit_actor_changes();
            return;
        }
        // Browse Mapsprite
        SDL_Rect brow2 = {px+10+100+150+5, py+10 + 4*35, 70, 22};
        if (mx >= brow2.x && mx < brow2.x+brow2.w && my >= brow2.y && my < brow2.y+brow2.h) {
            browse_asset(mapsprite_buf, sizeof(mapsprite_buf));
            commit_actor_changes();
            return;
        }
        // Кнопки нижнего ряда
        int btn_y = py + 10 + 5*35 + 5;
        SDL_Rect del_btn = {px+10, btn_y, 85, 30};
        SDL_Rect save_btn = {px+100, btn_y, 70, 30};
        SDL_Rect refresh_btn = {px+175, btn_y, 80, 30};
        SDL_Rect add_btn = {px+260, btn_y, 95, 30};
        if (mx >= del_btn.x && mx < del_btn.x+del_btn.w && my >= del_btn.y && my < del_btn.y+del_btn.h) {
            delete_actor(); return;
        }
        if (mx >= save_btn.x && mx < save_btn.x+save_btn.w && my >= save_btn.y && my < save_btn.y+save_btn.h) {
            actors_save_to_file(); return;
        }
        if (mx >= refresh_btn.x && mx < refresh_btn.x+refresh_btn.w && my >= refresh_btn.y && my < refresh_btn.y+refresh_btn.h) {
            actors_reload(); return;
        }
        if (mx >= add_btn.x && mx < add_btn.x+add_btn.w && my >= add_btn.y && my < add_btn.y+add_btn.h) {
            add_new_actor(); return;
        }
        // Кнопки Class (< >)
        SDL_Rect prev_c = {px+270, py+10 + 1*35, 20, 22};
        SDL_Rect next_c = {px+295, py+10 + 1*35, 20, 22};
        if (mx >= prev_c.x && mx < prev_c.x+prev_c.w && my >= prev_c.y && my < prev_c.y+prev_c.h) {
            if (class_list_count > 0 && selected_class_idx > 0) {
                selected_class_idx--;
                class_id_val = class_list[selected_class_idx].id;
                commit_actor_changes();
            }
            return;
        }
        if (mx >= next_c.x && mx < next_c.x+next_c.w && my >= next_c.y && my < next_c.y+next_c.h) {
            if (class_list_count > 0 && selected_class_idx < class_list_count-1) {
                selected_class_idx++;
                class_id_val = class_list[selected_class_idx].id;
                commit_actor_changes();
            }
            return;
        }
    }
}

void actors_set_window_height(int h) {
    g_window_height = h;
}