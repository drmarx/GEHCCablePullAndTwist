If you want to run it without Blake's UI program, just go in and uncomment lines 74-78, 178, 233, and 247-243. I did not have much time to test the old implementation of test running, with the Arduino controlling everything, so there may be some bugs. The command set is as follows:

TEST <length of test in minutes<int>> <rest time in seconds<int>> <test repetitions<int>> <force in kgs<float>> <spin turn degrees<int>> <continuity break stop (0=y,1=n)<bit>>
  Six total arguments. Enters the test parameters into the system.
RATE <float>
  Time to wait between reporting sensor data in seconds. Default is 0.25 s.
START
  Starts test after entering test parameters
STOP
  Stops test
TEMP
  Gets current temperature and humidity
DATA
  Gets current force, spin position, and cable resistance
SPIN (degrees)<int>​​
  Spins the motor to a certain degree. Can take positive or negative. SPIN 0 to return to initial position. Resets if serial disconnects
PULL (force)<float>
  Pulls the motor to a certain force. If not connected to a cable, will go down to the bottom position. PULL 0 to push to the top position.
STATUS
  Returns 1 if test is running, 0 if test is not running.
