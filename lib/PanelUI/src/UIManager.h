#pragma once
#include <MachineState.h>
#include <lvgl.h>

namespace tv4 {
enum class Screen { Splash, Home, Settings, Stats, Diagnostics, ConfirmReset };
enum class Action { None, ToggleMode, Decrease, Increase, Calibrate, Reset };
struct Command { Action action = Action::None; Field field = Field::Brightness; };
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
    enum class Key { Home, Settings, Stats, Diagnostics, Confirm, Cancel, Yes, Mode, FieldNext, Dec, Inc, Cal, More };
    static void clicked(lv_event_t* e);
    void navigate(Key key);
    void build();
    void button(Key key, const char* text, int x, int y, int w, int h, uint32_t colour);
    void rows(unsigned count, int firstY, int step);
    void renderRows(const MachineState& state);
    bool splashExpired() const;
    Screen screen_ = Screen::Splash, origin_ = Screen::Settings;
    uint32_t splashStarted_ = 0;
    Field field_ = Field::Brightness;
    bool rebuild_ = true, secondPage_ = false, fieldChanged_ = false, resetError_ = false;
    Command pending_{};
    lv_obj_t *body_ = nullptr, *badge_ = nullptr, *rpm_ = nullptr, *unit_ = nullptr,
             *bar_ = nullptr, *signal_ = nullptr, *time_ = nullptr, *limit_ = nullptr,
             *value_ = nullptr, *fieldName_ = nullptr, *confirm_ = nullptr, *activity_ = nullptr;
    lv_obj_t* rowLabels_[6]{};
    lv_obj_t* rowValues_[6]{};
};
}
