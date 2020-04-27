/* Copyright 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "brave/browser/brave_rewards/checkout_dialog.h"
#include "chrome/browser/payments/chrome_payment_request_delegate.h"
#include "components/payments/content/payment_request.h"
#include "content/public/browser/web_contents.h"

namespace {

using payments::ChromePaymentRequestDelegate;

constexpr char kBatPaymentMethod[] = "https://batpay.brave.com";

class BravePaymentRequestDelegate : public ChromePaymentRequestDelegate {
 public:
  explicit BravePaymentRequestDelegate(content::WebContents* web_contents)
      : ChromePaymentRequestDelegate(web_contents),
        web_contents_(web_contents),
        weak_factory_(this) {}

  ~BravePaymentRequestDelegate() override {}

  void ShowDialog(payments::PaymentRequest* request) override {
    DCHECK(request);
    request_ = request;

    auto* spec = request->spec();
    if (spec->stringified_method_data().count(kBatPaymentMethod) == 0) {
      ChromePaymentRequestDelegate::ShowDialog(request);
      return;
    }

    double total_amount = 0;
    base::StringToDouble(spec->details().total->amount->value, &total_amount);

    controller_ = brave_rewards::ShowCheckoutDialog(web_contents_, {
      spec->details().total->label,
      total_amount,
      {}  // TODO(zenparsing): Add sku/description items
    });

    controller_->SetOnDialogClosedCallback(base::BindOnce(
        &BravePaymentRequestDelegate::OnDialogClosed,
        weak_factory_.GetWeakPtr()));

    controller_->SetOnPaymentReadyCallback(base::BindOnce(
        &BravePaymentRequestDelegate::OnPaymentReady,
        weak_factory_.GetWeakPtr()));
  }

  void CloseDialog() override {
    if (!controller_) {
      ChromePaymentRequestDelegate::CloseDialog();
      return;
    }
    controller_->NotifyPaymentAborted();
  }

 private:
  void EnsureAppSelected() {
    using payments::PaymentRequestState;
    DCHECK(request_);
    for (auto& app : request_->state()->available_apps()) {
      if (app->GetAppMethodNames().count(kBatPaymentMethod) > 0) {
        request_->state()->SetSelectedApp(
            app.get(),
            PaymentRequestState::SectionSelectionStatus::kSelected);
      }
    }
  }

  void OnDialogClosed(bool payment_confirmed) {
    if (!payment_confirmed) {
      DCHECK(request_);
      EnsureAppSelected();
      request_->UserCancelled();
    }
  }

  void OnPaymentReady(const std::string& order_id) {
    if (controller_) {
      DCHECK(request_);

      EnsureAppSelected();
      // payment_app_->details_ = "{}";

      // NOTE: Bypass request_->Pay() because it will incorrectly
      // reinterpret the payment app as a ServiceWorkerPaymentApp
      request_->state()->GeneratePaymentResponse();

      controller_->NotifyPaymentConfirmed();
    }
  }

  content::WebContents* web_contents_;
  payments::PaymentRequest* request_ = nullptr;
  base::WeakPtr<brave_rewards::CheckoutDialogController> controller_;
  base::WeakPtrFactory<BravePaymentRequestDelegate> weak_factory_;
};

}  // namespace

#include "../../../../../chrome/browser/payments/payment_request_factory.cc"
