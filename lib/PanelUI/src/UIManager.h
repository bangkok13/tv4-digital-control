#pragma once
#include <MachineState.h>
#include <lvgl.h>

namespace tv4 {
enum class Screen { Splash, Home, Settings, Stats, Diagnostics, ConfirmReset, Menu, Graphs,
    Turning, Thread, Rough, Feed, Controls, ControlCard, Inputs, Number, Demo, Network, ConfirmOTA, History, ThreadInfo, SettingsExtra };
enum class Action { None, ToggleMode, Decrease, Increase, Calibrate, Reset, SetInput,
    DemoSpeed, Portal, ArmOTA, CloseNetwork };
struct Command { Action action=Action::None; Field field=Field::Brightness; Input input=Input::Diameter; int value=0; };
class UIManager {
public:
    void begin();
    void update(const MachineState& state);
    Command take();
    void redraw() { lv_obj_invalidate(lv_scr_act()); }
    Screen screen() const { return screen_; }
    bool needsUpdate() const { return rebuild_ || fieldChanged_ || splashExpired(); }
    void resetFailed();
private:
    enum class Key { Home, Settings, Stats, Diagnostics, Confirm, Cancel, Yes, Mode, FieldNext, Dec, Inc, Cal, More,
        Menu, Graphs, Turning, Thread, Rough, Feed, Controls, Inputs, Back, Prev, Next, Demo, Network,
        Portal, Arm, AcceptOTA, CloseNet, History, Auto, Save, Erase, Dot, ThreadInfo, SettingsExtra,
        WorkSettings, ThreadSettings, Slot0, Slot1, Slot2, Slot3, Slot4,
        Slot5, Slot6, Slot7, Slot8, Digit0,Digit1,Digit2,Digit3,Digit4,Digit5,Digit6,Digit7,Digit8,Digit9 };
    static void clicked(lv_event_t* e);
    void navigate(Key key);
    void build();
    void button(Key key, const char* text, int x, int y, int w, int h, uint32_t colour);
    void rows(unsigned count, int firstY, int step);
    void renderRows(const MachineState& state);
    void buildWorkshop();
    void renderWorkshop(const MachineState& state);
    void workshopKey(Key key);
    void dataRows(unsigned count,int y,int step);
    void put(unsigned n,const char* name,const char* value);
    lv_obj_t* caption(const char* text,int x,int y,int w,uint32_t ink=0xf1f6fa,bool wrap=false);
    static void sliderChanged(lv_event_t* e);
    Screen calc_=Screen::Turning,controlOrigin_=Screen::Menu;
    bool threadGuide_=true,numberError_=false;
    unsigned graphWindow_=0,controlGroup_=0,controlIndex_=0;
    Input edit_=Input::Diameter;
    char digits_[12]{};
    const MachineState* state_=nullptr;
    lv_obj_t *chart_=nullptr,*note_=nullptr,*slider_=nullptr;
    lv_chart_series_t* series_=nullptr;
    uint64_t plottedSecond_=UINT64_MAX;
    bool splashExpired() const;
    Screen screen_ = Screen::Splash, origin_ = Screen::Settings;
    uint32_t splashStarted_ = 0;
    Field field_ = Field::Brightness;
    unsigned diagnosticPage_=0;
    bool rebuild_ = true, fieldChanged_ = false, resetError_ = false;
    Command pending_{};
    lv_obj_t *body_ = nullptr, *title_ = nullptr, *badge_ = nullptr, *rpm_ = nullptr, *unit_ = nullptr,
             *bar_ = nullptr, *signal_ = nullptr, *time_ = nullptr, *limit_ = nullptr,
             *value_ = nullptr, *fieldName_ = nullptr, *confirm_ = nullptr, *activity_ = nullptr;
    lv_obj_t* rowLabels_[6]{};
    lv_obj_t* rowValues_[6]{};
};
}
