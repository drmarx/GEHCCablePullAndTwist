#include "Main.h"
const int tempSensor = A0; // DEBUG sample sensor
const int contSensor = A1;
static float temp; // DEBUG sample sensor data
static float load;
static float spinpos;
static float cont;
static bool running;
struct params {
	int mins;
	int reps;
	float force;
	bool cont;
	float freq;
};
typedef struct params Params;
static Params p;

void setup() {
	// Initialize
	running = false;
	Serial.begin(9600);
	Serial.println("GEHC Cable Pull & Twist initialized. Awaiting commands...");
	Serial.println("Available commands: TEST <length of test (minutes)> <number of tests> <target force (kgs)>");
	Serial.println("<stop on continuity break [0,1]> <data frequency (seconds)>, START, STOP, TEMP");
}

void loop() {
	String in;
	in = Serial.readString();
	command(in);
}

void command(String cmd) {
	if (cmd.startsWith("TEST") && !(running)) {
		String args[5];
		for (int i = 0; i < 5; i++) {
			args[i] = getValue(cmd, ' ', i + 1);
		}
		p.mins = args[0].toInt();
		p.reps = args[1].toInt();
		p.force = args[2].toFloat();
		p.cont = args[3].toInt();
		p.freq = args[4].toFloat();
		Serial.println("Parameters received. Ready to start.");
	}
	else if (cmd.equalsIgnoreCase("START") && !(running)) {
		runTest();
	}
	else if (cmd.equalsIgnoreCase("TEMP")) {
		getTemp();
		Serial.println(temp);
	}
}

void runTest() {
	running = true;
	int freq = p.freq * 1000;
	int runTime = p.mins * 60;
	for (int i = 1; i <= p.reps; i++) { // repeats test suite
		for (float j = 0; j < runTime; j += p.freq) { // check sensors every quarter second
			if (emergencyStop()) { // emergency stop
				Serial.println("ALERT: EMERGENCY STOP");
				break;
			}
			getTemp();
			getCont();
			Serial.print("Test ");
			Serial.print(i);
			Serial.print("/");
			Serial.print(p.reps);
			Serial.print(" ");
			Serial.print(j);
			Serial.print("s: Temp = ");
			Serial.print(temp);
			Serial.print("F  Cont = ");
			if (cont > 0) {
				Serial.print(cont);
				Serial.println("ohm");
			}
			else Serial.println("DISCONNECT");
			delay(freq);
		}
		if (!(running)) break;
	}
	Serial.println("Test stopped.");
	running = false;
}

// TODO: add more methods for spin, pull

bool emergencyStop() {
	String in;
	if (Serial.available()) in = Serial.readString();
	if (in.equalsIgnoreCase("STOP") || (p.cont && cont == 0)) {
		running = false;
		return true;
	}
	return false;
}

void getTemp() {
	int in = analogRead(tempSensor);
	float voltage = (in / 1024.0) * 5.0; // convert analog to voltage
	temp = ((voltage - 0.5) * 100) * 1.8 + 32; // convert voltage to temp in C then to F
}

void getCont() {
	int raw = analogRead(contSensor);
	if (raw) {
		float buf = raw * 5;
		float v = buf / 1024;
		buf = (5 / v) - 1;
		cont = 998 * buf; // measured value of pull-up resistor for greatest accuracy
	}
	else cont = 0;
}

String getValue(String data, char separator, int index) { // splits input string by char separator, pieces accessible by index 0..*
	int found = 0;
	int strIndex[] = { 0, -1 };
	int maxIndex = data.length() - 1;
	for (int i = 0; i <= maxIndex && found <= index; i++) {
		if (data.charAt(i) == separator || i == maxIndex) {
			found++;
			strIndex[0] = strIndex[1] + 1;
			strIndex[1] = (i == maxIndex) ? i + 1 : i;
		}
	}
	return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
