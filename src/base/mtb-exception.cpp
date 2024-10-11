#include "base/mtb-exception.hxx"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

namespace MTB {
    [[noreturn]]
    void crash(bool pauses, SourceLocation &&srcloc, std::string const& reason)
    {
        fprintf(stderr, "================ [进程 %d 已崩溃] ================\n", getpid());
        fprintf(stderr, ""
                "文件:   %s\n函数:   %s\n行号:   %d\n列号:   %d\n",
                srcloc.file_name(), srcloc.function_name(),
                srcloc.line(), srcloc.column());
        if (!reason.empty()) {
            fputs("================ [崩溃信息] ================\n\n", stderr);
            fputs(reason.c_str(), stderr);
            fputs("\n\n", stderr);
        }

        fflush(stderr);

        if (pauses) {
            fprintf(stderr, "Press <ENTER> to kill this process...\n");
            getchar();
        }
        abort();
    }

    [[noreturn]]
    void crash_with_stacktrace(bool pauses, SourceLocation srcloc, const std::string &reason)
    {
        print_stacktrace();
        crash(pauses, std::move(srcloc), reason);
    }
} // namespace MTB::Compatibility