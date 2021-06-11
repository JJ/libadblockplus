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

#pragma once

#include "JsEngine.h"

namespace AdblockPlus
{
  class JsContext
  {
  public:
    JsContext(v8::Isolate* isolate, const v8::Global<v8::Context>& context);

    v8::Local<v8::Context> GetV8Context() const
    {
      return context;
    }

  private:
    const v8::Locker locker;
    const v8::Isolate::Scope isolateScope;
    const v8::HandleScope handleScope;
    const v8::Local<v8::Context> context;
    const v8::Context::Scope contextScope;
  };
}
