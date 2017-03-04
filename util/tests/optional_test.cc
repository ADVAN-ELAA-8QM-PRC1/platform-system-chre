/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "gtest/gtest.h"

#include "chre/util/optional.h"

using chre::Optional;

TEST(Optional, NoValueByDefault) {
  Optional<int> myInt;
  EXPECT_EQ(myInt.has_value(), false);
}

TEST(Optional, CopyAssignAndRead) {
  Optional<int> myInt;
  EXPECT_FALSE(myInt.has_value());
  myInt = 0x1337;
  EXPECT_EQ(*myInt, 0x1337);
  EXPECT_TRUE(myInt.has_value());
  myInt.reset();
  EXPECT_FALSE(myInt.has_value());
}

TEST(Optional, MoveAssignAndRead) {
  Optional<int> myInt;
  EXPECT_FALSE(myInt.has_value());
  myInt = std::move(0xcafe);
  EXPECT_TRUE(myInt.has_value());
  EXPECT_EQ(*myInt, 0xcafe);
}
