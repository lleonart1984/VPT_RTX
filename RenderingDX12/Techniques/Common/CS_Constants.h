#pragma once

#define USE_COMPUTESHADER
#define CS_GROUPSIZE_1D 128
#define CS_GROUPSIZE_2D 16

#define CS_LINEARGROUP(s) (int)(ceil(s * 1.0 / CS_GROUPSIZE_1D))
#define CS_SQUAREGROUP(w, h) (int)(ceil(w * 1.0 / CS_GROUPSIZE_2D)), (int)(ceil(h * 1.0 / CS_GROUPSIZE_2D))