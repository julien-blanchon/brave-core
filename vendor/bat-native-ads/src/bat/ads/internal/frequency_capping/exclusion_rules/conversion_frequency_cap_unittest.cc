/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

#include "base/time/time.h"

#include "bat/ads/internal/frequency_capping/exclusion_rule.h"
#include "bat/ads/internal/frequency_capping/frequency_capping.h"
#include "bat/ads/internal/frequency_capping/exclusion_rules/conversion_frequency_cap.h"

#include "bat/ads/internal/ads_client_mock.h"
#include "bat/ads/internal/ads_impl.h"
#include "bat/ads/creative_ad_notification_info.h"

// npm run test -- brave_unit_tests --filter=Ads*

using std::placeholders::_1;
using ::testing::_;
using ::testing::Invoke;

namespace {

const std::vector<std::string>             kTestCreativeSetIds = {
    "654f10df-fbc4-4a92-8d43-2edf73734a60",
    "465f10df-fbc4-4a92-8d43-4edf73734a60"
};

}  // namespace

namespace ads {

class BraveAdsConversionFrequencyCapTest : public ::testing::Test {
 protected:
    BraveAdsConversionFrequencyCapTest()
        : ads_client_mock_(std::make_unique<AdsClientMock>()),
          ads_(std::make_unique<AdsImpl>(ads_client_mock_.get())) {
    // You can do set-up work for each test here
  }

  ~BraveAdsConversionFrequencyCapTest() override {
    // You can do clean-up work that doesn't throw exceptions here
  }

  // If the constructor and destructor are not enough for setting up and
  // cleaning up each test, you can use the following methods

  void SetUp() override {
    // Code here will be called immediately after the constructor (right before
    // each test)

    auto callback = std::bind(
        &BraveAdsConversionFrequencyCapTest::OnAdsImplInitialize, this, _1);
    ads_->Initialize(callback);

    frequency_cap_ = std::make_unique<ConversionFrequencyCap>(ads_.get());
  }

  void OnAdsImplInitialize(const Result result) {
    EXPECT_EQ(Result::SUCCESS, result);
  }

  void TearDown() override {
    // Code here will be called immediately after each test (right before the
    // destructor)
  }

  void GeneratePastHistory(
      const std::string& creative_set_id,
      const uint64_t time_offset_in_seconds,
      const uint8_t count) {
    uint64_t now_in_seconds = base::Time::Now().ToDoubleT();

    for (uint8_t i = 0; i < count; i++) {
      now_in_seconds -= time_offset_in_seconds;

      ads_->get_client()->AppendTimestampToAdConversionHistory(creative_set_id,
          now_in_seconds);
    }
  }

  std::unique_ptr<AdsClientMock> ads_client_mock_;
  std::unique_ptr<AdsImpl> ads_;

  std::unique_ptr<ConversionFrequencyCap> frequency_cap_;
  CreativeAdNotificationInfo ad_notification_info_;
};

TEST_F(BraveAdsConversionFrequencyCapTest, AdAllowedWithNoAdHistory) {
  // Arrange
  ad_notification_info_.creative_set_id = kTestCreativeSetIds.at(0);

  // Act
  const bool should_exclude =
      frequency_cap_->ShouldExclude(ad_notification_info_);

  // Assert
  EXPECT_FALSE(should_exclude);
}

TEST_F(BraveAdsConversionFrequencyCapTest, AdNotAllowedWithMatchingAds) {
  // Arrange
  ad_notification_info_.creative_set_id = kTestCreativeSetIds.at(0);

  GeneratePastHistory(ad_notification_info_.creative_set_id,
      base::Time::kSecondsPerHour, 1);

  // Act
  const bool should_exclude =
      frequency_cap_->ShouldExclude(ad_notification_info_);

  // Assert
  EXPECT_TRUE(should_exclude);
}

TEST_F(BraveAdsConversionFrequencyCapTest, AdAllowedWithNonMatchingAds) {
  // Arrange
  GeneratePastHistory(kTestCreativeSetIds.at(0),
      base::Time::kSecondsPerHour, 1);

  ad_notification_info_.creative_set_id = kTestCreativeSetIds.at(1);

  // Act
  const bool should_exclude =
      frequency_cap_->ShouldExclude(ad_notification_info_);

  // Assert
  EXPECT_FALSE(should_exclude);
}

}  // namespace ads
