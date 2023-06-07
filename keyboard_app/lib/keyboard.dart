import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';

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
  double _buttonSize = 130.0;
  bool toggleValue = false;
  Color activeColor = Colors.green;
  List<bool> isSelected = [true, false];

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
      if (_enteredPin.length <= 10) {
        _enteredPin += number;
      }
    });
  }

  void _clearEnteredPin() {
    setState(() {
      _enteredPin = '';
    });
  }

  void _verifyPin() {
    setState(() {
      if (isSelected[1]) {
        _changePin();
      } else {
        if (_enteredPin == _pin) {
          _pinValidationMessage = 'Correct PIN';
        } else {
          _pinValidationMessage = 'Incorrect PIN';
        }
      }
    });
  }

  void _set_size_big() {
    setState(() {
      _buttonSize = 130;
    });
  }

  void _set_size_med() {
    setState(() {
      _buttonSize = 115;
    });
  }

  void _set_size_small() {
    setState(() {
      _buttonSize = 100;
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
        title: Text('Keyboard app'),
      ),
      body: Center(
        child: Padding(
          padding: EdgeInsets.all(16.0),
          child: Row(
            children: [
              Column(
                mainAxisAlignment: MainAxisAlignment.start,
                children: [
                  Container(
                    color: Colors.blueGrey[100],
                    width: 1140,
                    child: Row(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        Container(
                          padding: EdgeInsets.all(28),
                          width: 420,
                          child: Row(
                            children: [
                              Text(
                                'Actual PIN:',
                                style: TextStyle(
                                  fontSize: 25.0,
                                  fontWeight: FontWeight.bold,
                                ),
                              ),
                              Text(
                                ' $_pin',
                                style: TextStyle(
                                  fontSize: 25.0,
                                  fontFamily: 'Consolas',
                                ),
                              ),
                            ],
                          ),
                        ),
                        SizedBox(
                          width: 10,
                        ),
                        Container(
                          padding: EdgeInsets.all(28),
                          width: 420,
                          child: Row(
                            children: [
                              Text(
                                'Entered PIN:',
                                style: TextStyle(
                                  fontSize: 25.0,
                                  fontFamily: 'Consolas',
                                  fontWeight: FontWeight.bold,
                                ),
                              ),
                              Text(
                                ' $_enteredPin',
                                style: TextStyle(
                                  fontSize: 25.0,
                                  fontFamily: 'Consolas',
                                ),
                              ),
                            ],
                          ),
                        ),
                        SizedBox(
                          width: 10,
                        ),
                        Container(
                          padding: EdgeInsets.all(28),
                          width: 270,
                          height: 80,
                          child: Row(
                            children: [
                              Text(
                                _pinValidationMessage,
                                style: TextStyle(
                                  fontSize: 24.0,
                                  fontWeight: FontWeight.bold,
                                  color:
                                      _pinValidationMessage == 'Incorrect PIN'
                                          ? Colors.red
                                          : Colors.green,
                                ),
                              ),
                              SizedBox(
                                width: 20,
                              ),
                              _pinValidationMessage == 'Incorrect PIN'
                                  ? Container(
                                      child: Icon(
                                        Icons.sentiment_dissatisfied_rounded,
                                        size: 30,
                                        color: Colors.red,
                                      ),
                                    )
                                  : Container(
                                      child: Icon(
                                          Icons.sentiment_very_satisfied_sharp,
                                          size: 30,
                                          color: Colors.green),
                                    ),
                            ],
                          ),
                        ),
                      ],
                    ),
                  ),
                  SizedBox(
                    height: 20,
                  ),

                  // aqui va el selector
                  Container(
                      color: Colors.blueGrey[100],
                      width: 1140,
                      height: 600,
                      child: Row(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          Container(
                            child: Column(
                              mainAxisAlignment: MainAxisAlignment.end,
                              children: [
                                ToggleButtons(
                                  children: <Widget>[
                                    Container(
                                      width: 160,
                                      padding: EdgeInsets.all(10),
                                      child: Center(
                                        child: Text(
                                          'Verify PIN',
                                          style: TextStyle(
                                            fontSize: 25,
                                          ),
                                        ),
                                      ),
                                    ),
                                    Container(
                                      width: 160,
                                      padding: EdgeInsets.all(10),
                                      child: Center(
                                        child: Text('Change PIN',
                                            style: TextStyle(fontSize: 25)),
                                      ),
                                    ),
                                  ],
                                  onPressed: (int index) {
                                    setState(() {
                                      for (int buttonIndex = 0;
                                          buttonIndex < isSelected.length;
                                          buttonIndex++) {
                                        if (buttonIndex == index) {
                                          isSelected[buttonIndex] = true;
                                        } else {
                                          isSelected[buttonIndex] = false;
                                        }
                                      }
                                    });
                                  },
                                  isSelected: isSelected,
                                  selectedColor: Colors.white,
                                  borderColor: Colors.blue,
                                  selectedBorderColor: Colors.blue,
                                  borderRadius:
                                      BorderRadius.all(Radius.circular(15)),
                                  color: Colors.blue,
                                  disabledColor: Colors.amber,
                                  fillColor: Colors.blue,
                                ),
                                SizedBox(
                                  height: 20,
                                )
                              ],
                            ),
                          ),
                          Container(
                            color: Colors.white,
                            width: 450,
                            margin: EdgeInsets.all(8),
                            child: Column(
                              mainAxisAlignment: MainAxisAlignment.center,
                              children: <Widget>[
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
                                    _buildActionButton(Icons.backspace,
                                        _clearEnteredPin, Colors.red),
                                    _buildNumberButton('0'),
                                    _buildActionButton(Icons.check_box,
                                        _verifyPin, Colors.green),
                                  ],
                                ),
                              ],
                            ),
                          ),
                          Column(
                            mainAxisAlignment: MainAxisAlignment.end,
                            children: [
                              Container(
                                padding: EdgeInsets.all(8),
                                decoration: BoxDecoration(
                                  color: Colors.blueGrey[100],
                                  borderRadius: BorderRadius.circular(15),
                                  border: Border.all(
                                    color: Colors.blue,
                                    width: 1,
                                  ),
                                ),
                                width: 310,
                                child: Row(
                                  mainAxisAlignment: MainAxisAlignment.center,
                                  children: <Widget>[
                                    Text(
                                      'Size:',
                                      style: TextStyle(fontSize: 25),
                                    ),
                                    SizedBox(width: 20),
                                    ElevatedButton(
                                      onPressed: _set_size_big,
                                      style: ElevatedButton.styleFrom(
                                        shape: CircleBorder(),
                                        padding: EdgeInsets.all(20),
                                        backgroundColor: _buttonSize == 130
                                            ? Colors.red
                                            : Colors.blue, // Fondo rojo
                                      ),
                                      child: Text(
                                        'B',
                                        style: TextStyle(fontSize: 25),
                                      ),
                                    ),
                                    SizedBox(width: 10),
                                    ElevatedButton(
                                      onPressed: _set_size_med,
                                      style: ElevatedButton.styleFrom(
                                        shape: CircleBorder(),
                                        padding: EdgeInsets.all(20),
                                        backgroundColor: _buttonSize == 115
                                            ? Colors.red
                                            : Colors.blue, // Fondo rojo
                                        // Fondo rojo
                                      ),
                                      child: Text(
                                        'M',
                                        style: TextStyle(fontSize: 25),
                                      ),
                                    ),
                                    SizedBox(width: 10),
                                    ElevatedButton(
                                      onPressed: _set_size_small,
                                      style: ElevatedButton.styleFrom(
                                        shape: CircleBorder(),
                                        padding: EdgeInsets.all(20),
                                        backgroundColor: _buttonSize == 100
                                            ? Colors.red
                                            : Colors.blue, // Fondo rojo
                                      ),
                                      child: Text(
                                        'S',
                                        style: TextStyle(fontSize: 25),
                                      ),
                                    ),
                                  ],
                                ),
                              ),
                              SizedBox(
                                height: 20,
                              )
                            ],
                          ),
                        ],
                      ))
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }

  Future<void> _changePin() async {
    SharedPreferences prefs = await SharedPreferences.getInstance();
    setState(() {
      _pin = _enteredPin;
    });
    // Guardar el nuevo PIN en SharedPreferences
    await prefs.setString('pin', _pin);
    print('Nuevo PIN guardado: $_pin');
    _loadPin();
    _enteredPin = "";
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
}
