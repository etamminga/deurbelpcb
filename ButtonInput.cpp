/**
 * ButtonInput.cpp
 * 
 * ButtonInput, encapsulation of a binary input
 * 
 * @author Creator Erik Tamminga
 * @author etamminga
 * @version 0.0.0
 * @license MIT
 */

#include <Arduino.h>
#include "ButtonInput.h"

/**
 * --------------------------------------------------------------------------------
 *  ButtonInput 
 * --------------------------------------------------------------------------------
**/

// constructors
ButtonInput::ButtonInput( int pin, bool pulledHigh, bool userInternalPullUp) {
  _buttonPin = pin;
  _pulledHigh = pulledHigh;
  
  pinMode(_buttonPin, (_pulledHigh && userInternalPullUp) ? INPUT_PULLUP : INPUT);  
}

ButtonInput::ButtonInput(int pin, bool pulledHigh){
  _buttonPin = pin;
  _pulledHigh = pulledHigh;
  
  pinMode(_buttonPin, INPUT);
}

// destructor
ButtonInput::~ButtonInput() {
}

int ButtonInput::getPin() {
  return _buttonPin;
}

bool ButtonInput::getPulledHigh() {
  return _pulledHigh;
}

void ButtonInput::Test() {
  int inputValue =  digitalRead(_buttonPin);
  if (_pulledHigh) {
    inputValue = (inputValue == HIGH) ? LOW : HIGH;
  }
  _pressed = (inputValue == HIGH);

  _hasChanged = false;
  if (_lastState != inputValue) {
    _lastState = inputValue;
    _hasChanged = true;    
  }
}

bool ButtonInput::getPressed() {
  return _pressed;
}

bool ButtonInput::getChanged() {
  return _hasChanged;
}
