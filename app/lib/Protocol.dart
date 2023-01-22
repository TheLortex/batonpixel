import 'dart:async';

import 'package:flutter/foundation.dart';

abstract class Parse<T> {
  int id();
  T parse(ByteData data);

  T expect(ByteData lst) {
    int gotLength = lst.getUint32(0, Endian.little);
    int gotId = lst.getUint8(4);

    assert(gotId == id());
    assert(gotLength + 5 == lst.lengthInBytes);

    return parse(ByteData.sublistView(lst, 5, 5 + gotLength));
  }
}

abstract class Send<T> {
  Uint8List serialize(T v);
  int id();

  void write(StreamSink s, T v) {
    final Uint8List l = serialize(v);
    final ByteData frame = ByteData(5 + l.length);
    frame.setUint32(0, l.length, Endian.little);
    frame.setUint8(4, id());
    final lst = frame.buffer.asUint8List();
    lst.setRange(5, 5 + l.length, l);

    s.add(lst);
  }
}

class Hello extends Send<Null> {
  int id() {
    return 0;
  }

  Uint8List serialize(Null _) {
    return Uint8List(0);
  }
}

class PixelCount extends Parse<int> {
  int id() {
    return 1;
  }

  int parse(ByteData data) {
    return ByteData.sublistView(data).getUint32(0, Endian.little);
  }
}

class PixelData extends Send<Uint8List> {
  int id() {
    return 2;
  }

  Uint8List serialize(Uint8List v) {
    return v;
  }
}

class PixelBegin extends Send<int> {
  int id() {
    return 3;
  }

  Uint8List serialize(int v) {
    return Uint8List.fromList([v]);
  }
}

class PixelAck extends Parse<int> {
  int id() {
    return 4;
  }

  int parse(ByteData data) {
    return ByteData.sublistView(data).getUint32(0, Endian.little);
  }
}

class PixelEnd extends Send<Null> {
  int id() {
    return 5;
  }

  Uint8List serialize(Null _) {
    return Uint8List(0);
  }
}
