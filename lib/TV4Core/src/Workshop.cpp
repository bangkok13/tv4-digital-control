#include "Workshop.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <iterator>

namespace tv4 {
const InputSpec inputSpecs[] = {{"Диаметр детали", "мм", 1, 65535, true},
                                {"Материал", "", 0, 4, false},
                                {"Резец", "", 0, 1, false},
                                {"Скорость резания", "м/мин", 0, 2000, false},
                                {"Диаметр резьбы", "мм", 1, 65535, true},
                                {"Шаг", "мм", 1, 2000, true},
                                {"Вид резьбы", "", 0, 1, false},
                                {"Направление", "", 0, 1, false},
                                {"Исходный диаметр", "мм", 1, 65535, true},
                                {"Конечный диаметр", "мм", 1, 65535, true},
                                {"Глубина на сторону", "мм", 1, 65535, true},
                                {"Чистовой на сторону", "мм", 0, 65535, true},
                                {"Продольная подача", "мм/об", 0, 2, false}};
static_assert(sizeof(inputSpecs) / sizeof(*inputSpecs) == unsigned(Input::Count));
bool Workshop::set(Input f, unsigned v) {
    if (f >= Input::Count)
        return false;
    const auto &spec = inputSpecs[unsigned(f)];
    if (v < spec.low || v > spec.high)
        return false;
    value[unsigned(f)] = v;
    return true;
}
bool Workshop::valid() const {
    for (unsigned i = 0; i < unsigned(Input::Count); ++i)
        if (value[i] < inputSpecs[i].low || value[i] > inputSpecs[i].high)
            return false;
    return true;
}
const char *choice(Input f, unsigned v) {
    static const char *materials[] = {"Сталь", "Нержавейка", "Алюминий", "Латунь", "Чугун"};
    switch (f) {
    case Input::Material:
        return v < 5 ? materials[v] : nullptr;
    case Input::Tool:
        return v ? "Твёрдый сплав" : "HSS";
    case Input::Internal:
        return v ? "Внутренняя" : "Наружная";
    case Input::Left:
        return v ? "Левая" : "Правая";
    case Input::Feed:
        return v == 0 ? "0,08" : v == 1 ? "0,10" : "0,12";
    default:
        return nullptr;
    }
}
void formatInput(const Workshop &w, Input f, char *dest, unsigned size) {
    const auto v = w.get(f);
    if (const char *name = choice(f, v))
        std::snprintf(dest, size, "%s", name);
    else if (inputSpecs[unsigned(f)].hundredths)
        std::snprintf(dest, size, "%u.%02u", v / 100, v % 100);
    else
        std::snprintf(dest, size, "%u", v);
}
std::optional<unsigned> parseInput(Input f, const char *text) {
    if (!text || !*text || f >= Input::Count)
        return {};
    const auto &spec = inputSpecs[unsigned(f)];
    unsigned whole = 0, fraction = 0, digits = 0;
    bool decimal = false, any = false;
    for (; *text; ++text) {
        if ((*text == '.' || *text == ',') && !decimal && spec.hundredths) {
            decimal = true;
            continue;
        }
        if (*text < '0' || *text > '9')
            return {};
        const unsigned d = unsigned(*text - '0');
        any = true;
        if (decimal) {
            if (++digits > 2)
                return {};
            fraction = 10 * fraction + d;
        } else {
            if (whole > 65535)
                return {};
            whole = 10 * whole + d;
        }
    }
    if (!any)
        return {};
    const unsigned v = spec.hundredths ? whole * 100 + fraction * (digits == 1 ? 10 : 1) : whole;
    if (v < spec.low || v > spec.high)
        return {};
    return v;
}
bool supportedPitch(unsigned p) {
    return std::find(std::begin(pitches), std::end(pitches), p) != std::end(pitches);
}
namespace {
constexpr double pi = 3.14159265358979323846;
bool positive(double value) { return std::isfinite(value) && value > 0; }
bool nonnegative(double value) { return std::isfinite(value) && value >= 0; }
std::optional<double> finiteResult(double v) {
    return std::isfinite(v) ? std::optional<double>{v} : std::nullopt;
}
} // namespace
std::optional<double> turningRpm(double d, double v) {
    return positive(d) && positive(v) ? finiteResult(1000 * v / (pi * d)) : std::nullopt;
}
std::optional<double> cuttingSpeed(double d, double n) {
    return positive(d) && nonnegative(n) ? finiteResult(pi * d * n / 1000) : std::nullopt;
}
std::optional<double> feedTravel(double f, double n) {
    return positive(f) && nonnegative(n) ? finiteResult(f * n) : std::nullopt;
}
std::optional<Thread> threadGeometry(double d, double p) {
    if (!positive(d) || !positive(p) || d <= 1.226869 * p)
        return {};
    return Thread{0.8660254 * p, d - 0.649519 * p, d - 1.226869 * p, d - 1.082532 * p, d - p};
}
const char* positionName(LeverPos p) {
    switch (p) {
    case LeverPos::DownRight: return "вниз-вправо";
    case LeverPos::Down: return "вниз";
    case LeverPos::DownLeft: return "вниз-влево";
    case LeverPos::Up: return "вверх";
    case LeverPos::UpRight: return "вверх-вправо";
    case LeverPos::UpLeft: return "вверх-влево";
    }
    return "";
}
std::optional<LeverPos> pitchPosition(unsigned pitch) {
    switch (pitch) {
    case 80: return LeverPos::Up;
    case 100: return LeverPos::UpRight;
    case 125: return LeverPos::UpLeft;
    default: return {};
    }
}
// Rostov plate with the 120 / 180 / 250 and 390 / 500 / 710 rows.
const SpindleGear spindleGears[6] = {
    {120,LeverPos::DownRight,LeverPos::Down}, {180,LeverPos::DownRight,LeverPos::DownRight},
    {250,LeverPos::DownRight,LeverPos::DownLeft}, {390,LeverPos::DownLeft,LeverPos::Down},
    {500,LeverPos::DownLeft,LeverPos::DownRight}, {710,LeverPos::DownLeft,LeverPos::DownLeft}
};
std::optional<ThreadSpeed> threadSpeed(const Workshop& w) {
    if (!w.valid() || !supportedPitch(w.get(Input::Pitch)) || w.mm(Input::ThreadDiameter)>200 ||
        !threadGeometry(w.mm(Input::ThreadDiameter),w.mm(Input::Pitch))) return {};
    // TO 34-1-10, table 4-2: lower threading speed in feet/min for carbide.
    // Steel is medium carbon. For HSS use the stated 50% reduction.
    constexpr double fpm[] = {30,25,100,75,40};
    const double speed = fpm[w.get(Input::Material)] * 0.3048 * (w.get(Input::Tool) ? 1.0 : 0.5);
    const unsigned calculated = unsigned(std::floor(*turningRpm(w.mm(Input::ThreadDiameter),speed)));
    unsigned gear = 0;
    while (gear+1<std::size(spindleGears) && spindleGears[gear+1].rpm<=calculated) ++gear;
    const bool below = calculated<spindleGears[0].rpm;
    return ThreadSpeed{speed,calculated,below ? calculated : spindleGears[gear].rpm,gear,below};
}
void formatMillimetres(unsigned v,char* text,unsigned size) {
    if (v%100==0) std::snprintf(text,size,"%u",v/100);
    else if (v%10==0) std::snprintf(text,size,"%u,%u",v/100,v%100/10);
    else std::snprintf(text,size,"%u,%02u",v/100,v%100);
}
bool DialPair::valid() const {
    for (const auto& p:points)
        if (p.percent>100 || p.rpm>6000 || p.reserved || (!p.rpm && p.percent)) return false;
    return !points[0].rpm || !points[1].rpm ||
        (points[0].percent!=points[1].percent && points[0].rpm!=points[1].rpm);
}
std::optional<unsigned> DialPair::percentFor(unsigned rpm) const {
    const auto a=points[0], b=points[1];
    if (!valid() || !a.rpm || !b.rpm || rpm<std::min(a.rpm,b.rpm) || rpm>std::max(a.rpm,b.rpm))
        return {};
    const double fraction=(double(rpm)-a.rpm)/(int(b.rpm)-int(a.rpm));
    return unsigned(std::lround(a.percent+fraction*(int(b.percent)-int(a.percent))));
}
bool DialBook::valid() const {
    for (const auto& pair:gears) if (!pair.valid()) return false;
    return true;
}
bool DialBook::record(unsigned gear,unsigned point,unsigned percent,unsigned rpm) {
    if (gear>=std::size(gears) || point>=2 || percent>100 || !rpm || rpm>6000) return false;
    auto pair=gears[gear];
    pair.points[point]={uint16_t(rpm),uint8_t(percent),0};
    if (!pair.valid()) return false;
    gears[gear]=pair;
    return true;
}
std::optional<Passes> roughing(double start, double finish, double depth, double allowance) {
    if (!positive(start) || !positive(finish) || !positive(depth) || !nonnegative(allowance) ||
        start < finish)
        return {};
    const double radial = (start - finish) / 2;
    if (allowance > radial + 1e-10)
        return {};
    const double count = std::ceil(std::max(0.0, (radial - allowance) / depth - 1e-10));
    if (count > std::numeric_limits<unsigned>::max())
        return {};
    return Passes{start - finish, radial, allowance, unsigned(count)};
}
const Control controls[9] = {
    {1,"А","Скорость I","Средняя ручка передней бабки. Вместе с Б задаёт ступень шпинделя.",
     "Положение I в таблице скоростей.","Положение I в таблице скоростей."},
    {2,"Б","Скорость II","Правая ручка передней бабки. Выбирает скорость вместе с А.",
     "Положение II в таблице скоростей.","Положение II в таблице скоростей."},
    {3,"В","Направление","Л и П задают ход каретки, Н отключает подачу.",
     "Сверьте ход при отведённом резце.","Л для хода влево, П для хода вправо."},
    {4,"Г","Шаг и подача III","Правая ручка коробки подач. Верхние положения выбирают три шага или подачи.",
     "См. колонку шага на шильдике.","См. колонку подачи на шильдике."},
    {5,"Д","Винт или вал","Левая ручка коробки подач. Выбирает, чем приводить каретку.",
     "К надписи ВИНТ.","К надписи ВАЛ."},
    {6,"Е","Маховик каретки","Перемещает каретку вручную.","Фиксированного положения нет.","Фиксированного положения нет."},
    {7,"Ж","Реечная шестерня","Кнопка связи маховика с рейкой.","Вытянуть на себя.","Вытянуть на себя."},
    {8,"З","Самоход","Включает продольную подачу от ходового вала.","Выключить, вниз от себя.","Включить, на себя."},
    {9,"И","Маточная гайка","Соединяет каретку с ходовым винтом.","При настройке вверх. Для рабочего прохода вниз.","Вверх, разомкнута."}};
} // namespace tv4
