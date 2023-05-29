import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';

class ChangePinScreen extends StatefulWidget {
  const ChangePinScreen({super.key});

  @override
  State<ChangePinScreen> createState() => _ChangePinScreenState();
}

class _ChangePinScreenState extends State<ChangePinScreen> {
  TextEditingController _pinController = TextEditingController();
  String _pin = '';

  Future<void> _changePin() async {
    SharedPreferences prefs = await SharedPreferences.getInstance();
    setState(() {
      _pin = _pinController.text;
    });
    // Guardar el nuevo PIN en SharedPreferences
    await prefs.setString('pin', _pin);
    print('Nuevo PIN guardado: $_pin');
  }

  @override
  void dispose() {
    _pinController.dispose();
    super.dispose();
  }

  @override
  void initState() {
    super.initState();
    _loadPin();
  }

  Future<void> _loadPin() async {
    SharedPreferences prefs = await SharedPreferences.getInstance();
    String savedPin = prefs.getString('pin') ?? '';
    setState(() {
      _pin = savedPin;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('Cambiar PIN'),
      ),
      body: Center(
        child: Padding(
          padding: EdgeInsets.all(16.0),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: <Widget>[
              TextField(
                controller: _pinController,
                decoration: InputDecoration(
                  labelText: 'Ingrese el nuevo pin',
                ),
                keyboardType: TextInputType.number,
              ),
              SizedBox(height: 16.0),
              ElevatedButton(
                onPressed: _changePin,
                child: Text('Guardar PIN'),
              ),
              SizedBox(height: 16.0),
              Text('PIN actual: $_pin'),
            ],
          ),
        ),
      ),
    );
  }
}
