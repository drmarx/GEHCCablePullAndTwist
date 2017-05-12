#include <WheatstoneBridge.h> // load cell amplifier library
#include <dht11.h> // temp/humid sensor
#include <avr/wdt.h> // watchdog
#define DHT11_PIN 8
const int linPot = A2; // pin for linear acutator position
const int contSensor = A0; // pin for continuity resistance test BLUE
const int loadPin = A1; // pin for (amplified) load cell PINK
const int spinPinPwm = 6; // pin for spin motor shield pwm
const int spinPinDir = 7; // pin for spin motor shield direction
const int linPinPwm = 5; // pin for linear actuator motor shield pwm
const int linPinDir = 4; // pin for linear acutator motor shield direction
const int spinPosA = 2; // pin for spin motor rotory encoder channel A YELLOW
const int spinPosB = 3; // pin for spin motor rotory encoder channel B BROWN
const int runLED = 13;
const int restLED = 12;

volatile byte aFlag = 0; // variable for storing previous A value
volatile byte bFlag = 0; // variable for storing previous B value
volatile long spinPos = 0; // variable for storing spin motor position
						   //volatile byte oldSpinPos = 0; // variable for previous spin motor position
volatile byte readEncoder; // variable for reading input data from encoder

						   /*
						   Variables used with the WheatstoneBridge amplifier for the load cell
						   Specifically for calibration
						   */
const int CST_STRAIN_IN_MIN = 350;       // Raw value calibration lower point
const int CST_STRAIN_IN_MAX = 650;       // Raw value calibration upper point
const int CST_STRAIN_OUT_MIN = 0;        // Weight calibration lower point
const int CST_STRAIN_OUT_MAX = 50000;    // Weight calibration upper point
										 // Initialize the Wheatstone bridge object
WheatstoneBridge wsb(A1, CST_STRAIN_IN_MIN, CST_STRAIN_IN_MAX, CST_STRAIN_OUT_MIN, CST_STRAIN_OUT_MAX);

static dht11 DHT; // temp/humid object
static int spinSpeed = 200;
static int linSpeed = 100;
static int offset;
static float load = 0;
static float inLoad = 0;
static float linPos;
static float cont;
static bool runningTest = false;

struct params {
	int mins;
	int rest;
	int reps;
	float force;
	long deg; // 8758 count = 360 deg => 24.33 counts = 1 deg
	bool cont;
	float freq;
};
typedef struct params Params;
static Params p;  // test parameters


void setup() {
	// Initialize
	wdt_enable(WDTO_2S); // initialize watchdog at 2s limit
	runningTest = false;
	pinMode(spinPosA, INPUT_PULLUP);
	pinMode(spinPosB, INPUT_PULLUP);
	pinMode(spinPinDir, OUTPUT);
	pinMode(spinPinPwm, OUTPUT);
	pinMode(linPinDir, OUTPUT);
	pinMode(linPinPwm, OUTPUT);
	pinMode(runLED, OUTPUT);
	pinMode(restLED, OUTPUT);
	attachInterrupt(0, encoder, RISING);
	Serial.begin(9600);
	Serial.setTimeout(100);
	linear(0);
	calibrate();
}

void loop() {
	if (runningTest) {
		runTest();
	}
	String in;
	if (Serial.available()) {
		in = Serial.readString();
		command(in);
	}
	getLinPos();
	getLoad();
	getCont();
	linear(inLoad);
	wdt_reset(); // tell watchdog everything is okay
}

void serialEvent() {

}
void command(String cmd) {
	if (cmd.startsWith("TEST") && !(runningTest)) {
		String args[6];
		for (int i = 0; i < 6; i++) {
			args[i] = parse(cmd, ' ', i + 1);
		}
		p.mins = args[0].toInt();
		p.rest = args[1].toInt();
		p.reps = args[2].toInt();
		p.force = args[3].toFloat();
		p.deg = args[4].toInt() * 24.33; // converts degrees to spin counter units
		p.cont = args[5].toInt();
	}
	else if (cmd.startsWith("RATE")) {
		p.freq = parse(cmd, ' ', 1).toFloat();
	}
	else if (cmd.equals("START") && !(runningTest)) {
		spin(p.deg);
		inLoad = p.force;
		runningTest = true;
	}
	else if (cmd.equals("DATA")) {
		Serial.print(load);
		Serial.print(" ");
		Serial.print(spinPos / 24);
		Serial.print(" ");
		Serial.println(cont);
	}
	else if (cmd.equals("TEMP")) {
		DHT.read(DHT11_PIN);
		Serial.print(DHT.temperature);
		Serial.print("C / ");
		Serial.print(DHT.humidity);
		Serial.println("%");
	}
	else if (cmd.startsWith("SPIN")) {
		long arg = parse(cmd, ' ', 1).toInt() * 24.33;
		runningTest = true;
		spin(arg);
	}
	else if (cmd.equals("REST")) {
		digitalWrite(runLED, LOW);
		digitalWrite(restLED, HIGH);
		inLoad = 0;
		linear(inLoad);
		runningTest = false;
	}
	else if (cmd.equals("STOP")) {
		digitalWrite(runLED, LOW);
		digitalWrite(restLED, HIGH);
		inLoad = 0;
		linear(inLoad);
		spin(0);
		runningTest = false;
	}
	else if (cmd.startsWith("PULL")) {
		inLoad = parse(cmd, ' ', 1).toFloat();
	}
	else if (cmd.equals("STATUS")) {
		Serial.println(runningTest);
	}

	else if (cmd.equals("CAL")) {
		calibrate();
	}
}

void runTest() {
	getLoad();
	getCont();
	getLinPos();
	spin(p.deg);
	linear(inLoad);
}

bool emergencyStop() {
	//String in;
	//if (Serial.available()) in = Serial.readString();
	//if (in.equalsIgnoreCase("STOP") || (p.cont && cont == 0)) {
	//  runningTest = false;
	//  return true;
	//}
	return false;
}

void getCont() { // measures resistance in ohms, 0 if disconnected
	int in = analogRead(contSensor);
	if (in) {
		float voltage = (in / 1024.0) * 5.0;
		float temp = (((27 * voltage) / (5 - voltage)) / 27) - 0.08;
		cont = (temp >= 3.43) ? 0 : temp;
	}
	else cont = 0;
}

void spin(long d) {
	//Serial.println("Spin started");
	if (d > 0) {
		while (spinPos < d) {
			if (emergencyStop()) { // emergency stop
				Serial.println("ALERT: EMERGENCY STOP");
				analogWrite(spinPinPwm, 0);
				break;
			}
			analogWrite(spinPinPwm, spinSpeed);
			digitalWrite(spinPinDir, LOW);
			//Serial.print("Spin Position = "); // DEBUG messages
			//Serial.println(spinPos);
			delay(10);
			wdt_reset(); // allows spin to go longer
		}
		if (!(runningTest)) {
			runningTest = true;
			Serial.println("READY");
		}
	}
	else {
		while (spinPos > d) {
			if (emergencyStop()) { // emergency stop
				Serial.println("ALERT: EMERGENCY STOP");
				analogWrite(spinPinPwm, 0);
				break;
			}
			analogWrite(spinPinPwm, spinSpeed);
			digitalWrite(spinPinDir, HIGH);
			//Serial.print("Spin Position = "); // DEBUG messages
			//Serial.println(spinPos);
			delay(10);
			wdt_reset(); // allows spin to go longer
		}
		analogWrite(spinPinPwm, 0);
	}
	runningTest = false;
	analogWrite(spinPinPwm, 0);
	//Serial.println("Spin stopped");
}

void linear(float f) {
	if (f > 0) {
		getLoad();
		if (load < f /*&& linPos < 3.0*/) {
			if (emergencyStop()) { // emergency stop
				Serial.println("ALERT: EMERGENCY STOP");
				analogWrite(linPinPwm, 0);
				//break;
			}
			analogWrite(linPinPwm, linSpeed);
			digitalWrite(linPinDir, HIGH);
			delay(10);
		}
		//else if (load > f){
		//  analogWrite(linPinPwm, linSpeed);
		//  digitalWrite(linPinDir, LOW);
		//  delay(10);
		//}
		else analogWrite(linPinPwm, 0);
	}
	else {
		if (linPos >= 0.6) {
			if (emergencyStop()) { // emergency stop
				Serial.println("ALERT: EMERGENCY STOP");
				analogWrite(linPinPwm, 0);
				//break;
			}
			getLinPos();
			analogWrite(linPinPwm, linSpeed);
			digitalWrite(linPinDir, LOW);
			delay(10);
		}
		else analogWrite(linPinPwm, 0);
	}
}

void getLoad() {
	int l = -wsb.measureForce() + offset;
	//load = l;
	load = (l > 0) ? (float)l / 387.16 : 0; // converts to kgs 
}

void getLinPos() {
	int in = analogRead(linPot);
	linPos = (in / 1024.0) * 5.0;
}

void encoder() { // Channel A went High
	cli(); //stop interrupts happening before we read pin values
	bFlag = digitalRead(spinPosB);
	if (bFlag) {
		spinPos--; //increment the encoder's position count
	}
	else {
		spinPos++;
	}
	sei(); //restart interrupts
}

void calibrate() {
	int avg = 0;
	for (int i = 0; i < 10; i++) {
		avg += wsb.measureForce();
		delay(100);
	}
	avg /= 10;
	offset = avg;
	wdt_reset(); // allows spin to go longer
}


String parse(String data, char separator, int index) { // splits input string by char separator, pieces accessible by index 0..*
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
