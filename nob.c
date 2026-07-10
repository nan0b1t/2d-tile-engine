#define NOB_IMPLEMENTATION
#include "nob.h"
#include <stdlib.h>

bool append_src_files(Nob_Cmd *cmd, const char *dir_path) {
    Nob_Dir_Entry dir = {0};

    if (!nob_dir_entry_open(dir_path, &dir)) {
        return false; 
    }

    while (nob_dir_entry_next(&dir)) {
        const char *name = dir.name;
        size_t len = strlen(name);

        if (len > 2 && strcmp(name + len - 2, ".c") == 0) {
            Nob_String_Builder sb = {0};
            nob_sb_append_cstr(&sb, dir_path);
            nob_sb_append_cstr(&sb, "/");
            nob_sb_append_cstr(&sb, name);
            nob_sb_append_null(&sb);

            nob_cmd_append(cmd, sb.items);
        }
    }

    nob_dir_entry_close(dir);

    return !dir.error;
}

int main(int argc, char **argv) {
    bool isDebug;
    if (argc <= 1) {
        nob_log(NOB_ERROR, "chose debug or release");
        exit(1);
    }

    if (strcmp(argv[1], "release") == 0) {
        isDebug = false;
        nob_log(NOB_INFO, "running in release mode");
    } else {
        isDebug = true;
        nob_log(NOB_INFO, "running in debug mode (default)");
    }

    NOB_GO_REBUILD_URSELF(argc, argv);

    if (!nob_mkdir_if_not_exists("build")) {
        nob_log(NOB_ERROR, "error making build directory");
        return 1;
    }

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "cc");
    nob_cmd_append(&cmd, "-std=c99");
    nob_cmd_append(&cmd, "-Wall");
    nob_cmd_append(&cmd, "-Wextra");
    nob_cmd_append(&cmd, "-Werror");
    nob_cmd_append(&cmd, "-pedantic");
    nob_cmd_append(&cmd, "-Wnull-dereference");
    nob_cmd_append(&cmd, "-Wwrite-strings");
    nob_cmd_append(&cmd, "-Wshadow");
    nob_cmd_append(&cmd, "-Wdouble-promotion");
    nob_cmd_append(&cmd, "-Wno-conversion");

    if (isDebug) {
        nob_cmd_append(&cmd, "-g");
        nob_cmd_append(&cmd, "-fsanitize=address,undefined");
    } else {
        nob_cmd_append(&cmd, "-O3");
    }

    append_src_files(&cmd, "src");
    nob_cmd_append(&cmd, "-o");
    nob_cmd_append(&cmd, "build/game");
    nob_cmd_append(&cmd, "-lm");
    nob_cmd_append(&cmd, "-lraylib");

    if (!nob_cmd_run_sync(cmd)) {
        return 1;
    }

    return 0;
}
