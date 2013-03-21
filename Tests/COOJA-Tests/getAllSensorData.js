GENERATE_MSG(10000, "sleep"); //Wait for 10 secs, give the nodes a chance to start up
YIELD_THEN_WAIT_UNTIL(msg.equals("sleep"));

printAllIDs(); //Print all the IDs of the nodes

//prints the information of the node
goThroughNodes(function(m){
	log.log("\nMote ID = " + m.getID() + "\n");
	log.log("Rime Address = " + m.getInterfaces().getRimeAddress().getAddressString() + "\n");
	log.log("Temprature =  " + m.getInterfaces().get("Temperature").getTemperature() +"\n");
	log.log("Humidity = " + m.getInterfaces().get("Humidity").getHumidity() + "\n");
});

log.log("\n");
log.testOK(); /* Report test success and quit */

function goThroughNodes(func)
{
	allmotes = sim.getMotes();

	for(var i = 0; i < allmotes.length; i++)
	{
		func(allmotes[i]);
	}
}

function printAllIDs()
{
	log.log("Mote IDs: [");

	goThroughNodes(function(mote) {
		log.log(" " + mote.getID());
	});
	log.log(" ]\n");
}