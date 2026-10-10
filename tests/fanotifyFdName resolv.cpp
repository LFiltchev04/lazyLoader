#include <gtest/gtest.h>
#include <fcntl.h>
#include <sys/fanotify.h>

#include "../src/utils/fanotifyResolve.hpp"

TEST(FanotifyFdNameResolveTest, resolveFdFilename){
    int fd = open("testfileOne", O_CREAT | O_RDONLY, 0644);
    EXPECT_NE(fd, -1);
    close(fd);

    int fd2 = open("testfileTwo", O_CREAT | O_RDONLY, 0644);
    EXPECT_NE(fd2, -1);
    close(fd2);

    mkdir("testdir", 0755);
    int fd3 = open("testdir/testfileThree", O_CREAT | O_RDONLY, 0644);
    EXPECT_NE(fd3, -1);
    close(fd3);


    int fanfd = fanotify_init(FAN_CLASS_NOTIF, O_RDONLY);
    EXPECT_NE(fanfd, -1);

    fanotify_mark(fanfd, FAN_MARK_ADD, FAN_OPEN, AT_FDCWD, "testfileOne");
    fanotify_mark(fanfd, FAN_MARK_ADD, FAN_OPEN, AT_FDCWD, "testfileTwo");
    fanotify_mark(fanfd, FAN_MARK_ADD, FAN_OPEN, AT_FDCWD, "testdir/testfileThree");

    fanotify_event_metadata event;

    std::string currentDir = get_current_dir_name();
    open("testfileOne", O_RDONLY);
    read(fanfd, &event, sizeof(event));
    printf("event resolve output %s\n", resolveFdPath(event.fd)->c_str());
    EXPECT_EQ(resolveFdPath(event.fd), std::optional<std::string>(currentDir + "/testfileOne"));

    open("testfileTwo", O_RDONLY);
    read(fanfd, &event, sizeof(event));
    EXPECT_EQ(resolveFdPath(event.fd), std::optional<std::string>(currentDir + "/testfileTwo"));

    open("testdir/testfileThree", O_RDONLY);
    read(fanfd, &event, sizeof(event));
    EXPECT_EQ(resolveFdPath(event.fd), std::optional<std::string>(currentDir + "/testdir/testfileThree"));
    close(fanfd);



    unlink("testfileOne");
    unlink("testfileTwo");
    unlink("testdir/testfileThree");
    rmdir("testdir");
}