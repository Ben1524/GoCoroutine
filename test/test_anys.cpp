#include <common/anys.h>
#include <gtest/gtest.h>
using namespace cxk;

// 定义两个不同的分组，确保类型注册和实例属于同一分组
struct Group1 {}; // 分组1：存储 int 和 tmp
struct Group2 {}; // 分组2：存储 double

// 测试用结构体（包含资源管理）
struct tmp
{
     int p ;

    tmp()
    {
        std::cout << "tmp constructor" << std::endl;
        p =90;

    }

    ~tmp()
    {
        std::cout << "tmp destructor" << std::endl;
        // delete[] p; // 释放内存
        // p = nullptr;
    }
};

TEST(Anys, Basic)
{
     // 注册类型（注意分组归属）
     auto idx1 = Anys<Group1>::Register<int>();          // Group1 注册 int
     // auto idx2 = Anys<Group2>::Register<double>();       // Group2 注册 double
     auto idx3 = Anys<Group1>::Register<tmp>();          // Group1 注册 tmp

     // 创建 Group1 实例（管理 int 和 tmp）
     Anys<Group1> anys_group1;
     // auto &int_val = anys_group1.get<int>(idx1);
     // int_val = 42; // 设置 int 值
     //
     // auto &tmp_val = anys_group1.get<tmp>(idx3);
     // // tmp_val.p[0] = 1; // 修改 tmp 实例的数组值
     // // EXPECT_EQ(tmp_val.p[0], 1); // 验证 tmp 数据正确性
     //
     // // 创建 Group2 实例（管理 double）
     // Anys<Group2> anys_group2;
     // auto &double_val = anys_group2.get<double>(idx2);
     // double_val = 3.14; // 设置 double 值
     // EXPECT_DOUBLE_EQ(double_val, 3.14);
     //
     // // 测试类型安全：同一分组内类型不匹配
     // EXPECT_THROW({
     //     anys_group1.get<double>(idx1); //越界访问
     // }, std::logic_error);

}