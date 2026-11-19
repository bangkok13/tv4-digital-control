#include "UIManager.h"
#include <cstdio>
#include <cstring>

namespace tv4 {
namespace {
constexpr uint32_t blue=0x65cff5, white=0xf1f6fa, muted=0xa9b9c5, amber=0xf6c662;
constexpr uint32_t surface=0x23313e, accent=0x235268;
void change(lv_obj_t* label,const char* text) {
    if (std::strcmp(lv_label_get_text(label),text)) lv_label_set_text(label,text);
}
void plateHandle(lv_obj_t* parent,int x,int y,LeverPos pos) {
    // The open circle is the pivot. The line points towards the free end.
    static const lv_point_t lines[][2]={
        {{11,11},{19,19}},{{11,11},{11,22}},{{11,11},{3,19}},
        {{11,11},{11,0}},{{11,11},{19,3}},{{11,11},{3,3}}
    };
    auto* line=lv_line_create(parent);
    lv_line_set_points(line,lines[unsigned(pos)],2);
    lv_obj_set_pos(line,x,y);
    lv_obj_set_style_line_width(line,3,0);
    lv_obj_set_style_line_color(line,lv_color_hex(white),0);
    lv_obj_set_style_line_rounded(line,true,0);
    auto* pivot=lv_obj_create(parent);
    lv_obj_remove_style_all(pivot);
    lv_obj_clear_flag(pivot,LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(pivot,9,9);
    lv_obj_set_pos(pivot,x+7,y+7);
    lv_obj_set_style_radius(pivot,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_border_width(pivot,2,0);
    lv_obj_set_style_border_color(pivot,lv_color_hex(blue),0);
    lv_obj_set_style_bg_opa(pivot,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(pivot,lv_color_hex(0x111820),0);
}
}

void UIManager::buildThreadView() {
    const auto& w=state_->workshop;
    const auto speed=threadSpeed(w);
    const auto pitch=pitchPosition(w.get(Input::Pitch));
    const bool internal=w.get(Input::Internal), left=w.get(Input::Left);
    char text[150],diameter[16],step[16];
    formatMillimetres(w.get(Input::ThreadDiameter),diameter,sizeof(diameter));
    formatMillimetres(w.get(Input::Pitch),step,sizeof(step));
    auto line=[&](const char* t,int y,uint32_t ink=white) { return caption(t,10,y,220,ink); };
    auto back=[&] { button(Key::Back,"Назад",10,220,104,44,surface); };
    switch (screen_) {
    case Screen::Thread:
        std::snprintf(text,sizeof(text),"Резьба  M%s x %s",diameter,step);
        line(text,0,blue);
        std::snprintf(text,sizeof(text),"%s, %s",internal ? "Внутренняя" : "Наружная",left ? "левая" : "правая");
        line(text,24,muted);
        if (speed && pitch) {
            std::snprintf(text,sizeof(text),"Г   Шаг (III): %s",positionName(*pitch));
            line(text,51);
            line("Д   Винт/вал: ВИНТ",76);
            line(left ? "В   Ход: П, вправо" : "В   Ход: Л, к патрону",101);
            std::snprintf(text,sizeof(text),speed->belowPlate ? "Расчёт %u об/мин, ниже 120" : "Цель   %u об/мин",speed->target);
            line(text,126,amber);
            line("Сверьте ряд 120 / 180 / 250",149,muted);
            button(Key::ThreadSetup,"Все рычаги",10,174,104,38,surface);
            button(Key::ThreadRpm,"Обороты",120,174,110,38,accent);
        } else {
            const char* why=!pitch ? "Штатно недоступно. Выберите шаг 0,8, 1 или 1,25 мм." :
                w.mm(Input::ThreadDiameter)>200 ? "Диаметр больше 200 мм. Проверьте вместимость станка." :
                "Шаг слишком велик для заданного диаметра.";
            caption(why,10,60,220,amber,true);
            button(Key::Plate,"Шильдик",10,174,220,38,surface);
        }
        back();
        button(Key::Inputs,"Параметры",120,220,110,44,accent);
        break;
    case Screen::ThreadSetup: {
        if (!speed) break;
        const auto& g=spindleGears[speed->gear];
        line("Рычаги для резьбы",0,blue);
        std::snprintf(text,sizeof(text),"А (I)    %s",positionName(g.first)); line(text,25);
        std::snprintf(text,sizeof(text),"Б (II)   %s",positionName(g.second)); line(text,47);
        line("Ж   вытянуть на себя",68);
        line("З   самоход вниз от себя",89);
        line("И   вверх при настройке",110);
        line("       вниз для прохода",131);
        line("Коробки: только на останове",152,amber);
        button(Key::Controls,"На рисунке",10,174,104,38,surface);
        button(Key::Plate,"Шильдик",120,174,110,38,surface);
        back();
        button(Key::ThreadRpm,"Обороты",120,220,110,44,accent);
        break;
    }
    case Screen::ThreadRpm: {
        if (!speed) break;
        line("Обороты и ручка",0,blue);
        std::snprintf(text,sizeof(text),speed->belowPlate ? "Ниже ступени: %u об/мин" : "Цель   %u об/мин",speed->target);
        line(text,25,amber);
        std::snprintf(text,sizeof(text),"Расчёт %u при %.1f м/мин",speed->calculated,speed->metresPerMinute);
        line(text,49,muted);
        std::snprintf(text,sizeof(text),"Ступень на шильдике   %u",spindleGears[speed->gear].rpm);
        line(text,73);
        rowValues_[0]=line("",98);
        rowValues_[1]=line("",120,blue);
        note_=caption("",10,142,220,amber,true);
        button(Key::Plate,"Шильдик",10,184,104,32,surface);
        button(Key::Dial,"Шкала ручки",120,184,110,32,surface);
        back();
        button(Key::ThreadSetup,"Все рычаги",120,220,110,44,accent);
        break;
    }
    case Screen::Dial:
        if (!speed) break;
        std::snprintf(text,sizeof(text),"Шкала ручки / ступень %u",spindleGears[speed->gear].rpm);
        line(text,0,blue);
        line("Введите метку вашей ручки",24,muted);
        button(Key::DialMinus,"-5",10,50,58,33,surface);
        rowValues_[0]=caption("",75,56,90,white);
        lv_obj_set_style_text_align(rowValues_[0],LV_TEXT_ALIGN_CENTER,0);
        button(Key::DialPlus,"+5",172,50,58,33,surface);
        for (unsigned i=0;i<2;++i) {
            button(i ? Key::PointB : Key::PointA,"",10,90+40*i,220,36,accent);
            auto* btn=lv_obj_get_child(body_,-1);
            lv_obj_set_style_opa(btn,LV_OPA_60,LV_STATE_DISABLED);
            rowValues_[i+1]=lv_obj_get_child(btn,0);
        }
        rowValues_[3]=line("",174);
        note_=line("",196,amber);
        back();
        break;
    case Screen::ThreadInfo: {
        line("Размеры профиля, мм",0,blue);
        const auto t=threadGeometry(w.mm(Input::ThreadDiameter),w.mm(Input::Pitch));
        if (t) {
            std::snprintf(text,sizeof(text),"Высота треугольника   %.3f",t->height); line(text,30);
            std::snprintf(text,sizeof(text),"Средний диаметр   %.3f",t->pitchDiameter); line(text,60);
            line(internal ? "Малый диаметр отверстия" : "Диаметр дна впадины",90);
            std::snprintf(text,sizeof(text),"%.3f мм",internal ? t->internalMinor : t->externalRoot); line(text,112);
            if (internal) {
                std::snprintf(text,sizeof(text),"Сверло под метчик ~ %.2f",t->drill);
                line(text,143);
            }
            caption("Базовые размеры. Допуск задаёт чертёж детали.",10,171,220,muted,true);
        } else line("Проверьте диаметр и шаг.",43,amber);
        back();
        break;
    }
    case Screen::Plate: {
        const bool threadPlate=plateOrigin_==Screen::Thread || plateOrigin_==Screen::ThreadSetup ||
            plateOrigin_==Screen::ThreadRpm || (plateOrigin_==Screen::Controls && controlOrigin_==Screen::ThreadSetup);
        const bool feedPlate=plateOrigin_==Screen::Controls && controlOrigin_==Screen::Feed;
        line(platePage_ ? "Шильдик / шаг и подача" : "Шильдик / скорости",0,blue);
        if (!platePage_) {
            line("Ростов, ряд 120 / 180 / 250",22,muted);
            caption("А (I)",14,48,55); caption("Б (II)",82,48,58); caption("об/мин",153,48,74);
            for (unsigned i=0;i<6;++i) {
                const int y=72+21*i;
                const auto& g=spindleGears[i];
                plateHandle(body_,20,y-4,g.first); plateHandle(body_,91,y-4,g.second);
                std::snprintf(text,sizeof(text),"%u",g.rpm);
                caption(text,157,y,62,threadPlate && speed && i==speed->gear ? amber : white);
            }
        } else {
            line("Г (III), вид спереди",25,muted);
            caption("Шаг, мм",10,54,78); caption("мм/об",99,54,65); caption("Ручка",171,54,58);
            for (unsigned i=0;i<3;++i) {
                const int y=84+34*i;
                formatMillimetres(pitches[i],text,sizeof(text));
                caption(text,15,y,75,threadPlate && w.get(Input::Pitch)==pitches[i] ? amber : white);
                formatMillimetres(feeds[i],text,sizeof(text));
                caption(text,98,y,62,feedPlate && w.get(Input::Feed)==i ? amber : white);
                plateHandle(body_,181,y-2,*pitchPosition(pitches[i]));
            }
            line("ВИНТ: резьба   ВАЛ: подача",182,muted);
        }
        back();
        button(Key::Next,platePage_ ? "Скорости" : "Шаг / подача",120,220,110,44,accent);
        break;
    }
    default: break;
    }
}

void UIManager::renderThreadView(const MachineState& s) {
    const auto speed=threadSpeed(s.workshop);
    if (!speed) return;
    char text[120];
    const auto measured=liveSpindleRpm(s);
    if (measured) std::snprintf(text,sizeof(text),"Датчик LIVE   %u об/мин",*measured);
    else std::snprintf(text,sizeof(text),"%s",s.settings.mode==Mode::Demo ? "DEMO: включите LIVE" : "LIVE: нет импульсов");
    const auto& pair=s.dial.gears[speed->gear];
    if (screen_==Screen::Dial) {
        change(rowValues_[3],text);
        std::snprintf(text,sizeof(text),"%u %%",dialPercent_); change(rowValues_[0],text);
        for (unsigned i=0;i<2;++i) {
            const auto p=pair.points[i];
            if (p.rpm) std::snprintf(text,sizeof(text),"Точка %u: %u %% / %u об/мин",i+1,p.percent,p.rpm);
            else std::snprintf(text,sizeof(text),"Записать точку %u",i+1);
            change(rowValues_[1+i],text);
            auto* btn=lv_obj_get_parent(rowValues_[1+i]);
            if (measured) lv_obj_clear_state(btn,LV_STATE_DISABLED);
            else lv_obj_add_state(btn,LV_STATE_DISABLED);
        }
        const char* message=dialMessage_[0] ? dialMessage_ : "Снимите две разные метки.";
        change(note_,measured ? message : "Для записи нужен LIVE.");
        return;
    }
    change(rowValues_[0],text);
    const auto percent=pair.percentFor(speed->target);
    if (percent) std::snprintf(text,sizeof(text),"Ручка ~ %u %% по замерам",*percent);
    else std::snprintf(text,sizeof(text),"%s",speed->belowPlate ? "Ручка: вне замеров" : "Ручка: по тахометру LIVE");
    change(rowValues_[1],text);
    const char* hint="Сверьте цель по датчику.";
    if (speed->belowPlate) hint="Ниже 120. Сверьте диапазон ПЧ и двигателя.";
    else if (measured && *measured<speed->target) hint="Факт ниже цели.";
    else if (measured && *measured>speed->target) hint="Факт выше цели.";
    else if (measured) hint="Обороты на цели.";
    change(note_,hint);
}
}
