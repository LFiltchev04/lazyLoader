#include <optional>
#include <string>
#include <limits.h>
#include <unistd.h>


std::optional<std::string> resolveFdPath(int fd);