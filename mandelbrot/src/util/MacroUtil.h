#pragma once

#define _EVAL(x) x

#define _PASTE(a, b) a ## b
#define _EXPAND_AND_PASTE(a, b) _PASTE(a, b)

#define _CONCAT2(a, b) _EXPAND_AND_PASTE(a, b)
#define _CONCAT3(a, b, c) _CONCAT2(_CONCAT2(a, b), c)
#define _CONCAT4(a, b, c, d) _CONCAT2(_CONCAT3(a, b, c), d)
#define _CONCAT5(a, b, c, d, e) _CONCAT2(_CONCAT4(a, b, c, d), e)
#define _CONCAT6(a, b, c, d, e, f) _CONCAT2(_CONCAT5(a, b, c, d, e), f)
#define _CONCAT7(a, b, c, d, e, f, g) _CONCAT2(_CONCAT6(a, b, c, d, e, f), g)
#define _CONCAT8(a, b, c, d, e, f, g, h) _CONCAT2(_CONCAT7(a, b, c, d, e, f, g), h)
#define _CONCAT9(a, b, c, d, e, f, g, h, i) _CONCAT2(_CONCAT8(a, b, c, d, e, f, g, h), i)
#define _CONCAT10(a, b, c, d, e, f, g, h, i, j) _CONCAT2(_CONCAT9(a, b, c, d, e, f, g, h, i), j)