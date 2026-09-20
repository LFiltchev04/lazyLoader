#include "containerClass.hpp"
#include <filesystem>
namespace fs = std::filesystem;

#include <cerrno>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>

int container::fanotifyFd = -1;
databaseSingleton* container::dbSingleton = nullptr;

namespace {
std::string formatUuid(const __u8 uuid[BTRFS_UUID_SIZE]) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');

    for (std::size_t i = 0; i < BTRFS_UUID_SIZE; ++i) {
        out << std::setw(2) << static_cast<unsigned int>(uuid[i]);
        if (i == 3 || i == 5 || i == 7 || i == 9) {
            out << '-';
        }
    }

    return out.str();
}

bool isZeroUuid(const __u8 uuid[BTRFS_UUID_SIZE]) {
    for (std::size_t i = 0; i < BTRFS_UUID_SIZE; ++i) {
        if (uuid[i] != 0) {
            return false;
        }
    }
    return true;
}
} // namespace






container::container(std::vector<blobState*> blobLayers){
    
    for (const auto& layer : blobLayers) {
        if (layer == nullptr) {
            throw std::invalid_argument("Blob layer pointer cannot be null");
        }

        if(!dbSingleton->blobPresent(layer->getBlobHash())){
            throw std::invalid_argument("Blob layer with hash " + layer->getBlobHash() + " is not present in the database");
        }

    }
    
    blobStates = blobLayers;
}





std::string container::determineLayer(std::string path){
    if (path.empty()) {
        throw std::invalid_argument("Path cannot be empty");
    }

    int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        throw std::runtime_error(std::string("Failed to open path '") + path + "': " + std::strerror(errno));
    }

    btrfs_ioctl_get_subvol_info_args args{};
    args.treeid = 0;

    if (ioctl(fd, BTRFS_IOC_GET_SUBVOL_INFO, &args) < 0) {
        const std::string err = std::strerror(errno);
        close(fd);
        throw std::runtime_error(std::string("BTRFS_IOC_GET_SUBVOL_INFO failed for '") + path + "': " + err);
    }

    if (close(fd) < 0) {
        throw std::runtime_error(std::string("Failed to close fd for '") + path + "': " + std::strerror(errno));
    }

    const std::string uuid = formatUuid(args.uuid);
    const std::string parentUuid = formatUuid(args.parent_uuid);

    std::cout << "subvol uuid: " << uuid << std::endl;
    std::cout << "parent uuid: " << parentUuid << std::endl;
    std::cout << "filepath: " << args.name << std::endl;
    std::cout << "subvol id: " << args.treeid << std::endl;

    // For snapshots, parent UUID identifies the lower layer.
    if (!isZeroUuid(args.parent_uuid)) {
        return parentUuid;
    }

    return uuid;
}


std::string container::xattrLayerDerive(std::string path){
    char buffer[310];
    ssize_t ret = getxattr(path.c_str(), "user.layer_hash", buffer, sizeof(buffer));
    if (ret == -1) {
        throw std::runtime_error(std::string("Failed to get xattr for '") + path + "': " + std::strerror(errno));
    }

    return std::string(buffer, ret);
}



void container::setupFanotify(std::string treeRoot){
    fs::directory_iterator start{treeRoot};
    fs::directory_iterator end{};

    for (auto iter{start}; iter != end; ++iter) {
        if (fanotify_mark(fanotifyFd, FAN_MARK_ADD, FAN_OPEN | FAN_EVENT_ON_CHILD | FAN_OPEN_PERM, AT_FDCWD, iter->path().c_str()) < 0) {
            std::perror("fanotify_mark");
            close(fanotifyFd);
            return;
        }
    }    
}



