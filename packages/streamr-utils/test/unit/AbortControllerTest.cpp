#include <gtest/gtest.h>

import streamr.utils;

using streamr::utils::AbortController;

TEST(AbortController, ItCanBeConstructed) {
    AbortController controller;
}
