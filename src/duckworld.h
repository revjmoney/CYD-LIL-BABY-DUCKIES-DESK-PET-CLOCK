// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Rev. J. Money
// Duck Pet's pond: a clutch of glossy toy ducklings, one of them yours, plus
// the odd evil one, bugs, frogs, goldfish and a passing turtle. Runs on core 1;
// each step ends by publishing a Snapshot.
#pragma once
#include <stdint.h>
#include "evolve.h"

namespace cp {

void       duckBegin();
void       duckStep(uint32_t nowMs);
void       duckTap(int x, int y);
void       duckSave();
void       duckPause(bool on);
void       duckGenesis();  // a new clutch: round one, the duck is gone
evo::Save& duckData();
void       duckForce(float sun, float hunt, float plump);
void       duckDebug(char c);  // serial: + six hours, n next stage, t traits, r new clutch, w fast round one, h hold round one, q next silly achievement now, Z freeze for filming, . one 50 ms step while frozen, K pet to the middle, E (nearest duckling turns evil), V (your duck turns evil), N (force night), X (prey and the turtle now)

}  // namespace cp
