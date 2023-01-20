import 'dart:io';

import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';
import 'dart:isolate';
import 'package:file_picker/file_picker.dart';
import 'package:flutter/foundation.dart' as ft;
import 'package:image/image.dart' as img;
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:flutter_bluetooth_serial/flutter_bluetooth_serial.dart';

class DecodeParam {
  final Uint8List fileBytes;
  final SendPort sendPort;
  DecodeParam(this.fileBytes, this.sendPort);
}

img.Image? prepareImageSync(
    Uint8List fileBytes, double widthFactor, double brightness) {
  var image = img.decodeImage(fileBytes);

  if (image == null) {
    return null;
  }

  if (image.height > image.width) {
    image = img.copyRotate(image, angle: 90);
  }

  for (final pixel in image) {
    pixel.r = pixel.r * pixel.a * brightness / 255;
    pixel.g = pixel.g * pixel.a * brightness / 255;
    pixel.b = pixel.b * pixel.a * brightness / 255;
    pixel.a = 255;
  }

  final width = (widthFactor * image.width * 144 / image.height).round();

  return img.copyResize(image,
      height: 144, width: width, interpolation: img.Interpolation.cubic);
}

Future<img.Image?> prepareImage(PlatformFile file,
    {required double widthFactor, required double brightness}) async {
  var receivePort = ReceivePort();

  await Isolate.spawn((DecodeParam p) {
    ft.debugPrint("Preparing image");
    final image = prepareImageSync(p.fileBytes, widthFactor, brightness);
    ft.debugPrint("Image ready");
    p.sendPort.send(image);
  }, DecodeParam(file.bytes!, receivePort.sendPort));

  return (await receivePort.first) as img.Image?;
}

class ConnectedWidget extends StatefulWidget {
  const ConnectedWidget(
      {super.key,
      required this.connection,
      required this.pixels,
      required this.input});

  final BluetoothConnection connection;
  final int pixels;
  final Stream input;

  @override
  _ConnectedWidget createState() => new _ConnectedWidget();
}

class _ConnectedWidget extends State<ConnectedWidget> {
  img.Image? _image;
  Uint8List? _imageRender;
  PlatformFile? _file;
  double _delay = 5;
  double _speed = 30;
  double _widthFactor = 1;
  double _brightness = 1;
  int? _streaming;

  @override
  Widget build(BuildContext context) {
    Widget image;

    if (_imageRender != null) {
      image = SingleChildScrollView(
        scrollDirection: Axis.horizontal,
        child: SizedBox(
          height: widget.pixels * 2,
          width: _image!.width * 2,
          child: Image.memory(_imageRender!, fit: BoxFit.cover),
        ),
      );
    } else {
      image = ListTile(title: Text('Select an image'));
    }

    Widget streamingControl;

    if (_imageRender == null) {
      streamingControl = ElevatedButton(
          onPressed: null, child: Text("Please select an image"));
    } else if (_streaming == null) {
      streamingControl = ElevatedButton(
          onPressed: () {
            streamImage();
          },
          child: Text('Start'));
    } else {
      streamingControl = Row(
        mainAxisAlignment: MainAxisAlignment.spaceEvenly,
        children: [
          ElevatedButton(
              onPressed: null,
              child: Text('Streaming image: ' +
                  _streaming.toString() +
                  ' / ' +
                  _image!.width.toString())),
          ElevatedButton(
              onPressed: () {
                setState(() {
                  _streaming = null;
                });
              },
              child: Text("STOP"))
        ],
      );
    }
    return Column(
      children: [
        ListTile(title: const Text('Select image')),
        ListTile(
            title: ElevatedButton(
          child: const Text('Select'),
          onPressed: () {
            FilePicker.platform
                .pickFiles(type: FileType.image, withData: true)
                .then((f) {
              if (f != null) {
                final file = f.files.first;
                prepareImage(file, widthFactor: 1, brightness: 1).then((image) {
                  if (image != null) {
                    setState(() => {
                          _image = image,
                          _imageRender = img.encodeJpg(image),
                          _file = file,
                          _widthFactor = 1,
                          _brightness = 1,
                        });
                  }
                });
              }
            });
          },
        )),
        image,
        ListTile(
            title: Row(
          children: [
            Text("delay: " + _delay.toStringAsFixed(1) + "s"),
            Expanded(
              child: Slider(
                  value: _delay,
                  min: 0,
                  max: 10,
                  divisions: 20,
                  label: "delay",
                  onChanged: (v) {
                    setState(() => {_delay = v});
                  }),
            )
          ],
        )),
        ListTile(
            title: Row(
          children: [
            Text("speed: " + _speed.toInt().toString() + "px/s"),
            Expanded(
              child: Slider(
                  value: _speed,
                  min: 1,
                  max: 125,
                  label: "Speed",
                  onChanged: (v) {
                    setState(() => {_speed = v});
                  }),
            )
          ],
        )),
        ListTile(
            title: Row(
          children: [
            Text("width factor: " + _widthFactor.toStringAsFixed(1).toString()),
            Expanded(
                child: Slider(
                    value: _widthFactor,
                    min: 1,
                    max: 10,
                    divisions: 20,
                    label: "Width factor",
                    onChanged: (v) {
                      setState(() => {_widthFactor = v});
                    },
                    onChangeEnd: (v) {
                      if (_file != null) {
                        prepareImage(_file as PlatformFile,
                                widthFactor: v, brightness: _brightness)
                            .then((image) {
                          if (image != null) {
                            setState(() => {
                                  _image = image,
                                  _imageRender = img.encodeJpg(image),
                                  _widthFactor = v
                                });
                          }
                        });
                      }
                    }))
          ],
        )),
        ListTile(
            title: Row(
          children: [
            Text("brightness: " + _brightness.toStringAsFixed(1).toString()),
            Expanded(
              child: Slider(
                  value: _brightness,
                  min: 0,
                  max: 1,
                  divisions: 10,
                  label: "Width factor",
                  onChanged: (v) {
                    setState(() => {_brightness = v});
                  },
                  onChangeEnd: (v) {
                    if (_file != null) {
                      prepareImage(_file as PlatformFile,
                              widthFactor: _widthFactor, brightness: v)
                          .then((image) {
                        if (image != null) {
                          setState(() => {
                                _image = image,
                                _imageRender = img.encodeJpg(image),
                                _brightness = v
                              });
                        }
                      });
                    }
                  }),
            )
          ],
        )),
        ListTile(
          title: streamingControl,
        )
      ],
    );
  }

  Future<void> waitAck() async {
    var response = await widget.input.first;
    if (response[0] == 4) {
      return;
    } else {
      throw Exception("Unexpected answer from ESP");
    }
  }

  void streamImage() async {
    widget.connection.output
        .add(Uint8List.fromList([3, _speed.toInt()])); // MSG_INIT
    await waitAck();
    ft.debugPrint("ESP is ready");

    setState(() {
      _streaming = 0;
    });

    await Future.delayed(Duration(milliseconds: (_delay * 1000).toInt()));

    var pixelMessage = Uint8List(2 + widget.pixels * 3);
    pixelMessage[0] = 2; // TODO: protocol

    for (var x = 0; x < _image!.width; x++) {
      if (_streaming == null) {
        // Cancelled
        ft.debugPrint("Cancelled");
        break;
      }

      ft.debugPrint("x: " + x.toString());
      for (int y = 0; y < widget.pixels; y++) {
        final px = _image!.getPixel(x, y);
        pixelMessage[2 + y * 3] = gamma[px.r as int];
        pixelMessage[2 + y * 3 + 1] = gamma[px.g as int];
        pixelMessage[2 + y * 3 + 2] = gamma[px.b as int];
      }

      widget.connection.output.add(pixelMessage);
      await waitAck();

      setState(() {
        if (_streaming != null) {
          _streaming = x;
        }
      });
    }

    widget.connection.output.add(Uint8List.fromList([5])); // MSG_DONE
    await waitAck();

    ft.debugPrint("done. ");
    setState(() {
      _streaming = null;
    });
  }
}

Uint8List gamma = Uint8List.fromList([
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  1,
  2,
  2,
  2,
  2,
  2,
  2,
  2,
  2,
  3,
  3,
  3,
  3,
  3,
  3,
  3,
  4,
  4,
  4,
  4,
  4,
  5,
  5,
  5,
  5,
  6,
  6,
  6,
  6,
  7,
  7,
  7,
  7,
  8,
  8,
  8,
  9,
  9,
  9,
  10,
  10,
  10,
  11,
  11,
  11,
  12,
  12,
  13,
  13,
  13,
  14,
  14,
  15,
  15,
  16,
  16,
  17,
  17,
  18,
  18,
  19,
  19,
  20,
  20,
  21,
  21,
  22,
  22,
  23,
  24,
  24,
  25,
  25,
  26,
  27,
  27,
  28,
  29,
  29,
  30,
  31,
  32,
  32,
  33,
  34,
  35,
  35,
  36,
  37,
  38,
  39,
  39,
  40,
  41,
  42,
  43,
  44,
  45,
  46,
  47,
  48,
  49,
  50,
  50,
  51,
  52,
  54,
  55,
  56,
  57,
  58,
  59,
  60,
  61,
  62,
  63,
  64,
  66,
  67,
  68,
  69,
  70,
  72,
  73,
  74,
  75,
  77,
  78,
  79,
  81,
  82,
  83,
  85,
  86,
  87,
  89,
  90,
  92,
  93,
  95,
  96,
  98,
  99,
  101,
  102,
  104,
  105,
  107,
  109,
  110,
  112,
  114,
  115,
  117,
  119,
  120,
  122,
  124,
  126,
  127,
  129,
  131,
  133,
  135,
  137,
  138,
  140,
  142,
  144,
  146,
  148,
  150,
  152,
  154,
  156,
  158,
  160,
  162,
  164,
  167,
  169,
  171,
  173,
  175,
  177,
  180,
  182,
  184,
  186,
  189,
  191,
  193,
  196,
  198,
  200,
  203,
  205,
  208,
  210,
  213,
  215,
  218,
  220,
  223,
  225,
  228,
  231,
  233,
  236,
  239,
  241,
  244,
  247,
  249,
  252,
  255
]);
