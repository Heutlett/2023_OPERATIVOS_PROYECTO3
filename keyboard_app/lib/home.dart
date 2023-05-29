import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';

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
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const SizedBox(height: 40.0),
            SizedBox(
              width: 250,
              height: 50,
              child: ElevatedButton(
                onPressed: () => _showChangePinPage(context),
                child: Text('Cambiar pin'),
              ),
            ),
            const SizedBox(height: 20.0),
            SizedBox(
              width: 250,
              height: 50,
              child: ElevatedButton(
                onPressed: () => _showKeyboardPage(context),
                child: Text('Abrir teclado'),
              ),
            ),
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

  Future<void> _showLoginPage(BuildContext context) async {
    final prefs = await SharedPreferences.getInstance();
    String? userName = prefs.getString('lastUsername');

    if (context.mounted) {
      Navigator.of(context).pushNamed("/login", arguments: userName);
    }
  }
}
