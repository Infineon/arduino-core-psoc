
double number;
double result;

// the setup function runs once when you press reset or power the board
void setup() {

  // Define a number to calculate the square root of
  number = 25.0;
}

// the loop function runs over and over again forever
void loop() {
  // Loop to count from 0 to 100
  for (int i = 0; i <= 100; i++) {
    result = sqrt(number);
  }
}
