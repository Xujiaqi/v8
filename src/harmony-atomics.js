// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalObject = global.Object;

// -------------------------------------------------------------------


function CheckSharedTypedArray(sta) {
  if (!%_IsSharedTypedArray(sta)) {
    throw MakeTypeError(kNotSharedTypedArray, sta);
  }
}

function CheckSharedIntegerTypedArray(ia) {
  if (!%_IsSharedIntegerTypedArray(ia)) {
    throw MakeTypeError(kNotIntegerSharedTypedArray, ia);
  }
}

//-------------------------------------------------------------------

function AtomicsCompareExchangeJS(sta, index, oldValue, newValue) {
  CheckSharedTypedArray(sta);
  index = $toInteger(index);
  if (index < 0 || index >= %_TypedArrayGetLength(sta)) {
    return UNDEFINED;
  }
  oldValue = $toNumber(oldValue);
  newValue = $toNumber(newValue);
  return %_AtomicsCompareExchange(sta, index, oldValue, newValue);
}

function AtomicsLoadJS(sta, index) {
  CheckSharedTypedArray(sta);
  index = $toInteger(index);
  if (index < 0 || index >= %_TypedArrayGetLength(sta)) {
    return UNDEFINED;
  }
  return %_AtomicsLoad(sta, index);
}

function AtomicsStoreJS(sta, index, value) {
  CheckSharedTypedArray(sta);
  index = $toInteger(index);
  if (index < 0 || index >= %_TypedArrayGetLength(sta)) {
    return UNDEFINED;
  }
  value = $toNumber(value);
  return %_AtomicsStore(sta, index, value);
}

function AtomicsAddJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = $toInteger(index);
  if (index < 0 || index >= %_TypedArrayGetLength(ia)) {
    return UNDEFINED;
  }
  value = $toNumber(value);
  return %_AtomicsAdd(ia, index, value);
}

function AtomicsSubJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = $toInteger(index);
  if (index < 0 || index >= %_TypedArrayGetLength(ia)) {
    return UNDEFINED;
  }
  value = $toNumber(value);
  return %_AtomicsSub(ia, index, value);
}

function AtomicsAndJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = $toInteger(index);
  if (index < 0 || index >= %_TypedArrayGetLength(ia)) {
    return UNDEFINED;
  }
  value = $toNumber(value);
  return %_AtomicsAnd(ia, index, value);
}

function AtomicsOrJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = $toInteger(index);
  if (index < 0 || index >= %_TypedArrayGetLength(ia)) {
    return UNDEFINED;
  }
  value = $toNumber(value);
  return %_AtomicsOr(ia, index, value);
}

function AtomicsXorJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = $toInteger(index);
  if (index < 0 || index >= %_TypedArrayGetLength(ia)) {
    return UNDEFINED;
  }
  value = $toNumber(value);
  return %_AtomicsXor(ia, index, value);
}

function AtomicsExchangeJS(ia, index, value) {
  CheckSharedIntegerTypedArray(ia);
  index = $toInteger(index);
  if (index < 0 || index >= %_TypedArrayGetLength(ia)) {
    return UNDEFINED;
  }
  value = $toNumber(value);
  return %_AtomicsExchange(ia, index, value);
}

function AtomicsIsLockFreeJS(size) {
  return %_AtomicsIsLockFree(size);
}

// -------------------------------------------------------------------

function AtomicsConstructor() {}

var Atomics = new AtomicsConstructor();

%InternalSetPrototype(Atomics, GlobalObject.prototype);
%AddNamedProperty(global, "Atomics", Atomics, DONT_ENUM);
%FunctionSetInstanceClassName(AtomicsConstructor, 'Atomics');

%AddNamedProperty(Atomics, symbolToStringTag, "Atomics", READ_ONLY | DONT_ENUM);

utils.InstallFunctions(Atomics, DONT_ENUM, [
  "compareExchange", AtomicsCompareExchangeJS,
  "load", AtomicsLoadJS,
  "store", AtomicsStoreJS,
  "add", AtomicsAddJS,
  "sub", AtomicsSubJS,
  "and", AtomicsAndJS,
  "or", AtomicsOrJS,
  "xor", AtomicsXorJS,
  "exchange", AtomicsExchangeJS,
  "isLockFree", AtomicsIsLockFreeJS,
]);

})