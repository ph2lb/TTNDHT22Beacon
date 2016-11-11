# TTNDHT22Beacon
A low power beacon with a DHT22 sensor for the ThingsNetwork with deepsleep support and variable interval

Hardware used : 
  - Arduino Pro-Mini 3.3V
  - RN2483
  - DHT22

Software used : 
  - Modified TheThingsNetwork library (for deepsleep support) check my github
  - DHT library
  - LowPower library
 
 
TheThingsNetwork Payload functions : 
  
  DECODER : 
  
  function (bytes) {
  var batt = bytes[0] / 10.0;
 
  if (bytes.length >= 2)
  {
    var humidity = bytes[1];
  } 
  if (bytes.length >= 3)
  {
    var temperature = (((bytes[2] << 8) | bytes[3]) / 10.0) - 40.0;
  } 
  if (bytes.length >= 5)
  { 
    var pkcount = bytes[4];
    var txresult = (bytes[5] & 0x0f);
    var txretrycount = (bytes[5] & 0xf0) >> 4;
  }
