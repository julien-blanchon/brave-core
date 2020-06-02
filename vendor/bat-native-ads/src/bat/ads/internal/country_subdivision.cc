/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/country_subdivision.h"

#include <stdint.h>

#include <sstream>
#include <string>

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "brave/components/l10n/browser/locale_helper.h"
#include "brave/components/l10n/common/locale_util.h"
#include "bat/ads/internal/country_subdivision_codes.h"
#include "bat/ads/internal/logging.h"
#include "bat/ads/internal/static_values.h"
#include "bat/ads/internal/time_util.h"

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

namespace ads {

CountrySubdivision::CountrySubdivision(
    AdsClient* ads_client)
    : ads_client_(ads_client) {
  DCHECK(ads_client_);
  BuildUrl();
}

CountrySubdivision::~CountrySubdivision() = default;

void CountrySubdivision::FetchCountrySubdivisionIfAllowedForRegion() {
  std::string locale = brave_l10n::LocaleHelper::GetInstance()->GetLocale();
  const std::string region_code = brave_l10n::GetRegionCode(locale);
  const auto iter = kCountrySubdivisionAllowedForRegions.find(region_code);
  if (iter == kCountrySubdivisionAllowedForRegions.end()) {
    ads_client_->SetSubdivisionAdTargetingRegion(false);
    return;
  }

  ads_client_->SetSubdivisionAdTargetingRegion(true);

  if (!ads_client_->DidOverrideAdsSubdivision()) {
    Fetch();
  }
}

std::string CountrySubdivision::GetCountrySubdivision() const {
  return ads_client_->GetCountrySubdivision();
}

bool CountrySubdivision::IsValidSubdivisionCountryCode(
    const std::string& country_subdivision_code) const {
  const auto iter = kValidSubdivisionCountryCodes.find(
      country_subdivision_code);
  if (iter == kValidSubdivisionCountryCodes.end()) {
    return false;
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////

void CountrySubdivision::Fetch() {
  if (retry_timer_.IsRunning()) {
    return;
  }

  BLOG(1, "Fetch country subdivision");
  BLOG(2, "GET " << GETSTATE_PATH);

  auto callback = std::bind(&CountrySubdivision::OnFetchCountrySubdivision,
      this, url_, _1, _2, _3);
  ads_client_->URLRequest(url_, {}, "", "", URLRequestMethod::GET, callback);

  return;
}

void CountrySubdivision::OnFetchCountrySubdivision(
    const std::string& url,
    const uint16_t response_status_code,
    const std::string& response,
    const std::map<std::string, std::string>& headers) {
  BLOG(7, UrlResponseToString(url, response_status_code, response, headers));

  bool should_retry = false;

  if (response_status_code / 100 == 2) {
    if (!response.empty()) {
      BLOG(1, "Successfully fetched country subdivision");
    }

    if (!Process(response)) {
      BLOG(1, "Failed to parse country subdivision");
      should_retry = true;
    }
  } else if (response_status_code == 304) {
    BLOG(1, "Country subdivision is up to date");
  } else {
    BLOG(1, "Failed to fetch country subdivision");

    should_retry = true;
  }

  if (should_retry) {
    Retry();
    return;
  }

  retry_timer_.Stop();

  FetchAfterDelay();
}

bool CountrySubdivision::Process(
    const std::string& json) {
  base::Optional<base::Value> root = base::JSONReader::Read(json);
  if (!root) {
    return false;
  }

  std::string country = *root->FindStringKey("country");
  if (country.empty()) {
    return false;
  }

  std::string region = *root->FindStringKey("region");
  if (region.empty()) {
    return false;
  }

  std::string country_subdivision =
      base::StringPrintf("%2s-%1s", country.c_str(), region.c_str());
  ads_client_->SetCountrySubdivision(country_subdivision);
  BLOG(3, "Country subdivision set to " << country_subdivision);

  return true;
}

void CountrySubdivision::Retry() {
  const base::Time time = retry_timer_.StartWithBackoff(
      kRetryFetchCountrySubdivisionAfterSeconds,
          base::BindOnce(&CountrySubdivision::Fetch, base::Unretained(this)));

  BLOG(1, "Retry fetching country subdivision " << FriendlyDateAndTime(time));
}

void CountrySubdivision::FetchAfterDelay() {
  const uint64_t delay = _is_debug ? kDebugFetchCountrySubdivisionPing :
      kDefaultFetchCountrySubdivisionPing;

  const base::Time time = timer_.StartWithPrivacy(delay,
      base::BindOnce(&CountrySubdivision::Fetch, base::Unretained(this)));

  BLOG(1, "Fetch country subdivision " << FriendlyDateAndTime(time));
}

void CountrySubdivision::BuildUrl() {
  switch (_environment) {
    case Environment::PRODUCTION: {
      url_ = PRODUCTION_SERVER;
      break;
    }

    case Environment::STAGING: {
      url_ = STAGING_SERVER;
      break;
    }

    case Environment::DEVELOPMENT: {
      url_ = DEVELOPMENT_SERVER;
      break;
    }
  }

  url_ += GETSTATE_PATH;
}

}  // namespace ads
