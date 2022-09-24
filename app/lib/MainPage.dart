import 'dart:async';
import 'dart:isolate';
import 'dart:typed_data';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:flutter_bluetooth_serial/flutter_bluetooth_serial.dart';
import 'package:flutter_bluetooth_serial_example/ConnectedWidget.dart';
import 'package:image/image.dart' as img;

import 'dart:developer' as developer;
import 'package:flutter/foundation.dart' as ft;
import 'dart:convert';

// import './helpers/LineChart.dart';

class MainPage extends StatefulWidget {
  @override
  _MainPage createState() => new _MainPage();
}

const deviceMac = "30:AE:A4:39:11:EA";

class _MainPage extends State<MainPage> {
  BluetoothState _bluetoothState = BluetoothState.UNKNOWN;
  BluetoothDevice? _bluetoothDevice;
  BluetoothConnection? _connection;
  Stream? _input;
  int? _pixels;

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
    if (_pixels == null) {
      return ListView(children: [
        ListTile(title: const Text('Connected to pixel stick')),
        Divider(),
        ListTile(title: const Text('Handshaking')),
      ]);
    } else {
      return ListView(children: [
        ListTile(title: const Text('Connected to pixel stick')),
        Divider(),
        ConnectedWidget(connection: c, pixels: _pixels!, input: _input!)
      ]);
    }
  }

  Widget connectPage(BluetoothDevice target) {
    return ListView(children: [
      ListTile(title: const Text('Pixel stick is associated')),
      Divider(),
      ListTile(
          title: ElevatedButton(
        onPressed: () {
          BluetoothConnection.toAddress(target.address).then((connection) {
            connection.output.add(Uint8List.fromList([0]));
            Stream input = connection.input!.asBroadcastStream();
            setState(() => {_connection = connection, _input = input});

            _input!.first.then((response) {
              if (response[0] == 1) {
                int pixels = ByteData.sublistView(response, 1, 5)
                    .getUint32(0, Endian.little);
                ft.debugPrint("Got pixels: " + pixels.toString());
                setState(() => {_pixels = pixels});
              } else {
                setState(() => {_connection = null});
              }
            });
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
      } else if (!_bluetoothDevice!.isConnected || _connection == null) {
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
