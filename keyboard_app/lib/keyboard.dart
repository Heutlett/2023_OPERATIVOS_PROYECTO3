import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'dart:convert';
import 'dart:io';

class VerifyPinScreen extends StatefulWidget {
  const VerifyPinScreen({Key? key}) : super(key: key);

  @override
  State<VerifyPinScreen> createState() => _VerifyPinScreenState();
}

class _VerifyPinScreenState extends State<VerifyPinScreen> {
  TextEditingController _pinController = TextEditingController();
  String _enteredPin = '';
  String _pin = '';
  String _pinValidationMessage = '';
  double _buttonSize = 100.0;
  final double _minButtonSize = 70.0;
  final double _maxButtonSize = 100.0;

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

  void _updateEnteredPin(String number) {
    setState(() {
      _enteredPin += number;
    });
  }

  void _clearEnteredPin() {
    setState(() {
      _enteredPin = '';
    });
  }

  void _verifyPin() {
    setState(() {
      if (_enteredPin == _pin) {
        _pinValidationMessage = 'PIN correcto';
      } else {
        _pinValidationMessage = 'PIN incorrecto';
      }
    });
  }

  void _increaseButtonSize() {
    setState(() {
      if (_buttonSize < _maxButtonSize) {
        _buttonSize += 15.0;
      }
    });
  }

  void _decreaseButtonSize() {
    setState(() {
      if (_buttonSize > _minButtonSize) {
        _buttonSize -= 15.0;
      }
    });
  }

  @override
  void dispose() {
    _pinController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('Verificar PIN'),
      ),
      body: Center(
        child: Padding(
          padding: EdgeInsets.all(16.0),
          child: Column(
            children: [
              SizedBox(
                height: 20,
              ),
              ElevatedButton(
                onPressed: _sendToServer,
                style: ElevatedButton.styleFrom(
                    padding: EdgeInsets.all(16),
                    backgroundColor: Colors.yellow,
                    foregroundColor: Colors.black // Fondo rojo
                    ),
                child: Text('Iniciar comunicación',
                    style: TextStyle(fontSize: 20)),
              ),
              SizedBox(
                height: 20,
              ),
              Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: <Widget>[
                  ElevatedButton(
                    onPressed: _decreaseButtonSize,
                    style: ElevatedButton.styleFrom(
                      shape: CircleBorder(),
                      padding: EdgeInsets.all(8),
                      backgroundColor: Colors.red, // Fondo rojo
                    ),
                    child: Icon(Icons.remove),
                  ),
                  ElevatedButton(
                    onPressed: _increaseButtonSize,
                    style: ElevatedButton.styleFrom(
                      shape: CircleBorder(),
                      padding: EdgeInsets.all(8),
                      backgroundColor: Colors.green, // Fondo verde
                    ),
                    child: Icon(Icons.add),
                  ),
                  Text(
                    'Tamaño de teclado: ${_buttonSize.toInt()}',
                    style: TextStyle(fontSize: 20.0),
                  ),
                ],
              ),
              Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: <Widget>[
                  Text(
                    'PIN: $_enteredPin',
                    style: TextStyle(fontSize: 24.0),
                  ),
                  SizedBox(height: 16.0),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: <Widget>[
                      _buildNumberButton('1'),
                      _buildNumberButton('2'),
                      _buildNumberButton('3'),
                    ],
                  ),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: <Widget>[
                      _buildNumberButton('4'),
                      _buildNumberButton('5'),
                      _buildNumberButton('6'),
                    ],
                  ),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: <Widget>[
                      _buildNumberButton('7'),
                      _buildNumberButton('8'),
                      _buildNumberButton('9'),
                    ],
                  ),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: <Widget>[
                      _buildActionButton(
                          Icons.backspace, _clearEnteredPin, Colors.red),
                      _buildNumberButton('0'),
                      _buildActionButton(
                          Icons.check_box, _verifyPin, Colors.green),
                    ],
                  ),
                  SizedBox(height: 16.0),
                  Text(
                    _pinValidationMessage,
                    style: TextStyle(
                      fontSize: 24.0,
                      fontWeight: FontWeight.bold,
                      color: _pinValidationMessage == 'PIN incorrecto'
                          ? Colors.red
                          : Colors.green,
                    ),
                  ),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildNumberButton(String number) {
    return Padding(
      padding: const EdgeInsets.all(8.0),
      child: ElevatedButton(
        onPressed: () => _updateEnteredPin(number),
        style: ElevatedButton.styleFrom(
          padding: EdgeInsets.all(0),
        ),
        child: Container(
          width: _buttonSize,
          height: _buttonSize,
          child: Center(
            child: Text(
              number,
              style: TextStyle(fontSize: _buttonSize * 0.35),
            ),
          ),
        ),
      ),
    );
  }

  Widget _buildActionButton(
      IconData icon, VoidCallback onPressed, Color color) {
    return Padding(
      padding: const EdgeInsets.all(8.0),
      child: SizedBox(
        width: _buttonSize,
        height: _buttonSize,
        child: ElevatedButton(
          onPressed: onPressed,
          style: ButtonStyle(backgroundColor: MaterialStateProperty.all(color)),
          child: Icon(icon),
        ),
      ),
    );
  }

  void _sendToServer() async {
    final String serverIP = '192.168.18.90';
    final int serverPort =
        8888; // Reemplaza con el puerto correcto de tu servidor

    RawDatagramSocket? socket;
    InternetAddress serverAddress;

    try {
      serverAddress = await InternetAddress(serverIP);
      socket = await RawDatagramSocket.bind(InternetAddress.anyIPv4, 0);

      // Enviar mensaje al servidor
      String message = 'hola servidor';
      socket.send(message.codeUnits, serverAddress, serverPort);

      print('Mensaje enviado al servidor');

      socket.listen((RawSocketEvent event) {
        if (event == RawSocketEvent.read) {
          Datagram? datagram = socket?.receive();
          if (datagram != null) {
            String response = utf8.decode(datagram.data);
            print('Respuesta del servidor: $response');
          }
        }
      });
    } catch (e) {
      print('Error: $e');
    }
  }
}
