GENERATE_MSG(10000, "sleep"); //Wait for 10 secs, give the nodes a chance to start up
YIELD_THEN_WAIT_UNTIL(msg.equals("sleep"));

//ids of the nodes that need their sensor values changed
var ids_to_change = [1,2,3,4,5];
var temperature_value = 100;
var humidity_value = 100;

for (var i = 0; i < ids_to_change.length; i++)
{
	m = sim.getMoteWithID(ids_to_change[i]);
	log.log("ID: = " + m.getID() + "\n");
	m.getInterfaces().get("Temperature").setTemperature(temperature_value);
	m.getInterfaces().get("Humidity").setHumidity(humidity_value);
}

log.testOK();