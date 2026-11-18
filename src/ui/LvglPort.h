#pragma once
#include <lvgl.h>
#include "../touch/TouchManager.h"
#include "../display/DisplayManager.h"

namespace tv4 {
class LvglPort {
public:
    bool begin(DisplayManager& lcd,TouchManager& touch);
    void tick(uint32_t now);
    void ignoreUntilReleased() { suppress_=true; }
    bool takeCalibrationRequest() { bool pending=wantCalibration_; wantCalibration_=false; return pending; }
private:
    static void flush(lv_disp_drv_t* driver,const lv_area_t* area,lv_color_t* pixels);
    static void input(lv_indev_drv_t* driver,lv_indev_data_t* data);
    DisplayManager* display_=nullptr;
    TouchManager* touch_=nullptr;
    lv_color_t pixels_[240*16]{};
    lv_disp_draw_buf_t buffer_{};
    lv_disp_drv_t screen_{};
    lv_indev_drv_t pointer_{};
    uint32_t lastMs_=0, lastHandler_=0;
    Point lastPoint_{};
    bool suppress_=false, wantCalibration_=false;
};
}
