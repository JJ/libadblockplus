/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-present eyeo GmbH
 *
 * Adblock Plus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Adblock Plus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <cassert>
#include <functional>
#include <string>

#include <AdblockPlus/FilterEngineFactory.h>

#include "DefaultFilterEngine.h"
#include "JsContext.h"
#include "Thread.h"
#include "Utils.h"

using namespace AdblockPlus;

void FilterEngineFactory::CreateAsync(JsEngine& jsEngine,
                                      const EvaluateCallback& evaluateCallback,
                                      const OnCreatedCallback& onCreated,
                                      const CreationParameters& params)
{
  // Why wrap a unique_ptr in a shared_ptr? Because we cannot pass a
  // unique_ptr to an std::function - this would make it move-only and
  // STL doesn't like that. This is just a workaround, the function in
  // question retrieves the unique_ptr from within and keeps using that
  // or the reminder of the stack.
  auto wrappedFilterEngine =
      std::make_shared<std::unique_ptr<DefaultFilterEngine>>(new DefaultFilterEngine(jsEngine));
  auto* bareFilterEngine = wrappedFilterEngine->get();
  {
    auto isSubscriptionDownloadAllowedCallback = params.isSubscriptionDownloadAllowedCallback;
    jsEngine.SetEventCallback(
        "_isSubscriptionDownloadAllowed",
        [bareFilterEngine, isSubscriptionDownloadAllowedCallback](JsValueList&& params) {
          auto& jsEngine = bareFilterEngine->GetJsEngine();

          // param[0] - nullable string Prefs.allowed_connection_type
          // param[1] - function(Boolean)
          bool areArgumentsValid = params.size() == 2 &&
                                   (params[0].IsNull() || params[0].IsString()) &&
                                   params[1].IsFunction();
          assert(
              areArgumentsValid &&
              "Invalid argument: there should be two args and the second one should be a function");
          if (!areArgumentsValid)
            return;
          if (!isSubscriptionDownloadAllowedCallback)
          {
            params[1].Call(jsEngine.NewValue(true));
            return;
          }
          auto valuesID = jsEngine.StoreJsValues(params);
          auto callJsCallback = [bareFilterEngine, valuesID](bool isAllowed) {
            auto& jsEngine = bareFilterEngine->GetJsEngine();
            auto jsParams = jsEngine.TakeJsValues(valuesID);
            jsParams[1].Call(jsEngine.NewValue(isAllowed));
          };
          std::string allowedConnectionType =
              params[0].IsString() ? params[0].AsString() : std::string();
          isSubscriptionDownloadAllowedCallback(
              params[0].IsString() ? &allowedConnectionType : nullptr, callJsCallback);
        });
  }

  jsEngine.SetEventCallback("_init",
                            [&jsEngine, wrappedFilterEngine, onCreated](JsValueList&& /*params*/) {
                              auto uniqueFilterEngine = std::move(*wrappedFilterEngine);
                              onCreated(std::move(uniqueFilterEngine));
                              jsEngine.RemoveEventCallback("_init");
                            });

  bareFilterEngine->SetFilterChangeCallback(
      [bareFilterEngine](const std::string& reason, JsValue&&) {
        if (reason == "save")
          bareFilterEngine->GetJsEngine().NotifyLowMemory();
      });

  // Lock the JS engine while we are loading scripts, no timeouts should fire
  // until we are done.
  const JsContext context(jsEngine.GetIsolate(), jsEngine.GetContext());
  // Set the preconfigured prefs
  auto preconfiguredPrefsObject = jsEngine.NewObject();
  for (const auto& pref : params.preconfiguredPrefs)
  {
    preconfiguredPrefsObject.SetProperty(pref.first, pref.second);
  }
  jsEngine.SetGlobalProperty("_preconfiguredPrefs", preconfiguredPrefsObject);

  const auto& jsFiles = Utils::SplitString(ABP_SCRIPT_FILES, ' ');
  // Load adblockplus scripts
  for (const auto& filterEngineJsFile : jsFiles)
  {
    auto filepathComponents = Utils::SplitString(filterEngineJsFile, '/');
    evaluateCallback(filepathComponents.back());
  }
}
