#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>

static_assert(__cplusplus >= 201703L, "C++17 required");
static_assert(sizeof(void *) == 4, "BB10 pointers must be 32-bit");
static_assert(sizeof(wchar_t) == 4, "QNX wchar_t ABI");
static_assert(alignof(double) == 8, "QNX ARM double ABI");
static_assert(std::is_same<decltype(std::strchr(static_cast<const char *>(nullptr), 'x')), const char *>::value,
              "const C string overload");
static_assert(std::is_same<decltype(std::strchr(static_cast<char *>(nullptr), 'x')), char *>::value,
              "mutable C string overload");

int main()
{
    std::optional<std::string> text("Berrygram");
    std::atomic<std::uint64_t> counter(0);
    std::mutex mutex;
    std::thread worker([&] {
        std::lock_guard<std::mutex> lock(mutex);
        counter.fetch_add(1);
    });
    worker.join();
    if (text->empty() || counter.load() != 1 || std::sqrt(4.0) != 2.0) return 1;

    // Use runtime arguments and function pointers so constant folding cannot
    // hide recursive C++ wrappers accidentally emitted for libc symbols.
    typedef const void *(*ConstMemchr)(const void *, int, std::size_t);
    typedef void *(*MutableMemchr)(void *, int, std::size_t);
    typedef const char *(*ConstStrchr)(const char *, int);
    typedef char *(*MutableStrchr)(char *, int);
    ConstMemchr volatile findConst = static_cast<ConstMemchr>(&std::memchr);
    MutableMemchr volatile findMutable = static_cast<MutableMemchr>(&std::memchr);
    ConstStrchr volatile findCharConst = static_cast<ConstStrchr>(&std::strchr);
    MutableStrchr volatile findCharMutable = static_cast<MutableStrchr>(&std::strchr);
    const std::size_t position = static_cast<std::size_t>(counter.load());
    const char *data = text->c_str();
    const int needle = static_cast<unsigned char>(data[position]);
    if (findConst(data, needle, text->size()) != data + position) return 10;
    if (findConst(data, 127, text->size()) != nullptr) return 11;
    if (findCharConst(data, needle) != data + position) return 12;
    if (findCharConst(data, 127) != nullptr) return 13;
    char *mutableData = &(*text)[0];
    if (findMutable(mutableData, needle, text->size()) != mutableData + position) return 14;
    if (findMutable(mutableData, 127, text->size()) != nullptr) return 15;
    if (findCharMutable(mutableData, needle) != mutableData + position) return 16;
    if (findCharMutable(mutableData, 127) != nullptr) return 17;
    return 0;
}
