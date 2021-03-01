class Relay{
  private:
    byte _pin;
  public:
  boolean status;
  String relayName;
  Relay(byte pin = 0,String _relayName = "relay"){
    if (pin !=0 || _relayName != "relay"){init(pin,_relayName);}
  }
  void init(byte pin,String _relayName){
    _pin = pin;
    relayName = _relayName;
    pinMode(_pin,OUTPUT);
//    Serial.println("setted New relay named " + _relayName + " to pin " + _pin);
  }
  void set(boolean val){
    if (status != val)
    {
    digitalWrite(_pin,val);
    status = val;
    Serial.println(relayName + " recieved command - " + val);
    if (val == HIGH || val == 1){
      mqttClient.publish("/"+relayName+"/state", "1");
    } else {
      mqttClient.publish("/"+relayName+"/state", "0");
    }
    
    }
  }
  void flip(){
    if(status == HIGH){
     set(LOW);
    } else {
      set(HIGH);
    }
  }
};