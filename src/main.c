/*
 * debug main during restructuring of tasknc
 * by mjheagle
 */

#include <stdio.h>
#include "json.h"

const char *teststr = "{\"id\":49,\"description\":\"test \\\"json parse\\\", \\\\ with special chars\",\"entry\":\"20130130T024033Z\",\"project\":\"tasknc\",\"status\":\"pending\",\"uuid\":\"9b06712a-b198-4e8e-ad06-cf0b3c5d5c9a\",\"urgency\":\"1\"}";

int main() {
        char ** fields = parse_json(teststr);
        char ** f;
        for (f = fields; *f != NULL; f++)
                printf("%s\n", *f);

        return 0;
}
