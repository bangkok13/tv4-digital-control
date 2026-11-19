#include "UIManager.h"
#include "version.h"
#include <cstdio>
#include <cstring>

extern "C" {
LV_IMG_DECLARE(tv4_head);
LV_IMG_DECLARE(tv4_feedbox);
LV_IMG_DECLARE(tv4_apron);
}
namespace tv4 {
namespace {
constexpr uint32_t surface = 0x23313e, accent = 0x235268, blue = 0x65cff5, amber = 0xf6c662;
constexpr unsigned windows[]{30, 120, 600};
constexpr const char *groupNames[]{"Передняя бабка", "Коробка подач", "Фартук"};
const lv_img_dsc_t *drawings[]{&tv4_head, &tv4_feedbox, &tv4_apron};
constexpr unsigned firstControl[]{0,3,5}, groupCount[]{3,2,4};
// Centres on the handles inside the 224 x 112 crops of factory figure 1.
constexpr lv_point_t handleCentres[][4]={
    {{118,80},{178,80},{78,80},{}}, {{123,22},{68,22},{},{}},
    {{41,73},{84,32},{123,91},{177,53}}
};
constexpr Input turning[]{Input::Diameter, Input::Material, Input::Tool, Input::Speed};
constexpr Input threading[]{Input::ThreadDiameter, Input::Pitch,    Input::Internal,
                            Input::Left,           Input::Material, Input::Tool};
constexpr Input rough[]{Input::Start, Input::Finish, Input::Depth, Input::Allowance};
constexpr Input feed[]{Input::Feed};
struct Form {
    const Input *inputs;
    unsigned count;
};
Form form(Screen s) {
    switch (s) {
    case Screen::Thread:
        return {threading, 6};
    case Screen::Rough:
        return {rough, 4};
    case Screen::Feed:
        return {feed, 1};
    default:
        return {turning, 4};
    }
}
const char *name(Screen s) {
    switch (s) {
    case Screen::Turning:
        return "Точение";
    case Screen::Thread:
        return "Резьбы";
    case Screen::Rough:
        return "Проточка";
    case Screen::Feed:
        return "Подачи";
    default:
        return "Параметры";
    }
}
void updateText(lv_obj_t *obj, const char *t) {
    if (std::strcmp(lv_label_get_text(obj), t))
        lv_label_set_text(obj, t);
}
void elapsed(char *dest, size_t size, uint64_t us) {
    const auto s = us / 1000000;
    std::snprintf(dest, size, "%llu:%02u:%02u", (unsigned long long)(s / 3600), unsigned(s / 60 % 60),
                  unsigned(s % 60));
}
} // namespace
void UIManager::workshopKey(Key key) {
    const auto &w = state_->workshop;
    if (key >= Key::Digit0 && key <= Key::Digit9) {
        size_t len = std::strlen(digits_);
        if (len + 1 < sizeof(digits_)) {
            digits_[len] = char('0' + int(key) - int(Key::Digit0));
            digits_[len + 1] = 0;
        }
        numberError_ = false;
        fieldChanged_ = true;
        return;
    }
    if (key >= Key::Slot0 && key <= Key::Slot8) {
        unsigned i = unsigned(key) - unsigned(Key::Slot0);
        if (screen_ == Screen::Controls && i < groupCount[controlGroup_]) {
            controlIndex_ = firstControl[controlGroup_] + i;
            screen_ = Screen::ControlCard;
        } else if (screen_ == Screen::Inputs && i < form(calc_).count) {
            edit_ = form(calc_).inputs[i];
            if (choice(edit_, w.get(edit_))) {
                pending_ = {Action::SetInput, Field::Brightness, edit_,
                            int((w.get(edit_) + 1) % (inputSpecs[unsigned(edit_)].high + 1))};
                fieldChanged_ = true;
                return;
            }
            digits_[0] = 0;
            numberError_ = false;
            screen_ = Screen::Number;
        }
    } else
        switch (key) {
        case Key::Menu:
            screen_ = Screen::Menu;
            break;
        case Key::SettingsExtra:
            screen_ = Screen::SettingsExtra;
            break;
        case Key::WorkSettings:
        case Key::ThreadSettings:
            calc_ = key == Key::WorkSettings ? Screen::Turning : Screen::Thread;
            screen_ = Screen::Inputs;
            break;
        case Key::Turning:
            calc_ = screen_ = Screen::Turning;
            break;
        case Key::Thread:
            calc_ = screen_ = Screen::Thread;
            break;
        case Key::Rough:
            calc_ = screen_ = Screen::Rough;
            break;
        case Key::Feed:
            calc_ = screen_ = Screen::Feed;
            break;
        case Key::Controls:
            controlOrigin_ = screen_;
            threadGuide_ = screen_ != Screen::Feed;
            controlGroup_ = screen_==Screen::Feed ? 1 : 0;
            screen_ = Screen::Controls;
            break;
        case Key::ThreadSetup:
        case Key::ThreadRpm:
            if (!threadSpeed(w)) return;
            screen_=key==Key::ThreadSetup ? Screen::ThreadSetup : Screen::ThreadRpm;
            break;
        case Key::Plate:
            plateOrigin_=screen_;
            platePage_=(screen_==Screen::Thread || (screen_==Screen::Controls && controlGroup_==1)) ? 1 : 0;
            screen_=Screen::Plate;
            break;
        case Key::Dial:
            dialMessage_="";
            dialPercent_=50;
            screen_=Screen::Dial;
            break;
        case Key::DialMinus:
        case Key::DialPlus:
            dialPercent_=unsigned(std::clamp(int(dialPercent_)+(key==Key::DialPlus ? 5 : -5),0,100));
            dialMessage_="";
            fieldChanged_=true;
            return;
        case Key::PointA:
        case Key::PointB: {
            const auto setup=threadSpeed(w);
            if (!setup) return;
            if (!liveSpindleRpm(*state_)) return;
            pending_.action=Action::RecordDial;
            pending_.gear=setup->gear;
            pending_.point=key==Key::PointB ? 1 : 0;
            pending_.value=int(dialPercent_);
            return;
        }
        case Key::Graphs:
            screen_ = Screen::Graphs;
            break;
        case Key::Inputs:
            screen_ = Screen::Inputs;
            break;
        case Key::Demo:
            screen_ = Screen::Demo;
            break;
        case Key::Network:
            screen_ = Screen::Network;
            break;
        case Key::History:
            screen_ = Screen::History;
            break;
        case Key::ThreadInfo:
            screen_ = Screen::ThreadInfo;
            break;
        case Key::Auto:
            pending_.action = Action::DemoSpeed;
            pending_.value = -1;
            return;
        case Key::Portal:
            pending_.action = Action::Portal;
            return;
        case Key::CloseNet:
            pending_.action = Action::CloseNetwork;
            return;
        case Key::Arm:
            screen_ = Screen::ConfirmOTA;
            break;
        case Key::AcceptOTA:
            pending_.action = Action::ArmOTA;
            screen_ = Screen::Network;
            break;
        case Key::Prev:
        case Key::Next:
            if (screen_ == Screen::Controls)
                controlGroup_ = (controlGroup_ + (key == Key::Next ? 1 : 2)) % 3;
            if (screen_ == Screen::Plate) platePage_=1-platePage_;
            if (screen_ == Screen::Graphs)
                graphWindow_ = (graphWindow_ + 1) % 3;
            break;
        case Key::Save: {
            auto v = parseInput(edit_, digits_);
            if (!v) {
                numberError_ = true;
                fieldChanged_ = true;
                return;
            }
            pending_ = {Action::SetInput, Field::Brightness, edit_, int(*v)};
            screen_ = Screen::Inputs;
            break;
        }
        case Key::Erase:
            if (auto len = std::strlen(digits_))
                digits_[len - 1] = 0;
            numberError_ = false;
            fieldChanged_ = true;
            return;
        case Key::Dot:
            if (!std::strchr(digits_, '.') && std::strlen(digits_) + 1 < sizeof(digits_))
                std::strcat(digits_, ".");
            fieldChanged_ = true;
            return;
        case Key::Back:
            switch (screen_) {
            case Screen::Inputs:
                screen_ = calc_;
                break;
            case Screen::Number:
                screen_ = Screen::Inputs;
                break;
            case Screen::Controls:
                screen_ = controlOrigin_;
                break;
            case Screen::ControlCard:
                screen_ = Screen::Controls;
                break;
            case Screen::History:
                screen_ = Screen::Stats;
                break;
            case Screen::ConfirmOTA:
                screen_ = Screen::Network;
                break;
            case Screen::ThreadInfo:
                screen_=Screen::Inputs;
                break;
            case Screen::ThreadSetup:
            case Screen::ThreadRpm:
                screen_ = Screen::Thread;
                break;
            case Screen::Dial: screen_=Screen::ThreadRpm; break;
            case Screen::Plate: screen_=plateOrigin_; break;
            default:
                screen_ = Screen::Menu;
                break;
            }
            break;
        default:
            return;
        }
    rebuild_ = true;
}
void UIManager::sliderChanged(lv_event_t *e) {
    auto *ui = static_cast<UIManager *>(lv_event_get_user_data(e));
    ui->pending_.action = Action::DemoSpeed;
    ui->pending_.value = lv_slider_get_value(lv_event_get_target(e));
}
void UIManager::buildWorkshop() {
    auto back = [&] { button(Key::Back, "Назад", 10, 220, 104, 44, surface); };
    auto heading = [&](const char *t) { caption(t, 10, 0, 220, blue); };
    switch (screen_) {
    case Screen::Menu: {
        heading("Меню");
        const Key keys[] = {Key::Graphs, Key::Stats,    Key::Turning, Key::Thread,  Key::Rough,
                            Key::Feed,   Key::Controls, Key::Demo,    Key::Network, Key::Settings};
        const char *labels[] = {"Графики", "Статистика",  "Точение", "Резьбы",      "Проточка",
                                "Подачи",  "Рычаги ТВ-4", "DEMO",    "Wi-Fi / OTA", "Настройки"};
        for (unsigned i = 0; i < 10; ++i)
            button(keys[i], labels[i], 10 + 112 * (i % 2), 24 + 39 * (i / 2), 108, 36, surface);
        button(Key::Home, "Главная", 10, 223, 220, 41, accent);
        break;
    }
    case Screen::SettingsExtra:
        heading("Настройки мастерской");
        button(Key::WorkSettings, "Диаметр, материал, резец", 10, 27, 220, 39, surface);
        button(Key::ThreadSettings, "Последняя резьба", 10, 72, 220, 39, surface);
        button(Key::Network, "Wi-Fi и обновление OTA", 10, 117, 220, 39, surface);
        button(Key::Cal, "Калибровка тача", 10, 162, 220, 39, surface);
        button(Key::Settings, "Назад", 10, 220, 104, 44, surface);
        button(Key::Demo, "DEMO", 120, 220, 110, 44, accent);
        break;
    case Screen::Graphs:
        heading("История RPM");
        chart_ = lv_chart_create(body_);
        lv_obj_set_pos(chart_, 10, 28);
        lv_obj_set_size(chart_, 220, 124);
        lv_obj_set_style_bg_color(chart_, lv_color_hex(surface), 0);
        lv_obj_set_style_bg_opa(chart_, LV_OPA_COVER, 0);
        lv_obj_set_style_line_color(chart_, lv_color_hex(0x364755), 0);
        lv_obj_set_style_size(chart_, 0, LV_PART_INDICATOR);
        lv_chart_set_type(chart_, LV_CHART_TYPE_LINE);
        lv_chart_set_div_line_count(chart_, 4, 5);
        series_ = lv_chart_add_series(chart_, lv_color_hex(blue), LV_CHART_AXIS_PRIMARY_Y);
        plottedSecond_ = UINT64_MAX;
        note_ = caption("", 10, 160, 220, 0xf1f6fa, true);
        back();
        button(Key::Next,
               graphWindow_ == 0   ? "30 сек"
               : graphWindow_ == 1 ? "2 мин"
                                   : "10 мин",
               120, 220, 110, 44, accent);
        break;
    case Screen::Turning:
    case Screen::Rough:
    case Screen::Feed:
        heading(name(screen_));
        dataRows(4, 25, 28);
        note_ = caption("", 10, 139, 220, amber, true);
        if (screen_ == Screen::Feed) button(Key::Controls,"Рычаги ТВ-4",10,176,220,38,surface);
        back();
        button(Key::Inputs, "Параметры", 120, 220, 110, 44, accent);
        break;
    case Screen::Inputs: {
        heading("Нажмите для изменения");
        const auto f = form(calc_);
        for (unsigned i = 0; i < f.count; ++i) {
            button(static_cast<Key>(unsigned(Key::Slot0) + i), "", 10, 24 + 32 * i, 220, 30, surface);
            rowValues_[i] = lv_obj_get_child(lv_obj_get_child(body_, -1), 0);
        }
        if (calc_==Screen::Thread) {
            button(Key::Back,"Результат",10,220,104,44,accent);
            button(Key::ThreadInfo,"Размеры",120,220,110,44,surface);
        } else back();
        break;
    }
    case Screen::Number: {
        heading(inputSpecs[unsigned(edit_)].name);
        value_ = caption("", 12, 25, 215);
        const char *keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "<"};
        for (unsigned i = 0; i < 12; ++i) {
            auto k = i < 9     ? static_cast<Key>(int(Key::Digit1) + i)
                     : i == 9  ? Key::Dot
                     : i == 10 ? Key::Digit0
                               : Key::Erase;
            button(k, keys[i], 10 + 74 * (i % 3), 52 + 40 * (i / 3), 70, 37, surface);
        }
        back();
        button(Key::Save, "Сохранить", 120, 220, 110, 44, accent);
        break;
    }
    case Screen::Controls: {
        heading(groupNames[controlGroup_]);
        auto *img = lv_img_create(body_);
        lv_img_set_src(img, drawings[controlGroup_]);
        lv_obj_set_pos(img, 8, 23);
        const unsigned count = groupCount[controlGroup_];
        for (unsigned i = 0; i < count; ++i) {
            const auto &c=controls[firstControl[controlGroup_]+i];
            const auto at=handleCentres[controlGroup_][i];
            button(static_cast<Key>(unsigned(Key::Slot0)+i),c.letter,8+at.x-14,23+at.y-14,
                   28,28,0x12465e);
            auto *spot=lv_obj_get_child(body_,-1);
            lv_obj_set_style_border_width(spot,2,0);
            lv_obj_set_style_border_color(spot,lv_color_hex(0xf9e6a7),0);
            lv_obj_set_ext_click_area(spot,3);
        }
        const char* legends[]={"Коробка скоростей: А и Б","Г: шаг    Д: винт или вал","Е: маховик    Ж: рейка"};
        caption(legends[controlGroup_],10,136,220);
        if (controlGroup_==0) caption("В: направление подачи",10,155,220);
        if (controlGroup_==2) caption("З: самоход    И: гайка",10,155,220);
        button(Key::Prev,"<",10,176,48,38,surface);
        button(Key::Next,"Другой узел",62,176,168,38,surface);
        back();
        button(Key::Plate,"Шильдик",120,220,110,44,accent);
        break;
    }
    case Screen::ControlCard: {
        const auto &c = controls[controlIndex_];
        char title[55];
        std::snprintf(title, sizeof(title), "%s   %s", c.letter, c.name);
        heading(title);
        caption(c.purpose, 10, 32, 220, 0xf1f6fa, true);
        const char* instruction=threadGuide_ ? c.threading : c.feeding;
        char selected[140]{};
        if (controlOrigin_!=Screen::Menu) {
            const auto &w = state_->workshop;
            const auto speed=threadSpeed(w);
            if (threadGuide_ && speed && (c.number==1 || c.number==2)) {
                const auto& g=spindleGears[speed->gear];
                std::snprintf(selected,sizeof(selected),"%s, ступень %u об/мин.",
                              positionName(c.number==1 ? g.first : g.second),g.rpm);
            } else if (threadGuide_ && c.number==3) {
                instruction=w.get(Input::Left) ? "П, ход вправо. Сверьте с отведённым резцом." :
                                                "Л, ход к патрону. Сверьте с отведённым резцом.";
            } else if (c.number==4) {
                const unsigned index=w.get(Input::Feed);
                const auto pos=pitchPosition(threadGuide_ ? w.get(Input::Pitch) : pitches[index]);
                char size[16]; formatMillimetres(threadGuide_ ? w.get(Input::Pitch) : feeds[index],size,sizeof(size));
                if (pos) std::snprintf(selected,sizeof(selected),"%s %s %s. Ручка III %s.",
                         threadGuide_ ? "Шаг" : "Подача",size,threadGuide_ ? "мм" : "мм/об",positionName(*pos));
            }
        }
        caption(threadGuide_ ? "Для резьбы" : "Для подачи",10,103,220,blue);
        caption(selected[0] ? selected : instruction,10,130,220,0xf1f6fa,true);
        back();
        break;
    }
    case Screen::Demo:
        heading("Демонстрация");
        note_ = caption("", 10, 35, 220, 0xf1f6fa, true);
        slider_ = lv_slider_create(body_);
        lv_obj_set_pos(slider_, 19, 105);
        lv_obj_set_size(slider_, 202, 22);
        lv_slider_set_range(slider_, 0, 6000);
        lv_obj_set_style_bg_opa(slider_, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(slider_, lv_color_hex(surface), 0);
        lv_obj_set_style_bg_opa(slider_, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(slider_, lv_color_hex(blue), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(slider_, LV_OPA_COVER, LV_PART_KNOB);
        lv_obj_set_style_bg_color(slider_, lv_color_hex(blue), LV_PART_KNOB);
        lv_obj_add_event_cb(slider_, sliderChanged, LV_EVENT_VALUE_CHANGED, this);
        button(Key::Auto, "Автоматический цикл", 10, 158, 220, 43, surface);
        back();
        button(Key::Mode, "DEMO / LIVE", 120, 220, 110, 44, accent);
        break;
    case Screen::History:
        heading("Последние операции");
        dataRows(5, 33, 32);
        back();
        break;
    case Screen::Thread: case Screen::ThreadInfo: case Screen::ThreadSetup:
    case Screen::ThreadRpm: case Screen::Dial: case Screen::Plate:
        buildThreadView();
        break;
    case Screen::Network:
        heading("Wi-Fi / OTA");
        note_ = caption("", 10, 26, 220, 0xf1f6fa, true);
        button(Key::Portal, "Настроить Wi-Fi", 10, 158, 135, 43, surface);
        button(Key::CloseNet, "Закрыть", 149, 158, 81, 43, surface);
        back();
        button(Key::Arm, "Доступ OTA", 120, 220, 110, 44, accent);
        break;
    case Screen::ConfirmOTA:
        heading("Обновление прошивки");
        caption("Остановите шпиндель и физически обесточьте привод. Питание панели оставьте. "
                "Подтвердите на TFT для окна OTA на 5 минут.",
                10, 35, 220, amber, true);
        back();
        button(Key::AcceptOTA, "Отключён", 120, 220, 110, 44, accent);
        break;
    default:
        break;
    }
}
void UIManager::renderWorkshop(const MachineState &s) {
    const auto &w = s.workshop;
    char buf[240], v[45];
    auto numeric = [&](unsigned i, const char *label, std::optional<double> n, const char *format) {
        if (n)
            std::snprintf(v, sizeof(v), format, *n);
        put(i, label, n ? v : "Н/Д");
    };
    const auto rpm = s.motion.hasRpm ? std::optional<double>(s.motion.rpm) : std::nullopt;
    switch (screen_) {
    case Screen::Graphs: {
        const auto sec = s.uptimeUs / 1000000;
        if (plottedSecond_ == sec)
            break;
        plottedSecond_ = sec;
        RpmHistory::View view;
        if (s.history)
            view = s.history->view(windows[graphWindow_], s.uptimeUs);
        lv_chart_set_point_count(chart_, std::max(2u, view.width));
        lv_chart_set_range(chart_, LV_CHART_AXIS_PRIMARY_Y, 0,
                           std::max(100u, ((view.maximum + 99) / 100) * 100));
        for (unsigned i = 0; i < view.width; ++i)
            lv_chart_set_value_by_id(chart_, series_, i,
                                     view.points[i] < 0 ? LV_CHART_POINT_NONE : view.points[i]);
        if (view.count)
            std::snprintf(buf, sizeof(buf), "Мин %u   Макс %u\nСреднее %.0f RPM", view.minimum,
                          view.maximum, view.mean);
        updateText(note_, view.count ? buf : "Н/Д: нет измерений");
        break;
    }
    case Screen::Turning: {
        numeric(0, "Диаметр", w.mm(Input::Diameter), "%.2f мм");
        numeric(1, "Задано Vc",
                w.get(Input::Speed) ? std::optional<double>(w.get(Input::Speed)) : std::nullopt,
                "%.0f м/мин");
        numeric(2, "Расчёт RPM", turningRpm(w.mm(Input::Diameter), w.get(Input::Speed)), "%.0f");
        numeric(3, "Факт. Vc", rpm ? cuttingSpeed(w.mm(Input::Diameter), *rpm) : std::nullopt,
                "%.1f м/мин");
        std::snprintf(buf, sizeof(buf), "%s / %s. Vc введите по данным изготовителя резца.",
                      choice(Input::Material, w.get(Input::Material)),
                      choice(Input::Tool, w.get(Input::Tool)));
        updateText(note_, buf);
        break;
    }
    case Screen::ThreadRpm: case Screen::Dial: {
        renderThreadView(s);
        break;
    }
    case Screen::Rough: {
        const auto p = roughing(w.mm(Input::Start), w.mm(Input::Finish), w.mm(Input::Depth),
                                w.mm(Input::Allowance));
        auto field = [&](unsigned i, const char *label, double v, const char *fmt) {
            numeric(i, label, p ? std::optional<double>{v} : std::nullopt, fmt);
        };
        field(0, "По диаметру", p ? p->diameterRemoval : 0, "%.2f мм");
        field(1, "На сторону", p ? p->radialRemoval : 0, "%.2f мм");
        field(2, "Черновых", p ? p->rough : 0, "%.0f");
        field(3, "Чистовой", p ? p->finishing : 0, "%.2f мм");
        updateText(note_, p ? "Чистовой припуск и глубина заданы на сторону."
                            : "Проверьте диаметры и чистовой припуск.");
        break;
    }
    case Screen::Feed: {
        const double f = feeds[w.get(Input::Feed)] / 100.;
        numeric(0, "Подача", f, "%.2f мм/об");
        numeric(1, "Факт. RPM", rpm, "%.0f");
        numeric(2, "Ход в минуту", rpm ? feedTravel(f, *rpm) : std::nullopt, "%.1f мм/мин");
        put(3, "Механизм", "Ходовой вал");
        const auto pos=pitchPosition(pitches[w.get(Input::Feed)]);
        if (pos) {
            std::snprintf(buf,sizeof(buf),"Г (III): %s   Д: ВАЛ",positionName(*pos));
            updateText(note_,buf);
        }
        break;
    }
    case Screen::Inputs: {
        auto f = form(calc_);
        for (unsigned i = 0; i < f.count; ++i) {
            auto x = f.inputs[i];
            formatInput(w, x, v, sizeof(v));
            std::snprintf(buf, sizeof(buf), "%s  %s", inputSpecs[unsigned(x)].name, v);
            updateText(rowValues_[i], buf);
        }
        break;
    }
    case Screen::Number:
        std::snprintf(buf, sizeof(buf), "%s %s", digits_[0] ? digits_ : "_",
                      inputSpecs[unsigned(edit_)].unit);
        updateText(value_, numberError_ ? "Вне диапазона" : buf);
        break;
    case Screen::Demo:
        if (s.settings.mode != Mode::Demo) {
            lv_obj_add_state(slider_, LV_STATE_DISABLED);
            updateText(note_, "Включите DEMO для ползунка.");
        } else {
            lv_obj_clear_state(slider_, LV_STATE_DISABLED);
            std::snprintf(buf, sizeof(buf), "%s   %.0f RPM", s.demoTarget < 0 ? "Авто" : "Вручную",
                          s.motion.rpm);
            updateText(note_, buf);
            if (!lv_obj_has_state(slider_, LV_STATE_PRESSED))
                lv_slider_set_value(slider_, std::lround(s.motion.rpm), LV_ANIM_OFF);
        }
        break;
    case Screen::History:
        for (unsigned i = 0; i < 4; ++i) {
            std::snprintf(buf, sizeof(buf), "Сеанс %u", i + 1);
            elapsed(v, sizeof(v), s.recent[i].us);
            put(i, buf, s.recent[i].us ? v : "Н/Д");
        }
        elapsed(v, sizeof(v), s.poweredUs);
        put(4, "Питание всего", v);
        break;
    case Screen::Network: {
        const auto &n = s.network;
        if (n.portal)
            std::snprintf(buf, sizeof(buf), "AP %s\nКлюч %s\nIP %s\nadmin: %s\n%s", n.apName, n.apKey,
                          n.ip, n.adminKey, n.status);
        else if (n.armed)
            std::snprintf(buf, sizeof(buf), "IP %s\nadmin: %s\n%s  %d%%", n.ip, n.adminKey, n.status,
                          n.progress);
        else if (n.connected)
            std::snprintf(buf, sizeof(buf), "%s\n%s   %d dBm\n%s", n.ssid, n.ip, n.rssi, n.status);
        else
            std::snprintf(buf, sizeof(buf), "%s\nНастройка открывает защищённую точку доступа.",
                          n.status);
        updateText(note_, buf);
        break;
    }
    default:
        break;
    }
}
} // namespace tv4
