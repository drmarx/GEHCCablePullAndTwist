// #include "Main.h"
const int tempSensor = A0; // DEBUG sample sensor
const int contSensor = A1;
static float temp; // DEBUG sample sensor data
static float load;
static float spinpos;
static float cont;
static bool running;
struct params {
	int mins;
	int rest;
	int reps;
	float force;
	int deg;
	bool cont;
	float freq;
};
typedef struct params Params;
static Params p;	// test parameters


void setup() {
	// Initialize
	running = false;
	Serial.begin(9600);
	Serial.setTimeout(100);
	Serial.println("GEHC Cable Pull & Twist initialized. Awaiting commands...");
	Serial.println("Available commands: TEST <length of test in minutes<int>> <rest time in seconds<int>> <test repetitions<int>> <force in kgs<float>>");
	Serial.println("<spin turn degrees<int>> <continuity break stop<int[0,1]>>, START, STOP, TEMP, DATA, RATE<poll rate in seconds<float>>");
}

void loop() {
	String in;
	in = Serial.readString();
	command(in);
}

void command(String cmd) {
	if (cmd.startsWith("TEST") && !(running)) {
		String args[6];
		for (int i = 0; i < 6; i++) {
			args[i] = getValue(cmd, ' ', i + 1);
		}
		p.mins = args[0].toInt();
		p.rest = args[1].toInt();
		p.reps = args[2].toInt();
		p.force = args[3].toFloat();
		p.deg = args[4].toInt();
		p.cont = args[5].toInt();
		Serial.println("Parameters received. Ready to start.");
	}
	else if (cmd.startsWith("RATE")) {
		p.freq = getValue(cmd, ' ', 1).toFloat();
	}
	else if (cmd.equals("START") && !(running)) {
		runTest();
	}
	else if (cmd.equals("DATA")) {
		Serial.print(load);
		Serial.print(" ");
		Serial.print(spinpos);
		Serial.print(" ");
		Serial.println(cont);
	}
	else if (cmd.equals("TEMP")) {
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
	temp = ((voltage - 0.5) * 100.0) * 1.8 + 32.0; // convert voltage to temp in C then to F
}

void getCont() { // measures resistance in ohms, 0 if disconnected
	int in = analogRead(contSensor);
	if (in) {
		/*
		float buf = in * 5;
		float v = buf / 1024;
		buf = (5 / v) - 1;
		cont = 1000 * buf;
		*/
		float voltage = (in / 1024.0) * 5.0;
		cont = ((5.0 / voltage) - 1.0) * 1000.0;
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
