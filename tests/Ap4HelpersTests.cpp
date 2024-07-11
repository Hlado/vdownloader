#include <vd/Ap4Helpers.h>

#include <gtest/gtest.h>

TEST(Ap4HelpersTests, AP4_ArrayRange)
{
    AP4_Array<int> arr;
    const AP4_Array<int> &carr = arr;
    int acc{0};

    ASSERT_EQ(begin(arr), end(arr));
    for(auto i : arr)
    {
        acc += i;
    }
    ASSERT_EQ(0, acc);

    ASSERT_EQ(begin(carr), end(carr));
    for(auto i : carr)
    {
        acc += i;
    }
    ASSERT_EQ(0, acc);

    arr.Append(1);
    arr.Append(2);
    arr.Append(3);

    for(auto i : carr)
    {
        acc += i;
    }
    ASSERT_EQ(6, acc);

    for(auto &i : arr)
    {
        i -= 1;
    }
    for(auto i : carr)
    {
        acc -= i;
    }
    ASSERT_EQ(3, acc);
}