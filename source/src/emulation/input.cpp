#include "input.h"

void Input::init(char SingleMachine) {
  singleMachine = SingleMachine;
  virtual_coin_state = 0;
#ifdef MCP23017_INPUT
  mcp.setup();
#elif defined(NUNCHUCK_INPUT)
  nunchuck.setup();
#else
  pinMode(BTN_START_PIN, INPUT_PULLUP);
  #ifdef BTN_COIN_PIN
    pinMode(BTN_COIN_PIN, INPUT_PULLUP);
  #endif
  pinMode(BTN_LEFT_PIN, INPUT_PULLUP);
  pinMode(BTN_RIGHT_PIN, INPUT_PULLUP);
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_UP_PIN, INPUT_PULLUP);
  pinMode(BTN_FIRE_PIN, INPUT_PULLUP);
#endif

  char inputs = buttons_get();
  if (inputs & BUTTON_FIRE) {
    printf("Demo Sounds switched off\n");
    switchDemoSoundsOff = 1;
    firePressedAtStart = 1;
  }
}

char Input::demoSoundsOff() {
  return switchDemoSoundsOff;
}

unsigned char Input::buttons_get(void) {
  unsigned char input_states = 0;
#ifdef MCP23017_INPUT
  input_states = mcp.getInput();
#elif defined(NUNCHUCK_INPUT)
  input_states = nunchuck.getInput();  
#else
  #ifdef BTN_COIN_PIN
    input_states = (!digitalRead(BTN_COIN_PIN)) ? BUTTON_EXTRA : 0;
  #else
    input_states = (!digitalRead(BTN_START_PIN)) ? BUTTON_EXTRA : 0;
  #endif
  input_states |=
    (digitalRead(BTN_LEFT_PIN) ? 0 : BUTTON_LEFT) |
    (digitalRead(BTN_RIGHT_PIN) ? 0 : BUTTON_RIGHT) |
    (digitalRead(BTN_UP_PIN) ? 0 : BUTTON_UP) |
    (digitalRead(BTN_DOWN_PIN) ? 0 : BUTTON_DOWN) |
    (digitalRead(BTN_FIRE_PIN) ? 0 : BUTTON_FIRE);
#endif
  
  // Capture physical EXTRA button state for the Virtual Coin state machine
  bool extraPhysicalPressed = (input_states & BUTTON_EXTRA);

  unsigned char startAndCoinState = 0;

  // Virtual Coin State Machine (The "Extra" button logic)
  switch(virtual_coin_state)  {
    case 0:  // idle state
      if(extraPhysicalPressed) {
        virtual_coin_state = 1;   // virtual coin pressed
        virtual_coin_timer = millis();
      }
      break;
    case 1:  // start was just pressed
      if(millis() - virtual_coin_timer > 100) {
        virtual_coin_state = 2;   // virtual coin released
        virtual_coin_timer = millis();        
      }
      break;
    case 2:  // virtual coin was released
      if(millis() - virtual_coin_timer > 500) {
        virtual_coin_state = 3;   // pause between virtual coin an start ended
        virtual_coin_timer = millis();        
      }
      break;
    case 3:  // pause ended
      if(millis() - virtual_coin_timer > 100) {
        virtual_coin_state = 4;   // virtual start ended
        virtual_coin_timer = millis();        
      }
      break;
    case 4:  // virtual start has ended
      if(!extraPhysicalPressed)
        virtual_coin_state = 0;   // button has been released, return to idle
      break;
  }
  
  startAndCoinState = ((virtual_coin_state != 1) ? 0 : BUTTON_COIN) | 
                      (((virtual_coin_state != 3) && (virtual_coin_state != 4)) ? 0 : BUTTON_START);

  // If MCP23017 is used, allow hardware start/coin pins as well
#ifdef MCP23017_INPUT
  startAndCoinState |= (input_states & (BUTTON_START | BUTTON_COIN));
#elif defined(BTN_COIN_PIN)
  // standard pins
  startAndCoinState |= (digitalRead(BTN_START_PIN) ? 0 : BUTTON_START) |
    (digitalRead(BTN_COIN_PIN) ? 0 : BUTTON_COIN);
#endif

  // Clean up input_states for return, but KEEP BUTTON_EXTRA for reset check below
  input_states &= ~(BUTTON_START | BUTTON_COIN);

  // volume control
  if (extraPhysicalPressed && _volume_callback) {
    _volume_callback(input_states & BUTTON_UP, input_states & BUTTON_DOWN);
    
    if ((input_states & BUTTON_UP) | (input_states & BUTTON_DOWN))
      reset_timer = 0;
  }

  if (!singleMachine) {
    bool buttonExtraRisingEdge = extraPhysicalPressed && !(input_states_last & BUTTON_EXTRA); 
    bool buttonUpRisingEdge = (input_states & BUTTON_UP) && !(input_states_last & BUTTON_UP); 
    bool buttonDownRisingEdge = (input_states & BUTTON_DOWN) && !(input_states_last & BUTTON_DOWN); 

    if (buttonUpRisingEdge | buttonDownRisingEdge | buttonExtraRisingEdge ) {
      if (_doAttractReset_callback)
        _doAttractReset_callback();  
    }

    if(extraPhysicalPressed) {
      if(!reset_timer)
        reset_timer = millis();
    
      if(millis() - reset_timer > 3000) {
        reset_timer = millis();
        if (_doReset_callback)
          _doReset_callback();
      }
    } 
    else
      reset_timer = 0;

    // Track state for rising edge detection
    input_states_last = (input_states & ~BUTTON_EXTRA) | (extraPhysicalPressed ? BUTTON_EXTRA : 0);
  }

  if (firePressedAtStart && input_states & BUTTON_FIRE) {
    printf("Wait for release fire button...\n");
    input_states = 0;
  }
  else
  {
    firePressedAtStart = false;
  }

  // Return merged state without physical EXTRA button (handled internally)
  return (input_states & ~BUTTON_EXTRA) | startAndCoinState;
}

Input &Input::onVolumeUpDown(THandlerVolume fn) {
  _volume_callback = fn;
  return *this;
}

Input &Input::onDoReset(THandlerDoReset fn) {
  _doReset_callback = fn;
  return *this;
}

Input &Input::onDoAttractReset(THandlerDoAttractReset fn) {
  _doAttractReset_callback = fn;
  return *this;
}
