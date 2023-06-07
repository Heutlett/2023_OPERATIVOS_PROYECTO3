import 'package:flutter/material.dart';

class HomePage extends StatelessWidget {
  const HomePage({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Keyboard app'),
        automaticallyImplyLeading: false,
      ),
      body: Center(
        child: Row(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const SizedBox(height: 40.0),
            SizedBox(
              width: 250,
              height: 250,
              child: ElevatedButton(
                onPressed: () => _showChangePinPage(context),
                child: Text(
                  'Change pin',
                  style: TextStyle(fontSize: 25),
                ),
              ),
            ),
            const SizedBox(width: 50.0),
            SizedBox(
              width: 250,
              height: 250,
              child: ElevatedButton(
                onPressed: () => _showKeyboardPage(context),
                child: Text(
                  'Open Keyboard',
                  style: TextStyle(fontSize: 25),
                ),
              ),
            ),
            const SizedBox(height: 20.0),
          ],
        ),
      ),
    );
  }

  void _showChangePinPage(BuildContext context) {
    Navigator.of(context).pushNamed("/change_pin");
  }

  void _showKeyboardPage(BuildContext context) {
    Navigator.of(context).pushNamed("/keyboard");
  }
}
