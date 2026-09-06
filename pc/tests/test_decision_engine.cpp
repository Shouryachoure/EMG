/// @file test_decision_engine.cpp
/// @brief Unit tests for DecisionEngine.

#include "decision_engine.h"
#include <stdexcept>

#include "test_framework.h"

using namespace emg;

TEST(decision_engine_mapping) {
    DecisionEngine engine(0.6); // threshold 0.6

    // Valid high-confidence classes
    Prediction p1{1, 0.9};
    auto d1 = engine.decide(p1);
    ASSERT_EQ(d1.command, Command::RELAX);
    ASSERT_EQ(d1.class_id, 1);

    Prediction p2{2, 0.85};
    auto d2 = engine.decide(p2);
    ASSERT_EQ(d2.command, Command::GRASP);

    Prediction p3{3, 0.8};
    auto d3 = engine.decide(p3);
    ASSERT_EQ(d3.command, Command::OPEN);

    Prediction p4{4, 0.95};
    auto d4 = engine.decide(p4);
    ASSERT_EQ(d4.command, Command::CLOSE);
}

TEST(decision_engine_low_confidence_gates_to_none) {
    DecisionEngine engine(0.75);

    // Below threshold -> Command::NONE
    Prediction low_conf{2, 0.70};
    auto d = engine.decide(low_conf);
    ASSERT_EQ(d.command, Command::NONE);
    ASSERT_EQ(d.class_id, 2);

    // Unmapped class -> Command::NONE
    Prediction unmapped{99, 0.99};
    auto d_unmapped = engine.decide(unmapped);
    ASSERT_EQ(d_unmapped.command, Command::NONE);
}

TEST(decision_engine_proportional_and_postures) {
    DecisionEngine engine(0.5);

    // GRASP posture
    auto d_grasp = engine.decide(Prediction{2, 0.92});
    ASSERT_EQ(d_grasp.command, Command::GRASP);
    ASSERT_EQ(d_grasp.intensity, 92);
    ASSERT_EQ(d_grasp.finger_angles[0], 0);
    ASSERT_EQ(d_grasp.finger_angles[1], 0);
    ASSERT_EQ(d_grasp.finger_angles[2], 0);
    ASSERT_EQ(d_grasp.finger_angles[3], 0);
    ASSERT_EQ(d_grasp.finger_angles[4], 0);

    // OPEN posture
    auto d_open = engine.decide(Prediction{3, 0.75});
    ASSERT_EQ(d_open.command, Command::OPEN);
    ASSERT_EQ(d_open.intensity, 75);
    ASSERT_EQ(d_open.finger_angles[0], 180);
    ASSERT_EQ(d_open.finger_angles[1], 180);

    // CLOSE / Pinch posture (Thumb + Index close, others neutral 90)
    auto d_close = engine.decide(Prediction{4, 0.88});
    ASSERT_EQ(d_close.command, Command::CLOSE);
    ASSERT_EQ(d_close.intensity, 88);
    ASSERT_EQ(d_close.finger_angles[0], 0);
    ASSERT_EQ(d_close.finger_angles[1], 0);
    ASSERT_EQ(d_close.finger_angles[2], 90);
    ASSERT_EQ(d_close.finger_angles[3], 90);
    ASSERT_EQ(d_close.finger_angles[4], 90);
}

