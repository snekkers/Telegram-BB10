/* Use UTM's bundled qemu-img as a local disk conversion utility on macOS.
 * Tested with UTM 5.0.8 / qemu-img 10.0.12. This does not control the VM UI.
 */
#include <dlfcn.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    const char *library = "/Applications/UTM.app/Contents/Frameworks/qemu-img.framework/Versions/A/qemu-img";
    void *handle = dlopen(library, RTLD_NOW | RTLD_LOCAL | RTLD_FIRST);
    if (!handle) {
        fprintf(stderr, "%s\n", dlerror());
        return 1;
    }
    int (*entry)(int, char **) = (int (*)(int, char **))dlsym(handle, "main");
    if (!entry) {
        fprintf(stderr, "%s\n", dlerror());
        return 1;
    }
    return entry(argc, argv);
}
