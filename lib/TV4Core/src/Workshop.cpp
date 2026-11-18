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
    {1, "Ступень шпинделя", "Вместе с N2 выбирает одну из шести механических скоростей.",
     "По табличке скоростей.", "По табличке скоростей."},
    {2, "Ступень шпинделя", "Второй рычаг коробки скоростей. Паспортный диапазон 120-710 об/мин.",
     "По табличке скоростей.", "По табличке скоростей."},
    {3, "Трензель", "Направление движения подачи. Положения Л, Н, П. В нейтрали привод подач разомкнут.",
     "Проверить ход суппорта для выбранной руки резьбы.", "По направлению требуемой подачи."},
    {4, "Шаг и подача", "Три положения на коробке подач. Значение читать по табличке станка.",
     "По строке выбранного шага.", "По строке выбранной подачи."},
    {5, "Винт / вал", "Выбирает ходовой винт для резьбы или ходовой вал для подачи.",
     "Винт, крайнее правое.", "Вал, крайнее левое."},
    {6, "Маховик каретки", "Ручное продольное перемещение каретки. Фиксированных положений нет.",
     "См. состояние реечной шестерни N7.", "См. состояние реечной шестерни N7."},
    {7, "Реечная шестерня", "Кнопка связи маховика с рейкой. Имеет два положения.", "Кнопка на себя.",
     "Кнопка на себя."},
    {8, "Самоход", "Включает продольную подачу от ходового вала.", "Выключен, вниз от себя.",
     "Включён, на себя."},
    {9, "Маточная гайка", "Соединяет каретку с ходовым винтом для резьбы.", "Замкнута, крайнее нижнее.",
     "Разомкнута, верхнее."}};
} // namespace tv4
