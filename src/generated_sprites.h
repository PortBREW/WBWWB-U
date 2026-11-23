#pragma once
#include <SDL.h>
#include <cstring>
inline SDL_Rect spriteFrame(const char *sheet, int index) {
    if (std::strcmp(sheet, "body")==0) {
        static const SDL_Rect spriteFrames_body[] = { {0,0,120,140},{120,0,120,140} };
        if(index<0 || index>=int(sizeof(spriteFrames_body)/sizeof(spriteFrames_body[0]))) index=0;
        return spriteFrames_body[index];
    }
    if (std::strcmp(sheet, "body_red")==0) {
        static const SDL_Rect spriteFrames_body_red[] = { {0,0,120,140},{120,0,120,140} };
        if(index<0 || index>=int(sizeof(spriteFrames_body_red)/sizeof(spriteFrames_body_red[0]))) index=0;
        return spriteFrames_body_red[index];
    }
    if (std::strcmp(sheet, "face")==0) {
        static const SDL_Rect spriteFrames_face[] = { {0,0,120,140},{120,0,120,140},{0,140,120,140},{120,140,120,140},{0,280,120,140},{120,280,120,140},{0,420,120,140},{120,420,120,140},{0,560,120,140},{120,560,120,140},{0,700,120,140},{120,700,120,140},{0,840,120,140} };
        if(index<0 || index>=int(sizeof(spriteFrames_face)/sizeof(spriteFrames_face[0]))) index=0;
        return spriteFrames_face[index];
    }
    if (std::strcmp(sheet, "face_murder")==0) {
        static const SDL_Rect spriteFrames_face_murder[] = { {0,0,120,140},{120,0,120,140},{0,140,120,140},{120,140,120,140},{0,280,120,140},{120,280,120,140},{0,420,120,140},{120,420,120,140},{0,560,120,140},{120,560,120,140},{0,700,120,140},{120,700,120,140},{0,840,120,140},{120,840,120,140} };
        if(index<0 || index>=int(sizeof(spriteFrames_face_murder)/sizeof(spriteFrames_face_murder[0]))) index=0;
        return spriteFrames_face_murder[index];
    }
    if (std::strcmp(sheet, "hat")==0) {
        static const SDL_Rect spriteFrames_hat[] = { {0,0,120,140},{120,0,120,140},{240,0,120,140},{360,0,120,140},{0,140,120,140},{120,140,120,140},{240,140,120,140},{360,140,120,140},{0,280,120,140},{120,280,120,140},{240,280,120,140},{360,280,120,140},{0,420,120,140},{120,420,120,140},{240,420,120,140},{360,420,120,140},{0,560,120,140},{120,560,120,140},{240,560,120,140},{360,560,120,140},{0,700,120,140},{120,700,120,140},{240,700,120,140},{360,700,120,140},{0,840,120,140},{120,840,120,140},{240,840,120,140} };
        if(index<0 || index>=int(sizeof(spriteFrames_hat)/sizeof(spriteFrames_hat[0]))) index=0;
        return spriteFrames_hat[index];
    }
    if (std::strcmp(sheet, "hangry")==0) {
        static const SDL_Rect spriteFrames_hangry[] = { {0,0,150,140},{150,0,150,140},{300,0,150,140},{0,140,150,140},{150,140,150,140},{300,140,150,140},{0,280,150,140},{150,280,150,140},{300,280,150,140},{0,420,150,140},{150,420,150,140},{300,420,150,140},{0,560,150,140},{150,560,150,140},{300,560,150,140},{0,700,150,140},{150,700,150,140},{300,700,150,140},{0,840,150,140},{150,840,150,140},{300,840,150,140} };
        if(index<0 || index>=int(sizeof(spriteFrames_hangry)/sizeof(spriteFrames_hangry[0]))) index=0;
        return spriteFrames_hangry[index];
    }
    if (std::strcmp(sheet, "hatguy")==0) {
        static const SDL_Rect spriteFrames_hatguy[] = { {0,0,120,140} };
        if(index<0 || index>=int(sizeof(spriteFrames_hatguy)/sizeof(spriteFrames_hatguy[0]))) index=0;
        return spriteFrames_hatguy[index];
    }
    if (std::strcmp(sheet, "gun")==0) {
        static const SDL_Rect spriteFrames_gun[] = { {0,0,160,140},{160,0,160,140},{320,0,160,140},{0,140,160,140},{160,140,160,140},{320,140,160,140},{0,280,160,140},{160,280,160,140},{320,280,160,140},{0,420,160,140},{160,420,160,140},{320,420,160,140},{0,560,160,140},{160,560,160,140},{320,560,160,140},{0,700,160,140},{160,700,160,140},{320,700,160,140} };
        if(index<0 || index>=int(sizeof(spriteFrames_gun)/sizeof(spriteFrames_gun[0]))) index=0;
        return spriteFrames_gun[index];
    }
    if (std::strcmp(sheet, "peace")==0) {
        static const SDL_Rect spriteFrames_peace[] = { {0,0,240,245},{240,0,240,245},{480,0,240,245},{720,0,240,245},{960,0,240,245},{1200,0,240,245},{1440,0,240,245},{1680,0,240,245},{0,245,240,245},{240,245,240,245},{480,245,240,245},{720,245,240,245},{960,245,240,245},{1200,245,240,245},{1440,245,240,245},{1680,245,240,245},{0,490,240,245},{240,490,240,245},{480,490,240,245},{720,490,240,245},{960,490,240,245},{1200,490,240,245},{1440,490,240,245},{1680,490,240,245},{0,735,240,245},{240,735,240,245},{480,735,240,245},{720,735,240,245},{960,735,240,245},{1200,735,240,245},{1440,735,240,245},{1680,735,240,245},{0,980,240,245} };
        if(index<0 || index>=int(sizeof(spriteFrames_peace)/sizeof(spriteFrames_peace[0]))) index=0;
        return spriteFrames_peace[index];
    }
    if (std::strcmp(sheet, "helping")==0) {
        static const SDL_Rect spriteFrames_helping[] = { {0,0,300,164},{300,0,300,164},{600,0,300,164},{900,0,300,164},{1200,0,300,164},{1500,0,300,164},{0,164,300,164},{300,164,300,164},{600,164,300,164},{900,164,300,164},{1200,164,300,164},{1500,164,300,164},{0,328,300,164},{300,328,300,164},{600,328,300,164},{900,328,300,164},{1200,328,300,164},{1500,328,300,164},{0,492,300,164},{300,492,300,164},{600,492,300,164},{900,492,300,164},{1200,492,300,164},{1500,492,300,164},{0,656,300,164},{300,656,300,164},{600,656,300,164},{900,656,300,164},{1200,656,300,164},{1500,656,300,164},{0,820,300,164},{300,820,300,164},{600,820,300,164},{900,820,300,164},{1200,820,300,164},{1500,820,300,164},{0,984,300,164},{300,984,300,164},{600,984,300,164},{900,984,300,164},{1200,984,300,164},{1500,984,300,164},{0,1148,300,164},{300,1148,300,164},{600,1148,300,164},{900,1148,300,164},{1200,1148,300,164},{1500,1148,300,164},{0,1312,300,164},{300,1312,300,164},{600,1312,300,164},{900,1312,300,164},{1200,1312,300,164},{1500,1312,300,164},{0,1476,300,164},{300,1476,300,164},{600,1476,300,164},{900,1476,300,164},{1200,1476,300,164},{1500,1476,300,164},{0,1640,300,164},{300,1640,300,164},{600,1640,300,164},{900,1640,300,164},{1200,1640,300,164},{1500,1640,300,164},{0,1804,300,164},{300,1804,300,164},{600,1804,300,164},{900,1804,300,164},{1200,1804,300,164},{1500,1804,300,164},{0,1968,300,164},{300,1968,300,164},{600,1968,300,164},{900,1968,300,164},{1200,1968,300,164},{1500,1968,300,164},{0,2132,300,164},{300,2132,300,164},{600,2132,300,164},{900,2132,300,164},{1200,2132,300,164},{1500,2132,300,164},{0,2296,300,164},{300,2296,300,164},{600,2296,300,164},{900,2296,300,164},{1200,2296,300,164},{1500,2296,300,164},{0,2460,300,164},{300,2460,300,164},{600,2460,300,164},{900,2460,300,164},{1200,2460,300,164},{1500,2460,300,164},{0,2624,300,164},{300,2624,300,164},{600,2624,300,164},{900,2624,300,164},{1200,2624,300,164},{1500,2624,300,164},{0,2788,300,164},{300,2788,300,164},{600,2788,300,164},{900,2788,300,164},{1200,2788,300,164},{1500,2788,300,164},{0,2952,300,164},{300,2952,300,164},{600,2952,300,164},{900,2952,300,164} };
        if(index<0 || index>=int(sizeof(spriteFrames_helping)/sizeof(spriteFrames_helping[0]))) index=0;
        return spriteFrames_helping[index];
    }
    if (std::strcmp(sheet, "face_nervous")==0) {
        static const SDL_Rect spriteFrames_face_nervous[] = { {0,0,120,140},{120,0,120,140},{240,0,120,140},{360,0,120,140},{480,0,120,140},{600,0,120,140},{720,0,120,140},{840,0,120,140},{0,140,120,140},{120,140,120,140},{240,140,120,140},{360,140,120,140},{480,140,120,140},{600,140,120,140},{720,140,120,140},{840,140,120,140},{0,280,120,140},{120,280,120,140},{240,280,120,140},{360,280,120,140},{480,280,120,140},{600,280,120,140},{720,280,120,140},{840,280,120,140},{0,420,120,140},{120,420,120,140},{240,420,120,140},{360,420,120,140},{480,420,120,140},{600,420,120,140} };
        if(index<0 || index>=int(sizeof(spriteFrames_face_nervous)/sizeof(spriteFrames_face_nervous[0]))) index=0;
        return spriteFrames_face_nervous[index];
    }
    if (std::strcmp(sheet, "face_snobby")==0) {
        static const SDL_Rect spriteFrames_face_snobby[] = { {0,0,120,141},{120,0,120,141},{240,0,120,141},{360,0,120,141},{480,0,120,141},{600,0,120,141},{720,0,120,141},{840,0,120,141},{0,141,120,141},{120,141,120,141},{240,141,120,141},{360,141,120,141},{480,141,120,141},{600,141,120,141},{720,141,120,141},{840,141,120,141},{0,282,120,141},{120,282,120,141},{240,282,120,141},{360,282,120,141},{480,282,120,141},{600,282,120,141},{720,282,120,141},{840,282,120,141},{0,423,120,141},{120,423,120,141},{240,423,120,141},{360,423,120,141},{480,423,120,141},{600,423,120,141} };
        if(index<0 || index>=int(sizeof(spriteFrames_face_snobby)/sizeof(spriteFrames_face_snobby[0]))) index=0;
        return spriteFrames_face_snobby[index];
    }
    if (std::strcmp(sheet, "face_snobby_hmph")==0) {
        static const SDL_Rect spriteFrames_face_snobby_hmph[] = { {0,0,120,140},{120,0,120,140},{0,140,120,140},{120,140,120,140} };
        if(index<0 || index>=int(sizeof(spriteFrames_face_snobby_hmph)/sizeof(spriteFrames_face_snobby_hmph[0]))) index=0;
        return spriteFrames_face_snobby_hmph[index];
    }
    if (std::strcmp(sheet, "face_angry")==0) {
        static const SDL_Rect spriteFrames_face_angry[] = { {0,0,120,140},{120,0,120,140},{240,0,120,140},{360,0,120,140},{0,140,120,140},{120,140,120,140},{240,140,120,140},{360,140,120,140},{0,280,120,140},{120,280,120,140},{240,280,120,140},{360,280,120,140},{0,420,120,140},{120,420,120,140},{240,420,120,140},{360,420,120,140},{0,560,120,140},{120,560,120,140},{240,560,120,140},{360,560,120,140},{0,700,120,140},{120,700,120,140},{240,700,120,140},{360,700,120,140},{0,840,120,140},{120,840,120,140} };
        if(index<0 || index>=int(sizeof(spriteFrames_face_angry)/sizeof(spriteFrames_face_angry[0]))) index=0;
        return spriteFrames_face_angry[index];
    }
    if (std::strcmp(sheet, "happy_weirdo")==0) {
        static const SDL_Rect spriteFrames_happy_weirdo[] = { {0,0,120,140},{120,0,120,140},{240,0,120,140},{360,0,120,140},{0,140,120,140},{120,140,120,140},{240,140,120,140} };
        if(index<0 || index>=int(sizeof(spriteFrames_happy_weirdo)/sizeof(spriteFrames_happy_weirdo[0]))) index=0;
        return spriteFrames_happy_weirdo[index];
    }
    if (std::strcmp(sheet, "lovehat")==0) {
        static const SDL_Rect spriteFrames_lovehat[] = { {0,0,120,140} };
        if(index<0 || index>=int(sizeof(spriteFrames_lovehat)/sizeof(spriteFrames_lovehat[0]))) index=0;
        return spriteFrames_lovehat[index];
    }
    if (std::strcmp(sheet, "lover_shirt")==0) {
        static const SDL_Rect spriteFrames_lover_shirt[] = { {0,0,120,140},{120,0,120,140} };
        if(index<0 || index>=int(sizeof(spriteFrames_lover_shirt)/sizeof(spriteFrames_lover_shirt[0]))) index=0;
        return spriteFrames_lover_shirt[index];
    }
    if (std::strcmp(sheet, "lover_panic")==0) {
        static const SDL_Rect spriteFrames_lover_panic[] = { {0,0,124,202},{124,0,124,202} };
        if(index<0 || index>=int(sizeof(spriteFrames_lover_panic)/sizeof(spriteFrames_lover_panic[0]))) index=0;
        return spriteFrames_lover_panic[index];
    }
    if (std::strcmp(sheet, "gore")==0) {
        static const SDL_Rect spriteFrames_gore[] = { {0,0,30,30},{30,0,30,30},{0,30,30,30} };
        if(index<0 || index>=int(sizeof(spriteFrames_gore)/sizeof(spriteFrames_gore[0]))) index=0;
        return spriteFrames_gore[index];
    }
    if (std::strcmp(sheet, "gore_bodies")==0) {
        static const SDL_Rect spriteFrames_gore_bodies[] = { {0,0,161,121},{161,0,161,121},{322,0,161,121},{0,121,161,121},{161,121,161,121},{322,121,161,121},{0,242,161,121},{161,242,161,121},{322,242,161,121},{0,363,161,121},{161,363,161,121},{322,363,161,121} };
        if(index<0 || index>=int(sizeof(spriteFrames_gore_bodies)/sizeof(spriteFrames_gore_bodies[0]))) index=0;
        return spriteFrames_gore_bodies[index];
    }
    if (std::strcmp(sheet, "blood")==0) {
        static const SDL_Rect spriteFrames_blood[] = { {0,0,169,44},{0,44,169,44},{0,88,169,44} };
        if(index<0 || index>=int(sizeof(spriteFrames_blood)/sizeof(spriteFrames_blood[0]))) index=0;
        return spriteFrames_blood[index];
    }
    if (std::strcmp(sheet, "cricket")==0) {
        static const SDL_Rect spriteFrames_cricket[] = { {0,0,137,137},{0,137,137,137} };
        if(index<0 || index>=int(sizeof(spriteFrames_cricket)/sizeof(spriteFrames_cricket[0]))) index=0;
        return spriteFrames_cricket[index];
    }
    if (std::strcmp(sheet, "candlelight")==0) {
        static const SDL_Rect spriteFrames_candlelight[] = { {0,0,20,20},{20,0,20,20},{40,0,20,20},{0,20,20,20},{20,20,20,20} };
        if(index<0 || index>=int(sizeof(spriteFrames_candlelight)/sizeof(spriteFrames_candlelight[0]))) index=0;
        return spriteFrames_candlelight[index];
    }
    if (std::strcmp(sheet, "lovers_watching")==0) {
        static const SDL_Rect spriteFrames_lovers_watching[] = { {0,0,100,120},{0,120,100,120} };
        if(index<0 || index>=int(sizeof(spriteFrames_lovers_watching)/sizeof(spriteFrames_lovers_watching[0]))) index=0;
        return spriteFrames_lovers_watching[index];
    }
    if (std::strcmp(sheet, "cursor")==0) {
        static const SDL_Rect spriteFrames_cursor[] = { {0,0,100,100},{0,100,100,100},{0,200,100,100},{0,300,100,100},{0,400,100,100} };
        if(index<0 || index>=int(sizeof(spriteFrames_cursor)/sizeof(spriteFrames_cursor[0]))) index=0;
        return spriteFrames_cursor[index];
    }
    if (std::strcmp(sheet, "weapon_gun")==0) {
        static const SDL_Rect spriteFrames_weapon_gun[] = { {0,0,300,140},{300,0,300,140},{600,0,300,140},{0,140,300,140},{300,140,300,140},{600,140,300,140},{0,280,300,140},{300,280,300,140},{600,280,300,140},{0,420,300,140},{300,420,300,140},{600,420,300,140},{0,560,300,140},{300,560,300,140},{600,560,300,140},{0,700,300,140} };
        if(index<0 || index>=int(sizeof(spriteFrames_weapon_gun)/sizeof(spriteFrames_weapon_gun[0]))) index=0;
        return spriteFrames_weapon_gun[index];
    }
    if (std::strcmp(sheet, "weapon_axe")==0) {
        static const SDL_Rect spriteFrames_weapon_axe[] = { {0,0,300,140},{300,0,300,140},{600,0,300,140},{0,140,300,140},{300,140,300,140},{600,140,300,140},{0,280,300,140},{300,280,300,140},{600,280,300,140},{0,420,300,140},{300,420,300,140},{600,420,300,140},{0,560,300,140},{300,560,300,140},{600,560,300,140},{0,700,300,140} };
        if(index<0 || index>=int(sizeof(spriteFrames_weapon_axe)/sizeof(spriteFrames_weapon_axe[0]))) index=0;
        return spriteFrames_weapon_axe[index];
    }
    if (std::strcmp(sheet, "weapon_bat")==0) {
        static const SDL_Rect spriteFrames_weapon_bat[] = { {0,0,300,140},{300,0,300,140},{600,0,300,140},{0,140,300,140},{300,140,300,140},{600,140,300,140},{0,280,300,140},{300,280,300,140},{600,280,300,140},{0,420,300,140},{300,420,300,140},{600,420,300,140},{0,560,300,140},{300,560,300,140},{600,560,300,140},{0,700,300,140} };
        if(index<0 || index>=int(sizeof(spriteFrames_weapon_bat)/sizeof(spriteFrames_weapon_bat[0]))) index=0;
        return spriteFrames_weapon_bat[index];
    }
    if (std::strcmp(sheet, "weapon_shotgun")==0) {
        static const SDL_Rect spriteFrames_weapon_shotgun[] = { {0,0,300,140},{300,0,300,140},{600,0,300,140},{0,140,300,140},{300,140,300,140},{600,140,300,140},{0,280,300,140},{300,280,300,140},{600,280,300,140},{0,420,300,140},{300,420,300,140},{600,420,300,140},{0,560,300,140},{300,560,300,140},{600,560,300,140},{0,700,300,140} };
        if(index<0 || index>=int(sizeof(spriteFrames_weapon_shotgun)/sizeof(spriteFrames_weapon_shotgun[0]))) index=0;
        return spriteFrames_weapon_shotgun[index];
    }
    if (std::strcmp(sheet, "end_button")==0) {
        static const SDL_Rect spriteFrames_end_button[] = { {0,0,207,57},{0,57,207,57},{0,114,207,57},{0,171,207,57},{0,228,207,57} };
        if(index<0 || index>=int(sizeof(spriteFrames_end_button)/sizeof(spriteFrames_end_button[0]))) index=0;
        return spriteFrames_end_button[index];
    }
    return SDL_Rect{0,0,0,0};
}
