#include "Application.h"

namespace { tv4::Application panel; }
void setup() { panel.begin(); }
void loop() { panel.tick(); yield(); }
