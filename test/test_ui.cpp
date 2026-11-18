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
            has(lv_scr_act(),"D") && !has(lv_scr_act(),"1250"),"birthday splash at boot");
    if (argc>=3) capture(argv[2]);
    state.settings.mode=Mode::Live; advance();
    require(has(lv_scr_act(),"L") && !has(lv_scr_act(),"D"),"saved mode badge on splash");
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
            has(lv_scr_act(),"TV-4 Шкильнык") && !has(lv_scr_act(),"GARAGE"),"home after held boot touch");
    require(flushes>0 && has(lv_scr_act(),"1250") && has(lv_scr_act(),"D"),"demo tachometer");
    if (argc>=2) capture(argv[1]);
    tap(170,285); require(ui.screen()==Screen::Stats && has(lv_scr_act(),"Пуски") &&
                          has(lv_scr_act(),"TV-4 DIGITAL") && !has(lv_scr_act(),"TV-4 Шкильнык"),"stats");
    tap(170,285); require(ui.screen()==Screen::ConfirmReset,"reset asks first");
    require(tap(50,285).action==Action::None && ui.screen()==Screen::Stats,"cancel reset");
    tap(170,285); require(tap(170,285).action==Action::Reset,"confirmed reset");
    tap(50,285); tap(50,285);
    require(ui.screen()==Screen::Settings && has(lv_scr_act(),"D") &&
            has(lv_scr_act(),"Сменить DEMO / LIVE"),"settings mode control and badge");
    require(tap(120,88).action==Action::ToggleMode,"mode control");
    for (unsigned i=0;i<unsigned(Field::Count);++i) {
        const auto plus=tap(206,185);
        require(plus.action==Action::Increase && unsigned(plus.field)==i,"parameter +");
        require(tap(30,185).action==Action::Decrease,"parameter -");
        tap(120,139);
    }
    tap(170,285); require(ui.screen()==Screen::Diagnostics,"diagnostics");
    require(tap(120,238).action==Action::Calibrate,"calibration entry");
    tap(170,285); require(has(lv_scr_act(),"Не подключено"),"disconnected Modbus");
    tap(50,285); tap(50,285);
    state.settings.mode=Mode::Live; state.motion={}; advance();
    require(has(lv_scr_act(),"L") && !has(lv_scr_act(),"D") && has(lv_scr_act(),"Н/Д") &&
            has(lv_scr_act(),"TV-4 Шкильнык"),"live without sensor");
    lv_mem_monitor_t start{}, end{};
    // Warm each view before comparing the allocator's steady-state usage.
    tap(50,285); tap(50,285); lv_mem_monitor(&start);
    for (unsigned i=0;i<300;++i) {
        tap(50,285); tap(170,285); tap(170,285); tap(50,285); tap(50,285);
        tap(170,285); tap(50,285);
        require(ui.screen()==Screen::Home && has(lv_scr_act(),"L"),"navigation cycle");
    }
    lv_mem_monitor(&end);
    require(end.free_size+256>=start.free_size,"LVGL heap stable");
    require(lv_mem_test()==LV_RES_OK,"LVGL allocator integrity");
    std::printf("OK: 300 navigation cycles, free LVGL heap %u -> %u bytes, flushes %u\n",
        unsigned(start.free_size),unsigned(end.free_size),flushes);
}
