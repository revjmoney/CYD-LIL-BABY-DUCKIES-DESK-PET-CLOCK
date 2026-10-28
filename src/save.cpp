// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Rev. J. Money
#include "save.h"
#include <Arduino.h>
#include <Preferences.h>

namespace cp {

evo::Save petLoad(const char* ns) {
    evo::Save   s;
    Preferences p;
    p.begin(ns, true);
    size_t n = p.getBytesLength("s"), got = 0;
    if (n == sizeof s) got = p.getBytes("s", &s, sizeof s);
    p.end();
    if (!evo::valid(s, got)) {
        if (n) Serial.println("pet: save unreadable, starting a new clutch");
        s = evo::freshSave(esp_random());
    }
    return s;
}

void petStore(evo::Save& s, const char* ns) {
    evo::seal(s);
    Preferences p;
    p.begin(ns);
    p.putBytes("s", &s, sizeof s);
    p.end();
}

}  // namespace cp
