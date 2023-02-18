#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CuTest.h"

static int StringReplaceAll(char *res, unsigned int len, const char *str, const char *a, const char *b)
{
    char *substr = strstr(str, a);
    char *res2 = malloc(len);

    if (substr)
    {
        size_t len_a = strlen(a);
        size_t len_b = strlen(b);

        if (strlen(str) + (len_b - len_a) >= len)
        {
            return -1;
        }

        int pos = substr - str;

        strncpy(res2, str, pos);
        strncpy(res2 + pos, b, len_b);
        strncpy(res2 + pos + len_b, str + pos + len_a, len - (pos + len_a) + 1);
        memcpy(res, res2, len);
        StringReplaceAll(res, len, res2, a, b);
        free(res2);
    }
    else
    {
        memcpy(res, str, len);
    }

    return 0;
}

void Test_StringReplaceAll(CuTest *tc)
{
    const char *str = "foo bar plop foo plap test foo";
    const char *str2 = "foo bar\nplop\n\nplap";
    const char *str3 = "foo bar";
    char res[128] = {0};
    char res2[128] = {0};
    char res3[16] = {0};

    CuAssertIntEquals(tc, 0, StringReplaceAll(res, sizeof(res), str, "foo", "hello"));
    CuAssertStrEquals(tc, "hello bar plop hello plap test hello", res);
    CuAssertIntEquals(tc, 0, StringReplaceAll(res2, sizeof(res2), str2, "\n", "\\n"));
    CuAssertStrEquals(tc, "foo bar\\nplop\\n\\nplap", res2);
    CuAssertIntEquals(tc, 0, StringReplaceAll(res3, sizeof(res3), str3, "bar", "aaaaa"));
    CuAssertIntEquals(tc, -1, StringReplaceAll(res3, sizeof(res3), str3, "bar", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
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