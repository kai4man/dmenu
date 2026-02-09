#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/input.h>
#include <fcntl.h>

const int SPACING_ADJUST = 40; 

typedef struct {
    char *name; char *script; char *bg_path; char *logo_path;
    SDL_Surface *surf; SDL_Surface *logo;
    int real_w;
} Console;

Console consoles[] = {
    {"CPS1", "/mnt/sdcard/scripts/cps1.sh", "/mnt/sdcard/theme/cps1/bg.png", "/mnt/sdcard/theme/cps1/logo.png", NULL, NULL, 0},
    {"CPS2", "/mnt/sdcard/scripts/cps2.sh", "/mnt/sdcard/theme/cps2/bg.png", "/mnt/sdcard/theme/cps2/logo.png", NULL, NULL, 0},
    {"CPS3", "/mnt/sdcard/scripts/cps3.sh", "/mnt/sdcard/theme/cps3/bg.png", "/mnt/sdcard/theme/cps3/logo.png", NULL, NULL, 0},
    {"GBA",  "/mnt/sdcard/scripts/gba.sh",  "/mnt/sdcard/theme/gba/bg.png",  "/mnt/sdcard/theme/gba/logo.png",  NULL, NULL, 0},
    {"SNES", "/mnt/sdcard/scripts/snes.sh", "/mnt/sdcard/theme/sfc/bg.png",  "/mnt/sdcard/theme/sfc/logo.png",  NULL, NULL, 0},
    {"MD",   "/mnt/sdcard/scripts/md.sh",   "/mnt/sdcard/theme/md/bg.png",   "/mnt/sdcard/theme/md/logo.png",   NULL, NULL, 0},
    {"GB",   "/mnt/sdcard/scripts/gb.sh",   "/mnt/sdcard/theme/gb/bg.png",   "/mnt/sdcard/theme/gb/logo.png",   NULL, NULL, 0},
    {"GBC",  "/mnt/sdcard/scripts/gbc.sh",  "/mnt/sdcard/theme/gbc/bg.png",  "/mnt/sdcard/theme/gbc/logo.png",  NULL, NULL, 0}
};

const int COUNT = sizeof(consoles) / sizeof(Console);

SDL_Surface* prepare_image(SDL_Surface* src, int target_w, int target_h) {
    if (!src) return NULL;
    SDL_Surface* safe_src = SDL_CreateRGBSurface(SDL_SWSURFACE, src->w, src->h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    SDL_SetAlpha(src, 0, 0); 
    SDL_BlitSurface(src, NULL, safe_src, NULL);
    SDL_Surface* tmp = SDL_CreateRGBSurface(SDL_SWSURFACE, target_w, target_h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    SDL_SoftStretch(safe_src, NULL, tmp, NULL);
    SDL_Surface* final = SDL_DisplayFormatAlpha(tmp);
    SDL_FreeSurface(safe_src);
    SDL_FreeSurface(tmp);
    return final;
}

int main() {
    putenv("SDL_NOMOUSE=1");
    putenv("SDL_VIDEODRIVER=fbcon");
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    const SDL_VideoInfo* info = SDL_GetVideoInfo();
    int sw = info->current_w;
    int sh = info->current_h;

    SDL_Surface *screen = SDL_SetVideoMode(sw, sh, 32, SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_FULLSCREEN);

    // Подготовка полноэкранного затемнения
    SDL_Surface *shadow = SDL_CreateRGBSurface(SDL_SWSURFACE, sw, sh, 32, 0, 0, 0, 0);
    SDL_FillRect(shadow, NULL, 0); 
    SDL_SetAlpha(shadow, SDL_SRCALPHA, 180); // 180 - довольно плотное затемнение

    for(int i = 0; i < COUNT; i++) {
        SDL_Surface *t = IMG_Load(consoles[i].bg_path);
        if(t) { 
            float aspect = (float)t->w / t->h;
            int target_w = (int)(sh * aspect);
            consoles[i].surf = prepare_image(t, target_w, sh);
            consoles[i].real_w = target_w;
            SDL_FreeSurface(t); 
        }
        t = IMG_Load(consoles[i].logo_path);
        if(t) {
            int lw = (int)(sw * 0.4f);
            int lh = (int)(lw * t->h / t->w);
            consoles[i].logo = prepare_image(t, lw, lh); 
            SDL_FreeSurface(t);
        }
    }

    int input_fd = open("/dev/input/event1", O_RDONLY | O_NONBLOCK);
    int selected = 0;
    struct input_event ev;

    while(1) {
        while(read(input_fd, &ev, sizeof(ev)) == sizeof(ev)) {
            if(ev.type == EV_ABS && ev.code == 0x11) {
                if(ev.value == -1) selected = (selected - 1 + COUNT) % COUNT;
                if(ev.value == 1)  selected = (selected + 1) % COUNT;
            }
        }

        // 1. Очистка фона
        SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));

        // 2. Рисуем ВЕСЬ СЛАЙДЕР (кроме центрального блока для оптимизации)
        for(int offset = -4; offset <= 4; offset++) {
            if (offset == 0) continue; // Центр пропустим, нарисуем его позже поверх тени
            
            int idx = (selected + offset + (COUNT * 100)) % COUNT;
            if(!consoles[idx].surf) continue;

            int x_pos = (sw / 2) - (consoles[selected].real_w / 2);
            if (offset < 0) { 
                for(int k = -1; k >= offset; k--) {
                    int k_idx = (selected + k + (COUNT * 100)) % COUNT;
                    x_pos -= (consoles[k_idx].real_w - SPACING_ADJUST);
                }
            } else {
                x_pos += (consoles[selected].real_w - SPACING_ADJUST);
                for(int k = 1; k < offset; k++) {
                    int k_idx = (selected + k + (COUNT * 100)) % COUNT;
                    x_pos += (consoles[k_idx].real_w - SPACING_ADJUST);
                }
            }
            SDL_Rect dst = { x_pos, 0, consoles[idx].real_w, sh };
            SDL_BlitSurface(consoles[idx].surf, NULL, screen, &dst);
        }

        // 3. НАКЛАДЫВАЕМ ЭКРАН ЗАТЕМНЕНИЯ (на всё, что нарисовали)
        SDL_BlitSurface(shadow, NULL, screen, NULL);

        // 4. РИСУЕМ ЦЕНТРАЛЬНЫЙ БЛОК (он будет выше по индексу/слою, чем тень)
        if(consoles[selected].surf) {
            int cx = (sw / 2) - (consoles[selected].real_w / 2);
            SDL_Rect c_dst = { cx, 0, consoles[selected].real_w, sh };
            SDL_BlitSurface(consoles[selected].surf, NULL, screen, &c_dst);
        }

        // 5. РИСУЕМ ЛОГОТИП (самый верхний слой)
        if(consoles[selected].logo) {
            SDL_Rect l_pos = { (sw - consoles[selected].logo->w)/2, (sh - consoles[selected].logo->h)/2 };
            SDL_BlitSurface(consoles[selected].logo, NULL, screen, &l_pos);
        }

        SDL_Flip(screen);
        usleep(16000);
    }
    return 0;
}