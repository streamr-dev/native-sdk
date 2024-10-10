#include "streamr-utils/Branded.hpp"
#include <gtest/gtest.h>

using streamr::utils::Branded;
using streamr::utils::operator""_brand; // NOLINT(misc-unused-using-decls)

TEST(Branded, TestIntCanBeBranded) {
    using TestId = Branded<int, decltype("TestId"_brand)>;

    constexpr int intValue = 42;

    auto id = TestId(42); // NOLINT(readability-magic-numbers)
    EXPECT_EQ(id, intValue);
}

TEST(Branded, TestIntCanBeBrandedUsingInitializerList) {
    using TestId = Branded<int, decltype("TestId"_brand)>;

    constexpr int intValue = 42;

    auto id = TestId{42}; // NOLINT(readability-magic-numbers)
    EXPECT_EQ(id, intValue);
}

TEST(Branded, TestConstexprIntCanBeBrandedWithMove) {
    using TestId = Branded<int, decltype("TestId"_brand)>;

    constexpr int intValue = 42;

    auto id = TestId(std::move(intValue)); // NOLINT(performance-move-const-arg)
    EXPECT_EQ(id, intValue);
}

TEST(Branded, TestConstexprIntCanBeBrandedWithoutMove) {
    using TestId = Branded<int, decltype("TestId"_brand)>;

    constexpr int intValue = 42;

    auto id = TestId(intValue);
    EXPECT_EQ(id, intValue);
}

TEST(Branded, TestIntMoveConstructor) {
    using TestId = Branded<int, decltype("TestId"_brand)>;

    TestId original(42); // NOLINT(readability-magic-numbers)
    TestId moved(std::move(original));

    EXPECT_EQ(moved, 42);
}

TEST(Branded, TestIntCopyConstructor) {
    using TestId = Branded<int, decltype("TestId"_brand)>;

    TestId original(42); // NOLINT(readability-magic-numbers)
    TestId copied(original);

    EXPECT_EQ(original, 42);
    EXPECT_EQ(copied, 42);
}

TEST(Branded, TestIntMoveAssignmentOperator) {
    using TestId = Branded<int, decltype("TestId"_brand)>;

    TestId original(42); // NOLINT(readability-magic-numbers)
    TestId assigned(0);

    assigned = std::move(original);

    EXPECT_EQ(assigned, 42);
}

TEST(Branded, TestIntCopyAssignmentOperator) {
    using TestId = Branded<int, decltype("TestId"_brand)>;

    TestId original(42); // NOLINT(readability-magic-numbers)
    TestId assigned(0);

    assigned = original;

    EXPECT_EQ(original, 42);
    EXPECT_EQ(assigned, 42);
}

TEST(Branded, TestStringConversion) {
    using TestString = Branded<std::string, decltype("TestString"_brand)>;

    const std::string stringValue = "Hello, World!";

    auto str = TestString(stringValue);
    EXPECT_EQ(str, stringValue);
}

TEST(Branded, TestStringMoveConstructor) {
    using TestString = Branded<std::string, decltype("TestString"_brand)>;

    TestString original("Hello, World!");
    TestString moved(std::move(original));

    EXPECT_EQ(moved, "Hello, World!");
}

TEST(Branded, TestStringCopyConstructor) {
    using TestString = Branded<std::string, decltype("TestString"_brand)>;

    TestString original("Hello, World!");
    TestString copied( // NOLINT(performance-unnecessary-copy-initialization)
        original);

    EXPECT_EQ(original, "Hello, World!");
    EXPECT_EQ(copied, "Hello, World!");
}

TEST(Branded, TestStringMoveAssignmentOperator) {
    using TestString = Branded<std::string, decltype("TestString"_brand)>;

    TestString original("Hello, World!");
    TestString assigned("");

    assigned = std::move(original);

    EXPECT_EQ(assigned, "Hello, World!");
}

TEST(Branded, TestStringCopyAssignmentOperator) {
    using TestString = Branded<std::string, decltype("TestString"_brand)>;

    TestString original("Hello, World!");
    TestString assigned("");

    assigned = original;

    EXPECT_EQ(original, "Hello, World!");
    EXPECT_EQ(assigned, "Hello, World!");
}

TEST(Branded, TestStringMemberFunctionsInBrandedString) {
    using TestString = Branded<std::string, decltype("TestString"_brand)>;

    TestString str("Hello");

    // Test size() and length()
    EXPECT_EQ(str.size(), 5);
    EXPECT_EQ(str.length(), 5);

    // Test empty()
    EXPECT_FALSE(str.empty());

    // Test append()
    str.append(", World!");
    EXPECT_EQ(str, "Hello, World!");

    // Test substr()
    EXPECT_EQ(str.substr(0, 5), "Hello");

    // Test find()
    EXPECT_EQ(str.find("World"), 7);

    // Test at()
    EXPECT_EQ(str.at(0), 'H');

    // Test operator[]
    EXPECT_EQ(str[1], 'e');

    // Test clear()
    str.clear();
    EXPECT_TRUE(str.empty());

    // Test operator+=
    str += "New String";
    EXPECT_EQ(str, "New String");

    // Test insert()
    str.insert(0, "A ");
    EXPECT_EQ(str, "A New String");

    // Test erase()
    str.erase(2, 4); // Erase "New "
    EXPECT_EQ(str, "A String");

    // Test replace()
    str.replace(2, 6, "Test"); // NOLINT(readability-magic-numbers)
    EXPECT_EQ(str, "A Test");
}
