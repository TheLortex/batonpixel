import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_bluetooth_serial/flutter_bluetooth_serial.dart';
import 'package:scoped_model/scoped_model.dart';

import './BackgroundCollectedPage.dart';
import './BackgroundCollectingTask.dart';

import 'dart:developer' as developer;
import 'package:flutter/foundation.dart' as ft;

// import './helpers/LineChart.dart';

class MainPage extends StatefulWidget {
  @override
  _MainPage createState() => new _MainPage();
}

class _MainPage extends State<MainPage> {
  BluetoothState _bluetoothState = BluetoothState.UNKNOWN;

  Timer? _discoverableTimeoutTimer;
  int _discoverableTimeoutSecondsLeft = 0;

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

        // Discoverable mode is disabled when Bluetooth gets disabled
        _discoverableTimeoutTimer = null;
        _discoverableTimeoutSecondsLeft = 0;
      });
    });

    // Check if Pixelstick is connected.
    FlutterBluetoothSerial.instance.getBondedDevices().then((bondedDevices) {
      ft.debugPrint(
          "Listing bonded devices. " + bondedDevices.length.toString());
      final res = bondedDevices;
      res.retainWhere((element) {
        ft.debugPrint("device: " +
            element.address +
            " /" +
            element.name.toString() +
            " :: " +
            element.isBonded.toString() +
            ":" +
            element.isConnected.toString());
        return element.isConnected && element.address.startsWith('AA');
      });
      ft.debugPrint("Res: " + res.length.toString());
    });
  }

  @override
  void dispose() {
    FlutterBluetoothSerial.instance.setPairingRequestHandler(null);
    _discoverableTimeoutTimer?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Pixel stick'),
      ),
      body: Container(
        child: ListView(
          children: <Widget>[
            Divider(),
            ListTile(title: const Text('Select image')),
            ListTile(
                title: ElevatedButton(
              onPressed: () => {},
              child: const Text('Start'),
            )),
            Divider(),
          ],
        ),
      ),
    );
  }
}
