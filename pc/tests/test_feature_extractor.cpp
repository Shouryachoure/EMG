/// @file test_feature_extractor.cpp
/// @brief Unit tests for FeatureExtractor.

#include "feature_extractor.h"
#include <vector>
#include <cmath>
#include <stdexcept>

#include "test_framework.h"

using namespace emg;

TEST(feature_extractor_individual) {
    std::vector<double> samples = {1.0, -2.0, 3.0, -4.0};

    // RMS: sqrt((1 + 4 + 9 + 16) / 4) = sqrt(30/4) = sqrt(7.5) = 2.7386127875
    ASSERT_NEAR(FeatureExtractor::computeRMS(samples), std::sqrt(7.5), 1e-6);

    // MAV: (1 + 2 + 3 + 4) / 4 = 10 / 4 = 2.5
    ASSERT_NEAR(FeatureExtractor::computeMAV(samples), 2.5, 1e-6);

    // Variance: mean = -0.5. diffs: 1.5, -1.5, 3.5, -3.5. sq: 2.25, 2.25, 12.25, 12.25. sum = 29.0. / 3 = 9.666667
    ASSERT_NEAR(FeatureExtractor::computeVariance(samples), 29.0 / 3.0, 1e-6);

    // WL: | -2 - 1 | + | 3 - (-2) | + | -4 - 3 | = 3 + 5 + 7 = 15
    ASSERT_NEAR(FeatureExtractor::computeWaveformLength(samples), 15.0, 1e-6);

    // ZC: 1->-2, -2->3, 3->-4 (all cross 0 and diff > 0.01) -> 3
    ASSERT_NEAR(FeatureExtractor::computeZeroCrossings(samples), 3.0, 1e-6);

    // SSC: 3 points needed for 1 change: diff1 = -3, diff2 = 5 (changes sign). Next: diff1 = 5, diff2 = -7 (changes sign). -> 2
    ASSERT_NEAR(FeatureExtractor::computeSlopeSignChanges(samples), 2.0, 1e-6);
}

TEST(feature_extractor_config) {
    FeatureConfig cfg;
    cfg.rms = true;
    cfg.mav = true;
    cfg.variance = false;
    cfg.waveform_length = false;
    cfg.zero_crossings = false;
    cfg.slope_sign_changes = false;

    FeatureExtractor ext(cfg);
    ASSERT_EQ(ext.featureCount(), 2u);

    std::vector<double> samples = {2.0, -2.0};
    auto vec = ext.extract(samples);
    ASSERT_EQ(vec.size(), 2u);
    ASSERT_NEAR(vec[0], 2.0, 1e-6); // RMS
    ASSERT_NEAR(vec[1], 2.0, 1e-6); // MAV

    auto names = ext.featureNames();
    ASSERT_EQ(names[0], "RMS");
    ASSERT_EQ(names[1], "MAV");
}
