{
    if ($0 ~ /#include "lwip\/sockets.h"/) {
        print "#include <lwip/sockets.h>"
        next;
    }
    print $0;
}
