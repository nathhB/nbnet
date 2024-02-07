#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CuTest.h"

static void StringReplaceAll(char *res, const char *str, const char *a, const char *b)
{
    char *substr = strstr(str, a);
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);

    if (substr)
    {
        int pos = substr - str;

        strncpy(res, str, pos);
        strncpy(res + pos, b, len_b);

        StringReplaceAll(res + pos + len_b, str + pos + len_a, a, b);
    }
    else
    {
        strncpy(res, str, strlen(str) + 1);
    }
}

void Test_StringReplaceAll(CuTest *tc)
{
    const char *str = "foo bar plop foo plap test foo";
    const char *str2 = "foo bar\nplop\n\nplap";
    const char *str3 = "foo bar";
    char res[128] = {0};
    char res2[128] = {0};
    char res3[16] = {0};

    StringReplaceAll(res, str, "foo", "hello");
    CuAssertStrEquals(tc, "hello bar plop hello plap test hello", res);
    StringReplaceAll(res2, str2, "\n", "\\n");
    CuAssertStrEquals(tc, "foo bar\\nplop\\n\\nplap", res2);
    StringReplaceAll(res3, str3, "test", "aaaaa");
    CuAssertStrEquals(tc, "foo bar", res3);
}

int main(int argc, char *argv[])
{
    CuString *output = CuStringNew();
    CuSuite* suite = CuSuiteNew();

    SUITE_ADD_TEST(suite, Test_StringReplaceAll);

    CuSuiteRun(suite);
    CuSuiteSummary(suite, output);
    CuSuiteDetails(suite, output);

    printf("%s\n", output->buffer);

    return suite->failCount;
}
