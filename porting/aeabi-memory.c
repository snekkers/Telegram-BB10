/* ARM EABI memory helpers emitted by Clang and absent from BB10 libc.
 * Compile with -fno-builtin so forwarding to libc cannot recurse into these.
 */
#include <stddef.h>
#include <string.h>

void __aeabi_memcpy(void *dest, const void *src, size_t size) { memcpy(dest, src, size); }
void __aeabi_memcpy4(void *dest, const void *src, size_t size) { memcpy(dest, src, size); }
void __aeabi_memcpy8(void *dest, const void *src, size_t size) { memcpy(dest, src, size); }
void __aeabi_memmove(void *dest, const void *src, size_t size) { memmove(dest, src, size); }
void __aeabi_memmove4(void *dest, const void *src, size_t size) { memmove(dest, src, size); }
void __aeabi_memmove8(void *dest, const void *src, size_t size) { memmove(dest, src, size); }
void __aeabi_memset(void *dest, size_t size, int value) { memset(dest, value, size); }
void __aeabi_memset4(void *dest, size_t size, int value) { memset(dest, value, size); }
void __aeabi_memset8(void *dest, size_t size, int value) { memset(dest, value, size); }
void __aeabi_memclr(void *dest, size_t size) { memset(dest, 0, size); }
void __aeabi_memclr4(void *dest, size_t size) { memset(dest, 0, size); }
void __aeabi_memclr8(void *dest, size_t size) { memset(dest, 0, size); }
