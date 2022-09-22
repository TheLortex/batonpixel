import 'dart:async';
import 'dart:convert';
import 'dart:isolate';
import 'dart:typed_data';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:flutter_bluetooth_serial/flutter_bluetooth_serial.dart';
import 'package:image/image.dart' as img;

import 'dart:developer' as developer;
import 'package:flutter/foundation.dart' as ft;

// import './helpers/LineChart.dart';

class MainPage extends StatefulWidget {
  @override
  _MainPage createState() => new _MainPage();
}

const deviceMac = "30:AE:A4:39:11:EA";

class DecodeParam {
  final Uint8List fileBytes;
  final SendPort sendPort;
  DecodeParam(this.fileBytes, this.sendPort);
}

img.Image? prepareImageSync(Uint8List fileBytes) {
  ft.debugPrint("bytes" +
      fileBytes.length.toString() +
      ": " +
      fileBytes.sublist(0, 1000).toString());

  var image = img.decodeImage(fileBytes);

  if (image == null) {
    return null;
  }

  if (image.height > image.width) {
    image = img.copyRotate(image, 90);
  }

  return img.copyResize(image, height: 144);
}

Future<img.Image?> prepareImage(PlatformFile file) async {
  var receivePort = ReceivePort();

  await Isolate.spawn((DecodeParam p) {
    ft.debugPrint("Preparing image");
    final image = prepareImageSync(p.fileBytes);
    ft.debugPrint("Image ready");
    p.sendPort.send(image);
  }, DecodeParam(file.bytes!, receivePort.sendPort));

  return (await receivePort.first) as img.Image?;
}

class _MainPage extends State<MainPage> {
  BluetoothState _bluetoothState = BluetoothState.UNKNOWN;
  BluetoothDevice? _bluetoothDevice;
  BluetoothConnection? _connection;
  img.Image? _image;
  Uint8List? _imageRender;

  @override
  void initState() {
    super.initState();

    // Get current state
    FlutterBluetoothSerial.instance.state.then((state) {
      setState(() {
        _bluetoothState = state;
      });
    });

    Future.doWhile(() async {
      // Wait if adapter not enabled
      if ((await FlutterBluetoothSerial.instance.isEnabled) ?? false) {
        return false;
      }
      await Future.delayed(Duration(milliseconds: 0xDD));
      return true;
    });

    // Listen for futher state changes
    FlutterBluetoothSerial.instance
        .onStateChanged()
        .listen((BluetoothState state) {
      setState(() {
        _bluetoothState = state;
      });
    });

    // Check if Pixelstick is connected.
    Stream.periodic(const Duration(seconds: 1))
        .asyncMap((_) => FlutterBluetoothSerial.instance
                .getBondedDevices()
                .then((bondedDevices) {
              ft.debugPrint(
                  "Listing bonded devices. " + bondedDevices.length.toString());
              try {
                return bondedDevices.firstWhere((element) {
                  return element.address == deviceMac;
                });
              } on StateError {
                return null;
              }
            }))
        .listen((device) {
      setState(() {
        _bluetoothDevice = device;
      });
    });
  }

  @override
  void dispose() {
    FlutterBluetoothSerial.instance.setPairingRequestHandler(null);
    super.dispose();
  }

  Widget noBluetoothPage() {
    return ListView(
      children: [
        ListTile(title: const Text('Bluetooth is not enabled')),
        ListTile(
            title: ElevatedButton(
          child: const Text('Enable'),
          onPressed: () {
            FlutterBluetoothSerial.instance
                .requestEnable()
                .then((_) => setState(() {}));
          },
        ))
      ],
    );
  }

  Widget notConnectedPage() {
    return ListView(
      children: [
        ListTile(title: const Text('Device is not associated')),
      ],
    );
  }

  Widget connectedPage(BluetoothConnection c) {
    Widget image = const Text('waiting for image');

    if (_imageRender != null) {
      image = SingleChildScrollView(
        scrollDirection: Axis.horizontal,
        child: SizedBox(
          height: 144 * 2,
          width: _image!.width * 2,
          child: Image.memory(_imageRender!, fit: BoxFit.cover),
        ),
      );
    }

    return ListView(children: [
      ListTile(title: const Text('Connected to pixel stick')),
      Divider(),
      ListTile(title: const Text('Select image')),
      ListTile(
          title: ElevatedButton(
        child: const Text('Select'),
        onPressed: () {
          FilePicker.platform
              .pickFiles(type: FileType.image, withData: true)
              .then((file) {
            if (file != null) {
              prepareImage(file.files.first).then((image) {
                if (image != null) {
                  setState(() => {
                        _image = image,
                        _imageRender = img.encodeJpg(image) as Uint8List
                      });
                }
              });
            }
          });
        },
      )),
      image,
      ListTile(
          title: ElevatedButton(
        onPressed: () {
          c.output.add(ascii.encode('Hello!'));
        },
        child: const Text('Start'),
      )),
    ]);
  }

  Widget connectPage(BluetoothDevice target) {
    return ListView(children: [
      ListTile(title: const Text('Pixel stick is associated')),
      Divider(),
      ListTile(
          title: ElevatedButton(
        onPressed: () {
          BluetoothConnection.toAddress(target.address).then((connection) {
            setState(() => {_connection = connection});
          });
        },
        child: const Text('Connect'),
      )),
    ]);
  }

  @override
  Widget build(BuildContext context) {
    Widget content = noBluetoothPage();

    if (_bluetoothState == BluetoothState.STATE_ON) {
      if (_bluetoothDevice == null) {
        content = notConnectedPage();
      } else if (!_bluetoothDevice!.isConnected) {
        content = connectPage(_bluetoothDevice!);
      } else if (_connection != null) {
        content = connectedPage(_connection!);
      }
    }

    return Scaffold(
      appBar: AppBar(
        title: const Text('Pixel stick'),
      ),
      body: Container(child: content),
    );
  }
}
