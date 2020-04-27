/* Copyright 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include <string>

#include "components/payments/content/payment_app_factory.h"
#include "components/payments/content/payment_request_state.h"

namespace {

using payments::PaymentApp;

constexpr char kBatPaymentMethod[] = "https://batpay.brave.com";
constexpr auto kBatPaymentAppType = PaymentApp::Type::SERVICE_WORKER_APP;

class BATPaymentApp : public PaymentApp {
 public:
  BATPaymentApp() : PaymentApp(0, kBatPaymentAppType), weak_factory_(this) {
    app_method_names_.insert(kBatPaymentMethod);
  }

  ~BATPaymentApp() override {}

  void InvokePaymentApp(Delegate* delegate) override {
    delegate->OnInstrumentDetailsReady(
        kBatPaymentMethod,
        details_,
        payer_data_);
  }

  bool IsCompleteForPayment() const override {
    return true;
  }

  uint32_t GetCompletenessScore() const override {
    return 0;
  }

  bool CanPreselect() const override {
    return false;
  }

  base::string16 GetMissingInfoLabel() const override {
    return base::string16();
  }

  bool HasEnrolledInstrument() const override {
    return true;
  }

  void RecordUse() override {}

  bool NeedsInstallation() const override {
    return false;
  }

  base::string16 GetLabel() const override {
    return base::string16();
  }

  base::string16 GetSublabel() const override {
    return base::string16();
  }

  bool IsValidForModifier(
      const std::string& method,
      bool supported_networks_specified,
      const std::set<std::string>& supported_networks) const override {
    bool is_valid = false;
    IsValidForPaymentMethodIdentifier(method, &is_valid);
    return is_valid;
  }

  base::WeakPtr<PaymentApp> AsWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  bool HandlesShippingAddress() const override {
    return false;
  }

  bool HandlesPayerName() const override {
    return false;
  }

  bool HandlesPayerEmail() const override {
    return false;
  }

  bool HandlesPayerPhone() const override {
    return false;
  }

 private:
  std::string details_ = "{}";
  payments::PayerData payer_data_;
  base::WeakPtrFactory<BATPaymentApp> weak_factory_;
};

class BATPaymentAppFactory : public payments::PaymentAppFactory {
 public:
  BATPaymentAppFactory() : PaymentAppFactory(kBatPaymentAppType) {}
  ~BATPaymentAppFactory() override = default;

  // PaymentAppFactory:
  void Create(base::WeakPtr<Delegate> delegate) override {
    auto& methods = delegate->GetSpec()->payment_method_identifiers_set();
    if (methods.count(kBatPaymentMethod) > 0) {
      delegate->OnPaymentAppCreated(std::make_unique<BATPaymentApp>());
    }
    delegate->OnDoneCreatingPaymentApps();
  }
};

}  // namespace

#define BRAVE_ADD_BAT_PAYMENT_APP_FACTORY \
    factories_.emplace_back(std::make_unique<BATPaymentAppFactory>());

#include "../../../../../components/payments/content/payment_app_service.cc"

#undef BRAVE_ADD_BAT_PAYMENT_APP_FACTORY
