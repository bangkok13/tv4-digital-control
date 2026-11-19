#include <UIManager.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace tv4;
namespace {
lv_color_t pixels[240*16];
std::array<lv_color_t,240*320> frame{};
lv_disp_draw_buf_t buffer;
lv_disp_drv_t display;
lv_indev_drv_t pointer;
MachineState state;
UIManager ui;
lv_point_t point{};
bool down;
unsigned flushes;
void require(bool v,const char* name) {
    if (!v) { std::fprintf(stderr,"UI check failed: %s\n",name); std::exit(1); }
}
void flush(lv_disp_drv_t* d,const lv_area_t* a,lv_color_t* data) {
    for (int y=a->y1;y<=a->y2;++y)
        for (int x=a->x1;x<=a->x2;++x) frame[y*240+x]=*data++;
    ++flushes; lv_disp_flush_ready(d);
}
void input(lv_indev_drv_t*,lv_indev_data_t* data) {
    data->point=point; data->state=down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
void advance() {
    ui.update(state);
    for (int step=0;step<5;++step) { lv_tick_inc(10); lv_timer_handler(); }
}
Command tap(int x,int y) {
    point={lv_coord_t(x),lv_coord_t(y)}; down=true; advance();
    down=false; advance(); advance();
    return ui.take();
}
bool has(lv_obj_t* root,const char* text) {
    if (lv_obj_check_type(root,&lv_label_class) && !std::strcmp(lv_label_get_text(root),text)) return true;
    for (uint32_t i=0;i<lv_obj_get_child_cnt(root);++i)
        if (has(lv_obj_get_child(root,i),text)) return true;
    return false;
}
void labelsFit(lv_obj_t* obj) {
    if (lv_obj_check_type(obj,&lv_label_class) && lv_label_get_long_mode(obj)==LV_LABEL_LONG_CLIP) {
        const auto* text=lv_label_get_text(obj);
        auto width=lv_txt_get_width(text,std::strlen(text),lv_obj_get_style_text_font(obj,0),
                                    lv_obj_get_style_text_letter_space(obj,0),LV_TEXT_FLAG_NONE);
        if (width>lv_obj_get_content_width(obj)) {
            std::fprintf(stderr,"Clipped label (%d > %d): %s\n",width,lv_obj_get_content_width(obj),text);
            require(false,"visible workshop labels");
        }
    }
    for (unsigned i=0;i<lv_obj_get_child_cnt(obj);++i) labelsFit(lv_obj_get_child(obj,i));
}
void capture(const char* path) {
    auto* f=std::fopen(path,"wb"); require(f!=nullptr,"open framebuffer file");
    std::fprintf(f,"P6\n240 320\n255\n");
    for (auto pixel : frame) {
        const uint32_t c=lv_color_to32(pixel);
        std::fputc((c>>16)&255,f); std::fputc((c>>8)&255,f); std::fputc(c&255,f);
    }
    std::fclose(f);
}
}
int main(int argc,char** argv) {
    lv_init(); lv_disp_draw_buf_init(&buffer,pixels,nullptr,240*16);
    lv_disp_drv_init(&display); display.draw_buf=&buffer;
    display.hor_res=240; display.ver_res=320; display.flush_cb=flush;
    require(lv_disp_drv_register(&display)!=nullptr,"display");
    lv_indev_drv_init(&pointer); pointer.type=LV_INDEV_TYPE_POINTER; pointer.read_cb=input;
    require(lv_indev_drv_register(&pointer)!=nullptr,"pointer"); ui.begin();
    const uint32_t splashStart = lv_tick_get();
    state.motion.hasRpm=true; state.motion.rpm=1250; state.motion.rotating=true;
    state.motion.signal=Signal::Tracking; state.uptimeUs=137000000;
    state.motion.lastEdgeUs=state.uptimeUs-1000; state.motion.pulses=5200;
    advance();
    require(ui.screen()==Screen::Splash && flushes>0 && has(lv_scr_act(),"6 VOLT") &&
            has(lv_scr_act(),"GARAGE") && has(lv_scr_act(),"С ДР, БРО!") &&
            has(lv_scr_act(),"DEMO") && !has(lv_scr_act(),"1250"),"birthday splash at boot");
    if (argc>=3) capture(argv[2]);
    state.settings.mode=Mode::Live; advance();
    require(has(lv_scr_act(),"LIVE") && !has(lv_scr_act(),"DEMO"),"saved mode badge on splash");
    state.settings.mode=Mode::Demo;
    require(tap(50,285).action==Action::None && ui.screen()==Screen::Splash,"touch during splash");
    point={50,285}; down=true; advance();
    lv_tick_inc(6999-lv_tick_elaps(splashStart));
    ui.update(state);
    require(!ui.needsUpdate() && ui.screen()==Screen::Splash,"splash remains through 6999 ms");
    lv_tick_inc(1);
    require(ui.needsUpdate(),"transition due at 7000 ms");
    advance();
    down=false; advance();
    require(ui.take().action==Action::None && ui.screen()==Screen::Home &&
            has(lv_scr_act(),"TV-4 Шкыльнык") && !has(lv_scr_act(),"GARAGE"),"home after held boot touch");
    require(flushes>0 && has(lv_scr_act(),"1250") && has(lv_scr_act(),"D"),"demo tachometer");
    if (argc>=2) capture(argv[1]);
    tap(170,285); require(ui.screen()==Screen::Stats && has(lv_scr_act(),"Пуски") &&
                          has(lv_scr_act(),"TV-4 DIGITAL") && has(lv_scr_act(),"DEMO"),"stats");
    tap(170,285); require(ui.screen()==Screen::ConfirmReset,"reset asks first");
    require(tap(50,285).action==Action::None && ui.screen()==Screen::Stats,"cancel reset");
    tap(170,285); require(tap(170,285).action==Action::Reset,"confirmed reset");
    tap(50,285); tap(50,285); tap(170,244);
    require(ui.screen()==Screen::Settings && has(lv_scr_act(),"DEMO") &&
            has(lv_scr_act(),"Сменить DEMO / LIVE"),"settings mode control and badge");
    require(tap(120,88).action==Action::ToggleMode,"mode control");
    tap(120,237);require(ui.screen()==Screen::SettingsExtra && has(lv_scr_act(),"Последняя резьба"),"workshop settings");
    tap(50,285);require(ui.screen()==Screen::Settings,"return to basic settings");
    for (unsigned i=0;i<unsigned(Field::Count);++i) {
        const auto plus=tap(206,185);
        require(plus.action==Action::Increase && unsigned(plus.field)==i,"parameter +");
        require(tap(30,185).action==Action::Decrease,"parameter -");
        tap(120,139);
    }
    tap(170,285); require(ui.screen()==Screen::Diagnostics,"diagnostics");
    require(tap(120,238).action==Action::Calibrate,"calibration entry");
    tap(170,285); require(has(lv_scr_act(),"Не подключено"),"disconnected Modbus");
    tap(50,285); tap(50,285);tap(50,285);
    state.settings.mode=Mode::Live; state.motion={}; advance();
    require(has(lv_scr_act(),"L") && !has(lv_scr_act(),"D") && has(lv_scr_act(),"Н/Д") &&
            has(lv_scr_act(),"TV-4 Шкыльнык"),"live without sensor");
    tap(50,285);tap(170,126);require(ui.screen()==Screen::Thread,"thread menu");
    tap(170,285);tap(100,111);require(ui.screen()==Screen::Number,"pitch keyboard");
    tap(40,113);tap(40,234);tap(120,113);tap(120,153);const auto edit=tap(170,285);
    require(edit.action==Action::SetInput&&edit.input==Input::Pitch&&edit.value==125,"enter 1.25");
    auto picture=[&](const char* name) {
        labelsFit(lv_scr_act());
        if (argc<5) return;
        char filename[512];std::snprintf(filename,sizeof(filename),"%s-%s.ppm",argv[4],name);
        capture(filename);
    };
    state.workshop.set(edit.input,edit.value);tap(170,285);require(ui.screen()==Screen::ThreadInfo,"plain sizes");
    picture("sizes");
    tap(50,285);tap(50,285);
    require(has(lv_scr_act(),"Г   Шаг (III): вверх-влево") && has(lv_scr_act(),"Цель   120 об/мин"),"practical M10 thread");
    picture("thread");
    tap(50,240);require(ui.screen()==Screen::ThreadSetup,"all handles");picture("setup");
    tap(50,240);require(ui.screen()==Screen::Controls,"thread controls");
    if (argc>=4) capture(argv[3]);
    labelsFit(lv_scr_act());
    auto hit=[&](int x,int y,const char* card) {
        tap(x,y);require(ui.screen()==Screen::ControlCard&&has(lv_scr_act(),card),"tap physical handle");
        labelsFit(lv_scr_act());tap(50,285);
    };
    hit(126,149,"А   Скорость I");hit(186,149,"Б   Скорость II");hit(86,149,"В   Направление");
    tap(170,240);require(has(lv_scr_act(),"Коробка подач"),"next group is feedbox");picture("feedbox");
    tap(131,91);require(ui.screen()==Screen::ControlCard && has(lv_scr_act(),"Шаг 1,25 мм. Ручка III вверх-влево."),"pitch on marked handle");
    tap(50,285);hit(76,91,"Д   Винт или вал");
    tap(170,240);picture("apron");hit(49,142,"Е   Маховик каретки");hit(92,101,"Ж   Реечная шестерня");
    hit(131,160,"З   Самоход");hit(185,122,"И   Маточная гайка");
    tap(170,285);require(ui.screen()==Screen::Plate,"plate link");picture("speed-plate");
    tap(170,285);picture("feed-plate");
    tap(50,285);tap(50,285);tap(50,285);
    tap(170,240);require(ui.screen()==Screen::ThreadRpm&&has(lv_scr_act(),"LIVE: нет импульсов"),"pot without tach");
    picture("rpm");tap(170,244);require(ui.screen()==Screen::Dial,"dial calibration");
    labelsFit(lv_scr_act());
    require(tap(125,151).action==Action::None,"no sampling a missing signal");
    state.settings.mode=Mode::Demo;state.motion={Signal::Tracking,120,true,true};advance();
    require(tap(125,151).action==Action::None,"no sampling demo");
    state.settings.mode=Mode::Live;advance();const auto cal=tap(125,151);
    require(cal.action==Action::RecordDial&&cal.gear==0&&cal.point==0&&cal.value==50,"record physical reading");
    picture("dial");
    state.dial.record(0,0,25,80);state.dial.record(0,1,75,160);tap(50,285);
    require(has(lv_scr_act(),"Ручка ~ 50 % по замерам"),"empirical percent");tap(50,285);
    state.workshop.set(Input::Pitch,150);tap(170,285);tap(50,285);
    require(!has(lv_scr_act(),"Все рычаги")&&!has(lv_scr_act(),"Обороты"),"unsupported pitch has no setup");
    state.workshop.set(Input::Pitch,80);state.workshop.set(Input::Left,1);
    state.workshop.set(Input::Internal,1);state.workshop.set(Input::ThreadDiameter,5000);
    tap(170,285);tap(170,285);
    require(ui.screen()==Screen::ThreadInfo&&has(lv_scr_act(),"Сверло под метчик ~ 49.20"),"internal drill reference");
    labelsFit(lv_scr_act());tap(50,285);tap(50,285);
    require(has(lv_scr_act(),"В   Ход: П, вправо")&&has(lv_scr_act(),"Г   Шаг (III): вверх"),"left-hand pitch");
    tap(170,240);require(has(lv_scr_act(),"Ниже ступени: 29 об/мин")&&has(lv_scr_act(),"Ручка: вне замеров"),"below slowest step");
    labelsFit(lv_scr_act());
    tap(50,285);tap(50,285);
    tap(50,85);require(ui.screen()==Screen::Graphs,"graph view");tap(170,285);require(has(lv_scr_act(),"2 мин"),"graph range");
    tap(50,285);tap(50,243);require(ui.screen()==Screen::Network,"network menu");
    tap(170,285);require(ui.screen()==Screen::ConfirmOTA,"maintenance physical confirmation");tap(50,285);tap(50,285);tap(50,285);
    tap(50,285);tap(50,128);require(ui.screen()==Screen::Turning&&has(lv_scr_act(),"Расчёт RPM"),"turning");
    tap(50,285);tap(50,162);require(ui.screen()==Screen::Rough&&has(lv_scr_act(),"Черновых"),"roughing");
    tap(50,285);tap(170,162);require(ui.screen()==Screen::Feed&&has(lv_scr_act(),"Ходовой вал"),"mechanical feed");
    tap(120,240);require(ui.screen()==Screen::Controls&&has(lv_scr_act(),"Коробка подач"),"feedbox from feed");
    tap(131,91);require(has(lv_scr_act(),"Подача 0,08 мм/об. Ручка III вверх."),"feed detent");
    tap(50,285);tap(50,285);
    tap(50,285);tap(170,205);require(ui.screen()==Screen::Demo,"demo slider");
    state.settings.mode=Mode::Demo;advance();
    require(tap(170,160).action==Action::DemoSpeed,"slider sends demo target");
    require(tap(120,225).value==-1,"return to automatic simulation");
    state.settings.mode=Mode::Live;tap(50,285);tap(170,85);tap(120,232);
    require(ui.screen()==Screen::History&&has(lv_scr_act(),"Питание всего"),"operation and power history");
    tap(50,285);tap(50,285);
    lv_mem_monitor_t start{}, end{};
    // Warm each view before comparing the allocator's steady-state usage.
    tap(50,285); tap(50,285); lv_mem_monitor(&start);
    for (unsigned i=0;i<300;++i) {
        tap(50,285);tap(170,244);tap(170,285);tap(170,285);tap(50,285);tap(50,285);tap(50,285);
        tap(170,285); tap(50,285);
        require(ui.screen()==Screen::Home && has(lv_scr_act(),"L"),"navigation cycle");
    }
    lv_mem_monitor(&end);
    require(end.free_size+256>=start.free_size,"LVGL heap stable");
    require(lv_mem_test()==LV_RES_OK,"LVGL allocator integrity");
    std::printf("OK: 300 navigation cycles, free LVGL heap %u -> %u bytes, flushes %u\n",
        unsigned(start.free_size),unsigned(end.free_size),flushes);
}
