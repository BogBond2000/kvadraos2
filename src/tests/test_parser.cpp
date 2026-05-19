#include <gtest/gtest.h>
#include "../collector/StatsCollector.h"
#include <fstream>
#include <sstream>

TEST(ProcParserTest, CpuLineParse) {
    std::string line = "cpu  12345 6789 101112 131415 161718 192021 222324 252627";
    std::istringstream iss(line);
    std::string cpu_label;
    iss >> cpu_label;
    unsigned long long vals[8];
    for (int i=0; i<8; ++i) iss >> vals[i];
    EXPECT_EQ(vals[0], 12345);
    EXPECT_EQ(vals[2], 101112);
}

TEST(MemInfoTest, MemTotalParse) {
    std::string line = "MemTotal:       16384000 kB";
    unsigned long long val;
    sscanf(line.c_str(), "MemTotal: %llu kB", &val);
    EXPECT_EQ(val, 16384000);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}