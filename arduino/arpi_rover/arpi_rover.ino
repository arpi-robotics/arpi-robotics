#include <AFMotor.h>
#include <NewPing.h>
#include <Servo.h> 

#define runEvery(t) for (static typeof(t) _lasttime;\
                         (typeof(t))((typeof(t))millis() - _lasttime) > (t);\
                         _lasttime += (t))

AF_DCMotor motor1(1);
AF_DCMotor motor2(2);
AF_DCMotor motor3(3);
AF_DCMotor motor4(4);

#define TRIGGER_PIN  2
#define ECHO_PIN     13
#define MAX_DISTANCE 200

NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);

int motorSpeed = 100;
int maxMotorSpeed = 255;
bool isMovingForward = false;
bool isMovingBackward = false;
bool safetyDriveMode = true;
bool debugMode = false;

int ledPin = 10;

String input = "";
  
// DC hobby servo
Servo servo;
  
void setup()
{
  Serial.begin(9600);
  Serial.print("Ready\n");
  pinMode(ledPin, OUTPUT);

  // calibrate the servo during setup
  servo.attach(9);
  
  servo.write(0);
  delay(500);
  servo.write(180);
  delay(500);
  servo.write(90);
  delay(350);
  
  servo.detach();
}

void loop()
{
  char character;
  
  while(Serial.available()) {
    character = Serial.read();    
    input.concat(character);
    delay(2);
  }

  if (input != "") {
    String result = handleCommand(input);
    Serial.print(result + "\n");
    input = "";
  }

  if (safetyDriveMode) {

    // set the interval for measuring distance
    int checkDistanceInterval = map(
      motorSpeed,
      0, maxMotorSpeed,
      400, 50 // in milliseconds
    );
    
    int distanceToStop = map(
      motorSpeed,
      0, maxMotorSpeed,
      15, 60 // in centimeter
    );
    
    runEvery(checkDistanceInterval) {
      if (isMovingForward) {
        int distanceInCm = getDistanceInCm();
        
        if (distanceInCm != NO_ECHO && distanceInCm < distanceToStop) {
          halt(0);
          if (debugMode) {
            Serial.print("Stopped by 'Safety drive'");
          }
        }
      }
    }

    if (debugMode) {
      // the "checkDistanceInterval" and "distanceToStop" is dynamically set based on how fast
      // the robot is moving. this output shows us every 2 seconds how its configured for the moment
      runEvery(2000) {
        Serial.print("Safety drive: check interval "+String(checkDistanceInterval)+
        ", distance to stop: "+String(distanceToStop)+"\n");
      }
    }
  }
}

String getCommandFromInput(String input) {
  String result = input;
  int positionOfColon = input.indexOf(':');
  
  if (positionOfColon)
  {
    result = input.substring(0, positionOfColon);
  }
 
  return result; 
}

int getArgumentFromInput(String input) {
  int result = -1;
  int positionOfColon = input.indexOf(':');
  
  if (positionOfColon)
  {
    int length = input.length() - positionOfColon;
    String argument = input.substring(positionOfColon + 1);
    result = argument.toInt();
  }
 
  return result; 
}

void setMotorSpeed(int speedInPercentage) {
  motorSpeed = map(
    speedInPercentage,
    0, 100,
    0, maxMotorSpeed
  );

  if (debugMode) {
    Serial.print(String("Set speed to ") + motorSpeed + " ("+ speedInPercentage +"%)\n");
  }
}


String handleCommand(String input) {
  String result;
  
  String command = getCommandFromInput(input);
  int argument = getArgumentFromInput(input);

  // yes, a switch-case-construct would be better, but its not supporting strings as case value.. only integers
  if(command == "battery") {
    result = checkBatteryStatus(argument);
  } else if(command == "measureDistance") {
    result = measureDistance(argument);
  } else if(command == "look") {
    result = lookDirection(argument);
  } else if(command == "forward") {
    result = forward(argument);
  } else if(command == "backward") {
    result = backward(argument);
  } else if(command == "left") {
    result = left(argument);
  } else if(command == "right") {
    result = right(argument);
  } else if(command == "stop") {
    result = halt(argument);
  } else if(command == "debug") {
    result = debug(argument);
  // here we can add additional functions:
  // } else if(command == "doSomething") {
  //   result = doSomething(argument);
  } else {
    result = "unknown command: " + command;
  }
    
  return result;
}

String lookDirection(int argument) {
  String result;

  if (argument >= 0 && argument <= 180) {
  
    digitalWrite(ledPin, HIGH);
   
    if (servo.attached() == false) {
      servo.attach(9);
    }

    int waitTimeInMs;
    
    if (servo.read() > argument) {
      waitTimeInMs = servo.read() - argument;
    } else {
      waitTimeInMs = argument - servo.read();
    }
    
    waitTimeInMs = (waitTimeInMs + 100) * 2;
    
    servo.write(argument);

    // give the servo some time to do its job
    delay(waitTimeInMs);

    // we detach the servo variable from its pin to save energy
    servo.detach();
    
    digitalWrite(ledPin, LOW);
    
    result = "ok";
  } else {
    result = "failed";
  }
  
  return result;
}

String forward(int speedInPercentage)
{
  if (speedInPercentage > 0)
  {
    setMotorSpeed(speedInPercentage);
  }
  
  isMovingForward = true;
  isMovingBackward = false;
  
  turnMotorsOn();
  
  motor1.run(FORWARD);
  motor2.run(FORWARD);
  motor3.run(BACKWARD);
  motor4.run(BACKWARD);
  
  return "ok";
}

String backward(int speedInPercentage)
{
  if (speedInPercentage > 0)
  {
    setMotorSpeed(speedInPercentage);
  }

  isMovingForward = false;
  isMovingBackward = true;
  
  turnMotorsOn();

  motor1.run(BACKWARD);
  motor2.run(BACKWARD);
  motor3.run(FORWARD);
  motor4.run(FORWARD);
  
  return "ok";
}

String halt(int argument)
{
  int currentMotorSpeedInPercentage = map(
    motorSpeed,
    0, maxMotorSpeed,
    0, 100
  );

  // to pull the brake strongly we move for a short time in the opposite direction
  if (isMovingForward)
  {
    backward(80);
  }
  else if(isMovingBackward)
  {
    forward(80);
  }

  delay(currentMotorSpeedInPercentage * 2);
  
  turnMotorsOff();
  
  isMovingForward = false;
  isMovingBackward = false;

  return "ok";
}


String left(int speedInPercentage)
{
  int currentMotorSpeedInPercentage = map(
    motorSpeed,
    0, maxMotorSpeed,
    0, 100
  );

  // reset the speed of all motors to "motorSpeed"
  turnMotorsOn();

  int speedOfLeftMotors = ((float) currentMotorSpeedInPercentage / 100) * speedInPercentage;

  motor2.setSpeed(speedOfLeftMotors);
  motor3.setSpeed(speedOfLeftMotors);
  
  return "ok";
}

String right(int speedInPercentage)
{
  int currentMotorSpeedInPercentage = map(
    motorSpeed,
    0, maxMotorSpeed,
    0, 100
  );

  // reset the speed of all motors to "motorSpeed"
  turnMotorsOn();

  int speedOfRightMotors = ((float) currentMotorSpeedInPercentage / 100) * speedInPercentage;

  motor1.setSpeed(speedOfRightMotors);
  motor4.setSpeed(speedOfRightMotors);
  
  return "ok";
}

String checkBatteryStatus(int argument)
{
  int counter = 0;
  for (int i = 0; i <= 3; i++) {
    int value = analogRead(0);
    if (value < 10) {
      counter = counter + 1;
    }
      
    delay(1000);
  }
  
  String result;
  
  if (counter > 2) {
    result = "low";
  } else {
    result = "ok";
  }
  
  return result;
}

String debug(int argument) {
  if (argument == 1) {
    debugMode = true;
  } else {
    debugMode = false;
  }

  return "ok";
}

void turnMotorsOff()
{
  motor1.setSpeed(0);
  motor2.setSpeed(0);
  motor3.setSpeed(0);
  motor4.setSpeed(0);

  motor1.run(RELEASE);
  motor2.run(RELEASE);
  motor3.run(RELEASE);
  motor4.run(RELEASE);
}

void turnMotorsOn()
{
  motor1.setSpeed(motorSpeed);
  motor2.setSpeed(motorSpeed);
  motor3.setSpeed(motorSpeed);
  motor4.setSpeed(motorSpeed);
}

String measureDistance(int argument) {
  return String("Ping: ") + String(getDistanceInCm()) + String("cm");
}

int getDistanceInCm() {
  return sonar.ping_cm();
}
