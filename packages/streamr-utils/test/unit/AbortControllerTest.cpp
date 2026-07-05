#include <gtest/gtest.h>

import streamr.utils.AbortController;

using streamr::utils::AbortController;

TEST(AbortController, ItCanBeConstructed) {
    AbortController controller;
}
