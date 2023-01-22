import 'dart:async';

import 'StreamIteratorCustomState.dart';

class StreamIteratorCustom<T> implements StreamIterator<T> {
  @pragma("vm:entry-point")
  State<T> _state;

  StreamIteratorCustom(final Stream<T> stream) : _state = State.init(stream);

  T get current {
    return _state.maybeWhen(
        hasValue: (_, value) => value!,
        orElse: () => throw StateError("Value is not ready."));
  }

  Future<bool> moveNext() {
    return _state.when(
      init: (s) => _initialize(s),
      finished: () => Future.value(false),
      waitForValue: (_, __) =>
          throw new StateError("Already waiting for next."),
      hasValue: (subscription, __) {
        final completer = new Completer<bool>();
        subscription.resume();
        _state = State.waitForValue(subscription, completer);
        return completer.future;
      },
    );
  }

  Future<bool> _initialize(Stream<T> stream) {
    var future = new Completer<bool>();
    final subscription = stream.listen(_onData,
        onError: _onError, onDone: _onDone, cancelOnError: true);
    _state = State.waitForValue(subscription, future);
    return future.future;
  }

  void cancelNext() {
    _state.maybeWhen(
        waitForValue: (ss, c) {
          c.complete(false);
          ss.pause();
          _state = State.hasValue(ss, null);
        },
        orElse: () => {});
  }

  Future cancel() async {
    await _state.when(
        init: (_) => Future.value(null),
        finished: () => Future.value(null),
        waitForValue: (ss, c) {
          c.complete(false);
          return ss.cancel();
        },
        hasValue: (ss, _) {
          return ss.cancel();
        });
    _state = State.finished();
  }

  void _onData(T data) {
    _state.when(
        init: (_) => {},
        waitForValue: (ss, c) {
          ss.pause();
          _state = State.hasValue(ss, data);
          c.complete(true);
        },
        hasValue: (_, __) {
          // when a value is obtained, the subscription is supposed to be paused
          assert(false);
        },
        finished: () => {});
  }

  void _onError(Object error, StackTrace stackTrace) {
    _state.maybeWhen(waitForValue: (ss, c) {
      c.completeError(error, stackTrace);
      _state = State.finished();
    }, orElse: () {
      _state = State.finished();
    });
  }

  void _onDone() {
    _state.maybeWhen(waitForValue: (ss, c) {
      c.complete(false);
      _state = State.finished();
    }, orElse: () {
      _state = State.finished();
    });
  }
}
