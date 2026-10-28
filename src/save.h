// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Rev. J. Money
// The duck's save in NVS "duck1", checked with a CRC so a half-written save is never trusted.
#pragma once
#include "evolve.h"

namespace cp {

evo::Save petLoad(const char* ns = "duck1");
void      petStore(evo::Save& s, const char* ns = "duck1");

}  // namespace cp
