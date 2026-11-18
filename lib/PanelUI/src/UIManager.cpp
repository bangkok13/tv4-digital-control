#include "UIManager.h"
#include "version.h"
#include <cstdio>
#include <cstring>

extern "C" {
LV_FONT_DECLARE(tv4_text_15);
LV_FONT_DECLARE(tv4_title_23);
}
namespace tv4 {
namespace {
constexpr uint32_t bg=0x111820, surface=0x23313e, white=0xf1f6fa, muted=0xa9b9c5,
                   blue=0x65cff5, green=0x7ad69b, amber=0xf6c662, red=0xff7878;
constexpr uint32_t splashDurationMs = 7000;
lv_color_t color(uint32_t c) { return lv_color_hex(c); }
lv_obj_t* label(lv_obj_t* parent, const char* text, int x, int y, int w,
                uint32_t ink=white, lv_text_align_t align=LV_TEXT_ALIGN_LEFT,
                const lv_font_t* font=&tv4_text_15) {
    auto* obj = lv_label_create(parent);
    lv_obj_set_pos(obj,x,y);
    lv_obj_set_width(obj,w);
    lv_label_set_long_mode(obj,LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(obj,font,0);
    lv_obj_set_style_text_color(obj,color(ink),0);
    lv_obj_set_style_text_align(obj,align,0);
    lv_label_set_text(obj,text);
    return obj;
}
void set(lv_obj_t* obj,const char* text) {
    if (std::strcmp(lv_label_get_text(obj),text)) lv_label_set_text(obj,text);
}
void ink(lv_obj_t* obj,uint32_t value) {
    if (lv_obj_get_style_text_color(obj,LV_PART_MAIN).full!=color(value).full)
        lv_obj_set_style_text_color(obj,color(value),0);
}
void duration(char* out, size_t size, uint64_t us) {
    const auto sec=us/1000000;
    std::snprintf(out,size,"%llu:%02u:%02u",static_cast<unsigned long long>(sec/3600),
        unsigned(sec/60%60),unsigned(sec%60));
}
const char* signalText(Signal s) {
    switch(s) {
    case Signal::Waiting: return "Ожидание импульсов";
    case Signal::FirstPulse: return "Первый импульс";
    case Signal::Tracking: return "Есть импульсы";
    case Signal::Silent: return "Нет импульсов";
    case Signal::Overrun: return "Потеря импульсов";
    }
    return "Н/Д";
}
void number(char* out,size_t size,uint64_t n) {
    std::snprintf(out,size,"%llu",static_cast<unsigned long long>(n));
}
void drawSplash(lv_obj_t* body) {
    constexpr uint32_t brass = 0xffbf51;
    label(body, LV_SYMBOL_CHARGE, 0, 7, 240, brass, LV_TEXT_ALIGN_CENTER, &lv_font_montserrat_48);
    label(body, "6 VOLT", 0, 67, 240, white, LV_TEXT_ALIGN_CENTER, &lv_font_montserrat_48);

    auto* garage = label(body, "GARAGE", 0, 127, 240, brass, LV_TEXT_ALIGN_CENTER, &lv_font_montserrat_20);
    lv_obj_set_style_text_letter_space(garage, 7, 0);

    auto* greeting = label(body, "С ДР, БРО!", 42, 177, 156, white, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_border_side(greeting, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(greeting, color(brass), 0);
    lv_obj_set_style_border_width(greeting, 1, 0);
    lv_obj_set_style_pad_top(greeting, 15, 0);
    lv_obj_set_style_text_letter_space(greeting, 2, 0);
}
}
bool UIManager::splashExpired() const {
    return screen_ == Screen::Splash && lv_tick_elaps(splashStarted_) >= splashDurationMs;
}
void UIManager::begin() {
    auto* root=lv_scr_act();
    lv_obj_remove_style_all(root);
    lv_obj_set_style_bg_color(root,color(bg),0);
    lv_obj_set_style_bg_opa(root,LV_OPA_COVER,0);
    lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);
    title_=label(root,"",12,12,184);
    badge_=label(root,"",200,10,28,amber,LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_bg_color(badge_,color(surface),0);
    lv_obj_set_style_bg_opa(badge_,LV_OPA_COVER,0);
    lv_obj_set_style_radius(badge_,5,0);
    lv_obj_set_style_pad_ver(badge_,3,0);
    body_=lv_obj_create(root);
    lv_obj_remove_style_all(body_);
    lv_obj_set_pos(body_,0,46);
    lv_obj_set_size(body_,240,274);
    lv_obj_clear_flag(body_,LV_OBJ_FLAG_SCROLLABLE);
    build();
    splashStarted_ = lv_tick_get();
}
void UIManager::button(Key key,const char* caption,int x,int y,int w,int h,uint32_t fill) {
    auto* btn=lv_btn_create(body_);
    lv_obj_remove_style_all(btn);
    lv_obj_set_pos(btn,x,y);
    lv_obj_set_size(btn,w,h);
    lv_obj_set_style_radius(btn,7,0);
    lv_obj_set_style_bg_opa(btn,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(btn,color(fill),0);
    lv_obj_set_style_bg_color(btn,color(0x426078),LV_STATE_PRESSED);
    auto* text=label(btn,caption,0,0,w,white,LV_TEXT_ALIGN_CENTER);
    lv_obj_center(text);
    if (key==Key::FieldNext) fieldName_=text;
    lv_obj_set_user_data(btn,reinterpret_cast<void*>(static_cast<uintptr_t>(key)));
    lv_obj_add_event_cb(btn,clicked,LV_EVENT_CLICKED,this);
}
void UIManager::clicked(lv_event_t* e) {
    auto* self=static_cast<UIManager*>(lv_event_get_user_data(e));
    auto key=static_cast<Key>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(lv_event_get_target(e))));
    self->navigate(key);
}
void UIManager::navigate(Key key) {
    switch(key) {
    case Key::Home: screen_=Screen::Home; break;
    case Key::Settings: screen_=Screen::Settings; break;
    case Key::Stats: screen_=Screen::Stats; break;
    case Key::Diagnostics: screen_=Screen::Diagnostics; secondPage_=false; break;
    case Key::Confirm: origin_=screen_; screen_=Screen::ConfirmReset; resetError_=false; break;
    case Key::Cancel: screen_=origin_; break;
    case Key::Yes: pending_={Action::Reset,field_}; screen_=origin_; break;
    case Key::Mode: pending_={Action::ToggleMode,field_}; return;
    case Key::Inc: pending_={Action::Increase,field_}; return;
    case Key::Dec: pending_={Action::Decrease,field_}; return;
    case Key::Cal: pending_={Action::Calibrate,field_}; return;
    case Key::FieldNext: field_=static_cast<Field>((unsigned(field_)+1)%unsigned(Field::Count)); fieldChanged_=true; return;
    case Key::More: secondPage_=!secondPage_; break;
    }
    rebuild_=true;
}
Command UIManager::take() { const auto c=pending_; pending_={}; return c; }
void UIManager::resetFailed() { screen_=Screen::ConfirmReset; resetError_=rebuild_=true; }
void UIManager::rows(unsigned count,int firstY,int step) {
    for (unsigned i=0;i<count;++i) {
        const int y=firstY+int(i)*step;
        const int split=screen_==Screen::Diagnostics ? 105 : 126;
        rowLabels_[i]=label(body_,"",12,y,split-13,muted);
        rowValues_[i]=label(body_,"",split,y,228-split,white,LV_TEXT_ALIGN_RIGHT);
    }
}
void UIManager::build() {
    lv_obj_clean(body_);
    rebuild_=false;
    if (screen_!=Screen::Splash) {
        const bool home = screen_==Screen::Home;
        set(title_,home ? "TV-4 Шкильнык" : "TV-4 DIGITAL");
        lv_obj_set_y(title_,home ? 7 : 12);
        lv_obj_set_style_text_font(title_,home ? &tv4_title_23 : &lv_font_montserrat_20,0);
    }
    switch(screen_) {
    case Screen::Splash:
        drawSplash(body_);
        break;
    case Screen::Home:
        label(body_,"ОБОРОТЫ ШПИНДЕЛЯ",12,5,216,muted);
        rpm_=label(body_,"",12,32,168,white,LV_TEXT_ALIGN_LEFT,&lv_font_montserrat_48);
        unit_=label(body_,"RPM",171,61,57,blue,LV_TEXT_ALIGN_RIGHT,&lv_font_montserrat_20);
        bar_=lv_bar_create(body_);
        lv_obj_set_pos(bar_,12,104); lv_obj_set_size(bar_,216,9);
        lv_obj_set_style_bg_color(bar_,color(surface),LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar_,LV_OPA_COVER,LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar_,color(blue),LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(bar_,LV_OPA_COVER,LV_PART_INDICATOR);
        label(body_,"0",12,116,35,muted);
        limit_=label(body_,"",136,116,92,muted,LV_TEXT_ALIGN_RIGHT);
        activity_=lv_obj_create(body_);
        lv_obj_remove_style_all(activity_);
        lv_obj_set_pos(activity_,13,157); lv_obj_set_size(activity_,10,10);
        lv_obj_set_style_radius(activity_,LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_bg_opa(activity_,LV_OPA_COVER,0);
        signal_=label(body_,"",32,151,196,green);
        label(body_,"Сеанс",12,185,80,muted);
        time_=label(body_,"",92,185,136,white,LV_TEXT_ALIGN_RIGHT);
        button(Key::Settings,"Настройки",10,220,106,44,surface);
        button(Key::Stats,"Статистика",122,220,108,44,surface);
        break;
    case Screen::Settings:
        label(body_,"Настройки",12,0,216,blue);
        button(Key::Mode,"Сменить DEMO / LIVE",10,26,220,40,surface);
        button(Key::FieldNext,"",10,72,220,40,surface);
        button(Key::Dec,"−",10,118,49,42,surface);
        value_=label(body_,"",63,129,114,white,LV_TEXT_ALIGN_CENTER);
        button(Key::Inc,"+",181,118,49,42,surface);
        button(Key::Confirm,"Сброс статистики",10,170,220,40,0x493137);
        button(Key::Home,"Главная",10,220,96,44,surface);
        button(Key::Diagnostics,"Диагностика",112,220,118,44,surface);
        break;
    case Screen::Stats:
        label(body_,"Статистика вращения",12,0,216,blue);
        rows(5,36,34);
        button(Key::Home,"Главная",10,220,106,44,surface);
        button(Key::Confirm,"Сбросить",122,220,108,44,0x493137);
        break;
    case Screen::Diagnostics:
        label(body_,secondPage_ ? "Диагностика  2/2" : "Диагностика  1/2",12,0,216,blue);
        rows(6,28,23);
        button(Key::Cal,"Калибровка тача",10,170,220,40,surface);
        button(Key::Settings,"Назад",10,220,106,44,surface);
        button(Key::More,secondPage_ ? "Стр. 1" : "Стр. 2",122,220,108,44,surface);
        break;
    case Screen::ConfirmReset:
        label(body_,"Сброс статистики",12,12,216,red);
        confirm_=label(body_,"",12,55,216);
        lv_label_set_long_mode(confirm_,LV_LABEL_LONG_WRAP);
        button(Key::Cancel,"Отмена",10,220,106,44,surface);
        button(Key::Yes,"Сбросить",122,220,108,44,0x713a40);
        break;
    }
}
void UIManager::renderRows(const MachineState& s) {
    char val[48]{};
    auto row=[&](int n,const char* name,const char* value) {
        set(rowLabels_[n],name); set(rowValues_[n],value);
    };
    const auto& d=s.diagnostic;
    if (screen_==Screen::Stats) {
        duration(val,sizeof(val),s.sessionUs); row(0,"Текущий цикл",val);
        duration(val,sizeof(val),s.totals.runningUs); row(1,"Моточасы",val);
        std::snprintf(val,sizeof(val),"%u RPM",unsigned(s.totals.peakRpm)); row(2,"Максимум",s.totals.runningUs ? val : "Н/Д");
        std::snprintf(val,sizeof(val),"%.0f RPM",s.totals.average()); row(3,"Среднее",s.totals.runningUs ? val : "Н/Д");
        number(val,sizeof(val),s.totals.starts); row(4,"Пуски",val);
    } else if (!secondPage_) {
        row(0,"Чип",d.chip);
        number(val,sizeof(val),d.revision); row(1,"Ревизия ESP",val);
        std::snprintf(val,sizeof(val),"%u / %u КБ",unsigned(d.freeHeap/1024),unsigned(d.minHeap/1024)); row(2,"ОЗУ / мин.",val);
        std::snprintf(val,sizeof(val),"%u КБ",unsigned(d.flashBytes/1024)); row(3,"Flash",val);
        duration(val,sizeof(val),s.uptimeUs); row(4,"Аптайм",val);
        row(5,"Сброс",d.resetReason);
    } else {
        row(0,"Тач",!d.touchBus ? "Ошибка SPI" : d.touchSeen ? (d.calibrated ? "Калиброван" : "Есть отклик") : "Нет касаний");
        const char* rpmState = s.motion.signal==Signal::Tracking ? "Импульсы" :
              s.motion.signal==Signal::Overrun ? "Потеря" : s.motion.signal==Signal::FirstPulse ? "Первый" : "Нет данных";
        row(1,"Датчик RPM",s.settings.mode==Mode::Demo ? "Симуляция" : rpmState);
        number(val,sizeof(val),s.motion.pulses); row(2,"Импульсы",val);
        row(3,"RS-485",s.drive.connected ? "Подключено" : "Не подключено");
        row(4,"NVS",d.storageOK ? "Готово" : "Ошибка");
        ink(rowValues_[4],d.storageOK ? white : red);
        row(5,"Прошивка",TV4_VERSION);
    }
}
void UIManager::update(const MachineState& s) {
    if (splashExpired()) navigate(Key::Home);
    if (rebuild_) build();
    fieldChanged_=false;
    const bool demo=s.settings.mode==Mode::Demo;
    set(badge_,demo ? "D" : "L"); ink(badge_,demo ? amber : green);
    char buf[80];
    switch(screen_) {
    case Screen::Splash: break;
    case Screen::Home: {
        const lv_font_t* font=s.motion.hasRpm ? &lv_font_montserrat_48 : &tv4_text_15;
        if (lv_obj_get_style_text_font(rpm_,LV_PART_MAIN)!=font) lv_obj_set_style_text_font(rpm_,font,0);
        if (s.motion.hasRpm) {
            std::snprintf(buf,sizeof(buf),"%.0f",s.motion.rpm); set(rpm_,buf);
        } else set(rpm_,"Н/Д");
        int scale=s.settings.get(Field::Scale);
        if (lv_bar_get_max_value(bar_)!=scale) lv_bar_set_range(bar_,0,scale);
        lv_bar_set_value(bar_,s.motion.hasRpm ? std::min<int>(std::lround(s.motion.rpm),scale) : 0,LV_ANIM_OFF);
        std::snprintf(buf,sizeof(buf),"%d RPM",scale); set(limit_,buf);
        set(signal_,demo ? (s.motion.rotating ? "Симуляция вращения" : "Пауза симуляции") : signalText(s.motion.signal));
        ink(signal_,s.motion.signal==Signal::Overrun ? red : s.motion.rotating ? green : amber);
        const bool recent=s.motion.pulses && s.uptimeUs>=s.motion.lastEdgeUs && s.uptimeUs-s.motion.lastEdgeUs<180000;
        const auto dot=color(recent ? green : surface);
        if (lv_obj_get_style_bg_color(activity_,LV_PART_MAIN).full!=dot.full) lv_obj_set_style_bg_color(activity_,dot,0);
        duration(buf,sizeof(buf),s.uptimeUs); set(time_,buf);
        break;
    }
    case Screen::Settings: {
        const auto& f=fields[unsigned(field_)];
        std::snprintf(buf,sizeof(buf),"%s  >",f.label); set(fieldName_,buf);
        std::snprintf(buf,sizeof(buf),"%d %s",s.settings.get(field_),f.unit); set(value_,buf);
        break;
    }
    case Screen::Stats: case Screen::Diagnostics: renderRows(s); break;
    case Screen::ConfirmReset:
        set(confirm_,resetError_ ? "Ошибка записи NVS. Статистика сохранена в памяти. Повторить сброс?" :
            demo ? "Обнулить демонстрационные значения?" : "Обнулить записанные моточасы, максимум и число пусков LIVE?");
        break;
    }
}
}
