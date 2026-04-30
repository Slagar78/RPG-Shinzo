// classes_editor.c
#include "classes_editor.h"
#include <stdio.h>
#include <string.h>
#include <SDL_ttf.h>
#include <ctype.h>
#include <windows.h>
#include <commdlg.h>

extern cJSON *spells_json;
extern int spells_count;
extern cJSON *classes_json;
extern int classes_count;
extern TTF_Font *g_font;
extern int g_font_ok;
extern char error_msg[256];
int draw_text_ext(SDL_Renderer *r, int x, int y, const char *text, SDL_Color color);
char* open_file_dialog();

static cJSON *classes = NULL;
static int class_count = 0;
static int selected_class = -1;

static char  name_buf[11] = {0};
static char  full_name_buf[17] = {0};
static int   move_val = 5;
static char  move_type_buf[32] = {"REGULAR"};

typedef struct {
    int start;
    int projected;
    char curve[64];
} GrowthBlock;
static GrowthBlock growth[5];
static const char *growth_names[] = {"HP", "MP", "Attack", "Defense", "Agility"};

#define MAX_SPELLS_PER_CLASS 12
typedef struct {
    int level;
    char spell_name[64];
    int spell_level;
} ClassSpellEntry;
static ClassSpellEntry class_spells[MAX_SPELLS_PER_CLASS];
static int class_spell_count = 0;
static int selected_spell_entry = -1;

static char curve_names[10][64];
static int curve_count = 0;
static int selected_curve_idx[5];

static char spell_names[100][64];
static int spell_name_count = 0;

static int save_timer = 0;
#define SAVE_BLINK_DURATION 90
static int class_scroll = 0;

#define MAX_CLASS_FIELDS 20
typedef struct {
    char text[256];
    int cursor;
    int active;
    int is_numeric;
    int max_len;
    SDL_Rect rect;
    int field_id;
} ClassField;
static ClassField class_fields[MAX_CLASS_FIELDS];
static int class_field_count = 0;
static int class_active_field = -1;

static int last_btn_y = 0;   // для синхронизации с обработчиком

// Прототипы
static void build_curve_list(void);
static void build_spell_name_list(void);
static void commit_class_changes(void);
static void load_class_fields(void);
static void open_class_fields(void);
static void commit_class_field(int idx);
static int  handle_class_input(SDL_Event *evt, int idx);
static void draw_class_field(SDL_Renderer *r, int x, int y, int w, int h, int idx, const char *label, const char *text);
static void add_new_class(void);
static void delete_class(void);

// ----------------------------------------------------------------
static void build_curve_list(void) {
    curve_count = 0;
    extern cJSON *curves_json;
    if (curves_json) {
        for (int i = 0; i < cJSON_GetArraySize(curves_json); i++) {
            cJSON *c = cJSON_GetArrayItem(curves_json, i);
            const char *id = cJSON_GetObjectItem(c, "id")->valuestring;
            if (curve_count < 10) {
                strncpy(curve_names[curve_count], id, 63);
                curve_names[curve_count][63] = '\0';
                curve_count++;
            }
        }
    }
    if (curve_count == 0) {
        static const char *hard[] = {"LINEAR","LATE","EARLY","MIDDLE","EARLYANDLATE","VERY_LATE","MIDDLE_PEAK"};
        for (int i = 0; i < 7; i++) {
            strncpy(curve_names[i], hard[i], 63);
            curve_names[i][63] = '\0';
        }
        curve_count = 7;
    }
    for (int i = 0; i < 5; i++) selected_curve_idx[i] = 0;
}

static void build_spell_name_list(void) {
    spell_name_count = 0;
    for (int i = 0; i < spells_count; i++) {
        cJSON *sp = cJSON_GetArrayItem(spells_json, i);
        const char *nm = cJSON_GetObjectItem(sp, "name")->valuestring;
        int exists = 0;
        for (int j = 0; j < spell_name_count; j++)
            if (strcmp(spell_names[j], nm) == 0) { exists = 1; break; }
        if (!exists && spell_name_count < 100) {
            strncpy(spell_names[spell_name_count], nm, 63);
            spell_names[spell_name_count][63] = '\0';
            spell_name_count++;
        }
    }
}

static void commit_class_changes(void) {
    if (selected_class < 0) return;
    cJSON *cls = cJSON_GetArrayItem(classes, selected_class);
    cJSON *nm = cJSON_GetObjectItem(cls, "name");
    if (nm) cJSON_SetValuestring(nm, name_buf);
    cJSON *fn = cJSON_GetObjectItem(cls, "full_name");
    if (fn) cJSON_SetValuestring(fn, full_name_buf);
    cJSON *mv = cJSON_GetObjectItem(cls, "move");
    if (mv && cJSON_IsNumber(mv)) { mv->valueint = move_val; mv->valuedouble = move_val; }
    cJSON *mt = cJSON_GetObjectItem(cls, "move_type");
    if (mt) cJSON_SetValuestring(mt, move_type_buf);

    const char *gkeys[] = {"hp_growth","mp_growth","attack_growth","defense_growth","agility_growth"};
    for (int i = 0; i < 5; i++) {
        cJSON *gr = cJSON_GetObjectItem(cls, gkeys[i]);
        if (!gr) { gr = cJSON_CreateObject(); cJSON_ReplaceItemInObject(cls, gkeys[i], gr); }
        cJSON *st = cJSON_GetObjectItem(gr, "start");
        if (st && cJSON_IsNumber(st)) { st->valueint = growth[i].start; st->valuedouble = growth[i].start; }
        cJSON *pr = cJSON_GetObjectItem(gr, "projected");
        if (pr && cJSON_IsNumber(pr)) { pr->valueint = growth[i].projected; pr->valuedouble = growth[i].projected; }
        cJSON *cu = cJSON_GetObjectItem(gr, "curve");
        if (cu && cJSON_IsString(cu)) cJSON_SetValuestring(cu, growth[i].curve);
    }

    cJSON *spells = cJSON_CreateArray();
    for (int i = 0; i < class_spell_count; i++) {
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddNumberToObject(entry, "level", class_spells[i].level);
        cJSON_AddStringToObject(entry, "spell", class_spells[i].spell_name);
        cJSON_AddNumberToObject(entry, "spell_level", class_spells[i].spell_level);
        cJSON_AddItemToArray(spells, entry);
    }
    cJSON_ReplaceItemInObject(cls, "spell_list", spells);
}

static void load_class_fields(void) {
    if (selected_class < 0) return;
    cJSON *cls = cJSON_GetArrayItem(classes, selected_class);
    const char *s = cJSON_GetObjectItem(cls, "name")->valuestring;
    strncpy(name_buf, s ? s : "", 10); name_buf[10] = '\0';
    const char *fn = cJSON_GetObjectItem(cls, "full_name")->valuestring;
    strncpy(full_name_buf, fn ? fn : "", 16); full_name_buf[16] = '\0';
    move_val = cJSON_GetObjectItem(cls, "move")->valueint;
    const char *mt = cJSON_GetObjectItem(cls, "move_type")->valuestring;
    strncpy(move_type_buf, mt ? mt : "", 31); move_type_buf[31] = '\0';

    const char *gkeys[] = {"hp_growth","mp_growth","attack_growth","defense_growth","agility_growth"};
    for (int i = 0; i < 5; i++) {
        cJSON *gr = cJSON_GetObjectItem(cls, gkeys[i]);
        if (gr) {
            growth[i].start = cJSON_GetObjectItem(gr, "start")->valueint;
            growth[i].projected = cJSON_GetObjectItem(gr, "projected")->valueint;
            const char *cv = cJSON_GetObjectItem(gr, "curve")->valuestring;
            strncpy(growth[i].curve, cv ? cv : "LINEAR", 63);
        } else {
            growth[i].start = 10; growth[i].projected = 30;
            strncpy(growth[i].curve, "LINEAR", 63);
        }
        for (int j = 0; j < curve_count; j++)
            if (strcmp(curve_names[j], growth[i].curve) == 0) { selected_curve_idx[i] = j; break; }
    }

    cJSON *spells = cJSON_GetObjectItem(cls, "spell_list");
    class_spell_count = 0;
    if (spells && cJSON_IsArray(spells)) {
        int sz = cJSON_GetArraySize(spells);
        for (int i = 0; i < sz && i < MAX_SPELLS_PER_CLASS; i++) {
            cJSON *ent = cJSON_GetArrayItem(spells, i);
            class_spells[i].level = cJSON_GetObjectItem(ent, "level")->valueint;
            const char *sp = cJSON_GetObjectItem(ent, "spell")->valuestring;
            strncpy(class_spells[i].spell_name, sp ? sp : "HEAL", 63);
            class_spells[i].spell_level = cJSON_GetObjectItem(ent, "spell_level")->valueint;
            class_spell_count++;
        }
    }
    selected_spell_entry = -1;
    open_class_fields();
}

static void open_class_fields(void) {
    class_field_count = 0;
    class_active_field = -1;
    int base_x = 360, base_y = 80, off = 50;

    snprintf(class_fields[0].text, sizeof(class_fields[0].text), "%s", name_buf);
    class_fields[0].cursor = strlen(class_fields[0].text);
    class_fields[0].active = 0; class_fields[0].is_numeric = 0; class_fields[0].max_len = 10; class_fields[0].field_id = 0;
    class_fields[0].rect = (SDL_Rect){base_x+off, base_y + 0*35, 150, 22};

    snprintf(class_fields[1].text, sizeof(class_fields[1].text), "%s", full_name_buf);
    class_fields[1].cursor = strlen(class_fields[1].text);
    class_fields[1].active = 0; class_fields[1].is_numeric = 0; class_fields[1].max_len = 16; class_fields[1].field_id = 1;
    class_fields[1].rect = (SDL_Rect){base_x+off, base_y + 1*35, 150, 22};

    snprintf(class_fields[2].text, sizeof(class_fields[2].text), "%d", move_val);
    class_fields[2].cursor = strlen(class_fields[2].text);
    class_fields[2].active = 0; class_fields[2].is_numeric = 1; class_fields[2].max_len = 3; class_fields[2].field_id = 2;
    class_fields[2].rect = (SDL_Rect){base_x+off, base_y + 2*35, 150, 22};

    snprintf(class_fields[3].text, sizeof(class_fields[3].text), "%s", move_type_buf);
    class_fields[3].cursor = 0; class_fields[3].active = 0; class_fields[3].is_numeric = 0; class_fields[3].max_len = 0; class_fields[3].field_id = 3;
    class_fields[3].rect = (SDL_Rect){base_x+off, base_y + 3*35, 150, 22};

    int idx = 4;
    for (int s = 0; s < 5; s++) {
        snprintf(class_fields[idx].text, sizeof(class_fields[idx].text), "%d", growth[s].start);
        class_fields[idx].cursor = strlen(class_fields[idx].text);
        class_fields[idx].active = 0; class_fields[idx].is_numeric = 1; class_fields[idx].max_len = 5;
        class_fields[idx].field_id = 100 + s*3 + 0;
        class_fields[idx].rect = (SDL_Rect){base_x+off, base_y + (4 + s)*35, 150, 22};
        idx++;
        snprintf(class_fields[idx].text, sizeof(class_fields[idx].text), "%d", growth[s].projected);
        class_fields[idx].cursor = strlen(class_fields[idx].text);
        class_fields[idx].active = 0; class_fields[idx].is_numeric = 1; class_fields[idx].max_len = 5;
        class_fields[idx].field_id = 100 + s*3 + 1;
        class_fields[idx].rect = (SDL_Rect){base_x+off+160, base_y + (4 + s)*35, 150, 22};
        idx++;
        snprintf(class_fields[idx].text, sizeof(class_fields[idx].text), "%s", growth[s].curve);
        class_fields[idx].cursor = 0; class_fields[idx].active = 0; class_fields[idx].is_numeric = 0; class_fields[idx].max_len = 0;
        class_fields[idx].field_id = 200 + s;
        class_fields[idx].rect = (SDL_Rect){base_x+off+320, base_y + (4 + s)*35, 150, 22};
        idx++;
    }
    class_field_count = idx;
}

static void commit_class_field(int idx) {
    if (idx < 0 || idx >= class_field_count) return;
    ClassField *f = &class_fields[idx];
    int id = f->field_id;
    if (id < 100) {
        if (id == 0) { strncpy(name_buf, f->text, 10); name_buf[10] = '\0'; }
        else if (id == 1) { strncpy(full_name_buf, f->text, 16); full_name_buf[16] = '\0'; }
        else if (id == 2) move_val = atoi(f->text);
        else if (id == 3) { strncpy(move_type_buf, f->text, 31); move_type_buf[31] = '\0'; }
    } else if (id >= 100 && id < 200) {
        int stat = (id - 100) / 3;
        int field = (id - 100) % 3;
        if (field == 0) growth[stat].start = atoi(f->text);
        else if (field == 1) growth[stat].projected = atoi(f->text);
    } else if (id >= 200) {
        int stat = id - 200;
        strncpy(growth[stat].curve, f->text, 63);
        for (int j = 0; j < curve_count; j++)
            if (strcmp(curve_names[j], growth[stat].curve) == 0) { selected_curve_idx[stat] = j; break; }
    }
    f->active = 0;
}

static int handle_class_input(SDL_Event *evt, int idx) {
    ClassField *f = &class_fields[idx];
    SDL_Rect r = f->rect;
    int is_curve = (f->field_id >= 200);
    if (evt->type == SDL_MOUSEBUTTONDOWN && evt->button.button == SDL_BUTTON_LEFT) {
        int mx = evt->button.x, my = evt->button.y;
        if (mx >= r.x && mx < r.x+r.w && my >= r.y && my < r.y+r.h) {
            if (!is_curve) {
                if (class_active_field >= 0 && class_active_field != idx) commit_class_field(class_active_field);
                class_active_field = idx;
                f->active = 1;
                f->cursor = strlen(f->text);
            }
            return 1;
        }
        if (f->active && !is_curve) {
            commit_class_field(idx);
            int best = -1, bestDist = 1000;
            for (int i = 0; i < class_field_count; i++) {
                if (class_fields[i].field_id >= 200) continue;
                SDL_Rect fr = class_fields[i].rect;
                int cy = fr.y + fr.h/2;
                int dist = abs(my - cy);
                if (dist < bestDist && mx >= fr.x - 20 && mx < fr.x + fr.w + 20) { bestDist = dist; best = i; }
            }
            if (best >= 0) {
                class_fields[best].active = 1;
                class_fields[best].cursor = strlen(class_fields[best].text);
                class_active_field = best;
            } else class_active_field = -1;
            return 1;
        } else {
            int best = -1, bestDist = 1000;
            for (int i = 0; i < class_field_count; i++) {
                if (class_fields[i].field_id >= 200) continue;
                SDL_Rect fr = class_fields[i].rect;
                int cy = fr.y + fr.h/2;
                int dist = abs(my - cy);
                if (dist < bestDist && mx >= fr.x - 20 && mx < fr.x + fr.w + 20) { bestDist = dist; best = i; }
            }
            if (best >= 0) {
                if (class_active_field >= 0) commit_class_field(class_active_field);
                class_fields[best].active = 1;
                class_fields[best].cursor = strlen(class_fields[best].text);
                class_active_field = best;
                return 1;
            }
        }
    } else if (evt->type == SDL_KEYDOWN && f->active && !is_curve) {
        if (evt->key.keysym.sym == SDLK_UP) {
            if (idx == 0) return 1;
            commit_class_field(idx);
            int new_idx = idx - 1;
            while (new_idx >= 0 && class_fields[new_idx].field_id >= 200) new_idx--;
            if (new_idx >= 0) {
                class_fields[new_idx].active = 1;
                class_fields[new_idx].cursor = strlen(class_fields[new_idx].text);
                class_active_field = new_idx;
            } else class_active_field = -1;
            return 1;
        } else if (evt->key.keysym.sym == SDLK_DOWN) {
            commit_class_field(idx);
            int new_idx = idx + 1;
            while (new_idx < class_field_count && class_fields[new_idx].field_id >= 200) new_idx++;
            if (new_idx < class_field_count) {
                class_fields[new_idx].active = 1;
                class_fields[new_idx].cursor = strlen(class_fields[new_idx].text);
                class_active_field = new_idx;
            } else class_active_field = -1;
            return 1;
        } else if (evt->key.keysym.sym == SDLK_BACKSPACE) {
            if (f->cursor > 0) {
                memmove(f->text + f->cursor - 1, f->text + f->cursor, strlen(f->text) - f->cursor + 1);
                f->cursor--;
            }
            return 1;
        } else if (evt->key.keysym.sym == SDLK_RETURN || evt->key.keysym.sym == SDLK_KP_ENTER) {
            commit_class_field(idx);
            return 1;
        } else if (evt->key.keysym.sym == SDLK_LEFT && f->cursor > 0) { f->cursor--; return 1; }
        else if (evt->key.keysym.sym == SDLK_RIGHT && f->cursor < strlen(f->text)) { f->cursor++; return 1; }
    } else if (evt->type == SDL_TEXTINPUT && f->active && !is_curve) {
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

static void draw_class_field(SDL_Renderer *r, int x, int y, int w, int h, int idx, const char *label, const char *display) {
    SDL_Color white = {255,255,255}, black = {0,0,0}, gray = {100,100,100};
    draw_text_ext(r, x, y + 3, label, white);
    int fx = x + 50;
    SDL_Rect rect = {fx, y, w, h};
    SDL_SetRenderDrawColor(r, gray.r, gray.g, gray.b, 255); SDL_RenderFillRect(r, &rect);
    SDL_SetRenderDrawColor(r, white.r, white.g, white.b, 255); SDL_RenderDrawRect(r, &rect);
    draw_text_ext(r, fx+5, y+3, display, black);
    if (idx >= 0 && idx < class_field_count && class_fields[idx].active) {
        ClassField *f = &class_fields[idx];
        char before[256] = {0};
        if (f->cursor > 0) {
            strncpy(before, f->text, f->cursor);
            before[f->cursor] = '\0';
        }
        SDL_Surface *s = TTF_RenderUTF8_Solid(g_font, before, black);
        int offset = fx+5 + (s ? s->w : 0);
        if (s) SDL_FreeSurface(s);
        draw_text_ext(r, offset, y+3, "|", black);
    }
}

// ------------------------------------------------------------
// Публичные функции
// ------------------------------------------------------------

void classes_init(cJSON *json_array, int count) {
    classes = json_array;
    class_count = count;
    selected_class = -1;
    class_scroll = 0;
    save_timer = 0;
    build_curve_list();
    build_spell_name_list();
    class_field_count = 0;
    class_active_field = -1;
    if (class_count > 0) { selected_class = 0; load_class_fields(); }
}

void classes_save_to_file(void) {
    if (!classes) return;
    cJSON *root = cJSON_CreateObject();
    cJSON *dup = cJSON_Duplicate(classes, 1);
    cJSON_AddItemToObject(root, "classes", dup);
    char *js = cJSON_PrintBuffered(root, 0, 1);
    FILE *f = fopen("../data/actors/classes.json", "w");
    if (f) { fputs(js, f); fclose(f); error_msg[0] = '\0'; }
    else snprintf(error_msg, sizeof(error_msg), "Failed to save classes.json");
    free(js); cJSON_Delete(root);
    save_timer = SAVE_BLINK_DURATION;
}

int classes_is_edit_active(void) { return class_active_field >= 0; }
void classes_reset_selection(void) {
    if (class_count > 0) { selected_class = 0; load_class_fields(); }
    else { selected_class = -1; class_active_field = -1; class_field_count = 0; }
}
void classes_adjust_scroll(int delta) { class_scroll -= delta; }
int classes_get_scroll(void) { return class_scroll; }

void classes_draw_list(SDL_Renderer *renderer, int y_offset, int scroll) {
    if (!classes) return;
    int y = y_offset - scroll;
    SDL_Color white = {255,255,255};
    for (int i = 0; i < class_count; i++) {
        if (y + 20 < y_offset || y > y_offset + 600) { y += 20; continue; }
        cJSON *cls = cJSON_GetArrayItem(classes, i);
        int id = cJSON_GetObjectItem(cls, "id")->valueint;
        const char *nm = cJSON_GetObjectItem(cls, "name")->valuestring;
        char buf[256];
        snprintf(buf, sizeof(buf), "%d: %s", id, nm);
        SDL_Rect rr = {10, y, 290, 20};
        if (i == selected_class) { SDL_SetRenderDrawColor(renderer, 100,100,255,128); SDL_RenderFillRect(renderer, &rr); }
        draw_text_ext(renderer, 10, y, buf, white);
        y += 20;
    }
}

int classes_get_total_height(void) { return class_count * 20; }

void classes_handle_click(int mx, int my, int y_offset, int scroll) {
    int y = y_offset - scroll;
    for (int i = 0; i < class_count; i++) {
        if (mx >= 10 && mx < 300 && my >= y && my < y+20) {
            if (selected_class != i) {
                if (selected_class >= 0) commit_class_changes();
                selected_class = i;
                load_class_fields();
            }
            return;
        }
        y += 20;
    }
}

void classes_draw_edit_panel(SDL_Renderer *renderer, int px, int py) {
    if (selected_class < 0) return;
    SDL_Color black = {0,0,0,255}, white = {255,255,255}, gray = {100,100,100}, blue = {70,70,120};
    SDL_Rect panel = {px, py, 580, 550};
    SDL_SetRenderDrawColor(renderer, 60,60,60,255); SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &panel);

    int y = py + 10;

    cJSON *cls = cJSON_GetArrayItem(classes, selected_class);
    int actor_id = cJSON_GetObjectItem(cls, "id")->valueint;
    char idstr[16]; snprintf(idstr, sizeof(idstr), "ID:%d", actor_id);
    draw_text_ext(renderer, px+240, y+3, idstr, white);
    draw_class_field(renderer, px+10, y, 150, 22, 0, "Name:", name_buf);
    y += 35;

    draw_class_field(renderer, px+10, y, 150, 22, 1, "Full Name:", full_name_buf);
    y += 35;

    char mvstr[8]; snprintf(mvstr, sizeof(mvstr), "%d", move_val);
    draw_class_field(renderer, px+10, y, 150, 22, 2, "Move:", mvstr);
    y += 35;

    draw_text_ext(renderer, px+10, y+3, "Move Type:", white);
    draw_text_ext(renderer, px+110, y+3, move_type_buf, white);
    y += 35;

    for (int s = 0; s < 5; s++) {
        char start_str[8], proj_str[8];
        snprintf(start_str, sizeof(start_str), "%d", growth[s].start);
        snprintf(proj_str, sizeof(proj_str), "%d", growth[s].projected);
        int base_idx = 4 + s*3;
        draw_text_ext(renderer, px+10, y+3, growth_names[s], white);
        draw_class_field(renderer, px+90, y, 70, 22, base_idx, "St:", start_str);
        draw_class_field(renderer, px+260, y, 80, 22, base_idx+1, "Pr:", proj_str);
        draw_text_ext(renderer, px+360, y+3, "Curve:", white);
        SDL_Rect cr = {px+420, y, 100, 22};
        SDL_SetRenderDrawColor(renderer, gray.r, gray.g, gray.b, 255); SDL_RenderFillRect(renderer, &cr);
        SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &cr);
        draw_text_ext(renderer, px+425, y+3, growth[s].curve, black);
        SDL_Rect prev = {px+525, y, 20, 22};
        SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, 255); SDL_RenderFillRect(renderer, &prev);
        SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &prev);
        draw_text_ext(renderer, px+530, y+3, "<", white);
        SDL_Rect next = {px+550, y, 20, 22};
        SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, 255); SDL_RenderFillRect(renderer, &next);
        SDL_SetRenderDrawColor(renderer, white.r, white.g, white.b, 255); SDL_RenderDrawRect(renderer, &next);
        draw_text_ext(renderer, px+555, y+3, ">", white);
        y += 35;
    }

    y += 10;
    draw_text_ext(renderer, px+10, y, "Spell List:", white);
    y += 20;
    for (int i = 0; i < class_spell_count; i++) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Lv %d: %s (Lv %d)", class_spells[i].level, class_spells[i].spell_name, class_spells[i].spell_level);
        SDL_Rect rr = {px+20, y, 400, 20};
        if (i == selected_spell_entry) {
            SDL_SetRenderDrawColor(renderer, blue.r, blue.g, blue.b, 128);
            SDL_RenderFillRect(renderer, &rr);
        }
        draw_text_ext(renderer, px+20, y, buf, white);
        y += 20;
    }
    SDL_Rect add_spell_btn = {px+20, y, 30, 20};
    SDL_SetRenderDrawColor(renderer, 100,200,100,255); SDL_RenderFillRect(renderer, &add_spell_btn);
    SDL_SetRenderDrawColor(renderer, 0,0,0,255); SDL_RenderDrawRect(renderer, &add_spell_btn);
    draw_text_ext(renderer, add_spell_btn.x+5, add_spell_btn.y+2, "+", black);
    if (class_spell_count > 0) {
        SDL_Rect del_spell_btn = {px+60, y, 30, 20};
        SDL_SetRenderDrawColor(renderer, 200,80,80,255); SDL_RenderFillRect(renderer, &del_spell_btn);
        SDL_SetRenderDrawColor(renderer, 0,0,0,255); SDL_RenderDrawRect(renderer, &del_spell_btn);
        draw_text_ext(renderer, del_spell_btn.x+5, del_spell_btn.y+2, "-", black);
    }
    y += 30;

    int btn_y = y + 10;
    last_btn_y = btn_y;

    SDL_Rect save_btn = {px+130, btn_y, 80, 30};
    SDL_Color save_col = (save_timer > 0) ? (SDL_Color){0,255,0,255} : (SDL_Color){255,255,0,255};
    SDL_SetRenderDrawColor(renderer, save_col.r, save_col.g, save_col.b, 255); SDL_RenderFillRect(renderer, &save_btn);
    SDL_SetRenderDrawColor(renderer, 0,0,0,255); SDL_RenderDrawRect(renderer, &save_btn);
    draw_text_ext(renderer, save_btn.x+15, save_btn.y+5, "SAVE", black);

    SDL_Rect refresh_btn = {px+220, btn_y, 80, 30};
    SDL_SetRenderDrawColor(renderer, 180,180,255,255); SDL_RenderFillRect(renderer, &refresh_btn);
    SDL_SetRenderDrawColor(renderer, 0,0,0,255); SDL_RenderDrawRect(renderer, &refresh_btn);
    draw_text_ext(renderer, refresh_btn.x+12, refresh_btn.y+5, "Refresh", black);

    SDL_Rect del_btn = {px+10, btn_y, 100, 30};
    SDL_SetRenderDrawColor(renderer, 200,80,80,255); SDL_RenderFillRect(renderer, &del_btn);
    SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &del_btn);
    draw_text_ext(renderer, del_btn.x+15, del_btn.y+5, "Del Class", white);

    SDL_Rect add_btn = {px+310, btn_y, 100, 30};
    SDL_SetRenderDrawColor(renderer, 100,200,100,255); SDL_RenderFillRect(renderer, &add_btn);
    SDL_SetRenderDrawColor(renderer, 255,255,255,255); SDL_RenderDrawRect(renderer, &add_btn);
    draw_text_ext(renderer, add_btn.x+10, add_btn.y+5, "Add Class", white);
}

void classes_handle_input(SDL_Event *evt) {
    for (int i = 0; i < class_field_count; i++) {
        if (handle_class_input(evt, i)) break;
    }
    if (class_active_field >= 0 && (evt->type == SDL_KEYDOWN || evt->type == SDL_TEXTINPUT))
        return;

    if (evt->type == SDL_MOUSEBUTTONDOWN && evt->button.button == SDL_BUTTON_LEFT) {
        int mx = evt->button.x, my = evt->button.y;
        if (selected_class < 0) return;
        int px = 360, py = 50;
        int base_y = py + 10;

        for (int s = 0; s < 5; s++) {
            SDL_Rect prev = {px+525, base_y + (4+s)*35, 20, 22};
            SDL_Rect next = {px+550, base_y + (4+s)*35, 20, 22};
            if (mx >= prev.x && mx < prev.x+prev.w && my >= prev.y && my < prev.y+prev.h) {
                if (curve_count > 0 && selected_curve_idx[s] > 0) {
                    selected_curve_idx[s]--;
                    strncpy(growth[s].curve, curve_names[selected_curve_idx[s]], 63);
                    commit_class_changes();
                }
                return;
            }
            if (mx >= next.x && mx < next.x+next.w && my >= next.y && my < next.y+next.h) {
                if (curve_count > 0 && selected_curve_idx[s] < curve_count-1) {
                    selected_curve_idx[s]++;
                    strncpy(growth[s].curve, curve_names[selected_curve_idx[s]], 63);
                    commit_class_changes();
                }
                return;
            }
        }

        int y_spell_header = base_y + 4*35 + 5*35 + 10;
        int list_y = y_spell_header + 20;
        for (int i = 0; i < class_spell_count; i++) {
            SDL_Rect item = {px+20, list_y + i*20, 400, 20};
            if (mx >= item.x && mx < item.x+item.w && my >= item.y && my < item.y+item.h) {
                selected_spell_entry = i;
                return;
            }
        }
        SDL_Rect add_spell_btn = {px+20, list_y + class_spell_count*20, 30, 20};
        if (mx >= add_spell_btn.x && mx < add_spell_btn.x+add_spell_btn.w && my >= add_spell_btn.y && my < add_spell_btn.y+add_spell_btn.h) {
            if (class_spell_count < MAX_SPELLS_PER_CLASS) {
                class_spells[class_spell_count].level = 1;
                strncpy(class_spells[class_spell_count].spell_name, "HEAL", 63);
                class_spells[class_spell_count].spell_level = 1;
                class_spell_count++;
                commit_class_changes();
            }
            return;
        }
        if (class_spell_count > 0) {
            SDL_Rect del_spell_btn = {px+60, list_y + class_spell_count*20, 30, 20};
            if (mx >= del_spell_btn.x && mx < del_spell_btn.x+del_spell_btn.w && my >= del_spell_btn.y && my < del_spell_btn.y+del_spell_btn.h) {
                if (selected_spell_entry >= 0 && selected_spell_entry < class_spell_count) {
                    for (int i = selected_spell_entry; i < class_spell_count-1; i++)
                        class_spells[i] = class_spells[i+1];
                    class_spell_count--;
                    selected_spell_entry = -1;
                    commit_class_changes();
                }
                return;
            }
        }

        int btn_y = last_btn_y;
        SDL_Rect save_btn = {px+130, btn_y, 80, 30};
        SDL_Rect refresh_btn = {px+220, btn_y, 80, 30};
        SDL_Rect del_btn = {px+10, btn_y, 100, 30};
        SDL_Rect add_btn = {px+310, btn_y, 100, 30};
        if (mx >= save_btn.x && mx < save_btn.x+save_btn.w && my >= save_btn.y && my < save_btn.y+save_btn.h) {
            classes_save_to_file(); return;
        }
        if (mx >= refresh_btn.x && mx < refresh_btn.x+refresh_btn.w && my >= refresh_btn.y && my < refresh_btn.y+refresh_btn.h) {
            classes_reload(); return;
        }
        if (mx >= del_btn.x && mx < del_btn.x+del_btn.w && my >= del_btn.y && my < del_btn.y+del_btn.h) {
            delete_class(); return;
        }
        if (mx >= add_btn.x && mx < add_btn.x+add_btn.w && my >= add_btn.y && my < add_btn.y+add_btn.h) {
            add_new_class(); return;
        }
    }
}

static void add_new_class(void) {
    if (!classes) return;
    int new_id = 0;
    for (int i = 0; i < class_count; i++) {
        int id = cJSON_GetObjectItem(cJSON_GetArrayItem(classes, i), "id")->valueint;
        if (id >= new_id) new_id = id + 1;
    }
    char new_name[64]; int idx = 0;
    while (1) {
        snprintf(new_name, sizeof(new_name), "Class%02d", idx);
        int found = 0;
        for (int i = 0; i < class_count; i++)
            if (strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(classes, i), "name")->valuestring, new_name) == 0) found = 1;
        if (!found) break;
        idx++;
    }
    cJSON *cls = cJSON_CreateObject();
    cJSON_AddNumberToObject(cls, "id", new_id);
    cJSON_AddStringToObject(cls, "name", new_name);
    cJSON_AddStringToObject(cls, "full_name", "FullName");
    cJSON_AddNumberToObject(cls, "move", 5);
    cJSON_AddStringToObject(cls, "move_type", "REGULAR");
    const char *gkeys[] = {"hp_growth","mp_growth","attack_growth","defense_growth","agility_growth"};
    for (int i = 0; i < 5; i++) {
        cJSON *gr = cJSON_CreateObject();
        cJSON_AddNumberToObject(gr, "start", 10);
        cJSON_AddNumberToObject(gr, "projected", 30);
        cJSON_AddStringToObject(gr, "curve", "LINEAR");
        cJSON_AddItemToObject(cls, gkeys[i], gr);
    }
    cJSON *spells = cJSON_CreateArray();
    cJSON_AddItemToObject(cls, "spell_list", spells);
    cJSON_AddItemToArray(classes, cls);
    class_count++;
    if (selected_class >= 0) commit_class_changes();
    selected_class = class_count - 1;
    load_class_fields();
}

static void delete_class(void) {
    if (selected_class < 0 || !classes) return;
    cJSON *new_arr = cJSON_CreateArray();
    for (int i = 0; i < class_count; i++) {
        if (i == selected_class) continue;
        cJSON_AddItemToArray(new_arr, cJSON_Duplicate(cJSON_GetArrayItem(classes, i), 1));
    }
    cJSON_Delete(classes);
    classes = new_arr;
    class_count--;
    if (class_count == 0) {
        selected_class = -1;
        class_active_field = -1;
        class_field_count = 0;
    } else {
        if (selected_class >= class_count) selected_class = class_count - 1;
        if (selected_class >= 0) load_class_fields();
    }
}

void classes_reload(void) {
    if (classes) { cJSON_Delete(classes); classes = NULL; }
    FILE *f = fopen("../data/actors/classes.json", "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(len+1); fread(buf, 1, len, f); buf[len] = '\0'; fclose(f);
    cJSON *root = cJSON_Parse(buf); free(buf);
    if (root) {
        cJSON *arr = cJSON_GetObjectItem(root, "classes");
        if (arr && cJSON_IsArray(arr)) {
            classes_json = arr;
            classes_count = cJSON_GetArraySize(arr);
            classes = classes_json;
            class_count = classes_count;
            selected_class = -1;
            class_scroll = 0;
            save_timer = 0;
            build_curve_list();
            build_spell_name_list();
            class_field_count = 0;
            class_active_field = -1;
            if (class_count > 0) { selected_class = 0; load_class_fields(); }
        } else cJSON_Delete(root);
    }
}

void classes_update_timer(void) {
    if (save_timer > 0) save_timer--;
}