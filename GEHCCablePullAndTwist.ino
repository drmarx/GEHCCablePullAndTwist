#include <WheatstoneBridge.h>
#include <dht11.h> // temp/humid sensor
#define DHT11_PIN 8
const int linPot = A2; // DEBUG sample sensor YELLOW
const int contSensor = A1; // pin for continuity resistance test BLUE
const int loadPin = A0; // pin for (amplified) load cell PINK
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
const int CST_CAL_FORCE_MIN = 0;
const int CST_CAL_FORCE_MAX = 32000;
const int CST_CAL_FORCE_STEP = 50;
const int CST_CAL_FORCE_STEP_LARGE = 500;
// Initialize the Wheatstone bridge object
WheatstoneBridge wsb(A1, CST_STRAIN_IN_MIN, CST_STRAIN_IN_MAX, CST_STRAIN_OUT_MIN, CST_STRAIN_OUT_MAX);

static dht11 DHT; // temp/humid object
static int spinSpeed;
static int linSpeed;
static float temp; // DEBUG sample sensor data
static float load;
//static float spinPos;
static float cont;
static bool runningTest;

struct params {
	int mins;
	int rest;
	int reps;
	float force;
	int deg; // 8652 count = 360 deg => 24 counts = 1 deg
	bool cont;
	float freq;
};
typedef struct params Params;
static Params p;	// test parameters


void setup() {
	// Initialize
	runningTest = false;
	spinSpeed = 255; // spin speed 8-255
	linSpeed = 255; // DEBUG: example speed
	pinMode(spinPosA, INPUT_PULLUP);
	pinMode(spinPosB, INPUT_PULLUP);
	pinMode(spinPinDir, OUTPUT);
	pinMode(spinPinPwm, OUTPUT);
	pinMode(linPinDir, OUTPUT);
	pinMode(linPinPwm, OUTPUT);
	pinMode(runLED, OUTPUT);
	pinMode(restLED, OUTPUT);
	attachInterrupt(0, encoderA, RISING);
	//attachInterrupt(1, encoder, CHANGE);
	Serial.begin(9600);
	Serial.setTimeout(100);
	//Serial.println("GEHC Cable Pull & Twist initialized. Awaiting commands...");
	//Serial.println("Available commands: TEST <length of test in minutes<int>> <rest time in seconds<int>> <test repetitions<int>> <force in kgs<float>>");
	//Serial.println("<spin turn degrees<int>> <continuity break stop<int[0,1]>>, START, STOP, TEMP, RATE <poll rate in seconds<float>>, SPIN <deg>");
}

void loop() {
	if (runningTest) {
		runTest();
	}
	String in;
	if (Serial.available()) in = Serial.readString();
	command(in);
	//Serial.print("Spin Position ");
	//Serial.println(spinPos);
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
		p.deg = args[4].toInt() * 24; // converts degrees to spin counter units
		p.cont = args[5].toInt();
		//Serial.println("Parameters received. Ready to start.");
	}
	else if (cmd.startsWith("RATE")) {
		p.freq = parse(cmd, ' ', 1).toFloat();
	}
	else if (cmd.equals("START") && !(runningTest)) {
		digitalWrite(runLED, HIGH);
		digitalWrite(restLED, LOW);
		//spin(p.deg);
		runningTest = true;
	}
	else if (cmd.equals("DATA")) {
		Serial.print(load);
		Serial.print(" ");
		Serial.print(spinPos);
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
		int arg = parse(cmd, ' ', 1).toInt();
		runningTest = true;
		spin(arg);
	}
	else if (cmd.equals("REST")) {
		digitalWrite(runLED, LOW);
		digitalWrite(restLED, HIGH);
		// spin to 0?
		//linear(0);
		//Serial.println("RESTING");
		runningTest = false;
	}
	else if (cmd.equals("STOP")) {
		digitalWrite(runLED, LOW);
		digitalWrite(restLED, HIGH);
		//linear(0);
		//spin(0);
		runningTest = false;
	}
	else if (cmd.equals("STATUS")) {
		Serial.println(runningTest);
	}
}

void runTest() {
	getLoad();
	getCont();
	//spin(p.deg);
	//linear(p.force);
	//spinMotor(p.deg);
	//runningTest = true;
	/*
	int freq = p.freq * 1000;
	int runTime = p.mins * 60;
	for (int i = 1; i <= p.reps; i++) { // repeats test suite
		for (float j = 0; j < runTime; j += p.freq) { // check sensors every quarter second
			if (emergencyStop()) { // emergency stop
				Serial.println("ALERT: EMERGENCY STOP");
				break;
			}
			getLoad();
			//linearActuator(p.force);
			getTemp();
			getCont();
			// access test data with variables
			// pulled force = load
			// temperature = DHT.temperature
			// humidity = DHT.humidity
			// spin position = spinPos
			// print data with Serial.print() with the last one being .println()
			Serial.print("Test ");
			Serial.print(i);
			Serial.print("/");
			Serial.print(p.reps);
		Serial.print(" ");
			Serial.print(j);
			Serial.print("s: Temp = ");
			Serial.print(temp);
			Serial.print("F Spin = ");
			Serial.print(spinPos);
			Serial.print("u Cont = ");
			if (cont > 0) {
				Serial.print(cont);
				Serial.println("ohm");
			}
			else Serial.println("DISCONNECT");
			delay(freq);
		}
		//spinMotor(0);
		//linearActuator(0);
		if (!(runningTest)) break;
		Serial.print("Resting for ");
		Serial.print(p.rest);
		Serial.println(" seconds");
		delay(p.rest * 1000);
	}
	Serial.println("Test stopped.");
	runningTest = false; */
}

bool emergencyStop() {
	String in;
	if (Serial.available()) in = Serial.readString();
	if (in.equalsIgnoreCase("STOP") || (p.cont && cont == 0)) {
		runningTest = false;
		return true;
	}
	return false;
}

void getCont() { // measures resistance in ohms, 0 if disconnected
	int in = analogRead(contSensor);
	if (in) {
		float voltage = (in / 1024.0) * 5.0;
		cont = ((5.0 / voltage) - 1.0) * 1000.0;
	}
	else cont = 0;
}

void spin(long d) {
	//Serial.println("Spin started");
	if (d > 0) {
		while (spinPos < d) {
			if (emergencyStop()) { // emergency stop
				Serial.println("ALERT: EMERGENCY STOP");
				break;
			}
			analogWrite(spinPinPwm, spinSpeed);
			digitalWrite(spinPinDir, LOW);
			Serial.print("Spin Position = "); // DEBUG messages
			Serial.println(spinPos);
			delay(10);
		}
		if (!(runningTest)) {
			runningTest = true;
			Serial.println("READY");
		}
	}
	else {
		while (spinPos > 0){
			if (emergencyStop()) { // emergency stop
				Serial.println("ALERT: EMERGENCY STOP");
				break;
			}
			analogWrite(spinPinPwm, spinSpeed);
			digitalWrite(spinPinDir, HIGH);
			Serial.print("Spin Position = "); // DEBUG messages
			Serial.println(spinPos);
			delay(10);
		}
	}
	runningTest = false;
	analogWrite(spinPinPwm, 0);
	Serial.println("Spin stopped");
}

void linear(int f) {
	if (f > 0) {
		while (load < f) {
			if (emergencyStop()) { // emergency stop
				Serial.println("ALERT: EMERGENCY STOP");
				break;
			}
			analogWrite(linPinPwm, linSpeed);
			digitalWrite(linPinDir, HIGH);
			delay(10);
		}
	}
	else {
		while (load != 0) {
			if (emergencyStop()) { // emergency stop
				Serial.println("ALERT: EMERGENCY STOP");
				break;
			}
			analogWrite(linPinPwm, -linSpeed);
			digitalWrite(linPinDir, LOW);
			delay(10);
		}
	}
}

void getLoad() {
	int l = wsb.measureForce();
	load = (float) l; // convert this to kgs
}

void encoderA() { // Channel A went High
	cli(); //stop interrupts happening before we read pin values
	bFlag = digitalRead(spinPosB);
	if (bFlag) {
		spinPos--; //increment the encoder's position count
	}
	else {
		spinPos++;
	}
	//Serial.print("Spin Position = "); // DEBUG messages
	//Serial.println(spinPos);
	sei(); //restart interrupts
}

/*
void encoderB() { // Channel B went High
	cli(); //stop interrupts happening before we read pin values
	aFlag = digitalRead(spinPosA);
	if (aFlag) {
		spinPos++; //decrement the encoder's position count
	}
	//Serial.print("Spin Position = "); // DEBUG messages
	//Serial.println(spinPos);
	sei(); //restart interrupts
}

void encoder() { // interrupts on rise and fall
	
		aFlag holds the last position of A
		bFlag holds the last position of B
		These positions are compared for each combination of A and B detected
		if B changes away from A, and A changes to match B, CW
		if A changes away from B, and B changes to match A, CCW
	
	cli(); //stop interrupts happening before we read pin values
	readEncoder = PIND & 0xC; //read all eight pin values then strip away all but pinA and pinB's values
	switch (readEncoder) {
		case B00000000:
			if (!(aFlag) && bFlag) { //CCW
				spinPos--; //decrement the encoder's position count
				bFlag = 0; //updates current position for next interrupt compare
			}
			else if (aFlag && !(bFlag)) { //CW
				spinPos++; //increment the encoder's position count
				aFlag = 0; //updates current position for next interrupt compare
			}
			break;
		case B00000100:
			if (aFlag && bFlag) { //CCW
				spinPos--; //decrement the encoder's position count
				aFlag = 0; //updates current position for next interrupt compare
			}
			else if (!(aFlag) && !(bFlag)) { //CW
				spinPos++; //increment the encoder's position count
				bFlag = 1; //updates current position for next interrupt compare
			}
			break;
		case B00001000:
			if (!(aFlag) && !(bFlag)) {
				spinPos--; //decrement the encoder's position count
				aFlag = 1; //updates current position for next interrupt compare
			}
			else if (aFlag && bFlag) {
				spinPos++; //increment the encoder's position count
				bFlag = 0; //updates current position for next interrupt compare
			}
			break;
		case B00001100:
			if (aFlag && !(bFlag)) { //CCW
				spinPos--; //decrement the encoder's position count
				bFlag = 1; //updates current position for next interrupt compare
			}
			else if (!(aFlag) && bFlag) { //CW
				spinPos++; //increment the encoder's position count
				aFlag = 1; //updates current position for next interrupt compare
			}
			break;
		default:
			break;
	}
//	Serial.print("Spin Position = "); // DEBUG
//	Serial.println(spinPos);
	sei(); //restart interrupts
}
*/

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
