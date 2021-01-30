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


#ifndef ButtonInput_h
#define ButtonInput_h

class ButtonInput
{
  public:
    ButtonInput( int pin, bool pulledHigh);
    ButtonInput( int pin, bool pulledHigh, bool userInternalPullUp);
    ~ButtonInput();

    int getPin();
    bool getPulledHigh();
    void Test();
    bool getInvert();
    bool getPressed();
    bool getChanged();

  private:
    int        _buttonPin                 = 0;
    bool       _pulledHigh                = false;

    int        _lastState                 = 0;
    bool       _hasChanged                = false;
    bool       _pressed                   = false;
};

#endif
