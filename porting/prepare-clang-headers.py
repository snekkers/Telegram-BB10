#!/usr/bin/env python3
"""Make a private Clang overlay for the community GNU sysroot's string header."""
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
source = ROOT / "build/porting/gcc9/gcc-9.3.0-berrycore/include/string.h"
data = source.read_bytes()
assert hashlib.sha256(data).hexdigest() == "f9258b1e7a6cb82462600711229275da092976d5eb6caa31b09ec922cc4bd3a2"
text = data.decode()
begin = text.index("// posix defines these")
end = text.index("\nextern int   memcmp", begin)
# The community header gives its C function and C++ inline wrapper different
# return types but the same C linkage. Clang correctly rejects that. Give the
# standard C++ overloads C++ linkage and alias both to the existing libc symbol.
replacement = """#ifdef __cplusplus
extern "C++" {
extern void *memchr(void *, int, size_t) __asm__("memchr");
extern const void *memchr(const void *, int, size_t) __asm__("memchr");
extern char *strchr(char *, int) __asm__("strchr");
extern const char *strchr(const char *, int) __asm__("strchr");
extern char *strpbrk(char *, const char *) __asm__("strpbrk");
extern const char *strpbrk(const char *, const char *) __asm__("strpbrk");
extern char *strrchr(char *, int) __asm__("strrchr");
extern const char *strrchr(const char *, int) __asm__("strrchr");
extern char *strstr(char *, const char *) __asm__("strstr");
extern const char *strstr(const char *, const char *) __asm__("strstr");
}
#else
extern void *memchr(const void *, int, size_t);
extern char *strchr(const char *, int);
extern char *strpbrk(const char *, const char *);
extern char *strrchr(const char *, int);
extern char *strstr(const char *, const char *);
#endif
#if defined(__EXT_UNIX_MISC)
extern char *strcasestr(const char *, const char *);
#endif
"""
target = ROOT / "build/porting/clang-include/string.h"
target.parent.mkdir(parents=True, exist_ok=True)
updated = text[:begin] + replacement + text[end:]
if not target.exists() or target.read_text() != updated:
    target.write_text(updated)
print(target)
resource = target.parent / "sys/resource.h"
resource.parent.mkdir(parents=True, exist_ok=True)
resource_text = """#ifndef BERRYGRAM_QNX_RESOURCE_OVERLAY
#define BERRYGRAM_QNX_RESOURCE_OVERLAY
/* The BB10 resource header uses timeval without including its definition. */
#include <sys/time.h>
#include_next <sys/resource.h>
#endif
"""
if not resource.exists() or resource.read_text() != resource_text:
    resource.write_text(resource_text)
