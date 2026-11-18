#include "LvglPort.h"

namespace tv4 {
bool LvglPort::begin(DisplayManager& lcd,TouchManager& touch) {
    display_=&lcd; touch_=&touch;
    lv_init();
    lv_disp_draw_buf_init(&buffer_,pixels_,nullptr,240*16);
    lv_disp_drv_init(&screen_);
    screen_.hor_res=240; screen_.ver_res=320;
    screen_.draw_buf=&buffer_; screen_.flush_cb=flush; screen_.user_data=this;
    if (!lv_disp_drv_register(&screen_)) return false;
    lv_indev_drv_init(&pointer_);
    pointer_.type=LV_INDEV_TYPE_POINTER; pointer_.read_cb=input; pointer_.user_data=this;
    lastMs_=millis();
    return lv_indev_drv_register(&pointer_)!=nullptr;
}
void LvglPort::flush(lv_disp_drv_t* d,const lv_area_t* area,lv_color_t* pixels) {
    auto& lcd=static_cast<LvglPort*>(d->user_data)->display_->panel();
    const uint16_t w=area->x2-area->x1+1, h=area->y2-area->y1+1;
    lcd.startWrite();
    lcd.setAddrWindow(area->x1,area->y1,w,h);
    lcd.pushColors(reinterpret_cast<uint16_t*>(pixels),uint32_t(w)*h,true);
    lcd.endWrite();
    lv_disp_flush_ready(d);
}
void LvglPort::input(lv_indev_drv_t* d,lv_indev_data_t* data) {
    auto& self=*static_cast<LvglPort*>(d->user_data);
    const auto c=self.touch_->read();
    if (c.down && !self.suppress_ && !self.touch_->calibrated()) {
        self.wantCalibration_=true;
        self.suppress_=true;
    }
    if (!c.down) self.suppress_=false;
    const bool down=c.down && !self.suppress_;
    if (down) self.lastPoint_=c.pixel;
    data->point.x=self.lastPoint_.x; data->point.y=self.lastPoint_.y;
    data->state=down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
void LvglPort::tick(uint32_t now) {
    lv_tick_inc(now-lastMs_);
    lastMs_=now;
    if (now-lastHandler_ >= 5) {
        lastHandler_=now; lv_timer_handler();
    }
}
}
