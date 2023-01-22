import 'dart:async';

import 'package:freezed_annotation/freezed_annotation.dart';
import 'package:flutter/foundation.dart';

part 'StreamIteratorCustomState.freezed.dart';

// To generate, run `flutter pub run build_runner build`

@freezed
class State<T> with _$State<T> {
  const factory State.init(Stream<T> s) = Init;
  const factory State.waitForValue(StreamSubscription ss, Completer<bool> c) =
      WaitForValue;
  const factory State.hasValue(StreamSubscription ss, T? value) = HasValue;
  const factory State.finished() = Finished;
}
