import 'package:flutter/material.dart';
import 'package:keyboard_app/home.dart';
import 'package:keyboard_app/change_pin.dart';
import 'package:keyboard_app/keyboard.dart';

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Material App',
      initialRoute: "/",
      routes: {
        "/": (BuildContext context) => const HomePage(),
        "/change_pin": (BuildContext context) => const ChangePinScreen(),
        "/keyboard": (BuildContext context) => const VerifyPinScreen(),
      },
    );
  }
}
