#pragma once
#include <stdio.h>
#define ESP_GOTO_ON_FALSE(cond, code, label, tag, ...) do { if (!(cond)) { ret = code; goto label;} } while (0)
#define ESP_RETURN_ON_FALSE(cond, code, tag, ...) do { if (!(cond)) return code; } while (0)
