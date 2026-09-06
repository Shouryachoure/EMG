/// @file test_mock_ml.cpp
/// @brief Unit tests for MockMLModel.

#include "mock_ml_model.h"
#include <vector>
#include <cmath>
#include <stdexcept>

#include "test_framework.h"

using namespace emg;

TEST(mock_ml_not_ready_predict) {
    MockMLModel model;
    ASSERT_FALSE(model.isReady());

    auto pred = model.predict({0.1});
    ASSERT_EQ(pred.class_id, 0);
    ASSERT_NEAR(pred.confidence, 0.0, 1e-6);
}

TEST(mock_ml_classification_ranges) {
    MockMLModel model;
    ASSERT_TRUE(model.loadModel());
    ASSERT_TRUE(model.isReady());

    // Magnitude < 0.2 -> Class 1 (RELAX), conf 0.95
    auto p1 = model.predict({0.1});
    ASSERT_EQ(p1.class_id, 1);
    ASSERT_NEAR(p1.confidence, 0.95, 1e-6);

    // Magnitude 0.2 .. 0.5 -> Class 2 (GRASP), conf 0.85
    auto p2 = model.predict({0.35});
    ASSERT_EQ(p2.class_id, 2);
    ASSERT_NEAR(p2.confidence, 0.85, 1e-6);

    // Magnitude 0.5 .. 0.8 -> Class 3 (OPEN), conf 0.80
    auto p3 = model.predict({0.65});
    ASSERT_EQ(p3.class_id, 3);
    ASSERT_NEAR(p3.confidence, 0.80, 1e-6);

    // Magnitude >= 0.8 -> Class 4 (CLOSE), conf 0.90
    auto p4 = model.predict({0.95});
    ASSERT_EQ(p4.class_id, 4);
    ASSERT_NEAR(p4.confidence, 0.90, 1e-6);
}
