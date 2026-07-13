#pragma once

// Annotation macros are metadata for the Clang export tool only.
// They expand to nothing in the MSVC engine compile.
#ifndef BLUNDER_CLASS
#define BLUNDER_CLASS(...)
#endif
#ifndef BLUNDER_PROPERTY
#define BLUNDER_PROPERTY(...)
#endif
#ifndef BLUNDER_FUNCTION
#define BLUNDER_FUNCTION(...)
#endif
