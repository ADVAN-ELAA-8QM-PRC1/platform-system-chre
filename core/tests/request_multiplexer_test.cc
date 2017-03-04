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

#include <algorithm>

#include "gtest/gtest.h"

#include "chre/core/request_multiplexer.h"

using chre::RequestMultiplexer;

class DummyRequest {
 public:
  DummyRequest() : DummyRequest(0) {}

  DummyRequest(int priority) : mPriority(priority) {}

  bool isEquivalentTo(const DummyRequest& request) const {
    return (mPriority == request.mPriority);
  }

  DummyRequest generateIntersectionOf(const DummyRequest& request) const {
    return DummyRequest(std::max(mPriority, request.mPriority));
  }

  int getPriority() const {
    return mPriority;
  }

 private:
  int mPriority;
};

TEST(RequestMultiplexer, DefaultRequestDoesNotCauseNewMaximal) {
  RequestMultiplexer<DummyRequest> multiplexer;
  DummyRequest request;
  bool maximalRequestChanged;
  ASSERT_TRUE(multiplexer.addRequest(request, &maximalRequestChanged));
  EXPECT_FALSE(maximalRequestChanged);
  EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 0);
}

TEST(RequestMultiplexer, FirstHighPriorityRequestCausesNewMaximal) {
  RequestMultiplexer<DummyRequest> multiplexer;
  DummyRequest request(10);
  bool maximalRequestChanged;
  ASSERT_TRUE(multiplexer.addRequest(request, &maximalRequestChanged));
  EXPECT_TRUE(maximalRequestChanged);
  EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
}

TEST(RequestMultiplexer, NewLowerPriorityRequestDoesNotCauseNewMaximal) {
  RequestMultiplexer<DummyRequest> multiplexer;

  {
    DummyRequest request(10);
    bool maximalRequestChanged;
    ASSERT_TRUE(multiplexer.addRequest(request, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  }

  {
    DummyRequest request(5);
    bool maximalRequestChanged;
    ASSERT_TRUE(multiplexer.addRequest(request, &maximalRequestChanged));
    EXPECT_FALSE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  }
}

TEST(RequestMultiplexer, AddOneRemoveMaximal) {
  RequestMultiplexer<DummyRequest> multiplexer;
  DummyRequest request(10);
  bool maximalRequestChanged;
  ASSERT_TRUE(multiplexer.addRequest(request, &maximalRequestChanged));
  EXPECT_TRUE(maximalRequestChanged);
  EXPECT_EQ(multiplexer.getRequests()[0].getPriority(), 10);
  EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);

  DummyRequest defaultRequest;
  multiplexer.removeRequest(0, &maximalRequestChanged);
  EXPECT_TRUE(maximalRequestChanged);
  EXPECT_TRUE(multiplexer.getCurrentMaximalRequest()
      .isEquivalentTo(defaultRequest));
  EXPECT_TRUE(multiplexer.getRequests().empty());
}

TEST(RequestMultiplexer, AddManyRemoveMaximal) {
  RequestMultiplexer<DummyRequest> multiplexer;

  {
    DummyRequest request(10);
    bool maximalRequestChanged;
    ASSERT_TRUE(multiplexer.addRequest(request, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[0].getPriority(), 10);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  }

  {
    DummyRequest request(5);
    bool maximalRequestChanged;
    ASSERT_TRUE(multiplexer.addRequest(request, &maximalRequestChanged));
    EXPECT_FALSE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[1].getPriority(), 5);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  }

  {
    DummyRequest request(10);
    bool maximalRequestChanged;
    ASSERT_TRUE(multiplexer.addRequest(request, &maximalRequestChanged));
    EXPECT_FALSE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[2].getPriority(), 10);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  }

  bool maximalRequestChanged;
  multiplexer.removeRequest(0, &maximalRequestChanged);
  EXPECT_FALSE(maximalRequestChanged);
  EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  EXPECT_EQ(multiplexer.getRequests()[0].getPriority(), 5);
  EXPECT_EQ(multiplexer.getRequests()[1].getPriority(), 10);
}

TEST(RequestMultiplexer, AddManyRemoveBeforeMaximalThenRemoveMaximal) {
  RequestMultiplexer<DummyRequest> multiplexer;

  {
    DummyRequest request(1);
    bool maximalRequestChanged;
    ASSERT_TRUE(multiplexer.addRequest(request, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[0].getPriority(), 1);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 1);
  }

  {
    DummyRequest request(5);
    bool maximalRequestChanged;
    ASSERT_TRUE(multiplexer.addRequest(request, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[1].getPriority(), 5);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 5);
  }

  {
    DummyRequest request(10);
    bool maximalRequestChanged;
    ASSERT_TRUE(multiplexer.addRequest(request, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[2].getPriority(), 10);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  }

  bool maximalRequestChanged;
  multiplexer.removeRequest(0, &maximalRequestChanged);
  EXPECT_FALSE(maximalRequestChanged);
  EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  EXPECT_EQ(multiplexer.getRequests()[0].getPriority(), 5);
  EXPECT_EQ(multiplexer.getRequests()[1].getPriority(), 10);

  multiplexer.removeRequest(1, &maximalRequestChanged);
  EXPECT_TRUE(maximalRequestChanged);
  EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 5);
  EXPECT_EQ(multiplexer.getRequests()[0].getPriority(), 5);
}

TEST(RequestMultiplexer, AddManyRemoveAfterMaximalThenRemoveMaximal) {
  RequestMultiplexer<DummyRequest> multiplexer;

  {
    DummyRequest request(1);
    bool maximalRequestChanged;
    ASSERT_TRUE(multiplexer.addRequest(request, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[0].getPriority(), 1);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 1);
  }

  {
    DummyRequest request(5);
    bool maximalRequestChanged;
    ASSERT_TRUE(multiplexer.addRequest(request, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[1].getPriority(), 5);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 5);
  }

  {
    DummyRequest request(10);
    bool maximalRequestChanged;
    ASSERT_TRUE(multiplexer.addRequest(request, &maximalRequestChanged));
    EXPECT_TRUE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[2].getPriority(), 10);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  }

  {
    DummyRequest request(5);
    bool maximalRequestChanged;
    ASSERT_TRUE(multiplexer.addRequest(request, &maximalRequestChanged));
    EXPECT_FALSE(maximalRequestChanged);
    EXPECT_EQ(multiplexer.getRequests()[3].getPriority(), 5);
    EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  }

  bool maximalRequestChanged;
  multiplexer.removeRequest(3, &maximalRequestChanged);
  EXPECT_FALSE(maximalRequestChanged);
  EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 10);
  EXPECT_EQ(multiplexer.getRequests()[0].getPriority(), 1);
  EXPECT_EQ(multiplexer.getRequests()[1].getPriority(), 5);
  EXPECT_EQ(multiplexer.getRequests()[2].getPriority(), 10);

  multiplexer.removeRequest(2, &maximalRequestChanged);
  EXPECT_TRUE(maximalRequestChanged);
  EXPECT_EQ(multiplexer.getCurrentMaximalRequest().getPriority(), 5);
  EXPECT_EQ(multiplexer.getRequests()[0].getPriority(), 1);
  EXPECT_EQ(multiplexer.getRequests()[1].getPriority(), 5);
}
