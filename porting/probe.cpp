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
    return text->empty() || counter.load() != 1 || std::sqrt(4.0) != 2.0;
}
